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

void NPCOverviewController::createFaction()
{
    if (retryPendingSave())
        return;

    FactionModel faction;
    faction.name = gm::localization::trUi("New Faction");
    faction.type = "Faction";
    _context.campaign.factions.push_back(faction);
    _selectedFactionIndex = static_cast<int>(_context.campaign.factions.size()) - 1;
    _context.campaign.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedFactionName(const std::string &name)
{
    if (_selectedFactionIndex < 0 || _selectedFactionIndex >= static_cast<int>(_context.campaign.factions.size()))
        return;

    FactionModel &faction = _context.campaign.factions[_selectedFactionIndex];
    faction.name = name;
    faction.metadata.touch();
    for (NPCModel &npc : _context.campaign.npcs)
    {
        if (npc.factionId == faction.metadata.id)
        {
            npc.faction = faction.name;
            npc.metadata.touch();
        }
    }
    saveCampaign();
}

void NPCOverviewController::setSelectedFactionType(const std::string &type)
{
    if (_selectedFactionIndex < 0 || _selectedFactionIndex >= static_cast<int>(_context.campaign.factions.size()))
        return;

    FactionModel &faction = _context.campaign.factions[_selectedFactionIndex];
    faction.type = type;
    faction.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedFactionSummary(const std::string &summary)
{
    if (_selectedFactionIndex < 0 || _selectedFactionIndex >= static_cast<int>(_context.campaign.factions.size()))
        return;

    FactionModel &faction = _context.campaign.factions[_selectedFactionIndex];
    faction.summary = summary;
    faction.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedFactionHeadquarters(const std::string &headquarters)
{
    if (_selectedFactionIndex < 0 || _selectedFactionIndex >= static_cast<int>(_context.campaign.factions.size()))
        return;

    FactionModel &faction = _context.campaign.factions[_selectedFactionIndex];
    faction.headquarters = headquarters;
    faction.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedFactionLeaderNpcId(const EntityId &leaderNpcId)
{
    if (_selectedFactionIndex < 0 || _selectedFactionIndex >= static_cast<int>(_context.campaign.factions.size()))
        return;

    FactionModel &faction = _context.campaign.factions[_selectedFactionIndex];
    faction.leaderNpcId = leaderNpcId;
    faction.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedFactionAttitude(const std::string &attitude)
{
    if (_selectedFactionIndex < 0 || _selectedFactionIndex >= static_cast<int>(_context.campaign.factions.size()))
        return;

    FactionModel &faction = _context.campaign.factions[_selectedFactionIndex];
    faction.attitudeTowardParty = attitude;
    faction.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedFactionGoals(const std::string &goals)
{
    if (_selectedFactionIndex < 0 || _selectedFactionIndex >= static_cast<int>(_context.campaign.factions.size()))
        return;

    FactionModel &faction = _context.campaign.factions[_selectedFactionIndex];
    faction.goals = goals;
    faction.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedFactionResources(const std::string &resources)
{
    if (_selectedFactionIndex < 0 || _selectedFactionIndex >= static_cast<int>(_context.campaign.factions.size()))
        return;

    FactionModel &faction = _context.campaign.factions[_selectedFactionIndex];
    faction.resources = resources;
    faction.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedFactionGmNotes(const std::string &notes)
{
    if (_selectedFactionIndex < 0 || _selectedFactionIndex >= static_cast<int>(_context.campaign.factions.size()))
        return;

    FactionModel &faction = _context.campaign.factions[_selectedFactionIndex];
    faction.gmNotes = notes;
    faction.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedFactionInfluence(int value)
{
    if (_selectedFactionIndex < 0 || _selectedFactionIndex >= static_cast<int>(_context.campaign.factions.size()))
        return;

    FactionModel &faction = _context.campaign.factions[_selectedFactionIndex];
    faction.influence = std::clamp(value, 0, 10);
    faction.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedFactionThreatLevel(int value)
{
    if (_selectedFactionIndex < 0 || _selectedFactionIndex >= static_cast<int>(_context.campaign.factions.size()))
        return;

    FactionModel &faction = _context.campaign.factions[_selectedFactionIndex];
    faction.threatLevel = std::clamp(value, 0, 10);
    faction.metadata.touch();
    saveCampaign();
}
