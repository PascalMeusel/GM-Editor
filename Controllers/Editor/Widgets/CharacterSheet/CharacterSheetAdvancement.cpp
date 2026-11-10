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

CharacterAdvancement::Request CharacterSheetController::advancementRequest() const
{
    return _state.advancement.request();
}

void CharacterSheetController::requestLevelUpSelectedCharacter()
{
    if (selectedCharacterNotesDraftDirty() && !saveSelectedCharacterNotesDraft()) return;
    _errorMessage.clear(); _statusMessage.clear();
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size())) return;
    const auto &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (_levelUpPending && _levelUpCharacterId == character.metadata.id) return;
    _levelUpBase = character;
    _levelUpCharacterId = character.metadata.id;
    _levelUpRulesetId = _currentCampaign.ruleSet ? _currentCampaign.ruleSet->getId() : "";
    _levelUpRulesetVersion = _currentCampaign.ruleSet ? _currentCampaign.ruleSet->getVersionId() : "";
    _levelUpTargetLevel = character.level < 1000 ? character.level + 1 : character.level;
    _levelUpManualHitPoints = !_currentCampaign.ruleSet ||
        _currentCampaign.ruleSet->advancementProfile(character.selectedClass).fixedHitPointsPerLevel <= 0;
    _levelUpHitPointGain = suggestedLevelUpHitPointGain(character);
    _levelUpRestoreHealth = true;
    _levelUpFeatures.clear();
    _levelUpChoices = {}; _levelUpChoices.guided = true;
    _levelUpChoices.advancingClass = character.selectedClass;
    _levelUpChoices.slotMaximums = character.spellcasting.slotMaximums;
    _levelUpChoices.slotMaximums.resize(10, 0);
    const bool emptySlots = std::all_of(_levelUpChoices.slotMaximums.begin(), _levelUpChoices.slotMaximums.end(), [](int n){return n == 0;});
    _levelUpChoices.automaticSlots = emptySlots || _levelUpChoices.slotMaximums == CharacterAdvancement::standardSlots(character);
    _levelUpStep = 0; _levelUpSpellSearch.clear();
    _levelUpPending = true;
}

void CharacterSheetController::applyLevelUpToSelectedCharacter()
{
    _errorMessage.clear(); _statusMessage.clear();
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto found = std::find_if(_currentCampaign.characters.begin(), _currentCampaign.characters.end(),
        [&](const auto &item) { return item.metadata.id == _levelUpCharacterId; });
    if (!_levelUpPending || found == _currentCampaign.characters.end() || isCharacterArchived(*found) || !_currentCampaign.ruleSet) {
        _errorMessage = german ? "Figur oder Regelwerk ist nicht mehr verfügbar. Der Entwurf bleibt erhalten." :
            "The character or ruleset is unavailable. Your draft is retained."; return;
    }
    if (_levelUpRulesetId != _currentCampaign.ruleSet->getId() || _levelUpRulesetVersion != _currentCampaign.ruleSet->getVersionId() ||
        serializeCharacterForExport(*found) != serializeCharacterForExport(_levelUpBase)) {
        _errorMessage = german ? "Die Figur wurde inzwischen geändert. Schließe den Entwurf und öffne den Stufenaufstieg erneut." :
            "The character changed. Cancel this draft and reopen level advancement."; return;
    }
    Campaign staged = _currentCampaign;
    auto &updated = staged.characters[static_cast<std::size_t>(found - _currentCampaign.characters.begin())];
    const auto spells = getAvailableSpells(_currentCampaign);
    if (!CharacterAdvancement::apply(updated, *_currentCampaign.ruleSet, advancementRequest(), _errorMessage, &spells)) { _errorMessage = gm::localization::trUi(_errorMessage.c_str()); return; }
    const auto result = ProjectSaveService::getInstance().saveCampaignDetailed(staged, "character advancement");
    if (!result.committed()) {
        _errorMessage = german ? "Nicht gespeichert. Der Stufenaufstieg bleibt als Entwurf erhalten. Erneut anwenden, um zu speichern." :
            "Not saved. Level advancement is retained as a draft. Apply again to retry.";
        return;
    }
    // The view may retain a reference to this character until the frame ends.
    // Assign into the existing slot instead of replacing the campaign vector.
    *found = std::move(updated);
    _currentCampaign.metadata = staged.metadata;
    _levelUpPending = false;
    _statusMessage = german ? "Stufenaufstieg gespeichert." : "Level advancement saved.";
}

int CharacterSheetController::suggestedLevelUpHitPointGain(const CharacterModel &character) const
{
    if (!_currentCampaign.ruleSet) return 0;
    CharacterAdvancement::Request request;
    request.targetLevel = character.level < 1000 ? character.level + 1 : character.level;
    const auto preview = CharacterAdvancement::preview(character, *_currentCampaign.ruleSet, request);
    return preview.valid ? preview.hitPointGain : 0;
}

void CharacterSheetController::toggleInventoryEntryEquipped(int entryIndex)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    InventoryModel &inventory = _currentCampaign.characters[_selectedCharacterIndex].inventory;
    if (entryIndex < 0 || entryIndex >= static_cast<int>(inventory.entries.size()))
        return;

    inventory.entries[entryIndex].equipped = !inventory.entries[entryIndex].equipped;
    _currentCampaign.characters[_selectedCharacterIndex].metadata.touch();
    saveCampaign();
}

void CharacterSheetController::removeInventoryEntry(int entryIndex)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    InventoryModel &inventory = _currentCampaign.characters[_selectedCharacterIndex].inventory;
    if (entryIndex < 0 || entryIndex >= static_cast<int>(inventory.entries.size()))
        return;

    inventory.entries.erase(inventory.entries.begin() + entryIndex);
    _currentCampaign.characters[_selectedCharacterIndex].metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setInventoryEntryQuantity(int entryIndex, int quantity)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    InventoryModel &inventory = character.inventory;
    if (entryIndex < 0 || entryIndex >= static_cast<int>(inventory.entries.size()))
        return;

    inventory.entries[entryIndex].quantity = std::max(1, quantity);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::addItemToSelectedCharacter(const EntityId &itemId, int quantity, bool equipped)
{
    _errorMessage.clear();
    _statusMessage.clear();
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    std::vector<ItemModel> items;
    if (_currentCampaign.ruleSet)
        items = _currentCampaign.ruleSet->getItems();
    items.insert(items.end(), _currentCampaign.customItems.begin(), _currentCampaign.customItems.end());
    const ItemModel *item = findItemById(items, itemId);
    if (!item)
    {
        _errorMessage = gm::localization::trUi("The selected item is no longer available.");
        return;
    }

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    InventoryModel &inventory = character.inventory;
    const int normalizedQuantity = std::max(1, quantity);
    const std::string characterName = character.nameBuffer[0] == '\0' ? std::string(gm::localization::trUi("character")) : std::string(character.nameBuffer);
    for (InventoryEntryModel &entry : inventory.entries)
    {
        if (entry.itemId == item->metadata.id && item->stackable)
        {
            entry.quantity += normalizedQuantity;
            entry.equipped = entry.equipped || equipped;
            character.metadata.touch();
            if (!saveCampaign()) return;
            _statusMessage = std::string(gm::localization::trUi("Added")) + ": " + std::to_string(normalizedQuantity) + " " + item->name + " " +
                             gm::localization::trUi("to") + " " + characterName + ".";
            return;
        }
    }

    InventoryEntryModel entry;
    entry.itemId = item->metadata.id;
    entry.quantity = normalizedQuantity;
    entry.equipped = equipped;
    if (item->maxCharges > 0)
        entry.charges = item->maxCharges;
    if (item->maxDurability > 0)
        entry.durability = item->maxDurability;
    if (item->ammunition)
        entry.ammunitionRemaining = normalizedQuantity;
    inventory.entries.push_back(entry);
    character.metadata.touch();
    if (!saveCampaign()) return;
    _statusMessage = std::string(gm::localization::trUi("Added")) + ": " + std::to_string(normalizedQuantity) + " " + item->name + " " +
                     gm::localization::trUi("to") + " " + characterName + ".";
}
