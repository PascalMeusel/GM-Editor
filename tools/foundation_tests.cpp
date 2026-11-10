#include "Models/Campaign/Campaign.hpp"
#include "Models/DataBaseManager.hpp"
#include "SQLiteCpp/SQLiteCpp.h"
#include "Models/Character/CharacterModel.hpp"
#include "Models/Character/CharacterHitPointEstimate.hpp"
#include <limits>
#include "Models/Core/ProductInfo.hpp"
#include "Models/Audio/AudioAssetPathService.hpp"
#include "Models/Audio/AudioPlaybackService.hpp"
#include "Models/Inventory/InventoryModels.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapStructuredCreation.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Project/CampaignBookmarkService.hpp"
#include "Models/Project/CampaignCommandPaletteService.hpp"
#include "Models/Project/CampaignCollectionService.hpp"
#include "Models/Project/CampaignGraphService.hpp"
#include "Models/Project/CampaignIndexRebuildService.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/CampaignReferenceService.hpp"
#include "Models/Project/CampaignSearchIndex.hpp"
#include "Models/Project/CampaignTagService.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include "Models/Project/ProjectAssetCachePolicy.hpp"
#include "Models/Project/ProjectAssetCollectionService.hpp"
#include "Models/Project/ProjectAssetRepairService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectManifestService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectPackageExportService.hpp"
#include "Models/Project/ProjectPackageImportService.hpp"
#include "Models/Project/ProjectBackupRestoreService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include <exception>
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/ProjectStorageService.hpp"
#include "Models/UX/LegacySurfaceRegistry.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Rulesets/CharacterRulesService.hpp"
#include "Models/Rulesets/RulesetContentPackJson.hpp"
#include "Models/Rulesets/RulesetDefinitionLayering.hpp"
#include "Models/Rulesets/RulesetDefinitionValidator.hpp"
#include "Models/Rulesets/RulesetFormulaEvaluator.hpp"
#include "Models/Rulesets/RulesetMigrationPlan.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Models/Rulesets/RulesetVersioning.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/Session/PlayerSafeDataFilter.hpp"
#include "Models/Media/RgbaImageLoader.hpp"

#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace
{
bool require(bool condition, const std::string &message)
{
    if (condition)
        return true;

    std::cerr << message << std::endl;
    return false;
}

class ScopedCurrentPath
{
public:
    explicit ScopedCurrentPath(const std::filesystem::path &path)
        : _original(std::filesystem::current_path())
    {
        std::filesystem::current_path(path);
    }

    ~ScopedCurrentPath()
    {
        std::error_code ignored;
        std::filesystem::current_path(_original, ignored);
    }

private:
    std::filesystem::path _original;
};

std::filesystem::path prepareDisposableProjectRoot()
{
    const std::filesystem::path root = std::filesystem::current_path() / "foundation_test_project_root";
    std::filesystem::remove_all(mapFileIoPath(root));
    std::filesystem::create_directories(mapFileIoPath(root / "Resources" / "Campaigns"));

    std::ofstream cmakeList(root / "CMakeLists.txt", std::ios::binary | std::ios::trunc);
    cmakeList << "# Disposable project root for foundation_tests.\n";
    return root;
}

void configureDisposableDatabase(const std::filesystem::path &projectRoot)
{
    const std::filesystem::path databasePath = projectRoot / "TTRPG.db";
    std::filesystem::remove(databasePath);
#if defined(_WIN32)
    _putenv_s("TTRPG_DB_PATH", databasePath.string().c_str());
#else
    setenv("TTRPG_DB_PATH", databasePath.string().c_str(), 1);
#endif
}

Campaign makeCampaign(const std::string &suffix)
{
    Campaign campaign;
    campaign.metadata.id = "test_campaign_" + suffix;
    campaign.name = "Foundation Test Campaign " + suffix;
    campaign.description = "Round trip test campaign.";
    campaign.ruleSet = std::make_shared<DND5e>();
    campaign.presentationModeActive = true;
    campaign.audioVolumeGroups.master = 0.8f;
    campaign.audioVolumeGroups.music = 0.7f;
    campaign.audioVolumeGroups.ambience = 0.6f;
    campaign.audioVolumeGroups.effects = 0.9f;
    campaign.audioVolumeGroups.voice = 0.5f;
    campaign.clock.calendar.name = "Test Lunar Calendar";
    campaign.clock.calendar.months = {{"Emberwane", 28}, {"Bloomwane", 32}, {"Frostwane", 30}};
    campaign.clock.calendar.weekdays = {"Firstday", "Secondday", "Thirdday", "Restday"};
    campaign.clock.calendar.hoursPerDay = 20;
    campaign.clock.calendar.daylightHours = 11;
    campaign.clock.calendar.nightHours = 7;
    campaign.clock.calendar.dayStartsAtHour = 5;
    campaign.clock.multiplier = 3.5;
    InWorldDateTime campaignTime;
    campaignTime.year = 77;
    campaignTime.month = 2;
    campaignTime.day = 12;
    campaignTime.hour = 9;
    campaignTime.minute = 45;
    InWorldTime::setDateTime(campaign.clock, campaignTime, false);

    CharacterModel character;
    character.metadata.id = "test_character_" + suffix;
    std::snprintf(character.nameBuffer, sizeof(character.nameBuffer), "%s", "Test Hero");
    character.level = 3;
    character.attributes = {{"Strength", 10}, {"Dexterity", 14}, {"Constitution", 16}};
    character.selectedClass = "Fighter";
    character.selectedRace = "Human";
    character.spellcasting.enabled = true;
    character.spellcasting.slotMaximums[1] = 2;
    campaign.characters.push_back(character);

    ItemModel item;
    item.metadata.id = "test_item_" + suffix;
    item.name = "Test Finesse Blade";
    item.category = "Weapon";
    item.itemType = "Weapon";
    item.weaponDamage = "1d6 slashing";
    item.weaponDamageType = "slashing";
    item.weaponAbility = "Finesse";
    item.weaponReachFeet = 5;
    item.weaponRangeFeet = 20;
    item.weaponLongRangeFeet = 60;
    item.weaponAttackKind = "MeleeOrThrown";
    item.weaponMastery = "Vex";
    item.weaponVersatileDamage = "1d8 slashing";
    item.weaponProperties = {"Finesse", "Thrown", "Versatile"};
    campaign.customItems.push_back(item);

    ShopModel shop;
    shop.metadata.id = "test_shop_" + suffix;
    shop.name = "Test Market";
    shop.merchantName = "Test Merchant";
    shop.factionReputationNotes = "Friends of the party get better prices.";
    shop.stock.push_back({item.metadata.id, 3, 1.25, true});
    shop.stock.front().sellPriceMultiplier = 0.45;
    shop.stock.front().availability = "Limited";
    shop.stock.front().haggleNotes = "Mentions guild dues.";
    shop.stock.front().factionModifier = 0.9;
    shop.stock.front().reputationModifier = 0.8;
    campaign.shops.push_back(shop);

    LootParcelModel parcel;
    parcel.metadata.id = "test_loot_" + suffix;
    parcel.name = "Test Loot";
    parcel.gold = 12;
    parcel.items.push_back({item.metadata.id, 2, "Recovered after battle"});
    campaign.lootParcels.push_back(parcel);

    campaign.characters.front().inventory.entries.push_back(InventoryEntryModel{item.metadata.id, 1, true, "Off Hand"});

    MapModel map;
    map.metadata.id = "test_map_" + suffix;
    map.name = "Test Map";
    map.width = 2;
    map.height = 2;
    map.cells.resize(4);
    map.cells.front().movementCost = 2;
    map.cells.front().difficultTerrain = true;
    map.cells.front().blocksLineOfSight = true;
    map.cells.front().lighting = MapLightingMode::Dim;
    map.cells.front().fogged = true;
    map.cells.front().elevation = 2;
    map.cells.front().movementBlockedMask = 3;
    map.cells.front().lineOfSightBlockedMask = 1;
    map.cells.front().symbolVariantMode = MapSymbolVariantMode::Individual;
    map.cells.front().wayKind = MapWayKind::DirtRoad;
    map.cells.front().rotationLocked = true;
    MapLayerModel layer;
    layer.metadata.id = "test_layer_" + suffix;
    layer.name = "GM Notes";
    layer.sortOrder = 1;
    map.layers.push_back(layer);
    MapTokenModel token;
    token.metadata.id = "test_token_" + suffix;
    token.name = "Test Hero Token";
    token.kind = MapTokenKind::Player;
    token.linkedEntityType = "player";
    token.linkedEntityId = character.metadata.id;
    token.layerId = layer.metadata.id;
    token.x = 1.0f;
    token.y = 1.0f;
    token.size = 1.5f;
    map.tokens.push_back(token);
    MapFeatureModel feature;
    feature.metadata.id = "test_feature_" + suffix;
    feature.kind = MapFeatureKind::SecretDoor;
    feature.name = "Hidden Hatch";
    feature.x = 1;
    feature.y = 0;
    feature.gmOnly = true;
    map.features.push_back(feature);
    MapRegionModel region;
    region.metadata.id = "test_region_" + suffix;
    region.name = "Difficult Hall";
    region.movementCost = 2;
    region.difficultTerrain = true;
    map.regions.push_back(region);
    MapRouteModel route;
    route.metadata.id = "test_route_" + suffix;
    route.name = "Old Road";
    route.fromMapId = map.metadata.id;
    route.distanceMiles = 12.0f;
    route.terrain = "Road";
    map.routes.push_back(route);
    campaign.maps.push_back(map);
    campaign.partyLocation.mapId = map.metadata.id;
    campaign.partyLocation.x = 1;
    campaign.partyLocation.y = 1;
    campaign.partyTravel.currentMapId = map.metadata.id;
    campaign.partyTravel.routeId = route.metadata.id;
    campaign.partyTravel.routeName = route.name;
    campaign.partyTravel.distanceMiles = route.distanceMiles;
    campaign.partyTravel.milesTraveled = 4.0f;
    campaign.partyTravel.watches = 1;
    campaign.partyTravel.rationsUsed = 4;

    HandoutModel handout;
    handout.metadata.id = "test_handout_" + suffix;
    handout.title = "Test Handout";
    handout.body = "Public clue text.";
    handout.filePath = "Resources/Test/handout.png";
    handout.gmNotes = "Do not show the answer.";
    campaign.handouts.push_back(handout);
    campaign.localPlayerDisplay.enabled = true;
    campaign.localPlayerDisplay.publicMapId = map.metadata.id;
    campaign.localPlayerDisplay.showInitiative = false;
    campaign.localPlayerDisplay.mapOnly = false;
    campaign.localPlayerDisplay.preferredMonitorIndex = 2;
    campaign.localPlayerDisplay.fullscreenMapOnly = false;
    campaign.localPlayerDisplay.publicCamera.normalizedCenterX = 0.25f;
    campaign.localPlayerDisplay.publicCamera.normalizedCenterY = 0.75f;
    campaign.localPlayerDisplay.publicCamera.zoom = 2.5f;
    campaign.localPlayerDisplay.publicCamera.followGmCamera = false;
    campaign.localPlayerDisplay.publicHandoutIds.push_back(handout.metadata.id);
    campaign.localPlayerDisplay.showParty = true;
    campaign.localPlayerDisplay.showPartySpellSlots = true;
    campaign.localPlayerDisplay.showPartyInventory = true;
    campaign.localPlayerDisplay.visibleCharacterIds.push_back(campaign.characters.front().metadata.id);
    campaign.localPlayerDisplay.playerTextScale = 1.45f;
    campaign.characters.front().temporaryHitPoints = 6;

    DungeonModel dungeon;
    dungeon.metadata.id = "test_dungeon_" + suffix;
    dungeon.name = "Test Dungeon";
    dungeon.parentMapId = map.metadata.id;
    dungeon.mapIds.push_back(map.metadata.id);
    DungeonRoomModel room;
    room.metadata.id = "test_room_" + suffix;
    room.name = "Entry Hall";
    room.keyedDescription = "A keyed room.";
    room.readAloudText = "Read this at the table.";
    room.secrets = "Secret panel.";
    room.traps = "Needle trap.";
    room.explorationState = DungeonRoomExplorationState::Seen;
    DungeonRoomConnectionModel connection;
    connection.targetRoomId = "next_room_" + suffix;
    connection.label = "North door";
    connection.secret = true;
    room.connections.push_back(connection);
    dungeon.activeRoomId = room.metadata.id;
    dungeon.rooms.push_back(room);
    campaign.dungeons.push_back(dungeon);

    TimelineEventModel timelineEvent;
    timelineEvent.metadata.id = "test_timeline_" + suffix;
    timelineEvent.title = "Test Calendar Event";
    timelineEvent.description = "The party records an in-world date.";
    timelineEvent.inWorldDate = "Bloomwane 12";
    timelineEvent.relatedEntityIds.push_back(character.metadata.id);
    campaign.timelineEvents.push_back(timelineEvent);

    SessionModel session;
    session.metadata.id = "test_session_" + suffix;
    session.title = "Test Live Session";
    session.scheduledAtUtc = "2026-05-23T18:00:00Z";
    session.startedAtUtc = "2026-05-23T18:05:00Z";
    session.endedAtUtc = "2026-05-23T21:15:00Z";
    session.summary = "The party tested live session persistence.";
    session.recap = "Recovered the clue and escaped.";
    session.unresolvedThreads = "Who sent the signal?";
    session.lootAwarded = "Silver key";
    session.xpMilestones = "Milestone: found the hatch";
    session.npcsMet = "Test Guide";
    session.locationsVisited = "Entry Hall";
    session.openHooks = "The hatch leads deeper.";
    session.participantCharacterIds.push_back(character.metadata.id);
    session.liveModeActive = false;
    session.roughNotes = "Ruß; clues\nA \"lantern\" \\ path";
    SessionPlanEntry plannedScene;
    plannedScene.metadata.id = "test_plan_entry_" + suffix;
    plannedScene.storyBeatId = "test_scene_" + suffix;
    plannedScene.prepNote = "GM-only preparation; preserve order";
    plannedScene.state = SessionEntryState::Completed;
    session.planEntries.push_back(plannedScene);
    session.liveState.initialized = true;
    session.liveState.phase = SessionPhase::WrapUp;
    session.liveState.phaseBeforeBreak = SessionPhase::Combat;
    session.liveState.activePlanEntryId = plannedScene.metadata.id;
    SessionNoteModel note;
    note.metadata.id = "test_session_note_" + suffix;
    note.timestampUtc = "2026-05-23T18:30:00Z";
    note.body = "The hero found a linked clue.";
    note.linkedEntityId = character.metadata.id;
    note.linkedEntityType = "character";
    note.linkedEntityLabel = "Test Hero";
    session.notes.push_back(note);
    campaign.sessions.push_back(session);
    campaign.sessionIds.push_back(session.metadata.id);

    AudioAssetModel audioAsset;
    audioAsset.metadata.id = "test_audio_" + suffix;
    audioAsset.name = "Test Rain Loop";
    audioAsset.originalFileName = "rain_loop.wav";
    audioAsset.filePath = "Resources/Campaigns/test/Assets/Audio/rain_loop.wav";
    audioAsset.type = AudioAssetType::Ambience;
    audioAsset.defaultVolume = 0.35f;
    audioAsset.loop = true;
    audioAsset.durationSeconds = 72;
    audioAsset.lastPlayedAtUtc = "2026-05-23T20:00:00Z";
    campaign.audioAssets.push_back(audioAsset);

    AudioPlaylistModel playlist;
    playlist.metadata.id = "test_playlist_" + suffix;
    playlist.name = "Test Ambience Stack";
    playlist.audioAssetIds.push_back(audioAsset.metadata.id);
    playlist.shuffle = true;
    playlist.repeat = true;
    playlist.oneShot = true;
    playlist.crossfadeSeconds = 1.5f;
    playlist.metadata.tags = {"audio", "test"};
    campaign.audioPlaylists.push_back(playlist);

    AudioAmbienceLayerModel ambienceLayer;
    ambienceLayer.metadata.id = "test_ambience_layer_" + suffix;
    ambienceLayer.name = "Rain Layer";
    ambienceLayer.audioAssetId = audioAsset.metadata.id;
    ambienceLayer.volume = 0.42f;
    ambienceLayer.loop = true;
    ambienceLayer.active = true;
    ambienceLayer.sortOrder = 2;
    ambienceLayer.metadata.tags = {"ambience", "test"};
    campaign.audioAmbienceLayers.push_back(ambienceLayer);

    AudioTriggerModel audioTrigger;
    audioTrigger.metadata.id = "test_audio_trigger_" + suffix;
    audioTrigger.name = "Scene Rain Trigger";
    audioTrigger.sourceType = "StoryBeat";
    audioTrigger.sourceEntityId = "test_scene_" + suffix;
    audioTrigger.eventName = "Scene Started";
    audioTrigger.audioAssetId = audioAsset.metadata.id;
    audioTrigger.volume = 0.5f;
    audioTrigger.loop = true;
    audioTrigger.enabled = true;
    campaign.audioTriggers.push_back(audioTrigger);

    StoryThreadModel storyThread;
    storyThread.metadata.id = "test_story_" + suffix;
    storyThread.title = "Test Story";
    StoryBeatModel storyBeat;
    storyBeat.metadata.id = "test_scene_" + suffix;
    storyBeat.title = "Test Scene";
    storyBeat.summary = "Private GM outline.";
    storyBeat.playerSummary = "Public scene summary.";
    storyBeat.readAloudText = "Read this scene text aloud.";
    storyBeat.clues = "Hidden clue answer.";
    storyBeat.consequences = "Private consequence branch.";
    storyBeat.gmNotes = "Private GM note.";
    storyBeat.cueAudioAssetId = audioAsset.metadata.id;
    StoryLinkModel handoutReveal;
    handoutReveal.type = StoryLinkType::Reveals;
    handoutReveal.targetEntityId = handout.metadata.id;
    handoutReveal.targetEntityType = "Handout";
    storyBeat.links.push_back(handoutReveal);
    storyThread.beats.push_back(storyBeat);
    campaign.storyThreads.push_back(storyThread);
    campaign.localPlayerDisplay.publicSceneId = storyBeat.metadata.id;

    CampaignDiceMacroModel macro;
    macro.metadata.id = "test_macro_" + suffix;
    macro.name = "Toolkit macro";
    macro.expression = "1d20+2";
    campaign.diceMacros.push_back(macro);
    StoryObjectiveModel firstObjective;
    firstObjective.metadata.id = "test_objective_first_" + suffix;
    firstObjective.title = "Find the test clue";
    firstObjective.linkedStoryThreadId = storyThread.metadata.id;
    firstObjective.linkedStoryBeatId = storyBeat.metadata.id;
    firstObjective.assigneeCharacterIds = {character.metadata.id};
    StoryObjectiveModel secondObjective;
    secondObjective.metadata.id = "test_objective_second_" + suffix;
    secondObjective.title = "Finish the test route";
    secondObjective.prerequisiteObjectiveIds = {firstObjective.metadata.id};
    campaign.storyObjectives = {firstObjective, secondObjective};
    StoryClueModel clue;
    clue.metadata.id = "test_clue_" + suffix;
    clue.title = "Test clue";
    clue.sourceEntityId = character.metadata.id;
    campaign.storyClues.push_back(clue);
    SafetyBoundaryModel boundary;
    boundary.metadata.id = "test_boundary_" + suffix;
    boundary.topic = "Test boundary";
    campaign.safetyProfile.boundaries.push_back(boundary);
    SafetyContentWarningModel warning;
    warning.metadata.id = "test_warning_" + suffix;
    warning.targetEntityId = character.metadata.id;
    warning.targetEntityType = "Player";
    warning.warning = "Test warning";
    campaign.safetyProfile.warnings.push_back(warning);

    RulesetDefinitionModel rulesetDefinition;
    rulesetDefinition.metadata.id = "test_ruleset_definition_" + suffix;
    rulesetDefinition.name = "Test Homebrew Layer " + suffix;
    rulesetDefinition.version = "1.0.0";
    rulesetDefinition.baseRulesetId = "dnd-2024-5.5e";
    rulesetDefinition.metadata.tags = {"homebrew", "test"};
    rulesetDefinition.abilityScores.push_back({"might", "Might", "MIG", "Physical power"});
    rulesetDefinition.derivedStats.push_back({"resolve_dc", "Resolve DC", "8 + proficiency + spirit.mod", "Class feature difficulty"});
    rulesetDefinition.skills.push_back({"sneak", "Sneak", "agility", "Move unseen"});
    rulesetDefinition.defenses.push_back({"guard", "Guard", "10 + agility.mod + shield", "Avoid attacks"});
    rulesetDefinition.resources.push_back({"focus", "Focus", "level + spirit.mod", "long_rest", "Heroic focus pool"});
    rulesetDefinition.diceFormulas.push_back({"strain", "Strain Roll", "1d20 + endurance.mod", "Endure pressure"});
    rulesetDefinition.currencies.push_back({"silver", "Silver", "sp", 10, "Standard trade coin"});
    rulesetDefinition.sizes.push_back({"towering", "Towering", 15, "Very large creatures"});
    rulesetDefinition.speeds.push_back({"burrow", "Burrow", "feet", "Move underground"});
    rulesetDefinition.conditions.push_back({"dazed", "Dazed", "Slow to react", {"Cannot take reactions", "Disadvantage on initiative"}, "End of next turn"});
    rulesetDefinition.classes.push_back({"warden", "Warden", "d10", {"might", "spirit"}, "Front-line protector"});
    rulesetDefinition.subclasses.push_back({"stone_warden", "Stone Warden", "warden", "Earthbound defender"});
    rulesetDefinition.ancestries.push_back({"emberkin", "Emberkin", "medium", "walk", "Fire-touched folk"});
    rulesetDefinition.backgrounds.push_back({"relic_hunter", "Relic Hunter", {"sneak", "lore"}, "Finder of old secrets"});
    rulesetDefinition.features.push_back({"guarding_mark", "Guarding Mark", "class", 2, "Punish foes who ignore you"});
    rulesetDefinition.equipmentCategories.push_back({"focus_relic", "Focus Relic", "held", "Implements for channeling focus"});
    rulesetDefinition.spellTraditions.push_back({"primal_echo", "Primal Echo", "spirit", "Nature and memory magic"});
    rulesetDefinition.monsterTemplates.push_back({"ashbound", "Ashbound", "medium", "party_level / 2", "Fire-scarred creature template"});
    campaign.rulesetDefinitions.push_back(rulesetDefinition);

    RulesetContentPackModel contentPack;
    contentPack.metadata.id = "test_content_pack_" + suffix;
    contentPack.rulesetId = rulesetDefinition.metadata.id;
    contentPack.name = "Sky Isles Homebrew Pack " + suffix;
    contentPack.version = "0.2.0";
    contentPack.author = "Test GM";
    contentPack.license = "Table Use Only";
    contentPack.importedAtUtc = "2026-05-22T00:00:00Z";
    contentPack.source = "local-test-fixture";
    contentPack.definitionJson = R"({"pack":"sky-isles"})";
    contentPack.enabled = true;
    campaign.rulesetContentPacks.push_back(contentPack);

    HouseRuleModel houseRule;
    houseRule.metadata.id = "test_house_rule_" + suffix;
    houseRule.label = "Potion Timing";
    houseRule.description = "Drinking a potion uses a bonus action.";
    houseRule.enabled = true;
    houseRule.changeHistory.push_back({"2026-05-22T00:00:00Z", "Created house rule for faster combat."});
    campaign.houseRules.push_back(houseRule);

    CampaignSavedFilterModel savedFilter;
    savedFilter.metadata.id = "test_saved_filter_" + suffix;
    savedFilter.name = "Open Clues";
    savedFilter.query = "hatch";
    savedFilter.entityTypes = {"Dungeon", "Session"};
    savedFilter.requiredTags = {"prep"};
    savedFilter.smartCollection = true;
    campaign.savedFilters.push_back(savedFilter);

    CampaignEntityBookmarkModel bookmark;
    bookmark.metadata.id = "test_bookmark_" + suffix;
    bookmark.entityId = character.metadata.id;
    bookmark.entityType = "Player";
    bookmark.label = "Test Hero";
    bookmark.favorite = true;
    bookmark.lastOpenedAtUtc = "2026-05-23T20:30:00Z";
    bookmark.openCount = 2;
    campaign.entityBookmarks.push_back(bookmark);

    return campaign;
}

bool testModelNormalization()
{
    CharacterModel character;
    character.level = 2;
    character.attributes = {{"Constitution", 14}};
    normalizeCharacterHitPoints(character);

    bool ok = true;
    ok &= require(character.maxHitPoints == 20, "Character max HP normalization failed");
    ok &= require(character.currentHitPoints == 20, "Character current HP normalization failed");

    character.currentHitPoints = 0;
    normalizeCharacterHitPoints(character);
    ok &= require(character.currentHitPoints == 0 && character.maxHitPoints == 20,
                  "Normalizing an unconscious character must not heal them");
    character.currentHitPoints = -4;
    normalizeCharacterHitPoints(character);
    ok &= require(character.currentHitPoints == 0, "Negative current HP must clamp to zero, not heal");
    character.currentHitPoints = 25;
    normalizeCharacterHitPoints(character);
    ok &= require(character.currentHitPoints == 20, "Current HP above maximum must clamp to maximum");
    character.temporaryHitPoints = 6;
    adjustCharacterHitPoints(character, -4);
    ok &= require(character.currentHitPoints == 20 && character.temporaryHitPoints == 2,
                  "Character-sheet damage must spend temporary health first");
    adjustCharacterHitPoints(character, -5);
    ok &= require(character.currentHitPoints == 17 && character.temporaryHitPoints == 0,
                  "Damage beyond temporary health must reduce current health");
    character.temporaryHitPoints = 3;
    adjustCharacterHitPoints(character, 5);
    ok &= require(character.currentHitPoints == 20 && character.temporaryHitPoints == 3,
                  "Healing caps at maximum and preserves temporary health");
    adjustCharacterHitPoints(character, std::numeric_limits<int>::min());
    ok &= require(character.currentHitPoints == 0 && character.temporaryHitPoints == 0,
                  "Large damage must not overflow into healing");
    adjustCharacterHitPoints(character, std::numeric_limits<int>::max());
    ok &= require(character.currentHitPoints == 20, "Large healing must remain bounded");

    EncounterModel encounter;
    encounter.battlefieldWidth = 500;
    encounter.battlefieldHeight = 1;
    encounter.cellSizeFeet = 0;
    encounter.participants.push_back(EncounterParticipantModel{});
    encounter.participants.front().x = 1000;
    encounter.participants.front().y = -4;
    encounter.participants.front().startingX = 1000;
    encounter.participants.front().startingY = -4;
    encounter.participants.front().waveNumber = 0;
    encounter.participants.front().reinforcementRound = -3;
    encounter.participants.front().maxHitPoints = 10;
    encounter.participants.front().currentHitPoints = 20;
    normalizeEncounter(encounter);

    ok &= require(encounter.cellSizeFeet == 1, "Encounter cell size normalization failed");
    ok &= require(encounter.battlefieldWidth == 500 && encounter.battlefieldHeight == 4 && encounter.battlefieldCells.size() == 2000,
                  "Encounter normalization must preserve supported grid width independently of feet per cell and repair the minimum height");
    ok &= require(encounter.participants.front().currentHitPoints == 10, "Encounter HP clamp failed");
    ok &= require(encounter.participants.front().waveNumber == 1, "Encounter wave normalization failed");
    ok &= require(encounter.participants.front().reinforcementRound == 0, "Encounter reinforcement normalization failed");
    ok &= require(encounter.participants.front().startingX == encounter.battlefieldWidth - 1 && encounter.participants.front().startingY == 0, "Encounter starting position normalization failed");

    DiceRollPresetModel preset;
    preset.name = "Claw Damage";
    preset.expression = "2d6+3";
    preset.notes = "Named combat roll.";
    const std::vector<DiceRollPresetModel> presets = PersistenceSerialization::deserializeDiceRollPresets(PersistenceSerialization::serializeDiceRollPresets({preset}));
    ok &= require(presets.size() == 1 && presets.front().name == "Claw Damage" && presets.front().expression == "2d6+3", "Dice roll preset serialization failed");
    return ok;
}

bool testNewCharacterHitPointEstimates()
{
    DND5e ruleset;
    CharacterModel character;
    character.level = 1;
    character.attributes = {{"Constitution", 14}};

    const std::vector<std::pair<std::string, int>> expectedStartingHp{
        {"Barbarian", 14}, {"Bard", 10}, {"Cleric", 10}, {"Druid", 10},
        {"Fighter", 12}, {"Monk", 10}, {"Paladin", 12}, {"Ranger", 12},
        {"Rogue", 10}, {"Sorcerer", 8}, {"Warlock", 10}, {"Wizard", 8}};

    bool ok = true;
    for (const auto &[className, expected] : expectedStartingHp)
    {
        character.selectedClass = className;
        ok &= require(estimateNewCharacterMaxHitPoints(character, &ruleset) == expected,
                      "DND class should use its explicit first-level HP profile: " + className);
    }

    character.selectedClass = "Ranger";
    character.attributes["Constitution"] = 10;
    ok &= require(estimateNewCharacterMaxHitPoints(character, &ruleset) == 10,
                  "Ranger level one should use its class starting HP at Constitution 10");
    character.level = 2;
    character.attributes["Constitution"] = 14;
    ok &= require(estimateNewCharacterMaxHitPoints(character, &ruleset) == 20,
                  "Later levels should use the ruleset fixed HP progression after the explicit first level");

    RulesetDefinitionModel customDefinition;
    customDefinition.name = "Custom fallback";
    DefinitionBackedRuleSet customRules(customDefinition);
    character.level = 1;
    character.selectedClass = "Custom class";
    character.attributes["Constitution"] = 14;
    ok &= require(estimateNewCharacterMaxHitPoints(character, &customRules) == 10,
                  "Rulesets without explicit starting HP should retain the generic creator estimate");

    character.maxHitPoints = 37;
    character.currentHitPoints = 11;
    (void)estimateNewCharacterMaxHitPoints(character, &ruleset);
    ok &= require(character.maxHitPoints == 37 && character.currentHitPoints == 11,
                  "Estimating new-character HP must not overwrite manual or saved character HP");
    return ok;
}

bool testSessionControlService()
{
    Campaign campaign;
    campaign.metadata.id = "test_session_control";
    InWorldDateTime openingTime;
    openingTime.year = 1492;
    openingTime.month = 3;
    openingTime.day = 14;
    openingTime.hour = 8;
    openingTime.minute = 0;
    InWorldTime::setDateTime(campaign.clock, openingTime, false);

    CharacterModel character;
    character.metadata.id = "test_session_control_character";
    std::snprintf(character.nameBuffer, sizeof(character.nameBuffer), "%s", "Clock Hero");
    campaign.characters.push_back(character);

    bool ok = true;
    ok &= require(SessionControl::activeSession(campaign) == nullptr, "New campaign should not have an active session");

    SessionModel *started = SessionControl::startLiveSession(campaign, "Clock Test");
    ok &= require(started != nullptr, "Session control should start a live session");
    ok &= require(SessionControl::activeSession(campaign) == started, "Started session should become active");
    ok &= require(started && started->title == "Clock Test", "Started session should keep the requested title");
    ok &= require(started && started->liveModeActive, "Started session should be marked live");
    ok &= require(started && !started->startedAtUtc.empty(), "Started session should record a start timestamp");
    ok &= require(started && started->participantCharacterIds.size() == 1 && started->participantCharacterIds.front() == character.metadata.id, "Started session should snapshot party participants");
    ok &= require(campaign.sessionIds.size() == 1 && campaign.sessions.size() == 1, "Started session should be tracked by campaign session IDs and session records");
    ok &= require(SessionControl::startLiveSession(campaign, "Duplicate") == nullptr, "Session control should not start a second live session");

    std::string inWorldTime;
    ok &= require(SessionControl::advanceInWorldTime(campaign, "10 minutes", "Searching the archive", "Dawn", &inWorldTime), "Session control should advance in-world time");
    ok &= require(inWorldTime.find("14 March 1492 08:10") != std::string::npos, "Game time advance should use the campaign clock");
    ok &= require(campaign.sessions.front().notes.back().linkedEntityType == kSessionClockNoteLinkType, "Game time advance should be saved as a clock-linked session note");
    ok &= require(campaign.timelineEvents.size() == 1 && campaign.timelineEvents.back().inWorldDate == inWorldTime, "Game time advance should create a timeline event with the in-world timestamp");
    InWorldDateTime towerTime;
    towerTime.year = 1492;
    towerTime.month = 3;
    towerTime.day = 14;
    towerTime.hour = 16;
    towerTime.minute = 0;
    ok &= require(SessionControl::setInWorldDateTime(campaign, towerTime, "Reached the tower", &inWorldTime), "Session control should set typed in-world time");
    ok &= require(inWorldTime.find("14 March 1492 16:00") != std::string::npos, "Game time adjustment should report the campaign clock label");
    ok &= require(campaign.timelineEvents.size() == 2 && campaign.timelineEvents.back().description.find("Reached the tower") != std::string::npos, "Game time adjustment should create a timeline event with the reason");

    const std::string breakClock = inWorldTime;
    const std::size_t clockTimelineEvents = campaign.timelineEvents.size();
    ok &= require(!SessionControl::isLiveSessionBreakActive(campaign), "Live session should not begin on break");
    ok &= require(SessionControl::startBreak(campaign, "Audio paused: 2 channels.", breakClock), "Session control should start a break");
    ok &= require(SessionControl::isLiveSessionBreakActive(campaign), "Session break should become active");
    ok &= require(campaign.sessions.front().notes.back().body.find("Audio paused") != std::string::npos, "Session break note should preserve optional break detail");
    ok &= require(campaign.timelineEvents.size() == clockTimelineEvents + 1 && campaign.timelineEvents.back().title == "Session break started", "Session break should create a timeline marker");
    ok &= require(campaign.timelineEvents.back().inWorldDate == breakClock, "Session break timeline marker should keep the current in-world time");
    ok &= require(!SessionControl::startBreak(campaign), "Session control should not duplicate active break markers");
    ok &= require(SessionControl::resumeFromBreak(campaign, "Audio resumed: 2 channels.", breakClock), "Session control should resume from break");
    ok &= require(!SessionControl::isLiveSessionBreakActive(campaign), "Session break should clear after resume");
    ok &= require(campaign.timelineEvents.size() == clockTimelineEvents + 2 && campaign.timelineEvents.back().title == "Session break resumed", "Session resume should create a timeline marker");
    ok &= require(campaign.sessions.front().notes.back().body.find("Audio resumed") != std::string::npos, "Session resume note should preserve optional resume detail");
    ok &= require(!SessionControl::resumeFromBreak(campaign), "Session control should not resume when no break is active");

    campaign.localPlayerDisplay.enabled = true;
    campaign.localPlayerDisplay.showMap = true;
    campaign.localPlayerDisplay.publicMapId = "session_public_map";
    campaign.localPlayerDisplay.published = true;
    campaign.localPlayerDisplay.blackout = false;
    campaign.localPlayerDisplay.publishedPayloadFingerprint =
        localPlayerDisplayPayloadFingerprint(campaign.localPlayerDisplay);

    EntityId endedId;
    std::string endedTitle;
    ok &= require(SessionControl::endLiveSession(campaign, &endedId, &endedTitle), "Session control should end the active session");
    ok &= require(SessionControl::activeSession(campaign) == nullptr, "Ended session should no longer be active");
    ok &= require(endedId == campaign.sessions.front().metadata.id && endedTitle == "Clock Test", "Ending session should report the ended session identity");
    ok &= require(!campaign.sessions.front().endedAtUtc.empty(), "Ended session should record an end timestamp");
    ok &= require(campaign.localPlayerDisplay.blackout && !campaign.localPlayerDisplay.published &&
                      campaign.localPlayerDisplay.publishedPayloadFingerprint == 0,
                  "Ending a session should fail closed by blacking out the local player display");
    ok &= require(campaign.localPlayerDisplay.enabled && campaign.localPlayerDisplay.publicMapId == "session_public_map",
                  "Ending a session should preserve the configured public payload for a later reviewed publication");
    ok &= require(!SessionControl::endLiveSession(campaign), "Session control should not end a missing active session");
    return ok;
}

bool testCampaignSearchIndex()
{
    Campaign campaign = makeCampaign("search");

    NPCModel npc;
    npc.metadata.id = "test_npc_search";
    npc.name = "Archivist Vela";
    npc.role = "Clue keeper";
    npc.dialogueNotes = "Knows the obsidian password.";
    npc.metadata.tags = {"clue", "library"};
    campaign.npcs.push_back(npc);

    SpellModel spell;
    spell.metadata.id = "test_spell_search";
    spell.name = "Lantern of Recall";
    spell.description = "Reveals echoes of forgotten rooms.";
    spell.metadata.tags = {"memory"};
    campaign.customSpells.push_back(spell);

    MonsterDefinitionModel monster;
    monster.metadata.id = "test_monster_search";
    monster.name = "Mnemonic Warden";
    monster.traits = "Guards indexed secrets.";
    monster.encounterRoleTags = {"boss"};
    campaign.customMonsters.push_back(monster);

    EncounterModel encounter;
    encounter.metadata.id = "test_encounter_search";
    encounter.name = "Library Ambush";
    encounter.description = "A fight among shelves of living notes.";
    campaign.encounters.push_back(encounter);

    StoryThreadModel thread;
    thread.metadata.id = "test_story_search";
    thread.title = "The Indexed Conspiracy";
    thread.summary = "A faction hides clues in cross-referenced ledgers.";
    StoryBeatModel beat;
    beat.metadata.id = "test_story_beat_search";
    beat.title = "Find the Red Folio";
    beat.summary = "The folio points to Vela.";
    thread.beats.push_back(beat);
    campaign.storyThreads.push_back(thread);

    StoryObjectiveModel objective;
    objective.metadata.id = "test_objective_search";
    objective.title = "Find the missing lamplighter";
    objective.playerSummary = "Follow the lantern trail through the archive.";
    objective.gmNotes = "The missing lamplighter is hiding from the warden.";
    objective.status = StoryObjectiveStatus::Active;
    campaign.storyObjectives.push_back(objective);

    StoryClueModel clue;
    clue.metadata.id = "test_clue_search";
    clue.title = "Lamplighter's brass key";
    clue.playerText = "A key marked with a small lantern.";
    clue.gmTruth = "It unlocks the warden's private index.";
    clue.status = StoryClueStatus::Discovered;
    campaign.storyClues.push_back(clue);

    CampaignSearchIndex index;
    index.rebuild(campaign);

    bool ok = true;
    ok &= require(index.documents().size() >= 12, "Campaign search index should include major campaign entities");

    const std::vector<CampaignSearchResult> npcResults = index.search("obsidian");
    ok &= require(!npcResults.empty() && npcResults.front().document.id == npc.metadata.id, "Campaign search should find NPC body text");

    const std::vector<CampaignSearchResult> tagResults = index.search("library");
    ok &= require(std::any_of(tagResults.begin(), tagResults.end(), [&](const CampaignSearchResult &result) {
        return result.document.id == npc.metadata.id;
    }), "Campaign search should match metadata tags");

    const std::vector<CampaignSearchResult> storyResults = index.search("hatch");
    ok &= require(std::any_of(storyResults.begin(), storyResults.end(), [](const CampaignSearchResult &result) {
        return result.document.type == "Dungeon" || result.document.type == "Session";
    }), "Campaign search should include dungeon and session prep text");

    const std::vector<CampaignSearchResult> objectiveResults = index.search("missing lamplighter");
    ok &= require(std::any_of(objectiveResults.begin(), objectiveResults.end(), [&](const CampaignSearchResult &result) {
        return result.document.id == objective.metadata.id && result.document.type == "Objective" &&
               result.document.subtitle.find("Active") != std::string::npos;
    }), "Campaign search should find saved objectives by title and preserve their stable navigation ID");

    MapModel exactTitleMap;
    exactTitleMap.metadata.id = "test_map_exact_title";
    exactTitleMap.name = "The Lanternlands";
    campaign.maps.push_back(exactTitleMap);
    EncounterModel repeatedBodyEncounter;
    repeatedBodyEncounter.metadata.id = "test_encounter_repeated_title_terms";
    repeatedBodyEncounter.name = "Roadside Trouble";
    repeatedBodyEncounter.description = "the the the the the the the the the the the the lanternlands";
    campaign.encounters.push_back(repeatedBodyEncounter);
    StoryThreadModel repeatedBodyStory;
    repeatedBodyStory.metadata.id = "test_story_repeated_title_terms";
    repeatedBodyStory.title = "A Different Chronicle";
    repeatedBodyStory.summary = "the the the the the the the the the the lanternlands";
    campaign.storyThreads.push_back(repeatedBodyStory);
    index.rebuild(campaign);
    const std::vector<CampaignSearchResult> exactTitleResults = index.search("  The   Lanternlands ");
    const std::vector<CampaignSearchResult> partialTitleResults = index.search("Lanternlands");
    ok &= require(!exactTitleResults.empty() && exactTitleResults.front().document.id == exactTitleMap.metadata.id &&
                      exactTitleResults.front().titleMatchPriority == 3,
                  "An exact campaign entity title must outrank repeated body-word matches");
    ok &= require(!partialTitleResults.empty() && partialTitleResults.front().document.id == exactTitleMap.metadata.id,
                  "A contained title match must outrank accumulated body occurrences");

    ItemModel firstTwin;
    firstTwin.metadata.id = "test_item_twin_a";
    firstTwin.name = "Twin Lantern";
    ItemModel secondTwin = firstTwin;
    secondTwin.metadata.id = "test_item_twin_z";
    campaign.customItems.push_back(secondTwin);
    campaign.customItems.push_back(firstTwin);
    index.rebuild(campaign);
    const std::vector<CampaignSearchResult> twinResults = index.search("Twin Lantern");
    ok &= require(twinResults.size() >= 2 && twinResults[0].document.id == firstTwin.metadata.id &&
                      twinResults[1].document.id == secondTwin.metadata.id,
                  "Equal search matches must use stable title, type, and ID tie-breaking");

    const std::vector<CampaignSearchResult> clueResults = index.search("warden private index");
    ok &= require(std::any_of(clueResults.begin(), clueResults.end(), [&](const CampaignSearchResult &result) {
        return result.document.id == clue.metadata.id && result.document.type == "Clue";
    }), "Campaign search should find GM clue text without exposing a separate unsaved record");

    const gm::ux::NavigationEntity objectiveNavigation = gm::ux::resolveNavigationEntity(campaign, objective.metadata.id);
    const gm::ux::NavigationEntity clueNavigation = gm::ux::resolveNavigationEntity(campaign, clue.metadata.id);
    ok &= require(objectiveNavigation.available && objectiveNavigation.type == "Objective" &&
                  clueNavigation.available && clueNavigation.type == "Clue",
                  "Saved toolkit records should pass stable-ID navigation validation");
    ok &= require(CampaignTags::setTagsForEntity(campaign, objective.metadata.id, {"urgent"}) &&
                  CampaignTags::tagsForEntity(campaign, objective.metadata.id) == std::vector<std::string>{"urgent"},
                  "Saved objectives should support the standard search-result tag action");

    const std::vector<CampaignSearchResult> limitedResults = index.search("test", 2);
    ok &= require(limitedResults.size() == 2, "Campaign search should honor result limits");
    return ok;
}

bool testCampaignTagService()
{
    Campaign campaign = makeCampaign("tags");
    campaign.metadata.id = "test_campaign_tags";
    campaign.metadata.tags = {" campaign ", "Campaign", "", "prep"};
    campaign.characters.front().metadata.tags = {"hero"};

    StoryThreadModel thread;
    thread.metadata.id = "test_story_tags";
    thread.title = "Tagged Story";
    StoryBeatModel beat;
    beat.metadata.id = "test_beat_tags";
    beat.title = "Tagged Beat";
    thread.beats.push_back(beat);
    campaign.storyThreads.push_back(thread);

    bool ok = true;
    const std::vector<std::string> normalized = CampaignTags::normalize({" clue ", "Clue", "", "npc"});
    ok &= require(normalized.size() == 2 && normalized.front() == "clue" && normalized.back() == "npc", "Tag normalization should trim and deduplicate tags");

    ok &= require(CampaignTags::setTagsForEntity(campaign, campaign.characters.front().metadata.id, {"frontline", "hero", "frontline"}), "Tag service should update character tags");
    ok &= require(campaign.characters.front().metadata.tags.size() == 2 && campaign.characters.front().metadata.tags.front() == "frontline", "Tag service should normalize updated entity tags");

    ok &= require(CampaignTags::setTagsForEntity(campaign, beat.metadata.id, {"clue", "scene"}), "Tag service should update nested story beat tags");
    ok &= require(campaign.storyThreads.back().beats.front().metadata.tags.size() == 2, "Nested story beat tags should be stored on the beat");

    const EntityId characterId = campaign.characters.front().metadata.id;
    ok &= require(CampaignTags::setEntityArchived(campaign, characterId, true), "Tag service should archive an entity by stable ID");
    ok &= require(CampaignTags::isEntityArchived(campaign, characterId), "Archived state should be readable by stable ID");
    ok &= require(std::find(campaign.characters.front().metadata.tags.begin(), campaign.characters.front().metadata.tags.end(), "frontline") != campaign.characters.front().metadata.tags.end(), "Archiving should preserve unrelated tags");
    ok &= require(CampaignTags::setEntityArchived(campaign, characterId, false), "Tag service should restore an entity by stable ID");
    ok &= require(!CampaignTags::isEntityArchived(campaign, characterId), "Restoring should remove the archived state");
    ok &= require(!CampaignTags::setEntityArchived(campaign, "missing_entity", true), "Archiving should reject missing stable IDs");

    const std::vector<std::string> allTags = CampaignTags::collectAllTags(campaign);
    ok &= require(std::find(allTags.begin(), allTags.end(), "campaign") != allTags.end(), "Tag service should collect campaign tags");
    ok &= require(std::find(allTags.begin(), allTags.end(), "frontline") != allTags.end(), "Tag service should collect entity tags");
    ok &= require(std::find(allTags.begin(), allTags.end(), "scene") != allTags.end(), "Tag service should collect nested entity tags");
    ok &= require(!CampaignTags::setTagsForEntity(campaign, "missing_entity", {"lost"}), "Tag service should report missing entities");
    return ok;
}

bool testCampaignCollectionService()
{
    Campaign campaign = makeCampaign("collections");
    campaign.dungeons.front().metadata.tags = {"prep", "dungeon"};
    campaign.sessions.front().metadata.tags = {"prep", "session"};
    campaign.audioAssets.front().metadata.tags = {"ambience"};

    CampaignSearchIndex index;
    index.rebuild(campaign);

    CampaignSavedFilterModel filter;
    filter.name = "Prep Notes";
    filter.query = "Test";
    filter.requiredTags = {"prep"};
    filter.entityTypes = {"Dungeon", "Session"};

    const std::vector<CampaignSearchResult> results = CampaignCollections::evaluate(index, filter);

    bool ok = true;
    ok &= require(std::any_of(results.begin(), results.end(), [](const CampaignSearchResult &result) {
        return result.document.type == "Dungeon";
    }) && std::any_of(results.begin(), results.end(), [](const CampaignSearchResult &result) {
        return result.document.type == "Session";
    }), "Smart collection should combine query, tag, and entity type filters");
    ok &= require(std::all_of(results.begin(), results.end(), [](const CampaignSearchResult &result) {
        return result.document.type == "Dungeon" || result.document.type == "Session";
    }), "Smart collection should exclude non-matching entity types");

    filter.query.clear();
    filter.requiredTags = {"ambience"};
    filter.entityTypes.clear();
    const std::vector<CampaignSearchResult> tagOnlyResults = CampaignCollections::evaluate(index, filter);
    ok &= require(tagOnlyResults.size() == 1 && tagOnlyResults.front().document.type == "Audio", "Smart collection should support tag-only saved filters");
    return ok;
}

bool testCampaignReferenceService()
{
    Campaign campaign = makeCampaign("references");
    campaign.characters.front().portraitAssetId = "Resources/Campaigns/test/Assets/Portraits/test_character_references.png";
    campaign.storyDocumentIds.push_back("doc_known_reference_runbook");
    campaign.sessions.front().storyDocumentIds.push_back("doc_known_reference_runbook");
    campaign.sessions.front().storyDocumentIds.push_back("missing_session_story_document");

    FactionModel atlasFaction;
    atlasFaction.metadata.id = "atlas_reference_faction";
    atlasFaction.name = "Atlas Faction";
    campaign.factions.push_back(atlasFaction);
    WorldPlaceModel atlasCapital;
    atlasCapital.metadata.id = "atlas_reference_capital";
    atlasCapital.type = WorldPlaceType::Capital;
    atlasCapital.name = "Reference Capital";
    atlasCapital.nationId = "atlas_reference_nation";
    atlasCapital.linkedMapId = campaign.maps.front().metadata.id;
    atlasCapital.linkedFeatureId = campaign.maps.front().features.front().metadata.id;
    campaign.worldPlaces.push_back(atlasCapital);
    NationModel atlasNation;
    atlasNation.metadata.id = "atlas_reference_nation";
    atlasNation.name = "Reference Nation";
    atlasNation.capitalPlaceId = atlasCapital.metadata.id;
    atlasNation.factionIds.push_back(atlasFaction.metadata.id);
    campaign.nations.push_back(atlasNation);

    StoryThreadModel thread;
    thread.metadata.id = "test_reference_story";
    thread.title = "Reference Story";
    StoryLinkModel threadLink;
    threadLink.label = "Points at the hero";
    threadLink.targetEntityId = campaign.characters.front().metadata.id;
    thread.links.push_back(threadLink);
    campaign.storyThreads.push_back(thread);

    EncounterModel encounter;
    encounter.metadata.id = "test_reference_encounter";
    encounter.name = "Reference Encounter";
    EncounterParticipantModel participant;
    participant.sourceEntityId = campaign.characters.front().metadata.id;
    participant.displayName = "Hero in combat";
    encounter.participants.push_back(participant);
    campaign.encounters.push_back(encounter);

    const std::vector<CampaignBacklink> characterLinks = CampaignReferences::backlinksTo(campaign, campaign.characters.front().metadata.id);

    bool ok = true;
    ok &= require(std::any_of(characterLinks.begin(), characterLinks.end(), [](const CampaignBacklink &link) {
        return link.sourceType == "Map" && link.relationship.find("map token") != std::string::npos;
    }), "Backlinks should include map tokens that reference a character");
    ok &= require(std::any_of(characterLinks.begin(), characterLinks.end(), [](const CampaignBacklink &link) {
        return link.sourceType == "Session" && link.relationship == "session participant";
    }), "Backlinks should include session participants");
    ok &= require(std::any_of(characterLinks.begin(), characterLinks.end(), [](const CampaignBacklink &link) {
        return link.sourceType == "Story Arc" && link.relationship == "Points at the hero";
    }), "Backlinks should include story links");
    ok &= require(std::any_of(characterLinks.begin(), characterLinks.end(), [](const CampaignBacklink &link) {
        return link.sourceType == "Encounter" && link.relationship == "combat participant";
    }), "Backlinks should include encounter participants");

    const std::vector<CampaignBacklink> itemLinks = CampaignReferences::backlinksTo(campaign, campaign.customItems.front().metadata.id);
    ok &= require(std::any_of(itemLinks.begin(), itemLinks.end(), [](const CampaignBacklink &link) {
        return link.sourceType == "Shop" && link.relationship == "stock item";
    }), "Backlinks should include shop stock references to items");
    const std::vector<CampaignBacklink> nationLinks = CampaignReferences::backlinksTo(campaign, atlasNation.metadata.id);
    ok &= require(std::any_of(nationLinks.begin(), nationLinks.end(), [](const CampaignBacklink &link) {
        return link.sourceType == "World Place" && link.relationship == "nation";
    }), "Backlinks should include world-place membership in a nation");
    const std::vector<CampaignBacklink> capitalLinks = CampaignReferences::backlinksTo(campaign, atlasCapital.metadata.id);
    ok &= require(std::any_of(capitalLinks.begin(), capitalLinks.end(), [](const CampaignBacklink &link) {
        return link.sourceType == "Nation" && link.relationship == "capital";
    }), "Backlinks should include nation capitals");

    campaign.maps.front().tokens.front().linkedEntityId = "missing_character_reference";
    StoryLinkModel brokenLink;
    brokenLink.label = "Broken story clue";
    brokenLink.targetEntityId = "missing_story_reference";
    campaign.storyThreads.back().links.push_back(brokenLink);
    campaign.nations.front().capitalPlaceId = "missing_atlas_capital";
    campaign.worldPlaces.front().linkedFeatureId = "missing_atlas_feature";

    const std::vector<CampaignBrokenReference> diagnostics = CampaignReferences::brokenReferences(campaign);
    ok &= require(std::any_of(diagnostics.begin(), diagnostics.end(), [](const CampaignBrokenReference &diagnostic) {
        return diagnostic.missingEntityId == "missing_character_reference" && diagnostic.sourceType == "Map";
    }), "Broken reference diagnostics should report map token targets");
    ok &= require(std::any_of(diagnostics.begin(), diagnostics.end(), [](const CampaignBrokenReference &diagnostic) {
        return diagnostic.missingEntityId == "missing_story_reference" && diagnostic.sourceType == "Story Arc";
    }), "Broken reference diagnostics should report story link targets");
    ok &= require(std::any_of(diagnostics.begin(), diagnostics.end(), [](const CampaignBrokenReference &diagnostic) {
        return diagnostic.missingEntityId == "missing_session_story_document" && diagnostic.sourceType == "Session";
    }), "Broken reference diagnostics should report unknown session story targets");
    ok &= require(std::none_of(diagnostics.begin(), diagnostics.end(), [](const CampaignBrokenReference &diagnostic) {
        return diagnostic.missingEntityId == "doc_known_reference_runbook";
    }), "Known story document ids should not be reported as broken references");
    ok &= require(std::none_of(diagnostics.begin(), diagnostics.end(), [](const CampaignBrokenReference &diagnostic) {
        return diagnostic.relationship == "portrait asset";
    }), "Portrait paths should not be reported as broken entity references");
    ok &= require(std::any_of(diagnostics.begin(), diagnostics.end(), [](const CampaignBrokenReference &diagnostic) {
        return diagnostic.missingEntityId == "missing_atlas_capital" && diagnostic.sourceType == "Nation";
    }), "Broken reference diagnostics should report missing nation capitals");
    ok &= require(std::any_of(diagnostics.begin(), diagnostics.end(), [](const CampaignBrokenReference &diagnostic) {
        return diagnostic.missingEntityId == "missing_atlas_feature" && diagnostic.sourceType == "World Place";
    }), "Broken reference diagnostics should report missing world-place feature links");
    return ok;
}

bool testCampaignGraphService()
{
    Campaign campaign = makeCampaign("graph");

    FactionModel faction;
    faction.metadata.id = "test_faction_graph";
    faction.name = "Graph Faction";
    campaign.factions.push_back(faction);

    NPCModel npc;
    npc.metadata.id = "test_npc_graph";
    npc.name = "Graph Envoy";
    npc.factionId = faction.metadata.id;
    campaign.npcs.push_back(npc);

    StoryThreadModel thread;
    thread.metadata.id = "test_story_graph";
    thread.title = "Graph Story";
    StoryBeatModel beat;
    beat.metadata.id = "test_beat_graph";
    beat.title = "Graph Beat";
    StoryLinkModel clue;
    clue.metadata.id = "test_clue_graph";
    clue.label = "Envoy clue";
    clue.targetEntityId = npc.metadata.id;
    beat.links.push_back(clue);
    thread.beats.push_back(beat);
    campaign.storyThreads.push_back(thread);

    const CampaignKnowledgeGraph graph = CampaignGraph::buildKnowledgeGraph(campaign);

    bool ok = true;
    ok &= require(std::any_of(graph.nodes.begin(), graph.nodes.end(), [](const CampaignGraphNode &node) {
        return node.id == "test_npc_graph" && node.type == "NPC";
    }), "Knowledge graph should include NPC nodes");
    ok &= require(std::any_of(graph.nodes.begin(), graph.nodes.end(), [](const CampaignGraphNode &node) {
        return node.id == "test_clue_graph" && node.type == "Clue";
    }), "Knowledge graph should include clue nodes");
    ok &= require(std::any_of(graph.edges.begin(), graph.edges.end(), [](const CampaignGraphEdge &edge) {
        return edge.fromId == "test_npc_graph" && edge.toId == "test_faction_graph" && edge.relationship == "member of";
    }), "Knowledge graph should include NPC faction edges");
    ok &= require(std::any_of(graph.edges.begin(), graph.edges.end(), [](const CampaignGraphEdge &edge) {
        return edge.fromId == "test_clue_graph" && edge.toId == "test_npc_graph" && edge.relationship == "targets";
    }), "Knowledge graph should include clue target edges");
    return ok;
}

bool testCampaignCommandPaletteService()
{
    Campaign campaign = makeCampaign("palette");
    StoryObjectiveModel objective;
    objective.metadata.id = "test_objective_palette";
    objective.title = "Find the missing lamplighter";
    campaign.storyObjectives.push_back(objective);
    CampaignSearchIndex index;
    index.rebuild(campaign);

    const std::vector<CampaignCommandPaletteItem> items = CampaignCommandPalette::buildItems(index);
    const std::vector<CampaignCommandPaletteItem> hatchItems = CampaignCommandPalette::filterItems(items, "Test Dungeon");
    const std::vector<CampaignCommandPaletteItem> saveItems = CampaignCommandPalette::filterItems(items, "save");
    const std::vector<CampaignCommandPaletteItem> objectiveItems = CampaignCommandPalette::filterItems(items, "missing lamplighter");
    const std::vector<CampaignCommandPaletteItem> objectivesCommands = CampaignCommandPalette::filterItems(items, "Objectives");
    const std::vector<CampaignCommandPaletteItem> cluesCommands = CampaignCommandPalette::filterItems(items, "Clues");

    gm::localization::LocalizationService &localization = gm::localization::LocalizationService::instance();
    const gm::localization::LocalePreference previousLocalePreference = localization.preference();
    localization.setPreference(gm::localization::LocalePreference::German, false);
    const std::vector<CampaignCommandPaletteItem> germanItems = CampaignCommandPalette::buildItems(index);
    const std::vector<CampaignCommandPaletteItem> displayItems = CampaignCommandPalette::filterItems(germanItems, "Spieleranzeige");
    const std::vector<CampaignCommandPaletteItem> germanObjectiveItems = CampaignCommandPalette::filterItems(germanItems, "lamplighter");
    const std::vector<CampaignCommandPaletteItem> germanObjectivesCommands = CampaignCommandPalette::filterItems(germanItems, "Ziele");
    const std::vector<CampaignCommandPaletteItem> germanCluesCommands = CampaignCommandPalette::filterItems(germanItems, "Hinweise");
    localization.setPreference(previousLocalePreference, false);

    bool ok = true;
    ok &= require(std::any_of(items.begin(), items.end(), [](const CampaignCommandPaletteItem &item) {
        return item.id == "workspace:cast";
    }), "Command palette should include stable workspace commands");
    ok &= require(std::any_of(items.begin(), items.end(), [](const CampaignCommandPaletteItem &item) {
        return item.id == "action:review_player_display";
    }), "Command palette should expose the player-display review inside Live");
    ok &= require(std::any_of(displayItems.begin(), displayItems.end(), [](const CampaignCommandPaletteItem &item) {
        return item.id == "action:review_player_display";
    }), "German command palette should find the player-display review by its visible label");
    ok &= require(std::any_of(objectivesCommands.begin(), objectivesCommands.end(), [](const CampaignCommandPaletteItem &item) {
        return item.id == "action:open_objectives";
    }), "Command palette should expose objectives even when no objective title matches the plural query");
    ok &= require(!objectivesCommands.empty() && objectivesCommands.front().id == "action:open_objectives",
                  "Exact Objectives shortcut should precede the general table-tools command");
    ok &= require(std::any_of(cluesCommands.begin(), cluesCommands.end(), [](const CampaignCommandPaletteItem &item) {
        return item.id == "action:open_clues";
    }), "Command palette should expose clues even when no clue title matches the plural query");
    ok &= require(!cluesCommands.empty() && cluesCommands.front().id == "action:open_clues",
                  "Exact Clues shortcut should precede the general table-tools command");
    ok &= require(std::any_of(germanObjectivesCommands.begin(), germanObjectivesCommands.end(), [](const CampaignCommandPaletteItem &item) {
        return item.id == "action:open_objectives";
    }), "German command palette should expose Ziele navigation by its visible label");
    ok &= require(!germanObjectivesCommands.empty() && germanObjectivesCommands.front().id == "action:open_objectives",
                  "Exact Ziele shortcut should precede the general table-tools command");
    ok &= require(std::any_of(germanCluesCommands.begin(), germanCluesCommands.end(), [](const CampaignCommandPaletteItem &item) {
        return item.id == "action:open_clues";
    }), "German command palette should expose Hinweise navigation by its visible label");
    ok &= require(!germanCluesCommands.empty() && germanCluesCommands.front().id == "action:open_clues",
                  "Exact Hinweise shortcut should precede the general table-tools command");
    ok &= require(std::any_of(germanObjectiveItems.begin(), germanObjectiveItems.end(), [&](const CampaignCommandPaletteItem &item) {
        return item.id == "open:" + objective.metadata.id && item.category == "Tischziel" &&
               item.label.find("Ziel öffnen") != std::string::npos;
    }), "German command palette should label saved objective navigation clearly");
    ok &= require(std::none_of(items.begin(), items.end(), [](const CampaignCommandPaletteItem &item) {
        const gm::ux::LegacySurfaceRegistration *surface = gm::ux::findLegacySurface(item.id);
        return surface && !gm::ux::isLegacyEntryPointAllowed(*surface, gm::ux::LegacyEntryPoint::UniversalSearch);
    }), "Command palette must not expose routes forbidden by the legacy-surface registry");
    ok &= require(std::none_of(items.begin(), items.end(), [](const CampaignCommandPaletteItem &item) {
        return item.id == "show:inventory_manager" || item.id == "show:dungeon_builder" || item.id == "show:knowledge_graph";
    }), "Command palette should not expose retired tool taxonomy");
    ok &= require(std::any_of(hatchItems.begin(), hatchItems.end(), [](const CampaignCommandPaletteItem &item) {
        return item.entityType == "Dungeon" && item.id.rfind("open:", 0) == 0;
    }), "Command palette should include entity open commands");
    ok &= require(std::any_of(saveItems.begin(), saveItems.end(), [](const CampaignCommandPaletteItem &item) {
        return item.id == "action:save";
    }), "Command palette should filter common actions");
    ok &= require(std::any_of(objectiveItems.begin(), objectiveItems.end(), [&](const CampaignCommandPaletteItem &item) {
        return item.id == "open:" + objective.metadata.id && item.entityType == "Objective" &&
               item.label.find(objective.title) != std::string::npos &&
               (item.category == "Table objective" || item.category == "Tischziel");
    }), "Command palette should label saved objective navigation clearly");
    return ok;
}

bool testCampaignBookmarkService()
{
    Campaign campaign = makeCampaign("bookmarks");
    CampaignCommandPaletteItem item{"open:" + campaign.maps.front().metadata.id, "Test Map", "Map", campaign.maps.front().metadata.id, "Map"};

    CampaignBookmarks::recordOpened(campaign, item);
    CampaignBookmarks::setFavorite(campaign, campaign.maps.front().metadata.id, "Map", "Test Map", true);

    const std::vector<CampaignEntityBookmarkModel> favorites = CampaignBookmarks::favorites(campaign);
    const std::vector<CampaignEntityBookmarkModel> recent = CampaignBookmarks::recent(campaign, 5);

    bool ok = true;
    ok &= require(CampaignBookmarks::isFavorite(campaign, campaign.maps.front().metadata.id), "Bookmark service should mark favorites");
    ok &= require(std::any_of(favorites.begin(), favorites.end(), [&](const CampaignEntityBookmarkModel &bookmark) {
        return bookmark.entityId == campaign.maps.front().metadata.id;
    }), "Bookmark service should list favorites");
    ok &= require(std::any_of(recent.begin(), recent.end(), [&](const CampaignEntityBookmarkModel &bookmark) {
        return bookmark.entityId == campaign.maps.front().metadata.id && bookmark.openCount == 1;
    }), "Bookmark service should record recent opens");
    CampaignBookmarks::setFavorite(campaign, campaign.maps.front().metadata.id, "Map", "Test Map", false);
    ok &= require(!CampaignBookmarks::isFavorite(campaign, campaign.maps.front().metadata.id), "Bookmark service should clear favorites");
    return ok;
}

bool testCampaignIndexRebuildService()
{
    Campaign campaign = makeCampaign("index_rebuild");
    CampaignIndexRebuildResult result = CampaignIndexRebuild::rebuild(campaign);

    bool ok = true;
    ok &= require(result.searchableDocuments > 0, "Index rebuild should report searchable documents");
    ok &= require(result.graphNodes > 0, "Index rebuild should report graph nodes");
    ok &= require(!result.rebuiltAtUtc.empty(), "Index rebuild should record rebuild timestamp");
    return ok;
}

bool testRulesetBasics()
{
    DND5e ruleset;
    const std::vector<std::string> attributes = ruleset.getAttributes();
    const std::vector<ItemModel> items = ruleset.getItems();
    const std::vector<MonsterDefinitionModel> monsters = ruleset.getMonsters();
    const auto findItem = [&](const std::string &name) -> const ItemModel * {
        for (const ItemModel &item : items)
        {
            if (item.name == name)
                return &item;
        }
        return nullptr;
    };

    bool ok = true;
    ok &= require(ruleset.matchesRulesetName("Dungeons & Dragons 5th Edition"), "DND ruleset should load legacy campaign names");
    ok &= require(std::string(ruleset.getVersionId()) == "dnd-2024-5.5e", "DND ruleset version id changed unexpectedly");
    const std::vector<RuleContentAttribution> attributions = ruleset.getContentAttributions();
    ok &= require(!attributions.empty(), "DND ruleset should expose content attribution metadata");
    ok &= require(attributions.front().sourceVersion == "5.2.1" && attributions.front().licenseName.find("Creative Commons") != std::string::npos, "DND SRD attribution metadata failed");
    ok &= require(attributions.size() == 2 && attributions[1].sourceVersion == "5.1",
                  "Legacy catalog content must retain its SRD 5.1 attribution");
    for (const auto &attribution : attributions)
        ok &= require(attribution.attributionText.find("Wizards of the Coast LLC") != std::string::npos &&
                          attribution.attributionText.find("https://creativecommons.org/licenses/by/4.0/legalcode") != std::string::npos &&
                          ruleset.validateContentAttribution(attribution, true).allowed,
                      "Bundled SRD content requires complete publisher and license attribution");
    const auto publicSpecies = ruleset.getRaces();
    ok &= require(std::find(publicSpecies.begin(), publicSpecies.end(), "Aasimar") == publicSpecies.end() &&
                      publicSpecies.size() == 9,
                  "Bundled SRD 5.2.1 species must exclude non-SRD defaults without renumbering saved data");
    const RuleContentGateResult srdGate = ruleset.validateContentAttribution(attributions.front(), true);
    ok &= require(srdGate.allowed, "DND SRD content gate should allow attributed SRD content");
    RuleContentAttribution blockedAttribution = attributions.front();
    blockedAttribution.sourceName = "Player's Handbook";
    blockedAttribution.sourceVersion = "2024";
    blockedAttribution.licenseName = "All Rights Reserved";
    blockedAttribution.licenseUrl.clear();
    blockedAttribution.attributionText.clear();
    const RuleContentGateResult blockedGate = ruleset.validateContentAttribution(blockedAttribution, true);
    ok &= require(!blockedGate.allowed && !blockedGate.errors.empty(), "DND content gate should block public non-SRD official content");
    ok &= require(ruleset.levelsExist(), "DND ruleset should support levels");
    ok &= require(attributes.size() == 6, "DND ruleset attribute list changed unexpectedly");
    ok &= require(!items.empty(), "DND ruleset should expose items");
    for (const ItemModel &item : items)
        if (item.category == "Armor")
            ok &= require(item.damageReduction == 0, "Standard DND armor must not grant flat damage reduction");
    ok &= require(!monsters.empty(), "DND ruleset should expose monsters");
    ok &= require(ruleset.getAbilityModifier(8) == -1, "DND ability modifier for 8 should be -1");
    ok &= require(ruleset.getAbilityModifier(18) == 4, "DND ability modifier for 18 should be +4");
    ok &= require(ruleset.getProficiencyBonus(1) == 2, "DND proficiency at level 1 should be +2");
    ok &= require(ruleset.getProficiencyBonus(17) == 6, "DND proficiency at level 17 should be +6");
    InventoryModel coinPurse;
    coinPurse.copper = 12;
    coinPurse.silver = 9;
    coinPurse.gold = 4;
    coinPurse.platinum = 1;
    ok &= require(ruleset.getInventoryCurrencyValue(coinPurse) == 1502, "DND currency base conversion failed");
    ruleset.normalizeInventoryCurrency(coinPurse);
    ok &= require(coinPurse.platinum == 1 && coinPurse.gold == 5 && coinPurse.silver == 0 && coinPurse.copper == 2, "DND currency normalization failed");
    ok &= require(ruleset.formatCurrencyValue(1502) == "1 pp 5 gp 2 cp", "DND currency formatting failed");

    std::map<std::string, int> attributesMap{{"Strength", 12}, {"Dexterity", 16}, {"Constitution", 14}, {"Wisdom", 14}, {"Intelligence", 10}, {"Charisma", 8}};
    ok &= require(ruleset.getPassiveScore(attributesMap, "Perception", 5, RuleProficiencyTier::Proficient) == 15, "DND passive perception calculation failed");
    ok &= require(ruleset.getSkillModifier(attributesMap, "Stealth", 5, RuleProficiencyTier::Expertise) == 9, "DND expertise skill calculation failed");

    InventoryModel inventory;
    const ItemModel *halfPlate = findItem("Half Plate Armor");
    const ItemModel *shield = findItem("Shield");
    ok &= require(halfPlate != nullptr && shield != nullptr, "DND armor test items missing");
    if (halfPlate && shield)
    {
        inventory.entries.push_back({halfPlate->metadata.id, 1, true, "Body"});
        inventory.entries.push_back({shield->metadata.id, 1, true, "Off Hand"});
        const RuleArmorSummary armor = ruleset.evaluateArmor(attributesMap, inventory, items);
        ok &= require(armor.armorClass == 19, "DND half plate plus shield armor calculation failed");
        ok &= require(armor.damageReduction == 0, "Standard armor and shield must not reduce incoming damage");
        CharacterModel armoredCharacter;
        armoredCharacter.selectedClass = "Fighter";
        armoredCharacter.attributes = attributesMap;
        armoredCharacter.inventory = inventory;
        const RuleArmorSummary characterArmor = CharacterRules::evaluateArmor(ruleset, armoredCharacter, items);
        ok &= require(characterArmor.armorClass == armor.armorClass && characterArmor.damageReduction == armor.damageReduction, "Character rules armor service should match ruleset armor evaluation");
        const RuleCarryingCapacitySummary carrying = ruleset.evaluateCarryingCapacity(attributesMap, inventory, items);
        ok &= require(carrying.capacityKnown && carrying.capacityWeight == 180.0, "DND carrying capacity calculation failed");
        const RuleEquipmentSlotValidation slotValidation = ruleset.validateEquipmentSlots(inventory, items);
        ok &= require(slotValidation.valid, "DND equipment slot validation should allow body armor plus off-hand shield");
    }

    if (const ItemModel *chainMail = findItem("Chain Mail"))
    {
        InventoryModel heavyArmor;
        heavyArmor.entries.push_back({chainMail->metadata.id, 1, true, "Body"});
        auto highDexterity = attributesMap;
        highDexterity["Dexterity"] = 30;
        const RuleArmorSummary armor = ruleset.evaluateArmor(highDexterity, heavyArmor, items);
        ok &= require(armor.armorClass == 16, "Worn heavy armor must replace the unarmored AC formula even with high Dexterity");
        auto customItems = items;
        for (ItemModel &item : customItems)
            if (item.metadata.id == chainMail->metadata.id) item.damageReduction = 2;
        const RuleArmorSummary customArmor = ruleset.evaluateArmor(highDexterity, heavyArmor, customItems);
        ok &= require(customArmor.damageReduction == 2, "Explicit custom armor damage reduction must remain supported");
    }

    RuleArmorInput barbarianArmorInput;
    barbarianArmorInput.attributes = attributesMap;
    barbarianArmorInput.className = "Barbarian";
    const RuleArmorSummary barbarianArmor = ruleset.evaluateArmor(barbarianArmorInput);
    ok &= require(barbarianArmor.armorClass == 15, "DND barbarian unarmored defense failed");

    RuleArmorInput monkArmorInput;
    monkArmorInput.attributes = attributesMap;
    monkArmorInput.className = "Monk";
    const RuleArmorSummary monkArmor = ruleset.evaluateArmor(monkArmorInput);
    ok &= require(monkArmor.armorClass == 15, "DND monk unarmored defense failed");

    RuleArmorInput naturalArmorInput;
    naturalArmorInput.attributes = attributesMap;
    naturalArmorInput.naturalArmorClass = 17;
    const RuleArmorSummary naturalArmor = ruleset.evaluateArmor(naturalArmorInput);
    ok &= require(naturalArmor.armorClass == 17, "DND natural armor calculation failed");

    InventoryModel shieldStackInventory;
    if (shield)
    {
        shieldStackInventory.entries.push_back({shield->metadata.id, 1, true, "Off Hand"});
        shieldStackInventory.entries.push_back({shield->metadata.id, 1, true, "Second Shield"});
        const RuleArmorSummary shieldStackArmor = ruleset.evaluateArmor(attributesMap, shieldStackInventory, items);
        ok &= require(shieldStackArmor.armorClass == 15, "DND shield stacking validation changed AC incorrectly");
        ok &= require(!shieldStackArmor.warnings.empty(), "DND multiple shields should produce a validation warning");
    }

    const RuleDiceSummary dice = ruleset.summarizeDiceExpression("2d6 + 3");
    ok &= require(dice.valid && dice.minimum == 5 && dice.maximum == 15 && dice.flatModifier == 3, "DND dice expression summary failed");
    const RuleDiceRollResult diceRoll = ruleset.evaluateDiceExpression("2d6 + 3 slashing", {4, 2});
    ok &= require(diceRoll.valid && diceRoll.total == 9, "DND supplied dice roll evaluation failed");

    RuleCheckRequest check;
    check.ability = "Dexterity";
    check.level = 5;
    check.proficiency = RuleProficiencyTier::Proficient;
    check.rollMode = RuleRollMode::Advantage;
    check.dc = 18;
    const RuleCheckRollResult checkRoll = ruleset.resolveCheckRoll(attributesMap, check, 7, 15);
    ok &= require(checkRoll.valid && checkRoll.chosenD20 == 15 && checkRoll.total == 21 && checkRoll.success, "DND advantage check resolution failed");

    const ItemModel *dagger = findItem("Dagger");
    const ItemModel *longsword = findItem("Longsword");
    const ItemModel *longbow = findItem("Longbow");
    ok &= require(dagger != nullptr && longsword != nullptr && longbow != nullptr, "DND weapon profile test items missing");
    if (dagger && longsword && longbow)
    {
        const RuleWeaponProfile daggerProfile = ruleset.getWeaponProfile(*dagger);
        const RuleWeaponProfile longswordProfile = ruleset.getWeaponProfile(*longsword);
        const RuleWeaponProfile longbowProfile = ruleset.getWeaponProfile(*longbow);
        ok &= require(daggerProfile.finesse && daggerProfile.thrown && daggerProfile.normalRangeFeet == 20 && daggerProfile.longRangeFeet == 60, "DND dagger profile failed");
        ok &= require(longswordProfile.versatile && longswordProfile.versatileDamage == "1d10 slashing", "DND longsword versatile profile failed");
        ok &= require(longbowProfile.ammunition && longbowProfile.twoHanded && longbowProfile.normalRangeFeet == 150 && longbowProfile.longRangeFeet == 600, "DND longbow profile failed");
        ok &= require(ruleset.getWeaponAbilityModifier(attributesMap, *dagger) == 3, "DND finesse weapon ability modifier failed");
    }

    ItemModel customWeapon;
    customWeapon.name = "Returning Star";
    customWeapon.category = "Weapon";
    customWeapon.itemType = "Weapon";
    customWeapon.weaponDamage = "1d6 piercing";
    customWeapon.weaponDamageType = "piercing";
    customWeapon.weaponAbility = "Dexterity";
    customWeapon.weaponReachFeet = 5;
    customWeapon.weaponRangeFeet = 30;
    customWeapon.weaponLongRangeFeet = 90;
    customWeapon.weaponAttackKind = "Thrown";
    customWeapon.weaponMastery = "Vex";
    customWeapon.ammunitionType = "Thrown";
    customWeapon.weaponProperties = {"Thrown", "Finesse"};
    const RuleWeaponProfile customWeaponProfile = ruleset.getWeaponProfile(customWeapon);
    ok &= require(customWeaponProfile.attackKind == RuleWeaponAttackKind::Thrown && customWeaponProfile.longRangeFeet == 90 && customWeaponProfile.mastery == "Vex", "DND custom weapon profile failed");

    const RuleConditionDefinition poisoned = ruleset.findConditionDefinition("Poisoned");
    ok &= require(poisoned.valid && !poisoned.mechanicalEffects.empty() && !poisoned.endCondition.empty(), "DND condition definition lookup failed");
    RuleActiveCondition activeCondition = ruleset.createActiveCondition("poisoned", "npc_viper", "Viper", 2);
    ok &= require(activeCondition.name == "Poisoned" && activeCondition.sourceName == "Viper" && activeCondition.remainingRounds == 2, "DND active condition creation failed");
    activeCondition = ruleset.advanceConditionRound(activeCondition);
    activeCondition = ruleset.advanceConditionRound(activeCondition);
    ok &= require(!activeCondition.active && activeCondition.remainingRounds == 0, "DND active condition duration ticking failed");

    RuleActionEconomyState actionState = ruleset.startTurnActionEconomy(30);
    RuleActionSpendResult actionSpend = ruleset.spendActionResource(actionState, RuleActionResource::Action);
    ok &= require(actionSpend.success && !actionSpend.state.actionAvailable, "DND action economy action spend failed");
    actionSpend = ruleset.spendActionResource(actionSpend.state, RuleActionResource::Action);
    ok &= require(!actionSpend.success, "DND action economy should reject spending an action twice");
    RuleActionSpendResult movementSpend = ruleset.spendActionResource(actionState, RuleActionResource::Movement, 25);
    ok &= require(movementSpend.success && movementSpend.state.movementUsedFeet == 25, "DND action economy movement spend failed");
    movementSpend = ruleset.spendActionResource(movementSpend.state, RuleActionResource::Movement, 10);
    ok &= require(!movementSpend.success, "DND action economy should reject movement beyond speed");
    RuleActionSpendResult concentrationSpend = ruleset.spendActionResource(actionState, RuleActionResource::Concentration, 0, "Bless");
    ok &= require(concentrationSpend.success && concentrationSpend.state.concentrating && concentrationSpend.state.concentrationSource == "Bless", "DND action economy concentration start failed");
    const RuleConcentrationCheck concentrationCheck = ruleset.createConcentrationCheck(27, 5, RuleProficiencyTier::Proficient);
    ok &= require(concentrationCheck.dc == 13 && concentrationCheck.savingThrow.ability == "Constitution", "DND concentration DC calculation failed");
    const RuleConcentrationResult concentrationResult = ruleset.resolveConcentrationCheck(attributesMap, concentrationCheck, 8);
    ok &= require(concentrationResult.roll.valid && concentrationResult.concentrationMaintained, "DND concentration save resolution failed");

    CharacterSpellcastingModel spellcasting;
    spellcasting.enabled = true;
    spellcasting.ability = "Wisdom";
    SpellModel spell;
    spell.metadata.id = "spell_test_flame";
    spell.name = "Test Flame";
    spell.level = 3;
    spell.resolution = SpellResolutionType::SavingThrow;
    spell.savingThrowAbility = "Dexterity";
    spell.damage = "8d6 fire";
    spell.area = "20-foot sphere";
    spell.duration = "1 minute";
    spell.concentration = true;
    spell.scaling = "+1d6 fire per higher slot";
    const RuleSpellResolutionSummary spellSummary = ruleset.summarizeSpellResolution(spell, spellcasting, attributesMap, 5, 4);
    ok &= require(spellSummary.saveDc == 13 && spellSummary.area.shape == "sphere" && spellSummary.area.sizeFeet == 20 && spellSummary.ongoingEffect, "DND spell resolution summary failed");

    RuleRestRequest longRest;
    longRest.type = RuleRestType::LongRest;
    longRest.currentHitPoints = 3;
    longRest.maxHitPoints = 20;
    longRest.spellcasting.enabled = true;
    longRest.spellcasting.slotMaximums[1] = 2;
    longRest.spellcasting.slotUsed[1] = 2;
    const RuleRestResult longRestResult = ruleset.applyRest(longRest);
    ok &= require(longRestResult.currentHitPoints == 20 && longRestResult.spellcasting.slotUsed[1] == 0, "DND long rest recovery failed");
    RuleRestRequest shortRest;
    shortRest.type = RuleRestType::ShortRest;
    shortRest.currentHitPoints = 3;
    shortRest.maxHitPoints = 20;
    const RuleRestResult shortRestResult = ruleset.applyRest(shortRest);
    ok &= require(shortRestResult.currentHitPoints == 3 && !shortRestResult.notes.empty(), "DND short rest summary failed");

    RuleDamageApplication damage;
    damage.currentHitPoints = 12;
    damage.maxHitPoints = 20;
    damage.temporaryHitPoints = 5;
    damage.amount = 14;
    damage.damageType = "fire";
    damage.resistances = {"fire"};
    const RuleDamageResult damageResult = ruleset.applyDamageOrHealing(damage);
    ok &= require(damageResult.effectiveAmount == 7 && damageResult.temporaryHitPoints == 0 && damageResult.currentHitPoints == 10, "DND resistance/temp HP damage pipeline failed");
    RuleDamageApplication healing;
    healing.healing = true;
    healing.currentHitPoints = 3;
    healing.maxHitPoints = 20;
    healing.amount = 8;
    ok &= require(ruleset.applyDamageOrHealing(healing).currentHitPoints == 11, "DND healing pipeline failed");
    RuleDeathSaveState deathState;
    RuleDeathSaveResult deathSave = ruleset.resolveDeathSave(deathState, 1);
    ok &= require(deathSave.valid && deathSave.state.failures == 2, "DND natural-one death save failed");
    deathSave = ruleset.resolveDeathSave(deathSave.state, 9);
    ok &= require(deathSave.state.dead, "DND death save death threshold failed");
    ok &= require(ruleset.getExperienceForChallengeRating("2") == 450, "DND challenge rating XP lookup failed");
    const RuleEncounterDifficultySummary difficulty = ruleset.summarizeEncounterDifficulty({3, 3, 3, 3}, {"1/2", "1/2", "1/2"});
    ok &= require(difficulty.adjustedExperience == 300 && difficulty.difficulty == "Easy", "DND encounter difficulty summary failed");
    const RuleRewardSummary rewards = ruleset.summarizeRewards(300, true, "Cleared the ruins");
    ok &= require(rewards.experience == 300 && rewards.milestoneAvailable && !rewards.notes.empty(), "DND reward summary failed");
    RuleLootGenerationRequest lootRequest;
    lootRequest.label = "Ruined watch post";
    lootRequest.sourceId = "encounter-ruins";
    lootRequest.sourceChallengeRatings = {"1/2", "1/2"};
    lootRequest.items.push_back({"Silvered key", 1, 250, 0.0, "Opens the west tower"});
    const RuleTreasureParcel treasure = ruleset.generateTreasureParcel(lootRequest);
    ok &= require(treasure.label == "Ruined watch post" && treasure.sourceId == "encounter-ruins", "DND treasure parcel tracking failed");
    ok &= require(treasure.totalCoinValueCopper() == 2000 && treasure.totalValueCopper() == 2250, "DND treasure parcel value failed");
    ok &= require(!treasure.notes.empty() && treasure.notes.front().find("GM-authored") != std::string::npos, "DND treasure parcel safety note failed");

    ok &= require(!ruleset.getConditionDefinitions().empty(), "DND ruleset should expose condition definitions");
    ok &= require(!ruleset.getCommonDifficultyClasses().empty(), "DND ruleset should expose common DCs");
    return ok;
}

bool testRulesetLayering()
{
    RulesetDefinitionModel baseDefinition;
    baseDefinition.metadata.id = "dnd-srd-base";
    baseDefinition.name = "D&D SRD Base";
    baseDefinition.version = "5.2.1";
    baseDefinition.abilityScores.push_back({"strength", "Strength", "STR", "Raw physical power"});
    baseDefinition.skills.push_back({"athletics", "Athletics", "strength", "Climb, jump, and swim"});
    baseDefinition.classes.push_back({"fighter", "Fighter", "d10", {"strength", "dexterity"}, "Martial combatant"});

    RulesetDefinitionModel campaignOverrides;
    campaignOverrides.metadata.id = "campaign-layer";
    campaignOverrides.name = "Campaign Overrides";
    campaignOverrides.version = "1.0.0";
    campaignOverrides.baseRulesetId = "dnd-srd-base";
    campaignOverrides.skills.push_back({"airship", "Airship Handling", "dexterity", "Pilot sky vessels"});
    campaignOverrides.classes.push_back({"fighter", "Fighter", "d10", {"strength"}, "Martial combatant with table-specific maneuvers"});

    const RulesetDefinitionModel layered = createLayeredRulesetDefinition(baseDefinition, campaignOverrides);

    bool ok = true;
    ok &= require(layered.metadata.id == "campaign-layer", "Layered ruleset should keep campaign layer id");
    ok &= require(layered.baseRulesetId == "dnd-srd-base", "Layered ruleset should retain base ruleset id");
    ok &= require(layered.abilityScores.size() == 1 && layered.abilityScores.front().id == "strength", "Layered ruleset lost base ability scores");
    ok &= require(layered.skills.size() == 2, "Layered ruleset did not append campaign skills");
    ok &= require(layered.classes.size() == 1 && layered.classes.front().description.find("table-specific") != std::string::npos, "Layered ruleset did not override matching class ids");
    return ok;
}

bool testRulesetFormulaEvaluator()
{
    const std::map<std::string, double> variables{
        {"level", 5},
        {"proficiency", 3},
        {"dex.mod", 4},
        {"con.mod", 2},
        {"spell.mod", 4},
        {"shield", 2},
        {"hit_die_average", 6},
        {"speed.base", 30}};

    bool ok = true;
    ok &= require(evaluateRulesetFormula("10 + dex.mod + shield", variables).value == 16, "Formula AC calculation failed");
    ok &= require(evaluateRulesetFormula("level * (hit_die_average + con.mod)", variables).value == 40, "Formula HP calculation failed");
    ok &= require(evaluateRulesetFormula("8 + proficiency + spell.mod", variables).value == 15, "Formula DC calculation failed");
    ok &= require(evaluateRulesetFormula("proficiency + dex.mod", variables).value == 7, "Formula attack bonus calculation failed");
    ok &= require(evaluateRulesetFormula("15 * (10 + con.mod)", variables).value == 180, "Formula carrying capacity calculation failed");
    ok &= require(evaluateRulesetFormula("speed.base / 2", variables).value == 15, "Formula movement calculation failed");

    const RulesetFormulaEvaluationResult missingVariable = evaluateRulesetFormula("10 + wisdom.mod", variables);
    ok &= require(!missingVariable.valid && !missingVariable.errors.empty(), "Formula evaluator should reject missing variables");
    const RulesetFormulaEvaluationResult unsafeToken = evaluateRulesetFormula("system('run')", variables);
    ok &= require(!unsafeToken.valid && !unsafeToken.errors.empty(), "Formula evaluator should reject unsupported tokens");
    const RulesetFormulaEvaluationResult divideByZero = evaluateRulesetFormula("10 / (level - 5)", variables);
    ok &= require(!divideByZero.valid && !divideByZero.errors.empty(), "Formula evaluator should reject division by zero");

    RulesetDefinitionModel previewDefinition;
    previewDefinition.abilityScores.push_back({"dexterity", "Dexterity", "DEX", ""});
    previewDefinition.derivedStats.push_back({"initiative", "Initiative", "dexterity.mod + proficiency", ""});
    const std::map<std::string, double> previewVariables = buildRulesetFormulaPreviewVariables(previewDefinition);
    ok &= require(previewVariables.find("dexterity.mod") != previewVariables.end(), "Formula preview variables should include ability modifiers");
    ok &= require(previewVariables.find("initiative") != previewVariables.end() && previewVariables.at("initiative") == 3.0, "Formula preview variables should evaluate derived stats");
    const RulesetFormulaEvaluationResult preview = previewRulesetFormula(previewDefinition, "10 + initiative");
    ok &= require(preview.valid && preview.value == 13.0, "Formula preview should evaluate with derived-stat variables");
    return ok;
}

bool testRulesetValidationDiagnostics()
{
    RulesetDefinitionModel invalidDefinition;
    invalidDefinition.abilityScores.push_back({"might", "Might", "MIG", ""});
    invalidDefinition.abilityScores.push_back({"might", "Might Again", "MIG", ""});
    invalidDefinition.skills.push_back({"sneak", "Sneak", "agility", ""});
    invalidDefinition.sizes.push_back({"medium", "Medium", 5, ""});
    invalidDefinition.speeds.push_back({"walk", "Walk", "feet", ""});
    invalidDefinition.ancestries.push_back({"skyborn", "Skyborn", "giant", "fly", ""});
    invalidDefinition.derivedStats.push_back({"armor", "Armor", "guard +", ""});
    invalidDefinition.derivedStats.push_back({"guard", "Guard", "armor + 1", ""});
    invalidDefinition.diceFormulas.push_back({"bad_damage", "Bad Damage", "2dd6 + might.mod", ""});

    const std::vector<RulesetValidationDiagnostic> diagnostics = validateRulesetDefinition(invalidDefinition);
    const auto hasCode = [&](const std::string &code) {
        for (const RulesetValidationDiagnostic &diagnostic : diagnostics)
        {
            if (diagnostic.code == code)
                return true;
        }
        return false;
    };

    bool ok = true;
    ok &= require(hasCode("duplicate_id"), "Ruleset validation should report duplicate IDs");
    ok &= require(hasCode("missing_reference"), "Ruleset validation should report missing references");
    ok &= require(hasCode("broken_formula"), "Ruleset validation should report broken formulas");
    ok &= require(hasCode("circular_dependency"), "Ruleset validation should report circular formula dependencies");
    ok &= require(hasCode("invalid_dice"), "Ruleset validation should report invalid dice strings");

    RulesetDefinitionModel validDefinition;
    validDefinition.abilityScores.push_back({"might", "Might", "MIG", ""});
    validDefinition.skills.push_back({"athletics", "Athletics", "might", ""});
    validDefinition.sizes.push_back({"medium", "Medium", 5, ""});
    validDefinition.speeds.push_back({"walk", "Walk", "feet", ""});
    validDefinition.ancestries.push_back({"human", "Human", "medium", "walk", ""});
    validDefinition.derivedStats.push_back({"armor", "Armor", "10 + might.mod", ""});
    validDefinition.diceFormulas.push_back({"strike", "Strike", "1d20 + might.mod", ""});
    ok &= require(validateRulesetDefinition(validDefinition).empty(), "Valid ruleset definition should not produce diagnostics");
    return ok;
}

bool testRulesetContentPackJson()
{
    RulesetContentPackModel pack;
    pack.metadata.id = "pack_export_test";
    pack.rulesetId = "ruleset_export_test";
    pack.name = "Export Test Pack";
    pack.version = "1.2.3";
    pack.author = "Test Author";
    pack.license = "CC-BY-4.0";
    pack.importedAtUtc = "2026-05-22T12:00:00Z";
    pack.source = "unit-test";
    pack.enabled = false;
    pack.metadata.tags = {"homebrew", "json"};
    pack.definitionJson = R"({"entries":[{"id":"spark","name":"Spark"}]})";

    const std::string json = exportRulesetContentPackToJson(pack);
    const RulesetContentPackImportResult imported = importRulesetContentPackFromJson(json);

    bool ok = true;
    ok &= require(json.find("ttrpg-editor.homebrew-pack.v1") != std::string::npos, "Content pack export format marker missing");
    ok &= require(imported.success, "Content pack JSON import failed");
    ok &= require(imported.pack.metadata.id == pack.metadata.id, "Content pack JSON id did not round trip");
    ok &= require(imported.pack.rulesetId == pack.rulesetId, "Content pack JSON ruleset id did not round trip");
    ok &= require(imported.pack.author == pack.author, "Content pack JSON author did not round trip");
    ok &= require(imported.pack.license == pack.license, "Content pack JSON license did not round trip");
    ok &= require(imported.pack.importedAtUtc == pack.importedAtUtc, "Content pack JSON import date did not round trip");
    ok &= require(imported.pack.source == pack.source, "Content pack JSON source did not round trip");
    ok &= require(!imported.pack.enabled, "Content pack JSON enabled flag did not round trip");
    ok &= require(imported.pack.metadata.tags.size() == 2, "Content pack JSON tags did not round trip");
    ok &= require(imported.pack.definitionJson.find("spark") != std::string::npos, "Content pack JSON definition did not round trip");

    const RulesetContentPackImportResult invalid = importRulesetContentPackFromJson(R"({"format":"unknown"})");
    ok &= require(!invalid.success && !invalid.error.empty(), "Content pack JSON import should reject unsupported formats");
    return ok;
}

bool testRulesetMigrationPlan()
{
    RulesetContentPackModel currentPack;
    currentPack.metadata.id = "pack_sky";
    currentPack.name = "Sky Isles";
    currentPack.enabled = true;

    RulesetContentPackModel disabledPack = currentPack;
    disabledPack.enabled = false;

    RulesetContentPackModel newPack;
    newPack.metadata.id = "pack_depths";
    newPack.name = "Depths";
    newPack.enabled = true;

    const RulesetMigrationPlan plan = createRulesetMigrationPlan("1.0.0", "1.1.0", {currentPack}, {disabledPack, newPack});
    const auto hasStep = [&](const std::string &code) {
        for (const RulesetMigrationStep &step : plan.steps)
        {
            if (step.code == code)
                return true;
        }
        return false;
    };

    bool ok = true;
    ok &= require(plan.requiresReview, "Ruleset migration plan should require review for version and pack changes");
    ok &= require(hasStep("ruleset_version_changed"), "Ruleset migration plan should report version changes");
    ok &= require(hasStep("content_pack_disabled"), "Ruleset migration plan should report disabled packs");
    ok &= require(hasStep("content_pack_enabled"), "Ruleset migration plan should report enabled packs");
    ok &= require(!createRulesetMigrationPlan("1.0.0", "1.0.0", {currentPack}, {currentPack}).requiresReview, "Unchanged ruleset migration plan should not require review");
    return ok;
}

bool testDefinitionBackedRulesetRuntime()
{
    RulesetDefinitionModel base;
    base.rulesetId = "test-system";
    base.name = "Test System";
    base.version = "1.0.0";
    base.published = true;
    base.metadata.id = "test-system@1.0.0";
    base.abilityScores = {
        {"strength", "Strength", "STR", ""},
        {"dexterity", "Dexterity", "DEX", ""}};
    base.derivedStats = {{"armor_class", "Armor Class", "10 + dexterity.mod", ""}};
    base.resources = {{"focus", "Focus", "level + proficiency", "proficiency", "Short Rest", ""}};
    base.conditions = {{"stunned", "Stunned", "Cannot act.", {}, "End of turn"}};

    DefinitionBackedRuleSet runtime(base);
    RulesEvaluationContext context;
    context.level = 5;
    context.values = {{"strength", 16}, {"dexterity", 14}};
    const RulesEvaluationSnapshot snapshot = runtime.evaluate(context);

    bool ok = true;
    ok &= require(snapshot.valid, "Definition-backed ruleset should evaluate a valid definition");
    ok &= require(snapshot.abilityModifiers.at("strength") == 3.0, "Stable-ID ability modifier evaluation failed");
    ok &= require(snapshot.proficiency == 3, "Definition-backed proficiency progression failed");
    ok &= require(snapshot.derivedStats.at("armor_class") == 12.0, "Derived stat dependency evaluation failed");
    ok &= require(snapshot.resourceMaximums.at("focus") == 8.0, "Definition-backed resource maximum failed");
    const RuleDiceRollResult dice = runtime.evaluateDiceExpression("2d6+3", {4, 5});
    ok &= require(dice.valid && dice.total == 12, "Definition-backed dice primitive evaluation failed");

    RuleCheckRequest check;
    check.ability = "strength";
    check.level = 5;
    check.proficiency = RuleProficiencyTier::Proficient;
    ok &= require(runtime.evaluateCheck(context.values, check).modifier == 6, "Definition-backed check evaluation failed");

    RuleRestRequest rest;
    rest.type = RuleRestType::ShortRest;
    const RuleRestResult rested = runtime.applyRest(rest);
    ok &= require(std::find(rested.recoveredResources.begin(), rested.recoveredResources.end(), "Focus") != rested.recoveredResources.end(),
                  "Resource recovery trigger evaluation failed");

    RulesetDefinitionModel overlay;
    overlay.rulesetId = "test-overlay";
    overlay.version = "1";
    overlay.derivedStats = {{"armor_class", "Guard", "12 + dexterity.mod", ""}};
    DefinitionBackedRuleSet layered(base, overlay);
    ok &= require(layered.evaluate(context).derivedStats.at("armor_class") == 14.0,
                  "Campaign overlay should take precedence over the published base definition");

    RulesetDefinitionModel draft = base;
    draft.published = false;
    draft.metadata.id = "draft-row";
    const RulesetPublishResult published = publishRulesetVersion(draft, "2026-08-23T10:00:00Z");
    ok &= require(published.success && published.version.published && published.version.metadata.id == "test-system@1.0.0",
                  "Ruleset publishing should create an immutable version identity");
    ok &= require(!draft.published && draft.metadata.id == "draft-row", "Publishing should not mutate the editable draft");
    CampaignRepository::saveInstalledRulesetDefinition(published.version);
    RulesetDefinitionModel changedPublishedVersion = published.version;
    changedPublishedVersion.derivedStats.front().formula = "11 + dexterity.mod";
    bool immutableRejected = false;
    try
    {
        CampaignRepository::saveInstalledRulesetDefinition(changedPublishedVersion);
    }
    catch (const std::exception &)
    {
        immutableRejected = true;
    }
    ok &= require(immutableRejected, "Published ruleset versions must reject in-place changes");

    RulesetDefinitionModel nextVersion = base;
    nextVersion.version = "1.1.0";
    nextVersion.metadata.id = "test-system@1.1.0";
    nextVersion.derivedStats.front().formula = "11 + dexterity.mod";
    nextVersion.resources.front().recoveryTrigger = "Long Rest";
    nextVersion.skills.push_back({"athletics", "Athletics", "strength", ""});
    nextVersion.currencies.push_back({"coin", "Coin", "CN", 1, ""});
    const RulesetUpgradePreview detailedPreview = createRulesetUpgradePreview(base, nextVersion);
    ok &= require(detailedPreview.structuredChanges.size() == detailedPreview.changes.size(),
                  "Ruleset upgrade preview should pair every legacy change with structured localization data");
    ok &= require(std::any_of(detailedPreview.changes.begin(), detailedPreview.changes.end(), [](const std::string &change) {
        return change.find("Changed derived stat: armor_class") != std::string::npos;
    }), "Ruleset upgrade preview should report changed formulas for stable IDs");
    ok &= require(std::any_of(detailedPreview.structuredChanges.begin(), detailedPreview.structuredChanges.end(), [](const RulesetUpgradeChange &change) {
        return change.kind == RulesetUpgradeChangeKind::Changed &&
               change.definitionKind == RulesetDefinitionKind::DerivedStat &&
               change.stableId == "armor_class";
    }), "Ruleset upgrade preview should expose structured Changed data for localization");
    ok &= require(std::find(detailedPreview.affectedStableIds.begin(), detailedPreview.affectedStableIds.end(), "focus") != detailedPreview.affectedStableIds.end(),
                  "Ruleset upgrade preview should report changed recovery definitions as affected stable IDs");
    ok &= require(std::find(detailedPreview.affectedStableIds.begin(), detailedPreview.affectedStableIds.end(), "athletics") != detailedPreview.affectedStableIds.end() &&
                      std::find(detailedPreview.affectedStableIds.begin(), detailedPreview.affectedStableIds.end(), "coin") != detailedPreview.affectedStableIds.end(),
                  "Ruleset upgrade preview should include added checks and currencies");
    const RulesetUpgradePreview overlayPreview = createRulesetUpgradePreview(base, nextVersion, &overlay);
    ok &= require(overlayPreview.overlayApplied && overlayPreview.overlayId == "test-overlay",
                  "Ruleset upgrade preview should declare the preserved campaign overlay");
    ok &= require(std::none_of(overlayPreview.changes.begin(), overlayPreview.changes.end(), [](const std::string &change) {
        return change.find("Changed derived stat: armor_class") != std::string::npos;
    }), "An overlay that preserves a derived-stat override should mask the underlying base formula change in the effective diff");

    Campaign campaign;
    campaign.ruleSet = std::make_shared<DND5e>();
    CharacterModel character;
    character.attributes["Strength"] = 16;
    campaign.characters.push_back(character);

    RulesetDefinitionModel invalid = base;
    invalid.version = "2.0.0";
    invalid.derivedStats.front().formula = "unknown_symbol + 1";
    std::vector<std::string> errors;
    const RulesetReference originalReference = campaign.rulesetReference;
    ok &= require(!migrateCampaignToRuleset(campaign, invalid, nullptr, errors), "Invalid ruleset migration should fail");
    ok &= require(campaign.rulesetReference.rulesetId == originalReference.rulesetId && campaign.characters.front().ruleValues.empty(),
                  "Failed migration should roll back campaign state");

    errors.clear();
    ok &= require(migrateCampaignToRuleset(campaign, base, nullptr, errors), "Published ruleset migration should succeed");
    ok &= require(campaign.rulesetReference.rulesetId == "test-system" && campaign.rulesetReference.version == "1.0.0",
                  "Campaign should pin the selected published version");
    ok &= require(campaign.characters.front().ruleValues.at("strength") == 16,
                  "Legacy display-name values should migrate to stable definition IDs");

    campaign.name = "definition-backed-reload";
    CampaignRepository::saveCampaign(campaign);
    Campaign &reloaded = CampaignRepository::loadCampaign(campaign.name);
    const auto *reloadedRules = dynamic_cast<const DefinitionBackedRuleSet *>(reloaded.ruleSet.get());
    ok &= require(reloadedRules != nullptr,
                  "A campaign pinned to a published definition must restore its definition-backed runtime");
    ok &= require(reloaded.characters.size() == 1 && reloaded.characters.front().ruleValues.at("strength") == 16,
                  "Definition-backed campaign content and stable-ID values must reload");
    return ok;
}

bool testPersistenceRoundTrip()
{
    Campaign campaign = makeCampaign("roundtrip");
    campaign.characters.front().maxHitPoints = 24;
    campaign.characters.front().currentHitPoints = 0;
    campaign.contentLanguage = "de";
    campaign.rulesetReference = {"dnd5e", "dnd5e@compiled", "campaign-overlay", "3"};
    campaign.characters.front().ruleValues["strength"] = 17;
    campaign.maps.front().atlasLevel = MapLevel::Region;
    campaign.maps.front().mapKind = MapKind::Geographic;
    campaign.maps.front().atlasStatus = MapAtlasStatus::Linked;
    campaign.maps.front().visibility = MapVisibility::PlayersKnown;
    campaign.maps.front().playerRelease = {true, false, true, false, true};
    campaign.maps.front().revision = 7;
    campaign.maps.front().parentMapId = "test_parent_map_roundtrip";
    campaign.maps.front().atlasPlacement.parentId = "test_parent_map_roundtrip";
    campaign.maps.front().atlasPlacement.parentRect = {0.15f, 0.2f, 0.45f, 0.35f};
    campaign.maps.front().atlasPlacement.parentRevision = 5;
    campaign.maps.front().atlasPlacement.sourceRevision = 7;
    campaign.maps.front().creationIntent = MapCreationIntent::RegionExploration;
    campaign.maps.front().sizePreset = MapSizePreset::Large;
    campaign.maps.front().spatialReference = {true, 180.0f, 120.0f, MapDistanceUnit::Kilometers, 4.0f, 7.0f, 0.0f};
    campaign.maps.front().importedReference.enabled = true;
    campaign.maps.front().importedReference.assetId = "test_managed_map_reference";
    campaign.maps.front().importedReference.projectRelativePath = "Resources/Campaigns/test/Assets/Maps/reference.png";
    campaign.maps.front().importedReference.contentHash = "fnv1a64:0123456789abcdef";
    campaign.maps.front().importedReference.sourceWidth = 2048;
    campaign.maps.front().importedReference.sourceHeight = 1024;
    campaign.maps.front().importedReference.workingProxyAssetId = "test_managed_map_proxy";
    campaign.maps.front().importedReference.workingProxyProjectRelativePath =
        "Resources/Campaigns/test/Assets/Maps/reference-working.png";
    campaign.maps.front().importedReference.workingProxyContentHash = "fnv1a64:fedcba9876543210";
    campaign.maps.front().importedReference.workingProxyWidth = 1024;
    campaign.maps.front().importedReference.workingProxyHeight = 512;
    campaign.maps.front().importedReference.exifOrientation = 8;
    campaign.maps.front().importedReference.workingProxyGenerated = true;
    campaign.maps.front().importedReference.crop = {0.1f, 0.2f, 0.7f, 0.6f};
    FactionModel atlasFaction;
    atlasFaction.metadata.id = "test_atlas_faction_roundtrip";
    atlasFaction.name = "Cartographers Guild";
    campaign.factions.push_back(atlasFaction);
    WorldPlaceModel capital;
    capital.metadata.id = "test_world_place_roundtrip";
    capital.type = WorldPlaceType::Capital;
    capital.name = "Aurora Gate";
    capital.description = "Capital above the northern river.";
    capital.notes = "Seat of the winter council.";
    capital.nationId = "test_nation_roundtrip";
    capital.linkedMapId = campaign.maps.front().metadata.id;
    capital.linkedFeatureId = campaign.maps.front().features.front().metadata.id;
    capital.hasMapPosition = true;
    capital.mapPosition = {0.32, 0.68};
    capital.visibility = WorldObjectVisibility::Public;
    campaign.worldPlaces.push_back(capital);
    NationModel nation;
    nation.metadata.id = "test_nation_roundtrip";
    nation.name = "Northern Compact";
    nation.description = "A federation of river provinces.";
    nation.government = "Council federation";
    nation.culture = "River and observatory traditions";
    nation.capitalPlaceId = capital.metadata.id;
    nation.relationships = "Trade accord with the southern league.";
    nation.factionIds.push_back(atlasFaction.metadata.id);
    nation.visibility = WorldObjectVisibility::Discovered;
    NationTerritoryModel territory;
    territory.metadata.id = "test_territory_roundtrip";
    territory.name = "Compact Heartland";
    territory.polygon = {{-0.25, 0.1}, {0.8, 0.15}, {1.3, 0.9}, {0.2, 0.85}};
    nation.territories.push_back(territory);
    campaign.nations.push_back(nation);
    EncounterModel encounter;
    encounter.metadata.id = "test_encounter_roundtrip";
    encounter.name = "Roundtrip Skirmish";
    encounter.combatPhase = CombatPhase::ActionResolution;
    encounter.round = 2;
    encounter.sourceMapId = campaign.maps.front().metadata.id;
    encounter.sourceMapRect = {0.2f, 0.25f, 0.4f, 0.5f};
    encounter.sourceMapRevision = campaign.maps.front().revision;
    encounter.tacticalMapId = campaign.maps.front().metadata.id;
    EncounterParticipantModel participant;
    participant.displayName = "Test Hero";
    participant.sourceEntityId = campaign.characters.front().metadata.id;
    participant.kind = CombatantKind::Player;
    participant.narrativePosition = "Front line";
    participant.narrativeRangeBand = "Engaged";
    participant.narrativeSpotlight = true;
    participant.temporaryHitPoints = 4;
    encounter.participants.push_back(participant);
    campaign.encounters.push_back(encounter);
    CampaignRepository::saveCampaign(campaign);
    Campaign &loaded = CampaignRepository::loadCampaign(campaign.name);

    bool ok = true;
    ok &= require(!loaded.encounters.empty() && !loaded.encounters.front().participants.empty() &&
                      loaded.encounters.front().participants.front().temporaryHitPoints == 4,
                  "Temporary HP must survive the real campaign save/reopen path");
    ok &= require(loaded.name == campaign.name, "Campaign name did not round trip");
    ok &= require(loaded.contentLanguage == "de", "Campaign content language did not round trip independently of UI language");
    ok &= require(loaded.presentationModeActive, "Campaign presentation mode did not round trip");
    ok &= require(loaded.audioVolumeGroups.master == 0.8f && loaded.audioVolumeGroups.ambience == 0.6f && loaded.audioVolumeGroups.voice == 0.5f, "Audio volume groups did not round trip");
    const InWorldDateTime loadedClockTime = InWorldTime::currentDateTime(loaded.clock);
    ok &= require(loaded.clock.calendar.name == "Test Lunar Calendar", "Campaign calendar name did not round trip");
    ok &= require(loaded.clock.calendar.months.size() == 3 && loaded.clock.calendar.months[1].name == "Bloomwane" && loaded.clock.calendar.months[1].days == 32, "Campaign calendar months did not round trip");
    ok &= require(loaded.clock.calendar.weekdays.size() == 4 && loaded.clock.calendar.weekdays.front() == "Firstday", "Campaign calendar weekdays did not round trip");
    ok &= require(loaded.clock.calendar.hoursPerDay == 20 && loaded.clock.calendar.daylightHours == 11 && loaded.clock.calendar.nightHours == 7 && loaded.clock.calendar.dayStartsAtHour == 5, "Campaign calendar day shape did not round trip");
    ok &= require(!loaded.clock.useSystemTime && !loaded.clock.running && loaded.clock.multiplier == 3.5, "Campaign clock mode did not round trip");
    ok &= require(loadedClockTime.year == 77 && loadedClockTime.month == 2 && loadedClockTime.day == 12 && loadedClockTime.hour == 9 && loadedClockTime.minute == 45, "Campaign clock time did not round trip");
    ok &= require(loaded.characters.size() == 1, "Character did not round trip");
    ok &= require(loaded.characters.front().maxHitPoints == 24 && loaded.characters.front().currentHitPoints == 0 &&
                      loaded.characters.front().temporaryHitPoints == 6,
                  "Character HP including temporary HP must survive the real campaign save/reopen path");
    ok &= require(loaded.rulesetReference.rulesetId == "dnd5e" && loaded.rulesetReference.overlayId == "campaign-overlay",
                  "Campaign ruleset reference did not round trip");
    ok &= require(loaded.characters.front().ruleValues["strength"] == 17,
                  "Stable-ID character rules values did not round trip");
    ok &= require(loaded.customItems.size() == 1, "Custom item did not round trip");
    ok &= require(loaded.nations.size() == 1 && loaded.nations.front().capitalPlaceId == "test_world_place_roundtrip",
                  "Atlas nation identity and capital did not round trip");
    ok &= require(loaded.nations.front().government == "Council federation" && loaded.nations.front().visibility == WorldObjectVisibility::Discovered,
                  "Atlas nation details and visibility did not round trip");
    ok &= require(loaded.nations.front().factionIds.size() == 1 && loaded.nations.front().factionIds.front() == "test_atlas_faction_roundtrip",
                  "Atlas nation faction links did not round trip independently of factions");
    ok &= require(loaded.nations.front().territories.size() == 1 && loaded.nations.front().territories.front().metadata.id == "test_territory_roundtrip",
                  "Atlas territory stable identity did not round trip");
    if (!loaded.nations.front().territories.empty())
    {
        const auto &polygon = loaded.nations.front().territories.front().polygon;
        ok &= require(polygon.size() == 4 && polygon.front().x == 0.0 && polygon[2].x == 1.0,
                      "Atlas territory polygon coordinates should normalize into the zero-to-one range");
    }
    ok &= require(loaded.worldPlaces.size() == 1 && loaded.worldPlaces.front().type == WorldPlaceType::Capital,
                  "Atlas world place type did not round trip");
    ok &= require(loaded.worldPlaces.front().nationId == "test_nation_roundtrip" &&
                      loaded.worldPlaces.front().linkedMapId == "test_map_roundtrip" &&
                      loaded.worldPlaces.front().linkedFeatureId == "test_feature_roundtrip" &&
                      loaded.worldPlaces.front().visibility == WorldObjectVisibility::Public,
                  "Atlas world place links and visibility did not round trip");
    ok &= require(loaded.worldPlaces.front().hasMapPosition &&
                      loaded.worldPlaces.front().mapPosition.x == 0.32 &&
                      loaded.worldPlaces.front().mapPosition.y == 0.68,
                  "Atlas world place generation anchor did not round trip");
    ok &= require(loaded.maps.size() == 1, "Map did not round trip");
    ok &= require(loaded.maps.front().atlasLevel == MapLevel::Region &&
                      loaded.maps.front().mapKind == MapKind::Geographic &&
                      loaded.maps.front().atlasStatus == MapAtlasStatus::Linked &&
                      loaded.maps.front().visibility == MapVisibility::PlayersKnown &&
                      loaded.maps.front().revision == 7,
                  "Atlas classification and visibility did not round trip");
    ok &= require(loaded.maps.front().playerRelease.politicalBorders &&
                      !loaded.maps.front().playerRelease.waysAndRoutes &&
                      loaded.maps.front().playerRelease.placesAndMarkers &&
                      !loaded.maps.front().playerRelease.labels &&
                      loaded.maps.front().playerRelease.tokensAndAnnotations,
                  "Player map release categories did not round trip fail-closed persistence");
    ok &= require(loaded.maps.front().atlasPlacement.parentId == "test_parent_map_roundtrip" &&
                      loaded.maps.front().atlasPlacement.parentRect.x == 0.15f &&
                      loaded.maps.front().atlasPlacement.parentRect.height == 0.35f &&
                      loaded.maps.front().atlasPlacement.parentRevision == 5 &&
                      loaded.maps.front().atlasPlacement.sourceRevision == 7,
                  "Atlas placement and revision lineage did not round trip");
    ok &= require(loaded.maps.front().creationIntent == MapCreationIntent::RegionExploration &&
                      loaded.maps.front().sizePreset == MapSizePreset::Large &&
                      loaded.maps.front().spatialReference.calibrated &&
                      loaded.maps.front().spatialReference.unit == MapDistanceUnit::Kilometers &&
                      loaded.maps.front().spatialReference.extentWidth == 180.0f,
                  "Map V5 creation intent and spatial reference did not round trip");
    ok &= require(loaded.maps.front().importedReference.enabled &&
                      loaded.maps.front().importedReference.assetId == "test_managed_map_reference" &&
                      loaded.maps.front().importedReference.sourceWidth == 2048 &&
                      loaded.maps.front().importedReference.crop.width == 0.7f &&
                      loaded.maps.front().importedReference.workingProxyAssetId == "test_managed_map_proxy" &&
                      loaded.maps.front().importedReference.workingProxyProjectRelativePath.find("reference-working.png") != std::string::npos &&
                      loaded.maps.front().importedReference.workingProxyWidth == 1024 &&
                      loaded.maps.front().importedReference.workingProxyHeight == 512 &&
                      loaded.maps.front().importedReference.exifOrientation == 8 &&
                      loaded.maps.front().importedReference.workingProxyGenerated,
                  "Managed map reference metadata did not round trip");
    ok &= require(loaded.maps.front().layers.size() == 1 && loaded.maps.front().layers.front().name == "GM Notes", "Map layers did not round trip");
    ok &= require(loaded.maps.front().tokens.size() == 1 && loaded.maps.front().tokens.front().linkedEntityType == "player", "Map tokens did not round trip");
    ok &= require(loaded.maps.front().features.size() == 1 && loaded.maps.front().features.front().gmOnly, "Map features did not round trip");
    ok &= require(loaded.maps.front().regions.size() == 1 && loaded.maps.front().regions.front().difficultTerrain, "Map regions did not round trip");
    ok &= require(loaded.maps.front().routes.size() == 1 && loaded.maps.front().routes.front().distanceMiles == 12.0f, "Map routes did not round trip");
    ok &= require(!loaded.maps.front().cells.empty() && loaded.maps.front().cells.front().fogged && loaded.maps.front().cells.front().movementCost == 2, "Map cell exploration metadata did not round trip");
    ok &= require(loaded.maps.front().cells.front().elevation == 2 && loaded.maps.front().cells.front().movementBlockedMask == 3 && loaded.maps.front().cells.front().lineOfSightBlockedMask == 1, "Map cell tactical edge metadata did not round trip");
    ok &= require(loaded.maps.front().cells.front().symbolVariantMode == MapSymbolVariantMode::Individual && loaded.maps.front().cells.front().wayKind == MapWayKind::DirtRoad && loaded.maps.front().cells.front().rotationLocked, "Map cell symbolic/way metadata did not round trip");
    ok &= require(loaded.dungeons.size() == 1 && loaded.dungeons.front().rooms.size() == 1, "Dungeon did not round trip");
    ok &= require(loaded.dungeons.front().rooms.front().readAloudText.find("table") != std::string::npos, "Dungeon room read-aloud text did not round trip");
    ok &= require(loaded.dungeons.front().rooms.front().connections.size() == 1 && loaded.dungeons.front().rooms.front().connections.front().secret, "Dungeon room connections did not round trip");
    ok &= require(loaded.encounters.size() == 1 && loaded.encounters.front().combatPhase == CombatPhase::ActionResolution, "Encounter combat phase did not round trip");
    ok &= require(loaded.encounters.front().sourceMapId == "test_map_roundtrip" &&
                      loaded.encounters.front().sourceMapRect.x == 0.2f &&
                      loaded.encounters.front().sourceMapRect.height == 0.5f &&
                      loaded.encounters.front().sourceMapRevision == 7 &&
                      loaded.encounters.front().tacticalMapId &&
                      *loaded.encounters.front().tacticalMapId == "test_map_roundtrip",
                  "Encounter atlas provenance did not round trip");
    ok &= require(!loaded.encounters.front().participants.empty() &&
                      loaded.encounters.front().participants.front().narrativePosition == "Front line" &&
                      loaded.encounters.front().participants.front().narrativeRangeBand == "Engaged" &&
                      loaded.encounters.front().participants.front().narrativeSpotlight,
                  "Encounter narrative participant state did not round trip");
    ok &= require(loaded.handouts.size() == 1 && loaded.handouts.front().title == "Test Handout", "Handout did not round trip");
    ok &= require(loaded.handouts.front().gmNotes.find("answer") != std::string::npos, "Handout GM notes did not round trip");
    ok &= require(!loaded.localPlayerDisplay.enabled && loaded.localPlayerDisplay.blackout &&
                      !loaded.localPlayerDisplay.published && loaded.localPlayerDisplay.publicMapId == "test_map_roundtrip",
                  "Loading a saved campaign must retain selected player content while keeping the window closed");
    ok &= require(!loaded.localPlayerDisplay.showInitiative && loaded.localPlayerDisplay.publicHandoutIds.size() == 1 && loaded.localPlayerDisplay.publicHandoutIds.front() == "test_handout_roundtrip" &&
                      loaded.localPlayerDisplay.showParty && loaded.localPlayerDisplay.showPartySpellSlots && loaded.localPlayerDisplay.showPartyInventory &&
                      loaded.localPlayerDisplay.visibleCharacterIds.size() == 1 &&
                      loaded.localPlayerDisplay.visibleCharacterIds.front() == loaded.characters.front().metadata.id &&
                      std::abs(loaded.localPlayerDisplay.playerTextScale - 1.45f) < 0.001f,
                  "Local player display reveal and presentation preferences did not round trip");
    ok &= require(!loaded.localPlayerDisplay.mapOnly &&
                      loaded.localPlayerDisplay.preferredMonitorIndex == 2 &&
                      !loaded.localPlayerDisplay.fullscreenMapOnly &&
                      std::abs(loaded.localPlayerDisplay.publicCamera.normalizedCenterX - 0.25f) < 0.001f &&
                      std::abs(loaded.localPlayerDisplay.publicCamera.normalizedCenterY - 0.75f) < 0.001f &&
                      std::abs(loaded.localPlayerDisplay.publicCamera.zoom - 2.5f) < 0.001f &&
                      !loaded.localPlayerDisplay.publicCamera.followGmCamera,
                  "Local player display layout and public camera did not round trip");
    ok &= require(loaded.localPlayerDisplay.blackout && !loaded.localPlayerDisplay.published,
                  "Persisted player display state must reload in fail-closed blackout");
    ok &= require(loaded.timelineEvents.size() == 1 && loaded.timelineEvents.front().inWorldDate == "Bloomwane 12", "Timeline event did not round trip");
    ok &= require(loaded.storyThreads.size() == 1 && loaded.storyThreads.front().beats.size() == 1, "Story scene did not round trip");
    if (!loaded.storyThreads.empty() && !loaded.storyThreads.front().beats.empty())
    {
        const StoryBeatModel &loadedBeat = loaded.storyThreads.front().beats.front();
        ok &= require(loadedBeat.playerSummary.find("Public") != std::string::npos, "Story scene player summary did not round trip");
        ok &= require(loadedBeat.readAloudText.find("aloud") != std::string::npos, "Story scene read-aloud text did not round trip");
        ok &= require(loadedBeat.clues.find("Hidden") != std::string::npos, "Story scene clues did not round trip");
        ok &= require(loadedBeat.consequences.find("branch") != std::string::npos, "Story scene consequences did not round trip");
    }
    const PlayerSafeStoryScene playerScene = buildPlayerSafeStoryScene(loaded);
    ok &= require(playerScene.available && playerScene.playerSummary.find("Public") != std::string::npos, "Player-safe scene should expose public scene summary");
    ok &= require(playerScene.readAloudText.find("aloud") != std::string::npos, "Player-safe scene should expose read-aloud text");
    ok &= require(playerScene.playerSummary.find("Private") == std::string::npos, "Player-safe scene should not expose private GM outline");
    ok &= require(!loaded.timelineEvents.front().relatedEntityIds.empty() && loaded.timelineEvents.front().relatedEntityIds.front() == "test_character_roundtrip", "Timeline related entities did not round trip");
    ok &= require(loaded.sessions.size() == 1, "Session did not round trip");
    ok &= require(loaded.sessions.front().roughNotes == campaign.sessions.front().roughNotes && loaded.sessions.front().planEntries.size() == 1, "Session draft and plan did not round trip");
    ok &= require(loaded.sessions.front().planEntries.front().storyBeatId == "test_scene_roundtrip" && loaded.sessions.front().planEntries.front().state == SessionEntryState::Completed, "Planned scene identity/progress did not round trip");
    ok &= require(loaded.sessions.front().liveState.initialized && loaded.sessions.front().liveState.phase == SessionPhase::WrapUp && loaded.sessions.front().liveState.activePlanEntryId == "test_plan_entry_roundtrip", "Live session cursor/phase did not round trip");
    ok &= require(loaded.sessions.front().startedAtUtc == "2026-05-23T18:05:00Z" && loaded.sessions.front().endedAtUtc == "2026-05-23T21:15:00Z", "Session start/end timestamps did not round trip");
    ok &= require(!loaded.sessions.front().participantCharacterIds.empty() && loaded.sessions.front().participantCharacterIds.front() == "test_character_roundtrip", "Session participants did not round trip");
    ok &= require(loaded.sessions.front().recap.find("escaped") != std::string::npos, "Session recap did not round trip");
    ok &= require(loaded.sessions.front().lootAwarded == "Silver key" && loaded.sessions.front().openHooks.find("deeper") != std::string::npos, "Session structured recap fields did not round trip");
    ok &= require(loaded.sessions.front().notes.size() == 1, "Session note did not round trip");
    ok &= require(loaded.sessions.front().notes.front().timestampUtc == "2026-05-23T18:30:00Z" && loaded.sessions.front().notes.front().linkedEntityId == "test_character_roundtrip", "Session note timestamp/link did not round trip");
    {
        const auto legacyPath = std::filesystem::temp_directory_path() / (createEntityId("session_v22_migration") + ".db");
        CampaignRepository::writeSnapshot(legacyPath.string());
        {
            SQLite::Database legacy(legacyPath.string(), SQLite::OPEN_READWRITE);
            legacy.exec("ALTER TABLE sessions DROP COLUMN rough_notes;");
            legacy.exec("ALTER TABLE sessions DROP COLUMN plan_entries_json;");
            legacy.exec("ALTER TABLE sessions DROP COLUMN live_state_json;");
            legacy.exec("ALTER TABLE local_player_display DROP COLUMN show_party_inventory;");
            legacy.exec("DELETE FROM schema_migrations WHERE version = 23;");
            legacy.exec("DELETE FROM schema_migrations WHERE version = 31;");
            legacy.exec("PRAGMA user_version = 22;");
        }
        Campaign migrated; std::string migrationError;
        ok &= require(DatabaseManager::readCampaignFromSnapshot(legacyPath.string(), campaign.metadata.id, migrated, migrationError), "Legacy session migration failed: " + migrationError);
        ok &= require(!migrated.localPlayerDisplay.showPartyInventory,
                      "Legacy player-display inventory sharing must migrate with the privacy-preserving default off");
        if (!migrated.sessions.empty())
        {
            ok &= require(migrated.sessions.front().planEntries.empty() && !migrated.sessions.front().liveState.initialized && migrated.sessions.front().roughNotes.empty(), "Legacy migration must not guess scene order or live cursor");
            ok &= require(migrated.sessions.front().summary == campaign.sessions.front().summary && migrated.sessions.front().notes.front().body == campaign.sessions.front().notes.front().body, "Legacy migration changed existing session prose/history");
        }
        Campaign reopened;
        ok &= require(DatabaseManager::readCampaignFromSnapshot(legacyPath.string(), campaign.metadata.id, reopened, migrationError) && reopened.sessions.size() == migrated.sessions.size(), "Session migration must be idempotent");
        std::error_code removeError; std::filesystem::remove(legacyPath, removeError);
    }
    ok &= require(loaded.audioAssets.size() == 1 && loaded.audioAssets.front().name == "Test Rain Loop", "Audio asset did not round trip");
    ok &= require(loaded.audioAssets.front().durationSeconds == 72 && loaded.audioAssets.front().lastPlayedAtUtc == "2026-05-23T20:00:00Z", "Audio preview metadata did not round trip");
    ok &= require(loaded.audioPlaylists.size() == 1 && loaded.audioPlaylists.front().name == "Test Ambience Stack", "Audio playlist did not round trip");
    ok &= require(loaded.audioPlaylists.front().shuffle && loaded.audioPlaylists.front().repeat && loaded.audioPlaylists.front().oneShot && loaded.audioPlaylists.front().crossfadeSeconds == 1.5f, "Audio playlist playback settings did not round trip");
    ok &= require(loaded.audioPlaylists.front().audioAssetIds.size() == 1 && loaded.audioPlaylists.front().audioAssetIds.front() == "test_audio_roundtrip", "Audio playlist asset IDs did not round trip");
    ok &= require(loaded.audioAmbienceLayers.size() == 1 && loaded.audioAmbienceLayers.front().name == "Rain Layer", "Audio ambience layer did not round trip");
    ok &= require(loaded.audioAmbienceLayers.front().audioAssetId == "test_audio_roundtrip" && loaded.audioAmbienceLayers.front().volume == 0.42f && loaded.audioAmbienceLayers.front().sortOrder == 2, "Audio ambience layer settings did not round trip");
    ok &= require(loaded.audioTriggers.size() == 1 && loaded.audioTriggers.front().eventName == "Scene Started", "Audio trigger did not round trip");
    ok &= require(loaded.audioTriggers.front().sourceType == "StoryBeat" && loaded.audioTriggers.front().audioAssetId == "test_audio_roundtrip" && loaded.audioTriggers.front().loop, "Audio trigger settings did not round trip");
    ok &= require(loaded.shops.size() == 1 && loaded.shops.front().stock.size() == 1, "Shop stock did not round trip");
    ok &= require(loaded.shops.front().stock.front().restocks && loaded.shops.front().stock.front().priceMultiplier == 1.25, "Shop stock details did not round trip");
    ok &= require(loaded.shops.front().factionReputationNotes.find("better prices") != std::string::npos, "Shop faction reputation notes did not round trip");
    ok &= require(loaded.shops.front().stock.front().availability == "Limited" && loaded.shops.front().stock.front().sellPriceMultiplier == 0.45, "Shop merchant pricing fields did not round trip");
    ok &= require(loaded.lootParcels.size() == 1 && loaded.lootParcels.front().items.size() == 1, "Loot parcel did not round trip");
    ok &= require(loaded.lootParcels.front().gold == 12 && loaded.lootParcels.front().items.front().quantity == 2, "Loot parcel rewards did not round trip");
    ok &= require(loaded.partyLocation.mapId == "test_map_roundtrip", "Party location did not round trip");
    ok &= require(loaded.partyTravel.routeId == "test_route_roundtrip" && loaded.partyTravel.milesTraveled == 4.0f, "Party travel state did not round trip");
    ok &= require(loaded.characters.front().inventory.entries.size() == 1, "Inventory entry did not round trip");
    ok &= require(loaded.customItems.front().weaponLongRangeFeet == 60, "Custom weapon long range did not round trip");
    ok &= require(loaded.customItems.front().weaponProperties.size() == 3, "Custom weapon properties did not round trip");
    ok &= require(loaded.customItems.front().weaponMastery == "Vex", "Custom weapon mastery did not round trip");
    ok &= require(loaded.rulesetDefinitions.size() == 1, "Campaign ruleset definition did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().baseRulesetId == "dnd-2024-5.5e", "Ruleset definition base ruleset did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().abilityScores.size() == 1 && loaded.rulesetDefinitions.front().abilityScores.front().name == "Might", "Homebrew ability scores did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().derivedStats.size() == 1 && loaded.rulesetDefinitions.front().derivedStats.front().formula.find("proficiency") != std::string::npos, "Homebrew derived stats did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().skills.size() == 1 && loaded.rulesetDefinitions.front().skills.front().abilityScoreId == "agility", "Homebrew skills did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().defenses.size() == 1 && loaded.rulesetDefinitions.front().defenses.front().id == "guard", "Homebrew defenses did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().resources.size() == 1 && loaded.rulesetDefinitions.front().resources.front().maximumFormula.find("level") != std::string::npos, "Homebrew resources did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().diceFormulas.size() == 1 && loaded.rulesetDefinitions.front().diceFormulas.front().formula == "1d20 + endurance.mod", "Homebrew dice formulas did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().currencies.size() == 1 && loaded.rulesetDefinitions.front().currencies.front().baseUnitValue == 10, "Homebrew currencies did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().sizes.size() == 1 && loaded.rulesetDefinitions.front().sizes.front().spaceFeet == 15, "Homebrew sizes did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().speeds.size() == 1 && loaded.rulesetDefinitions.front().speeds.front().id == "burrow", "Homebrew speeds did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().conditions.size() == 1 && loaded.rulesetDefinitions.front().conditions.front().mechanicalEffects.size() == 2, "Homebrew conditions did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().classes.size() == 1 && loaded.rulesetDefinitions.front().classes.front().primaryAbilityScoreIds.size() == 2, "Homebrew classes did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().subclasses.size() == 1 && loaded.rulesetDefinitions.front().subclasses.front().parentClassId == "warden", "Homebrew subclasses did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().ancestries.size() == 1 && loaded.rulesetDefinitions.front().ancestries.front().name == "Emberkin", "Homebrew ancestries did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().backgrounds.size() == 1 && loaded.rulesetDefinitions.front().backgrounds.front().skillIds.size() == 2, "Homebrew backgrounds did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().features.size() == 1 && loaded.rulesetDefinitions.front().features.front().level == 2, "Homebrew features did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().equipmentCategories.size() == 1 && loaded.rulesetDefinitions.front().equipmentCategories.front().slotType == "held", "Homebrew equipment categories did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().spellTraditions.size() == 1 && loaded.rulesetDefinitions.front().spellTraditions.front().abilityScoreId == "spirit", "Homebrew spell traditions did not round trip");
    ok &= require(loaded.rulesetDefinitions.front().monsterTemplates.size() == 1 && loaded.rulesetDefinitions.front().monsterTemplates.front().challengeFormula.find("party_level") != std::string::npos, "Homebrew monster templates did not round trip");
    ok &= require(loaded.rulesetContentPacks.size() == 1, "Homebrew content pack did not round trip");
    ok &= require(loaded.rulesetContentPacks.front().author == "Test GM", "Homebrew content pack author did not round trip");
    ok &= require(loaded.rulesetContentPacks.front().license == "Table Use Only", "Homebrew content pack license did not round trip");
    ok &= require(loaded.rulesetContentPacks.front().importedAtUtc == "2026-05-22T00:00:00Z", "Homebrew content pack import date did not round trip");
    ok &= require(loaded.rulesetContentPacks.front().source == "local-test-fixture", "Homebrew content pack source did not round trip");
    ok &= require(loaded.rulesetContentPacks.front().enabled, "Homebrew content pack enabled flag did not round trip");
    ok &= require(loaded.houseRules.size() == 1, "House rule did not round trip");
    ok &= require(loaded.houseRules.front().label == "Potion Timing", "House rule label did not round trip");
    ok &= require(loaded.houseRules.front().changeHistory.size() == 1, "House rule change history did not round trip");
    ok &= require(loaded.savedFilters.size() == 1, "Saved filter did not round trip");
    ok &= require(loaded.savedFilters.front().name == "Open Clues" && loaded.savedFilters.front().query == "hatch", "Saved filter query did not round trip");
    ok &= require(loaded.savedFilters.front().entityTypes.size() == 2 && loaded.savedFilters.front().requiredTags.size() == 1, "Saved filter criteria did not round trip");
    ok &= require(loaded.entityBookmarks.size() == 1, "Entity bookmark did not round trip");
    ok &= require(loaded.entityBookmarks.front().favorite && loaded.entityBookmarks.front().openCount == 2, "Entity bookmark favorite/recent state did not round trip");

    campaign.characters.front().level = 4;
    campaign.customItems.clear();
    campaign.rulesetDefinitions.front().version = "1.0.1";
    campaign.rulesetContentPacks.front().version = "0.2.1";
    campaign.rulesetContentPacks.front().enabled = false;
    campaign.houseRules.front().enabled = false;
    campaign.houseRules.front().changeHistory.push_back({"2026-05-22T01:00:00Z", "Disabled after table vote."});
    campaign.savedFilters.front().query = "signal";
    campaign.entityBookmarks.front().openCount = 3;
    campaign.sessions.clear();
    campaign.timelineEvents.clear();
    campaign.audioPlaylists.clear();
    campaign.audioAmbienceLayers.clear();
    campaign.audioTriggers.clear();
    CampaignRepository::saveCampaign(campaign);
    Campaign &updated = CampaignRepository::loadCampaign(campaign.name);
    ok &= require(updated.characters.size() == 1, "Character upsert created duplicates");
    ok &= require(updated.characters.front().level == 4, "Character upsert did not update level");
    ok &= require(updated.customItems.empty(), "Missing custom item was not deleted by stable-id save");
    ok &= require(updated.rulesetDefinitions.size() == 1, "Ruleset definition upsert created duplicates");
    ok &= require(updated.rulesetDefinitions.front().version == "1.0.1", "Ruleset definition upsert did not update version");
    ok &= require(updated.rulesetContentPacks.size() == 1, "Content pack upsert created duplicates");
    ok &= require(updated.rulesetContentPacks.front().version == "0.2.1", "Content pack upsert did not update version");
    ok &= require(!updated.rulesetContentPacks.front().enabled, "Content pack enabled flag did not update");
    ok &= require(updated.houseRules.size() == 1, "House rule upsert created duplicates");
    ok &= require(!updated.houseRules.front().enabled, "House rule enabled flag did not update");
    ok &= require(updated.houseRules.front().changeHistory.size() == 2, "House rule change history did not update");
    ok &= require(updated.savedFilters.size() == 1 && updated.savedFilters.front().query == "signal", "Saved filter upsert did not update query");
    ok &= require(updated.entityBookmarks.size() == 1 && updated.entityBookmarks.front().openCount == 3, "Entity bookmark upsert did not update recent state");
    ok &= require(updated.sessions.empty(), "Missing session was not deleted by stable-id save");
    ok &= require(updated.timelineEvents.empty(), "Missing timeline event was not deleted by stable-id save");
    ok &= require(updated.audioPlaylists.empty(), "Missing audio playlist was not deleted by stable-id save");
    ok &= require(updated.audioAmbienceLayers.empty(), "Missing audio ambience layer was not deleted by stable-id save");
    ok &= require(updated.audioTriggers.empty(), "Missing audio trigger was not deleted by stable-id save");

    RulesetDefinitionModel installedDefinition;
    installedDefinition.metadata.id = "test_global_ruleset_definition";
    installedDefinition.name = "Global Test Ruleset";
    installedDefinition.version = "0.1.0";
    installedDefinition.definitionJson = R"({"name":"Global Test Ruleset"})";
    CampaignRepository::saveInstalledRulesetDefinition(installedDefinition);
    std::vector<RulesetDefinitionModel> installedDefinitions = CampaignRepository::loadInstalledRulesetDefinitions();
    const auto installed = std::find_if(installedDefinitions.begin(), installedDefinitions.end(), [](const RulesetDefinitionModel &definition) {
        return definition.metadata.id == "test_global_ruleset_definition";
    });
    ok &= require(installed != installedDefinitions.end(), "Installed ruleset definition did not persist globally");
    if (installed != installedDefinitions.end())
    {
        ok &= require(installed->installedGlobally, "Installed ruleset definition did not keep installed flag");
        ok &= require(installed->name == "Global Test Ruleset", "Installed ruleset definition name did not round trip");
    }
    return ok;
}

bool testCampaignDetailsRenamePersistence()
{
    Campaign original = makeCampaign("details_rename");
    Campaign staged = original;
    staged.name = "Renamed Foundation Campaign";
    staged.description = "A deliberately saved campaign details update.";

    ProjectSaveService service(ProjectSaveDependencies{});
    service.setBackupSnapshotsEnabled(false);
    service.setRecoverySnapshotsEnabled(false);
    const ProjectSaveResult saved = service.saveCampaignDetailed(staged, "save campaign details");
    bool ok = require(saved.committed(), "Campaign details rename did not commit");
    Campaign &reopened = CampaignRepository::loadCampaign(staged.name);
    ok &= require(reopened.metadata.id == original.metadata.id && reopened.name == staged.name &&
                      reopened.description == staged.description,
                  "Campaign details rename did not preserve the stable identity and saved details");
    ok &= require(reopened.characters.size() == original.characters.size() &&
                      reopened.characters.front().metadata.id == original.characters.front().metadata.id,
                  "Campaign details rename changed campaign relationships");

    SQLite::Database control(ApplicationPaths::databasePath().u8string(), SQLite::OPEN_READWRITE);
    control.exec("CREATE TRIGGER abort_campaign_details_rename BEFORE UPDATE ON campaigns WHEN NEW.stable_id = 'test_campaign_details_rename' BEGIN SELECT RAISE(ABORT, 'injected campaign details failure'); END;");
    struct TriggerGuard
    {
        SQLite::Database &database;
        ~TriggerGuard()
        {
            try { database.exec("DROP TRIGGER IF EXISTS abort_campaign_details_rename"); }
            catch (...) {}
        }
    } trigger{control};
    Campaign rejected = staged;
    rejected.name = "Rejected Campaign Details Rename";
    rejected.description = "This failed update must remain a retryable draft only.";
    const ProjectSaveResult failed = service.saveCampaignDetailed(rejected, "save campaign details");
    ok &= require(!failed.committed() && !failed.error.empty(),
                  "A failed campaign details save must report a transaction failure");
    Campaign &afterFailure = CampaignRepository::loadCampaign(staged.name);
    ok &= require(afterFailure.metadata.id == staged.metadata.id && afterFailure.name == staged.name &&
                      afterFailure.description == staged.description,
                  "A failed campaign details save changed the durable campaign");
    return ok;
}

bool testCampaignSaveTransactionRollback()
{
    Campaign original = makeCampaign("late_save_rollback");
    EncounterModel retainedEncounter;
    retainedEncounter.metadata.id = "test_retained_encounter_late_save";
    retainedEncounter.name = "Combat draft retry";
    EncounterParticipantModel retainedParticipant;
    retainedParticipant.metadata.id = "test_retained_participant_late_save";
    retainedParticipant.displayName = "Guard";
    retainedParticipant.currentHitPoints = retainedParticipant.maxHitPoints = 12;
    retainedEncounter.participants.push_back(retainedParticipant);
    original.encounters.push_back(retainedEncounter);
    MapModel retainedMap;
    retainedMap.metadata.id = "test_retained_map_late_save";
    retainedMap.name = "Must survive failed deletion";
    retainedMap.width = retainedMap.height = 1;
    retainedMap.cells.resize(1);
    original.maps.push_back(retainedMap);
    SessionModel retainedSession;
    retainedSession.metadata.id = "test_retained_session_late_save";
    retainedSession.title = "Must survive failed deletion";
    original.sessions.push_back(retainedSession);
    original.sessionIds.push_back(retainedSession.metadata.id);
    CampaignRepository::saveCampaign(original);

    SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
    // Compare every persisted value and row ID, including tables whose save
    // strategy first deletes children and then inserts their replacements.
    const auto databaseRows = [&]() {
        std::string snapshot;
        SQLite::Statement tables(control, "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name;");
        while (tables.executeStep())
        {
            const std::string table = tables.getColumn(0).getString();
            snapshot += table + "\n";
            SQLite::Statement rows(control, "SELECT * FROM \"" + table + "\" ORDER BY rowid;");
            while (rows.executeStep())
            {
                for (int index = 0; index < rows.getColumnCount(); ++index)
                {
                    const auto column = rows.getColumn(index);
                    if (column.isNull()) snapshot += "null;";
                    else
                    {
                        const std::string value = column.getString();
                        snapshot += std::to_string(column.getType()) + ":" + std::to_string(value.size()) + ":" + value + ";";
                    }
                }
                snapshot += "\n";
            }
        }
        return snapshot;
    };
    const std::string before = databaseRows();
    Campaign attempted = original;
    attempted.encounters.front().participants.front().temporaryHitPoints = 4;
    attempted.description = "Must roll back with every child row";
    attempted.characters.front().level = 9;
    attempted.maps.front().name = "Changed before the late failure";
    attempted.maps.front().cells.front().fogged = false;
    attempted.maps.front().tokens.clear();
    attempted.maps.pop_back();
    attempted.sessions.front().roughNotes = "Unsaved replacement preparation";
    attempted.sessions.front().notes.clear();
    attempted.sessions.pop_back();
    attempted.sessionIds.pop_back();
    attempted.storyThreads.front().beats.front().summary = "Changed scene before the late failure";
    attempted.timelineEvents.clear();
    TimelineEventModel lateEvent;
    lateEvent.metadata.id = "test_late_save_failure_event";
    lateEvent.title = "Final write fails";
    attempted.timelineEvents.push_back(lateEvent);

    control.exec("CREATE TRIGGER test_abort_late_campaign_save BEFORE INSERT ON timeline_events "
                 "WHEN NEW.stable_id = 'test_late_save_failure_event' "
                 "BEGIN SELECT RAISE(ABORT, 'injected late campaign save failure'); END;");
    struct TriggerCleanup
    {
        SQLite::Database &database;
        ~TriggerCleanup() { try { database.exec("DROP TRIGGER IF EXISTS test_abort_late_campaign_save;"); } catch (...) {} }
    } cleanup{control};
    bool injectedFailure = false;
    try { CampaignRepository::saveCampaign(attempted); }
    catch (const std::exception &error)
    {
        injectedFailure = std::string(error.what()).find("injected late campaign save failure") != std::string::npos;
    }
    bool ok = require(injectedFailure, "The save did not reach the injected failure on the final table");
    ok &= require(databaseRows() == before, "A late save failure left partial updates/deletions in the database");
    ok &= require(attempted.encounters.front().participants.front().temporaryHitPoints == 4,
                  "Failed saving discarded the editable temporary-HP draft");

    control.exec("DROP TRIGGER test_abort_late_campaign_save;");
    CampaignRepository::saveCampaign(attempted);
    const Campaign reopened = CampaignRepository::loadCampaign(attempted.name);
    ok &= require(reopened.encounters.front().participants.front().temporaryHitPoints == 4,
                  "Retrying the retained combat draft did not persist temporary HP");
    ok &= require(reopened.description == attempted.description && reopened.characters.front().level == 9 &&
                      reopened.maps.size() == 1 && reopened.maps.front().name == attempted.maps.front().name &&
                      reopened.maps.front().tokens.empty() && !reopened.maps.front().cells.front().fogged &&
                      reopened.sessions.size() == 1 && reopened.sessions.front().roughNotes == attempted.sessions.front().roughNotes &&
                      reopened.sessions.front().notes.empty() && reopened.timelineEvents.size() == 1 &&
                      reopened.timelineEvents.front().metadata.id == lateEvent.metadata.id,
                  "Retry after rollback did not persist the complete campaign");
    const auto snapshotPath = std::filesystem::current_path() / "after_transaction_commit.db";
    CampaignRepository::writeSnapshot(snapshotPath.string());
    ok &= require(std::filesystem::is_regular_file(snapshotPath), "A successful transaction left the connection unable to create a snapshot");
    std::filesystem::remove(snapshotPath);
    return ok;
}

bool testTabletopToolkitOwnershipGuard()
{
    Campaign original = makeCampaign("toolkit_owner_original");
    Campaign copy = makeCampaign("toolkit_owner_copy");
    CampaignRepository::saveCampaign(original);
    CampaignRepository::saveCampaign(copy);

    Campaign attempted = copy;
    attempted.description = "This failed toolkit write must not persist.";
    attempted.diceMacros.clear();
    attempted.safetyProfile.warnings.front().metadata.id = original.safetyProfile.warnings.front().metadata.id;
    bool rejected = false;
    try { CampaignRepository::saveCampaign(attempted); }
    catch (const std::exception &error)
    {
        rejected = std::string(error.what()).find("Tabletop toolkit identity belongs to another campaign") != std::string::npos;
    }

    const Campaign reopenedOriginal = CampaignRepository::loadCampaign(original.name);
    const Campaign reopenedCopy = CampaignRepository::loadCampaign(copy.name);
    bool ok = require(rejected, "A cross-campaign tabletop toolkit stable-ID collision must fail");
    ok &= require(reopenedOriginal.safetyProfile.warnings.size() == 1 &&
                      reopenedOriginal.safetyProfile.warnings.front().metadata.id == original.safetyProfile.warnings.front().metadata.id &&
                      reopenedOriginal.storyObjectives.size() == original.storyObjectives.size() &&
                      reopenedOriginal.storyClues.size() == original.storyClues.size(),
                  "Rejected toolkit collision changed the owning campaign");
    ok &= require(reopenedCopy.description == copy.description &&
                      reopenedCopy.diceMacros.size() == 1 &&
                      reopenedCopy.diceMacros.front().metadata.id == copy.diceMacros.front().metadata.id &&
                      reopenedCopy.safetyProfile.warnings.size() == 1 &&
                      reopenedCopy.safetyProfile.warnings.front().metadata.id == copy.safetyProfile.warnings.front().metadata.id,
                  "Rejected toolkit collision partially saved or deleted the attempted campaign");
    return ok;
}

bool testAudioDraftSaveFailureRetry()
{
    Campaign draft = makeCampaign("audio_failed_retry");
    CampaignRepository::saveCampaign(draft);
    const auto originalAudioCount = draft.audioAssets.size();
    AudioAssetModel asset;
    asset.metadata.id = "test_audio_failed_retry_asset";
    asset.name = "Rain for the next scene";
    draft.audioAssets.push_back(asset);
    AudioPlaylistModel playlist;
    playlist.metadata.id = "test_audio_failed_retry_playlist";
    playlist.name = "Retained queue";
    playlist.audioAssetIds.push_back(asset.metadata.id);
    draft.audioPlaylists.push_back(playlist);

    SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
    control.exec("CREATE TRIGGER test_abort_audio_draft BEFORE INSERT ON audio_assets "
                 "WHEN NEW.stable_id = 'test_audio_failed_retry_asset' "
                 "BEGIN SELECT RAISE(ABORT, 'injected audio save failure'); END;");
    struct TriggerCleanup
    {
        SQLite::Database &database;
        ~TriggerCleanup() { try { database.exec("DROP TRIGGER IF EXISTS test_abort_audio_draft;"); } catch (...) {} }
    } cleanup{control};
    auto &service = ProjectSaveService::getInstance();
    const auto countSavedEvents = [&]() {
        return std::count_if(ProjectEventBus::getInstance().history().begin(), ProjectEventBus::getInstance().history().end(),
            [&](const ProjectEvent &event) { return event.type == ProjectEventType::CampaignSaved && event.campaignId == draft.metadata.id; });
    };
    const auto savedEventsBefore = countSavedEvents();
    bool ok = require(!service.saveCampaign(draft, "audio edits"), "Injected audio persistence failure must be reported");
    ok &= require(service.state().dirty && !service.state().lastError.empty(), "Failed audio save must retain dirty/error state");
    ok &= require(countSavedEvents() == savedEventsBefore, "A failed audio save must not publish a saved event");
    const Campaign unchanged = CampaignRepository::loadCampaign(draft.name);
    ok &= require(unchanged.audioAssets.size() == originalAudioCount, "Failed audio save must not partially install the asset");
    ok &= require(draft.audioAssets.back().metadata.id == asset.metadata.id && draft.audioPlaylists.back().audioAssetIds.front() == asset.metadata.id,
                  "Failed audio save must retain the editable asset and linked queue");
    control.exec("DROP TRIGGER test_abort_audio_draft;");
    ok &= require(service.saveCampaign(draft, "retry audio edits"), "Retrying retained audio edits should save");
    const Campaign reopened = CampaignRepository::loadCampaign(draft.name);
    ok &= require(reopened.audioAssets.size() == originalAudioCount + 1 &&
                  std::count_if(reopened.audioAssets.begin(), reopened.audioAssets.end(), [&](const AudioAssetModel &value) { return value.metadata.id == asset.metadata.id; }) == 1,
                  "Audio retry must preserve one stable asset identity without duplication");
    const auto reopenedPlaylist = std::find_if(reopened.audioPlaylists.begin(), reopened.audioPlaylists.end(),
        [&](const AudioPlaylistModel &value) { return value.metadata.id == playlist.metadata.id; });
    ok &= require(reopenedPlaylist != reopened.audioPlaylists.end() && reopenedPlaylist->audioAssetIds == playlist.audioAssetIds,
                  "Audio retry must preserve playlist identity and queue references");
    return ok;
}

bool testProjectSaveWorkflow()
{
    ProjectSaveService &saveService = ProjectSaveService::getInstance();
    saveService.setBackupSnapshotsEnabled(false);
    saveService.setRecoverySnapshotsEnabled(false);

    Campaign campaign = makeCampaign("save_workflow");
    saveService.markDirty("test");
    const bool saved = saveService.saveCampaign(campaign, "test save workflow");
    const ProjectSaveState &state = saveService.state();

    bool ok = true;
    ok &= require(saved, "Project save service failed");
    ok &= require(!state.dirty, "Project save service did not clear dirty state");
    ok &= require(!state.lastSavedAtUtc.empty(), "Project save service did not record save time");
    ok &= require(state.lastError.empty(), "Project save service recorded an unexpected error");
    std::filesystem::remove_all(ProjectPackageLayout::campaignPackageDirectory(campaign));
    return ok;
}

bool testAudioAssetPathService()
{
    Campaign campaign;
    campaign.metadata.id = "audio path test campaign";

    const std::filesystem::path sourcePath = std::filesystem::current_path() / "audio_source_test.WAV";
    {
        std::ofstream file(sourcePath, std::ios::binary);
        file << "RIFFtest";
    }

    AudioAssetModel asset;
    asset.metadata.id = "audio asset/test";
    asset.originalSourcePath = sourcePath.string();
    asset.filePath = AudioAssetPaths::copyAudioFileToCampaign(campaign, sourcePath, asset.metadata.id);

    const std::filesystem::path copiedPath = AudioAssetPaths::resolveAudioPath(asset);

    bool ok = true;
    ok &= require(AudioAssetPaths::sanitizePathSegment("audio asset/test") == "audio_assettest", "Audio path segment sanitization changed unexpectedly");
    ok &= require(ProjectAssets::sanitizePathSegment("audio asset/test") == "audio_assettest", "Project asset path segment sanitization changed unexpectedly");
    ok &= require(AudioAssetPaths::isSupportedAudioFile(sourcePath), "Uppercase WAV extension should be supported");
    ok &= require(ProjectAssets::isSupportedAudioFile(sourcePath), "Project asset service should support uppercase WAV extension");
    ok &= require(asset.filePath.find("Resources/Campaigns/audio_path_test_campaign/Assets/Audio/audio_assettest.WAV") != std::string::npos, "Audio copy should return a project-relative campaign asset path");
    ok &= require(std::filesystem::exists(copiedPath), "Copied campaign audio file was not resolvable");
    ok &= require(AudioAssetPaths::resolveOriginalSourcePath(asset) == sourcePath, "Absolute original source path should round trip");

    std::filesystem::remove(sourcePath);
    std::filesystem::remove(copiedPath);
    std::filesystem::remove_all(ProjectPackageLayout::campaignPackageDirectory(campaign));
    return ok;
}

bool testProjectPackageLayout()
{
    Campaign campaign;
    campaign.metadata.id = "Project Layout/Test Campaign";

    const ProjectPackageLayout::CampaignPackageLayout layout = ProjectPackageLayout::layoutForCampaign(campaign);

    bool ok = true;
    ok &= require(layout.rootDirectory.filename().string() == "Project_LayoutTest_Campaign", "Campaign package root should use sanitized campaign IDs");
    ok &= require(layout.databasePath.filename().string() == "campaign.db", "Campaign package database should be named campaign.db");
    ok &= require(layout.databasePath.parent_path() == layout.rootDirectory, "Campaign database should live inside the package root");
    ok &= require(layout.manifestPath.filename().string() == "project.json", "Campaign package manifest should be named project.json");
    ok &= require(ProjectPackageLayout::assetDirectory(campaign, "Audio") == layout.assetsDirectory / "Audio", "Audio assets should live under the package Assets folder");
    ok &= require(ProjectPackageLayout::assetDirectory(campaign, "PDFs/Handouts") == layout.assetsDirectory / "PDFsHandouts", "Asset folder names should be sanitized");

    ProjectPackageLayout::ensureCampaignPackageLayout(campaign);
    ok &= require(std::filesystem::exists(layout.rootDirectory), "Campaign package root was not created");
    ok &= require(std::filesystem::exists(layout.assetsDirectory / "Images"), "Image asset folder was not created");
    ok &= require(std::filesystem::exists(layout.assetsDirectory / "Portraits"), "Portrait asset folder was not created");
    ok &= require(std::filesystem::exists(layout.assetsDirectory / "Tokens"), "Token asset folder was not created");
    ok &= require(std::filesystem::exists(layout.assetsDirectory / "Maps"), "Map asset folder was not created");
    ok &= require(std::filesystem::exists(layout.assetsDirectory / "Audio"), "Audio asset folder was not created");
    ok &= require(std::filesystem::exists(layout.assetsDirectory / "Handouts"), "Handout asset folder was not created");
    ok &= require(std::filesystem::exists(layout.assetsDirectory / "Files"), "Generic file asset folder was not created");
    ok &= require(std::filesystem::exists(layout.recoveryDirectory), "Recovery folder was not created");
    ok &= require(std::filesystem::exists(layout.backupsDirectory), "Backups folder was not created");
    ok &= require(std::filesystem::exists(layout.exportsDirectory), "Exports folder was not created");

    std::filesystem::remove_all(layout.rootDirectory);
    return ok;
}

bool testProjectManifestService()
{
    Campaign campaign = makeCampaign("manifest");
    campaign.rulesetContentPacks.clear();

    RulesetContentPackModel pack;
    pack.metadata.id = "pack_manifest";
    pack.rulesetId = "dnd2024";
    pack.name = "Manifest Pack";
    pack.version = "1.2.3";
    pack.enabled = false;
    campaign.rulesetContentPacks.push_back(pack);

    ProjectManifest::RecoveryMetadata recovery;
    recovery.latestRecoverySnapshotAtUtc = "2026-05-23T20:10:00Z";
    recovery.latestRecoverySnapshotPath = "Recovery/recover.db";
    recovery.latestBackupAtUtc = "2026-05-23T20:00:00Z";
    recovery.latestBackupPath = "Backups/backup.db";
    recovery.maxRecoverySnapshots = 7;
    recovery.maxBackupSnapshots = 12;

    std::string error;
    const bool wroteManifest = ProjectManifest::writeManifest(campaign, recovery, error);
    const ProjectManifest::ProjectManifestModel loaded = ProjectManifest::loadManifest(ProjectPackageLayout::layoutForCampaign(campaign).manifestPath, error);

    bool ok = true;
    ok &= require(wroteManifest, "Project manifest should write successfully");
    ok &= require(error.empty(), "Project manifest should not report an error");
    ok &= require(loaded.format == "ttrpg-editor.project.v1", "Project manifest format marker did not round trip");
    ok &= require(loaded.appVersion == ProductInfo::version, "Project manifest app version did not round trip");
    ok &= require(loaded.schemaVersion >= 14, "Project manifest schema version should track current database schema");
    ok &= require(loaded.campaignId == campaign.metadata.id && loaded.campaignName == campaign.name, "Project manifest campaign identity did not round trip");
    ok &= require(loaded.databasePath == "campaign.db", "Project manifest should point at the package-local database path");
    ok &= require(std::find(loaded.assetFolders.begin(), loaded.assetFolders.end(), "Audio") != loaded.assetFolders.end(), "Project manifest asset folders should include audio");
    ok &= require(std::find(loaded.assetFolders.begin(), loaded.assetFolders.end(), "Handouts") != loaded.assetFolders.end(), "Project manifest asset folders should include handouts");
    ok &= require(loaded.rulesetPacks.size() == 1 && loaded.rulesetPacks.front().id == "pack_manifest", "Project manifest ruleset pack id did not round trip");
    ok &= require(!loaded.rulesetPacks.front().enabled && loaded.rulesetPacks.front().version == "1.2.3", "Project manifest ruleset pack metadata did not round trip");
    ok &= require(loaded.recovery.latestRecoverySnapshotPath == "Recovery/recover.db", "Project manifest recovery snapshot path did not round trip");
    ok &= require(loaded.recovery.latestBackupPath == "Backups/backup.db", "Project manifest backup path did not round trip");
    ok &= require(loaded.recovery.maxRecoverySnapshots == 7 && loaded.recovery.maxBackupSnapshots == 12, "Project manifest recovery limits did not round trip");

    std::filesystem::remove_all(ProjectPackageLayout::campaignPackageDirectory(campaign));
    return ok;
}

bool testProjectAssetImportService()
{
    Campaign campaign;
    campaign.metadata.id = "asset import campaign";

    const struct ImportCase
    {
        ProjectAssetImport::AssetKind kind;
        std::string fileName;
        std::string assetId;
        std::string expectedFolder;
        std::string expectedMimeType;
    } cases[] = {
        {ProjectAssetImport::AssetKind::Image, "source_image.PNG", "image_asset", "Images", "image/png"},
        {ProjectAssetImport::AssetKind::Portrait, "source_portrait.jpg", "portrait_asset", "Portraits", "image/jpeg"},
        {ProjectAssetImport::AssetKind::Token, "source_token.webp", "token_asset", "Tokens", "image/webp"},
        {ProjectAssetImport::AssetKind::Map, "source_map.svg", "map_asset", "Maps", "image/svg+xml"},
        {ProjectAssetImport::AssetKind::Audio, "source_audio.MP3", "audio_asset", "Audio", "audio/mpeg"},
        {ProjectAssetImport::AssetKind::Handout, "source_handout.pdf", "handout_asset", "Handouts", "application/pdf"},
        {ProjectAssetImport::AssetKind::GenericFile, "source_notes.bin", "file_asset", "Files", "application/octet-stream"}};

    bool ok = true;
    for (const ImportCase &importCase : cases)
    {
        const std::filesystem::path sourcePath = std::filesystem::current_path() / importCase.fileName;
        {
            std::ofstream file(sourcePath, std::ios::binary);
            file << "asset bytes";
        }

        ProjectAssetImport::AssetImportRequest request;
        request.kind = importCase.kind;
        request.sourcePath = sourcePath;
        request.assetId = importCase.assetId;
        request.displayName = "Imported " + importCase.expectedFolder;
        request.sourceReference = "original/" + importCase.fileName;

        const ProjectAssetImport::AssetImportResult result = ProjectAssetImport::importAsset(campaign, request);
        const std::filesystem::path copiedPath = ProjectAssets::resolveProjectPath(result.projectPath);

        ok &= require(result.success, std::string("Asset import failed for ") + importCase.fileName + ": " + result.errorMessage);
        ok &= require(result.assetId == importCase.assetId, "Asset import should preserve requested asset ID");
        ok &= require(result.assetFolder == importCase.expectedFolder, "Asset import used the wrong asset folder");
        ok &= require(result.originalSourcePath == "original/" + importCase.fileName, "Asset import should preserve source references");
        ok &= require(result.mimeType == importCase.expectedMimeType, "Asset import detected the wrong MIME type");
        ok &= require(!result.contentHash.empty() && result.contentHash.find("fnv1a64:") == 0, "Asset import should record a content hash");
        ok &= require(result.byteSize == 11, "Asset import should record byte size");
        ok &= require(result.projectPath.find("Resources/Campaigns/asset_import_campaign/Assets/" + importCase.expectedFolder + "/" + importCase.assetId) != std::string::npos, "Asset import should return a package-relative asset path");
        ok &= require(std::filesystem::exists(copiedPath), "Imported asset copy was not created");

        std::filesystem::remove(sourcePath);
    }

    const std::filesystem::path unsupportedAudioPath = std::filesystem::current_path() / "unsupported_audio.txt";
    {
        std::ofstream file(unsupportedAudioPath, std::ios::binary);
        file << "not audio";
    }
    ProjectAssetImport::AssetImportRequest unsupportedRequest;
    unsupportedRequest.kind = ProjectAssetImport::AssetKind::Audio;
    unsupportedRequest.sourcePath = unsupportedAudioPath;
    const ProjectAssetImport::AssetImportResult unsupportedResult = ProjectAssetImport::importAsset(campaign, unsupportedRequest);
    ok &= require(!unsupportedResult.success && unsupportedResult.errorMessage.find("Unsupported Audio") != std::string::npos, "Audio import should reject unsupported extensions");

    std::filesystem::remove(unsupportedAudioPath);
    std::filesystem::remove_all(ProjectPackageLayout::campaignPackageDirectory(campaign));
    return ok;
}

bool testProjectAssetImportDeduplication()
{
    Campaign campaign;
    campaign.metadata.id = "asset dedupe campaign";

    const std::filesystem::path firstSourcePath = std::filesystem::current_path() / "dedupe_first.png";
    const std::filesystem::path secondSourcePath = std::filesystem::current_path() / "dedupe_second.png";
    {
        std::ofstream file(firstSourcePath, std::ios::binary);
        file << "same image bytes";
    }
    {
        std::ofstream file(secondSourcePath, std::ios::binary);
        file << "same image bytes";
    }

    ProjectAssetImport::AssetImportRequest firstRequest;
    firstRequest.kind = ProjectAssetImport::AssetKind::Image;
    firstRequest.sourcePath = firstSourcePath;
    firstRequest.assetId = "first_image";

    ProjectAssetImport::AssetImportRequest secondRequest;
    secondRequest.kind = ProjectAssetImport::AssetKind::Image;
    secondRequest.sourcePath = secondSourcePath;
    secondRequest.assetId = "second_image";

    const ProjectAssetImport::AssetImportResult firstResult = ProjectAssetImport::importAsset(campaign, firstRequest);
    const ProjectAssetImport::AssetImportResult secondResult = ProjectAssetImport::importAsset(campaign, secondRequest);

    bool ok = true;
    ok &= require(firstResult.success && secondResult.success, "Duplicate asset imports should succeed");
    ok &= require(!firstResult.deduplicated, "First asset import should create the stored copy");
    ok &= require(secondResult.deduplicated, "Second asset import should reuse an existing matching hash");
    ok &= require(firstResult.contentHash == secondResult.contentHash, "Duplicate asset imports should have matching content hashes");
    ok &= require(firstResult.projectPath == secondResult.projectPath, "Duplicate asset import should reuse the first project path");
    ok &= require(secondResult.assetId == "second_image", "Deduplicated imports should preserve the caller's asset ID");
    ok &= require(std::filesystem::exists(ProjectAssets::resolveProjectPath(firstResult.projectPath)), "Deduplicated project path should resolve to the shared copy");

    std::filesystem::remove(firstSourcePath);
    std::filesystem::remove(secondSourcePath);
    std::filesystem::remove_all(ProjectPackageLayout::campaignPackageDirectory(campaign));
    return ok;
}

bool testProjectAssetRepairService()
{
    Campaign campaign;
    campaign.metadata.id = "asset repair campaign";

    const std::filesystem::path originalPath = std::filesystem::current_path() / "repair_original.pdf";
    const std::filesystem::path replacementPath = std::filesystem::current_path() / "repair_replacement.pdf";
    {
        std::ofstream file(originalPath, std::ios::binary);
        file << "original handout";
    }
    {
        std::ofstream file(replacementPath, std::ios::binary);
        file << "replacement handout";
    }

    ProjectAssetImport::AssetImportRequest importRequest;
    importRequest.kind = ProjectAssetImport::AssetKind::Handout;
    importRequest.sourcePath = originalPath;
    importRequest.assetId = "handout_repair";
    const ProjectAssetImport::AssetImportResult imported = ProjectAssetImport::importAsset(campaign, importRequest);

    bool ok = true;
    ok &= require(imported.success, "Initial handout import should succeed before repair");
    if (imported.success)
    {
        const std::filesystem::path importedPath = ProjectAssets::resolveProjectPath(imported.projectPath);
        std::filesystem::remove(importedPath);
        ok &= require(ProjectAssetRepair::isProjectAssetMissing(imported.projectPath), "Removed imported asset should be reported missing");
    }

    ProjectAssetRepair::AssetRepairRequest repairRequest;
    repairRequest.kind = ProjectAssetImport::AssetKind::Handout;
    repairRequest.assetId = "handout_repair";
    repairRequest.replacementPath = replacementPath;
    repairRequest.displayName = "Repair Handout";
    repairRequest.sourceReference = "replacement/original.pdf";
    const ProjectAssetRepair::AssetRepairResult repaired = ProjectAssetRepair::repairAsset(campaign, repairRequest);

    ok &= require(repaired.success, "Asset repair should succeed with a valid replacement");
    ok &= require(repaired.importedAsset.assetId == "handout_repair", "Asset repair should preserve the asset ID");
    ok &= require(repaired.importedAsset.projectPath == imported.projectPath, "Asset repair should restore the original package path");
    ok &= require(repaired.importedAsset.originalSourcePath == "replacement/original.pdf", "Asset repair should preserve source references");
    ok &= require(!ProjectAssetRepair::isProjectAssetMissing(repaired.importedAsset.projectPath), "Repaired asset should no longer be missing");

    ProjectAssetRepair::AssetRepairRequest invalidRepairRequest;
    invalidRepairRequest.kind = ProjectAssetImport::AssetKind::Audio;
    invalidRepairRequest.assetId = "audio_repair";
    invalidRepairRequest.replacementPath = replacementPath;
    const ProjectAssetRepair::AssetRepairResult invalidRepair = ProjectAssetRepair::repairAsset(campaign, invalidRepairRequest);
    ok &= require(!invalidRepair.success, "Asset repair should reject replacements with the wrong type");

    std::filesystem::remove(originalPath);
    std::filesystem::remove(replacementPath);
    std::filesystem::remove_all(ProjectPackageLayout::campaignPackageDirectory(campaign));
    return ok;
}

bool testProjectAssetCachePolicy()
{
    const ProjectAssetCachePolicy::AssetCacheRule textureRule = ProjectAssetCachePolicy::textureCacheRule();
    const ProjectAssetCachePolicy::AssetCacheRule audioRule = ProjectAssetCachePolicy::audioCacheRule();

    bool ok = true;
    ok &= require(textureRule.owner.find("ImageTextureCache") != std::string::npos, "Texture cache owner should be the image texture cache");
    ok &= require(textureRule.lazyLoad, "Texture cache should lazy-load on first use");
    ok &= require(textureRule.invalidateOnFileChange, "Texture cache should invalidate when files change");
    ok &= require(textureRule.releaseOnOwnerShutdown, "Texture cache should release textures on owner shutdown");
    ok &= require(!textureRule.retainOnlyWhileActive, "Texture cache can retain recently used inactive images");

    ok &= require(audioRule.owner.find("AudioPlaybackService") != std::string::npos, "Audio cache owner should be the playback service");
    ok &= require(audioRule.lazyLoad, "Audio cache should open audio only when playback starts");
    ok &= require(!audioRule.invalidateOnFileChange, "Audio cache should not keep decoded file data that needs timestamp invalidation");
    ok &= require(audioRule.releaseOnOwnerShutdown, "Audio cache should stop and close channels on owner shutdown");
    ok &= require(audioRule.retainOnlyWhileActive, "Audio cache should only retain active playback aliases");
    return ok;
}

bool testAudioPlaybackPlaylistState()
{
    AudioPlaybackService playback;
    AudioPlaylistPlaybackState state;
    state.playlistId = "playlist_state";
    state.playlistName = "Table Queue";
    state.queueAssetIds = {"track_one", "track_two"};
    state.currentAssetId = "track_one";
    state.previousAssetId = "track_zero";
    state.lastAssetId = "track_one";
    state.nextAssetId = "track_two";
    state.currentIndex = 0;
    state.shuffle = true;
    state.repeat = true;
    state.transitionActive = true;
    state.transitionLabel = "Scene transition";

    playback.setPlaylistPlaybackState(state);
    AudioPlaylistPlaybackState snapshot = playback.playlistPlaybackState();

    bool ok = true;
    ok &= require(snapshot.active, "Playlist playback snapshot should become active when published");
    ok &= require(snapshot.playlistName == "Table Queue", "Playlist playback snapshot should preserve the playlist name");
    ok &= require(snapshot.queueAssetIds.size() == 2 && snapshot.previousAssetId == "track_zero" && snapshot.nextAssetId == "track_two", "Playlist playback snapshot should preserve queue and adjacent items");
    ok &= require(snapshot.shuffle && snapshot.repeat && snapshot.transitionActive, "Playlist playback snapshot should preserve playback flags");

    playback.clearPlaylistPlaybackState();
    ok &= require(!playback.playlistPlaybackState().active, "Playlist playback snapshot should clear explicitly");

    playback.setPlaylistPlaybackState(state);
    playback.stopAll();
    ok &= require(!playback.playlistPlaybackState().active, "Stop All should clear playlist playback snapshot");
    return ok;
}

bool importPackageFixtureAudio(Campaign &campaign)
{
    // Generic persistence fixtures deliberately contain a foreign managed path.
    // Transfer fixtures instead import their own one-sample PCM WAV and retain
    // the audio identity used by playlists, ambience layers and triggers.
    const auto source = std::filesystem::current_path() / (campaign.metadata.id + "_audio.wav");
    const unsigned char wave[] = {'R','I','F','F',38,0,0,0,'W','A','V','E','f','m','t',' ',
        16,0,0,0,1,0,1,0,64,31,0,0,128,62,0,0,2,0,16,0,'d','a','t','a',2,0,0,0,0,0};
    {
        std::ofstream file(source, std::ios::binary);
        file.write(reinterpret_cast<const char *>(wave), sizeof(wave));
    }
    ProjectAssetImport::AssetImportRequest request;
    request.kind = ProjectAssetImport::AssetKind::Audio;
    request.sourcePath = source;
    request.assetId = campaign.audioAssets.front().metadata.id;
    const auto imported = ProjectAssetImport::importAsset(campaign, request);
    std::filesystem::remove(source);
    if (imported.success)
    {
        campaign.audioAssets.front().filePath = imported.projectPath;
        campaign.audioAssets.front().originalFileName = imported.originalFileName;
    }
    return require(imported.success, "Package fixture audio import failed: " + imported.errorMessage);
}

bool testProjectPackageExportService()
{
    ProjectSaveService &saveService = ProjectSaveService::getInstance();
    saveService.setBackupSnapshotsEnabled(false);
    saveService.setRecoverySnapshotsEnabled(false);

    Campaign campaign = makeCampaign("package_export");
    const bool audioImported = importPackageFixtureAudio(campaign);

    const std::filesystem::path sourcePath = std::filesystem::current_path() / "export_asset.png";
    {
        std::ofstream file(sourcePath, std::ios::binary);
        file << "export image";
    }
    ProjectAssetImport::AssetImportRequest importRequest;
    importRequest.kind = ProjectAssetImport::AssetKind::Image;
    importRequest.sourcePath = sourcePath;
    importRequest.assetId = "export_image";
    const ProjectAssetImport::AssetImportResult imported = ProjectAssetImport::importAsset(campaign, importRequest);
    campaign.handouts.front().filePath = imported.projectPath;
    const bool saved = saveService.saveCampaign(campaign, "package export test");

    ProjectPackageExport::ExportRequest exportRequest;
    exportRequest.destinationDirectory = std::filesystem::current_path() / "package_exports";
    exportRequest.overwriteExisting = true;
    const ProjectPackageExport::ExportResult exported = ProjectPackageExport::exportCampaignPackage(campaign, exportRequest);

    bool ok = true;
    ok &= require(saved, "Campaign should save before package export");
    ok &= audioImported;
    ok &= require(imported.success, "Package export test asset import should succeed");
    ok &= require(exported.success, "Campaign package export should succeed: " + exported.errorMessage);
    ok &= require(exported.exportPath.extension() == ".ttrpgpkg", "Campaign package export should use the package directory extension");
    ok &= require(std::filesystem::exists(exported.manifestPath), "Campaign package export should include project manifest");
    ok &= require(std::filesystem::exists(exported.databasePath), "Campaign package export should include campaign database snapshot");
    ok &= require(std::filesystem::exists(exported.exportPath / "Assets" / "Images" / "export_image.png"), "Campaign package export should include package assets");
    ok &= require(std::filesystem::exists(exported.exportPath / "Assets" / "Audio" / (campaign.audioAssets.front().metadata.id + ".wav")),
                  "Campaign package export should include its own referenced audio asset");
    Campaign snapshot;
    std::string snapshotError;
    ok &= require(exported.success && CampaignRepository::readCampaignFromSnapshot(exported.databasePath.string(), campaign.metadata.id, snapshot, snapshotError) &&
                      snapshot.audioAssets.size() == 1 && snapshot.audioAssets.front().filePath == campaign.audioAssets.front().filePath &&
                      snapshot.handouts.size() == 1 && snapshot.handouts.front().filePath == imported.projectPath,
                  "Exported campaign should preserve its managed audio and handout references: " + snapshotError);
    ok &= require(exported.fileCount >= 3, "Campaign package export should count copied files");
    ok &= require(exported.byteCount > 0, "Campaign package export should count copied bytes");

    std::filesystem::remove(sourcePath);
    std::filesystem::remove_all(ProjectPackageLayout::campaignPackageDirectory(campaign));
    std::filesystem::remove_all(exportRequest.destinationDirectory);
    return ok;
}

bool testProjectPackageImportService()
{
    ProjectSaveService &saveService = ProjectSaveService::getInstance();
    saveService.setBackupSnapshotsEnabled(false);
    saveService.setRecoverySnapshotsEnabled(false);

    Campaign campaign = makeCampaign("package_import");
    const bool audioImported = importPackageFixtureAudio(campaign);
    WorldPlaceModel packageCapital;
    packageCapital.metadata.id = "package_import_capital";
    packageCapital.type = WorldPlaceType::Capital;
    packageCapital.name = "Package Capital";
    packageCapital.nationId = "package_import_nation";
    packageCapital.linkedMapId = campaign.maps.front().metadata.id;
    packageCapital.linkedFeatureId = campaign.maps.front().features.front().metadata.id;
    campaign.worldPlaces.push_back(packageCapital);
    NationModel packageNation;
    packageNation.metadata.id = "package_import_nation";
    packageNation.name = "Package Nation";
    packageNation.capitalPlaceId = packageCapital.metadata.id;
    NationTerritoryModel packageTerritory;
    packageTerritory.metadata.id = packageNation.metadata.id + "/map/" + campaign.maps.front().metadata.id;
    packageTerritory.polygon = {{0.1, 0.1}, {0.7, 0.1}, {0.6, 0.8}};
    packageNation.territories.push_back(packageTerritory);
    campaign.nations.push_back(packageNation);
    auto &setupMap = campaign.maps.front();
    setupMap.generation.recipe.facts = mapSetupFacts({
        {packageNation.metadata.id, packageNation.name, true, true, {}},
        {packageCapital.metadata.id, packageCapital.name, false, true, packageNation.metadata.id}});
    MapGeneratedFeature setupNation;
    setupNation.id = "setup_nation"; setupNation.kind = MapGeneratedFeatureKind::TerrainRegion;
    setupNation.materialId = "fantasy_neutral.territory.0"; setupNation.name = packageNation.name;
    setupNation.closed = true; setupNation.points = {{1,1},{4,1},{4,4},{1,4}};
    MapGeneratedFeature setupCity;
    setupCity.id = "setup_city"; setupCity.kind = MapGeneratedFeatureKind::Settlement;
    setupCity.name = packageCapital.name; setupCity.points = {{2,2}};
    setupMap.generation.graph.features = {setupNation, setupCity};
    setupMap.metadata.customProperties["gm.context.place"] = packageCapital.metadata.id;
    campaign.localPlayerDisplay.showParty = true;
    campaign.localPlayerDisplay.visibleCharacterIds = {campaign.characters.front().metadata.id};

    const std::filesystem::path sourcePath = std::filesystem::current_path() / "import_asset.pdf";
    {
        std::ofstream file(sourcePath, std::ios::binary);
        file << "import handout";
    }
    ProjectAssetImport::AssetImportRequest importAssetRequest;
    importAssetRequest.kind = ProjectAssetImport::AssetKind::Handout;
    importAssetRequest.sourcePath = sourcePath;
    importAssetRequest.assetId = "import_handout";
    const ProjectAssetImport::AssetImportResult importedAsset = ProjectAssetImport::importAsset(campaign, importAssetRequest);
    campaign.handouts.front().filePath = importedAsset.projectPath;
    const bool saved = saveService.saveCampaign(campaign, "package import test");

    ProjectPackageExport::ExportRequest exportRequest;
    exportRequest.destinationDirectory = std::filesystem::current_path() / "package_import_exports";
    exportRequest.overwriteExisting = true;
    const ProjectPackageExport::ExportResult exported = ProjectPackageExport::exportCampaignPackage(campaign, exportRequest);

    std::filesystem::remove_all(ProjectPackageLayout::campaignPackageDirectory(campaign));

    const ProjectPackageImport::ImportPreview preview = ProjectPackageImport::previewCampaignPackage(exported.exportPath);
    const ProjectPackageImport::ImportPreview quotedPathPreview =
        ProjectPackageImport::previewCampaignPackage(std::filesystem::path("\"" + exported.exportPath.string() + "\""));
    ProjectPackageImport::ImportRequest rejectedRequest;
    rejectedRequest.packagePath = exported.exportPath;
    const ProjectPackageImport::ImportResult rejectedConflict = ProjectPackageImport::importCampaignPackage(rejectedRequest);
    const bool rejectedImportLeftPackageAbsent = !std::filesystem::exists(ProjectPackageLayout::campaignPackageDirectory(campaign));

    ProjectPackageImport::ImportRequest importRequest;
    importRequest.packagePath = exported.exportPath;
    importRequest.conflictResolution = ProjectPackageImport::ConflictResolution::ReplaceExisting;
    const ProjectPackageImport::ImportResult importedPackage = ProjectPackageImport::importCampaignPackage(importRequest);

    ProjectPackageImport::ImportRequest cloneRequest;
    cloneRequest.packagePath = exported.exportPath;
    cloneRequest.conflictResolution = ProjectPackageImport::ConflictResolution::CloneAsCopy;
    cloneRequest.cloneCampaignName = campaign.name + " Copy";
    const ProjectPackageImport::ImportResult clonedPackage = ProjectPackageImport::importCampaignPackage(cloneRequest);

    const std::filesystem::path unsafePackagePath = std::filesystem::current_path() / "unsafe_import.ttrpgpkg";
    std::filesystem::create_directories(unsafePackagePath);
    ProjectManifest::ProjectManifestModel unsafeManifest = importedPackage.manifest;
    unsafeManifest.databasePath = "../TTRPG.db";
    {
        std::ofstream unsafeManifestFile(unsafePackagePath / "project.json", std::ios::binary | std::ios::trunc);
        unsafeManifestFile << ProjectManifest::serializeManifest(unsafeManifest);
    }
    const ProjectPackageImport::ImportPreview unsafePreview = ProjectPackageImport::previewCampaignPackage(unsafePackagePath);

    bool ok = true;
    ok &= require(saved, "Campaign should save before package import test");
    ok &= audioImported;
    ok &= require(importedAsset.success, "Package import test asset import should succeed");
    ok &= require(exported.success, "Campaign package export should succeed before import: " + exported.errorMessage);
    ok &= require(preview.valid, "Package import preview should validate a complete package: " + preview.errorMessage);
    ok &= require(quotedPathPreview.valid, "Package import preview should accept a quoted pasted folder path");
    ok &= require(preview.repositoryConflict, "Package import preview should report an existing repository campaign");
    ok &= require(!rejectedConflict.success, "Package import should reject a conflict without explicit replacement");
    ok &= require(rejectedImportLeftPackageAbsent, "Rejected import should not create or replace package files");
    ok &= require(importedPackage.success, "Campaign package import should succeed: " + importedPackage.errorMessage);
    ok &= require(importedPackage.manifest.campaignId == campaign.metadata.id, "Imported package manifest campaign ID should round trip");
    ok &= require(importedPackage.importedPackagePath == ProjectPackageLayout::campaignPackageDirectory(campaign), "Imported package should land in the campaign package folder");
    ok &= require(std::filesystem::exists(importedPackage.importedPackagePath / "project.json"), "Imported package should include project manifest");
    ok &= require(std::filesystem::exists(importedPackage.importedPackagePath / "campaign.db"), "Imported package should include campaign database snapshot");
    ok &= require(std::filesystem::exists(importedPackage.importedPackagePath / "Assets" / "Handouts" / "import_handout.pdf"), "Imported package should include handout assets");
    ok &= require(std::filesystem::exists(importedPackage.importedPackagePath / "Assets" / "Audio" / (campaign.audioAssets.front().metadata.id + ".wav")),
                  "Imported package should include its campaign-owned audio asset");
    ok &= require(importedPackage.restoredToRepository, "Imported campaign should be restored into the active repository");
    ok &= require(importedPackage.importedCampaignName == campaign.name, "Imported repository campaign name should match the manifest");
    ok &= require(std::filesystem::exists(importedPackage.backupPath / "repository-before-import.db"), "Replacing a repository campaign should create a database backup first");
    const std::vector<Campaign> summariesAfterImport = CampaignRepository::loadCampaignSummaries();
    ok &= require(std::any_of(summariesAfterImport.begin(), summariesAfterImport.end(), [&](const Campaign &summary) {
        return summary.metadata.id == campaign.metadata.id && summary.name == campaign.name;
    }), "Imported campaign should immediately appear in repository summaries");
    ok &= require(importedPackage.fileCount >= 3, "Imported package should count copied files");
    ok &= require(importedPackage.byteCount > 0, "Imported package should count copied bytes");
    ok &= require(!unsafePreview.valid, "Package preview should reject a database path that escapes the package folder");
    ok &= require(clonedPackage.success, "Conflicting package should import as an independent copy: " + clonedPackage.errorMessage);
    ok &= require(clonedPackage.importedAsCopy, "Clone conflict handling should report an imported copy");
    ok &= require(clonedPackage.manifest.campaignId != campaign.metadata.id, "Imported copy should receive a new campaign stable ID");
    ok &= require(clonedPackage.importedCampaignName == cloneRequest.cloneCampaignName, "Imported copy should use the requested unique name");
    Campaign importedOriginal;
    Campaign importedClone;
    for (const Campaign &savedCampaign : CampaignRepository::loadAllCampaigns())
    {
        if (savedCampaign.metadata.id == campaign.metadata.id)
            importedOriginal = savedCampaign;
        if (savedCampaign.metadata.id == clonedPackage.manifest.campaignId)
            importedClone = savedCampaign;
    }
    ok &= require(!importedOriginal.characters.empty() && !importedClone.characters.empty(), "Original and copied campaigns should retain characters");
    if (!importedOriginal.characters.empty() && !importedClone.characters.empty())
    {
        ok &= require(importedOriginal.characters.front().metadata.id != importedClone.characters.front().metadata.id,
                      "Imported copy should remap child entity IDs instead of taking ownership from the original");
        if (!importedClone.sessions.empty() && !importedClone.sessions.front().participantCharacterIds.empty())
            ok &= require(importedClone.sessions.front().participantCharacterIds.front() == importedClone.characters.front().metadata.id,
                          "Imported copy should preserve internal references after ID remapping");
        ok &= require(importedClone.localPlayerDisplay.showParty &&
                          importedClone.localPlayerDisplay.visibleCharacterIds.size() == 1 &&
                          importedClone.localPlayerDisplay.visibleCharacterIds.front() == importedClone.characters.front().metadata.id,
                      "Imported copy should remap explicit player party selections to cloned characters");
        if (!importedClone.sessions.empty() && !importedClone.sessions.front().planEntries.empty() && !importedClone.storyThreads.empty())
        {
            const auto &copied = importedClone.sessions.front();
            ok &= require(copied.planEntries.front().metadata.id != importedOriginal.sessions.front().planEntries.front().metadata.id &&
                          copied.planEntries.front().storyBeatId == importedClone.storyThreads.front().beats.front().metadata.id &&
                          copied.liveState.activePlanEntryId == copied.planEntries.front().metadata.id,
                          "Imported copy must remap plan entries, shared scenes and live cursor together");
        }
    }
    ok &= require(importedOriginal.diceMacros.size() == 1 && importedClone.diceMacros.size() == 1 &&
                      importedOriginal.storyObjectives.size() == 2 && importedClone.storyObjectives.size() == 2 &&
                      importedOriginal.storyClues.size() == 1 && importedClone.storyClues.size() == 1 &&
                      importedOriginal.safetyProfile.boundaries.size() == 1 && importedClone.safetyProfile.boundaries.size() == 1 &&
                      importedOriginal.safetyProfile.warnings.size() == 1 && importedClone.safetyProfile.warnings.size() == 1,
                  "Independent import must retain every toolkit record in both the original and copy");
    if (importedOriginal.storyObjectives.size() == 2 && importedClone.storyObjectives.size() == 2 &&
        !importedOriginal.characters.empty() && !importedClone.characters.empty() &&
        !importedClone.storyThreads.empty() && !importedClone.storyThreads.front().beats.empty())
    {
        ok &= require(importedOriginal.diceMacros.front().metadata.id != importedClone.diceMacros.front().metadata.id &&
                          importedOriginal.storyObjectives.front().metadata.id != importedClone.storyObjectives.front().metadata.id &&
                          importedOriginal.storyClues.front().metadata.id != importedClone.storyClues.front().metadata.id &&
                          importedOriginal.safetyProfile.boundaries.front().metadata.id != importedClone.safetyProfile.boundaries.front().metadata.id &&
                          importedOriginal.safetyProfile.warnings.front().metadata.id != importedClone.safetyProfile.warnings.front().metadata.id,
                      "Independent import must give toolkit rows distinct stable identities");
        ok &= require(importedClone.storyObjectives.front().linkedStoryThreadId == importedClone.storyThreads.front().metadata.id &&
                          importedClone.storyObjectives.front().linkedStoryBeatId == importedClone.storyThreads.front().beats.front().metadata.id &&
                          importedClone.storyObjectives.front().assigneeCharacterIds == std::vector<EntityId>{importedClone.characters.front().metadata.id} &&
                          importedClone.storyObjectives[1].prerequisiteObjectiveIds == std::vector<EntityId>{importedClone.storyObjectives.front().metadata.id} &&
                          importedClone.storyClues.front().sourceEntityId == importedClone.characters.front().metadata.id &&
                          importedClone.safetyProfile.warnings.front().targetEntityId == importedClone.characters.front().metadata.id,
                      "Independent import must remap toolkit relationships to copied entities");
    }
    ok &= require(importedOriginal.nations.size() == 1 && importedClone.nations.size() == 1 &&
                      importedOriginal.worldPlaces.size() == 1 && importedClone.worldPlaces.size() == 1,
                  "Original and copied campaigns should retain Atlas world objects");
    if (!importedClone.nations.empty() && !importedClone.worldPlaces.empty())
    {
        ok &= require(importedClone.nations.front().metadata.id != importedOriginal.nations.front().metadata.id &&
                          importedClone.worldPlaces.front().metadata.id != importedOriginal.worldPlaces.front().metadata.id,
                      "Imported copy should remap nation and world-place stable IDs");
        ok &= require(importedClone.maps.front().metadata.customProperties.count("gm.context.place") == 1 &&
                          importedClone.maps.front().metadata.customProperties.at("gm.context.place") == importedClone.worldPlaces.front().metadata.id &&
                          importedOriginal.maps.front().metadata.customProperties.count("gm.context.place") == 1 &&
                          importedOriginal.maps.front().metadata.customProperties.at("gm.context.place") == importedOriginal.worldPlaces.front().metadata.id,
                      "Package transfer must persist contextual links and remap copied targets to the copied campaign");
        ok &= require(importedClone.nations.front().capitalPlaceId == importedClone.worldPlaces.front().metadata.id &&
                          importedClone.worldPlaces.front().nationId == importedClone.nations.front().metadata.id,
                      "Imported copy should preserve nation/capital references after ID remapping");
        ok &= require(importedClone.worldPlaces.front().linkedMapId == importedClone.maps.front().metadata.id &&
                          importedClone.worldPlaces.front().linkedFeatureId == importedClone.maps.front().features.front().metadata.id,
                      "Imported copy should preserve world-place map and feature references after ID remapping");
        ok &= require(!importedClone.nations.front().territories.empty() &&
                          importedClone.nations.front().territories.front().metadata.id != packageTerritory.metadata.id,
                      "Imported copy should remap nation territory stable IDs");
        const auto bindings = mapSetupEntities(importedClone.maps.front().generation.recipe.facts);
        ok &= require(bindings.size() == 2 && bindings[0].id == importedClone.nations.front().metadata.id &&
                      bindings[1].id == importedClone.worldPlaces.front().metadata.id &&
                      bindings[1].nationId == importedClone.nations.front().metadata.id,
                      "Cloning must remap structured map selections and city-nation relationships");
        ok &= require(applyMapSetupEntities(importedClone, importedClone.maps.front()) &&
                      importedClone.nations.front().territories.size() == 1 && importedClone.worldPlaces.size() == 1,
                      "Saving a cloned structured map must preserve its territory identity and not duplicate campaign entities");
    }

    std::string backupListError;
    const std::vector<ProjectBackupRestore::BackupEntry> backups = ProjectBackupRestore::listBackups(backupListError);
    const auto importBackup = std::find_if(backups.begin(), backups.end(), [&](const ProjectBackupRestore::BackupEntry &entry) {
        return entry.snapshotPath == importedPackage.backupPath / "repository-before-import.db" &&
               entry.campaignId == campaign.metadata.id;
    });
    ok &= require(backupListError.empty(), "Managed backup listing should complete without errors: " + backupListError);
    ok &= require(importBackup != backups.end(), "Import replacement backup should appear in the managed backup browser");
    ProjectBackupRestore::RestoreResult restoredBackup;
    if (importBackup != backups.end())
    {
        importedOriginal.description = "Changed after import backup.";
        CampaignRepository::saveCampaign(importedOriginal);
        restoredBackup = ProjectBackupRestore::restoreBackup(*importBackup);
        ok &= require(restoredBackup.success, "Managed backup restore should succeed: " + restoredBackup.errorMessage);
        Campaign restoredCampaign;
        for (const Campaign &savedCampaign : CampaignRepository::loadAllCampaigns())
            if (savedCampaign.metadata.id == campaign.metadata.id)
                restoredCampaign = savedCampaign;
        ok &= require(restoredCampaign.description == campaign.description, "Backup restore should restore the selected campaign data");
        ok &= require(std::filesystem::exists(restoredBackup.safetyBackupPath / "repository-before-restore.db"),
                      "Backup restore should create a repository safety snapshot before mutation");
    }

    std::filesystem::remove(sourcePath);
    std::filesystem::remove_all(ProjectPackageLayout::campaignPackageDirectory(campaign));
    std::filesystem::remove_all(clonedPackage.importedPackagePath);
    std::filesystem::remove_all(importedPackage.backupPath);
    if (!restoredBackup.safetyBackupPath.empty())
        std::filesystem::remove_all(mapFileIoPath(restoredBackup.safetyBackupPath));
    std::filesystem::remove_all(unsafePackagePath);
    std::filesystem::remove_all(exportRequest.destinationDirectory);
    return ok;
}

bool testProjectAssetCollectionService()
{
    Campaign campaign;
    campaign.metadata.id = "asset collection campaign";

    const std::filesystem::path portraitPath = std::filesystem::current_path() / "collect_portrait.png";
    const std::filesystem::path mapPath = std::filesystem::current_path() / "collect_map.jpg";
    const std::filesystem::path audioPath = std::filesystem::current_path() / "collect_audio.wav";
    const std::filesystem::path handoutPath = std::filesystem::current_path() / "collect_handout.pdf";
    for (const std::filesystem::path &path : {portraitPath, mapPath, audioPath, handoutPath})
    {
        std::ofstream file(path, std::ios::binary);
        file << "collect " << path.filename().string();
    }

    CharacterModel character;
    character.metadata.id = "collect_character";
    std::snprintf(character.nameBuffer, sizeof(character.nameBuffer), "%s", "Collect Hero");
    character.portraitAssetId = portraitPath.string();
    campaign.characters.push_back(character);

    MapModel map;
    map.metadata.id = "collect_map";
    map.name = "Collect Map";
    map.backgroundImagePath = mapPath.string();
    campaign.maps.push_back(map);

    AudioAssetModel audio;
    audio.metadata.id = "collect_audio";
    audio.name = "Collect Audio";
    audio.filePath = audioPath.string();
    campaign.audioAssets.push_back(audio);

    HandoutModel handout;
    handout.metadata.id = "collect_handout";
    handout.title = "Collect Handout";
    handout.filePath = handoutPath.string();
    campaign.handouts.push_back(handout);

    const ProjectAssetCollection::CollectionResult collected = ProjectAssetCollection::collectAssetsIntoCampaignFolder(campaign);

    bool ok = true;
    ok &= require(collected.totalReferences == 4, "Asset collection should inspect all known asset references");
    ok &= require(collected.collectedAssets == 4, "Asset collection should copy loose assets into the campaign package");
    ok &= require(collected.missingAssets == 0 && collected.errors.empty(), "Asset collection should not report errors for valid assets");
    ok &= require(campaign.characters.front().portraitAssetId.find("Resources/Campaigns/asset_collection_campaign/Assets/Portraits/collect_character.png") != std::string::npos, "Asset collection should rewrite portrait paths");
    ok &= require(campaign.maps.front().backgroundImagePath.find("Resources/Campaigns/asset_collection_campaign/Assets/Maps/collect_map_background.jpg") != std::string::npos, "Asset collection should rewrite map background paths");
    ok &= require(campaign.audioAssets.front().filePath.find("Resources/Campaigns/asset_collection_campaign/Assets/Audio/collect_audio.wav") != std::string::npos, "Asset collection should rewrite audio paths");
    ok &= require(campaign.handouts.front().filePath.find("Resources/Campaigns/asset_collection_campaign/Assets/Handouts/collect_handout.pdf") != std::string::npos, "Asset collection should rewrite handout paths");
    ok &= require(std::filesystem::exists(ProjectAssets::resolveProjectPath(campaign.audioAssets.front().filePath)), "Collected audio copy should exist");

    const ProjectAssetCollection::CollectionResult secondPass = ProjectAssetCollection::collectAssetsIntoCampaignFolder(campaign);
    ok &= require(secondPass.skippedAlreadyPackaged == 4 && secondPass.collectedAssets == 0, "Asset collection should skip already packaged assets");

    std::filesystem::remove(portraitPath);
    std::filesystem::remove(mapPath);
    std::filesystem::remove(audioPath);
    std::filesystem::remove(handoutPath);
    std::filesystem::remove_all(ProjectPackageLayout::campaignPackageDirectory(campaign));
    return ok;
}

bool testProjectStorageService()
{
    Campaign campaign;
    campaign.metadata.id = "storage service campaign";
    ProjectPackageLayout::ensureCampaignPackageLayout(campaign);
    const ProjectPackageLayout::CampaignPackageLayout layout = ProjectPackageLayout::layoutForCampaign(campaign);

    {
        std::ofstream file(layout.databasePath, std::ios::binary);
        file << "database bytes";
    }
    {
        std::ofstream file(layout.manifestPath, std::ios::binary);
        file << "manifest";
    }
    {
        std::ofstream file(layout.assetsDirectory / "Images" / "image.bin", std::ios::binary);
        file << "image-bytes";
    }
    {
        std::ofstream file(layout.assetsDirectory / "Files" / "empty.tmp", std::ios::binary);
    }
    std::filesystem::create_directories(layout.assetsDirectory / "EmptyFolder" / "Nested");

    const ProjectStorage::StorageUsageReport report = ProjectStorage::measureCampaignStorage(campaign);
    ProjectStorage::CleanupOptions cleanupOptions;
    cleanupOptions.removeEmptyDirectories = true;
    cleanupOptions.removeZeroByteFiles = true;
    const ProjectStorage::CleanupResult cleanup = ProjectStorage::cleanupCampaignStorage(campaign, cleanupOptions);
    const ProjectStorage::StorageUsageReport cleanedReport = ProjectStorage::measureCampaignStorage(campaign);

    bool ok = true;
    ok &= require(report.totalFiles == 4, "Storage usage should count package files");
    ok &= require(report.totalBytes == 33, "Storage usage should sum package bytes");
    ok &= require(report.buckets.size() == 6, "Storage usage should report all package buckets");
    ok &= require(cleanup.removedFiles == 1, "Storage cleanup should remove requested zero-byte files");
    ok &= require(cleanup.removedDirectories >= 2, "Storage cleanup should remove empty nested directories");
    ok &= require(!std::filesystem::exists(layout.assetsDirectory / "Files" / "empty.tmp"), "Storage cleanup should delete zero-byte package files");
    ok &= require(!std::filesystem::exists(layout.assetsDirectory / "EmptyFolder" / "Nested"), "Storage cleanup should delete empty nested folders");
    ok &= require(cleanedReport.totalFiles == 3, "Storage usage should reflect cleaned package files");

    std::filesystem::remove_all(ProjectPackageLayout::campaignPackageDirectory(campaign));
    return ok;
}

bool testProjectStructureGuardrails()
{
    std::filesystem::path sourceRoot = std::filesystem::absolute(std::filesystem::path(__FILE__)).parent_path().parent_path();
    if (!std::filesystem::exists(sourceRoot / "CMakeLists.txt"))
    {
        sourceRoot = std::filesystem::current_path();
        while (!sourceRoot.empty() && !std::filesystem::exists(sourceRoot / "CMakeLists.txt"))
        {
            if (sourceRoot == sourceRoot.parent_path())
                break;
            sourceRoot = sourceRoot.parent_path();
        }
    }

    const auto hasDirectoryNamed = [](const std::filesystem::path &parent, const std::string &name) {
        if (!std::filesystem::exists(parent))
            return false;
        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(parent))
        {
            if (entry.is_directory() && entry.path().filename().string() == name)
                return true;
        }
        return false;
    };
    const auto treeContainsText = [](const std::filesystem::path &root, const std::string &needle) {
        if (!std::filesystem::exists(root))
            return false;
        for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(root))
        {
            if (!entry.is_regular_file())
                continue;
            const std::string extension = entry.path().extension().string();
            if (extension != ".cpp" && extension != ".hpp")
                continue;
            std::ifstream file(entry.path());
            const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (content.find(needle) != std::string::npos)
                return true;
        }
        return false;
    };

    const std::filesystem::path controllerWidgets = sourceRoot / "Controllers" / "Editor" / "Widgets";
    const std::filesystem::path viewWidgets = sourceRoot / "Views" / "Editor" / "Widgets";

    bool ok = true;
    ok &= require(hasDirectoryNamed(controllerWidgets, "PlayerCreator"), "PlayerCreator controller folder should remain the supported character creation path");
    ok &= require(!hasDirectoryNamed(controllerWidgets, "CharacterCreator"), "Old CharacterCreator controller folder should not return");
    ok &= require(hasDirectoryNamed(viewWidgets, "Npc"), "Npc view folder casing should remain exact for Linux portability");
    ok &= require(!hasDirectoryNamed(viewWidgets, "NPC"), "Uppercase NPC view folder should not be introduced beside Npc");
    ok &= require(!treeContainsText(sourceRoot / "Controllers", "DataBaseManager.hpp"), "Controllers should route persistence through services/repositories instead of including DataBaseManager");
    ok &= require(!treeContainsText(sourceRoot / "Views", "DataBaseManager.hpp"), "Views should not include DataBaseManager");
    return ok;
}

bool testLegacySurfaceRetirementRegistry()
{
    using namespace gm::ux;

    const auto &registry = legacySurfaceRegistry();
    const LegacySurfaceRegistryValidation validation = validateLegacySurfaceRegistry();
    bool ok = true;
    ok &= require(validation.valid, "Legacy surface retirement registry must remain internally valid");
    ok &= require(registry.size() == 28, "Every audited legacy UI surface must have an explicit retirement decision");

    std::size_t adapterCount = 0;
    std::size_t retiredCount = 0;
    std::size_t detachedCount = 0;
    for (const auto &surface : registry)
    {
        ok &= require(surface.persistedDataPreserved, std::string(surface.routeId) + " must preserve persisted data during UI retirement");
        ok &= require(!isLegacyEntryPointAllowed(surface, LegacyEntryPoint::PrimaryNavigation),
                      std::string(surface.routeId) + " must not return to primary navigation");
        ok &= require(!isLegacyEntryPointAllowed(surface, LegacyEntryPoint::UniversalSearch),
                      std::string(surface.routeId) + " must not be exposed as a duplicate command destination");
        switch (surface.disposition)
        {
        case LegacySurfaceDisposition::ContextualAdapter:
            ++adapterCount;
            ok &= require(isLegacyEntryPointAllowed(surface, LegacyEntryPoint::WorkspaceContext),
                          std::string(surface.routeId) + " contextual adapter must be available only from its replacement workspace");
            break;
        case LegacySurfaceDisposition::RetiredNavigation:
            ++retiredCount;
            ok &= require(!isLegacyEntryPointAllowed(surface, LegacyEntryPoint::WorkspaceContext),
                          std::string(surface.routeId) + " retired screen must not remain independently routable");
            break;
        case LegacySurfaceDisposition::DetachedOutput:
            ++detachedCount;
            ok &= require(isLegacyEntryPointAllowed(surface, LegacyEntryPoint::WorkspaceContext),
                          std::string(surface.routeId) + " detached output must be controlled from Live");
            break;
        }
    }

    ok &= require(adapterCount == 13 && retiredCount == 14 && detachedCount == 1,
                  "Legacy surface disposition totals changed without updating the audited retirement contract");
    ok &= require(findLegacySurface("show:inventory_manager")->disposition == LegacySurfaceDisposition::RetiredNavigation,
                  "Inventory Manager must remain contextual data, not top-level navigation");
    ok &= require(findLegacySurface("show:dungeon_builder")->replacementWorkspace == WorkspaceTarget::World,
                  "Dungeon structures must remain available through World");
    ok &= require(findLegacySurface("show:battlefield_creator")->replacementWorkspace == WorkspaceTarget::Encounters,
                  "Battlefield setup must remain available through Encounters");
    ok &= require(findLegacySurface("show:battle_tracker")->replacementWorkspace == WorkspaceTarget::Live,
                  "Prepared combat must transfer into Live");
    ok &= require(findLegacySurface("show:live_session_dashboard")->disposition == LegacySurfaceDisposition::RetiredNavigation,
                  "Live Dashboard must remain embedded in the stable Live workspace");
    ok &= require(findLegacySurface("show:gm_screen")->disposition == LegacySurfaceDisposition::RetiredNavigation,
                  "GM Screen controls must remain embedded in the stable Live workspace");
    ok &= require(findLegacySurface("show:local_player_display")->disposition == LegacySurfaceDisposition::DetachedOutput,
                  "Local Player Display must remain the supported detachable player-safe output");
    ok &= require(!isLegacyEntryPointAllowed("unknown-route", LegacyEntryPoint::WorkspaceContext),
                  "Unknown legacy routes must fail closed");

    const WorkspaceTarget workspaces[] = {
        WorkspaceTarget::Home, WorkspaceTarget::Story, WorkspaceTarget::Cast, WorkspaceTarget::World,
        WorkspaceTarget::Encounters, WorkspaceTarget::Compendium, WorkspaceTarget::Live, WorkspaceTarget::Settings};
    for (const WorkspaceTarget workspace : workspaces)
    {
        ok &= require(!legacySurfacesFor(workspace).empty(),
                      std::string(workspaceTargetName(workspace)) + " must own every related legacy capability explicitly");
    }
    return ok;
}

bool testMapGenerationMetadataRoundTrip()
{
    MapGenerationMetadata source;
    source.enabled = true;
    source.continuousRendering = true;
    source.recipe.generatorVersion = 3;
    source.recipe.recipeVersion = 7;
    source.recipe.recipeId = "river_valley";
    source.recipe.styleId = "fantasy_neutral";
    source.recipe.seed = 424242u;
    source.recipe.purpose = MapGenerationPurpose::TravelArea;
    source.recipe.landscape = MapLandscapeProfile::RiverValley;
    source.recipe.styleVersion = 2;
    source.recipe.scale = MapLevel::Region;
    source.recipe.quality = MapGenerationQualityLevel::Polished;
    source.recipe.creationIntent = MapCreationIntent::TravelScene;
    source.recipe.sizePreset = MapSizePreset::Large;
    source.recipe.contentLanguage = "de";
    source.recipe.nameCulture = "germanic_central_european";
    source.recipe.allowAdditionalNations = true;
    source.recipe.knownFacts = {"Three nations"};
    source.recipe.requiredElements = {"Typo"};
    source.recipe.optionalElements = {"Old road"};
    source.recipe.excludedElements = {"volcano"};
    source.recipe.lockedFeatureIds = {"anchor_town"};
    source.recipe.constraints.push_back({"anchor_town", "Town", MapGeneratedFeatureKind::Settlement, 8.5f, 11.5f, true, true});
    MapGenerationFact sharedRiverFact;
    sharedRiverFact.id = "fact_shared_river";
    sharedRiverFact.type = MapGenerationFactType::SharedRiver;
    sharedRiverFact.strength = MapGenerationFactStrength::Must;
    sharedRiverFact.label = "Gemeinsamer großer Fluss";
    sharedRiverFact.entityRefs = {"nation_a", "nation_b", "nation_c"};
    sharedRiverFact.sourceText = "Drei Nationen teilen sich einen großen Fluss.";
    sharedRiverFact.confirmed = true;
    source.recipe.facts.push_back(sharedRiverFact);
    MapGeneratedFeature road;
    road.id = "road_1";
    road.kind = MapGeneratedFeatureKind::Road;
    road.name = "Old Road";
    road.materialId = "fantasy_neutral.road.dirt";
    road.width = 0.8f;
    road.discovered = false;
    road.gmOnly = true;
    road.points = {{1.5f, 2.5f}, {7.5f, 9.5f}};
    source.graph.features.push_back(road);

    source.recipe.travelScenePurpose = 3; source.recipe.travelSceneBiome = "Mountain pass";
    source.recipe.travelSceneDangerLevel = 4; source.recipe.travelSceneCombatReady = true;
    source.recipe.travelSceneSeed = 0x1234abcd;
    source.recipe.landform = MapLandform::Continental;
    source.recipe.waterLevel = 31; source.recipe.forestDensity = 0; source.recipe.mountainDensity = 48;
    source.recipe.settlementDensity = 61; source.recipe.routeDensity = 22; source.recipe.tacticalDensity = 7;
    const std::string serialized = PersistenceSerialization::serializeMapGenerationMetadata(source);
    const MapGenerationMetadata restored = PersistenceSerialization::deserializeMapGenerationMetadata(serialized);
    bool ok = true;
    ok &= require(restored.recipe.travelScenePurpose == 3 && restored.recipe.travelSceneBiome == "Mountain pass" &&
        restored.recipe.travelSceneDangerLevel == 4 && restored.recipe.travelSceneCombatReady && restored.recipe.travelSceneSeed == 0x1234abcd,
        "Structured travel purpose, biome and requested seed must survive persistence");
    ok &= require(restored.recipe.landform == MapLandform::Continental && restored.recipe.waterLevel == 31 &&
        restored.recipe.forestDensity == 0 && restored.recipe.mountainDensity == 48 &&
        restored.recipe.settlementDensity == 61 && restored.recipe.routeDensity == 22 && restored.recipe.tacticalDensity == 7,
        "Structured map geography and exact zero/nonzero sliders must survive persistence");
    ok &= require(restored.enabled && restored.continuousRendering, "Map V3 rendering metadata should survive serialization");
    ok &= require(restored.recipe.generatorVersion == 3 && restored.recipe.recipeVersion == 7 &&
                      restored.recipe.seed == source.recipe.seed,
                  "Map V3 recipe identity should survive serialization");
    ok &= require(restored.recipe.styleVersion == 2 && restored.recipe.scale == MapLevel::Region && restored.recipe.quality == MapGenerationQualityLevel::Polished,
                  "Map V3 scale, style, and quality should survive serialization");
    ok &= require(restored.recipe.knownFacts == source.recipe.knownFacts && restored.recipe.requiredElements == source.recipe.requiredElements &&
                      restored.recipe.optionalElements == source.recipe.optionalElements && restored.recipe.excludedElements == source.recipe.excludedElements &&
                      restored.recipe.lockedFeatureIds == source.recipe.lockedFeatureIds,
                  "Map V3 fact, element, exclusion, and lock lists should survive serialization");
    ok &= require(restored.recipe.constraints.size() == 1 && restored.recipe.constraints.front().locked, "Map V2 locked constraints should survive serialization");
    ok &= require(restored.recipe.creationIntent == MapCreationIntent::TravelScene && restored.recipe.sizePreset == MapSizePreset::Large &&
                      restored.recipe.contentLanguage == "de" && restored.recipe.nameCulture == "germanic_central_european" &&
                      restored.recipe.allowAdditionalNations,
                  "Map V5 creation request settings should survive serialization");
    ok &= require(restored.recipe.facts.size() == 1 && restored.recipe.facts.front().type == MapGenerationFactType::SharedRiver &&
                      restored.recipe.facts.front().strength == MapGenerationFactStrength::Must && restored.recipe.facts.front().entityRefs.size() == 3,
                  "Typed map generation facts should survive serialization");
    ok &= require(restored.graph.features.size() == 1 && restored.graph.features.front().points.size() == 2, "Map V2 feature geometry should survive serialization");
    ok &= require(!restored.graph.features.front().discovered && restored.graph.features.front().gmOnly,
                  "Generated feature player-visibility flags should survive serialization");
    return ok;
}

bool testMapPngExport()
{
    MapModel presetMap;
    presetMap.width = 72;
    presetMap.height = 48;
    const int screenDetail = mapPngPixelsPerCellForPreset(
        presetMap, MapPngExportSizePreset::Screen);
    const int ultraHdDetail = mapPngPixelsPerCellForPreset(
        presetMap, MapPngExportSizePreset::UltraHd4K);
    const MapPngExportSizing screenSizing = calculateMapPngExportSizing(presetMap, screenDetail);
    const MapPngExportSizing ultraHdSizing = calculateMapPngExportSizing(presetMap, ultraHdDetail);
    bool ok = require(screenDetail == 22 && screenSizing.valid &&
                          screenSizing.widthPixels == 1584 && screenSizing.heightPixels == 1056,
                      "Screen PNG preset should fit the complete map inside 1920x1080 without cropping");
    ok &= require(ultraHdDetail == 45 && ultraHdSizing.valid &&
                      ultraHdSizing.widthPixels == 3240 && ultraHdSizing.heightPixels == 2160,
                  "4K PNG preset should fit the complete map inside 3840x2160 without cropping");
    ok &= require(mapPngPixelsPerCellForPreset(
                      presetMap, MapPngExportSizePreset::Tactical100PixelsPerCell) == 100,
                  "Tactical PNG preset should use exactly 100 pixels per cell");
    ok &= require(screenSizing.uncompressedBytes ==
                      static_cast<std::uint64_t>(1584) * 1056ull * 4ull,
                  "PNG sizing should report the exact uncompressed RGBA byte count");

    MapModel oversizedMap;
    oversizedMap.width = 200;
    oversizedMap.height = 100;
    const MapPngExportSizing oversizedSizing = calculateMapPngExportSizing(oversizedMap, 100);
    const MapPngExportSizing invalidDetail = calculateMapPngExportSizing(presetMap, 7);
    ok &= require(!oversizedSizing.valid && oversizedSizing.error ==
                      "PNG export would exceed the maximum edge of 16384 pixels.",
                  "Oversized PNG requests must be rejected instead of silently downscaled");
    ok &= require(!invalidDetail.valid && invalidDetail.error ==
                      "PNG export detail must be between 8 and 128 pixels per cell.",
                  "Out-of-range custom PNG detail should produce a clear validation error");
    MapPngExportOptions oversizedOptions;
    oversizedOptions.pixelsPerCell = 100;
    std::string oversizedError;
    const std::filesystem::path oversizedPath = "map_oversized_export_must_not_exist.png";
    std::filesystem::remove(oversizedPath);
    ok &= require(!exportMapPng(oversizedMap, oversizedPath, oversizedOptions, &oversizedError) &&
                      !std::filesystem::exists(oversizedPath) &&
                      oversizedError == oversizedSizing.error,
                  "PNG exporter must fail before writing when an exact request exceeds 16384 pixels");
    const MapPngTiledExportPlan oversizedTilePlan = planMapPngTiledExport(
        oversizedMap, 100, "oversized_map");
    ok &= require(oversizedTilePlan.valid && oversizedTilePlan.columns == 2 &&
                      oversizedTilePlan.rows == 1 && oversizedTilePlan.tiles.size() == 2,
                  "Oversized PNG requests should have a deterministic two-tile export plan");
    ok &= require(std::all_of(oversizedTilePlan.tiles.begin(), oversizedTilePlan.tiles.end(),
                             [](const MapPngExportTile &tile) {
                                 return tile.widthPixels <= kMapPngMaximumExportEdge &&
                                        tile.heightPixels <= kMapPngMaximumExportEdge &&
                                        tile.content.width > 0 && tile.content.height > 0;
                             }),
                  "Every tiled PNG must stay inside the encoder edge limit and contain map content");
    ok &= require(!planMapPngTiledExport(oversizedMap, 100, "../unsafe").valid,
                  "Tiled PNG file stems must not permit path traversal");

    MapModel map;
    map.width = 8;
    map.height = 6;
    map.atlasLevel = MapLevel::Site;
    map.visibility = MapVisibility::PlayersVisible;
    resizeMapCells(map);
    map.playerRelease.waysAndRoutes = true;
    map.playerRelease.labels = true;
    map.playerRelease.tokensAndAnnotations = true;
    map.generation.enabled = true;
    map.generation.continuousRendering = true;
    map.generation.recipe.generatorVersion = 2;
    MapGeneratedFeature road;
    road.id = "road_export";
    road.kind = MapGeneratedFeatureKind::Road;
    road.width = 0.8f;
    road.gmOnly = true;
    road.points = {{0.5f, 2.5f}, {7.5f, 3.5f}};
    map.generation.graph.features.push_back(road);
    MapGeneratedFeature generatedGate;
    generatedGate.id = "generated_city_gate";
    generatedGate.kind = MapGeneratedFeatureKind::Settlement;
    generatedGate.name = "City Gate";
    generatedGate.points = {{4.5f, 1.5f}};
    map.generation.graph.features.push_back(generatedGate);
    MapFeatureModel cityGateAnnotation;
    cityGateAnnotation.metadata.id = "generated_city_gate_annotation";
    cityGateAnnotation.kind = MapFeatureKind::Door;
    cityGateAnnotation.name = "City Gate";
    cityGateAnnotation.x = 4;
    cityGateAnnotation.y = 1;
    cityGateAnnotation.discovered = true;
    map.features.push_back(cityGateAnnotation);
    map.cells[2 * map.width + 3].kind = MapTileKind::Settlement;
    map.cells[3 * map.width + 5].fogged = true;

    MapPngExportOptions options;
    options.pixelsPerCell = 24;
    const std::filesystem::path path = "map_v2_export_test.png";
    const std::filesystem::path playerPath = "map_v2_export_player_safe_test.png";
    const std::filesystem::path unicodePath =
        std::filesystem::u8path(u8"map_export_München_日本.png");
    std::string error;
    ok &= require(exportMapPng(map, path, options, &error), "Map V2 PNG export should succeed: " + error);
    MapPngExportOptions playerOptions = options;
    playerOptions.playerSafe = true;
    playerOptions.applyFog = false;
    std::string playerError;
    ok &= require(exportMapPng(map, playerPath, playerOptions, &playerError), "Player-safe map PNG export should succeed: " + playerError);
    std::string unicodePathError;
    ok &= require(exportMapPng(map, unicodePath, options, &unicodePathError) &&
                      std::filesystem::exists(unicodePath) &&
                      std::filesystem::file_size(unicodePath) > 100,
                  "PNG export must preserve a UTF-8 destination path: " + unicodePathError);
    ok &= require(std::filesystem::exists(path) && std::filesystem::file_size(path) > 100, "Map V2 PNG export should write a non-empty file");
    ok &= require(std::filesystem::exists(playerPath) && std::filesystem::file_size(playerPath) > 100,
                  "Player-safe map PNG export should write a non-empty file");

    const std::filesystem::path englishLocalePath = "map_locale_en_test.png";
    const std::filesystem::path germanLocalePath = "map_locale_de_test.png";
    const std::filesystem::path germanPlanPath = "map_locale_de_plan_test.png";
    MapPngExportOptions englishLocaleOptions = options;
    englishLocaleOptions.locale = "en-US";
    MapPngExportOptions germanLocaleOptions = options;
    germanLocaleOptions.locale = "de-DE";
    std::string localeError;
    ok &= require(exportMapPng(map, englishLocalePath, englishLocaleOptions, &localeError),
                  "English localized PNG export should succeed: " + localeError);
    localeError.clear();
    ok &= require(exportMapPng(map, germanLocalePath, germanLocaleOptions, &localeError),
                  "German localized PNG export should succeed: " + localeError);
    MapRenderPlanRequest germanPlanRequest;
    germanPlanRequest.audience = MapRenderAudience::GameMaster;
    germanPlanRequest.locale = "de-DE";
    germanPlanRequest.layers = MapRenderPlanTerrain | MapRenderPlanWater |
                               MapRenderPlanWays | MapRenderPlanSymbols |
                               MapRenderPlanLabels | MapRenderPlanAnnotations |
                               MapRenderPlanRoutes | MapRenderPlanTokens |
                               MapRenderPlanFog;
    germanPlanRequest.applyFog = true;
    const MapRenderPlan germanPngPlan = compileMapRenderPlan(map, germanPlanRequest);
    localeError.clear();
    ok &= require(exportMapPng(map, germanPlanPath, germanPngPlan, 24, &localeError),
                  "MapRenderPlan locale PNG export should succeed: " + localeError);
    MapModel silentlyChangedMap = map;
    silentlyChangedMap.cells.front().elevation += 1;
    const std::filesystem::path stalePlanPath = "map_stale_plan_must_not_export.png";
    std::filesystem::remove(stalePlanPath);
    std::string stalePlanError;
    ok &= require(mapRenderPlanMatchesMap(germanPngPlan, silentlyChangedMap) &&
                      !mapRenderPlanContentMatchesMap(germanPngPlan, silentlyChangedMap) &&
                      !exportMapPng(silentlyChangedMap, stalePlanPath, germanPngPlan,
                                    24, &stalePlanError) &&
                      !std::filesystem::exists(stalePlanPath) &&
                      stalePlanError ==
                          "The map changed after the render preview. Create a new preview.",
                  "Reviewed-plan PNG export must reject render-relevant mutations even when a caller forgot to bump the revision");
    const auto readBinaryFile = [](const std::filesystem::path &filePath) {
        std::ifstream input(filePath, std::ios::binary);
        return std::vector<char>((std::istreambuf_iterator<char>(input)),
                                 std::istreambuf_iterator<char>());
    };
    const std::vector<char> englishLocaleBytes = readBinaryFile(englishLocalePath);
    const std::vector<char> germanLocaleBytes = readBinaryFile(germanLocalePath);
    const std::vector<char> germanPlanBytes = readBinaryFile(germanPlanPath);
    ok &= require(!englishLocaleBytes.empty() && englishLocaleBytes != germanLocaleBytes,
                  "DE and EN PNG output must present known generated labels in their requested locale");
    ok &= require(germanLocaleBytes == germanPlanBytes,
                  "MapRenderPlan.locale and direct PNG locale options must produce identical output");

    MapModel layerContractMap = map;
    layerContractMap.metadata.id = "png_layer_contract_map";
    layerContractMap.cells.front().kind = MapTileKind::Forest;
    layerContractMap.cells[1].kind = MapTileKind::Water;
    MapRenderPlanRequest groundRequest;
    groundRequest.audience = MapRenderAudience::GameMaster;
    groundRequest.locale = "de";
    groundRequest.layers = MapRenderPlanTerrain | MapRenderPlanWater;
    groundRequest.applyFog = false;
    const MapRenderPlan fullGroundPlan =
        compileMapRenderPlan(layerContractMap, groundRequest);
    MapRenderPlanRequest waterOnlyRequest = groundRequest;
    waterOnlyRequest.layers = MapRenderPlanWater;
    const MapRenderPlan waterOnlyPlan =
        compileMapRenderPlan(layerContractMap, waterOnlyRequest);
    const std::filesystem::path fullGroundPath = "map_full_ground_plan_test.png";
    const std::filesystem::path waterOnlyPath = "map_water_only_plan_test.png";
    std::string layerContractError;
    ok &= require(exportMapPng(layerContractMap, fullGroundPath, fullGroundPlan,
                               24, &layerContractError),
                  "Full-ground reviewed plan PNG should export: " + layerContractError);
    layerContractError.clear();
    ok &= require(exportMapPng(layerContractMap, waterOnlyPath, waterOnlyPlan,
                               24, &layerContractError),
                  "Water-only reviewed plan PNG should export: " + layerContractError);
    ok &= require(readBinaryFile(fullGroundPath) != readBinaryFile(waterOnlyPath),
                  "CPU PNG rasterization must obey the compiled plan's core terrain/water layer mask");

    const MapPngExportRect viewRect{2, 1, 3, 2};
    const MapPngExportSizing viewSizing = calculateMapPngExportSizing(viewRect, 24);
    ok &= require(viewSizing.valid && viewSizing.widthPixels == 72 && viewSizing.heightPixels == 48,
                  "Current-view PNG sizing should use the selected map rectangle");
    MapRenderPlanRequest viewRequest;
    viewRequest.audience = MapRenderAudience::Player;
    viewRequest.layers = MapRenderPlanAll;
    viewRequest.applyFog = true;
    const MapRenderPlan playerViewPlan = compileMapRenderPlan(map, viewRequest);
    const std::filesystem::path viewPath = "map_v2_export_current_view_test.png";
    std::filesystem::remove(viewPath);
    std::string viewError;
    ok &= require(exportMapPngRegion(map, viewPath, playerViewPlan, viewRect, 24, &viewError) &&
                      std::filesystem::exists(viewPath) && std::filesystem::file_size(viewPath) > 100,
                  "A reviewed current-view PNG should export the exact selected rectangle: " + viewError);
    MapPngExportRect invalidView = viewRect;
    invalidView.x = map.width;
    std::string invalidViewError;
    ok &= require(!exportMapPngRegion(map, "map_invalid_view_must_not_exist.png", playerViewPlan,
                                      invalidView, 24, &invalidViewError) &&
                      invalidViewError == "The selected map view is invalid.",
                  "An invalid current-view rectangle must fail before writing");

    const std::filesystem::path tiledDirectory = "map_png_tiled_export_test";
    std::filesystem::remove_all(tiledDirectory);
    MapPngTiledExportPlan writtenTilePlan;
    std::string tiledError;
    MapPngExportOptions tiledOptions = playerOptions;
    tiledOptions.locale = "de-DE";
    MapRenderPlanRequest tiledRenderRequest;
    tiledRenderRequest.audience = MapRenderAudience::Player;
    tiledRenderRequest.locale = tiledOptions.locale;
    tiledRenderRequest.layers = MapRenderPlanBackground |
                                MapRenderPlanTerrain | MapRenderPlanWater |
                                MapRenderPlanWays | MapRenderPlanSymbols |
                                MapRenderPlanLabels | MapRenderPlanAnnotations |
                                MapRenderPlanRoutes | MapRenderPlanTokens |
                                MapRenderPlanFog;
    tiledRenderRequest.applyFog = true;
    const MapRenderPlan tiledRenderPlan =
        compileMapRenderPlan(map, tiledRenderRequest);
    ok &= require(exportMapPngTiled(map, tiledDirectory, "safe_map", tiledRenderPlan,
                                    tiledOptions.pixelsPerCell, false,
                                    &writtenTilePlan, &tiledError),
                  "Tiled player-safe PNG export should succeed: " + tiledError);
    ok &= require(writtenTilePlan.valid && writtenTilePlan.tiles.size() == 1 &&
                      std::filesystem::exists(tiledDirectory / "safe_map_r000_c000.png") &&
                      std::filesystem::exists(tiledDirectory / "safe_map_tiles.json"),
                  "Tiled PNG export should publish an image and assembly manifest");
    {
        std::ifstream manifestFile(tiledDirectory / "safe_map_tiles.json");
        const std::string manifestText((std::istreambuf_iterator<char>(manifestFile)),
                                       std::istreambuf_iterator<char>());
        ok &= require(manifestText.find("\"schemaVersion\": 2") != std::string::npos &&
                          manifestText.find("\"locale\": \"de-DE\"") != std::string::npos,
                      "Tiled PNG manifest must record the locale used for every tile");
        ok &= require(manifestText.find("\"audience\": \"player\"") != std::string::npos &&
                          manifestText.find("\"renderPlanContentHash\": \"") != std::string::npos &&
                          manifestText.find("\"renderPlanLayers\": " +
                                            std::to_string(tiledRenderPlan.layers)) != std::string::npos,
                      "Tiled PNG manifest must prove the audience, layers, and immutable plan hash shared by every tile");
    }
    std::string duplicateTileError;
    ok &= require(!exportMapPngTiled(map, tiledDirectory, "safe_map", tiledRenderPlan,
                                     tiledOptions.pixelsPerCell, false,
                                     nullptr, &duplicateTileError) &&
                      duplicateTileError == "A tiled PNG output file already exists. Confirm replacement before exporting.",
                  "Tiled PNG export must never overwrite files without explicit confirmation");
    {
        std::ofstream staleTile(tiledDirectory / "safe_map_r999_c999.png", std::ios::binary | std::ios::trunc);
        staleTile << "stale tile from a larger previous export";
    }
    std::string replaceTileError;
    ok &= require(exportMapPngTiled(map, tiledDirectory, "safe_map", tiledRenderPlan,
                                    tiledOptions.pixelsPerCell, true,
                                    nullptr, &replaceTileError),
                  "A confirmed tiled PNG replacement should succeed: " + replaceTileError);
    ok &= require(!std::filesystem::exists(tiledDirectory / "safe_map_r999_c999.png") &&
                      std::filesystem::exists(tiledDirectory / "safe_map_previous_tiles" /
                                              "safe_map_r999_c999.png"),
                  "Confirmed replacement must archive stale tiles from a formerly larger output set");
    replaceTileError.clear();
    ok &= require(exportMapPngTiled(map, tiledDirectory, "safe_map", tiledRenderPlan,
                                    tiledOptions.pixelsPerCell, true,
                                    nullptr, &replaceTileError) &&
                      std::filesystem::exists(tiledDirectory / "safe_map_previous_tiles_2" /
                                              "safe_map_tiles.json"),
                  "Repeated confirmed exports should create another recoverable backup instead of becoming blocked");
    if (std::filesystem::exists(path) && std::filesystem::exists(playerPath))
    {
        std::ifstream gmFile(path, std::ios::binary);
        std::ifstream playerFile(playerPath, std::ios::binary);
        const std::vector<char> gmBytes((std::istreambuf_iterator<char>(gmFile)), std::istreambuf_iterator<char>());
        const std::vector<char> playerBytes((std::istreambuf_iterator<char>(playerFile)), std::istreambuf_iterator<char>());
        ok &= require(gmBytes != playerBytes, "Player-safe PNG export should omit GM-only generated features");
    }
    if (std::filesystem::exists(path))
    {
        std::ifstream file(path, std::ios::binary);
        unsigned char signature[8]{};
        file.read(reinterpret_cast<char *>(signature), 8);
        ok &= require(signature[0] == 0x89 && signature[1] == 'P' && signature[2] == 'N' && signature[3] == 'G', "Map V2 export should have a PNG signature");
        file.close();
        std::filesystem::remove(path);
    }
    if (std::filesystem::exists(playerPath))
        std::filesystem::remove(playerPath);
    if (std::filesystem::exists(unicodePath))
        std::filesystem::remove(unicodePath);
    if (std::filesystem::exists(viewPath))
        std::filesystem::remove(viewPath);
    std::filesystem::remove(englishLocalePath);
    std::filesystem::remove(germanLocalePath);
    std::filesystem::remove(germanPlanPath);
    std::filesystem::remove(stalePlanPath);
    std::filesystem::remove(fullGroundPath);
    std::filesystem::remove(waterOnlyPath);
    std::filesystem::remove_all(tiledDirectory);
    return ok;
}

bool testMapRenderPlanContract()
{
    MapModel map;
    map.metadata.id = "render_plan_map";
    map.visibility = MapVisibility::PlayersVisible;
    map.revision = 4;
    map.width = 8;
    map.height = 6;
    resizeMapCells(map);
    map.playerRelease.waysAndRoutes = true;
    map.playerRelease.tokensAndAnnotations = true;
    map.generation.recipe.styleSnapshotId = "snapshot_test";
    MapGeneratedFeature publicRoad;
    publicRoad.id = "public_road";
    publicRoad.kind = MapGeneratedFeatureKind::Road;
    publicRoad.points = {{0.5f, 1.5f}, {7.5f, 1.5f}};
    MapGeneratedFeature secretRoad = publicRoad;
    secretRoad.id = "secret_road";
    secretRoad.gmOnly = true;
    map.generation.graph.features = {publicRoad, secretRoad};
    MapTokenModel publicToken;
    publicToken.metadata.id = "public_token";
    MapTokenModel secretToken;
    secretToken.metadata.id = "secret_token";
    secretToken.hiddenFromPlayers = true;
    map.tokens = {publicToken, secretToken};

    MapRenderPlanRequest playerRequest;
    playerRequest.audience = MapRenderAudience::Player;
    playerRequest.locale = "de";
    playerRequest.showGrid = false;
    const MapRenderPlan first = compileMapRenderPlan(map, playerRequest);
    const MapRenderPlan second = compileMapRenderPlan(map, playerRequest);
    const MapModel filtered = filterMapForRenderPlan(map, first);
    bool ok = true;
    ok &= require(first.contentHash == second.contentHash, "Identical map render requests should have a stable plan hash");
    ok &= require(mapRenderPlanMatchesMap(first, map),
                  "A render plan should match the exact map revision it was compiled from");
    ok &= require(first.generatedFeatureIds.size() == 1 && first.generatedFeatureIds.front() == "public_road",
                  "Player render plan should exclude GM-only generated features");
    ok &= require(first.tokenIds.size() == 1 && first.tokenIds.front() == "public_token",
                  "Player render plan should exclude hidden tokens");
    ok &= require(filtered.generation.graph.features.size() == 1 && filtered.tokens.size() == 1,
                  "Render-plan filtering should produce the same public content contract for every backend");
    MapRenderPlanRequest terrainOnly = playerRequest;
    terrainOnly.audience = MapRenderAudience::GameMaster;
    terrainOnly.layers = MapRenderPlanTerrain;
    MapModel projectedRoadMap = map;
    projectedRoadMap.cells[1].kind = MapTileKind::Road;
    projectedRoadMap.cells[1].wayKind = MapWayKind::DirtRoad;
    const MapRenderPlan terrainPlan = compileMapRenderPlan(projectedRoadMap, terrainOnly);
    const MapModel terrainProjection =
        filterMapForRenderPlan(projectedRoadMap, terrainPlan);
    ok &= require(terrainPlan.generatedFeatureIds.empty() && terrainPlan.tokenIds.empty(),
                  "Disabled render layers must not leak into a backend plan or invalidate its cache");
    ok &= require(terrainProjection.cells[1].kind == MapTileKind::Empty &&
                      terrainProjection.cells[1].wayKind == MapWayKind::None,
                  "A terrain-only plan must strip synchronized road cells for GM and player raster backends");
    MapRenderPlanRequest campaignFiltered = playerRequest;
    campaignFiltered.additionalGeneratedFeatureVisibility = [](const MapGeneratedFeature &feature) {
        return feature.id != "public_road";
    };
    ok &= require(compileMapRenderPlan(map, campaignFiltered).generatedFeatureIds.empty(),
                  "Campaign publication rules should be able to hide, but never reveal, generated features");
    map.cells.front().kind = MapTileKind::Water;
    ok &= require(compileMapRenderPlan(map, playerRequest).contentHash != first.contentHash,
                  "Semantic map edits should invalidate the render plan hash");
    ++map.revision;
    ok &= require(!mapRenderPlanMatchesMap(first, map),
                  "A render plan must become stale after the map revision changes");
    return ok;
}

bool testRotatedImportedMapPngParity()
{
    LoadedRgbaImage sourceImage;
    sourceImage.width = 96;
    sourceImage.height = 64;
    sourceImage.sourceWidth = sourceImage.width;
    sourceImage.sourceHeight = sourceImage.height;
    sourceImage.pixels.resize(
        static_cast<std::size_t>(sourceImage.width * sourceImage.height * 4), 255u);
    for (int y = 0; y < sourceImage.height; ++y)
        for (int x = 0; x < sourceImage.width; ++x)
        {
            const std::size_t offset =
                static_cast<std::size_t>((y * sourceImage.width + x) * 4);
            sourceImage.pixels[offset] = static_cast<unsigned char>((x * 5 + y) & 0xff);
            sourceImage.pixels[offset + 1] = static_cast<unsigned char>((y * 7 + x) & 0xff);
            sourceImage.pixels[offset + 2] = static_cast<unsigned char>((x * 3 + y * 11) & 0xff);
        }

    const std::filesystem::path sourcePath = "rotated_import_source.png";
    std::string error;
    bool ok = require(saveRgbaImageAsPng(sourcePath, sourceImage, &error),
                      "Could not prepare rotated import export fixture: " + error);
    Campaign campaign = makeCampaign("rotated_import_export");
    MapModel map;
    map.metadata.id = "rotated_import_export_map";
    map.width = 12;
    map.height = 8;
    resizeMapCells(map);
    MapImageImport::Request importRequest;
    importRequest.sourcePath = sourcePath;
    importRequest.preserveExistingMapContract = true;
    importRequest.crop = {0.08f, 0.12f, 0.84f, 0.76f};
    const MapImageImport::Result imported =
        MapImageImport::importReference(campaign, map, importRequest);
    ok &= require(imported.success, "Could not import rotated export fixture: " +
                                       imported.errorCode + " " + imported.errorDetail);

    MapRenderPlanRequest planRequest;
    planRequest.audience = MapRenderAudience::GameMaster;
    planRequest.layers = MapRenderPlanBackground;
    planRequest.applyFog = false;
    const MapPngExportRect view{2, 1, 5, 4};
    const int pixelsPerCell = 8;
    const auto imageRegionMatches = [&](const LoadedRgbaImage &full,
                                        const LoadedRgbaImage &region,
                                        const MapPngExportRect &regionRect) {
        if (region.width != regionRect.width * pixelsPerCell ||
            region.height != regionRect.height * pixelsPerCell ||
            full.width < (regionRect.x + regionRect.width) * pixelsPerCell ||
            full.height < (regionRect.y + regionRect.height) * pixelsPerCell)
            return false;
        for (int y = 0; y < region.height; ++y)
            for (int x = 0; x < region.width; ++x)
            {
                const std::size_t regionOffset =
                    static_cast<std::size_t>((y * region.width + x) * 4);
                const int fullX = regionRect.x * pixelsPerCell + x;
                const int fullY = regionRect.y * pixelsPerCell + y;
                const std::size_t fullOffset =
                    static_cast<std::size_t>((fullY * full.width + fullX) * 4);
                for (int channel = 0; channel < 4; ++channel)
                    if (region.pixels[regionOffset + channel] !=
                        full.pixels[fullOffset + channel])
                        return false;
            }
        return true;
    };

    for (float rotation : {-180.0f, -90.0f, 0.0f, 90.0f, 180.0f})
    {
        MapSpatialReference spatialReference = map.spatialReference;
        error.clear();
        ok &= require(MapImageImport::updateCalibration(
                          map, importRequest.crop, rotation,
                          spatialReference, &error),
                      "Could not set imported-map quarter turn: " + error);
        const MapRenderPlan plan = compileMapRenderPlan(map, planRequest);
        const int rotationName = static_cast<int>(rotation + 180.0f);
        const std::filesystem::path fullPath =
            "rotated_import_full_" + std::to_string(rotationName) + ".png";
        const std::filesystem::path viewPath =
            "rotated_import_view_" + std::to_string(rotationName) + ".png";
        error.clear();
        ok &= require(exportMapPng(map, fullPath, plan, pixelsPerCell, &error),
                      "Whole imported-map quarter-turn export failed: " + error);
        error.clear();
        ok &= require(exportMapPngRegion(
                          map, viewPath, plan, view, pixelsPerCell, &error),
                      "Current-view imported-map quarter-turn export failed: " + error);
        LoadedRgbaImage fullImage;
        LoadedRgbaImage viewImage;
        ok &= require(loadRgbaImage(fullPath, fullImage) &&
                          loadRgbaImage(viewPath, viewImage) &&
                          imageRegionMatches(fullImage, viewImage, view),
                      "Current-view export must be an exact pixel crop of the whole-map export at every quarter turn");
        std::filesystem::remove(fullPath);
        std::filesystem::remove(viewPath);
    }

    // Force two tiles without allocating an oversized whole-map image. Their
    // bleed overlaps must contain identical pixels in global map space.
    MapModel tiledMap = map;
    tiledMap.width = 2060;
    tiledMap.height = 2;
    resizeMapCells(tiledMap);
    ++tiledMap.revision;
    MapSpatialReference tiledSpatialReference = tiledMap.spatialReference;
    ok &= require(MapImageImport::updateCalibration(
                      tiledMap, importRequest.crop, 90.0f,
                      tiledSpatialReference, &error),
                  "Could not prepare rotated tiled export: " + error);
    const MapRenderPlan tiledRenderPlan = compileMapRenderPlan(tiledMap, planRequest);
    const std::filesystem::path tiledDirectory = "rotated_import_tiles";
    std::filesystem::remove_all(tiledDirectory);
    MapPngTiledExportPlan tiledPlan;
    error.clear();
    ok &= require(exportMapPngTiled(
                      tiledMap, tiledDirectory, "rotated", tiledRenderPlan,
                      pixelsPerCell, false, &tiledPlan, &error) &&
                      tiledPlan.tiles.size() == 2,
                  "Rotated managed images must support multi-tile PNG export: " + error);
    if (tiledPlan.tiles.size() == 2)
    {
        LoadedRgbaImage left;
        LoadedRgbaImage right;
        const bool loaded = loadRgbaImage(
                                tiledDirectory / tiledPlan.tiles[0].fileName, left) &&
                            loadRgbaImage(
                                tiledDirectory / tiledPlan.tiles[1].fileName, right);
        bool overlapMatches = loaded;
        const int overlapStart = std::max(tiledPlan.tiles[0].render.x,
                                          tiledPlan.tiles[1].render.x);
        const int overlapEnd = std::min(
            tiledPlan.tiles[0].render.x + tiledPlan.tiles[0].render.width,
            tiledPlan.tiles[1].render.x + tiledPlan.tiles[1].render.width);
        for (int globalCellX = overlapStart;
             overlapMatches && globalCellX < overlapEnd; ++globalCellX)
            for (int y = 0; overlapMatches && y < left.height; ++y)
                for (int pixelInCell = 0;
                     overlapMatches && pixelInCell < pixelsPerCell; ++pixelInCell)
                {
                    const int leftX =
                        (globalCellX - tiledPlan.tiles[0].render.x) * pixelsPerCell +
                        pixelInCell;
                    const int rightX =
                        (globalCellX - tiledPlan.tiles[1].render.x) * pixelsPerCell +
                        pixelInCell;
                    const std::size_t leftOffset =
                        static_cast<std::size_t>((y * left.width + leftX) * 4);
                    const std::size_t rightOffset =
                        static_cast<std::size_t>((y * right.width + rightX) * 4);
                    overlapMatches = std::equal(
                        left.pixels.begin() + static_cast<std::ptrdiff_t>(leftOffset),
                        left.pixels.begin() + static_cast<std::ptrdiff_t>(leftOffset + 4),
                        right.pixels.begin() + static_cast<std::ptrdiff_t>(rightOffset));
                }
        ok &= require(overlapMatches && overlapEnd > overlapStart,
                      "Adjacent rotated PNG tiles must sample identical pixels throughout their shared bleed");
    }

    // Both original and working-proxy identities are part of the immutable
    // reviewed plan even though export reads the higher-resolution original.
    MapModel proxyBoundMap = map;
    proxyBoundMap.importedReference.workingProxyGenerated = true;
    proxyBoundMap.importedReference.workingProxyAssetId = "bound_proxy";
    proxyBoundMap.importedReference.workingProxyProjectRelativePath =
        proxyBoundMap.importedReference.projectRelativePath;
    proxyBoundMap.importedReference.workingProxyContentHash =
        proxyBoundMap.importedReference.contentHash;
    const MapRenderPlan proxyBoundPlan =
        compileMapRenderPlan(proxyBoundMap, planRequest);
    proxyBoundMap.importedReference.workingProxyContentHash = "changed-proxy-hash";
    error.clear();
    ok &= require(!exportMapPng(proxyBoundMap,
                                "stale_proxy_export_must_not_exist.png",
                                proxyBoundPlan, pixelsPerCell, &error) &&
                      !std::filesystem::exists(
                          "stale_proxy_export_must_not_exist.png"),
                  "Changing either bound managed-image hash must stale the reviewed export plan");

    std::filesystem::remove(sourcePath);
    std::filesystem::remove_all(tiledDirectory);
    return ok;
}

bool testRgbaExifOrientationAndManagedProxy()
{
    const auto makePattern = []() {
        LoadedRgbaImage image;
        image.width = 3;
        image.height = 2;
        image.sourceWidth = 3;
        image.sourceHeight = 2;
        image.pixels.resize(3u * 2u * 4u, 255);
        for (int index = 0; index < 6; ++index)
            image.pixels[static_cast<std::size_t>(index * 4)] = static_cast<unsigned char>(index + 1);
        return image;
    };
    const auto redValues = [](const LoadedRgbaImage &image) {
        std::vector<int> values;
        for (std::size_t offset = 0; offset < image.pixels.size(); offset += 4)
            values.push_back(image.pixels[offset]);
        return values;
    };
    const std::vector<std::vector<int>> expected{
        {},
        {1, 2, 3, 4, 5, 6},
        {3, 2, 1, 6, 5, 4},
        {6, 5, 4, 3, 2, 1},
        {4, 5, 6, 1, 2, 3},
        {1, 4, 2, 5, 3, 6},
        {4, 1, 5, 2, 6, 3},
        {6, 3, 5, 2, 4, 1},
        {3, 6, 2, 5, 1, 4}};
    bool ok = true;
    for (int orientation = 1; orientation <= 8; ++orientation)
    {
        LoadedRgbaImage image = makePattern();
        ok &= require(applyRgbaExifOrientation(image, orientation),
                      "EXIF orientation transform should accept all values 1..8");
        ok &= require(redValues(image) == expected[static_cast<std::size_t>(orientation)],
                      "EXIF orientation transform produced incorrect mirrored/rotated pixels for orientation " +
                          std::to_string(orientation));
        ok &= require((orientation < 5 && image.width == 3 && image.height == 2) ||
                          (orientation >= 5 && image.width == 2 && image.height == 3),
                      "EXIF orientation should swap dimensions only for transposed orientations");
    }

    // Minimal JPEG container with a valid little-endian EXIF orientation IFD.
    const std::filesystem::path exifPath = "managed_map_exif_orientation.jpg";
    std::vector<unsigned char> exifBytes{
        0xff, 0xd8, 0xff, 0xe1, 0x00, 0x22,
        'E', 'x', 'i', 'f', 0x00, 0x00,
        'I', 'I', 0x2a, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x01, 0x00,
        0x12, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x07, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0xff, 0xd9};
    {
        std::ofstream stream(exifPath, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char *>(exifBytes.data()), static_cast<std::streamsize>(exifBytes.size()));
    }
    ok &= require(readJpegExifOrientation(exifPath) == 7,
                  "JPEG EXIF parser should read mirrored orientations deterministically");

    LoadedRgbaImage large;
    large.width = 8193;
    large.height = 2;
    large.sourceWidth = large.width;
    large.sourceHeight = large.height;
    large.pixels.resize(static_cast<std::size_t>(large.width * large.height * 4), 255);
    for (int x = 0; x < large.width; ++x)
    {
        const std::size_t offset = static_cast<std::size_t>(x * 4);
        large.pixels[offset] = static_cast<unsigned char>(x % 251);
        large.pixels[offset + 1] = static_cast<unsigned char>((x * 3) % 251);
    }
    const std::filesystem::path largePath = "managed_map_large_source.png";
    std::string error;
    ok &= require(saveRgbaImageAsPng(largePath, large, &error),
                  "Could not create large managed-map fixture: " + error);
    const std::string originalHash = ProjectAssetImport::computeContentHash(largePath);

    Campaign campaign = makeCampaign("managed_map_proxy_import");
    MapModel map;
    MapImageImport::Request request;
    request.sourcePath = largePath;
    request.intent = MapCreationIntent::ImportedReference;
    request.sizePreset = MapSizePreset::Small;
    const MapImageImport::Result imported = MapImageImport::importReference(campaign, map, request);
    ok &= require(imported.success, "Large managed map import failed: " + imported.errorCode + " " + imported.errorDetail);
    ok &= require(imported.workingProxyGenerated && map.importedReference.workingProxyGenerated &&
                      imported.displayWidth == 8192 && imported.displayHeight == 2 &&
                      map.importedReference.workingProxyWidth == 8192 &&
                      map.importedReference.workingProxyHeight == 2,
                  "Sources above 8192 pixels should receive a bounded working PNG proxy");
    ok &= require(!map.importedReference.assetId.empty() && !map.importedReference.workingProxyAssetId.empty() &&
                      map.importedReference.assetId != map.importedReference.workingProxyAssetId &&
                      !std::filesystem::path(map.importedReference.projectRelativePath).is_absolute() &&
                      !std::filesystem::path(map.importedReference.workingProxyProjectRelativePath).is_absolute() &&
                      !map.importedReference.contentHash.empty() &&
                      !map.importedReference.workingProxyContentHash.empty(),
                  "Original and proxy should be distinct, hashed, project-relative managed assets");
    ok &= require(ProjectAssetImport::computeContentHash(
                          ProjectAssets::resolveProjectPath(map.importedReference.projectRelativePath)) == originalHash,
                  "Managed original bytes must remain unchanged after proxy generation");
    ok &= require(MapImageImport::preferredRenderProjectPath(map.importedReference) ==
                          map.importedReference.workingProxyProjectRelativePath &&
                      map.backgroundImagePath == map.importedReference.workingProxyProjectRelativePath,
                  "Runtime path resolution should prefer the bounded proxy");
    LoadedRgbaImage decodedProxy;
    ok &= require(loadRgbaImage(ProjectAssets::resolveProjectPath(map.importedReference.workingProxyProjectRelativePath),
                                decodedProxy) && decodedProxy.width == 8192 && decodedProxy.height == 2,
                  "Managed working proxy should be a valid, correctly sized PNG");

    RgbaImageLoadOptions strict;
    strict.maximumSourceEdge = 8192;
    strict.maximumSourcePixels = 100000000;
    LoadedRgbaImage rejected;
    ok &= require(!loadRgbaImage(largePath, rejected, strict, &error) && error == "image.too_large" &&
                      rejected.pixels.empty() && rejected.sourceWidth == 8193,
                  "Untrusted image dimensions must fail before allocating the RGBA buffer");
    strict.maximumSourceEdge = 16384;
    strict.maximumSourcePixels = 10000;
    ok &= require(!loadRgbaImage(largePath, rejected, strict, &error) && error == "image.too_large" &&
                      rejected.pixels.empty(),
                  "Pixel-count limits must be enforced independently of edge limits");

    LoadedRgbaImage overEdge;
    overEdge.width = MapImageImport::maximumSourceEdge + 1;
    overEdge.height = 1;
    overEdge.sourceWidth = overEdge.width;
    overEdge.sourceHeight = overEdge.height;
    overEdge.pixels.resize(static_cast<std::size_t>(overEdge.width * 4), 127);
    const std::filesystem::path overEdgePath = "managed_map_over_edge.png";
    ok &= require(saveRgbaImageAsPng(overEdgePath, overEdge, &error),
                  "Could not create over-edge fixture: " + error);
    MapModel sentinel;
    sentinel.name = "unchanged-on-rejection";
    request.sourcePath = overEdgePath;
    const MapImageImport::Result refused = MapImageImport::importReference(campaign, sentinel, request);
    ok &= require(!refused.success && refused.errorCode == "map.import.too_large" &&
                      refused.sourceWidth == MapImageImport::maximumSourceEdge + 1 &&
                      sentinel.name == "unchanged-on-rejection" && !sentinel.importedReference.enabled,
                  "Managed import must fail closed above 16384 pixels without mutating the map");

    std::error_code ignored;
    std::filesystem::remove(exifPath, ignored);
    std::filesystem::remove(largePath, ignored);
    std::filesystem::remove(overEdgePath, ignored);
    return ok;
}

bool testManagedMapImageImport()
{
    Campaign campaign = makeCampaign("managed_map_import");
    MapModel source;
    source.width = 8;
    source.height = 6;
    resizeMapCells(source);
    source.generation.enabled = true;
    source.generation.continuousRendering = true;
    const std::filesystem::path sourcePath = "managed_map_import_source.png";
    MapPngExportOptions png;
    png.pixelsPerCell = 16;
    std::string error;
    bool ok = require(exportMapPng(source, sourcePath, png, &error), "Could not create map import fixture: " + error);

    MapModel imported;
    MapImageImport::Request request;
    request.sourcePath = sourcePath;
    request.intent = MapCreationIntent::ImportedReference;
    request.sizePreset = MapSizePreset::Small;
    request.crop = {0.0f, 0.0f, 1.0f, 1.0f};
    request.spatialReference = {true, 24.0f, 18.0f, MapDistanceUnit::Kilometers, 0.0f, 0.0f, 0.0f};
    const MapImageImport::Result result = MapImageImport::importReference(campaign, imported, request);
    ok &= require(result.success, "Managed map image import failed: " + result.errorCode + " " + result.errorDetail);
    ok &= require(imported.importedReference.enabled && imported.importedReference.gmOnly &&
                      !imported.importedReference.projectRelativePath.empty() && !imported.importedReference.contentHash.empty() &&
                      !imported.importedReference.workingProxyGenerated &&
                      MapImageImport::preferredRenderProjectPath(imported.importedReference) ==
                          imported.importedReference.projectRelativePath &&
                      imported.backgroundImagePath == imported.importedReference.projectRelativePath,
                  "Imported map should use a managed, GM-only project reference");
    ok &= require(imported.width == 48 && imported.height == 36 && imported.atlasLevel == MapLevel::Site,
                  "Imported map intent and size preset were not applied atomically");

    MapImageImport::TraceRequest trace;
    trace.kind = MapGeneratedFeatureKind::River;
    trace.name = "Test River";
    trace.points = {{1.0f, 2.0f}, {20.0f, 12.0f}, {42.0f, 30.0f}};
    EntityId tracedId;
    ok &= require(MapImageImport::addTraceFeature(imported, trace, &tracedId, &error),
                  "Valid semantic trace should be accepted: " + error);
    ok &= require(!tracedId.empty() && imported.importedReference.semanticallyTraced &&
                      imported.generation.graph.features.size() == 1 && imported.generation.graph.features.front().locked,
                  "Semantic trace should create a stable locked graph feature");

    trace.points = {{-1.0f, 2.0f}, {4.0f, 6.0f}};
    ok &= require(!MapImageImport::addTraceFeature(imported, trace, nullptr, &error) && error == "map.import.trace_outside_map",
                  "Out-of-bounds semantic trace should fail without mutating the graph");
    ok &= require(imported.generation.graph.features.size() == 1, "Failed trace must not mutate imported map semantics");

    std::error_code ignored;
    std::filesystem::remove(sourcePath, ignored);
    return ok;
}
}

#include "ProjectSaveReliabilityTests.hpp"

int main()
{
    const std::filesystem::path disposableProjectRoot = prepareDisposableProjectRoot();
    configureDisposableDatabase(disposableProjectRoot);
    bool ok = true;
    auto run = [&](const char *name, const auto &test) {
        std::cout << "foundation: " << name << std::endl;
        try { ok &= test(); }
        catch (const std::exception &error) {
            std::cout << "FAIL foundation " << name << ": " << error.what() << std::endl;
            ok = false;
        }
    };
    {
        const ScopedCurrentPath scopedProjectRoot(disposableProjectRoot);

        run("model normalization", testModelNormalization);
        run("new character HP estimates", testNewCharacterHitPointEstimates);
        run("session control", testSessionControlService);
        run("campaign search", testCampaignSearchIndex);
        run("campaign tags", testCampaignTagService);
        run("campaign collection", testCampaignCollectionService);
        run("campaign references", testCampaignReferenceService);
        run("campaign graph", testCampaignGraphService);
        run("command palette", testCampaignCommandPaletteService);
        run("bookmarks", testCampaignBookmarkService);
        run("index rebuild", testCampaignIndexRebuildService);
        run("ruleset basics", testRulesetBasics);
        run("ruleset layering", testRulesetLayering);
        run("formula evaluator", testRulesetFormulaEvaluator);
        run("ruleset validation", testRulesetValidationDiagnostics);
        run("content pack JSON", testRulesetContentPackJson);
        run("migration plan", testRulesetMigrationPlan);
        run("definition runtime", testDefinitionBackedRulesetRuntime);
        run("persistence round trip", testPersistenceRoundTrip);
        run("campaign details rename", testCampaignDetailsRenamePersistence);
        run("campaign save transaction rollback", testCampaignSaveTransactionRollback);
        run("tabletop toolkit ownership guard", testTabletopToolkitOwnershipGuard);
        run("map generation metadata", testMapGenerationMetadataRoundTrip);
        run("map PNG export", testMapPngExport);
        run("map render plan", testMapRenderPlanContract);
        run("rotated imported-map PNG parity", testRotatedImportedMapPngParity);
        run("EXIF and managed map proxy", testRgbaExifOrientationAndManagedProxy);
        run("managed map image import", testManagedMapImageImport);
        run("project save", testProjectSaveWorkflow);
        run("audio failed save and retry", testAudioDraftSaveFailureRetry);
        run("save commit and follow-up failures", [] { return SaveReliabilityTests::run(makeCampaign); });
        run("map-cell byte compatibility", SaveReliabilityTests::serializationContract);
        run("audio paths", testAudioAssetPathService);
        run("package layout", testProjectPackageLayout);
        run("manifest", testProjectManifestService);
        run("asset import", testProjectAssetImportService);
        run("asset deduplication", testProjectAssetImportDeduplication);
        run("asset repair", testProjectAssetRepairService);
        run("asset cache", testProjectAssetCachePolicy);
        run("audio playlist", testAudioPlaybackPlaylistState);
        run("package export", testProjectPackageExportService);
        run("package import", testProjectPackageImportService);
        run("asset collection", testProjectAssetCollectionService);
        run("project storage", testProjectStorageService);
    }
    run("structure guardrails", testProjectStructureGuardrails);
    run("legacy retirement", testLegacySurfaceRetirementRegistry);
    std::error_code ignored;
    std::filesystem::remove_all(mapFileIoPath(disposableProjectRoot), ignored);
    return ok ? 0 : 1;
}
