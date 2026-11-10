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

std::vector<std::string> NPCOverviewController::getNpcReferenceWarnings(int npcIndex) const
{
    std::vector<std::string> warnings;
    if (npcIndex < 0 || npcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return warnings;

    const NPCModel &npc = _context.campaign.npcs[npcIndex];
    const EntityId &npcId = npc.metadata.id;
    int storyLinkCount = 0;
    for (const StoryThreadModel &thread : _context.campaign.storyThreads)
    {
        storyLinkCount += static_cast<int>(std::count_if(thread.links.begin(), thread.links.end(), [&](const StoryLinkModel &link) {
            return link.targetEntityType == "NPC" && link.targetEntityId == npcId;
        }));
        for (const StoryBeatModel &beat : thread.beats)
        {
            storyLinkCount += static_cast<int>(std::count_if(beat.links.begin(), beat.links.end(), [&](const StoryLinkModel &link) {
                return link.targetEntityType == "NPC" && link.targetEntityId == npcId;
            }));
        }
    }
    if (storyLinkCount > 0)
        warnings.push_back(std::to_string(storyLinkCount) + " " + gm::localization::trUi("story link(s) point at this NPC."));

    int encounterParticipantCount = 0;
    for (const EncounterModel &encounter : _context.campaign.encounters)
    {
        encounterParticipantCount += static_cast<int>(std::count_if(encounter.participants.begin(), encounter.participants.end(), [&](const EncounterParticipantModel &participant) {
            return participant.kind == CombatantKind::NPC && participant.sourceEntityId == npcId;
        }));
    }
    if (encounterParticipantCount > 0)
        warnings.push_back(std::to_string(encounterParticipantCount) + " " + gm::localization::trUi("encounter participant(s) use this NPC."));

    if (!npc.inventory.entries.empty())
        warnings.push_back(std::to_string(npc.inventory.entries.size()) + " " + gm::localization::trUi("inventory item row(s) will stay only with this NPC."));
    if (npc.spellcasting.enabled && !npc.spellcasting.spells.empty())
        warnings.push_back(std::to_string(npc.spellcasting.spells.size()) + " " + gm::localization::trUi("known spell(s) are assigned to this NPC."));

    return warnings;
}

std::vector<std::string> NPCOverviewController::getMonsterReferenceWarnings(int monsterIndex) const
{
    std::vector<std::string> warnings;
    if (monsterIndex < 0 || monsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return warnings;

    const MonsterDefinitionModel &monster = _context.campaign.customMonsters[monsterIndex];
    const EntityId &monsterId = monster.metadata.id;
    int storyLinkCount = 0;
    for (const StoryThreadModel &thread : _context.campaign.storyThreads)
    {
        storyLinkCount += static_cast<int>(std::count_if(thread.links.begin(), thread.links.end(), [&](const StoryLinkModel &link) {
            return link.targetEntityType == "Monster" && link.targetEntityId == monsterId;
        }));
        for (const StoryBeatModel &beat : thread.beats)
        {
            storyLinkCount += static_cast<int>(std::count_if(beat.links.begin(), beat.links.end(), [&](const StoryLinkModel &link) {
                return link.targetEntityType == "Monster" && link.targetEntityId == monsterId;
            }));
        }
    }
    if (storyLinkCount > 0)
        warnings.push_back(std::to_string(storyLinkCount) + " " + gm::localization::trUi("story link(s) point at this monster."));

    int encounterParticipantCount = 0;
    for (const EncounterModel &encounter : _context.campaign.encounters)
    {
        encounterParticipantCount += static_cast<int>(std::count_if(encounter.participants.begin(), encounter.participants.end(), [&](const EncounterParticipantModel &participant) {
            return participant.monsterDefinitionId == monsterId;
        }));
    }
    if (encounterParticipantCount > 0)
        warnings.push_back(std::to_string(encounterParticipantCount) + " " + gm::localization::trUi("encounter participant(s) use this monster."));

    if (!monster.actions.empty())
        warnings.push_back(std::to_string(monster.actions.size()) + " " + gm::localization::trUi("combat action(s) are defined on this monster."));

    return warnings;
}

bool NPCOverviewController::isNpcArchived(const NPCModel &npc) const
{
    return hasTag(npc.metadata, "archived");
}

bool NPCOverviewController::isMonsterArchived(const MonsterDefinitionModel &monster) const
{
    return hasTag(monster.metadata, "archived");
}

void NPCOverviewController::requestArchiveSelectedNpc()
{
    _pendingNpcRemovalIndex = _selectedNpcIndex;
    _pendingNpcRemovalIsArchive = true;
}

void NPCOverviewController::requestDeleteSelectedNpc()
{
    _pendingNpcRemovalIndex = _selectedNpcIndex;
    _pendingNpcRemovalIsArchive = false;
}

void NPCOverviewController::requestArchiveSelectedMonster()
{
    _pendingMonsterRemovalIndex = _selectedMonsterIndex;
    _pendingMonsterRemovalIsArchive = true;
}

void NPCOverviewController::requestDeleteSelectedMonster()
{
    _pendingMonsterRemovalIndex = _selectedMonsterIndex;
    _pendingMonsterRemovalIsArchive = false;
}

void NPCOverviewController::archivePendingNpc()
{
    if (retryPendingSave())
        return;

    if (_pendingNpcRemovalIndex < 0 || _pendingNpcRemovalIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_pendingNpcRemovalIndex];
    addTag(npc.metadata, "archived");
    npc.metadata.touch();
    _selectedNpcIndex = _pendingNpcRemovalIndex;
    _pendingNpcRemovalIndex = -1;
    saveCampaign();
}

void NPCOverviewController::deletePendingNpc()
{
    if (retryPendingSave())
        return;

    if (_pendingNpcRemovalIndex < 0 || _pendingNpcRemovalIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    const EntityId removedId = _context.campaign.npcs[_pendingNpcRemovalIndex].metadata.id;
    removeDeletedNpcReferences(removedId);
    _context.campaign.npcs.erase(_context.campaign.npcs.begin() + _pendingNpcRemovalIndex);
    if (_context.campaign.npcs.empty())
        _selectedNpcIndex = 0;
    else
        _selectedNpcIndex = std::clamp(_pendingNpcRemovalIndex, 0, static_cast<int>(_context.campaign.npcs.size()) - 1);
    _pendingNpcRemovalIndex = -1;
    _context.campaign.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::archivePendingMonster()
{
    if (retryPendingSave())
        return;

    if (_pendingMonsterRemovalIndex < 0 || _pendingMonsterRemovalIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_pendingMonsterRemovalIndex];
    addTag(monster.metadata, "archived");
    monster.metadata.touch();
    _selectedMonsterIndex = _pendingMonsterRemovalIndex;
    clampSelectedMonsterActionIndex();
    _pendingMonsterRemovalIndex = -1;
    saveCampaign();
}

void NPCOverviewController::deletePendingMonster()
{
    if (retryPendingSave())
        return;

    if (_pendingMonsterRemovalIndex < 0 || _pendingMonsterRemovalIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    const EntityId removedId = _context.campaign.customMonsters[_pendingMonsterRemovalIndex].metadata.id;
    removeDeletedMonsterReferences(removedId);
    _context.campaign.customMonsters.erase(_context.campaign.customMonsters.begin() + _pendingMonsterRemovalIndex);
    if (_context.campaign.customMonsters.empty())
        _selectedMonsterIndex = 0;
    else
        _selectedMonsterIndex = std::clamp(_pendingMonsterRemovalIndex, 0, static_cast<int>(_context.campaign.customMonsters.size()) - 1);
    clampSelectedMonsterActionIndex();
    _pendingMonsterRemovalIndex = -1;
    _context.campaign.metadata.touch();
    saveCampaign();
}
