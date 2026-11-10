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

void NPCOverviewController::createDeity()
{
    if (retryPendingSave())
        return;

    DeityModel deity;
    deity.name = gm::localization::trUi("New Deity");
    deity.powerType = "Deity";
    _context.campaign.deities.push_back(deity);
    _selectedDeityIndex = static_cast<int>(_context.campaign.deities.size()) - 1;
    _context.campaign.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeityName(const std::string &name)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.name = name;
    deity.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeityPowerType(const std::string &powerType)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.powerType = powerType;
    deity.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeityPantheon(const std::string &pantheon)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.pantheon = pantheon;
    deity.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeityDomains(const std::string &domains)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.domains = domains;
    deity.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeitySymbol(const std::string &symbol)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.symbol = symbol;
    deity.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeityAlignment(const std::string &alignment)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.alignment = alignment;
    deity.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeityTenets(const std::string &tenets)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.tenets = tenets;
    deity.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeityWorshipers(const std::string &worshipers)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.worshipers = worshipers;
    deity.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeityRelationships(const std::string &relationships)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.relationships = relationships;
    deity.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeityGmNotes(const std::string &notes)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.gmNotes = notes;
    deity.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeityInfluence(int value)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.influence = std::clamp(value, 0, 10);
    deity.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedDeityThreatLevel(int value)
{
    if (_selectedDeityIndex < 0 || _selectedDeityIndex >= static_cast<int>(_context.campaign.deities.size()))
        return;

    DeityModel &deity = _context.campaign.deities[_selectedDeityIndex];
    deity.threatLevel = std::clamp(value, 0, 10);
    deity.metadata.touch();
    saveCampaign();
}
