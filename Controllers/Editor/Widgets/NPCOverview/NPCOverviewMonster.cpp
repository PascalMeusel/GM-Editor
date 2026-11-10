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

void NPCOverviewController::setSelectedMonsterName(const std::string &name)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    monster.name = name;
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterSize(const std::string &size)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    monster.size = size;
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterType(const std::string &type)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    monster.type = type;
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterChallengeRating(const std::string &challengeRating)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    monster.challengeRating = challengeRating;
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterArmorClass(int value)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    monster.armorClass = std::max(1, value);
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterArmorDamageReduction(int value)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    monster.armorDamageReduction = std::max(0, value);
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterHitPoints(int value)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    monster.hitPoints = std::max(1, value);
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterSpeedFeet(int value)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    monster.speedFeet = std::max(0, value);
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterExperience(int value)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    monster.experience = std::max(0, value);
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterAttribute(const std::string &name, int value)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    monster.attributes[name] = std::clamp(value, 1, 30);
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterActionName(int actionIndex, const std::string &name)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    if (actionIndex < 0 || actionIndex >= static_cast<int>(monster.actions.size()))
        return;

    monster.actions[static_cast<std::size_t>(actionIndex)].name = name;
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterActionReachFeet(int actionIndex, int value)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    if (actionIndex < 0 || actionIndex >= static_cast<int>(monster.actions.size()))
        return;

    MonsterActionModel &action = monster.actions[static_cast<std::size_t>(actionIndex)];
    action.reachFeet = std::max(0, value);
    action.rangeFeet = std::max(action.reachFeet, action.rangeFeet);
    action.normalRangeFeet = std::max(action.reachFeet, action.normalRangeFeet);
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterActionRangeFeet(int actionIndex, int value)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    if (actionIndex < 0 || actionIndex >= static_cast<int>(monster.actions.size()))
        return;

    MonsterActionModel &action = monster.actions[static_cast<std::size_t>(actionIndex)];
    action.rangeFeet = std::max(action.reachFeet, value);
    action.normalRangeFeet = std::clamp(action.normalRangeFeet, action.reachFeet, action.rangeFeet);
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterActionAttackBonus(int actionIndex, const std::string &attackBonus)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    if (actionIndex < 0 || actionIndex >= static_cast<int>(monster.actions.size()))
        return;

    monster.actions[static_cast<std::size_t>(actionIndex)].attackBonus = attackBonus;
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterActionDamage(int actionIndex, const std::string &damage)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    if (actionIndex < 0 || actionIndex >= static_cast<int>(monster.actions.size()))
        return;

    monster.actions[static_cast<std::size_t>(actionIndex)].damage = damage;
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::setSelectedMonsterActionNotes(int actionIndex, const std::string &notes)
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    if (actionIndex < 0 || actionIndex >= static_cast<int>(monster.actions.size()))
        return;

    monster.actions[static_cast<std::size_t>(actionIndex)].notes = notes;
    monster.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::addActionToSelectedMonster()
{
    if (retryPendingSave())
        return;

    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    if (_monsterActionDraft.name.empty())
        _monsterActionDraft.name = gm::localization::trUi("Attack");
    _monsterActionDraft.reachFeet = std::max(0, _monsterActionDraft.reachFeet);
    _monsterActionDraft.rangeFeet = std::max(_monsterActionDraft.reachFeet, _monsterActionDraft.rangeFeet);
    _monsterActionDraft.normalRangeFeet = std::clamp(_monsterActionDraft.normalRangeFeet, _monsterActionDraft.reachFeet, _monsterActionDraft.rangeFeet);
    monster.actions.push_back(_monsterActionDraft);
    _selectedMonsterActionIndex = static_cast<int>(monster.actions.size()) - 1;
    monster.metadata.touch();
    _actionDraftPendingSave = true;
    saveCampaign();
}

void NPCOverviewController::removeSelectedMonsterAction(int actionIndex)
{
    if (retryPendingSave())
        return;

    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    if (actionIndex < 0 || actionIndex >= static_cast<int>(monster.actions.size()))
        return;

    monster.actions.erase(monster.actions.begin() + actionIndex);
    if (monster.actions.empty())
        _selectedMonsterActionIndex = -1;
    else if (_selectedMonsterActionIndex == actionIndex)
        _selectedMonsterActionIndex = std::min(actionIndex, static_cast<int>(monster.actions.size()) - 1);
    else if (_selectedMonsterActionIndex > actionIndex)
        --_selectedMonsterActionIndex;
    clampSelectedMonsterActionIndex();
    monster.metadata.touch();
    saveCampaign();
}
