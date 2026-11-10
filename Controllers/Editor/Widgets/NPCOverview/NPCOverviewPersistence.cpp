#include "NPCOverviewController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include <algorithm>
#include <cctype>
#include <utility>

namespace
{
std::string uiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}

bool hasTag(const EntityMetadata &metadata, const std::string &tag)
{
    return std::find(metadata.tags.begin(), metadata.tags.end(), tag) != metadata.tags.end();
}

void addTag(EntityMetadata &metadata, const std::string &tag)
{
    if (!hasTag(metadata, tag))
        metadata.tags.push_back(tag);
}

std::string trimCopy(const std::string &value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    return first >= last ? std::string{} : std::string(first, last);
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

int findFactionIndexByName(const std::vector<FactionModel> &factions, const std::string &name)
{
    const std::string needle = lowerCopy(trimCopy(name));
    if (needle.empty())
        return -1;

    for (int i = 0; i < static_cast<int>(factions.size()); ++i)
    {
        if (lowerCopy(trimCopy(factions[static_cast<std::size_t>(i)].name)) == needle)
            return i;
    }
    return -1;
}

int findDeityIndexByName(const std::vector<DeityModel> &deities, const std::string &name)
{
    const std::string needle = lowerCopy(trimCopy(name));
    if (needle.empty())
        return -1;

    for (int i = 0; i < static_cast<int>(deities.size()); ++i)
    {
        if (lowerCopy(trimCopy(deities[static_cast<std::size_t>(i)].name)) == needle)
            return i;
    }
    return -1;
}
}

void NPCOverviewController::toggleInventoryEntryEquipped(int entryIndex)
{
    if (retryPendingSave())
        return;

    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    if (entryIndex < 0 || entryIndex >= static_cast<int>(npc.inventory.entries.size()))
        return;

    npc.inventory.entries[entryIndex].equipped = !npc.inventory.entries[entryIndex].equipped;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setInventoryEntryQuantity(int entryIndex, int quantity)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    if (entryIndex < 0 || entryIndex >= static_cast<int>(npc.inventory.entries.size()))
        return;

    npc.inventory.entries[entryIndex].quantity = std::max(1, quantity);
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::removeInventoryEntry(int entryIndex)
{
    if (retryPendingSave())
        return;

    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    if (entryIndex < 0 || entryIndex >= static_cast<int>(npc.inventory.entries.size()))
        return;

    npc.inventory.entries.erase(npc.inventory.entries.begin() + entryIndex);
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::refreshSaveContext()
{
    if (_saveFailed && _saveFailureCampaignId != _context.campaign.metadata.id)
    {
        _saveFailed = false;
        _saveErrorMessage.clear();
        _resourceDraftPendingSave = false;
        _actionDraftPendingSave = false;
    }
}

bool NPCOverviewController::saveCampaign()
{
    refreshSaveContext();
    _context.campaign.metadata.touch();
    auto &service = ProjectSaveService::getInstance();
    _saveFailed = !service.saveCampaign(_context.campaign, "actor edits");
    if (_saveFailed)
    {
        _saveFailureCampaignId = _context.campaign.metadata.id;
        _saveErrorMessage = uiText("Not saved. Your changes remain here. Retry Save before closing. ",
                                   "Nicht gespeichert. Deine Änderungen bleiben hier. Vor dem Schließen erneut speichern. ") + service.state().lastError;
        return false;
    }
    _saveErrorMessage.clear();
    if (_resourceDraftPendingSave)
    {
        _npcResourceDraft = ResourceTrackerModel{};
        _resourceDraftPendingSave = false;
    }
    if (_actionDraftPendingSave)
    {
        _monsterActionDraft = MonsterActionModel{};
        _monsterActionDraft.name = gm::localization::trUi("Attack");
        _actionDraftPendingSave = false;
    }
    return true;
}

void NPCOverviewController::retrySave()
{
    saveCampaign(); // Save the retained identities; never repeat a mutation.
}

bool NPCOverviewController::retryPendingSave()
{
    refreshSaveContext();
    if (!_saveFailed)
        return false;
    retrySave();
    return true; // Even a successful retry does not repeat create/delete/toggle.
}

void NPCOverviewController::removeDeletedNpcReferences(const EntityId &npcId)
{
    auto removeNpcLinks = [&](std::vector<StoryLinkModel> &links) {
        const auto oldSize = links.size();
        links.erase(std::remove_if(links.begin(), links.end(), [&](const StoryLinkModel &link) {
            return link.targetEntityType == "NPC" && link.targetEntityId == npcId;
        }), links.end());
        return oldSize != links.size();
    };

    for (StoryThreadModel &thread : _context.campaign.storyThreads)
    {
        bool changed = removeNpcLinks(thread.links);
        for (StoryBeatModel &beat : thread.beats)
        {
            if (removeNpcLinks(beat.links))
            {
                beat.metadata.touch();
                changed = true;
            }
        }
        if (changed)
            thread.metadata.touch();
    }

    for (EncounterModel &encounter : _context.campaign.encounters)
    {
        const auto oldSize = encounter.participants.size();
        encounter.participants.erase(std::remove_if(encounter.participants.begin(), encounter.participants.end(), [&](const EncounterParticipantModel &participant) {
            return participant.kind == CombatantKind::NPC && participant.sourceEntityId == npcId;
        }), encounter.participants.end());
        if (oldSize != encounter.participants.size())
        {
            if (encounter.participants.empty())
                encounter.activeParticipantIndex = 0;
            else
                encounter.activeParticipantIndex = std::clamp(encounter.activeParticipantIndex, 0, static_cast<int>(encounter.participants.size()) - 1);
            encounter.metadata.touch();
        }
    }
}

void NPCOverviewController::removeDeletedMonsterReferences(const EntityId &monsterId)
{
    auto removeMonsterLinks = [&](std::vector<StoryLinkModel> &links) {
        const auto oldSize = links.size();
        links.erase(std::remove_if(links.begin(), links.end(), [&](const StoryLinkModel &link) {
            return link.targetEntityType == "Monster" && link.targetEntityId == monsterId;
        }), links.end());
        return oldSize != links.size();
    };

    for (StoryThreadModel &thread : _context.campaign.storyThreads)
    {
        bool changed = removeMonsterLinks(thread.links);
        for (StoryBeatModel &beat : thread.beats)
        {
            if (removeMonsterLinks(beat.links))
            {
                beat.metadata.touch();
                changed = true;
            }
        }
        if (changed)
            thread.metadata.touch();
    }

    for (EncounterModel &encounter : _context.campaign.encounters)
    {
        const auto oldSize = encounter.participants.size();
        encounter.participants.erase(std::remove_if(encounter.participants.begin(), encounter.participants.end(), [&](const EncounterParticipantModel &participant) {
            return participant.monsterDefinitionId == monsterId;
        }), encounter.participants.end());
        if (oldSize != encounter.participants.size())
        {
            if (encounter.participants.empty())
                encounter.activeParticipantIndex = 0;
            else
                encounter.activeParticipantIndex = std::clamp(encounter.activeParticipantIndex, 0, static_cast<int>(encounter.participants.size()) - 1);
            encounter.metadata.touch();
        }
    }
}

void NPCOverviewController::onShow()
{
    _view->show();
    _view->requestDockOnNextUpdate();
}
