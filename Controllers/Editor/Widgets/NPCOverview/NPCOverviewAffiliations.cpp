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

void NPCOverviewController::setSelectedNpcCategory(int categoryIndex)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.category = static_cast<NPCCategory>(std::clamp(categoryIndex, 0, 3));
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcFactionId(const EntityId &factionId)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.factionId = factionId;
    const auto factionIt = std::find_if(_context.campaign.factions.begin(), _context.campaign.factions.end(), [&](const FactionModel &faction) {
        return faction.metadata.id == factionId;
    });
    npc.faction = factionIt == _context.campaign.factions.end() ? std::string{} : factionIt->name;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcDeityId(const EntityId &deityId)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.deityId = deityId;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::createOrLinkFactionFromSelectedNpcText()
{
    if (retryPendingSave())
        return;

    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    const std::string factionName = trimCopy(npc.faction);
    if (factionName.empty())
        return;

    const int existingFactionIndex = findFactionIndexByName(_context.campaign.factions, factionName);
    if (existingFactionIndex >= 0)
    {
        FactionModel &faction = _context.campaign.factions[existingFactionIndex];
        npc.factionId = faction.metadata.id;
        npc.faction = faction.name.empty() ? factionName : faction.name;
        npc.metadata.touch();
        _selectedFactionIndex = existingFactionIndex;
        saveCampaign();
        return;
    }

    FactionModel faction;
    faction.name = factionName;
    faction.type = npc.category == NPCCategory::FactionEntity ? "Faction Entity" : "Faction";
    faction.summary = std::string(gm::localization::trUi("Created from legacy faction text on")) + " " +
                      (npc.name.empty() ? std::string(gm::localization::trUi("an NPC profile")) : npc.name) + ".";
    faction.headquarters = npc.threatLocation;
    faction.attitudeTowardParty = npc.attitudeTowardParty;
    faction.goals = npc.goals;
    faction.resources = npc.resources;
    faction.threatLevel = std::clamp(npc.threatLevel, 0, 10);
    addTag(faction.metadata, "from-npc-legacy-text");

    const EntityId factionId = faction.metadata.id;
    _context.campaign.factions.push_back(faction);
    _selectedFactionIndex = static_cast<int>(_context.campaign.factions.size()) - 1;
    npc.factionId = factionId;
    npc.faction = factionName;
    npc.metadata.touch();
    _context.campaign.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::createOrLinkDeityFromSelectedNpcName()
{
    if (retryPendingSave())
        return;

    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    const std::string deityName = trimCopy(npc.name);
    if (deityName.empty())
        return;

    const int existingDeityIndex = findDeityIndexByName(_context.campaign.deities, deityName);
    if (existingDeityIndex >= 0)
    {
        npc.deityId = _context.campaign.deities[existingDeityIndex].metadata.id;
        npc.metadata.touch();
        _selectedDeityIndex = existingDeityIndex;
        saveCampaign();
        return;
    }

    DeityModel deity;
    deity.name = deityName;
    deity.powerType = npc.category == NPCCategory::Deity ? "Deity/Patron" : "Patron";
    deity.domains = npc.role;
    deity.tenets = npc.goals;
    deity.worshipers = npc.resources;
    deity.relationships = npc.relationships;
    deity.gmNotes = npc.secrets.empty() ? npc.description : npc.secrets;
    deity.threatLevel = std::clamp(npc.threatLevel, 0, 10);
    addTag(deity.metadata, "from-npc-profile");

    const EntityId deityId = deity.metadata.id;
    _context.campaign.deities.push_back(deity);
    _selectedDeityIndex = static_cast<int>(_context.campaign.deities.size()) - 1;
    npc.deityId = deityId;
    npc.metadata.touch();
    _context.campaign.metadata.touch();
    saveCampaign();
}
