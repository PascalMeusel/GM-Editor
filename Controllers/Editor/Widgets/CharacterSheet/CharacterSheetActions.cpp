#include "Models/UX/CampaignNavigationResolver.hpp"
#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#endif

#include "CharacterSheetController.hpp"
#include "Models/Character/CharacterTransfer.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace
{
using CharacterTransfer::serializeCharacterForExport;
using CharacterTransfer::deserializeCharacterForImport;

bool hasTag(const EntityMetadata &metadata, const std::string &tag)
{
    return std::find(metadata.tags.begin(), metadata.tags.end(), tag) != metadata.tags.end();
}

void addTag(EntityMetadata &metadata, const std::string &tag)
{
    if (!hasTag(metadata, tag))
        metadata.tags.push_back(tag);
}

std::string sanitizePathSegment(const std::string &value)
{
    std::string sanitized;
    for (const char c : value)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
            sanitized.push_back(c);
        else if (c == ' ')
            sanitized.push_back('_');
    }
    return sanitized.empty() ? "asset" : sanitized;
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

void beginConcentration(CharacterModel &character, const SpellModel &spell)
{
    if (!spell.concentration)
        return;

    character.conditions.erase(std::remove_if(character.conditions.begin(), character.conditions.end(), [](const std::string &condition) {
                                   return lowerCopy(condition).find("concentrat") != std::string::npos;
                               }),
                               character.conditions.end());
    const std::string concentration = "Concentrating: " + spell.name;
    if (std::find(character.conditions.begin(), character.conditions.end(), concentration) == character.conditions.end())
        character.conditions.push_back(concentration);
}

std::string trimCopy(const std::string &value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }).base();
    if (first >= last)
        return "";
    return std::string(first, last);
}


}

void CharacterSheetController::addFeatureActionToSelectedCharacter()
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    if (_featureActionDraft.name.empty())
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    MonsterActionModel action = _featureActionDraft;
    if (action.actionKind.empty() || action.actionKind == "Attack")
        action.actionKind = "Feature";
    action.sourceLabel = std::string(character.nameBuffer);
    action.sourceEntityId = character.metadata.id;
    action.reachFeet = std::max(0, action.reachFeet);
    action.normalRangeFeet = std::max(action.reachFeet, action.normalRangeFeet);
    action.rangeFeet = std::max(action.normalRangeFeet, action.rangeFeet);
    character.featureActions.push_back(action);
    character.metadata.touch();
    saveCampaign();
    _featureActionDraft = MonsterActionModel{};
    _featureActionDraft.actionKind = "Feature";
}

void CharacterSheetController::removeFeatureActionFromSelectedCharacter(int actionIndex)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (actionIndex < 0 || actionIndex >= static_cast<int>(character.featureActions.size()))
        return;

    character.featureActions.erase(character.featureActions.begin() + actionIndex);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::addResourceTrackerToSelectedCharacter()
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    normalizeResourceTracker(_resourceTrackerDraft);
    if (_resourceTrackerDraft.name.empty())
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    _resourceTrackerDraft.metadata.touch();
    character.resourceTrackers.push_back(_resourceTrackerDraft);
    character.metadata.touch();
    saveCampaign();
    _resourceTrackerDraft = ResourceTrackerModel{};
}

void CharacterSheetController::removeResourceTrackerFromSelectedCharacter(int trackerIndex)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (trackerIndex < 0 || trackerIndex >= static_cast<int>(character.resourceTrackers.size()))
        return;

    const EntityId trackerId = character.resourceTrackers[static_cast<std::size_t>(trackerIndex)].metadata.id;
    character.resourceTrackers.erase(character.resourceTrackers.begin() + trackerIndex);
    for (MonsterActionModel &action : character.featureActions)
    {
        if (action.linkedResourceId == trackerId)
            action.linkedResourceId.clear();
    }
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterResourceUsed(int trackerIndex, int used)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (trackerIndex < 0 || trackerIndex >= static_cast<int>(character.resourceTrackers.size()))
        return;

    ResourceTrackerModel &tracker = character.resourceTrackers[static_cast<std::size_t>(trackerIndex)];
    normalizeResourceTracker(tracker);
    tracker.used = std::clamp(used, 0, tracker.maximum);
    tracker.metadata.touch();
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::resetSelectedCharacterResource(int trackerIndex)
{
    setSelectedCharacterResourceUsed(trackerIndex, 0);
}

void CharacterSheetController::setSelectedCharacterLevel(int value)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (character.classLevels.size() > 1) {
        _errorMessage = gm::localization::trUi("Use advancement to change a multiclass character's progression."); return;
    }
    character.level = std::max(1, value);
    if (!character.classLevels.empty()) character.classLevels = {{character.selectedClass, character.level}};
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterAttribute(const std::string &name, int value)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.attributes[name] = std::clamp(value, 1, 30);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterRuleValue(const std::string &stableId,
                                                              const std::string &displayName,
                                                              int value)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()) || stableId.empty())
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    const int normalized = std::clamp(value, 1, 30);
    character.ruleValues[stableId] = normalized;
    if (!displayName.empty())
        character.attributes[displayName] = normalized;
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterCurrentHitPoints(int value)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    normalizeCharacterHitPoints(character);
    character.currentHitPoints = std::clamp(value, 0, character.maxHitPoints);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterMaxHitPoints(int value)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.maxHitPoints = std::max(1, value);
    character.currentHitPoints = std::clamp(character.currentHitPoints, 0, character.maxHitPoints);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::adjustSelectedCharacterHitPoints(int delta)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    adjustCharacterHitPoints(character, delta);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterTemporaryHitPoints(int value)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.temporaryHitPoints = std::max(0, value);
    character.metadata.touch();
    saveCampaign();
}
