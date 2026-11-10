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

bool CharacterSheetController::saveCampaign()
{
    _currentCampaign.metadata.touch();
    const bool saved = ProjectSaveService::getInstance().saveCampaign(_currentCampaign);
    if (!saved)
    {
        _statusMessage.clear();
        const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
        _errorMessage = german ? "Nicht gespeichert. Änderungen bleiben im Speicher. Nutze Speichern, um es erneut zu versuchen."
                               : "Not saved. Changes remain in memory. Use Save to retry.";
        _errorMessage += " " + ProjectSaveService::getInstance().state().lastError;
    }
    else _errorMessage.clear();
    return saved;
}

std::filesystem::path CharacterSheetController::findProjectRoot() const
{
    return ProjectAssets::findProjectRoot();
}

std::filesystem::path CharacterSheetController::campaignPortraitDirectory() const
{
    return ProjectAssets::campaignAssetDirectory(_currentCampaign, "Portraits");
}

std::string CharacterSheetController::copyPortraitFileToCampaign(const std::filesystem::path &sourcePath, const EntityId &characterId)
{
    const std::filesystem::path targetDirectory = campaignPortraitDirectory();
    std::filesystem::create_directories(targetDirectory);

    std::filesystem::path extension = sourcePath.extension();
    if (extension.empty())
        extension = ".png";

    const std::filesystem::path destination = targetDirectory / (sanitizePathSegment(characterId) + extension.string());
    std::filesystem::copy_file(sourcePath, destination, std::filesystem::copy_options::overwrite_existing);

    const std::filesystem::path root = findProjectRoot();
    try
    {
        return std::filesystem::relative(destination, root).generic_string();
    }
    catch (const std::exception &)
    {
        return destination.generic_string();
    }
}

bool CharacterSheetController::isSupportedPortraitFile(const std::filesystem::path &sourcePath) const
{
    static const std::set<std::string> supportedExtensions{".png", ".jpg", ".jpeg", ".bmp", ".gif"};
    return supportedExtensions.count(lowerCopy(sourcePath.extension().string())) > 0;
}

std::vector<std::string> &CharacterSheetController::mutableCharacterDetailList(CharacterModel &character, CharacterDetailList list) const
{
    switch (list)
    {
    case CharacterDetailList::Feats:
        return character.feats;
    case CharacterDetailList::TraitsAndFeatures:
        return character.traitsAndFeatures;
    case CharacterDetailList::Languages:
        return character.languages;
    case CharacterDetailList::Senses:
        return character.senses;
    case CharacterDetailList::SavingThrows:
        return character.savingThrowProficiencies;
    case CharacterDetailList::Skills:
        return character.skillProficiencies;
    case CharacterDetailList::Tools:
        return character.toolProficiencies;
    }
    return character.traitsAndFeatures;
}

void CharacterSheetController::removeDeletedCharacterReferences(const EntityId &characterId)
{
    auto removePlayerLinks = [&](std::vector<StoryLinkModel> &links) {
        const auto oldSize = links.size();
        links.erase(std::remove_if(links.begin(), links.end(), [&](const StoryLinkModel &link) {
            return link.targetEntityType == "Player" && link.targetEntityId == characterId;
        }), links.end());
        return oldSize != links.size();
    };

    for (StoryThreadModel &thread : _currentCampaign.storyThreads)
    {
        bool changed = removePlayerLinks(thread.links);
        for (StoryBeatModel &beat : thread.beats)
        {
            if (removePlayerLinks(beat.links))
            {
                beat.metadata.touch();
                changed = true;
            }
        }
        if (changed)
            thread.metadata.touch();
    }

    for (EncounterModel &encounter : _currentCampaign.encounters)
    {
        const auto oldSize = encounter.participants.size();
        encounter.participants.erase(std::remove_if(encounter.participants.begin(), encounter.participants.end(), [&](const EncounterParticipantModel &participant) {
            return participant.kind == CombatantKind::Player && participant.sourceEntityId == characterId;
        }), encounter.participants.end());
        if (oldSize != encounter.participants.size())
        {
            if (encounter.participants.empty())
                encounter.activeParticipantIndex = 0;
            else
                encounter.activeParticipantIndex = std::clamp(encounter.activeParticipantIndex, 0, static_cast<int>(encounter.participants.size()) - 1);
            encounter.metadata.touch();
        }
    }
}

void CharacterSheetController::onShow()
{
    view->show();
    view->requestDockOnNextUpdate();
}


void CharacterSheetController::resetDrafts()
{
    _state.resetDrafts();
}
