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

void CharacterSheetController::setSelectedCharacterName(const std::string &name)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
#if defined(_MSC_VER)
    strncpy_s(character.nameBuffer, sizeof(character.nameBuffer), name.c_str(), _TRUNCATE);
#else
    std::strncpy(character.nameBuffer, name.c_str(), sizeof(character.nameBuffer) - 1);
    character.nameBuffer[sizeof(character.nameBuffer) - 1] = '\0';
#endif
    character.inventory.name = std::string(character.nameBuffer) + " – " + gm::localization::trUi("Inventory");
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterRace(const std::string &race)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.selectedRace = race;
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterClass(const std::string &className)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (character.classLevels.size() > 1) {
        _errorMessage = gm::localization::trUi("Use advancement to change a multiclass character's progression."); return;
    }
    if (!character.classLevels.empty()) character.classLevels = {{className, character.level}};
    character.selectedClass = className;
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterBackground(const std::string &background)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.background = background;
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterSpeedFeet(int speedFeet)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.speedFeet = std::max(0, speedFeet);
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterBackstory(const std::string &backstory)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.backstory = backstory;
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterPlayerNotes(const std::string &notes)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.playerNotes = notes;
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::setSelectedCharacterGmNotes(const std::string &notes)
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.gmNotes = notes;
    character.metadata.touch();
    saveCampaign();
}

void CharacterSheetController::ensureNotesDraftForSelectedCharacter()
{
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
    {
        if (selectedCharacterNotesDraftDirty()) return;
        _notesDraftCharacterId.clear();
        _backstoryDraft.clear();
        _playerNotesDraft.clear();
        _gmNotesDraft.clear();
        _savedBackstory.clear(); _savedPlayerNotes.clear(); _savedGmNotes.clear();
        return;
    }

    const CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    if (_notesDraftCharacterId == character.metadata.id && selectedCharacterNotesDraftDirty())
        return;

    _notesDraftCharacterId = character.metadata.id;
    _savedBackstory = _backstoryDraft = character.backstory;
    _savedPlayerNotes = _playerNotesDraft = character.playerNotes;
    _savedGmNotes = _gmNotesDraft = character.gmNotes;
}

bool CharacterSheetController::selectedCharacterNotesDraftDirty() const
{
    return _state.notes.dirty();
}

bool CharacterSheetController::saveSelectedCharacterNotesDraft()
{
    if(!draftRecoveryReady())return false;
    const EntityId id = _notesDraftCharacterId;
    if (!gm::ux::resolveNavigationEntity(_currentCampaign, id).available)
    {
        _errorMessage = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
            ? "Diese Figur ist nicht mehr verfügbar. Der Notizentwurf bleibt erhalten."
            : "This character is unavailable. The notes draft is retained.";
        return false;
    }
    Campaign proposed = _currentCampaign;
    const auto found = std::find_if(proposed.characters.begin(), proposed.characters.end(),
        [&](const CharacterModel &character) { return character.metadata.id == id; });
    if (found == proposed.characters.end()) return false;
    const bool alreadySaved=found->backstory==_backstoryDraft&&found->playerNotes==_playerNotesDraft&&found->gmNotes==_gmNotesDraft;
    if (!alreadySaved && (found->backstory != _savedBackstory || found->playerNotes != _savedPlayerNotes || found->gmNotes != _savedGmNotes))
    {
        _errorMessage = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
            ? "Die gespeicherten Notizen wurden an anderer Stelle geändert. Dein Entwurf bleibt erhalten."
            : "Saved notes changed elsewhere. Your draft is retained.";
        return false;
    }
    found->backstory = _backstoryDraft;
    found->playerNotes = _playerNotesDraft;
    found->gmNotes = _gmNotesDraft;
    found->metadata.touch();
    const std::string name = found->nameBuffer;
    if (!ProjectSaveService::getInstance().saveCampaign(proposed, "save character notes draft"))
    {
        _errorMessage = (gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
            ? std::string("Notizen nicht gespeichert. Versuche Speichern erneut. ")
            : std::string("Notes were not saved. Retry saving. ")) + ProjectSaveService::getInstance().state().lastError;
        return false;
    }
    _currentCampaign = std::move(proposed);
    _savedBackstory = _backstoryDraft;
    _savedPlayerNotes = _playerNotesDraft;
    _savedGmNotes = _gmNotesDraft;
    _errorMessage.clear();
    _statusMessage = std::string(gm::localization::trUi("Notes saved for")) + " " + name + ".";
    if(_recoveryCommitted)_recoveryCommitted();
    return true;
}

void CharacterSheetController::revertSelectedCharacterNotesDraft()
{
    if(_recoveryDiscard&&!_recoveryDiscard())return;
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
    {
        _state.notes.clear();
        return;
    }

    const CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    _notesDraftCharacterId = character.metadata.id;
    _savedBackstory = _backstoryDraft = character.backstory;
    _savedPlayerNotes = _playerNotesDraft = character.playerNotes;
    _savedGmNotes = _gmNotesDraft = character.gmNotes;
    _statusMessage = std::string(gm::localization::trUi("Unsaved notes reverted for")) + " " + std::string(character.nameBuffer) + ".";
}
