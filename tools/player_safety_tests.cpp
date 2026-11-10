#include "Controllers/Editor/Widgets/MapViewer/MapViewerFogStroke.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPlayerOutputPolicy.hpp"
#include "Models/Map/MapEditTransactions.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapFogTexture.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Controllers/Editor/Widgets/MapViewer/MapViewerCampaignSaveTransaction.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Session/PlayerDisplayBlackoutShortcut.hpp"
#include "Models/Session/LocalPlayerDisplayCameraPresets.hpp"
#include "Models/Session/PlayerSafeDataFilter.hpp"
#include "Models/Session/PlayerSafeSelectionPreview.hpp"
#include "selected_preview_fixture.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Media/RgbaImageLoader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace
{
bool require(bool condition, const std::string &message)
{
    if (condition)
        return true;
    std::cerr << message << '\n';
    return false;
}

bool contains(const std::string &value, const std::string &needle)
{
    return value.find(needle) != std::string::npos;
}
#include "selected_preview_safety_cases.hpp"

std::vector<char> readBytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string contentHash(const std::filesystem::path &path)
{
    const std::vector<char> bytes = readBytes(path);
    std::uint64_t hash = 14695981039346656037ull;
    for (const char value : bytes)
    {
        hash ^= static_cast<unsigned char>(value);
        hash *= 1099511628211ull;
    }
    std::ostringstream stream;
    stream << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

bool pixelMatches(const LoadedRgbaImage &image, int x, int y, int red, int green, int blue)
{
    if (x < 0 || y < 0 || x >= image.width || y >= image.height)
        return false;
    const std::size_t offset = static_cast<std::size_t>((y * image.width + x) * 4);
    return offset + 2 < image.pixels.size() &&
           image.pixels[offset] == red && image.pixels[offset + 1] == green && image.pixels[offset + 2] == blue;
}

bool fogPixelMatches(const LoadedRgbaImage &image,int x,int y)
{
    const auto color=gm::map_render::fogTextureAt((x+.5f)/16.f,(y+.5f)/16.f);
    return pixelMatches(image,x,y,color[0],color[1],color[2]);
}

LoadedRgbaImage solidImage(int red, int green, int blue)
{
    LoadedRgbaImage image;
    image.width = 4;
    image.height = 4;
    image.sourceWidth = image.width;
    image.sourceHeight = image.height;
    image.pixels.resize(static_cast<std::size_t>(image.width * image.height * 4));
    for (std::size_t offset = 0; offset < image.pixels.size(); offset += 4)
    {
        image.pixels[offset] = static_cast<unsigned char>(red);
        image.pixels[offset + 1] = static_cast<unsigned char>(green);
        image.pixels[offset + 2] = static_cast<unsigned char>(blue);
        image.pixels[offset + 3] = 255;
    }
    return image;
}

bool testPlayerSafeProjection()
{
    bool ok = true;
    Campaign campaign;

    MapModel map;
    map.metadata.id = "map_public";
    map.name = "Public Map";
    map.visibility = MapVisibility::PlayersVisible;
    map.width = 4;
    map.height = 2;
    map.cells.resize(8);
    map.cells[2].fogged = true;

    MapLayerModel visibleLayer;
    visibleLayer.metadata.id = "layer_visible";
    visibleLayer.visible = true;
    map.layers.push_back(visibleLayer);
    MapLayerModel hiddenLayer;
    hiddenLayer.metadata.id = "layer_hidden";
    hiddenLayer.visible = false;
    map.layers.push_back(hiddenLayer);

    auto addToken = [&](const char *id, const char *name, float x, float y, bool hidden, const char *layerId = "") {
        MapTokenModel token;
        token.metadata.id = id;
        token.name = name;
        token.x = x;
        token.y = y;
        token.hiddenFromPlayers = hidden;
        token.layerId = layerId;
        map.tokens.push_back(token);
    };
    addToken("token_public", "Public token", 0.0f, 0.0f, false);
    addToken("token_explicit_hidden", "EXPLICIT_TOKEN_SECRET", 1.0f, 0.0f, true);
    addToken("token_in_fog", "FOGGED_TOKEN_SECRET", 2.0f, 0.0f, false);
    addToken("token_hidden_layer", "LAYER_TOKEN_SECRET", 3.0f, 0.0f, false, "layer_hidden");
    addToken("token_stale_layer", "STALE_LAYER_TOKEN_SECRET", 0.0f, 1.0f, false, "missing_layer");
    addToken("token_visible_layer", "Layered public token", 1.0f, 1.0f, false, "layer_visible");
    addToken("token_invalid_position", "INVALID_POSITION_SECRET", std::numeric_limits<float>::quiet_NaN(), 1.0f, false);

    auto addFeature = [&](const char *id, const char *name, int x, int y, bool discovered, bool gmOnly) {
        MapFeatureModel feature;
        feature.metadata.id = id;
        feature.name = name;
        feature.description = std::string(name) + " public description";
        feature.gmNotes = std::string(name) + " GM_NOTE_SENTINEL";
        feature.x = x;
        feature.y = y;
        feature.discovered = discovered;
        feature.gmOnly = gmOnly;
        map.features.push_back(feature);
    };
    addFeature("feature_public", "Public feature", 0, 0, true, false);
    addFeature("feature_gm", "GM_FEATURE_SECRET", 1, 0, true, true);
    addFeature("feature_unknown", "UNDISCOVERED_FEATURE_SECRET", 1, 1, false, false);
    addFeature("feature_fog", "FOGGED_FEATURE_SECRET", 2, 0, true, false);
    addFeature("feature_invalid", "INVALID_FEATURE_SECRET", 99, 99, true, false);

    campaign.maps.push_back(map);
    MapModel knownMap = map;
    knownMap.metadata.id = "map_known";
    knownMap.visibility = MapVisibility::PlayersKnown;
    campaign.maps.push_back(knownMap);
    MapModel gmMap = map;
    gmMap.metadata.id = "map_gm";
    gmMap.visibility = MapVisibility::GameMasterOnly;
    campaign.maps.push_back(gmMap);

    const std::vector<PlayerSafeMapToken> tokens = buildPlayerSafeMapTokens(campaign.maps.front());
    ok &= require(tokens.size() == 2, "Player projection must exclude hidden, fogged, invalid, and hidden-layer tokens");
    ok &= require(std::any_of(tokens.begin(), tokens.end(), [](const PlayerSafeMapToken &token) { return token.name == "Public token"; }),
                  "Public token was removed from player projection");
    ok &= require(std::any_of(tokens.begin(), tokens.end(), [](const PlayerSafeMapToken &token) { return token.name == "Layered public token"; }),
                  "Token on visible layer was removed from player projection");

    const std::vector<PlayerSafeMapFeature> features = buildPlayerSafeMapFeatures(campaign.maps.front());
    ok &= require(features.size() == 1 && features.front().name == "Public feature",
                  "Player projection must expose only discovered, non-GM features on revealed cells");
    ok &= require(!contains(features.front().description, "GM_NOTE_SENTINEL"),
                  "Player feature description unexpectedly contains GM notes");

    ok &= require(findPlayerSafePublishedMap(campaign, "map_public") != nullptr,
                  "PlayersVisible map should be publishable");
    ok &= require(findPlayerSafePublishedMap(campaign, "map_known") == nullptr &&
                      findPlayerSafePublishedMap(campaign, "map_gm") == nullptr &&
                      findPlayerSafePublishedMap(campaign, "") == nullptr,
                  "Known-only, GM-only, and empty map references must fail closed");
    ok &= require(!playerSafeMapBackgroundArtIsFullyRevealed(campaign.maps.front()),
                  "Full background art must not bypass partial fog");
    MapModel fullyRevealed = campaign.maps.front();
    for (MapCellModel &cell : fullyRevealed.cells)
        cell.fogged = false;
    ok &= require(playerSafeMapBackgroundArtIsFullyRevealed(fullyRevealed),
                  "Fully revealed public background art should remain available");
    fullyRevealed.visibility = MapVisibility::PlayersKnown;
    ok &= require(!playerSafeMapBackgroundArtIsFullyRevealed(fullyRevealed),
                  "Known-only map background art must not be published");
    fullyRevealed.visibility = MapVisibility::PlayersVisible;
    fullyRevealed.cells.pop_back();
    ok &= require(!playerSafeMapBackgroundArtIsFullyRevealed(fullyRevealed),
                  "Incomplete legacy cell data must not allow full background art to bypass fog");

    NPCModel publicNpc;
    publicNpc.metadata.id = "npc_public";
    publicNpc.name = "Public NPC";
    publicNpc.role = "Guide";
    publicNpc.secrets = "NPC_SECRET_SENTINEL";
    campaign.npcs.push_back(publicNpc);
    NPCModel privateNpc = publicNpc;
    privateNpc.metadata.id = "npc_private";
    privateNpc.name = "PRIVATE_NPC_SENTINEL";
    campaign.npcs.push_back(privateNpc);
    campaign.localPlayerDisplay.visibleNpcIds = {"npc_public", "", "missing_npc"};
    const std::vector<PlayerSafeNpcSummary> npcs = buildPlayerSafeNpcSummaries(campaign);
    ok &= require(npcs.size() == 1 && npcs.front().name == "Public NPC" && npcs.front().role == "Guide",
                  "NPC projection must require an explicit, stable reveal ID and expose only name/role");

    CharacterModel publicCharacter;
    publicCharacter.metadata.id = "character_public";
    std::strncpy(publicCharacter.nameBuffer, "Ari", sizeof(publicCharacter.nameBuffer));
    publicCharacter.selectedClass = "Ranger";
    publicCharacter.level = 4;
    publicCharacter.currentHitPoints = 19;
    publicCharacter.maxHitPoints = 27;
    publicCharacter.temporaryHitPoints = 5;
    publicCharacter.speedFeet = 35;
    publicCharacter.conditions = {"Blessed"};
    publicCharacter.backstory = "CHARACTER_BACKSTORY_SECRET";
    publicCharacter.playerNotes = "CHARACTER_PLAYER_NOTE_SECRET";
    publicCharacter.gmNotes = "CHARACTER_GM_NOTE_SECRET";
    publicCharacter.portraitAssetId = "C:/private/portrait.png";
    publicCharacter.inventory.entries.push_back({});
    campaign.characters.push_back(publicCharacter);
    CharacterModel privateCharacter = publicCharacter;
    privateCharacter.metadata.id = "character_private";
    std::strncpy(privateCharacter.nameBuffer, "PRIVATE_CHARACTER_SENTINEL", sizeof(privateCharacter.nameBuffer));
    campaign.characters.push_back(privateCharacter);
    campaign.localPlayerDisplay.showParty = true;
    campaign.localPlayerDisplay.visibleCharacterIds = {"character_public", "", "missing_character"};
    const std::vector<PlayerSafeCharacterSummary> party = buildPlayerSafeCharacterSummaries(campaign);
    ok &= require(party.size() == 1 && party.front().entityId == "character_public" &&
                      party.front().name == "Ari" && party.front().characterClass == "Ranger" &&
                      party.front().level == 4 && party.front().currentHitPoints == 19 &&
                      party.front().maxHitPoints == 27 && party.front().temporaryHitPoints == 5 &&
                      party.front().speedFeet == 35 && party.front().conditions == std::vector<std::string>{"Blessed"},
                  "Party projection must expose only explicitly selected public summary fields");
    const std::string partyText = party.empty() ? std::string{} :
        party.front().entityId + party.front().name + party.front().characterClass + party.front().conditions.front();
    for (const char *secret : {"CHARACTER_BACKSTORY_SECRET", "CHARACTER_PLAYER_NOTE_SECRET",
                               "CHARACTER_GM_NOTE_SECRET", "C:/private/portrait.png", "PRIVATE_CHARACTER_SENTINEL"})
        ok &= require(!contains(partyText, secret), std::string("Party projection leaked ") + secret);
    campaign.localPlayerDisplay.showParty = false;
    ok &= require(buildPlayerSafeCharacterSummaries(campaign).empty(),
                  "Disabled party channel must fail closed even with selected IDs");

    HandoutModel publicHandout;
    publicHandout.metadata.id = "handout_public";
    publicHandout.title = "Public handout";
    publicHandout.body = "Public body";
    publicHandout.filePath = "public-image.png";
    publicHandout.gmNotes = "HANDOUT_GM_NOTE_SENTINEL";
    campaign.handouts.push_back(publicHandout);
    HandoutModel privateHandout = publicHandout;
    privateHandout.metadata.id = "handout_private";
    privateHandout.title = "PRIVATE_HANDOUT_SENTINEL";
    campaign.handouts.push_back(privateHandout);
    campaign.localPlayerDisplay.publicHandoutIds = {"handout_public"};
    const std::vector<PlayerSafeHandout> handouts = buildPlayerSafeHandouts(campaign);
    ok &= require(handouts.size() == 1 && handouts.front().title == "Public handout" && handouts.front().body == "Public body",
                  "Handout projection must require an explicit reveal and omit unselected handouts");

    StoryThreadModel thread;
    StoryBeatModel beat;
    beat.metadata.id = "scene_public";
    beat.title = "Public scene";
    beat.playerSummary = "Public summary";
    beat.readAloudText = "Public read-aloud";
    beat.summary = "SCENE_PRIVATE_SUMMARY_SENTINEL";
    beat.clues = "SCENE_CLUE_SENTINEL";
    beat.consequences = "SCENE_CONSEQUENCE_SENTINEL";
    beat.gmNotes = "SCENE_GM_NOTE_SENTINEL";
    StoryLinkModel privateLink;
    privateLink.targetEntityId = "UNREVEALED_LINK_SENTINEL";
    privateLink.targetEntityType = "NPC";
    beat.links.push_back(privateLink);
    thread.beats.push_back(beat);
    campaign.storyThreads.push_back(thread);
    campaign.localPlayerDisplay.publicSceneId = beat.metadata.id;
    const PlayerSafeStoryScene scene = buildPlayerSafeStoryScene(campaign);
    const std::string publicSceneText = scene.title + scene.playerSummary + scene.readAloudText;
    ok &= require(scene.available && contains(publicSceneText, "Public summary") && contains(publicSceneText, "Public read-aloud"),
                  "Explicitly selected scene should expose its public fields");
    for (const char *sentinel : {"SCENE_PRIVATE_SUMMARY_SENTINEL", "SCENE_CLUE_SENTINEL", "SCENE_CONSEQUENCE_SENTINEL",
                                 "SCENE_GM_NOTE_SENTINEL", "UNREVEALED_LINK_SENTINEL"})
        ok &= require(!contains(publicSceneText, sentinel), std::string("Player scene leaked ") + sentinel);

    EncounterModel encounter;
    encounter.round = 2;
    auto participant = [](const char *id, const char *name, int initiative) {
        EncounterParticipantModel value;
        value.metadata.id = id;
        value.displayName = name;
        value.initiative = initiative;
        value.currentHitPoints = 99;
        value.maxHitPoints = 123;
        value.armorClass = 27;
        return value;
    };
    encounter.participants.push_back(participant("visible", "Visible combatant", 18));
    encounter.participants.push_back(participant("hidden", "HIDDEN_INITIATIVE_SENTINEL", 17));
    encounter.participants.back().startsHidden = true;
    encounter.participants.push_back(participant("future", "FUTURE_REINFORCEMENT_SENTINEL", 16));
    encounter.participants.back().reinforcementRound = 3;
    encounter.participants.push_back(participant("arrived", "Arrived reinforcement", 15));
    encounter.participants.back().reinforcementRound = 1;
    encounter.activeParticipantIndex = 0;
    const std::vector<PlayerSafeInitiativeEntry> initiative = buildPlayerSafeInitiative(encounter);
    ok &= require(initiative.size() == 2 && initiative.front().displayName == "Visible combatant" && initiative.front().active,
                  "Initiative projection must omit hidden/future combatants while preserving visible turn state");
    ok &= require(std::any_of(initiative.begin(), initiative.end(), [](const PlayerSafeInitiativeEntry &entry) {
                      return entry.displayName == "Arrived reinforcement";
                  }), "Already-arrived reinforcement should be visible in public initiative");

    ok &= require(!playerSafeContainsId({""}, ""), "Empty entity IDs must never count as explicit reveals");
    return ok;
}

bool testPlayerDisplayReviewSnapshotChannels()
{
    bool ok = true;
    Campaign campaign;
    LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    display.mapOnly = false;
    display.showMap = false;
    display.showSharedArt = false;

    StoryThreadModel thread;
    StoryBeatModel scene;
    scene.metadata.id = "review_snapshot_scene";
    scene.title = "Public arrival";
    scene.playerSummary = "The gate is open.";
    scene.readAloudText = "Rain drums on the city walls.";
    scene.summary = "SCENE_GM_SUMMARY_SENTINEL";
    scene.clues = "SCENE_GM_CLUE_SENTINEL";
    scene.gmNotes = "SCENE_GM_NOTE_SENTINEL";
    thread.beats.push_back(scene);
    campaign.storyThreads.push_back(thread);
    display.publicSceneId = scene.metadata.id;

    EncounterModel encounter;
    encounter.metadata.id = "review_snapshot_encounter";
    encounter.name = "Gatehouse skirmish";
    encounter.round = 3;
    EncounterParticipantModel publicParticipant;
    publicParticipant.metadata.id = "review_snapshot_visible";
    publicParticipant.displayName = "Guard captain";
    publicParticipant.initiative = 17;
    publicParticipant.currentHitPoints = 42;
    publicParticipant.maxHitPoints = 42;
    publicParticipant.armorClass = 18;
    encounter.participants.push_back(publicParticipant);
    EncounterParticipantModel hiddenParticipant = publicParticipant;
    hiddenParticipant.metadata.id = "review_snapshot_hidden";
    hiddenParticipant.displayName = "HIDDEN_INITIATIVE_SENTINEL";
    hiddenParticipant.startsHidden = true;
    encounter.participants.push_back(hiddenParticipant);
    campaign.encounters.push_back(encounter);
    display.showInitiative = true;
    display.publicEncounterId = encounter.metadata.id;

    NPCModel npc;
    npc.metadata.id = "review_snapshot_npc";
    npc.name = "Mara";
    npc.role = "Gate warden";
    npc.secrets = "NPC_SECRET_SENTINEL";
    npc.goals = "NPC_GOAL_SENTINEL";
    campaign.npcs.push_back(npc);
    display.showVisibleNpcs = true;
    display.visibleNpcIds = {npc.metadata.id};

    CharacterModel character;
    character.metadata.id = "review_snapshot_character";
    std::strncpy(character.nameBuffer, "Iris", sizeof(character.nameBuffer));
    character.selectedClass = "Cleric";
    character.level = 5;
    character.currentHitPoints = 28;
    character.maxHitPoints = 34;
    character.temporaryHitPoints = 7;
    character.speedFeet = 30;
    character.conditions = {"Shielded"};
    character.backstory = "PARTY_BACKSTORY_SECRET";
    character.gmNotes = "PARTY_GM_NOTE_SECRET";
    character.inventory.entries.push_back({});
    campaign.characters.push_back(character);
    display.showParty = true;
    display.visibleCharacterIds = {character.metadata.id};

    const PlayerDisplayReviewSnapshot reviewed = PlayerDisplayReview::build(campaign, "en");
    const std::string reviewedNpcText = reviewed.playerNpcs.empty()
                                            ? std::string{}
                                            : reviewed.playerNpcs.front().name + reviewed.playerNpcs.front().role;
    const std::string reviewedNpcName = reviewed.playerNpcs.empty()
                                            ? std::string{}
                                            : reviewed.playerNpcs.front().name;
    const std::string reviewedPartyText = reviewed.playerParty.empty() ? std::string{} :
        reviewed.playerParty.front().name + reviewed.playerParty.front().characterClass +
        reviewed.playerParty.front().conditions.front();
    const std::string reviewedText = reviewed.playerScene.title + reviewed.playerScene.playerSummary +
                                    reviewed.playerScene.readAloudText + reviewed.playerEncounterName +
                                    reviewedNpcText + reviewedPartyText;
    ok &= require(reviewed.eligible && reviewed.playerScene.available &&
                      reviewed.playerScene.playerSummary == "The gate is open." &&
                      reviewed.playerEncounterName == "Gatehouse skirmish" &&
                      reviewed.playerEncounterRound == 3 &&
                      reviewed.playerInitiative.size() == 1 &&
                      reviewed.playerInitiative.front().displayName == "Guard captain" &&
                      reviewed.playerNpcs.size() == 1 && reviewed.playerNpcs.front().name == "Mara" &&
                      reviewed.playerParty.size() == 1 && reviewed.playerParty.front().name == "Iris" &&
                      reviewed.playerParty.front().temporaryHitPoints == 7,
                  "Reviewed snapshot did not copy the selected player-safe scene, initiative, and NPC channels");
    for (const char *secret : {"SCENE_GM_SUMMARY_SENTINEL", "SCENE_GM_CLUE_SENTINEL", "SCENE_GM_NOTE_SENTINEL",
                               "HIDDEN_INITIATIVE_SENTINEL", "NPC_SECRET_SENTINEL", "NPC_GOAL_SENTINEL",
                               "PARTY_BACKSTORY_SECRET", "PARTY_GM_NOTE_SECRET"})
        ok &= require(!contains(reviewedText, secret), std::string("Reviewed snapshot leaked ") + secret);

    const std::uint64_t reviewedFingerprint = reviewed.contentFingerprint;
    const std::uint64_t reviewedSource = PlayerDisplayReview::sourceFingerprint(campaign, "en");
    campaign.encounters.front().round = 4;
    const PlayerDisplayReviewSnapshot nextRound = PlayerDisplayReview::build(campaign, "en");
    ok &= require(nextRound.contentFingerprint != reviewedFingerprint &&
                      PlayerDisplayReview::sourceFingerprint(campaign, "en") != reviewedSource &&
                      !PlayerDisplayReview::isCurrent(campaign, reviewed),
                  "Changing the selected encounter round did not invalidate the review");

    campaign.storyThreads.front().beats.front().playerSummary = "Changed source text";
    campaign.encounters.front().name = "Changed encounter";
    campaign.npcs.front().name = "Changed NPC";
    campaign.characters.front().currentHitPoints = 1;
    ok &= require(reviewed.playerScene.playerSummary == "The gate is open." &&
                      reviewed.playerEncounterName == "Gatehouse skirmish" &&
                      reviewed.playerEncounterRound == 3 &&
                      reviewedNpcName == "Mara" && reviewed.playerParty.front().currentHitPoints == 28,
                  "A copied review snapshot changed when live campaign sources changed");

    display.showInitiative = false;
    display.showVisibleNpcs = false;
    display.showParty = false;
    const PlayerDisplayReviewSnapshot disabled = PlayerDisplayReview::build(campaign, "en");
    ok &= require(disabled.eligible && disabled.playerInitiative.empty() &&
                      disabled.playerEncounterName.empty() && disabled.playerEncounterRound == 0 &&
                      disabled.playerNpcs.empty() && disabled.playerParty.empty(),
                  "Disabled initiative, NPC, or party channels entered the reviewed snapshot");

    display.showParty = true;
    display.visibleCharacterIds = {"missing_party_character"};
    const PlayerDisplayReviewSnapshot unselectedParty = PlayerDisplayReview::build(campaign, "en");
    ok &= require(!unselectedParty.eligible && unselectedParty.issue == PlayerDisplayReviewIssue::PartyUnavailable &&
                      unselectedParty.playerParty.empty(),
                  "Stale party selections must fail closed without a player summary");

    display.showVisibleNpcs = true;
    display.visibleNpcIds.clear();
    display.showParty = false;
    const PlayerDisplayReviewSnapshot unselectedNpc = PlayerDisplayReview::build(campaign, "en");
    ok &= require(unselectedNpc.eligible && unselectedNpc.playerNpcs.empty(),
                  "Unselected NPCs entered the reviewed snapshot");
    return ok;
}

bool testPublicJournalCacheInvalidation()
{
    bool ok = true;
    Campaign baseline;
    baseline.localPlayerDisplay.mapOnly = false;
    baseline.localPlayerDisplay.showMap = false;
    baseline.localPlayerDisplay.showSharedArt = false;
    StoryObjectiveModel objective;
    objective.title = "Find Tessa";
    objective.playerSummary = "Search the square.";
    objective.status = StoryObjectiveStatus::Active;
    objective.playerVisible = true;
    baseline.storyObjectives.push_back(objective);
    StoryClueModel clue;
    clue.title = "Warm hatch";
    clue.playerText = "Soot marks the stone.";
    clue.status = StoryClueStatus::Shared;
    clue.playerVisible = true;
    baseline.storyClues.push_back(clue);
    SafetyBoundaryModel boundary;
    boundary.topic = "Horror";
    boundary.guidance = "Keep descriptions light.";
    boundary.shareWithPlayers = true;
    baseline.safetyProfile.boundaries.push_back(boundary);
    SafetyContentWarningModel warning;
    warning.targetEntityId = "square";
    warning.targetEntityType = "scene";
    warning.targetLabel = "Town square";
    warning.warning = "Darkness";
    baseline.safetyProfile.warnings.push_back(warning);
    const auto reviewed = PlayerDisplayReview::build(baseline, "en");
    const auto originalSource = PlayerDisplayReview::sourceFingerprint(baseline, "en");
    ok &= require(reviewed.eligible && reviewed.playerObjectives.size() == 1 &&
                      reviewed.playerClues.size() == 1 && reviewed.playerSafetyBoundaries.size() == 1 &&
                      reviewed.playerContentWarnings.size() == 1,
                  "Public journal fixture must be eligible with all four public channels");
    const auto changesPublicContent = [&](const char *label, auto edit) {
        Campaign changed = baseline;
        edit(changed);
        const auto next = PlayerDisplayReview::build(changed, "en");
        ok &= require(PlayerDisplayReview::sourceFingerprint(changed, "en") != originalSource &&
                          next.contentFingerprint != reviewed.contentFingerprint &&
                          !PlayerDisplayReview::isCurrent(changed, reviewed),
                      std::string("Cached player journal did not invalidate after ") + label);
    };
    changesPublicContent("objective title edit", [](Campaign &c) { c.storyObjectives[0].title += " tonight"; });
    changesPublicContent("objective text edit", [](Campaign &c) { c.storyObjectives[0].playerSummary += " Now."; });
    changesPublicContent("objective completion", [](Campaign &c) { c.storyObjectives[0].status = StoryObjectiveStatus::Completed; });
    changesPublicContent("objective revocation", [](Campaign &c) { c.storyObjectives[0].playerVisible = false; });
    changesPublicContent("clue title edit", [](Campaign &c) { c.storyClues[0].title += " below"; });
    changesPublicContent("clue text edit", [](Campaign &c) { c.storyClues[0].playerText += " Fresh soot."; });
    changesPublicContent("clue discovery time", [](Campaign &c) { c.storyClues[0].discoveredAtUtc = "2026-09-20T00:00:00Z"; });
    changesPublicContent("clue sharing revocation", [](Campaign &c) { c.storyClues[0].playerVisible = false; });
    changesPublicContent("clue hidden status", [](Campaign &c) { c.storyClues[0].status = StoryClueStatus::Hidden; });
    changesPublicContent("boundary guidance", [](Campaign &c) { c.safetyProfile.boundaries[0].guidance += " Fade out."; });
    changesPublicContent("boundary topic", [](Campaign &c) { c.safetyProfile.boundaries[0].topic += " and violence"; });
    changesPublicContent("boundary kind", [](Campaign &c) { c.safetyProfile.boundaries[0].kind = SafetyBoundaryKind::Veil; });
    changesPublicContent("boundary revocation", [](Campaign &c) { c.safetyProfile.boundaries[0].shareWithPlayers = false; });
    changesPublicContent("boundary deactivation", [](Campaign &c) { c.safetyProfile.boundaries[0].active = false; });
    changesPublicContent("warning text", [](Campaign &c) { c.safetyProfile.warnings[0].warning += " and fire"; });
    changesPublicContent("warning target", [](Campaign &c) { c.safetyProfile.warnings[0].targetEntityId += "_2"; });
    changesPublicContent("warning type", [](Campaign &c) { c.safetyProfile.warnings[0].targetEntityType = "map"; });
    changesPublicContent("warning label", [](Campaign &c) { c.safetyProfile.warnings[0].targetLabel += " below"; });
    changesPublicContent("warning revocation", [](Campaign &c) { c.safetyProfile.warnings[0].shareWithPlayers = false; });
    changesPublicContent("journal removal", [](Campaign &c) { c.storyObjectives.clear(); c.storyClues.clear(); });
    Campaign privateEdit = baseline;
    privateEdit.storyObjectives[0].gmNotes = "PRIVATE_OBJECTIVE";
    privateEdit.storyClues[0].gmTruth = "PRIVATE_CLUE";
    StoryClueModel hiddenClue;
    hiddenClue.playerText = "HIDDEN_CLUE";
    privateEdit.storyClues.push_back(hiddenClue);
    SafetyBoundaryModel privateBoundary;
    privateBoundary.topic = "PRIVATE_BOUNDARY";
    privateEdit.safetyProfile.boundaries.push_back(privateBoundary);
    ok &= require(PlayerDisplayReview::sourceFingerprint(privateEdit, "en") == originalSource &&
                      PlayerDisplayReview::build(privateEdit, "en").contentFingerprint == reviewed.contentFingerprint,
                  "Private journal edits must not enter the public cache key or reviewed content");
    ok &= require(reviewed.playerObjectives.front().title == "Find Tessa" &&
                      reviewed.playerClues.front().text == "Soot marks the stone.",
                  "Live journal edits must not mutate an already reviewed copy");
    return ok;
}

bool testPublicSpellSlotOptInAndCombatEnd()
{
    bool ok = true;
    Campaign campaign;
    campaign.ruleSet = std::make_shared<DND5e>();
    auto &display = campaign.localPlayerDisplay;
    display.mapOnly = false;
    display.showMap = display.showSharedArt = false;
    display.showParty = true;
    CharacterModel character;
    character.metadata.id = "slot_public";
    std::strncpy(character.nameBuffer, "Mira", sizeof(character.nameBuffer));
    character.spellcasting.slotMaximums = {9, 2, 1, 0};
    character.spellcasting.slotUsed = {0, 1, 8};
    character.gmNotes = "PRIVATE_SLOT_NOTES";
    campaign.characters.push_back(character);
    character.metadata.id = "slot_private";
    campaign.characters.push_back(character);
    ItemModel publicItem;
    publicItem.metadata.id = "public_lantern";
    publicItem.name = "Lantern";
    publicItem.description = "PRIVATE_ITEM_DESCRIPTION";
    campaign.customItems.push_back(publicItem);
    campaign.characters[0].inventory.entries.push_back({publicItem.metadata.id, 2});
    const std::vector<ItemModel> builtInItems = campaign.ruleSet->getItems();
    campaign.characters[0].inventory.entries.push_back({builtInItems.front().metadata.id, 1});
    campaign.characters[0].inventory.entries.push_back({"missing_item", 4});
    campaign.characters[0].inventory.entries.push_back({publicItem.metadata.id, 0});
    campaign.characters[0].inventory.entries.push_back({publicItem.metadata.id, -2});
    campaign.characters[1].inventory.entries.push_back({publicItem.metadata.id, 9});
    campaign.partyInventory.entries.push_back({publicItem.metadata.id, 3});
    display.visibleCharacterIds = {"slot_public"};
    auto before = PlayerDisplayReview::build(campaign, "en");
    ok &= require(!display.showPartySpellSlots && before.eligible && before.playerParty.size() == 1 &&
                      before.playerParty[0].spellSlots.empty() && before.playerInventory.empty(), "Spell slots and inventory must remain private without separate opt-ins");
    display.showPartyInventory = true;
    const auto inventoryReviewed = PlayerDisplayReview::build(campaign, "en");
    const auto inventorySource = PlayerDisplayReview::sourceFingerprint(campaign, "en");
    ok &= require(inventoryReviewed.playerInventory.size() == 3 &&
                      inventoryReviewed.playerInventory[0].ownerName == "Mira" &&
                      inventoryReviewed.playerInventory[0].quantity == 2 &&
                      inventoryReviewed.playerInventory[1].itemName == builtInItems.front().name &&
                      !inventoryReviewed.playerInventory[1].partyOwned &&
                      inventoryReviewed.playerInventory[2].partyOwned &&
                      inventoryReviewed.playerInventory[2].ownerName.empty() &&
                      !contains(inventoryReviewed.playerInventory[0].itemName, "PRIVATE"),
                  "Inventory sharing must resolve built-ins and include only selected positive, known entries and party stock");
    campaign.customItems[0].description = "CHANGED_PRIVATE_ITEM_DESCRIPTION";
    ok &= require(PlayerDisplayReview::sourceFingerprint(campaign, "en") == inventorySource &&
                      !contains(inventoryReviewed.playerInventory[0].itemName, "DESCRIPTION"),
                  "Item descriptions remain private even when the broad item-name and quantity option is enabled");
    std::strncpy(campaign.characters[1].nameBuffer, "Party Inventory", sizeof(campaign.characters[1].nameBuffer));
    display.visibleCharacterIds = {"slot_private"};
    const auto sameNamedOwner = PlayerDisplayReview::build(campaign, "en");
    ok &= require(sameNamedOwner.playerInventory.size() == 2 &&
                      sameNamedOwner.playerInventory[0].ownerName == "Party Inventory" &&
                      !sameNamedOwner.playerInventory[0].partyOwned &&
                      sameNamedOwner.playerInventory[1].partyOwned,
                  "A character named Party Inventory must remain distinct from shared campaign stock");
    display.visibleCharacterIds = {"slot_public"};
    campaign.characters[0].inventory.entries[0].quantity = 1;
    ok &= require(PlayerDisplayReview::sourceFingerprint(campaign, "en") != inventorySource &&
                      inventoryReviewed.playerInventory[0].quantity == 2,
                  "Shared inventory quantity changes must invalidate without mutating the approved copy");
    campaign.characters[0].inventory.entries[0].quantity = 2;
    display.showPartySpellSlots = true;
    const auto reviewed = PlayerDisplayReview::build(campaign, "en");
    const auto &slots = reviewed.playerParty[0].spellSlots;
    ok &= require(reviewed.playerParty.size() == 1 && slots.size() == 2 && slots[0].level == 1 &&
                      slots[0].available == 1 && slots[0].maximum == 2 && slots[1].available == 0,
                  "Selected party slot counts must be ordered, normalized and exclude cantrips/zero capacities");
    ok &= require(reviewed.contentFingerprint != before.contentFingerprint,
                  "Sharing spell slots must require a fresh review");
    const auto source = PlayerDisplayReview::sourceFingerprint(campaign, "en");
    const auto normalized = PlayerDisplayReview::sourceFingerprint(campaign, "en", true);
    campaign.characters[0].spellcasting.slotUsed[1] = 2;
    ok &= require(source != PlayerDisplayReview::sourceFingerprint(campaign, "en") &&
                      normalized != PlayerDisplayReview::sourceFingerprint(campaign, "en", true) &&
                      reviewed.playerParty[0].spellSlots[0].available == 1,
                  "Spending a slot must invalidate both refresh keys without mutating the approved snapshot");
    campaign.characters[0].spellcasting.slotUsed[1] = std::numeric_limits<int>::min();
    ok &= require(buildPlayerSafeCharacterSummaries(campaign)[0].spellSlots[0].available == 2,
                  "Malformed negative usage must clamp safely without overflow");
    display.showParty = false;
    ok &= require(buildPlayerSafeCharacterSummaries(campaign).empty(), "The party channel controls slots too");
    display.showParty = true;
    display.visibleCharacterIds.clear();
    ok &= require(buildPlayerSafeCharacterSummaries(campaign).empty(), "Slots require an explicitly selected party member");
    display.visibleCharacterIds = {"slot_public"};
    EncounterModel encounter;
    encounter.metadata.id = "finished_combat";
    encounter.name = "Rescue";
    EncounterParticipantModel participant;
    participant.displayName = "Mira";
    encounter.participants.push_back(participant);
    campaign.encounters.push_back(encounter);
    display.showInitiative = true;
    display.publicEncounterId = encounter.metadata.id;
    const auto combatSource = PlayerDisplayReview::sourceFingerprint(campaign, "en", true);
    campaign.encounters[0].combatPhase = CombatPhase::WrapUp;
    const auto ended = PlayerDisplayReview::build(campaign, "en");
    ok &= require(ended.eligible && ended.playerInitiative.empty() && ended.playerEncounterName.empty() &&
                      combatSource != PlayerDisplayReview::sourceFingerprint(campaign, "en", true),
                  "Wrapping combat must remove stale turns and require review even through the health/turn shortcut");
    return ok;
}

bool testLiveCombatStatsFingerprintPolicy()
{
    bool ok = true;
    Campaign campaign;
    LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    display.mapOnly = false;
    display.showMap = false;
    display.showSharedArt = false;
    display.showParty = true;
    display.showInitiative = true;

    CharacterModel character;
    character.metadata.id = "live_combat_party";
    std::strncpy(character.nameBuffer, "Iris", sizeof(character.nameBuffer));
    character.selectedClass = "Cleric";
    character.level = 5;
    character.currentHitPoints = 24;
    character.maxHitPoints = 34;
    character.temporaryHitPoints = 3;
    character.speedFeet = 30;
    character.conditions = {"Shielded"};
    campaign.characters.push_back(character);
    display.visibleCharacterIds = {character.metadata.id};

    EncounterModel encounter;
    encounter.metadata.id = "live_combat_encounter";
    encounter.name = "Gatehouse";
    encounter.round = 1;
    EncounterParticipantModel first;
    first.metadata.id = "live_combat_first";
    first.displayName = "Iris";
    first.initiative = 18;
    EncounterParticipantModel second;
    second.metadata.id = "live_combat_second";
    second.displayName = "Guard";
    second.initiative = 12;
    encounter.participants = {first, second};
    encounter.activeParticipantIndex = 0;
    campaign.encounters.push_back(encounter);
    display.publicEncounterId = encounter.metadata.id;

    const Campaign approvedCampaign = campaign;
    const std::uint64_t full = PlayerDisplayReview::sourceFingerprint(campaign, "en");
    const std::uint64_t normalized = PlayerDisplayReview::sourceFingerprint(campaign, "en", true);
    campaign.characters.front().currentHitPoints = 9;
    campaign.characters.front().temporaryHitPoints = 0;
    campaign.encounters.front().round = 2;
    campaign.encounters.front().activeParticipantIndex = 1;
    ok &= require(PlayerDisplayReview::sourceFingerprint(campaign, "en") != full &&
                      PlayerDisplayReview::sourceFingerprint(campaign, "en", true) == normalized,
                  "Only current/temporary HP and active turn/round should preserve the normalized public fingerprint");


    const auto rejectsAsNonCombatContent = [&](const char *label, auto edit) {
        Campaign changed = campaign;
        edit(changed);
        ok &= require(PlayerDisplayReview::sourceFingerprint(changed, "en", true) != normalized,
                      std::string("Normalized player review accepted non-combat content: ") + label);
    };
    rejectsAsNonCombatContent("party name", [](Campaign &c) { std::strncpy(c.characters[0].nameBuffer, "Mira", sizeof(c.characters[0].nameBuffer)); });
    rejectsAsNonCombatContent("party class", [](Campaign &c) { c.characters[0].selectedClass = "Fighter"; });
    rejectsAsNonCombatContent("party level", [](Campaign &c) { ++c.characters[0].level; });
    rejectsAsNonCombatContent("party maximum HP", [](Campaign &c) { ++c.characters[0].maxHitPoints; });
    rejectsAsNonCombatContent("party speed", [](Campaign &c) { c.characters[0].speedFeet = 25; });
    rejectsAsNonCombatContent("party condition", [](Campaign &c) { c.characters[0].conditions.push_back("Poisoned"); });
    rejectsAsNonCombatContent("party selection", [](Campaign &c) { c.localPlayerDisplay.visibleCharacterIds.clear(); });
    rejectsAsNonCombatContent("initiative name", [](Campaign &c) { c.encounters[0].participants[1].displayName = "Hidden guard"; });
    rejectsAsNonCombatContent("initiative value", [](Campaign &c) { ++c.encounters[0].participants[1].initiative; });
    rejectsAsNonCombatContent("initiative identity", [](Campaign &c) { c.encounters[0].participants[1].metadata.id = "replacement_guard"; });
    rejectsAsNonCombatContent("initiative roster", [](Campaign &c) { c.encounters[0].participants.push_back(c.encounters[0].participants[0]); });
    rejectsAsNonCombatContent("objective", [](Campaign &c) {
        StoryObjectiveModel objective; objective.title = "New public objective"; objective.playerVisible = true; c.storyObjectives.push_back(objective);
    });

    EncounterParticipantModel reinforcement = campaign.encounters.front().participants.front();
    reinforcement.metadata.id = "live_combat_reinforcement";
    reinforcement.displayName = "Reinforcement";
    reinforcement.reinforcementRound = 3;
    campaign.encounters.front().participants.push_back(reinforcement);
    const std::uint64_t beforeReinforcementRound = PlayerDisplayReview::sourceFingerprint(campaign, "en", true);
    campaign.encounters.front().round = 3;
    ok &= require(PlayerDisplayReview::sourceFingerprint(campaign, "en", true) != beforeReinforcementRound,
                  "A newly eligible reinforcement must invalidate the normalized player review");

    campaign = approvedCampaign;
    ok &= require(PlayerDisplayReview::sourceFingerprint(campaign, "en") == full &&
                      PlayerDisplayReview::sourceFingerprint(campaign, "en", true) == normalized,
                  "Returning the source to its approved values should clear a pending combat refresh");
    return ok;
}

bool testPlayerDisplayPublicationContract()
{
    bool ok = true;
    LocalPlayerDisplayModel display;

    ok &= require(!display.enabled && display.blackout && !display.published &&
                      !localPlayerDisplayIsPublished(display),
                  "Player display must default to a closed, fail-closed blackout");

    display.publicMapId = "map_public";
    armLocalPlayerDisplay(display);
    ok &= require(display.enabled && display.blackout && !display.published,
                  "Arming the detached window must not publish staged content");
    ok &= require(publishLocalPlayerDisplay(display) && localPlayerDisplayIsPublished(display),
                  "An enabled display should publish only after the explicit publish transition");

    const std::uint64_t publishedFingerprint = display.publishedPayloadFingerprint;
    display.publicCamera.zoom = 2.5f;
    display.publicCamera.normalizedCenterX = 0.25f;
    ok &= require(localPlayerDisplayIsPublished(display) &&
                      display.publishedPayloadFingerprint == publishedFingerprint,
                  "Moving the saved public camera must not invalidate reviewed public content");

    display.publicCamera.normalizedCenterY = 0.75f;
    display.publicCamera.followGmCamera = true;
    resetLocalPlayerDisplayCameraForMapChange(display.publicCamera);
    ok &= require(display.publicCamera.normalizedCenterX == 0.5f &&
                      display.publicCamera.normalizedCenterY == 0.5f &&
                      display.publicCamera.zoom == 1.0f && !display.publicCamera.followGmCamera,
                  "Changing the public map must begin with a whole-map camera detached from GM follow");

    display.publicSceneId = "new_unreviewed_scene";
    ok &= require(!localPlayerDisplayIsPublished(display),
                  "Changing staged public sources must invalidate the active publication");

    display.publicSceneId.clear();
    ok &= require(publishLocalPlayerDisplay(display), "Republishing a reviewed payload failed");
    display.ping = {true, 0.25f, 0.5f, 2.0f};
    display.preferredMonitorIndex = 1;
    display.playerTextScale = 1.5f;
    prepareLocalPlayerDisplayForApplicationStart(display);
    ok &= require(!display.enabled && display.blackout && !display.published && !display.ping.active &&
                      display.publicMapId == "map_public" && display.preferredMonitorIndex == 1 &&
                      display.playerTextScale == 1.5f && !publishLocalPlayerDisplay(display),
                  "Campaign reload must keep the window closed and preserve selections without allowing publication");
    armLocalPlayerDisplay(display);
    ok &= require(display.enabled && display.blackout && !display.published,
                  "Explicitly reopening after reload must start black with the saved content still staged");

    display.publicCamera.normalizedCenterX = std::numeric_limits<float>::quiet_NaN();
    display.publicCamera.normalizedCenterY = 2.0f;
    display.publicCamera.zoom = std::numeric_limits<float>::infinity();
    normalizeLocalPlayerDisplayCamera(display.publicCamera);
    ok &= require(display.publicCamera.normalizedCenterX == 0.5f &&
                      display.publicCamera.normalizedCenterY == 1.0f &&
                      display.publicCamera.zoom == 1.0f,
                  "Invalid persisted public camera data must normalize to safe finite bounds");

    disableLocalPlayerDisplay(display);
    ok &= require(!display.enabled && display.blackout && !publishLocalPlayerDisplay(display),
                  "A disabled player display must reject publication");
    return ok;
}

bool testGlobalBlackoutShortcutPolicy()
{
    bool ok = true;
    ok &= require(globalPlayerDisplayBlackoutRequested(true, true),
                  "B must request blackout even while a text field owns keyboard input");
    ok &= require(!globalPlayerDisplayBlackoutRequested(false, true),
                  "Blackout shortcut policy triggered without a B key press");
    ok &= require(!globalPlayerDisplayBlackoutRequested(true, false),
                  "An inactive player output captured the ordinary B key");

    std::vector<std::uint32_t> queuedCharacters = {
        static_cast<std::uint32_t>('a'),
        static_cast<std::uint32_t>('b'),
        static_cast<std::uint32_t>('B'),
        static_cast<std::uint32_t>(0x00e4),
        static_cast<std::uint32_t>('z')};
    bool blackoutCalled = false;
    ok &= require(handleGlobalPlayerDisplayBlackoutShortcut(
                      queuedCharacters, true, true, true,
                      [&]() { blackoutCalled = true; }) &&
                      blackoutCalled,
                  "Focused text input prevented the global blackout action");
    const std::vector<std::uint32_t> expected = {
        static_cast<std::uint32_t>('a'),
        static_cast<std::uint32_t>(0x00e4),
        static_cast<std::uint32_t>('z')};
    ok &= require(queuedCharacters == expected,
                  "Blackout must consume only B/b and preserve all unrelated text input");

    std::vector<std::uint32_t> inactiveText = {
        static_cast<std::uint32_t>('b'), static_cast<std::uint32_t>('x')};
    consumeGlobalPlayerDisplayBlackoutText(inactiveText, true, false);
    ok &= require(inactiveText.size() == 2 &&
                      inactiveText.front() == static_cast<std::uint32_t>('b'),
                  "Blackout modified the input queue without an active text field");

    std::vector<std::uint32_t> noShortcut = {
        static_cast<std::uint32_t>('b'), static_cast<std::uint32_t>('x')};
    blackoutCalled = false;
    ok &= require(!handleGlobalPlayerDisplayBlackoutShortcut(
                      noShortcut, false, true, true,
                      [&]() { blackoutCalled = true; }) &&
                      !blackoutCalled && noShortcut.size() == 2 &&
                      noShortcut.front() == static_cast<std::uint32_t>('b'),
                  "Text input was consumed without the blackout shortcut");

    std::vector<std::uint32_t> inactiveOutput = {
        static_cast<std::uint32_t>('b'), static_cast<std::uint32_t>('x')};
    blackoutCalled = false;
    ok &= require(!handleGlobalPlayerDisplayBlackoutShortcut(
                      inactiveOutput, true, false, true,
                      [&]() { blackoutCalled = true; }) &&
                      !blackoutCalled && inactiveOutput.size() == 2 &&
                      inactiveOutput.front() == static_cast<std::uint32_t>('b'),
                  "An inactive player output broke ordinary B typing in a text field");
    return ok;
}

bool testMapViewerCampaignSaveFailureRollback()
{
    bool ok = true;
    Campaign campaign;
    MapModel originalMap;
    originalMap.metadata.id = "original-map";
    originalMap.width = 1;
    originalMap.height = 1;
    originalMap.cells.resize(1);
    originalMap.cells.front().fogged = true;
    MapTokenModel originalToken;
    originalToken.metadata.id = "preserved-token";
    originalToken.hiddenFromPlayers = true;
    originalMap.tokens.push_back(originalToken);
    campaign.maps.push_back(originalMap);
    EncounterModel originalEncounter;
    originalEncounter.metadata.id = "original-encounter";
    campaign.encounters.push_back(originalEncounter);
    campaign.partyLocation.mapId = originalMap.metadata.id;
    campaign.partyLocation.x = 4;
    campaign.partyLocation.y = 7;

    int selectedMapIndex = 0;
    std::vector<int> candidates = {101, 202, 303};
    int selectedCandidateIndex = 1;
    MapViewerCampaignSaveTransaction<int> transaction(
        campaign, selectedMapIndex, candidates, selectedCandidateIndex);

    MapModel generatedMap;
    generatedMap.metadata.id = "generated-map";
    campaign.maps.push_back(generatedMap);
    EncounterModel generatedEncounter;
    generatedEncounter.metadata.id = "generated-encounter";
    campaign.encounters.push_back(generatedEncounter);
    campaign.partyLocation.mapId = generatedMap.metadata.id;
    campaign.partyLocation.x = 99;
    campaign.partyLocation.y = 88;
    selectedMapIndex = 1;
    candidates.clear();
    selectedCandidateIndex = -1;

    bool persistenceCalled = false;
    ok &= require(!transaction.persistOrRollback([&]() {
                      persistenceCalled = true;
                      return false;
                  }) &&
                      persistenceCalled,
                  "Injected save failure was not propagated by the map-viewer transaction");
    ok &= require(campaign.maps.size() == 1 &&
                      campaign.maps.front().metadata.id == "original-map" &&
                      campaign.maps.front().cells.size() == 1 &&
                      campaign.maps.front().cells.front().fogged &&
                      campaign.maps.front().tokens.size() == 1 &&
                      campaign.maps.front().tokens.front().metadata.id == "preserved-token",
                  "Save failure did not restore maps, fog, and tokens exactly");
    ok &= require(campaign.encounters.size() == 1 &&
                      campaign.encounters.front().metadata.id == "original-encounter",
                  "Save failure did not restore the encounter collection");
    ok &= require(campaign.partyLocation.mapId == "original-map" &&
                      campaign.partyLocation.x == 4 &&
                      campaign.partyLocation.y == 7,
                  "Save failure did not restore the party position");
    ok &= require(selectedMapIndex == 0 &&
                      candidates == std::vector<int>({101, 202, 303}) &&
                      selectedCandidateIndex == 1,
                  "Save failure did not restore the active map and retry candidates");

    MapViewerCampaignSaveTransaction<int> successfulTransaction(
        campaign, selectedMapIndex, candidates, selectedCandidateIndex);
    campaign.maps.push_back(generatedMap);
    selectedMapIndex = 1;
    ok &= require(successfulTransaction.persistOrRollback([]() { return true; }) &&
                      campaign.maps.size() == 2 && selectedMapIndex == 1,
                  "Successful persistence unexpectedly rolled back the accepted map");
    return ok;
}

bool testPlayerDisplayCanvasCameraCapture()
{
    bool ok = true;
    LocalPlayerDisplayCanvasCameraInput input;
    input.mapOriginX = 100.0f;
    input.mapOriginY = 50.0f;
    input.mapPixelWidth = 800.0f;
    input.mapPixelHeight = 400.0f;
    input.visibleMinimumX = 300.0f;
    input.visibleMinimumY = 100.0f;
    input.visibleMaximumX = 500.0f;
    input.visibleMaximumY = 300.0f;
    input.viewportWidth = 400.0f;
    input.viewportHeight = 200.0f;
    input.cellSize = 10.0f;
    input.mapWidth = 80;
    input.mapHeight = 40;

    LocalPlayerDisplayCameraState captured;
    captured.normalizedCenterX = 0.9f;
    captured.normalizedCenterY = 0.8f;
    captured.zoom = 3.0f;
    captured.followGmCamera = false;
    ok &= require(localPlayerDisplayCameraFromCanvas(input, captured),
                  "A valid GM editor canvas did not produce a public camera snapshot");
    ok &= require(std::fabs(captured.normalizedCenterX - 0.375f) < 0.0001f &&
                      std::fabs(captured.normalizedCenterY - 0.375f) < 0.0001f &&
                      std::fabs(captured.zoom - 2.0f) < 0.0001f && captured.followGmCamera,
                  "Canvas camera conversion did not preserve the visible center and relative zoom");

    const LocalPlayerDisplayCameraState validCapture = captured;
    input.viewportWidth = 0.0f;
    ok &= require(!localPlayerDisplayCameraFromCanvas(input, captured) &&
                      captured.normalizedCenterX == validCapture.normalizedCenterX &&
                      captured.normalizedCenterY == validCapture.normalizedCenterY &&
                      captured.zoom == validCapture.zoom,
                  "Invalid bootstrap canvas dimensions must fail closed without replacing the last camera");

    LocalPlayerDisplayModel display;
    display.publicMapId = "reviewed_map";
    armLocalPlayerDisplay(display);
    ok &= require(publishLocalPlayerDisplay(display),
                  "Could not prepare reviewed publication for camera isolation test");
    display.publishedReviewFingerprint = 0x1234u;
    const std::uint64_t payloadFingerprint = display.publishedPayloadFingerprint;
    const std::uint64_t reviewFingerprint = display.publishedReviewFingerprint;
    display.publicCamera = validCapture;
    ok &= require(display.publishedPayloadFingerprint == payloadFingerprint &&
                      display.publishedReviewFingerprint == reviewFingerprint &&
                      localPlayerDisplayIsPublished(display),
                  "Following an editor camera changed or bypassed the reviewed player payload");
    return ok;
}

bool testPlayerDisplayWindowPlacementPolicy()
{
    bool ok = true;
    LocalPlayerDisplayModel display;

    LocalPlayerDisplayWindowPlacement placement = resolveLocalPlayerDisplayWindowPlacement(display, 2, 0);
    ok &= require(placement.monitorIndex == 1 && placement.fullscreen && !placement.fallbackToMainWindow,
                  "Automatic player output did not prefer a dedicated second display");

    placement = resolveLocalPlayerDisplayWindowPlacement(display, 1, 0);
    ok &= require(placement.monitorIndex == 0 && !placement.fullscreen && placement.fallbackToMainWindow,
                  "Single-monitor automatic output did not fall back to a safe main-window surface");

    display.preferredMonitorIndex = 0;
    placement = resolveLocalPlayerDisplayWindowPlacement(display, 2, 0);
    ok &= require(placement.monitorIndex == 0 && placement.fullscreen && !placement.fallbackToMainWindow,
                  "An explicitly selected available main display was treated as a disconnected fallback");

    display.preferredMonitorIndex = 7;
    placement = resolveLocalPlayerDisplayWindowPlacement(display, 2, 0);
    ok &= require(placement.monitorIndex == 0 && !placement.fullscreen && placement.fallbackToMainWindow,
                  "A disconnected selected display did not fail closed to a window on the main display");

    display.preferredMonitorIndex = 1;
    display.mapOnly = false;
    placement = resolveLocalPlayerDisplayWindowPlacement(display, 2, 0);
    ok &= require(placement.monitorIndex == 1 && !placement.fullscreen,
                  "Panel-based player output unexpectedly entered full screen");

    display.preferredMonitorIndex = -5;
    prepareLocalPlayerDisplayForApplicationStart(display);
    ok &= require(display.preferredMonitorIndex == -1 && display.blackout && !display.published,
                  "Invalid persisted monitor selection was not normalized safely at startup");
    return ok;
}

bool testPlayerDisplayWholeMapFogContract()
{
    bool ok = true;
    Campaign campaign;
    MapModel map;
    map.metadata.id = "fog_control_map";
    map.visibility = MapVisibility::PlayersVisible;
    map.width = 2;
    map.height = 1;
    map.cells.resize(2);
    map.cells[0].fogged = true;
    map.cells[1].fogged = false;
    map.revision = 3;
    campaign.maps.push_back(map);
    LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    display.publicMapId = map.metadata.id;
    armLocalPlayerDisplay(display);
    publishLocalPlayerDisplay(display);
    display.publishedReviewFingerprint = 1;

    ok &= require(setAllPublicDisplayMapFog(campaign, false),
                  "Confirmed reveal-all operation rejected a valid public map");
    ok &= require(std::none_of(campaign.maps.front().cells.begin(), campaign.maps.front().cells.end(),
                               [](const MapCellModel &cell) { return cell.fogged; }) &&
                      campaign.maps.front().revision == 4 && display.blackout && !display.published,
                  "Reveal-all did not update every cell, revision, and fail-closed publication state");

    armLocalPlayerDisplay(display);
    publishLocalPlayerDisplay(display);
    display.publishedReviewFingerprint = 1;
    ok &= require(setAllPublicDisplayMapFog(campaign, true),
                  "Confirmed hide-all operation rejected a valid public map");
    ok &= require(std::all_of(campaign.maps.front().cells.begin(), campaign.maps.front().cells.end(),
                              [](const MapCellModel &cell) { return cell.fogged; }) &&
                      display.blackout && !display.published,
                  "Hide-all did not cover every cell and invalidate the published view");

    Campaign malformed;
    MapModel malformedMap = map;
    malformedMap.metadata.id = "malformed_fog_map";
    malformedMap.cells.resize(1);
    malformed.maps.push_back(malformedMap);
    malformed.localPlayerDisplay.publicMapId = malformedMap.metadata.id;
    ok &= require(!setAllPublicDisplayMapFog(malformed, false) && malformed.maps.front().cells.front().fogged,
                  "Malformed public map grid was implicitly resized or revealed");

    malformed.maps.front().visibility = MapVisibility::GameMasterOnly;
    malformed.maps.front().cells.resize(2);
    ok &= require(!setAllPublicDisplayMapFog(malformed, false),
                  "Private map accepted a player-display reveal-all operation");
    return ok;
}

bool testPlayerDisplayCameraPresets()
{
    bool ok = true;
    Campaign campaign;
    MapModel map;
    map.metadata.id = "camera_preset_map";
    map.visibility = MapVisibility::PlayersVisible;
    map.width = 10;
    map.height = 8;
    map.cells.resize(80);
    for (MapCellModel &cell : map.cells)
        cell.fogged = true;
    campaign.maps.push_back(map);
    campaign.localPlayerDisplay.publicMapId = map.metadata.id;

    ok &= require(!LocalPlayerDisplayCameraPresets::revealedAreaForPublicDisplay(campaign).has_value(),
                  "An all-hidden public map produced a player camera frame");

    MapModel &publicMap = campaign.maps.front();
    for (int y = 1; y <= 2; ++y)
        for (int x = 1; x <= 2; ++x)
            publicMap.cells[static_cast<std::size_t>(y * publicMap.width + x)].fogged = false;
    const auto revealed = LocalPlayerDisplayCameraPresets::revealedAreaForPublicDisplay(campaign);
    ok &= require(revealed.has_value() && !revealed->followGmCamera &&
                      std::fabs(revealed->normalizedCenterX - 0.2f) < 0.0001f &&
                      std::fabs(revealed->normalizedCenterY - 0.25f) < 0.0001f &&
                      std::fabs(revealed->zoom - (8.0f / 2.4f)) < 0.0001f,
                  "A compact revealed region did not produce its padded public camera frame");

    for (MapCellModel &cell : publicMap.cells)
        cell.fogged = false;
    const auto whole = LocalPlayerDisplayCameraPresets::wholeMapForPublicDisplay(campaign);
    const auto allRevealed = LocalPlayerDisplayCameraPresets::revealedAreaForPublicDisplay(campaign);
    ok &= require(whole.has_value() && allRevealed.has_value() && !whole->followGmCamera &&
                      std::fabs(whole->normalizedCenterX - 0.5f) < 0.0001f &&
                      std::fabs(whole->normalizedCenterY - 0.5f) < 0.0001f &&
                      std::fabs(whole->zoom - 1.0f) < 0.0001f &&
                      std::fabs(allRevealed->zoom - 1.0f) < 0.0001f,
                  "A fully revealed public map did not fit at the default public camera zoom");

    publicMap.cells.pop_back();
    ok &= require(!LocalPlayerDisplayCameraPresets::wholeMapForPublicDisplay(campaign).has_value() &&
                      !LocalPlayerDisplayCameraPresets::revealedAreaForPublicDisplay(campaign).has_value(),
                  "Malformed map geometry produced a player camera preset");
    publicMap.cells.resize(80);
    publicMap.visibility = MapVisibility::GameMasterOnly;
    ok &= require(!LocalPlayerDisplayCameraPresets::wholeMapForPublicDisplay(campaign).has_value() &&
                      !LocalPlayerDisplayCameraPresets::revealedAreaForPublicDisplay(campaign).has_value(),
                  "A private map produced a player camera preset");
    return ok;
}

bool testImmutablePlayerMapReviewPlans()
{
    bool ok = true;
    MapModel map;
    map.metadata.id = "review_map";
    map.revision = 17;
    map.visibility = MapVisibility::PlayersVisible;
    map.atlasLevel = MapLevel::Tactical;
    map.gridType = GridType::Square;
    map.playerRelease = {true, true, true, true, true};
    map.width = 2;
    map.height = 2;
    map.cells.resize(4);
    map.cells[3].fogged = true;
    map.importedReference.enabled = true;
    map.importedReference.gmOnly = true;
    map.importedReference.projectRelativePath = "assets/private-reference.png";

    auto generatedFeature = [](const char *id, bool discovered, bool gmOnly) {
        MapGeneratedFeature feature;
        feature.id = id;
        feature.kind = MapGeneratedFeatureKind::Landmark;
        feature.name = id;
        feature.discovered = discovered;
        feature.gmOnly = gmOnly;
        feature.points = {{0.5f, 0.5f}};
        return feature;
    };
    map.generation.graph.features.push_back(generatedFeature("generated_public", true, false));
    map.generation.graph.features.push_back(generatedFeature("generated_gm_only", true, true));
    map.generation.graph.features.push_back(generatedFeature("generated_unrevealed", false, false));

    auto manualFeature = [](const char *id, bool discovered, bool gmOnly) {
        MapFeatureModel feature;
        feature.metadata.id = id;
        feature.name = id;
        feature.discovered = discovered;
        feature.gmOnly = gmOnly;
        return feature;
    };
    map.features.push_back(manualFeature("manual_public", true, false));
    map.features.push_back(manualFeature("manual_gm_only", true, true));
    map.features.push_back(manualFeature("manual_unrevealed", false, false));

    MapRegionModel publicRegion;
    publicRegion.metadata.id = "region_public";
    map.regions.push_back(publicRegion);
    MapRegionModel privateRegion;
    privateRegion.metadata.id = "region_gm_only";
    privateRegion.gmOnly = true;
    map.regions.push_back(privateRegion);

    MapRouteModel publicRoute;
    publicRoute.metadata.id = "route_public";
    publicRoute.discovered = true;
    map.routes.push_back(publicRoute);
    MapRouteModel privateRoute;
    privateRoute.metadata.id = "route_unrevealed";
    privateRoute.discovered = false;
    map.routes.push_back(privateRoute);

    MapTokenModel publicToken;
    publicToken.metadata.id = "token_public";
    map.tokens.push_back(publicToken);
    MapTokenModel privateToken;
    privateToken.metadata.id = "token_hidden";
    privateToken.hiddenFromPlayers = true;
    map.tokens.push_back(privateToken);

    MapRenderPlanRequest gmRequest;
    gmRequest.audience = MapRenderAudience::GameMaster;
    gmRequest.layers = MapRenderPlanAll;
    gmRequest.applyFog = false;
    gmRequest.showPoliticalOverlay = true;
    gmRequest.showGrid = true;
    const MapRenderPlan gmPlan = compileMapRenderPlan(map, gmRequest);

    MapRenderPlanRequest playerRequest;
    playerRequest.audience = MapRenderAudience::Player;
    playerRequest.layers = MapRenderPlanTerrain | MapRenderPlanWater | MapRenderPlanWays |
                           MapRenderPlanSymbols | MapRenderPlanLabels |
                           MapRenderPlanAnnotations | MapRenderPlanRoutes |
                           MapRenderPlanTokens | MapRenderPlanFog | MapRenderPlanGrid;
    playerRequest.applyFog = true;
    playerRequest.showPoliticalOverlay = false;
    playerRequest.showGrid = true;
    const MapRenderPlan playerPlan = compileMapRenderPlan(map, playerRequest);

    const auto hasId = [](const std::vector<EntityId> &ids, const char *id) {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    };
    ok &= require(gmPlan.audience == MapRenderAudience::GameMaster && !gmPlan.applyFog && gmPlan.backgroundVisible,
                  "GM review plan must retain the complete, unfogged private map view");
    ok &= require(playerPlan.audience == MapRenderAudience::Player && playerPlan.applyFog && !playerPlan.backgroundVisible,
                  "Player review plan must force fog and suppress GM-only imported artwork");
    MapModel unmanagedLegacy = map;
    unmanagedLegacy.importedReference = {};
    unmanagedLegacy.backgroundImagePath = "C:/legacy/unreviewed-secret-map.png";
    ok &= require(compileMapRenderPlan(unmanagedLegacy, gmRequest).backgroundVisible &&
                      !compileMapRenderPlan(unmanagedLegacy, playerRequest).backgroundVisible,
                  "Unmanaged legacy background art must remain GM-visible but fail closed for players");
    ok &= require((playerPlan.layers & MapRenderPlanPolitical) == 0 && !playerPlan.showPoliticalOverlay,
                  "Player review plan unexpectedly enabled a GM political helper layer");
    ok &= require(hasId(playerPlan.generatedFeatureIds, "generated_public") &&
                      !hasId(playerPlan.generatedFeatureIds, "generated_gm_only") &&
                      !hasId(playerPlan.generatedFeatureIds, "generated_unrevealed"),
                  "Player review plan did not fail closed for private generated features");
    ok &= require(hasId(playerPlan.manualFeatureIds, "manual_public") &&
                      !hasId(playerPlan.manualFeatureIds, "manual_gm_only") &&
                      !hasId(playerPlan.manualFeatureIds, "manual_unrevealed"),
                  "Player review plan did not fail closed for private manual annotations");
    ok &= require(hasId(playerPlan.regionIds, "region_public") && !hasId(playerPlan.regionIds, "region_gm_only") &&
                      hasId(playerPlan.routeIds, "route_public") && !hasId(playerPlan.routeIds, "route_unrevealed") &&
                      hasId(playerPlan.tokenIds, "token_public") && !hasId(playerPlan.tokenIds, "token_hidden"),
                  "Player review plan exposed a private region, route, or token");

    const MapModel playerProjection = filterMapForRenderPlan(map, playerPlan);
    ok &= require(playerProjection.generation.graph.features.size() == 1 &&
                      playerProjection.features.size() == 1 && playerProjection.regions.size() == 1 &&
                      playerProjection.routes.size() == 1 && playerProjection.tokens.size() == 1 &&
                      !playerProjection.importedReference.enabled,
                  "Filtering by the immutable player plan did not produce the reviewed public projection");

    MapModel changed = map;
    ++changed.revision;
    ok &= require(!mapRenderPlanMatchesMap(playerPlan, changed),
                  "A player review plan remained eligible after the map revision changed");
    changed = map;
    changed.generation.graph.features.front().name = "Changed after review";
    const MapRenderPlan changedPlan = compileMapRenderPlan(changed, playerRequest);
    ok &= require(changedPlan.contentHash != playerPlan.contentHash,
                  "Player review content hash did not invalidate changed public map content");
    ok &= require(playerPlan.generatedFeatureIds.size() == 1 && playerPlan.generatedFeatureIds.front() == "generated_public",
                  "The immutable player review plan changed with its source map");
    changed = map;
    changed.visibility = MapVisibility::GameMasterOnly;
    ok &= require(!mapRenderPlanMatchesMap(playerPlan, changed),
                  "A player plan remained current after map publication visibility changed");
    return ok;
}

bool testPersistentPlayerMapReleaseCategories()
{
    bool ok = true;
    Campaign campaign;
    MapModel map;
    map.metadata.id = "release_category_map";
    map.name = "Secret GM map title";
    map.visibility = MapVisibility::PlayersVisible;
    map.atlasLevel = MapLevel::Region;
    map.width = 4;
    map.height = 3;
    map.cells.resize(12);

    auto generated = [](const char *id, MapGeneratedFeatureKind kind,
                        const char *material, bool closed = false) {
        MapGeneratedFeature feature;
        feature.id = id;
        feature.kind = kind;
        feature.name = id;
        feature.materialId = material;
        feature.discovered = true;
        feature.closed = closed;
        feature.points = closed
                             ? std::vector<MapFeaturePoint>{{0.0f, 0.0f}, {3.0f, 0.0f},
                                                            {3.0f, 2.0f}, {0.0f, 2.0f}}
                             : std::vector<MapFeaturePoint>{{0.5f, 0.5f}, {2.5f, 1.5f}};
        return feature;
    };
    map.generation.graph.features.push_back(generated(
        "base_terrain", MapGeneratedFeatureKind::TerrainRegion, "terrain.grass", true));
    map.generation.graph.features.push_back(generated(
        "base_water", MapGeneratedFeatureKind::WaterBody, "water.deep", true));
    map.generation.graph.features.push_back(generated(
        "base_river", MapGeneratedFeatureKind::River, "river.main"));
    map.generation.graph.features.push_back(generated(
        "base_mountains", MapGeneratedFeatureKind::MountainRange, "mountain.range"));
    map.generation.graph.features.push_back(generated(
        "base_vegetation", MapGeneratedFeatureKind::Vegetation, "forest.cluster"));
    map.generation.graph.features.push_back(generated(
        "private_border", MapGeneratedFeatureKind::TerrainRegion, "territory.realm", true));
    map.generation.graph.features.push_back(generated(
        "private_road", MapGeneratedFeatureKind::Road, "road.dirt"));
    map.generation.graph.features.push_back(generated(
        "private_place", MapGeneratedFeatureKind::Settlement, "settlement.town"));
    map.generation.graph.features.push_back(generated(
        "private_district", MapGeneratedFeatureKind::District, "district.market", true));

    MapFeatureModel annotation;
    annotation.metadata.id = "private_annotation";
    annotation.name = "private_annotation";
    annotation.description = "Public annotation description";
    annotation.discovered = true;
    map.features.push_back(annotation);
    MapRegionModel region;
    region.metadata.id = "private_region";
    region.name = "Secret region name";
    map.regions.push_back(region);
    MapRouteModel route;
    route.metadata.id = "private_route";
    route.name = "Secret route name";
    route.discovered = true;
    map.routes.push_back(route);
    MapTokenModel token;
    token.metadata.id = "private_token";
    token.name = "Secret token name";
    map.tokens.push_back(token);

    campaign.maps.push_back(map);
    campaign.localPlayerDisplay.publicMapId = map.metadata.id;
    campaign.localPlayerDisplay.showMap = true;
    campaign.localPlayerDisplay.mapOnly = true;

    const PlayerDisplayReviewSnapshot safeDefault = PlayerDisplayReview::build(campaign, "en");
    const std::uint32_t optionalLayers = MapRenderPlanPolitical | MapRenderPlanWays |
                                         MapRenderPlanSymbols | MapRenderPlanLabels |
                                         MapRenderPlanAnnotations | MapRenderPlanRoutes |
                                         MapRenderPlanTokens;
    const auto hasGenerated = [](const MapRenderPlan &plan, const char *id) {
        return std::find(plan.generatedFeatureIds.begin(), plan.generatedFeatureIds.end(), id) !=
               plan.generatedFeatureIds.end();
    };
    ok &= require(safeDefault.eligible && (safeDefault.playerMapPlan.layers & optionalLayers) == 0,
                  "A new map implicitly released optional player content categories");
    ok &= require(hasGenerated(safeDefault.playerMapPlan, "base_terrain") &&
                      hasGenerated(safeDefault.playerMapPlan, "base_water") &&
                      hasGenerated(safeDefault.playerMapPlan, "base_river") &&
                      hasGenerated(safeDefault.playerMapPlan, "base_mountains") &&
                      hasGenerated(safeDefault.playerMapPlan, "base_vegetation"),
                  "The fail-closed category profile hid the public terrain/coast/water base");
    ok &= require(!hasGenerated(safeDefault.playerMapPlan, "private_border") &&
                      !hasGenerated(safeDefault.playerMapPlan, "private_road") &&
                      !hasGenerated(safeDefault.playerMapPlan, "private_place") &&
                      !hasGenerated(safeDefault.playerMapPlan, "private_district") &&
                      safeDefault.playerMapPlan.manualFeatureIds.empty() &&
                      safeDefault.playerMapPlan.regionIds.empty() &&
                      safeDefault.playerMapPlan.routeIds.empty() &&
                      safeDefault.playerMapPlan.tokenIds.empty(),
                  "An unreleased optional category survived the player review plan");

    MapModel &releasedMap = campaign.maps.front();
    ok &= require(setMapPlayerReleaseCategory(releasedMap.playerRelease,
                                               MapPlayerReleaseCategory::PoliticalBorders, true) &&
                      setMapPlayerReleaseCategory(releasedMap.playerRelease,
                                                  MapPlayerReleaseCategory::WaysAndRoutes, true) &&
                      setMapPlayerReleaseCategory(releasedMap.playerRelease,
                                                  MapPlayerReleaseCategory::PlacesAndMarkers, true) &&
                      setMapPlayerReleaseCategory(releasedMap.playerRelease,
                                                  MapPlayerReleaseCategory::Labels, true) &&
                      setMapPlayerReleaseCategory(releasedMap.playerRelease,
                                                  MapPlayerReleaseCategory::TokensAndAnnotations, true),
                  "Explicit player category switches did not accept a first release");
    const PlayerDisplayReviewSnapshot released = PlayerDisplayReview::build(campaign, "en");
    ok &= require(!mapRenderPlanMatchesMap(safeDefault.playerMapPlan, releasedMap),
                  "A profile-only publication change left the old player plan current");
    ok &= require(released.eligible &&
                      (released.playerMapPlan.layers & optionalLayers) == optionalLayers &&
                      released.playerMapPlan.showPoliticalOverlay,
                  "Explicit releases did not map to all reviewed player layers");
    ok &= require(hasGenerated(released.playerMapPlan, "private_border") &&
                      hasGenerated(released.playerMapPlan, "private_road") &&
                      hasGenerated(released.playerMapPlan, "private_place") &&
                      hasGenerated(released.playerMapPlan, "private_district") &&
                      released.playerMapPlan.manualFeatureIds.size() == 1 &&
                      released.playerMapPlan.regionIds.size() == 1 &&
                      released.playerMapPlan.routeIds.size() == 1 &&
                      released.playerMapPlan.tokenIds.size() == 1,
                  "Explicit releases did not include the corresponding reviewed objects");
    ok &= require(released.contentFingerprint != safeDefault.contentFingerprint,
                  "Changing persistent player categories did not invalidate the review fingerprint");

    ok &= require(PlayerDisplayReview::arm(campaign, released) &&
                      PlayerDisplayReview::publish(campaign, released),
                  "Could not publish the explicitly released category review");
    setMapPlayerReleaseCategory(releasedMap.playerRelease,
                                MapPlayerReleaseCategory::TokensAndAnnotations, false);
    ok &= require(!PlayerDisplayReview::publishedStateIsCurrent(campaign, "en"),
                  "Hiding a released category left the previous publication current");

    MapModel labelsOnly = map;
    labelsOnly.playerRelease.labels = true;
    MapRenderPlanRequest labelRequest;
    labelRequest.audience = MapRenderAudience::Player;
    labelRequest.layers = PlayerDisplayReview::playerMapLayers(labelsOnly);
    const MapRenderPlan labelPlan = compileMapRenderPlan(labelsOnly, labelRequest);
    const bool hasLabelPrimitive = std::any_of(labelPlan.primitives.begin(), labelPlan.primitives.end(),
                                                [](const MapRenderPrimitive &primitive) {
                                                    return primitive.kind == MapRenderPrimitiveKind::Label;
                                                });
    const bool hasPlaceSymbol = std::any_of(labelPlan.primitives.begin(), labelPlan.primitives.end(),
                                             [](const MapRenderPrimitive &primitive) {
                                                 return primitive.kind == MapRenderPrimitiveKind::GeneratedSymbol;
                                             });
    const bool hasDistrictGeometry = std::any_of(labelPlan.primitives.begin(), labelPlan.primitives.end(),
                                                  [](const MapRenderPrimitive &primitive) {
                                                      return primitive.sourceId == "private_district" &&
                                                             primitive.kind != MapRenderPrimitiveKind::Label;
                                                  });
    ok &= require(hasLabelPrimitive && !hasPlaceSymbol && !hasDistrictGeometry &&
                      (labelPlan.layers & MapRenderPlanSymbols) == 0,
                  "Names/labels could not be released independently from place symbols");

    MapModel contentWithoutLabels = map;
    contentWithoutLabels.playerRelease.waysAndRoutes = true;
    contentWithoutLabels.playerRelease.placesAndMarkers = true;
    contentWithoutLabels.playerRelease.tokensAndAnnotations = true;
    MapRenderPlanRequest contentRequest;
    contentRequest.audience = MapRenderAudience::Player;
    contentRequest.layers = MapRenderPlanAll;
    const MapRenderPlan contentPlan = compileMapRenderPlan(contentWithoutLabels, contentRequest);
    const MapModel contentProjection = filterMapForRenderPlan(contentWithoutLabels, contentPlan);
    const bool primitiveNameLeak = std::any_of(contentPlan.primitives.begin(), contentPlan.primitives.end(),
                                                [](const MapRenderPrimitive &primitive) {
                                                    return !primitive.label.empty();
                                                });
    ok &= require(!primitiveNameLeak && contentProjection.name.empty() &&
                      std::all_of(contentProjection.generation.graph.features.begin(),
                                  contentProjection.generation.graph.features.end(),
                                  [](const MapGeneratedFeature &feature) { return feature.name.empty(); }) &&
                      std::all_of(contentProjection.features.begin(), contentProjection.features.end(),
                                  [](const MapFeatureModel &feature) { return feature.name.empty(); }) &&
                      std::all_of(contentProjection.routes.begin(), contentProjection.routes.end(),
                                  [](const MapRouteModel &routeValue) { return routeValue.name.empty(); }) &&
                      std::all_of(contentProjection.tokens.begin(), contentProjection.tokens.end(),
                                  [](const MapTokenModel &tokenValue) { return tokenValue.name.empty(); }) &&
                      contentProjection.features.front().description == "Public annotation description",
                  "Released geometry exposed names while the Names/labels category was disabled");

    MapModel legacyRaster;
    legacyRaster.metadata.id = "legacy_release_raster";
    legacyRaster.visibility = MapVisibility::PlayersVisible;
    legacyRaster.width = 6;
    legacyRaster.height = 1;
    legacyRaster.cells.resize(6);
    legacyRaster.cells[0].kind = MapTileKind::Road;
    legacyRaster.cells[0].wayKind = MapWayKind::DirtRoad;
    legacyRaster.cells[1].kind = MapTileKind::Bridge;
    legacyRaster.cells[2].kind = MapTileKind::Settlement;
    legacyRaster.cells[3].kind = MapTileKind::Building;
    legacyRaster.cells[4].kind = MapTileKind::Landmark;
    legacyRaster.cells[5].kind = MapTileKind::Water;
    legacyRaster.cells[5].wayKind = MapWayKind::River;
    MapRenderPlanRequest legacyRequest;
    legacyRequest.audience = MapRenderAudience::Player;
    legacyRequest.layers = MapRenderPlanAll;
    const MapModel hiddenLegacyRaster = filterMapForRenderPlan(
        legacyRaster, compileMapRenderPlan(legacyRaster, legacyRequest));
    ok &= require(hiddenLegacyRaster.cells[0].kind == MapTileKind::Empty &&
                      hiddenLegacyRaster.cells[0].wayKind == MapWayKind::None &&
                      hiddenLegacyRaster.cells[1].kind == MapTileKind::Empty &&
                      hiddenLegacyRaster.cells[2].kind == MapTileKind::Empty &&
                      hiddenLegacyRaster.cells[3].kind == MapTileKind::Empty &&
                      hiddenLegacyRaster.cells[4].kind == MapTileKind::Empty &&
                      hiddenLegacyRaster.cells[5].kind == MapTileKind::Empty &&
                      hiddenLegacyRaster.cells[5].wayKind == MapWayKind::None,
                  "Legacy raster roads or places bypassed the fail-closed category profile");
    legacyRaster.playerRelease.waysAndRoutes = true;
    legacyRaster.playerRelease.placesAndMarkers = true;
    const MapModel releasedLegacyRaster = filterMapForRenderPlan(
        legacyRaster, compileMapRenderPlan(legacyRaster, legacyRequest));
    ok &= require(releasedLegacyRaster.cells[0].kind == MapTileKind::Empty &&
                      releasedLegacyRaster.cells[0].wayKind == MapWayKind::None &&
                      releasedLegacyRaster.cells[1].kind == MapTileKind::Empty &&
                      releasedLegacyRaster.cells[2].kind == MapTileKind::Settlement &&
                      releasedLegacyRaster.cells[3].kind == MapTileKind::Building &&
                      releasedLegacyRaster.cells[4].kind == MapTileKind::Landmark &&
                      releasedLegacyRaster.cells[5].kind == MapTileKind::Empty &&
                      releasedLegacyRaster.cells[5].wayKind == MapWayKind::None,
                  "Player release restored an unprovenanced legacy raster road or river");

    MapRouteModel legacyRoute;
    legacyRoute.metadata.id = "released_legacy_route";
    legacyRoute.startX = 0;
    legacyRoute.endX = 5;
    legacyRaster.routes.push_back(legacyRoute);
    const MapRenderPlan legacyRoutePlan = compileMapRenderPlan(legacyRaster, legacyRequest);
    const auto legacyRoutePrimitive = std::find_if(
        legacyRoutePlan.primitives.begin(), legacyRoutePlan.primitives.end(),
        [](const MapRenderPrimitive &primitive) {
            return primitive.kind == MapRenderPrimitiveKind::Route &&
                   primitive.sourceId == "released_legacy_route";
        });
    ok &= require(legacyRoutePrimitive != legacyRoutePlan.primitives.end() &&
                      legacyRoutePrimitive->assetRole != "route_network",
                  "A public route reconstructed private geometry from an unprovenanced raster network");

    MapModel projectedLinear;
    projectedLinear.metadata.id = "private_projected_linear";
    projectedLinear.visibility = MapVisibility::PlayersVisible;
    projectedLinear.width = 4;
    projectedLinear.height = 1;
    projectedLinear.cells.resize(4);
    projectedLinear.playerRelease.waysAndRoutes = true;
    projectedLinear.cells[0].kind = MapTileKind::Road;
    projectedLinear.cells[0].wayKind = MapWayKind::DirtRoad;
    projectedLinear.cells[1].kind = MapTileKind::Road;
    projectedLinear.cells[1].wayKind = MapWayKind::DirtRoad;
    projectedLinear.cells[2].kind = MapTileKind::Water;
    projectedLinear.cells[2].wayKind = MapWayKind::River;
    projectedLinear.cells[3].kind = MapTileKind::Water;
    projectedLinear.cells[3].wayKind = MapWayKind::River;
    MapGeneratedFeature publicRoad;
    publicRoad.id = "public_projected_road";
    publicRoad.kind = MapGeneratedFeatureKind::Road;
    publicRoad.discovered = true;
    publicRoad.points = {{0.5f, 0.5f}, {1.5f, 0.5f}};
    projectedLinear.generation.graph.features.push_back(publicRoad);
    MapGeneratedFeature privateRoad = publicRoad;
    privateRoad.id = "private_projected_road";
    privateRoad.gmOnly = true;
    projectedLinear.generation.graph.features.push_back(privateRoad);
    MapGeneratedFeature publicRiver = publicRoad;
    publicRiver.id = "public_projected_river";
    publicRiver.kind = MapGeneratedFeatureKind::River;
    publicRiver.points = {{2.5f, 0.5f}, {3.5f, 0.5f}};
    projectedLinear.generation.graph.features.push_back(publicRiver);
    MapGeneratedFeature privateRiver = publicRiver;
    privateRiver.id = "private_projected_river";
    privateRiver.gmOnly = true;
    projectedLinear.generation.graph.features.push_back(privateRiver);
    const MapRenderPlan projectedLinearPlan = compileMapRenderPlan(projectedLinear, legacyRequest);
    const MapModel projectedLinearPlayer = filterMapForRenderPlan(
        projectedLinear, projectedLinearPlan);
    ok &= require(projectedLinearPlayer.generation.graph.features.size() == 2 &&
                      std::all_of(projectedLinearPlayer.cells.begin(),
                                  projectedLinearPlayer.cells.end(),
                                  [](const MapCellModel &cell) {
                                      return cell.kind == MapTileKind::Empty &&
                                             cell.wayKind == MapWayKind::None;
                                  }),
                  "Private road or river geometry survived through synchronized player raster cells");

    MapModel directSafety = contentWithoutLabels;
    directSafety.cells.back().fogged = true;
    MapLayerModel hiddenLayer;
    hiddenLayer.metadata.id = "hidden_player_layer";
    hiddenLayer.visible = false;
    directSafety.layers.push_back(hiddenLayer);
    MapTokenModel hiddenLayerToken;
    hiddenLayerToken.metadata.id = "hidden_layer_token";
    hiddenLayerToken.layerId = hiddenLayer.metadata.id;
    directSafety.tokens.push_back(hiddenLayerToken);
    MapTokenModel foggedToken;
    foggedToken.metadata.id = "fogged_token";
    foggedToken.x = 3.0f;
    foggedToken.y = 2.0f;
    directSafety.tokens.push_back(foggedToken);
    MapFeatureModel foggedFeature;
    foggedFeature.metadata.id = "fogged_annotation";
    foggedFeature.discovered = true;
    foggedFeature.x = 3;
    foggedFeature.y = 2;
    directSafety.features.push_back(foggedFeature);
    const MapRenderPlan directSafetyPlan = compileMapRenderPlan(directSafety, contentRequest);
    ok &= require(std::find(directSafetyPlan.tokenIds.begin(), directSafetyPlan.tokenIds.end(),
                            "hidden_layer_token") == directSafetyPlan.tokenIds.end() &&
                      std::find(directSafetyPlan.tokenIds.begin(), directSafetyPlan.tokenIds.end(),
                                "fogged_token") == directSafetyPlan.tokenIds.end() &&
                      std::find(directSafetyPlan.manualFeatureIds.begin(),
                                directSafetyPlan.manualFeatureIds.end(),
                                "fogged_annotation") == directSafetyPlan.manualFeatureIds.end(),
                  "Direct player render plans bypassed layer visibility or fog filtering");
    return ok;
}

bool testReviewedPlayerDisplayPublication()
{
    bool ok = true;
    Campaign campaign;
    MapModel map;
    map.metadata.id = "reviewed_public_map";
    map.revision = 9;
    map.visibility = MapVisibility::PlayersVisible;
    map.atlasLevel = MapLevel::World;
    map.width = 2;
    map.height = 1;
    map.cells.resize(2);
    map.generation.enabled = true;
    map.generation.continuousRendering = true;
    map.playerRelease.placesAndMarkers = true;
    map.playerRelease.labels = true;
    map.cells[0].kind = MapTileKind::Landmark;
    map.cells[1].kind = MapTileKind::Settlement;
    map.cells[1].fogged = true;

    MapGeneratedFeature publicFeature;
    publicFeature.id = "reviewed_public_feature";
    publicFeature.kind = MapGeneratedFeatureKind::Landmark;
    publicFeature.name = "Known shrine";
    publicFeature.discovered = true;
    publicFeature.points = {{0.5f, 0.5f}};
    map.generation.graph.features.push_back(publicFeature);
    MapGeneratedFeature privateFeature = publicFeature;
    privateFeature.id = "reviewed_private_feature";
    privateFeature.name = "SECRET FEATURE";
    privateFeature.gmOnly = true;
    map.generation.graph.features.push_back(privateFeature);
    MapGeneratedFeature foggedFeature = publicFeature;
    foggedFeature.id = "reviewed_fogged_feature";
    foggedFeature.kind = MapGeneratedFeatureKind::Settlement;
    foggedFeature.name = "FOGGED SETTLEMENT";
    foggedFeature.points = {{1.5f, 0.5f}};
    map.generation.graph.features.push_back(foggedFeature);
    MapGeneratedFeature privateNationFeature = publicFeature;
    privateNationFeature.id = "reviewed_private_nation";
    privateNationFeature.kind = MapGeneratedFeatureKind::Nation;
    privateNationFeature.name = "Hidden Realm";
    privateNationFeature.gmOnly = false; // Campaign visibility must still win.
    map.generation.graph.features.push_back(privateNationFeature);

    MapGeneratedFeature unmappedNationFeature = publicFeature;
    unmappedNationFeature.id = "reviewed_unmapped_nation";
    unmappedNationFeature.kind = MapGeneratedFeatureKind::Nation;
    unmappedNationFeature.name = "Unmapped Realm";
    map.generation.graph.features.push_back(unmappedNationFeature);
    MapGeneratedFeature unmappedPlaceFeature = publicFeature;
    unmappedPlaceFeature.id = "reviewed_unmapped_place";
    unmappedPlaceFeature.kind = MapGeneratedFeatureKind::Settlement;
    unmappedPlaceFeature.name = "Unmapped Town";
    map.generation.graph.features.push_back(unmappedPlaceFeature);
    MapGeneratedFeature ambiguousPlaceFeature = publicFeature;
    ambiguousPlaceFeature.id = "reviewed_ambiguous_place";
    ambiguousPlaceFeature.kind = MapGeneratedFeatureKind::Landmark;
    ambiguousPlaceFeature.name = "Twin Marker";
    map.generation.graph.features.push_back(ambiguousPlaceFeature);
    campaign.maps.push_back(map);

    WorldPlaceModel publicPlace;
    publicPlace.name = publicFeature.name;
    publicPlace.linkedMapId = map.metadata.id;
    publicPlace.linkedFeatureId = publicFeature.id;
    publicPlace.visibility = WorldObjectVisibility::Public;
    campaign.worldPlaces.push_back(publicPlace);
    WorldPlaceModel foggedPlace;
    foggedPlace.name = foggedFeature.name;
    foggedPlace.linkedMapId = map.metadata.id;
    foggedPlace.linkedFeatureId = foggedFeature.id;
    foggedPlace.visibility = WorldObjectVisibility::Public;
    campaign.worldPlaces.push_back(foggedPlace);
    WorldPlaceModel ambiguousPlaceA;
    ambiguousPlaceA.name = ambiguousPlaceFeature.name;
    ambiguousPlaceA.linkedMapId = map.metadata.id;
    ambiguousPlaceA.visibility = WorldObjectVisibility::Public;
    campaign.worldPlaces.push_back(ambiguousPlaceA);
    WorldPlaceModel ambiguousPlaceB = ambiguousPlaceA;
    ambiguousPlaceB.metadata = EntityMetadata{"world_place"};
    campaign.worldPlaces.push_back(ambiguousPlaceB);
    NationModel privateNation;
    privateNation.name = "Hidden Realm";
    privateNation.visibility = WorldObjectVisibility::GmOnly;
    campaign.nations.push_back(privateNation);

    LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    display.publicMapId = map.metadata.id;
    display.showMap = true;
    display.mapOnly = true;

    const std::uint64_t stableReviewSource =
        PlayerDisplayReview::sourceFingerprint(campaign, "en");
    display.publicCamera.zoom = 2.0f;
    ok &= require(PlayerDisplayReview::sourceFingerprint(campaign, "en") ==
                      stableReviewSource,
                  "Public camera motion must not invalidate the reviewed-content cache");
    const std::uint64_t stablePayload = localPlayerDisplayPayloadFingerprint(display);
    display.playerTextScale = 1.8f;
    ok &= require(localPlayerDisplayPayloadFingerprint(display) == stablePayload &&
                      PlayerDisplayReview::sourceFingerprint(campaign, "en") == stableReviewSource,
                  "Player text scale must remain presentation-only outside reviewed content fingerprints");
    display.playerTextScale = std::numeric_limits<float>::quiet_NaN();
    prepareLocalPlayerDisplayForApplicationStart(display);
    ok &= require(std::abs(display.playerTextScale - 1.15f) < 0.001f,
                  "Invalid persisted player text scale must return to the safe default");
    campaign.maps.front().cells.front().fogged = true;
    ok &= require(PlayerDisplayReview::sourceFingerprint(campaign, "en") !=
                      stableReviewSource,
                  "Fog changes must invalidate the reviewed-content cache even before a revision bump");
    campaign.maps.front().cells.front().fogged = false;
    campaign.maps.front().generation.recipe.seed += 1u;
    ok &= require(PlayerDisplayReview::sourceFingerprint(campaign, "en") !=
                      stableReviewSource,
                  "Render seed changes must invalidate the reviewed-content cache even before a revision bump");
    campaign.maps.front().generation.recipe.seed -= 1u;
    campaign.worldPlaces.front().visibility = WorldObjectVisibility::GmOnly;
    ok &= require(PlayerDisplayReview::sourceFingerprint(campaign, "en") !=
                      stableReviewSource,
                  "World-entity publication changes must invalidate the reviewed-content cache");
    campaign.worldPlaces.front().visibility = WorldObjectVisibility::Public;

    const PlayerDisplayReviewSnapshot review = PlayerDisplayReview::build(campaign, "en");
    ok &= require(review.eligible && review.hasMapPreview && review.contentFingerprint != 0,
                  "A valid public map did not produce an eligible reviewed publication snapshot");
    ok &= require(review.playerMapPlan.audience == MapRenderAudience::Player &&
                      !review.playerMapPlan.showPoliticalOverlay &&
                      review.playerMapPlan.generatedFeatureIds.size() == 1 &&
                      review.playerMapPlan.generatedFeatureIds.front() == publicFeature.id,
                  "Reviewed player snapshot retained a private, fogged, unmapped, or ambiguously mapped world feature");
    const MapModel reviewedProjection = filterMapForRenderPlan(campaign.maps.front(), review.playerMapPlan);
    ok &= require(reviewedProjection.cells.size() == 2 &&
                      reviewedProjection.cells[0].kind == MapTileKind::Landmark &&
                      reviewedProjection.cells[1].kind == MapTileKind::Empty,
                  "A point feature removed by the reviewed plan survived through its raster projection");
    ok &= require(PlayerDisplayReview::arm(campaign, review) && display.enabled && display.blackout,
                  "Reviewed arm transition failed to keep the player window in blackout");
    ok &= require(PlayerDisplayReview::publish(campaign, review) &&
                      display.publishedReviewFingerprint == review.contentFingerprint &&
                      PlayerDisplayReview::publishedStateIsCurrent(campaign, "en"),
                  "Reviewed publish transition did not retain its content proof");

    campaign.maps.front().generation.graph.features.front().name = "Changed after review";
    ok &= require(!PlayerDisplayReview::isCurrent(campaign, review) &&
                      !PlayerDisplayReview::publishedStateIsCurrent(campaign, "en"),
                  "Changing player-visible map content did not invalidate the reviewed publication");
    blackoutLocalPlayerDisplay(display);
    ok &= require(display.publishedReviewFingerprint == 0,
                  "Blackout did not clear the reviewed publication proof");

    Campaign mapOnlyWithoutMap;
    StoryThreadModel thread;
    StoryBeatModel scene;
    scene.metadata.id = "public_scene_without_map";
    thread.beats.push_back(scene);
    mapOnlyWithoutMap.storyThreads.push_back(thread);
    mapOnlyWithoutMap.localPlayerDisplay.publicSceneId = scene.metadata.id;
    mapOnlyWithoutMap.localPlayerDisplay.mapOnly = true;
    mapOnlyWithoutMap.localPlayerDisplay.showMap = false;
    const PlayerDisplayReviewSnapshot blocked = PlayerDisplayReview::build(mapOnlyWithoutMap, "en");
    ok &= require(!blocked.eligible && blocked.issue == PlayerDisplayReviewIssue::MapOnlyNeedsPublicMap,
                  "Map-only publication without a public map did not fail closed");

    Campaign sharedArtCampaign;
    MapModel artMap;
    artMap.metadata.id = "shared_art_map";
    artMap.visibility = MapVisibility::PlayersVisible;
    artMap.width = 1;
    artMap.height = 1;
    artMap.cells.resize(1);
    artMap.importedReference.enabled = true;
    artMap.importedReference.gmOnly = false;
    artMap.importedReference.projectRelativePath = "assets/shared-art.png";
    sharedArtCampaign.maps.push_back(artMap);
    sharedArtCampaign.localPlayerDisplay.publicMapId = artMap.metadata.id;
    sharedArtCampaign.localPlayerDisplay.showMap = false;
    sharedArtCampaign.localPlayerDisplay.showSharedArt = true;
    sharedArtCampaign.localPlayerDisplay.mapOnly = false;
    ok &= require(localPlayerDisplayHasPublicPayload(sharedArtCampaign) &&
                      PlayerDisplayReview::build(sharedArtCampaign, "en").eligible,
                  "Fully revealed shared map art did not count as a reviewed public payload");

    Campaign mapRecoveryCampaign;
    StoryThreadModel recoveryThread;
    StoryBeatModel recoveryScene;
    recoveryScene.title = "A safe scene after the map is removed";
    recoveryScene.playerSummary = "The public clue remains available.";
    recoveryThread.beats.push_back(recoveryScene);
    mapRecoveryCampaign.storyThreads.push_back(recoveryThread);
    MapModel privateRecoveryMap = artMap;
    privateRecoveryMap.metadata.id = "private_recovery_map";
    privateRecoveryMap.visibility = MapVisibility::GameMasterOnly;
    mapRecoveryCampaign.maps.push_back(privateRecoveryMap);
    LocalPlayerDisplayModel &recoveryDisplay = mapRecoveryCampaign.localPlayerDisplay;
    recoveryDisplay.publicSceneId = recoveryScene.metadata.id;
    recoveryDisplay.publicMapId = privateRecoveryMap.metadata.id;
    recoveryDisplay.mapOnly = true;
    recoveryDisplay.showMap = false;
    recoveryDisplay.showSharedArt = true;
    armLocalPlayerDisplay(recoveryDisplay);
    publishLocalPlayerDisplay(recoveryDisplay);
    const PlayerDisplayReviewSnapshot unavailableMapReview = PlayerDisplayReview::build(mapRecoveryCampaign, "en");
    ok &= require(!unavailableMapReview.eligible && unavailableMapReview.issue == PlayerDisplayReviewIssue::MapUnavailable,
                  "Private selected shared art did not block review before recovery");
    recoveryDisplay.mapOnly = false;
    recoveryDisplay.showMap = false;
    recoveryDisplay.showSharedArt = false;
    blackoutLocalPlayerDisplay(recoveryDisplay);
    const PlayerDisplayReviewSnapshot recoveredWithoutMap = PlayerDisplayReview::build(mapRecoveryCampaign, "en");
    ok &= require(recoveredWithoutMap.eligible && !recoveredWithoutMap.hasMapPreview &&
                      recoveryDisplay.blackout && !recoveryDisplay.published,
                  "Removing map-only and shared-art channels did not recover safely in blackout");

    armLocalPlayerDisplay(sharedArtCampaign.localPlayerDisplay);
    ok &= require(publishLocalPlayerDisplay(sharedArtCampaign.localPlayerDisplay) &&
                      !PlayerDisplayReview::publishedStateIsCurrent(sharedArtCampaign, "en"),
                  "A legacy direct publish unexpectedly bypassed the reviewed-content proof");
    return ok;
}

bool testReviewedPublicationTransactions()
{
    bool ok = true;
    Campaign campaign;
    StoryThreadModel thread;
    StoryBeatModel scene;
    scene.title = "Reviewed opening";
    scene.playerSummary = "The public square";
    thread.beats.push_back(scene);
    campaign.storyThreads.push_back(thread);
    campaign.localPlayerDisplay.mapOnly = false;
    campaign.localPlayerDisplay.showMap = false;
    campaign.localPlayerDisplay.showSharedArt = false;
    campaign.localPlayerDisplay.publicSceneId = scene.metadata.id;
    const auto review = PlayerDisplayReview::build(campaign, "en");
    ok &= require(review.eligible, "Transaction fixture has no reviewable player content");
    int saves = 0;
    const PlayerDisplayReview::Persist fail = [&](Campaign &) { ++saves; return false; };
    const PlayerDisplayReview::Persist throwing = [&](Campaign &) -> bool { ++saves; throw std::runtime_error("injected disk failure"); };
    const PlayerDisplayReview::Persist save = [&](Campaign &) { ++saves; return true; };
    ok &= require(!PlayerDisplayReview::armAndSave(campaign, review, fail).applied() &&
                      !campaign.localPlayerDisplay.enabled && campaign.localPlayerDisplay.blackout &&
                      PlayerDisplayReview::isCurrent(campaign, review),
                  "Failed arm changed the committed enabled state or invalidated a retryable review");
    ok &= require(!PlayerDisplayReview::armAndSave(campaign, review, throwing).applied() &&
                      !campaign.localPlayerDisplay.enabled && !campaign.localPlayerDisplay.published,
                  "Throwing arm save exposed the player window");
    const int beforeUnarmed = saves;
    ok &= require(PlayerDisplayReview::publishAndSave(campaign, review, save).status == PlayerDisplayReview::TransactionStatus::DisplayNotArmed &&
                      saves == beforeUnarmed, "Unarmed publication was persisted");
    ok &= require(PlayerDisplayReview::armAndSave(campaign, review, [&](Campaign &staged) {
        ++saves;
        ok &= require(!campaign.localPlayerDisplay.enabled && staged.localPlayerDisplay.enabled && staged.localPlayerDisplay.blackout,
                      "Arming reached canonical state before the save completed");
        return true;
    }).applied() && campaign.localPlayerDisplay.enabled && campaign.localPlayerDisplay.blackout,
                  "A valid arm retry did not commit its enabled blackout state");
    ok &= require(!PlayerDisplayReview::publishAndSave(campaign, review, fail).applied() &&
                      campaign.localPlayerDisplay.enabled && campaign.localPlayerDisplay.blackout &&
                      !campaign.localPlayerDisplay.published && PlayerDisplayReview::isCurrent(campaign, review),
                  "Failed publication exposed output or discarded retry context");
    ok &= require(!PlayerDisplayReview::publishAndSave(campaign, review, throwing).applied() &&
                      campaign.localPlayerDisplay.blackout && !campaign.localPlayerDisplay.published,
                  "Throwing publication save exposed output");
    const int beforePublish = saves;
    ok &= require(PlayerDisplayReview::publishAndSave(campaign, review, [&](Campaign &staged) {
        ++saves;
        ok &= require(campaign.localPlayerDisplay.blackout && !campaign.localPlayerDisplay.published &&
                          staged.localPlayerDisplay.published && !staged.localPlayerDisplay.blackout,
                      "Publication reached canonical state before the save completed");
        return true;
    }).applied() && saves == beforePublish + 1 && PlayerDisplayReview::publishedStateIsCurrent(campaign, "en"),
                  "A valid publication retry did not commit the reviewed payload exactly once");
    ok &= require(!PlayerDisplayReview::armAndSave(campaign, review, fail).applied() &&
                      campaign.localPlayerDisplay.enabled && campaign.localPlayerDisplay.blackout &&
                      campaign.localPlayerDisplay.publishedReviewFingerprint == 0,
                  "Failed rearming did not fail closed for an already published display");
    campaign.storyThreads.front().beats.front().playerSummary = "Changed after review";
    const int beforeStale = saves;
    ok &= require(PlayerDisplayReview::publishAndSave(campaign, review, save).status == PlayerDisplayReview::TransactionStatus::ReviewChanged &&
                      saves == beforeStale && campaign.localPlayerDisplay.blackout,
                  "Stale review was saved or published");
    const auto updatedReview = PlayerDisplayReview::build(campaign, "en");
    ok &= require(PlayerDisplayReview::publishAndSave(campaign, updatedReview, save).applied() &&
                      PlayerDisplayReview::publishedStateIsCurrent(campaign, "en"),
                  "Fresh content review could not retry after stale review rejection");
    ok &= require(PlayerDisplayReview::publishAndSave(campaign, updatedReview, [](Campaign &staged) {
        staged.storyThreads.front().beats.front().playerSummary = "Changed while saving";
        return true;
    }).status == PlayerDisplayReview::TransactionStatus::ReviewChanged &&
                      campaign.localPlayerDisplay.blackout && !campaign.localPlayerDisplay.published &&
                      campaign.storyThreads.front().beats.front().playerSummary == "Changed after review",
                  "Content changed during persistence was published without another review");
    return ok;
}

bool testPlayerDisplayPingContract()
{
    bool ok = true;
    Campaign campaign;
    MapModel map;
    map.metadata.id = "ping_map";
    map.visibility = MapVisibility::PlayersVisible;
    map.width = 2;
    map.height = 1;
    map.cells.resize(2);
    map.cells[1].fogged = true;
    campaign.maps.push_back(map);

    LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    display.publicMapId = map.metadata.id;
    display.showMap = true;
    armLocalPlayerDisplay(display);
    ok &= require(localPlayerDisplayHasPublicPayload(campaign),
                  "A selected PlayersVisible map should count as a publishable payload");
    ok &= require(publishLocalPlayerDisplay(display), "Could not publish the ping test payload");
    ok &= require(!startPlayerSafeDisplayPing(campaign, 0.25f, 0.5f),
                  "A ping without a reviewed publication proof did not fail closed");
    display.publishedReviewFingerprint = 1;
    ok &= require(startPlayerSafeDisplayPing(campaign, 0.25f, 0.5f),
                  "A ping on a revealed player-safe cell should start");
    ok &= require(display.ping.active && display.ping.remainingSeconds == 3.0f,
                  "Player ping did not receive its expected transient lifetime");

    advancePlayerSafeDisplayPing(display, 1.0f);
    ok &= require(display.ping.active && display.ping.remainingSeconds == 2.0f,
                  "Player ping lifetime did not advance deterministically");
    advancePlayerSafeDisplayPing(display, 2.0f);
    ok &= require(!display.ping.active, "Expired player ping remained visible");

    ok &= require(!startPlayerSafeDisplayPing(campaign, 0.75f, 0.5f),
                  "A ping must not reveal a fogged map cell");
    ok &= require(!startPlayerSafeDisplayPing(campaign, -0.1f, 0.5f) &&
                      !startPlayerSafeDisplayPing(campaign, 1.1f, 0.5f),
                  "Out-of-range normalized pings must fail closed");

    blackoutLocalPlayerDisplay(display);
    ok &= require(!startPlayerSafeDisplayPing(campaign, 0.25f, 0.5f),
                  "Blackout must reject new player pings");

    campaign.maps.front().visibility = MapVisibility::PlayersKnown;
    ok &= require(!localPlayerDisplayHasPublicPayload(campaign),
                  "A PlayersKnown map must not count as publishable player content");
    return ok;
}

bool testMapViewerFogStrokeTransaction()
{
    bool ok = true;
    MapModel map;
    map.metadata.id = "fog_stroke_map";
    map.metadata.createdAtUtc = "2026-09-06T06:00:00Z";
    map.metadata.updatedAtUtc = map.metadata.createdAtUtc;
    map.width = 3;
    map.height = 2;
    map.revision = 41;
    map.cells.resize(6);
    map.cells[0].fogged = true;
    map.cells[1].fogged = true;
    map.cells[4].fogged = true;
    // Authored tactical state represents the manual cell overrides that a fog
    // edit must never rewrite.
    map.cells[4].movementCost = 3;
    map.cells[4].difficultTerrain = true;
    map.cells[4].lineOfSightBlockedMask = 5;

    MapTokenModel token;
    token.metadata.id = "stable_token";
    token.name = "Do not move";
    token.x = 2.0f;
    token.y = 1.0f;
    map.tokens.push_back(token);

    MapFeatureModel note;
    note.metadata.id = "stable_note";
    note.name = "Secret marker";
    note.gmNotes = "Fog editing must preserve this note byte-for-byte.";
    note.x = 2;
    note.y = 1;
    map.features.push_back(note);

    MapGeneratedFeature lockedFeature;
    lockedFeature.id = "locked_river";
    lockedFeature.kind = MapGeneratedFeatureKind::River;
    lockedFeature.locked = true;
    lockedFeature.points = {{0.5f, 0.5f}, {2.5f, 1.5f}};
    map.generation.graph.features.push_back(lockedFeature);

    const MapModel before = map;
    MapViewerFogStroke stroke;
    ok &= require(stroke.begin(map, false), "A complete map grid should accept a fog stroke");
    ok &= require(stroke.paint(map, 0, 0) && stroke.paint(map, 1, 0) &&
                      !stroke.paint(map, 1, 0),
                  "Fog stroke did not reveal each changed cell exactly once");
    ok &= require(stroke.changedCellCount() == 2 && map.revision == before.revision + 1,
                  "A multi-cell fog stroke must advance the map revision exactly once");
    ok &= require(!map.cells[0].fogged && !map.cells[1].fogged &&
                      mapGenerationCellEqual(map.cells[2], before.cells[2]) &&
                      mapGenerationCellEqual(map.cells[3], before.cells[3]) &&
                      mapGenerationCellEqual(map.cells[4], before.cells[4]) &&
                      mapGenerationCellEqual(map.cells[5], before.cells[5]),
                  "Fog stroke changed authored state outside its touched cells");
    ok &= require(mapGenerationVectorEqual(map.tokens, before.tokens, mapGenerationTokenEqual) &&
                      mapGenerationVectorEqual(map.features, before.features, mapGenerationAnnotationEqual) &&
                      mapGenerationVectorEqual(map.generation.graph.features,
                                               before.generation.graph.features,
                                               mapGenerationFeatureEqual),
                  "Fog stroke changed stable token/note IDs or locked generated features");
    ok &= require(stroke.rollback(map) && mapGenerationMapsEqual(map, before),
                  "Cancelling a fog stroke did not restore the exact starting map");

    ok &= require(stroke.begin(map, true) && stroke.paint(map, 2, 0) &&
                      stroke.readyToPersist(map),
                  "A changed fog stroke was not ready for its single persistence boundary");
    stroke.accept();
    ok &= require(map.cells[2].fogged && map.revision == before.revision + 1 &&
                      mapGenerationCellEqual(map.cells[4], before.cells[4]) &&
                      map.tokens.front().metadata.id == "stable_token" &&
                      map.features.front().gmNotes == before.features.front().gmNotes &&
                      map.generation.graph.features.front().locked,
                  "Committing a fog stroke changed preserved map content");

    MapModel malformed = before;
    malformed.cells.pop_back();
    MapViewerFogStroke rejected;
    ok &= require(!rejected.begin(malformed, false),
                  "A malformed map grid must fail closed before fog editing");
    return ok;
}

bool testReviewedHandoutContentBinding()
{
    bool ok = true;
    Campaign campaign;
    campaign.metadata.id = "player_handout_binding_campaign";
    HandoutModel handout;
    handout.metadata.id = "bound_player_handout";
    handout.title = "Reviewed image";
    handout.body = "Public handout body";

    const std::filesystem::path writableRoot = ApplicationPaths::writableRoot();
    const std::filesystem::path assetDirectory =
        writableRoot / "Resources" / "Campaigns" / campaign.metadata.id /
        "Assets" / "Handouts";
    const std::filesystem::path assetPath = assetDirectory / "bound_player_handout.png";
    std::error_code ignored;
    std::filesystem::remove_all(
        writableRoot / "Resources" / "Campaigns" / campaign.metadata.id, ignored);
    std::filesystem::create_directories(assetDirectory, ignored);
    {
        std::ofstream file(assetPath, std::ios::binary | std::ios::trunc);
        file << "APPROVED_HANDOUT_BYTES";
    }
    handout.filePath = std::filesystem::relative(assetPath, writableRoot).generic_string();
    campaign.handouts.push_back(handout);
    campaign.localPlayerDisplay.mapOnly = false;
    campaign.localPlayerDisplay.showMap = false;
    campaign.localPlayerDisplay.showInitiative = false;
    campaign.localPlayerDisplay.showVisibleNpcs = false;
    campaign.localPlayerDisplay.showSharedArt = false;
    campaign.localPlayerDisplay.showPublicHandouts = true;
    campaign.localPlayerDisplay.publicHandoutIds = {handout.metadata.id};

    const PlayerDisplayReviewSnapshot reviewed =
        PlayerDisplayReview::build(campaign, "de");
    ok &= require(reviewed.eligible && reviewed.playerHandouts.size() == 1 &&
                      reviewed.playerHandouts.front().assetId == handout.metadata.id &&
                      !reviewed.playerHandouts.front().contentHash.empty() &&
                      !std::filesystem::path(
                           reviewed.playerHandouts.front().projectRelativePath).is_absolute(),
                  "A managed handout was not bound to reviewed identity, safe path, and bytes");
    ok &= require(PlayerDisplayReview::arm(campaign, reviewed) &&
                      PlayerDisplayReview::publish(campaign, reviewed),
                  "A content-bound handout review could not be published");
    const std::uint64_t approvedSource =
        PlayerDisplayReview::sourceFingerprint(campaign, "de");

    {
        std::ofstream file(assetPath, std::ios::binary | std::ios::trunc);
        file << "CHANGED_HANDOUT_BYTES";
    }
    ok &= require(PlayerDisplayReview::sourceFingerprint(campaign, "de") !=
                      approvedSource &&
                      !PlayerDisplayReview::isCurrent(campaign, reviewed) &&
                      !PlayerDisplayReview::publishedStateIsCurrent(campaign, "de"),
                  "Changed handout bytes left a published review current");

    const PlayerDisplayReviewSnapshot changed =
        PlayerDisplayReview::build(campaign, "de");
    ok &= require(changed.eligible &&
                      changed.playerHandouts.front().contentHash !=
                          reviewed.playerHandouts.front().contentHash,
                  "Re-review did not bind the replacement handout bytes");
    std::filesystem::remove(assetPath, ignored);
    const PlayerDisplayReviewSnapshot missing =
        PlayerDisplayReview::build(campaign, "de");
    ok &= require(!missing.eligible &&
                      missing.issue == PlayerDisplayReviewIssue::HandoutUnavailable,
                  "A missing reviewed handout did not fail closed");

    campaign.handouts.front().filePath = assetPath.string();
    const PlayerDisplayReviewSnapshot absolute =
        PlayerDisplayReview::build(campaign, "de");
    ok &= require(!absolute.eligible &&
                      absolute.issue == PlayerDisplayReviewIssue::HandoutUnavailable &&
                      absolute.playerHandouts.empty(),
                  "An unmanaged absolute handout path entered a player review");
    std::filesystem::remove_all(
        writableRoot / "Resources" / "Campaigns" / campaign.metadata.id, ignored);
    return ok;
}

bool testPrivateMapPngExportRejection()
{
    const auto root = ApplicationPaths::writableRoot() / "private_map_png_export_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    MapModel map;
    map.width = 3;
    map.height = 2;
    resizeMapCells(map);
    for (auto &cell : map.cells)
    {
        cell.kind = MapTileKind::Forest;
        cell.fogged = false;
    }
    map.playerRelease = {true, true, true, true, true};
    map.visibility = MapVisibility::PlayersVisible;
    const MapModel staleDraft = map;
    const auto sharingBaseline = captureMapCreatorPlayerSettings(staleDraft);
    MapPngExportOptions options;
    options.playerSafe = true;
    options.pixelsPerCell = 16;
    const std::string expectedError = "Player-safe PNG export requires a map visible to players. Save the map and choose Review player view first.";
    bool ok = true;
    for (const auto visibility : {MapVisibility::GameMasterOnly, MapVisibility::PlayersKnown})
    {
        MapModel current = staleDraft;
        current.visibility = visibility;
        map = makeMapCreatorPlayerOutputSnapshot(staleDraft, sharingBaseline, {current});
        ok &= require(map.visibility == visibility && staleDraft.visibility == MapVisibility::PlayersVisible,
                      "Player PNG must use a fresh canonical revocation without changing the public authoring draft");
        MapRenderPlanRequest request;
        request.audience = MapRenderAudience::Player;
        request.layers = MapRenderPlanAll;
        const auto plan = compileMapRenderPlan(map, request);
        const auto caseRoot = root / std::to_string(static_cast<int>(visibility));
        std::filesystem::create_directories(caseRoot);
        std::string error;
        const auto whole = caseRoot / "whole.png";
        ok &= require(!exportMapPng(map, whole, options, &error) && error == expectedError &&
                          !std::filesystem::exists(whole),
                      "A private or merely known map must reject player PNG options without creating a file");
        const auto reviewed = caseRoot / "reviewed.png";
        ok &= require(!exportMapPng(map, reviewed, plan, 16, &error) && error == expectedError &&
                          !std::filesystem::exists(reviewed),
                      "A reviewed player plan must not export a whole-map private PNG");
        const auto cropped = caseRoot / "cropped.png";
        ok &= require(!exportMapPngRegion(map, cropped, plan, {0, 0, 2, 1}, 16, &error) &&
                          error == expectedError && !std::filesystem::exists(cropped),
                      "Current-view player PNG must reject a private map before creating a file");
        const auto tiled = caseRoot / "tiled";
        ok &= require(!exportMapPngTiled(map, tiled, "map", plan, 16, false, nullptr, &error) &&
                          error == expectedError && !std::filesystem::exists(tiled),
                      "Reviewed tiled player export must reject a private map before creating its directory");
        const auto tiledOptions = caseRoot / "tiled_options";
        ok &= require(!exportMapPngTiled(map, tiledOptions, "map", options, false, nullptr, &error) &&
                          error == expectedError && !std::filesystem::exists(tiledOptions),
                      "Options-based tiled player export must also reject a private map without files");
    }
    MapModel annotatedDraft = staleDraft;
    MapTokenModel revokedToken;
    revokedToken.metadata.id = "externally_revoked_png_token";
    revokedToken.name = "REVOKED_SENTINEL";
    revokedToken.x = 1.5f;
    revokedToken.y = 1.0f;
    annotatedDraft.tokens.push_back(revokedToken);
    MapModel current = staleDraft;
    current.playerRelease = {};
    const auto revokedOutput = makeMapCreatorPlayerOutputSnapshot(annotatedDraft, sharingBaseline, {current});
    MapModel noTokenBaseline = revokedOutput;
    noTokenBaseline.tokens.clear();
    std::string categoryError;
    const auto revokedPath = root / "revoked_categories.png";
    const auto noTokenPath = root / "revoked_categories_baseline.png";
    const auto stalePath = root / "stale_grant_control.png";
    const bool categoryExports = exportMapPng(revokedOutput, revokedPath, options, &categoryError) &&
        exportMapPng(noTokenBaseline, noTokenPath, options, &categoryError) &&
        exportMapPng(annotatedDraft, stalePath, options, &categoryError);
    ok &= require(categoryExports && readBytes(revokedPath) == readBytes(noTokenPath) &&
                      readBytes(stalePath) != readBytes(noTokenPath) &&
                      annotatedDraft.playerRelease.tokensAndAnnotations && annotatedDraft.tokens.size() == 1,
                  "Fresh player PNG output must remove externally revoked content while retaining draft edits and a visible positive control: " + categoryError);
    map.visibility = MapVisibility::GameMasterOnly;
    options.playerSafe = false;
    std::string error;
    const auto gmPath = root / "private_gm.png";
    ok &= require(exportMapPng(map, gmPath, options, &error) && std::filesystem::exists(gmPath),
                  "Private map PNG must remain available to the GM: " + error);
    map.visibility = MapVisibility::PlayersVisible;
    options.playerSafe = true;
    const auto publicPath = root / "public_player.png";
    ok &= require(exportMapPng(map, publicPath, options, &error) && std::filesystem::exists(publicPath),
                  "A deliberately public map must remain available to player PNG export: " + error);
    std::filesystem::remove_all(root, ignored);
    return ok;
}

bool testCampaignConsentAcrossPlayerPngExports()
{
    const auto root = ApplicationPaths::writableRoot() / "campaign_consent_png_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    Campaign campaign;
    MapModel draft;
    draft.metadata.id = "campaign_consent_map";
    draft.atlasLevel = MapLevel::World;
    draft.width = 32;
    draft.height = 16;
    draft.visibility = MapVisibility::PlayersVisible;
    draft.playerRelease.placesAndMarkers = true;
    draft.playerRelease.labels = true;
    draft.generation.enabled = true;
    draft.generation.continuousRendering = true;
    draft.generation.recipe.styleVersion = 0;
    resizeMapCells(draft);
    for (auto &cell : draft.cells)
    {
        cell.kind = MapTileKind::Dirt;
        cell.fogged = false;
    }
    const auto addPlace = [&](const char *id, const char *name, float x, bool gmOnly = false) {
        MapGeneratedFeature feature;
        feature.id = id;
        feature.name = name;
        feature.kind = MapGeneratedFeatureKind::Settlement;
        feature.points = {{x, 8.5f}};
        feature.discovered = true;
        feature.gmOnly = gmOnly;
        draft.generation.graph.features.push_back(feature);
        draft.cells[8 * draft.width + static_cast<int>(x)].kind = MapTileKind::Settlement;
    };
    addPlace("public", "Havenfall QA", 5.5f);
    addPlace("unlinked", "UNLINKED SENTINEL", 11.5f);
    addPlace("hidden", "HIDDEN SENTINEL", 17.5f);
    addPlace("ambiguous", "AMBIGUOUS SENTINEL", 23.5f);
    addPlace("gm_only", "GM SENTINEL", 28.5f, true);
    const auto addLink = [&](const char *id, WorldObjectVisibility visibility) {
        WorldPlaceModel place;
        place.linkedMapId = draft.metadata.id;
        place.linkedFeatureId = id;
        place.visibility = visibility;
        campaign.worldPlaces.push_back(place);
    };
    addLink("public", WorldObjectVisibility::Public);
    addLink("hidden", WorldObjectVisibility::GmOnly);
    addLink("ambiguous", WorldObjectVisibility::Public);
    addLink("ambiguous", WorldObjectVisibility::Public);
    addLink("gm_only", WorldObjectVisibility::Public);
    campaign.maps.push_back(draft);
    const auto baseline = captureMapCreatorPlayerSettings(draft);
    const MapModel output = makeMapCreatorCampaignPlayerOutputSnapshot(draft, baseline, campaign);
    MapRenderPlanRequest request;
    request.audience = MapRenderAudience::Player;
    request.locale = "en";
    const auto plan = compileMapRenderPlan(output, request);
    bool ok = require(plan.generatedFeatureIds == std::vector<EntityId>{"public"},
                      "Campaign player snapshot must retain the public place and withhold unlinked, hidden, ambiguous and GM-only places");
    MapModel expected = draft;
    expected.generation.graph.features.resize(1);
    for (const int x : {11, 17, 23, 28})
    {
        expected.cells[8 * draft.width + x].kind = MapTileKind::Empty;
        ok &= require(output.cells[8 * draft.width + x].kind == MapTileKind::Empty,
                      "Withheld campaign places must lose their raster settlement projections before crop/tile export");
    }
    MapPngExportOptions options;
    options.playerSafe = true;
    options.pixelsPerCell = 16;
    options.locale = "en";
    std::string error;
    const auto exportFive = [&](const MapModel &source, const std::string &name,
                                std::vector<std::vector<char>> &bytes) {
        const auto destination = root / name;
        std::filesystem::create_directories(destination);
        const auto reviewed = compileMapRenderPlan(source, request);
        const auto whole = destination / "whole.png";
        const auto fullPlan = destination / "reviewed.png";
        const auto cropped = destination / "cropped.png";
        MapPngTiledExportPlan tiles;
        MapPngTiledExportPlan optionTiles;
        const bool written = exportMapPng(source, whole, options, &error) &&
            exportMapPng(source, fullPlan, reviewed, 16, &error) &&
            exportMapPngRegion(source, cropped, reviewed, {2, 3, 28, 10}, 16, &error) &&
            exportMapPngTiled(source, destination / "tiles", "map", reviewed, 16, false, &tiles, &error) &&
            exportMapPngTiled(source, destination / "option_tiles", "map", options, false, &optionTiles, &error);
        if (!written)
            return false;
        bytes = {readBytes(whole), readBytes(fullPlan), readBytes(cropped)};
        for (const auto &tile : tiles.tiles)
            bytes.push_back(readBytes(destination / "tiles" / tile.fileName));
        for (const auto &tile : optionTiles.tiles)
            bytes.push_back(readBytes(destination / "option_tiles" / tile.fileName));
        return bytes.size() >= 5 && std::all_of(bytes.begin(), bytes.end(), [](const auto &image) { return !image.empty(); });
    };
    std::vector<std::vector<char>> actualBytes, expectedBytes;
    const bool exported = exportFive(output, "actual", actualBytes) && exportFive(expected, "expected", expectedBytes);
    ok &= require(exported && actualBytes == expectedBytes,
                  "All five player PNG APIs must match a source with only the campaign-public control: " + error);
    MapModel empty = expected;
    empty.generation.graph.features.clear();
    empty.cells[8 * draft.width + 5].kind = MapTileKind::Empty;
    const auto emptyPath = root / "empty.png";
    const auto rawPath = root / "unfiltered_positive_control.png";
    const bool controls = exportMapPng(empty, emptyPath, options, &error) && exportMapPng(draft, rawPath, options, &error);
    ok &= require(controls && !actualBytes.empty() && actualBytes.front() != readBytes(emptyPath) &&
                      actualBytes.front() != readBytes(rawPath),
                  "PNG byte checks need both a visibly retained public control and a distinguishable unfiltered source");

    // Same map revision and draft serial: deletion/revocation/ambiguous links
    // must take effect at the next output boundary without saving the draft.
    campaign.worldPlaces.front().visibility = WorldObjectVisibility::GmOnly;
    const auto revoked = makeMapCreatorCampaignPlayerOutputSnapshot(draft, baseline, campaign);
    ok &= require(revoked.generation.graph.features.empty(), "A later world-place revocation must invalidate player output immediately");
    campaign.worldPlaces.erase(campaign.worldPlaces.begin());
    const auto deleted = makeMapCreatorCampaignPlayerOutputSnapshot(draft, baseline, campaign);
    ok &= require(deleted.generation.graph.features.empty(), "Deleting the public world link must fail closed without a map revision bump");
    ok &= require(draft.generation.graph.features.size() == 5 && campaign.maps.front().generation.graph.features.size() == 5 &&
                      draft.cells[8 * draft.width + 11].kind == MapTileKind::Settlement && baseline.release.labels,
                  "Player projection must preserve authored geometry, canonical map and sharing baseline");

    MapModel legacy = expected;
    legacy.playerRelease.labels = false;
    legacy.playerRelease.politicalBorders = true;
    MapGeneratedFeature territory;
    territory.id = "legacy_territory";
    territory.name = "Public Realm";
    territory.kind = MapGeneratedFeatureKind::TerrainRegion;
    territory.materialId = "territory.legacy";
    territory.closed = true;
    territory.points = {{1, 1}, {20, 1}, {20, 14}, {1, 14}};
    territory.discovered = true;
    territory.gmOnly = false;
    legacy.generation.graph.features.push_back(territory);
    Campaign legacyCampaign;
    legacyCampaign.maps.push_back(legacy);
    WorldPlaceModel legacyPlace;
    legacyPlace.linkedMapId = legacy.metadata.id;
    legacyPlace.name = legacy.generation.graph.features.front().name;
    legacyPlace.visibility = WorldObjectVisibility::Public;
    legacyCampaign.worldPlaces.push_back(legacyPlace);
    NationModel legacyNation;
    legacyNation.name = territory.name;
    legacyNation.visibility = WorldObjectVisibility::Public;
    legacyCampaign.nations.push_back(legacyNation);
    const auto legacyOutput = makeMapCreatorCampaignPlayerOutputSnapshot(
        legacy, captureMapCreatorPlayerSettings(legacy), legacyCampaign);
    MapRenderPlanRequest legacyRequest = request;
    legacyRequest.showPoliticalOverlay = true;
    legacyRequest.additionalGeneratedFeatureVisibility = [&](const MapGeneratedFeature &feature) {
        return mapCreatorPlayerOutputFeatureIsPublic(legacyCampaign, legacy, feature);
    };
    const auto legacyPreview = compileMapRenderPlan(legacyOutput, legacyRequest);
    ok &= require(legacyOutput.generation.graph.features.size() == 2 &&
                      legacyOutput.generation.graph.features.front().name.empty() &&
                      legacyPreview.generatedFeatureIds.size() == 2 &&
                      mapRenderPlanContainsGeneratedGeometry(legacyPreview, "public") &&
                      mapRenderPlanContainsGeneratedGeometry(legacyPreview, territory.id) &&
                      std::none_of(legacyPreview.primitives.begin(), legacyPreview.primitives.end(), [](const auto &primitive) {
                          return primitive.kind == MapRenderPrimitiveKind::Label;
                      }),
                  "Labels-off preview must preserve public legacy name-linked place symbols and nation geometry after sanitization");
    std::filesystem::remove_all(root, ignored);
    return ok;
}

bool testPlayerSafePngExport()
{
    bool ok = true;
    const std::filesystem::path root =
        ApplicationPaths::writableRoot() / "player_safety_png_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    MapModel legacy;
    legacy.visibility = MapVisibility::PlayersVisible;
    legacy.width = 3;
    legacy.height = 1;
    legacy.cells.resize(3);
    legacy.cells[0].kind = MapTileKind::Lava;
    legacy.cells[0].fogged = true;
    legacy.cells[1].kind = MapTileKind::Forest;
    legacy.cells[2].kind = MapTileKind::Water;

    MapPngExportOptions playerOptions;
    playerOptions.pixelsPerCell = 16;
    playerOptions.playerSafe = true;
    playerOptions.applyFog = false; // Player-safe mode must override this unsafe request.
    const std::filesystem::path safeImplicit = root / "legacy_safe_implicit.png";
    const std::filesystem::path safeExplicit = root / "legacy_safe_explicit.png";
    const std::filesystem::path gmNoFog = root / "legacy_gm_no_fog.png";
    std::string error;
    ok &= require(exportMapPng(legacy, safeImplicit, playerOptions, &error), "Implicit player fog export failed: " + error);
    MapPngExportOptions explicitFog = playerOptions;
    explicitFog.applyFog = true;
    error.clear();
    ok &= require(exportMapPng(legacy, safeExplicit, explicitFog, &error), "Explicit player fog export failed: " + error);
    MapPngExportOptions gmOptions = playerOptions;
    gmOptions.playerSafe = false;
    gmOptions.applyFog = false;
    error.clear();
    ok &= require(exportMapPng(legacy, gmNoFog, gmOptions, &error), "GM no-fog export failed: " + error);
    const std::filesystem::path unicodeDestination =
        root / std::filesystem::u8path(u8"Karte_München_日本.png");
    error.clear();
    ok &= require(exportMapPng(legacy, unicodeDestination, gmOptions, &error) &&
                      std::filesystem::exists(unicodeDestination) &&
                      std::filesystem::file_size(unicodeDestination) > 100,
                  "PNG writer did not preserve its UTF-8 destination path: " + error);

    ok &= require(readBytes(safeImplicit) == readBytes(safeExplicit),
                  "Player-safe PNG must force identical fog output even when applyFog=false");
    LoadedRgbaImage safeImage;
    LoadedRgbaImage gmImage;
    ok &= require(loadRgbaImage(safeImplicit, safeImage), "Could not decode player-safe PNG");
    ok &= require(loadRgbaImage(gmNoFog, gmImage), "Could not decode GM PNG");
    if (!safeImage.pixels.empty())
    {
        ok &= require(fogPixelMatches(safeImage, 8, 8),
                      "Fogged legacy cell was not masked in player-safe PNG");
        ok &= require(!fogPixelMatches(safeImage, 24, 8),
                      "Revealed legacy cell was incorrectly fogged");
    }
    if (!gmImage.pixels.empty())
        ok &= require(!fogPixelMatches(gmImage, 8, 8), "GM no-fog export was unexpectedly masked");

    MapModel atlasStroke;
    atlasStroke.width = 12;
    atlasStroke.height = 8;
    atlasStroke.atlasLevel = MapLevel::World;
    atlasStroke.cells.resize(96);
    atlasStroke.generation.enabled = true;
    atlasStroke.generation.continuousRendering = true;
    atlasStroke.generation.recipe.styleVersion = 0;
    MapGeneratedFeature atlasRiver;
    atlasRiver.id = "thin_atlas_river";
    atlasRiver.kind = MapGeneratedFeatureKind::River;
    atlasRiver.width = 1.0f;
    atlasRiver.locked = true;
    atlasRiver.points = {{2.0f, 4.0f}, {10.0f, 4.0f}};
    atlasStroke.generation.graph.features.push_back(atlasRiver);
    MapPngExportOptions atlasOptions = gmOptions;
    atlasOptions.showLabels = false;
    const auto atlasPath = root / "atlas_stroke_compositing.png";
    ok &= require(exportMapPng(atlasStroke, atlasPath, atlasOptions, &error),
                  "Atlas stroke export failed: " + error);
    LoadedRgbaImage atlasImage;
    ok &= require(loadRgbaImage(atlasPath, atlasImage), "Could not decode atlas stroke export");
    bool opaqueAtlas = !atlasImage.pixels.empty();
    for (std::size_t offset = 3; offset < atlasImage.pixels.size(); offset += 4)
        opaqueAtlas &= atlasImage.pixels[offset] == 255;
    ok &= require(opaqueAtlas,
                  "A translucent one-pixel river glint must blend over opaque terrain, not punch transparency into the PNG");
    if (atlasImage.pixels.size() >= static_cast<std::size_t>(atlasImage.width * atlasImage.height * 4))
    {
        const std::size_t riverPixel = static_cast<std::size_t>((64 * atlasImage.width + 96) * 4);
        ok &= require(riverPixel + 3 < atlasImage.pixels.size() &&
                          atlasImage.pixels[riverPixel + 2] > atlasImage.pixels[riverPixel],
                      "The compositing regression must exercise a visible blue river stroke");
    }

    MapModel generated;
    generated.visibility = MapVisibility::PlayersVisible;
    generated.width = 8;
    generated.height = 4;
    generated.cells.resize(32);
    generated.generation.enabled = true;
    generated.generation.continuousRendering = true;
    generated.generation.recipe.styleVersion = 0;
    generated.playerRelease.waysAndRoutes = true;
    MapGeneratedFeature secretRoad;
    secretRoad.id = "secret_road";
    secretRoad.kind = MapGeneratedFeatureKind::Road;
    secretRoad.gmOnly = true;
    secretRoad.discovered = false;
    secretRoad.points = {{0.5f, 1.5f}, {7.5f, 1.5f}};
    generated.generation.graph.features.push_back(secretRoad);
    MapModel generatedBaseline = generated;
    generatedBaseline.generation.graph.features.clear();
    const std::filesystem::path generatedSafe = root / "generated_safe.png";
    const std::filesystem::path generatedBaselineSafe = root / "generated_baseline_safe.png";
    const std::filesystem::path generatedGm = root / "generated_gm.png";
    error.clear();
    ok &= require(exportMapPng(generated, generatedSafe, playerOptions, &error), "Generated player-safe export failed: " + error);
    error.clear();
    ok &= require(exportMapPng(generatedBaseline, generatedBaselineSafe, playerOptions, &error), "Generated baseline export failed: " + error);
    error.clear();
    ok &= require(exportMapPng(generated, generatedGm, gmOptions, &error), "Generated GM export failed: " + error);
    ok &= require(readBytes(generatedSafe) == readBytes(generatedBaselineSafe),
                  "GM-only or undiscovered generated feature leaked into player-safe PNG");
    ok &= require(readBytes(generatedGm) != readBytes(generatedBaselineSafe),
                  "GM export did not contain the private generated feature used by the safety test");

    MapModel foggedSymbol;
    foggedSymbol.visibility = MapVisibility::PlayersVisible;
    foggedSymbol.width = 1;
    foggedSymbol.height = 1;
    foggedSymbol.cells.resize(1);
    foggedSymbol.cells.front().kind = MapTileKind::Landmark;
    foggedSymbol.cells.front().fogged = true;
    foggedSymbol.generation.enabled = true;
    foggedSymbol.generation.continuousRendering = true;
    foggedSymbol.generation.recipe.styleVersion = 0;
    foggedSymbol.playerRelease.placesAndMarkers = true;
    MapGeneratedFeature privateLandmark;
    privateLandmark.id = "private_landmark";
    privateLandmark.kind = MapGeneratedFeatureKind::Landmark;
    privateLandmark.gmOnly = true;
    privateLandmark.points = {{0.5f, 0.5f}};
    foggedSymbol.generation.graph.features.push_back(privateLandmark);
    const std::filesystem::path foggedSymbolPath = root / "fogged_symbol_safe.png";
    error.clear();
    ok &= require(exportMapPng(foggedSymbol, foggedSymbolPath, playerOptions, &error), "Fogged symbol export failed: " + error);
    LoadedRgbaImage foggedSymbolImage;
    ok &= require(loadRgbaImage(foggedSymbolPath, foggedSymbolImage), "Could not decode fogged symbol export");
    if (!foggedSymbolImage.pixels.empty())
        ok &= require(fogPixelMatches(foggedSymbolImage, 8, 8),
                      "Hidden generated symbol bypassed the final player-safe fog mask");

    MapModel privateProjectedSymbol;
    privateProjectedSymbol.visibility = MapVisibility::PlayersVisible;
    privateProjectedSymbol.metadata.id = "private_projected_symbol";
    privateProjectedSymbol.width = 2;
    privateProjectedSymbol.height = 1;
    privateProjectedSymbol.cells.resize(2);
    privateProjectedSymbol.cells.front().kind = MapTileKind::Landmark;
    privateProjectedSymbol.generation.enabled = true;
    privateProjectedSymbol.generation.continuousRendering = true;
    privateProjectedSymbol.generation.recipe.styleVersion = 0;
    privateProjectedSymbol.playerRelease.placesAndMarkers = true;
    privateProjectedSymbol.playerRelease.labels = true;
    MapGeneratedFeature projectedSecret;
    projectedSecret.id = "projected_secret";
    projectedSecret.kind = MapGeneratedFeatureKind::Landmark;
    projectedSecret.name = "PROJECTED SECRET";
    projectedSecret.gmOnly = true;
    projectedSecret.points = {{0.5f, 0.5f}};
    privateProjectedSymbol.generation.graph.features.push_back(projectedSecret);
    MapModel projectedBaseline = privateProjectedSymbol;
    projectedBaseline.metadata.id = privateProjectedSymbol.metadata.id;
    projectedBaseline.cells.front().kind = MapTileKind::Empty;
    projectedBaseline.generation.graph.features.clear();
    const MapRenderPlan privateProjectedPlan = [&]() {
        MapRenderPlanRequest request;
        request.audience = MapRenderAudience::Player;
        request.layers = MapRenderPlanTerrain | MapRenderPlanSymbols |
                         MapRenderPlanLabels | MapRenderPlanFog;
        request.applyFog = true;
        return compileMapRenderPlan(privateProjectedSymbol, request);
    }();
    const MapRenderPlan projectedBaselinePlan = [&]() {
        MapRenderPlanRequest request;
        request.audience = MapRenderAudience::Player;
        request.layers = MapRenderPlanTerrain | MapRenderPlanSymbols |
                         MapRenderPlanLabels | MapRenderPlanFog;
        request.applyFog = true;
        return compileMapRenderPlan(projectedBaseline, request);
    }();
    const std::filesystem::path projectedSecretPath = root / "projected_secret_safe.png";
    const std::filesystem::path projectedBaselinePath = root / "projected_baseline_safe.png";
    error.clear();
    ok &= require(exportMapPng(privateProjectedSymbol, projectedSecretPath,
                               privateProjectedPlan, 16, &error),
                  "Reviewed private projection export failed: " + error);
    error.clear();
    ok &= require(exportMapPng(projectedBaseline, projectedBaselinePath,
                               projectedBaselinePlan, 16, &error),
                  "Reviewed projection baseline export failed: " + error);
    ok &= require(readBytes(projectedSecretPath) == readBytes(projectedBaselinePath),
                  "A private generated point feature reappeared from its synchronized raster cell in reviewed PNG output");

    const std::filesystem::path originalReference = root / "reference_original.png";
    const std::filesystem::path workingProxy = root / "reference_proxy.png";
    std::string imageError;
    ok &= require(saveRgbaImageAsPng(originalReference, solidImage(201, 31, 29), &imageError),
                  "Could not write imported-reference original fixture: " + imageError);
    imageError.clear();
    ok &= require(saveRgbaImageAsPng(workingProxy, solidImage(23, 67, 211), &imageError),
                  "Could not write imported-reference proxy fixture: " + imageError);

    MapModel imported;
    imported.metadata.id = "imported_reference_export";
    imported.revision = 4;
    imported.visibility = MapVisibility::PlayersVisible;
    imported.width = 1;
    imported.height = 1;
    imported.cells.resize(1);
    imported.importedReference.enabled = true;
    imported.importedReference.gmOnly = false;
    imported.importedReference.assetId = "reference_original_asset";
    imported.importedReference.projectRelativePath = std::filesystem::relative(
        originalReference, ApplicationPaths::writableRoot()).generic_string();
    imported.importedReference.contentHash = contentHash(originalReference);
    imported.importedReference.workingProxyGenerated = true;
    imported.importedReference.workingProxyAssetId = "reference_proxy_asset";
    imported.importedReference.workingProxyProjectRelativePath = std::filesystem::relative(
        workingProxy, ApplicationPaths::writableRoot()).generic_string();
    imported.importedReference.workingProxyContentHash = contentHash(workingProxy);
    imported.backgroundImagePath = imported.importedReference.workingProxyProjectRelativePath;

    MapRenderPlanRequest importedGmRequest;
    importedGmRequest.audience = MapRenderAudience::GameMaster;
    importedGmRequest.layers = MapRenderPlanBackground;
    importedGmRequest.applyFog = false;
    const MapRenderPlan importedGmPlan = compileMapRenderPlan(imported, importedGmRequest);
    MapRenderPlanRequest importedPlayerRequest = importedGmRequest;
    importedPlayerRequest.audience = MapRenderAudience::Player;
    importedPlayerRequest.applyFog = true;
    const MapRenderPlan importedPlayerPlan = compileMapRenderPlan(imported, importedPlayerRequest);

    const std::filesystem::path importedGmExport = root / "reference_gm_export.png";
    const std::filesystem::path importedPlayerExport = root / "reference_player_export.png";
    error.clear();
    ok &= require(exportMapPng(imported, importedGmExport, importedGmPlan, 16, &error),
                  "GM render-plan export of imported reference failed: " + error);
    error.clear();
    ok &= require(exportMapPng(imported, importedPlayerExport, importedPlayerPlan, 16, &error),
                  "Player render-plan export of imported reference failed: " + error);
    LoadedRgbaImage importedGmImage;
    LoadedRgbaImage importedPlayerImage;
    ok &= require(loadRgbaImage(importedGmExport, importedGmImage), "Could not decode GM imported-reference export");
    ok &= require(loadRgbaImage(importedPlayerExport, importedPlayerImage), "Could not decode player imported-reference export");
    if (!importedGmImage.pixels.empty())
        ok &= require(pixelMatches(importedGmImage, 8, 8, 201, 31, 29) &&
                          !pixelMatches(importedGmImage, 8, 8, 23, 67, 211),
                      "GM render-plan export upscaled the working proxy instead of sampling the original reference");
    if (!importedPlayerImage.pixels.empty())
        ok &= require(pixelMatches(importedPlayerImage, 8, 8, 201, 31, 29) &&
                          !pixelMatches(importedPlayerImage, 8, 8, 23, 67, 211),
                      "Player render-plan export upscaled the working proxy instead of sampling the original reference");

    imageError.clear();
    ok &= require(saveRgbaImageAsPng(originalReference, solidImage(31, 201, 29), &imageError),
                  "Could not mutate imported-reference fixture for stale-content test: " + imageError);
    const std::filesystem::path changedReferenceExport = root / "changed_reference_export.png";
    error.clear();
    ok &= require(!exportMapPng(imported, changedReferenceExport, importedGmPlan, 16, &error) &&
                      error == "The original map image no longer matches the imported file." &&
                      !std::filesystem::exists(changedReferenceExport),
                  "A plan-bound original whose bytes changed after review must fail closed");

    const std::size_t preservedCellCount = imported.cells.size();
    std::filesystem::remove(originalReference, ignored);
    const std::filesystem::path missingReferenceExport = root / "missing_reference_export.png";
    error.clear();
    ok &= require(!exportMapPng(imported, missingReferenceExport, importedGmPlan, 16, &error) &&
                      error == "The managed map image is missing or cannot be read. Repair the image reference before exporting." &&
                      !std::filesystem::exists(missingReferenceExport) &&
                      imported.cells.size() == preservedCellCount && imported.importedReference.enabled,
                  "A missing authoritative image must block export without writing a fallback or mutating map semantics");

    std::filesystem::remove_all(root, ignored);
    return ok;
}
}

int main()
{
    bool ok = true;
    std::cout << "player safety: projections\n";
    ok &= testPlayerSafeProjection();
    ok &= testSelectedContextPreview();
    std::cout << "player safety: reviewed snapshot channels\n";
    ok &= testPlayerDisplayReviewSnapshotChannels();
    std::cout << "player safety: live combat refresh fingerprint\n";
    ok &= testLiveCombatStatsFingerprintPolicy();
    ok &= testPublicSpellSlotOptInAndCombatEnd();
    std::cout << "player safety: public journal cache invalidation\n";
    ok &= testPublicJournalCacheInvalidation();
    std::cout << "player safety: publication contract\n";
    ok &= testPlayerDisplayPublicationContract();
    std::cout << "player safety: global blackout shortcut\n";
    ok &= testGlobalBlackoutShortcutPolicy();
    std::cout << "player safety: map-viewer save rollback\n";
    ok &= testMapViewerCampaignSaveFailureRollback();
    std::cout << "player safety: editor canvas camera capture\n";
    ok &= testPlayerDisplayCanvasCameraCapture();
    std::cout << "player safety: display window placement\n";
    ok &= testPlayerDisplayWindowPlacementPolicy();
    std::cout << "player safety: whole-map fog contract\n";
    ok &= testPlayerDisplayWholeMapFogContract();
    std::cout << "player safety: public camera presets\n";
    ok &= testPlayerDisplayCameraPresets();
    std::cout << "player safety: immutable map review plans\n";
    ok &= testImmutablePlayerMapReviewPlans();
    std::cout << "player safety: persistent map release categories\n";
    ok &= testPersistentPlayerMapReleaseCategories();
    std::cout << "player safety: reviewed publication\n";
    ok &= testReviewedPlayerDisplayPublication();
    std::cout << "player safety: transactional reviewed publication\n";
    ok &= testReviewedPublicationTransactions();
    std::cout << "player safety: reviewed handout content binding\n";
    ok &= testReviewedHandoutContentBinding();
    std::cout << "player safety: ping contract\n";
    ok &= testPlayerDisplayPingContract();
    std::cout << "player safety: transactional live fog stroke\n";
    ok &= testMapViewerFogStrokeTransaction();
    std::cout << "player safety: PNG export\n";
    ok &= testPrivateMapPngExportRejection();
    ok &= testCampaignConsentAcrossPlayerPngExports();
    ok &= testPlayerSafePngExport();
    return ok ? 0 : 1;
}
