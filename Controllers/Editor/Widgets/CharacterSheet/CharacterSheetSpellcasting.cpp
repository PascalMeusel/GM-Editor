#include "Models/UX/CampaignNavigationResolver.hpp"
#include "Views/Editor/Widgets/Magic/SpellcastingValueUi.hpp"
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

void CharacterSheetController::setSpellcastingEnabled(bool enabled)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.spellcasting.enabled = enabled;
    normalizeSpellcasting(character.spellcasting);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSpellcastingAbility(const std::string &ability)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.spellcasting.ability = ability;
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSpellAttackBonus(int value)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.spellcasting.spellAttackBonus = value;
    character.spellcasting.spellAttackMode = SpellValueMode::Manual;
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSpellSaveDc(int value)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.spellcasting.spellSaveDc = std::max(1, value);
    character.spellcasting.spellSaveDcMode = SpellValueMode::Manual;
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSpellAttackMode(SpellValueMode mode)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size())) return;
    auto &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (character.spellcasting.spellAttackMode == mode) return;
    character.spellcasting.spellAttackMode = mode;
    character.metadata.touch(); saveCampaign();
}

void CharacterSheetController::setSpellSaveDcMode(SpellValueMode mode)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size())) return;
    auto &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (character.spellcasting.spellSaveDcMode == mode) return;
    character.spellcasting.spellSaveDcMode = mode;
    character.metadata.touch(); saveCampaign();
}

void CharacterSheetController::setSpellSlotMaximum(int spellLevel, int value)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()) || spellLevel < 1 || spellLevel > 9)
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    normalizeSpellcasting(character.spellcasting);
    character.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)] = std::max(0, value);
    character.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)] = std::clamp(character.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)], 0, character.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)]);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSpellSlotUsed(int spellLevel, int value)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()) || spellLevel < 1 || spellLevel > 9)
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    normalizeSpellcasting(character.spellcasting);
    character.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)] = std::clamp(value, 0, character.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)]);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::resetSpellSlots()
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    normalizeSpellcasting(character.spellcasting);
    std::fill(character.spellcasting.slotUsed.begin(), character.spellcasting.slotUsed.end(), 0);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::addSpellToSelectedCharacter(const EntityId &spellId)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()) || spellId.empty())
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    normalizeSpellcasting(character.spellcasting);
    const std::vector<SpellModel> availableSpells = getAvailableSpells(_currentCampaign);
    const SpellModel *spell = findSpellById(availableSpells, spellId);
    if (!spell)
        return;
    const auto found = std::find_if(character.spellcasting.spells.begin(), character.spellcasting.spells.end(), [&](const CharacterSpellEntryModel &entry) {
        return entry.spellId == spellId;
    });
    if (found != character.spellcasting.spells.end())
        return;

    CharacterSpellEntryModel entry;
    entry.spellId = spellId;
    entry.prepared = true;
    entry.source = std::find_if(_currentCampaign.customSpells.begin(), _currentCampaign.customSpells.end(), [&](const SpellModel &customSpell) {
                       return customSpell.metadata.id == spellId;
                   }) != _currentCampaign.customSpells.end()
                       ? "Campaign"
                       : (_currentCampaign.ruleSet ? _currentCampaign.ruleSet->getName() : "Ruleset");
    character.spellcasting.enabled = true;
    character.spellcasting.spells.push_back(entry);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::togglePreparedSpell(int spellEntryIndex)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (spellEntryIndex < 0 || spellEntryIndex >= static_cast<int>(character.spellcasting.spells.size()))
        return;
    character.spellcasting.spells[static_cast<std::size_t>(spellEntryIndex)].prepared = !character.spellcasting.spells[static_cast<std::size_t>(spellEntryIndex)].prepared;
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::removeSpellFromSelectedCharacter(int spellEntryIndex)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (spellEntryIndex < 0 || spellEntryIndex >= static_cast<int>(character.spellcasting.spells.size()))
        return;
    character.spellcasting.spells.erase(character.spellcasting.spells.begin() + spellEntryIndex);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::castSpellFromSelectedCharacter(int spellEntryIndex, int slotLevel, bool asRitual, bool usePactMagic)
{
    _errorMessage.clear();
    _statusMessage.clear();
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    Campaign staged = _currentCampaign;
    CharacterModel &character = staged.characters[_selectedCharacterIndex];
    if (spellEntryIndex < 0 || spellEntryIndex >= static_cast<int>(character.spellcasting.spells.size()))
        return;
    const std::vector<SpellModel> availableSpells = getAvailableSpells(_currentCampaign);
    const SpellModel *spell = findSpellById(availableSpells, character.spellcasting.spells[static_cast<std::size_t>(spellEntryIndex)].spellId);
    if (!spell)
    {
        _errorMessage = gm::localization::trUi("This spell no longer exists in the ruleset or campaign spell library.");
        return;
    }

    const auto &entry = character.spellcasting.spells[static_cast<std::size_t>(spellEntryIndex)];
    const auto casting = _currentCampaign.ruleSet ? CharacterAdvancement::spellcastingForEntry(character, *_currentCampaign.ruleSet, entry) : character.spellcasting;
    SpellcastingValues::Reason calculationReason = SpellcastingValues::Reason::None;
    if (!SpellcastingValues::readyForSpell(_currentCampaign, character.attributes, casting, character.level, *spell, &calculationReason)) {
        _errorMessage = SpellcastingValueUi::reasonText(calculationReason); return;
    }
    normalizeSpellcasting(character.spellcasting);
    const int effectiveSlotLevel = std::clamp(std::max(slotLevel, spell->level), 0, 9);
    if (asRitual && (!spell->ritual || spell->level <= 0))
    {
        _errorMessage = spell->name + " " + gm::localization::trUi("cannot be cast as a ritual.");
        return;
    }

    if (spell->level > 0 && !asRitual)
    {
        if (usePactMagic) {
            auto resource = std::find_if(character.resourceTrackers.begin(), character.resourceTrackers.end(), [](const auto &r) { return r.definitionId == "dnd-2024.pact-slots"; });
            if (resource == character.resourceTrackers.end() || resource->used >= resource->maximum || CharacterAdvancement::pactSlotLevel(character) < spell->level) {
                _errorMessage = gm::localization::trUi("No Pact Magic slot is available for this spell."); return;
            }
            ++resource->used;
        }
        else if (!_currentCampaign.ruleSet || !_currentCampaign.ruleSet->spendSpellSlot(character.spellcasting, effectiveSlotLevel, &_errorMessage))
        {
            if (_errorMessage.empty())
                _errorMessage = gm::localization::trUi("No slot level is available for this spell.");
            else
                _errorMessage = gm::localization::trUi(_errorMessage.c_str());
            return;
        }
    }

    beginConcentration(character, *spell);
    character.spellcasting.enabled = true;
    character.metadata.touch();
    const auto saved = ProjectSaveService::getInstance().saveCampaignDetailed(staged, "cast character spell");
    if (!saved.committed()) {
        _errorMessage = gm::localization::trUi("Spell was not saved. No resources were spent. Try again."); return;
    }
    // The spell table retains an entry reference until this frame ends.
    // Update only casting state; preserve the spellbook vector's storage.
    auto &live = _currentCampaign.characters[_selectedCharacterIndex];
    live.spellcasting.enabled = character.spellcasting.enabled;
    live.spellcasting.slotMaximums = character.spellcasting.slotMaximums;
    live.spellcasting.slotUsed = character.spellcasting.slotUsed;
    live.spellcasting.spellAttackBonus = character.spellcasting.spellAttackBonus;
    live.spellcasting.spellSaveDc = character.spellcasting.spellSaveDc;
    live.spellcasting.spellAttackMode = character.spellcasting.spellAttackMode;
    live.spellcasting.spellSaveDcMode = character.spellcasting.spellSaveDcMode;
    live.resourceTrackers = character.resourceTrackers;
    live.conditions = character.conditions;
    live.metadata = character.metadata;
    _currentCampaign.metadata = staged.metadata;
    if (asRitual)
        _statusMessage = std::string(gm::localization::trUi("Cast result")) + ": " + spell->name + " " + gm::localization::trUi("as a ritual.");
    else
        _statusMessage = std::string(gm::localization::trUi("Cast result")) + ": " + spell->name +
                         (spell->level > 0 ? " " + std::string(gm::localization::trUi("using slot level")) + " " + std::to_string(effectiveSlotLevel) + "." : ".");
    if (spell->concentration)
        _statusMessage += " " + std::string(gm::localization::trUi("Concentration started."));
}
