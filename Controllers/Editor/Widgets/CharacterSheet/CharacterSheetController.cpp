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

CharacterSheetController::CharacterSheetController(Campaign &currentCampaign) : _currentCampaign(currentCampaign)
{
    view = std::make_unique<CharacterSheetView>(*this);
}

void CharacterSheetController::update(std::shared_ptr<GLFWwindow> window)
{
    if (!m_isOpen)
        return;

    if (selectedCharacterNotesDraftDirty())
    {
        const auto found = std::find_if(_currentCampaign.characters.begin(), _currentCampaign.characters.end(),
            [&](const CharacterModel &character) { return character.metadata.id == _notesDraftCharacterId; });
        _selectedCharacterIndex = found == _currentCampaign.characters.end() ? -1 :
            static_cast<int>(std::distance(_currentCampaign.characters.begin(), found));
    }
    view->update();
    if (!view->isVisible())
        m_isOpen = false;
}

void CharacterSheetController::setOpenStoryThreadHandler(std::function<void(const EntityId &)> handler)
{
    _openStoryThreadHandler = std::move(handler);
}

bool CharacterSheetController::selectCharacterById(const EntityId &characterId)
{
    if (_levelUpPending && characterId != _levelUpCharacterId) return false;
    if (characterId.empty())
        return false;

    const auto found = std::find_if(_currentCampaign.characters.begin(), _currentCampaign.characters.end(), [&](const CharacterModel &character) {
        return character.metadata.id == characterId;
    });
    if (found == _currentCampaign.characters.end())
        return false;

    if (selectedCharacterNotesDraftDirty() && found->metadata.id != _notesDraftCharacterId)
    {
        _statusMessage = gm::localization::trUi("Save or revert draft notes before switching characters.");
        return false;
    }

    _selectedCharacterIndex = static_cast<int>(std::distance(_currentCampaign.characters.begin(), found));
    _pendingCharacterRemovalIndex = -1;
    ensureNotesDraftForSelectedCharacter();
    return true;
}

std::filesystem::path CharacterSheetController::resolvePortraitPath(const CharacterModel &character) const
{
    if (character.portraitAssetId.empty())
        return {};

    return ProjectAssets::resolveProjectPath(character.portraitAssetId);
}

void CharacterSheetController::openStoryThread(const EntityId &threadId)
{
    if (_openStoryThreadHandler)
        _openStoryThreadHandler(threadId);
}

void CharacterSheetController::duplicateSelectedCharacter()
{
    _errorMessage.clear();
    _statusMessage.clear();
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    const CharacterModel &source = _currentCampaign.characters[_selectedCharacterIndex];
    CharacterModel copy = source;
    copy.metadata = EntityMetadata{"character"};
    copy.metadata.tags = source.metadata.tags;
    const std::string sourceName = source.nameBuffer;
    const std::string copyName = std::string(gm::localization::trUi("Copy of")) + " " +
                                 (sourceName.empty() ? std::string(gm::localization::trUi("Hero")) : sourceName);
#if defined(_MSC_VER)
    strncpy_s(copy.nameBuffer, sizeof(copy.nameBuffer), copyName.c_str(), _TRUNCATE);
#else
    std::strncpy(copy.nameBuffer, copyName.c_str(), sizeof(copy.nameBuffer) - 1);
    copy.nameBuffer[sizeof(copy.nameBuffer) - 1] = '\0';
#endif
    copy.inventory.metadata = EntityMetadata{"inventory"};
    copy.inventory.ownerEntityId = copy.metadata.id;
    copy.inventory.name = std::string(copy.nameBuffer) + " – " + gm::localization::trUi("Inventory");
    normalizeCharacterHitPoints(copy);
    normalizeSpellcasting(copy.spellcasting);

    _currentCampaign.characters.insert(_currentCampaign.characters.begin() + _selectedCharacterIndex + 1, copy);
    ++_selectedCharacterIndex;
    _currentCampaign.metadata.touch();
    if (!saveCampaign()) return;
    _statusMessage = std::string(gm::localization::trUi("Duplicated")) + " " + sourceName + ".";
}

std::vector<std::string> CharacterSheetController::getCharacterReferenceWarnings(int characterIndex) const
{
    std::vector<std::string> warnings;
    if (characterIndex < 0 || characterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return warnings;

    const CharacterModel &character = _currentCampaign.characters[characterIndex];
    const EntityId &characterId = character.metadata.id;
    int storyLinkCount = 0;
    for (const StoryThreadModel &thread : _currentCampaign.storyThreads)
    {
        storyLinkCount += static_cast<int>(std::count_if(thread.links.begin(), thread.links.end(), [&](const StoryLinkModel &link) {
            return link.targetEntityType == "Player" && link.targetEntityId == characterId;
        }));
        for (const StoryBeatModel &beat : thread.beats)
        {
            storyLinkCount += static_cast<int>(std::count_if(beat.links.begin(), beat.links.end(), [&](const StoryLinkModel &link) {
                return link.targetEntityType == "Player" && link.targetEntityId == characterId;
            }));
        }
    }
    if (storyLinkCount > 0)
        warnings.push_back(std::to_string(storyLinkCount) + " " + gm::localization::trUi("story link(s) point at this character."));

    int encounterParticipantCount = 0;
    for (const EncounterModel &encounter : _currentCampaign.encounters)
    {
        encounterParticipantCount += static_cast<int>(std::count_if(encounter.participants.begin(), encounter.participants.end(), [&](const EncounterParticipantModel &participant) {
            return participant.kind == CombatantKind::Player && participant.sourceEntityId == characterId;
        }));
    }
    if (encounterParticipantCount > 0)
        warnings.push_back(std::to_string(encounterParticipantCount) + " " + gm::localization::trUi("encounter participant(s) use this character."));

    if (!character.inventory.entries.empty())
        warnings.push_back(std::to_string(character.inventory.entries.size()) + " " + gm::localization::trUi("inventory item row(s) will stay only with this character."));
    if (character.spellcasting.enabled && !character.spellcasting.spells.empty())
        warnings.push_back(std::to_string(character.spellcasting.spells.size()) + " " + gm::localization::trUi("known spell(s) are assigned to this character."));

    return warnings;
}

bool CharacterSheetController::isCharacterArchived(const CharacterModel &character) const
{
    return hasTag(character.metadata, "archived");
}

void CharacterSheetController::requestArchiveSelectedCharacter()
{
    _pendingCharacterRemovalIndex = _selectedCharacterIndex;
    _pendingCharacterRemovalIsArchive = true;
}

void CharacterSheetController::requestDeleteSelectedCharacter()
{
    _pendingCharacterRemovalIndex = _selectedCharacterIndex;
    _pendingCharacterRemovalIsArchive = false;
}

void CharacterSheetController::archivePendingCharacter()
{
    _errorMessage.clear();
    _statusMessage.clear();
    if (_pendingCharacterRemovalIndex < 0 || _pendingCharacterRemovalIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_pendingCharacterRemovalIndex];
    addTag(character.metadata, "archived");
    character.metadata.touch();
    _selectedCharacterIndex = _pendingCharacterRemovalIndex;
    _pendingCharacterRemovalIndex = -1;
    if (!saveCampaign()) return;
    _statusMessage = std::string(gm::localization::trUi("Archived")) + ": " + std::string(character.nameBuffer) + ".";
}

void CharacterSheetController::deletePendingCharacter()
{
    _errorMessage.clear();
    _statusMessage.clear();
    if (_pendingCharacterRemovalIndex < 0 || _pendingCharacterRemovalIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    const EntityId removedId = _currentCampaign.characters[_pendingCharacterRemovalIndex].metadata.id;
    const std::string removedName = _currentCampaign.characters[_pendingCharacterRemovalIndex].nameBuffer;
    removeDeletedCharacterReferences(removedId);
    removeLocalPlayerDisplayCharacterSelection(_currentCampaign.localPlayerDisplay, removedId);
    _currentCampaign.characters.erase(_currentCampaign.characters.begin() + _pendingCharacterRemovalIndex);
    if (_currentCampaign.characters.empty())
        _selectedCharacterIndex = 0;
    else
        _selectedCharacterIndex = std::clamp(_pendingCharacterRemovalIndex, 0, static_cast<int>(_currentCampaign.characters.size()) - 1);
    _pendingCharacterRemovalIndex = -1;
    _currentCampaign.metadata.touch();
    if (!saveCampaign()) return;
    _statusMessage = std::string(gm::localization::trUi("Deleted")) + ": " + removedName + ".";
}
