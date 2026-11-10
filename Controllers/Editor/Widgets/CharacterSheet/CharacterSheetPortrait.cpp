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

void CharacterSheetController::browseForSelectedCharacterPortrait()
{
    _errorMessage.clear();
    _statusMessage.clear();
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

#if defined(_WIN32)
    char fileName[MAX_PATH] = {};
    OPENFILENAMEA openFileName = {};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.hwndOwner = nullptr;
    openFileName.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0All Files\0*.*\0";
    openFileName.lpstrFile = fileName;
    openFileName.nMaxFile = MAX_PATH;
    openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&openFileName))
    {
        CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
        const std::filesystem::path sourcePath(fileName);
        if (!isSupportedPortraitFile(sourcePath))
        {
            _errorMessage = gm::localization::trUi("Supported portrait files: png, jpg, jpeg, bmp, gif.");
            return;
        }

        try
        {
            character.portraitAssetId = copyPortraitFileToCampaign(sourcePath, character.metadata.id);
            character.metadata.touch();
            if (!saveCampaign()) return;
            _statusMessage = std::string(gm::localization::trUi("Portrait updated for")) + " " + std::string(character.nameBuffer) + ".";
        }
        catch (const std::exception &e)
        {
            _errorMessage = std::string(gm::localization::trUi("Could not copy portrait:")) + " " + e.what();
        }
    }
#else
    _statusMessage = gm::localization::trUi("Portrait browsing is only wired on Windows right now.");
#endif
}

void CharacterSheetController::clearSelectedCharacterPortrait()
{
    _errorMessage.clear();
    _statusMessage.clear();
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[_selectedCharacterIndex];
    character.portraitAssetId.clear();
    character.metadata.touch();
    if (!saveCampaign()) return;
    _statusMessage = std::string(gm::localization::trUi("Portrait removed from")) + " " + std::string(character.nameBuffer) + ".";
}

void CharacterSheetController::openPortraitDirectory()
{
    _errorMessage.clear();
    _statusMessage.clear();
    try
    {
        const std::filesystem::path directory = campaignPortraitDirectory();
        std::filesystem::create_directories(directory);
#if defined(_WIN32)
        const std::string directoryString = directory.string();
        HINSTANCE result = ShellExecuteA(nullptr, "open", directoryString.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<intptr_t>(result) <= 32)
            _errorMessage = gm::localization::trUi("Could not open the portrait folder.");
        else
            _statusMessage = gm::localization::trUi("Opened the portrait folder.");
#else
        _statusMessage = std::string(gm::localization::trUi("Portrait folder:")) + " " + directory.string();
#endif
    }
    catch (const std::exception &e)
    {
        _errorMessage = std::string(gm::localization::trUi("Could not prepare portrait folder:")) + " " + e.what();
    }
}

void CharacterSheetController::exportSelectedCharacter()
{
    _errorMessage.clear();
    _statusMessage.clear();
    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

#if defined(_WIN32)
    char fileName[MAX_PATH] = {};
    OPENFILENAMEA saveFileName = {};
    saveFileName.lStructSize = sizeof(saveFileName);
    saveFileName.lpstrFilter = "TTRPG Character\0*.ttrpg-character\0All Files\0*.*\0";
    saveFileName.lpstrFile = fileName;
    saveFileName.nMaxFile = MAX_PATH;
    saveFileName.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    saveFileName.lpstrDefExt = "ttrpg-character";
    if (!GetSaveFileNameA(&saveFileName))
        return;

    try
    {
        std::ofstream file(fileName, std::ios::binary);
        file << serializeCharacterForExport(_currentCampaign.characters[_selectedCharacterIndex]);
        _statusMessage = gm::localization::trUi("Exported character.");
    }
    catch (const std::exception &e)
    {
        _errorMessage = std::string(gm::localization::trUi("Could not export character:")) + " " + e.what();
    }
#else
    _statusMessage = gm::localization::trUi("Character export file browsing is only wired on Windows right now.");
#endif
}

void CharacterSheetController::importCharacterFromFile()
{
    _errorMessage.clear();
    _statusMessage.clear();
#if defined(_WIN32)
    char fileName[MAX_PATH] = {};
    OPENFILENAMEA openFileName = {};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.lpstrFilter = "TTRPG Character\0*.ttrpg-character\0All Files\0*.*\0";
    openFileName.lpstrFile = fileName;
    openFileName.nMaxFile = MAX_PATH;
    openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameA(&openFileName))
        return;

    try
    {
        std::ifstream file(fileName, std::ios::binary);
        std::stringstream buffer;
        buffer << file.rdbuf();
        CharacterModel imported;
        std::string error;
        if (!deserializeCharacterForImport(buffer.str(), imported, error))
        {
            _errorMessage = gm::localization::trUi(error.c_str());
            return;
        }
        imported.metadata = EntityMetadata{"character"};
        imported.inventory.metadata = EntityMetadata{"inventory"};
        imported.inventory.ownerEntityId = imported.metadata.id;
        imported.inventory.name = std::string(imported.nameBuffer) + " – " + gm::localization::trUi("Inventory");
        _currentCampaign.characters.push_back(imported);
        _selectedCharacterIndex = static_cast<int>(_currentCampaign.characters.size()) - 1;
        if (!saveCampaign()) return;
        _statusMessage = std::string(gm::localization::trUi("Imported")) + ": " + std::string(imported.nameBuffer) + ".";
    }
    catch (const std::exception &e)
    {
        _errorMessage = std::string(gm::localization::trUi("Could not import character:")) + " " + e.what();
    }
#else
    _statusMessage = gm::localization::trUi("Character import file browsing is only wired on Windows right now.");
#endif
}
