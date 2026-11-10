#pragma once

#include "Models/Rulesets/IRuleSet.hpp"
#include "Models/Audio/AudioModels.hpp"
#include "Models/Character/CharacterModel.hpp"
#include "Models/Combat/EncounterModel.hpp"
#include "Models/Core/EntityMetadata.hpp"
#include "Models/Dungeon/DungeonModel.hpp"
#include "Models/Inventory/InventoryModels.hpp"
#include "Models/Magic/SpellModels.hpp"
#include "Models/Map/MapModels.hpp"
#include "Models/NPC/NPCModel.hpp"
#include "Models/Rulesets/HouseRuleModel.hpp"
#include "Models/Rulesets/RulesetContentPackModel.hpp"
#include "Models/Rulesets/RulesetDefinitionModel.hpp"
#include "Models/Rulesets/RulesetResolutionState.hpp"
#include "Models/Shop/ShopModel.hpp"
#include "Models/Session/HandoutModel.hpp"
#include "Models/Session/InWorldTimeModel.hpp"
#include "Models/Session/LocalPlayerDisplayModel.hpp"
#include "Models/Session/SessionModel.hpp"
#include "Models/Session/TabletopToolkit/TabletopToolkitModels.hpp"
#include "Models/Story/StoryModels.hpp"
#include "Models/World/AtlasWorldModels.hpp"
#include "Models/Project/CampaignCollectionModels.hpp"
#include "Models/Random/RandomTableModels.hpp"
#include <string>
#include <memory>
#include <vector>

struct Campaign
{
    long long id = -1;
    EntityMetadata metadata{"campaign"};
    std::string name;
    std::string description;
    // Language of generated campaign content (names, map descriptions, and
    // other authored suggestions). This is intentionally independent from
    // the application UI locale.
    std::string contentLanguage = "en";
    std::shared_ptr<IRuleSet> ruleSet;
    RulesetReference rulesetReference;
    RulesetResolutionState rulesetResolution;
    std::vector<CharacterModel> characters;
    std::vector<NPCModel> npcs;
    std::vector<FactionModel> factions;
    std::vector<DeityModel> deities;
    std::vector<NationModel> nations;
    std::vector<WorldPlaceModel> worldPlaces;
    std::vector<ItemModel> customItems;
    std::vector<SpellModel> customSpells;
    std::vector<MonsterDefinitionModel> customMonsters;
    std::vector<MapModel> maps;
    std::vector<EncounterModel> encounters;
    std::vector<AudioAssetModel> audioAssets;
    std::vector<AudioPlaylistModel> audioPlaylists;
    std::vector<AudioAmbienceLayerModel> audioAmbienceLayers;
    std::vector<AudioTriggerModel> audioTriggers;
    AudioVolumeGroupsModel audioVolumeGroups;
    std::vector<ShopModel> shops;
    std::vector<LootParcelModel> lootParcels;
    std::vector<DungeonModel> dungeons;
    std::vector<HandoutModel> handouts;
    std::vector<SessionModel> sessions;
    std::vector<CampaignDiceMacroModel> diceMacros;
    std::vector<StoryObjectiveModel> storyObjectives;
    std::vector<StoryClueModel> storyClues;
    CampaignSafetyProfileModel safetyProfile;
    std::vector<StoryThreadModel> storyThreads;
    std::vector<TimelineEventModel> timelineEvents;
    std::vector<RandomTableModel> randomTables;
    std::vector<RulesetDefinitionModel> rulesetDefinitions;
    std::vector<RulesetContentPackModel> rulesetContentPacks;
    std::vector<HouseRuleModel> houseRules;
    std::vector<CampaignSavedFilterModel> savedFilters;
    std::vector<CampaignEntityBookmarkModel> entityBookmarks;
    bool presentationModeActive = false;
    LocalPlayerDisplayModel localPlayerDisplay;
    CampaignClockModel clock;
    PartyMapLocationModel partyLocation;
    PartyTravelPlanModel partyTravel;
    InventoryModel partyInventory;
    std::vector<std::string> partyVehicles;
    std::vector<std::string> partyStrongholds;
    int partyRenown = 0;
    std::string partyReputation;
    std::vector<EntityId> npcIds;
    std::vector<EntityId> itemIds;
    std::vector<EntityId> inventoryIds;
    std::vector<EntityId> shopIds;
    std::vector<EntityId> mapIds;
    std::vector<EntityId> dungeonIds;
    std::vector<EntityId> encounterIds;
    std::vector<EntityId> audioAssetIds;
    std::vector<EntityId> audioPlaylistIds;
    std::vector<EntityId> audioAmbienceLayerIds;
    std::vector<EntityId> audioTriggerIds;
    std::vector<EntityId> storyDocumentIds;
    std::vector<EntityId> sessionIds;
};

inline std::vector<SpellModel> getAvailableSpells(const Campaign &campaign)
{
    std::vector<SpellModel> spells;
    if (campaign.ruleSet)
    {
        const std::vector<SpellModel> rulesetSpells = campaign.ruleSet->getSpells();
        spells.insert(spells.end(), rulesetSpells.begin(), rulesetSpells.end());
    }
    spells.insert(spells.end(), campaign.customSpells.begin(), campaign.customSpells.end());
    return spells;
}
