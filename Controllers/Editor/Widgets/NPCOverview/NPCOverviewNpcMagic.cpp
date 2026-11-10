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

void NPCOverviewController::setSelectedNpcSpellcastingEnabled(bool enabled)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.spellcasting.enabled = enabled;
    normalizeSpellcasting(npc.spellcasting);
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcSpellcastingAbility(const std::string &ability)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.spellcasting.ability = ability;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcSpellAttackBonus(int value)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.spellcasting.spellAttackBonus = value;
    npc.spellcasting.spellAttackMode = SpellValueMode::Manual;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcSpellSaveDc(int value)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.spellcasting.spellSaveDc = std::max(1, value);
    npc.spellcasting.spellSaveDcMode = SpellValueMode::Manual;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcSpellAttackMode(SpellValueMode mode)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size())) return;
    auto &npc = _context.campaign.npcs[_selectedNpcIndex];
    if (npc.spellcasting.spellAttackMode == mode) return;
    npc.spellcasting.spellAttackMode = mode;
    npc.metadata.touch(); saveCampaign();
}

void NPCOverviewController::setSelectedNpcSpellSaveDcMode(SpellValueMode mode)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size())) return;
    auto &npc = _context.campaign.npcs[_selectedNpcIndex];
    if (npc.spellcasting.spellSaveDcMode == mode) return;
    npc.spellcasting.spellSaveDcMode = mode;
    npc.metadata.touch(); saveCampaign();
}

void NPCOverviewController::setSelectedNpcSpellSlotMaximum(int spellLevel, int value)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()) || spellLevel < 1 || spellLevel > 9)
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    normalizeSpellcasting(npc.spellcasting);
    npc.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)] = std::max(0, value);
    npc.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)] = std::clamp(npc.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)], 0, npc.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)]);
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcSpellSlotUsed(int spellLevel, int value)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()) || spellLevel < 1 || spellLevel > 9)
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    normalizeSpellcasting(npc.spellcasting);
    npc.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)] = std::clamp(value, 0, npc.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)]);
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::addResourceTrackerToSelectedNpc()
{
    if (retryPendingSave())
        return;

    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;
    normalizeResourceTracker(_npcResourceDraft);
    if (_npcResourceDraft.name.empty())
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    _npcResourceDraft.metadata.touch();
    npc.resourceTrackers.push_back(_npcResourceDraft);
    npc.metadata.touch();
    _resourceDraftPendingSave = true;
    saveCampaign();
}

void NPCOverviewController::removeResourceTrackerFromSelectedNpc(int trackerIndex)
{
    if (retryPendingSave())
        return;

    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;
    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    if (trackerIndex < 0 || trackerIndex >= static_cast<int>(npc.resourceTrackers.size()))
        return;

    npc.resourceTrackers.erase(npc.resourceTrackers.begin() + trackerIndex);
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcResourceUsed(int trackerIndex, int used)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;
    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    if (trackerIndex < 0 || trackerIndex >= static_cast<int>(npc.resourceTrackers.size()))
        return;

    ResourceTrackerModel &tracker = npc.resourceTrackers[static_cast<std::size_t>(trackerIndex)];
    normalizeResourceTracker(tracker);
    tracker.used = std::clamp(used, 0, tracker.maximum);
    tracker.metadata.touch();
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::resetSelectedNpcResource(int trackerIndex)
{
    setSelectedNpcResourceUsed(trackerIndex, 0);
}

void NPCOverviewController::addSpellToSelectedNpc(const EntityId &spellId)
{
    if (retryPendingSave())
        return;

    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()) || spellId.empty())
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    normalizeSpellcasting(npc.spellcasting);
    const std::vector<SpellModel> availableSpells = getAvailableSpells(_context.campaign);
    const SpellModel *spell = findSpellById(availableSpells, spellId);
    if (!spell)
        return;
    const auto found = std::find_if(npc.spellcasting.spells.begin(), npc.spellcasting.spells.end(), [&](const CharacterSpellEntryModel &entry) {
        return entry.spellId == spellId;
    });
    if (found != npc.spellcasting.spells.end())
        return;

    CharacterSpellEntryModel entry;
    entry.spellId = spellId;
    entry.prepared = true;
    entry.source = std::find_if(_context.campaign.customSpells.begin(), _context.campaign.customSpells.end(), [&](const SpellModel &customSpell) {
                       return customSpell.metadata.id == spellId;
                   }) != _context.campaign.customSpells.end()
                       ? "Campaign"
                       : (_context.campaign.ruleSet ? _context.campaign.ruleSet->getName() : "Ruleset");
    npc.spellcasting.enabled = true;
    npc.spellcasting.spells.push_back(entry);
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::toggleSelectedNpcPreparedSpell(int spellEntryIndex)
{
    if (retryPendingSave())
        return;

    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    if (spellEntryIndex < 0 || spellEntryIndex >= static_cast<int>(npc.spellcasting.spells.size()))
        return;

    npc.spellcasting.spells[static_cast<std::size_t>(spellEntryIndex)].prepared = !npc.spellcasting.spells[static_cast<std::size_t>(spellEntryIndex)].prepared;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::removeSpellFromSelectedNpc(int spellEntryIndex)
{
    if (retryPendingSave())
        return;

    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    if (spellEntryIndex < 0 || spellEntryIndex >= static_cast<int>(npc.spellcasting.spells.size()))
        return;

    npc.spellcasting.spells.erase(npc.spellcasting.spells.begin() + spellEntryIndex);
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setNpcPublicVisibility(const EntityId &npcId, bool visible)
{
    std::vector<EntityId> &visibleNpcIds = _context.campaign.localPlayerDisplay.visibleNpcIds;
    const auto existing = std::find(visibleNpcIds.begin(), visibleNpcIds.end(), npcId);
    if (visible && existing == visibleNpcIds.end())
        visibleNpcIds.push_back(npcId);
    else if (!visible && existing != visibleNpcIds.end())
        visibleNpcIds.erase(existing);
    else
        return;

    _context.campaign.localPlayerDisplay.metadata.touch();
    saveCampaign();
}
