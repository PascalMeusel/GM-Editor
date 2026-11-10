#include "StoryWorkspaceController.hpp"
#include "Models/Audio/AudioMixService.hpp"
#include "Models/Project/CampaignBookmarkService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Story/SceneLaunchService.hpp"
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include <algorithm>
#include <cstddef>
#include <utility>

std::vector<std::string> StoryWorkspaceController::getEntityTypes() const
{
    return {"Player", "NPC", "Place", "Nation", "Monster", "Item", "Spell", "Map", "Encounter", "Handout", "Loot", "Audio"};
}

std::vector<StoryEntityOption> StoryWorkspaceController::getEntityOptions(const std::string &type) const
{
    std::vector<StoryEntityOption> options;
    if (type == "Player")
    {
        for (const auto &character : _currentCampaign.characters)
            options.push_back({type, character.metadata.id, character.nameBuffer});
    }
    else if (type == "NPC")
    {
        for (const auto &npc : _currentCampaign.npcs)
            options.push_back({type, npc.metadata.id, npc.name});
    }
    else if (type == "Place" || type == "world_place")
    {
        for (const auto &place : _currentCampaign.worldPlaces)
            options.push_back({type, place.metadata.id, place.name});
    }
    else if (type == "Nation" || type == "nation")
    {
        for (const auto &nation : _currentCampaign.nations)
            options.push_back({type, nation.metadata.id, nation.name});
    }
    else if (type == "Monster")
    {
        for (const auto &monster : _currentCampaign.customMonsters)
            options.push_back({type, monster.metadata.id, monster.name});
    }
    else if (type == "Item")
    {
        for (const auto &item : _currentCampaign.customItems)
            options.push_back({type, item.metadata.id, item.name});
    }
    else if (type == "Spell")
    {
        for (const auto &spell : _currentCampaign.customSpells)
            options.push_back({type, spell.metadata.id, spell.name});
    }
    else if (type == "Map")
    {
        for (const auto &map : _currentCampaign.maps)
            options.push_back({type, map.metadata.id, map.name});
    }
    else if (type == "Encounter")
    {
        for (const auto &encounter : _currentCampaign.encounters)
            options.push_back({type, encounter.metadata.id, encounter.name});
    }
    else if (type == "Handout")
    {
        for (const auto &handout : _currentCampaign.handouts)
            options.push_back({type, handout.metadata.id, handout.title.empty() ? std::string{gm::localization::trUi("Untitled Handout")} : handout.title});
    }
    else if (type == "Loot")
    {
        for (const auto &parcel : _currentCampaign.lootParcels)
            options.push_back({type, parcel.metadata.id, parcel.name.empty() ? std::string{gm::localization::trUi("Loot Parcel")} : parcel.name});
    }
    else if (type == "Audio")
    {
        for (const auto &asset : _currentCampaign.audioAssets)
            options.push_back({type, asset.metadata.id, asset.name});
    }
    return options;
}

StoryEntityOption StoryWorkspaceController::getRecentEntityOption(const std::string &type) const
{
    const std::vector<StoryEntityOption> options = getEntityOptions(type);
    if (options.empty())
        return {};

    const std::vector<CampaignEntityBookmarkModel> recent = CampaignBookmarks::recent(_currentCampaign, 20);
    for (const CampaignEntityBookmarkModel &bookmark : recent)
    {
        if (bookmark.entityType != type)
            continue;
        const auto found = std::find_if(options.begin(), options.end(), [&](const StoryEntityOption &option) {
            return option.id == bookmark.entityId;
        });
        if (found != options.end())
            return *found;
    }
    return {};
}

std::string StoryWorkspaceController::describeEntity(const StoryLinkModel &link) const
{
    const std::vector<StoryEntityOption> options = getEntityOptions(link.targetEntityType);
    const auto found = std::find_if(options.begin(), options.end(), [&](const StoryEntityOption &option) {
        return option.id == link.targetEntityId;
    });
    if (found != options.end())
        return found->label;
    return link.targetEntityId.empty() ? gm::localization::trUi("<No Target>") : link.targetEntityId;
}

AudioAssetModel *StoryWorkspaceController::findAudioAsset(const EntityId &assetId)
{
    auto found = std::find_if(_currentCampaign.audioAssets.begin(), _currentCampaign.audioAssets.end(), [&](const AudioAssetModel &asset) {
        return asset.metadata.id == assetId;
    });
    return found == _currentCampaign.audioAssets.end() ? nullptr : &(*found);
}

const AudioAssetModel *StoryWorkspaceController::findAudioAsset(const EntityId &assetId) const
{
    auto found = std::find_if(_currentCampaign.audioAssets.begin(), _currentCampaign.audioAssets.end(), [&](const AudioAssetModel &asset) {
        return asset.metadata.id == assetId;
    });
    return found == _currentCampaign.audioAssets.end() ? nullptr : &(*found);
}
