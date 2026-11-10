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

void NPCOverviewController::setSelectedNpcName(const std::string &name)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.name = name;
    npc.inventory.name = npc.name + " Inventory";
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcFaction(const std::string &faction)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.faction = faction;
    npc.factionId.clear();
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcRole(const std::string &role)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.role = role;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcAttitude(const std::string &attitude)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.attitudeTowardParty = attitude;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcSecrets(const std::string &secrets)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.secrets = secrets;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcGoals(const std::string &goals)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.goals = goals;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcResources(const std::string &resources)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.resources = resources;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcLocation(const std::string &location)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.threatLocation = location;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcRelationships(const std::string &relationships)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.relationships = relationships;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcThreatLevel(int value)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.threatLevel = std::clamp(value, 0, 10);
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcDescription(const std::string &description)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.description = description;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcDialogueNotes(const std::string &dialogueNotes)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.dialogueNotes = dialogueNotes;
    npc.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedNpcAttribute(const std::string &name, int value)
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    NPCModel &npc = _context.campaign.npcs[_selectedNpcIndex];
    npc.attributes[name] = std::clamp(value, 1, 30);
    npc.metadata.touch();
    saveCampaign();
}
