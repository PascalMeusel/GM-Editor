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

void CharacterSheetController::addCharacterDetailListEntry(CharacterDetailList list, const std::string &value)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    const std::string trimmed = trimCopy(value);
    if (trimmed.empty())
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    std::vector<std::string> &entries = mutableCharacterDetailList(character, list);
    if (std::find(entries.begin(), entries.end(), trimmed) == entries.end())
        entries.push_back(trimmed);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::removeCharacterDetailListEntry(CharacterDetailList list, int index)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    std::vector<std::string> &entries = mutableCharacterDetailList(character, list);
    if (index < 0 || index >= static_cast<int>(entries.size()))
        return;

    entries.erase(entries.begin() + index);
    character.metadata.touch();
    saveCampaign();
}

std::vector<std::string> CharacterSheetController::getRelationshipTargetTypes() const
{
    return {"Player", "NPC", "Item", "Spell", "Map", "Story Arc"};
}

std::vector<CharacterSheetController::RelationshipTargetOption> CharacterSheetController::getRelationshipTargetOptions(const std::string &type) const
{
    std::vector<RelationshipTargetOption> options;
    if (type == "Player")
    {
        for (const CharacterModel &character : _currentCampaign.characters)
            options.push_back({type, character.metadata.id, character.nameBuffer});
    }
    else if (type == "NPC")
    {
        for (const NPCModel &npc : _currentCampaign.npcs)
            options.push_back({type, npc.metadata.id, npc.name.empty() ? gm::localization::trUi("Unnamed NPC") : npc.name});
    }
    else if (type == "Item")
    {
        for (const ItemModel &item : _currentCampaign.customItems)
            options.push_back({type, item.metadata.id, item.name.empty() ? gm::localization::trUi("Unnamed Item") : item.name});
    }
    else if (type == "Spell")
    {
        for (const SpellModel &spell : getAvailableSpells(_currentCampaign))
            options.push_back({type, spell.metadata.id, spell.name.empty() ? gm::localization::trUi("Unnamed Spell") : spell.name});
    }
    else if (type == "Map")
    {
        for (const MapModel &map : _currentCampaign.maps)
            options.push_back({type, map.metadata.id, map.name.empty() ? gm::localization::trUi("Unnamed Map") : map.name});
    }
    else if (type == "Story Arc")
    {
        for (const StoryThreadModel &thread : _currentCampaign.storyThreads)
            options.push_back({type, thread.metadata.id, thread.title.empty() ? gm::localization::trUi("Untitled Story") : thread.title});
    }

    std::sort(options.begin(), options.end(), [](const RelationshipTargetOption &left, const RelationshipTargetOption &right) {
        return left.label < right.label;
    });
    return options;
}

std::string CharacterSheetController::describeRelationshipTarget(const CharacterRelationshipModel &relationship) const
{
    const std::vector<RelationshipTargetOption> options = getRelationshipTargetOptions(relationship.targetEntityType);
    const auto found = std::find_if(options.begin(), options.end(), [&](const RelationshipTargetOption &option) {
        return option.id == relationship.targetEntityId;
    });
    if (found != options.end())
        return found->label;
    return std::string(gm::localization::trUi("<Missing")) + " " +
           gm::localization::trUi(relationship.targetEntityType.c_str()) + ">";
}

std::vector<std::string> CharacterSheetController::getSelectedCharacterValidationWarnings() const
{
    std::vector<std::string> warnings;
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return warnings;

    const CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (trimCopy(character.nameBuffer).empty())
        warnings.push_back(gm::localization::trUi("Character name is empty."));
    if (character.level < 1)
        warnings.push_back(gm::localization::trUi("Level must be at least 1."));
    if (_currentCampaign.ruleSet)
    {
        if (character.selectedClass.empty())
            warnings.push_back(gm::localization::trUi("Class is not set."));
        if (character.selectedRace.empty())
            warnings.push_back(gm::localization::trUi("Species is not set."));
        for (const std::string &ability : _currentCampaign.ruleSet->getAttributes())
        {
            const auto found = character.attributes.find(ability);
            if (found == character.attributes.end())
                warnings.push_back(std::string(gm::localization::trUi("Missing ability score:")) + " " + gm::localization::trUi(ability.c_str()) + ".");
            else if (found->second < 1 || found->second > 30)
                warnings.push_back(std::string(gm::localization::trUi(ability.c_str())) + " " + gm::localization::trUi("score is outside the supported 1-30 range."));
        }
    }
    if (character.maxHitPoints < 1)
        warnings.push_back(gm::localization::trUi("Max health must be at least 1."));
    if (character.currentHitPoints < 0 || character.currentHitPoints > std::max(1, character.maxHitPoints))
        warnings.push_back(gm::localization::trUi("Current health is outside the valid 0-max health range."));
    if (character.speedFeet < 0)
        warnings.push_back(gm::localization::trUi("Speed cannot be negative."));

    std::vector<ItemModel> items;
    if (_currentCampaign.ruleSet)
        items = _currentCampaign.ruleSet->getItems();
    items.insert(items.end(), _currentCampaign.customItems.begin(), _currentCampaign.customItems.end());
    for (const InventoryEntryModel &entry : character.inventory.entries)
    {
        if (!findItemById(items, entry.itemId))
            warnings.push_back(gm::localization::trUi("Inventory contains a missing item reference."));
        if (entry.quantity < 1)
            warnings.push_back(gm::localization::trUi("Inventory contains an item with quantity below 1."));
    }

    if (character.spellcasting.enabled)
    {
        if (character.spellcasting.ability.empty())
            warnings.push_back(gm::localization::trUi("Spellcasting is enabled but no spellcasting ability is set."));
        else if (_currentCampaign.ruleSet)
        {
            const std::vector<std::string> abilities = _currentCampaign.ruleSet->getAttributes();
            if (std::find(abilities.begin(), abilities.end(), character.spellcasting.ability) == abilities.end())
                warnings.push_back(gm::localization::trUi("Spellcasting ability is not part of the active ruleset."));
        }
        CharacterSpellcastingModel spellcasting = character.spellcasting;
        for (int level = 1; level <= 9; ++level)
        {
            const int maximum = spellcasting.slotMaximums[static_cast<std::size_t>(level)];
            const int used = spellcasting.slotUsed[static_cast<std::size_t>(level)];
            if (used > maximum)
                warnings.push_back(std::string(gm::localization::trUi("Used spell slots exceed maximum at level")) + " " + std::to_string(level) + ".");
        }
        normalizeSpellcasting(spellcasting);
        const std::vector<SpellModel> availableSpells = getAvailableSpells(_currentCampaign);
        for (const CharacterSpellEntryModel &entry : spellcasting.spells)
        {
            if (!findSpellById(availableSpells, entry.spellId))
                warnings.push_back(gm::localization::trUi("Spellbook contains a missing spell reference."));
        }
    }

    for (const CharacterRelationshipModel &relationship : character.relationships)
    {
        const std::vector<RelationshipTargetOption> options = getRelationshipTargetOptions(relationship.targetEntityType);
        const bool exists = std::any_of(options.begin(), options.end(), [&](const RelationshipTargetOption &option) {
            return option.id == relationship.targetEntityId;
        });
        if (!exists)
            warnings.push_back(std::string(gm::localization::trUi("Relationship target is missing:")) + " " + gm::localization::trUi(relationship.targetEntityType.c_str()) + ".");
    }

    if (!character.portraitAssetId.empty() && !std::filesystem::exists(resolvePortraitPath(character)))
        warnings.push_back(gm::localization::trUi("Portrait asset path is missing."));

    std::sort(warnings.begin(), warnings.end());
    warnings.erase(std::unique(warnings.begin(), warnings.end()), warnings.end());
    return warnings;
}

void CharacterSheetController::addSelectedCharacterRelationship(const std::string &targetType, const EntityId &targetId, const std::string &label, const std::string &notes)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()) || targetId.empty())
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    CharacterRelationshipModel relationship;
    relationship.targetEntityType = targetType;
    relationship.targetEntityId = targetId;
    relationship.label = trimCopy(label);
    relationship.notes = notes;
    character.relationships.push_back(relationship);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::removeSelectedCharacterRelationship(int relationshipIndex)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (relationshipIndex < 0 || relationshipIndex >= static_cast<int>(character.relationships.size()))
        return;

    character.relationships.erase(character.relationships.begin() + relationshipIndex);
    character.metadata.touch();
    saveCampaign();
}
