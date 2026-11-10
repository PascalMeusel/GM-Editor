#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorPreviewContract.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapAuthoringWorkspaceContract.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorArtStylePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPlayerOutputPolicy.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapGeneratedNamePresentation.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapRenderPlanCache.hpp"
#include "Models/Map/MapRoutePresentation.hpp"
#include "Models/Map/MapWorldBrief.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
bool require(bool condition, const char *message)
{
    if (condition)
        return true;
    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool samePoint(const MapFeaturePoint &left, const MapFeaturePoint &right)
{
    return std::abs(left.x - right.x) < 0.0001f &&
           std::abs(left.y - right.y) < 0.0001f;
}

bool samePrimitive(const MapRenderPrimitive &left,
                   const MapRenderPrimitive &right)
{
    return left.kind == right.kind && left.layer == right.layer &&
           left.sourceId == right.sourceId &&
           left.assetRole == right.assetRole &&
           left.assetId == right.assetId && left.label == right.label &&
           left.closed == right.closed &&
           left.points.size() == right.points.size() &&
           std::equal(left.points.begin(), left.points.end(),
                      right.points.begin(), samePoint);
}

bool samePrimitiveSequence(const std::vector<MapRenderPrimitive> &left,
                           const std::vector<MapRenderPrimitive> &right)
{
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(), samePrimitive);
}

const MapRenderPrimitive *findPrimitive(
    const MapRenderPlan &plan,
    MapRenderPrimitiveKind kind,
    const EntityId &sourceId)
{
    const auto found = std::find_if(
        plan.primitives.begin(), plan.primitives.end(),
        [&](const MapRenderPrimitive &primitive) {
            return primitive.kind == kind && primitive.sourceId == sourceId;
        });
    return found == plan.primitives.end() ? nullptr : &*found;
}

bool hasCanonicalLayerOrder(const MapRenderPlan &plan)
{
    std::uint32_t previousLayer = 0;
    for (const MapRenderPrimitive &primitive : plan.primitives)
    {
        if (primitive.layer < previousLayer)
            return false;
        previousLayer = primitive.layer;
    }
    return true;
}

bool worldPlanContainsForbiddenScaleRole(const MapRenderPlan &plan)
{
    for (const MapRenderPrimitive &primitive : plan.primitives)
    {
        const std::string &role = primitive.assetRole;
        if (role == "vegetation_stamp" || role == "settlement_stamp" ||
            role == "district_overlay" || role == "tactical_prop" ||
            role.find("building") != std::string::npos ||
            role.find("detail") != std::string::npos ||
            role.find("dungeon") != std::string::npos ||
            role.find("cave") != std::string::npos)
            return true;
    }
    return false;
}

bool testRegeneratedDesignSharingAndHistory()
{
    bool ok = true;
    const auto fields = {&MapPlayerReleaseProfile::politicalBorders,
                         &MapPlayerReleaseProfile::waysAndRoutes,
                         &MapPlayerReleaseProfile::placesAndMarkers,
                         &MapPlayerReleaseProfile::labels,
                         &MapPlayerReleaseProfile::tokensAndAnnotations};
    const auto sameRelease = [&](const auto &left, const auto &right) {
        return std::all_of(fields.begin(), fields.end(),
                           [&](auto field) { return left.*field == right.*field; });
    };
    MapModel original;
    original.metadata.id = "regeneration-consent-map";
    original.atlasLevel = MapLevel::Region;
    original.visibility = MapVisibility::PlayersVisible;
    original.width = 6;
    original.height = 4;
    resizeMapCells(original);
    for (auto &cell : original.cells) cell.fogged = false;
    MapGeneratedFeature place;
    place.id = "regeneration-public-place";
    place.kind = MapGeneratedFeatureKind::Settlement;
    place.name = "Havenfall";
    place.points = {{1.5f, 1.5f}};
    original.generation.graph.features.push_back(place);
    place.id = "regeneration-private-place";
    place.name = "PRIVATE_REGENERATION_SENTINEL";
    place.gmOnly = true;
    place.points = {{4.5f, 1.5f}};
    original.generation.graph.features.push_back(place);
    const auto request = gm::mapui::mapCreatorPlayerPreviewRequest("en");

    // Generation starts with private category defaults. Accepting its geometry
    // must carry all combinations of the existing draft's sharing decisions.
    for (unsigned mask = 0; mask < 32; ++mask)
    {
        unsigned bit = 0;
        for (auto field : fields) original.playerRelease.*field = (mask & (1u << bit++)) != 0;
        const auto baseline = captureMapCreatorPlayerSettings(original);
        MapModel accepted = original;
        accepted.playerRelease = {};
        accepted.visibility = MapVisibility::GameMasterOnly;
        accepted.cells.front().kind = MapTileKind::Forest;
        accepted.name = "Accepted new geography";
        ok &= require(applyMapCreatorPlayerSettings(accepted, captureMapCreatorPlayerSettings(original)),
                      "Regeneration must carry sharing only after retaining the original map identity");
        const auto output = makeMapCreatorPlayerOutputSnapshot(accepted, baseline, {original});
        const auto plan = compileMapRenderPlan(output, request);
        ok &= require(sameRelease(accepted.playerRelease, original.playerRelease) &&
                          sameRelease(output.playerRelease, original.playerRelease) &&
                          output.visibility == original.visibility &&
                          output.cells.front().kind == MapTileKind::Forest,
                      "Accepting a regenerated design must preserve all five category choices and the new geography");
        ok &= require((findPrimitive(plan, MapRenderPrimitiveKind::GeneratedSymbol, "regeneration-public-place") != nullptr) ==
                          original.playerRelease.placesAndMarkers &&
                          (findPrimitive(plan, MapRenderPrimitiveKind::Label, "regeneration-public-place") != nullptr) ==
                          original.playerRelease.labels &&
                          !findPrimitive(plan, MapRenderPrimitiveKind::GeneratedSymbol, "regeneration-private-place") &&
                          !findPrimitive(plan, MapRenderPrimitiveKind::Label, "regeneration-private-place"),
                      "Regeneration consent must expose only opted-in public places and never private sentinels");

        MapModel externallyChanged = original;
        for (auto field : fields) externallyChanged.playerRelease.*field = !(original.playerRelease.*field);
        const auto changedOutput = makeMapCreatorPlayerOutputSnapshot(accepted, baseline, {externallyChanged});
        const auto merged = mergeMapCreatorPlayerSettings(baseline, accepted, externallyChanged);
        ok &= require(merged.valid && sameRelease(changedOutput.playerRelease, externallyChanged.playerRelease) &&
                          sameRelease(merged.release, externallyChanged.playerRelease) &&
                          sameRelease(baseline.release, original.playerRelease),
                      "Unchanged regeneration consent must follow external grants and revocations without rebasing history early");
    }

    original.playerRelease = {true, true, true, true, true};
    {
        MapModel worldDraft = original;
        worldDraft.atlasLevel = MapLevel::World;
        auto newPlace = worldDraft.generation.graph.features.front();
        newPlace.id = "new-unlinked-regenerated-place";
        newPlace.name = "NEW_WORLD_SECRET";
        newPlace.points = {{2.5f, 2.5f}};
        worldDraft.generation.graph.features.push_back(newPlace);
        Campaign worldCampaign;
        worldCampaign.maps.push_back(worldDraft);
        WorldPlaceModel linked;
        linked.metadata.id = "revealed-campaign-place";
        linked.name = "Havenfall";
        linked.linkedMapId = worldDraft.metadata.id;
        linked.linkedFeatureId = "regeneration-public-place";
        linked.visibility = WorldObjectVisibility::Discovered;
        worldCampaign.worldPlaces.push_back(linked);
        const auto worldBaseline = captureMapCreatorPlayerSettings(worldDraft);
        auto worldOutput = makeMapCreatorCampaignPlayerOutputSnapshot(worldDraft, worldBaseline, worldCampaign);
        auto worldPlan = compileMapRenderPlan(worldOutput, request);
        ok &= require(findPrimitive(worldPlan, MapRenderPrimitiveKind::GeneratedSymbol, linked.linkedFeatureId) &&
                          !findPrimitive(worldPlan, MapRenderPrimitiveKind::GeneratedSymbol, newPlace.id) &&
                          !findPrimitive(worldPlan, MapRenderPrimitiveKind::Label, newPlace.id),
                      "Preserved categories must not reveal a newly generated world place without its own campaign link");
        worldCampaign.worldPlaces.front().visibility = WorldObjectVisibility::GmOnly;
        worldOutput = makeMapCreatorCampaignPlayerOutputSnapshot(worldDraft, worldBaseline, worldCampaign);
        ok &= require(worldOutput.generation.graph.features.empty(),
                      "A private campaign place must remain withheld despite every map category being enabled");
        worldCampaign.worldPlaces.front().visibility = WorldObjectVisibility::Discovered;
        worldDraft.generation.graph.features.front().discovered = false;
        worldOutput = makeMapCreatorCampaignPlayerOutputSnapshot(worldDraft, worldBaseline, worldCampaign);
        ok &= require(worldOutput.generation.graph.features.empty(),
                      "A revealed campaign link must not override an undiscovered generated feature");
        worldDraft.generation.graph.features.front().discovered = true;
        worldCampaign.worldPlaces.push_back(linked);
        worldCampaign.worldPlaces.back().metadata.id = "ambiguous-second-link";
        worldOutput = makeMapCreatorCampaignPlayerOutputSnapshot(worldDraft, worldBaseline, worldCampaign);
        ok &= require(worldOutput.generation.graph.features.empty(),
                      "Ambiguous campaign links must stay hidden rather than selecting an arbitrary public match");
    }
    auto baseline = captureMapCreatorPlayerSettings(original);
    MapModel accepted = original;
    accepted.name = "Accepted new geography";
    accepted.cells.front().kind = MapTileKind::Forest;
    Campaign campaign;
    campaign.maps.push_back(original);
    campaign.maps.front().playerRelease.labels = false;
    campaign.maps.front().playerRelease.waysAndRoutes = false;
    const auto merged = mergeMapCreatorPlayerSettings(baseline, accepted, campaign.maps.front());
    int editingIndex = 0;
    int selectedIndex = 0;
    int parentIndex = 0;
    {
        MapCreatorCampaignSaveTransaction transaction(campaign, accepted, editingIndex, selectedIndex);
        campaign.maps.front() = accepted;
        applyMapCreatorPlayerSettings(campaign.maps.front(), merged);
        accepted.name = "Transient save normalization";
        editingIndex = selectedIndex = 9;
        ok &= require(!transaction.finish(false), "A failed regenerated-map save must roll back staging");
    }
    ok &= require(!finalizeMapCreatorPlayerSettingsSave(false, merged, accepted, baseline) &&
                      accepted.name == "Accepted new geography" && editingIndex == 0 && selectedIndex == 0 &&
                      accepted.cells.front().kind == MapTileKind::Forest && baseline.release.labels &&
                      !campaign.maps.front().playerRelease.labels && !campaign.maps.front().playerRelease.waysAndRoutes,
                  "Failed save must retain regenerated work, original baseline and current canonical revocations");
    const MapModel redoMap = accepted;
    const auto redoBaseline = baseline;
    restoreMapCreatorDraftHistory(original, baseline, campaign.maps, accepted, baseline,
                                  editingIndex, selectedIndex, {}, parentIndex);
    auto output = makeMapCreatorPlayerOutputSnapshot(accepted, baseline, campaign.maps);
    ok &= require(!output.playerRelease.labels && !output.playerRelease.waysAndRoutes &&
                      output.playerRelease.placesAndMarkers && accepted.cells.front().kind == original.cells.front().kind,
                  "Undo after a failed regeneration save must restore geography without resurrecting revoked content");
    restoreMapCreatorDraftHistory(redoMap, redoBaseline, campaign.maps, accepted, baseline,
                                  editingIndex, selectedIndex, {}, parentIndex);
    output = makeMapCreatorPlayerOutputSnapshot(accepted, baseline, campaign.maps);
    ok &= require(accepted.cells.front().kind == MapTileKind::Forest && !output.playerRelease.labels &&
                      output.playerRelease.placesAndMarkers && baseline.release.labels,
                  "Redo after failed save must restore the regenerated draft with its original sharing baseline");
    {
        MapCreatorCampaignSaveTransaction transaction(campaign, accepted, editingIndex, selectedIndex);
        campaign.maps.front() = accepted;
        applyMapCreatorPlayerSettings(campaign.maps.front(), merged);
        ok &= require(transaction.finish(true), "A successful retry must commit regenerated geometry");
    }
    ok &= require(finalizeMapCreatorPlayerSettingsSave(true, merged, accepted, baseline) &&
                      !baseline.release.labels && !accepted.playerRelease.labels &&
                      campaign.maps.front().cells.front().kind == MapTileKind::Forest,
                  "Only successful retry may advance the regenerated draft sharing baseline");
    MapModel rebasedUndo = original;
    applyMapCreatorPlayerSettings(rebasedUndo, baseline);
    restoreMapCreatorDraftHistory(rebasedUndo, baseline, campaign.maps, accepted, baseline,
                                  editingIndex, selectedIndex, {}, parentIndex);
    output = makeMapCreatorPlayerOutputSnapshot(accepted, baseline, campaign.maps);
    ok &= require(!output.playerRelease.labels && !accepted.playerRelease.labels &&
                      accepted.cells.front().kind == original.cells.front().kind,
                  "Committed sharing rebased into Undo must survive a later geometry restoration");
    const auto missing = makeMapCreatorPlayerOutputSnapshot(accepted, baseline, {});
    ok &= require(missing.visibility == MapVisibility::GameMasterOnly && !missing.playerRelease.placesAndMarkers,
                  "A deleted or never-saved regenerated map must still fail closed for players");
    return ok;
}
}

int main()
{
    bool ok = testRegeneratedDesignSharingAndHistory();

    {
        MapModel draft;
        draft.metadata.id = "loaded-review-map";
        draft.revision = 12;
        std::vector<MapModel> campaignMaps{draft};
        MapCreatorPersistentSaveReceipt receipt;
        MapCreatorLoadedDraftBaseline loaded{draft.metadata.id, draft.revision, 7};
        const auto ready = [&](std::uint64_t serial, bool parentMatches = true, bool dirty = false) {
            return mapCreatorDraftReadyForPlayerReview(receipt, loaded, draft, serial,
                                                       parentMatches, dirty, campaignMaps);
        };
        ok &= require(ready(7) && !receipt.committed,
            "a clean loaded map can enter the persist-before-review callback without fabricating a commit");
        ok &= require(!ready(8) && !ready(7, false) && !ready(7, true, true),
            "authoring, parent changes and active/failed save work block loaded-map review");
        ++campaignMaps.front().revision;
        ok &= require(!ready(7), "a canonical revision change invalidates the loaded review source");
        campaignMaps.clear();
        ok &= require(!ready(7), "a removed map cannot be reviewed using its stale loaded baseline");
        campaignMaps.push_back(draft);
        loaded = {}; // A save attempt invalidates the loaded gate before any campaign staging.
        ok &= require(!ready(7), "a failed explicit save must not fall back to the clean-load gate");
        receipt = {true, draft.metadata.id, draft.revision, 7};
        ok &= require(ready(7), "successful save retry restores review readiness");
        ok &= require(!ready(8), "an edit after retry invalidates its durable receipt");

        LocalPlayerDisplayModel display;
        display.enabled = false;
        stageMapCreatorPlayerReviewSelection(display, draft.metadata.id);
        ok &= require(!display.enabled && display.blackout,
            "loaded-map readiness does not arm or publish the detached display");
    }

    {
        MapGenerationRecipe recipe;
        recipe.generatorVersion = 6;
        recipe.seed = 173;
        applyMapCreatorArtStyle(recipe, MapCreatorArtStyle::IllustratedClassic);
        const auto classic = recipe;
        ok &= require(mapCreatorArtStyleMatches(recipe, MapCreatorArtStyle::IllustratedClassic) &&
                          !mapCreatorArtStyleMatches(recipe, MapCreatorArtStyle::Illustrated),
            "saved classic atlas remains a distinct selectable choice from the new illustrated style");
        applyMapCreatorArtStyle(recipe, MapCreatorArtStyle::Illustrated);
        ok &= require(recipe.styleId == "illustrated_atlas" && recipe.styleVersion == 5 &&
                          recipe.styleSnapshotId == "illustrated_atlas_v5" &&
                          recipe.generatorVersion == classic.generatorVersion && recipe.seed == classic.seed,
            "deliberate artwork update chooses v5 while retaining the saved generation recipe");
        recipe = classic;
        ok &= require(mapCreatorArtStyleMatches(recipe, MapCreatorArtStyle::IllustratedClassic),
            "restoring the previous recipe restores classic artwork without an implicit upgrade");
    }

    {
        using namespace gm::mapui;
        ok &= require(mapCreatorShouldFitOnStageChange(2, mapAuthoringEdit) &&
                          !mapCreatorShouldFitOnStageChange(mapAuthoringPlayerPreview, mapAuthoringEdit) &&
                          !mapCreatorShouldFitOnStageChange(mapAuthoringExport, mapAuthoringEdit),
            "a new accepted design fits once; peer preview/export tab changes preserve the working camera");
        // Representative measured action widths for German at 150% text.
        // The former longest-label sizing forced two rows at this width.
        int rows = 1;
        float used = 0.0f;
        for (float action : {126.0f, 137.0f, 175.0f, 224.0f, 157.0f, 155.0f})
        {
            if (used > 0.0f && !mapCreatorActionFitsRow(used, action, 8.0f, 1050.0f))
            {
                ++rows;
                used = 0.0f;
            }
            if (used > 0.0f) used += 8.0f;
            used += action;
            ok &= require(used <= 1050.0f, "responsive toolbar actions remain inside the available canvas width");
        }
        ok &= require(rows == 1 && !mapCreatorActionFitsRow(200.0f, 160.0f, 8.0f, 340.0f),
            "actions use one row when their actual labels fit and wrap before narrow-width clipping");
    }

    {
        MapPngExportOptions options;
        options.locale = "en";
        options.showGrid = true;
        options.showPoliticalOverlay = true;
        const auto all = mapPngExportRenderPlanRequest(options);
        ok &= require(all.audience == MapRenderAudience::GameMaster && all.layers == MapRenderPlanAll &&
                          all.applyFog && all.showGrid && all.showPoliticalOverlay && all.locale == "en",
                      "GM export preview and PNG writer must share the complete option-to-request contract");
        const std::pair<bool MapPngExportOptions::*, std::uint32_t> toggles[] = {
            {&MapPngExportOptions::showGrid, MapRenderPlanGrid},
            {&MapPngExportOptions::showLabels, MapRenderPlanLabels},
            {&MapPngExportOptions::showPoliticalOverlay, MapRenderPlanPolitical},
            {&MapPngExportOptions::showAnnotations, MapRenderPlanAnnotations},
            {&MapPngExportOptions::showRoutes, MapRenderPlanRoutes},
            {&MapPngExportOptions::showTokens, MapRenderPlanTokens},
            {&MapPngExportOptions::showBackground, MapRenderPlanBackground},
            {&MapPngExportOptions::applyFog, MapRenderPlanFog}};
        for (const auto &[field, layer] : toggles)
        {
            auto changed = options;
            changed.*field = false;
            ok &= require(mapPngExportRenderPlanRequest(changed).layers == (all.layers & ~layer),
                          "Each export checkbox must change only its corresponding shared preview/output layer");
        }
        options.playerSafe = true;
        options.applyFog = false;
        const auto playerRequest = mapPngExportRenderPlanRequest(options);
        ok &= require(playerRequest.audience == MapRenderAudience::Player && playerRequest.applyFog &&
                          (playerRequest.layers & MapRenderPlanFog) != 0,
                      "Player output preview must force fog even when a caller attempts to disable it");

        MapModel draft;
        draft.metadata.id = "export_preview_contract";
        draft.width = 72;
        draft.height = 48;
        draft.visibility = MapVisibility::PlayersVisible;
        draft.playerRelease.tokensAndAnnotations = true;
        resizeMapCells(draft);
        for (auto &cell : draft.cells) cell.fogged = false;
        draft.cells.back().fogged = true;
        MapTokenModel token;
        token.metadata.id = "export_preview_token";
        token.x = 2; token.y = 2;
        draft.tokens.push_back(token);
        MapRenderPlanCache plans;
        gm::mapui::MapCreatorPlayerPreviewCache filteredSources;
        auto gmOptions = options;
        gmOptions.playerSafe = false;
        gmOptions.applyFog = true;
        auto gmPlan = plans.getOrCompile(draft, mapPngExportRenderPlanRequest(gmOptions), 7, 8.0f);
        ok &= require(filteredSources.filteredSource(draft, *gmPlan).tokens.size() == 1 &&
                          filteredSources.filteredSource(draft, *gmPlan).cells.back().fogged,
                      "Explicit GM export previews must retain selected annotations/tokens and requested fog");
        const auto gmHash = gmPlan->contentHash;
        gmOptions.showTokens = false;
        gmOptions.applyFog = false;
        gmPlan = plans.getOrCompile(draft, mapPngExportRenderPlanRequest(gmOptions), 7, 8.0f);
        ok &= require(gmPlan->contentHash != gmHash && filteredSources.filteredSource(draft, *gmPlan).tokens.empty() &&
                          !filteredSources.filteredSource(draft, *gmPlan).cells.back().fogged,
                      "Changing GM export options must refresh the preview cache without a draft revision or edit serial bump");

        Campaign campaign;
        campaign.maps.push_back(draft);
        const auto baseline = captureMapCreatorPlayerSettings(draft);
        auto output = makeMapCreatorCampaignPlayerOutputSnapshot(draft, baseline, campaign);
        auto liveRequest = playerRequest;
        liveRequest.additionalGeneratedFeatureVisibility = [&](const MapGeneratedFeature &feature) {
            return mapCreatorPlayerOutputFeatureIsPublic(campaign, draft, feature);
        };
        auto preview = plans.getOrCompile(output, liveRequest, 7, 8.0f);
        ok &= require(filteredSources.filteredSource(output, *preview).tokens.size() == 1,
                      "Player export preview requires an initially visible released positive control");
        campaign.maps.front().playerRelease.tokensAndAnnotations = false;
        output = makeMapCreatorCampaignPlayerOutputSnapshot(draft, baseline, campaign);
        preview = plans.getOrCompile(output, liveRequest, 7, 8.0f);
        ok &= require(filteredSources.filteredSource(output, *preview).tokens.empty(),
                      "External release revocation must immediately remove content from Export preview without saving draft edits");
        campaign.maps.front().visibility = MapVisibility::GameMasterOnly;
        output = makeMapCreatorCampaignPlayerOutputSnapshot(draft, baseline, campaign);
        ok &= require(!gm::mapui::mapCreatorPlayerPreviewCanShowMap(output) && draft.visibility == MapVisibility::PlayersVisible,
                      "Private-map Export preview must blank before rendering while preserving the authoring draft");

        const auto whole = gm::mapui::mapCreatorPreviewFrame(draft, {0, 0, 72, 48}, 720, 480, 0);
        const MapPngExportRect selected{12, 8, 24, 16};
        const auto cropped = gm::mapui::mapCreatorPreviewFrame(draft, selected, 720, 480, 0);
        ok &= require(whole.valid && cropped.valid && whole.cellSize == 10.0f && cropped.cellSize == 30.0f &&
                          cropped.mapX + selected.x * cropped.cellSize == cropped.clipX &&
                          cropped.mapY + selected.y * cropped.cellSize == cropped.clipY &&
                          cropped.clipWidth == 720.0f && cropped.clipHeight == 480.0f,
                      "Current-view preview must fit and clip the selected full-map cell rectangle without rewriting geometry");
        ok &= require(!gm::mapui::mapCreatorPreviewFrame(draft, {}, 720, 480, 0).valid &&
                          !gm::mapui::mapCreatorPreviewFrame(draft, {-1, 0, 24, 16}, 720, 480, 0).valid &&
                          !gm::mapui::mapCreatorPreviewFrame(draft, {60, 40, 24, 16}, 720, 480, 0).valid &&
                          draft.width == 72 && draft.height == 48 && draft.tokens.size() == 1,
                      "Unavailable or out-of-map export crops must remain invalid rather than silently previewing the whole map");
    }

    {
        using namespace gm::mapui;
        ok &= require(!mapCreatorIsAuthoring(0) && !mapCreatorIsAuthoring(2) &&
                          mapCreatorIsAuthoring(mapAuthoringEdit) &&
                          mapCreatorIsAuthoring(mapAuthoringExport) &&
                          mapCreatorIsAuthoring(mapAuthoringPlayerPreview),
                      "creation has three stages; edit, player preview and export remain peer workspaces");
        ok &= require(mapCreatorExperimentalIntent(MapCreationIntent::Settlement) &&
                          mapCreatorExperimentalIntent(MapCreationIntent::Dungeon) &&
                          mapCreatorExperimentalIntent(MapCreationIntent::Cave) &&
                          !mapCreatorExperimentalIntent(MapCreationIntent::WorldOverview) &&
                          !mapCreatorExperimentalIntent(MapCreationIntent::RegionExploration) &&
                          !mapCreatorExperimentalIntent(MapCreationIntent::TacticalEncounter),
                      "unfinished settlement and dungeon generators must be explicitly distinguished from core map creation");
        for (float available : {300.0f, 420.0f, 600.0f, 1200.0f})
            ok &= require(mapCreatorCompactInspectorHeight(available) <= available * 0.4f &&
                              mapCreatorCompactInspectorHeight(available) <= 280.0f,
                          "compact inspector must leave most of the short workspace for the canvas");
        ok &= require(!mapCreatorUsesStackedEditWorkspace(947.0f, 520.0f, 30.0f) &&
                          mapCreatorUsesStackedEditWorkspace(680.0f, 520.0f, 30.0f) &&
                          mapCreatorUsesStackedEditWorkspace(680.0f, 520.0f, 19.5f) &&
                          mapCreatorUsesStackedEditWorkspace(947.0f, 1000.0f, 30.0f) &&
                          !mapCreatorUsesStackedEditWorkspace(1500.0f, 1000.0f, 30.0f),
                      "short enlarged-text editing uses useful side-by-side panels; narrow docks and tall compact layouts remain stacked");

        MapModel playerPreviewMap;
        playerPreviewMap.metadata.id = "authoring_player_preview";
        playerPreviewMap.width = 8;
        playerPreviewMap.height = 6;
        resizeMapCells(playerPreviewMap);
        for (auto &cell : playerPreviewMap.cells) cell.fogged = false;
        playerPreviewMap.cells.back().fogged = true;
        ok &= require(!mapCreatorPlayerPreviewCanShowMap(playerPreviewMap),
                      "a private map must not be drawn even when terrain layers are requested");
        playerPreviewMap.visibility = MapVisibility::PlayersVisible;
        playerPreviewMap.playerRelease.tokensAndAnnotations = true;
        playerPreviewMap.playerRelease.placesAndMarkers = true;
        playerPreviewMap.playerRelease.labels = true;
        MapTokenModel token;
        token.metadata.id = "public_preview_token";
        token.name = "Visible scout";
        token.x = 1; token.y = 1;
        playerPreviewMap.tokens.push_back(token);
        MapGeneratedFeature secret;
        secret.id = "private_preview_place";
        secret.name = "Secret stronghold";
        secret.kind = MapGeneratedFeatureKind::Settlement;
        secret.points = {{3.5f, 3.5f}};
        secret.gmOnly = true;
        playerPreviewMap.generation.graph.features.push_back(secret);
        const auto request = mapCreatorPlayerPreviewRequest("en");
        ok &= require(request.audience == MapRenderAudience::Player && request.applyFog &&
                          !(request.layers & MapRenderPlanInteractiveHelpers),
                      "authoring player preview must use the public audience, mandatory fog and no editing helpers");
        MapCreatorPlayerPreviewCache cache;
        auto plan = compileMapRenderPlan(playerPreviewMap, request);
        const auto &filtered = cache.filteredSource(playerPreviewMap, plan);
        ok &= require(mapCreatorPlayerPreviewCanShowMap(playerPreviewMap) &&
                          filtered.tokens.size() == 1 && filtered.generation.graph.features.empty() &&
                          filtered.cells.back().fogged && playerPreviewMap.generation.graph.features.size() == 1,
                      "preview source strips private features and preserves final fog without mutating the authoring draft");
        playerPreviewMap.playerRelease.tokensAndAnnotations = false;
        plan = compileMapRenderPlan(playerPreviewMap, request);
        ok &= require(cache.filteredSource(playerPreviewMap, plan).tokens.empty(),
                      "revoking player category consent must invalidate the sanitized preview cache immediately");
        playerPreviewMap.visibility = MapVisibility::GameMasterOnly;
        ok &= require(!mapCreatorPlayerPreviewCanShowMap(playerPreviewMap),
                      "revoking map visibility must immediately blank the player preview");
    }

    {
        MapModel draft;
        draft.metadata.id = "current_player_sharing";
        draft.revision = 17;
        draft.atlasLevel = MapLevel::Region;
        draft.width = 8;
        draft.height = 6;
        resizeMapCells(draft);
        for (auto &cell : draft.cells) cell.fogged = false;
        draft.cells.front().kind = MapTileKind::Forest;
        draft.description = "Unsaved authoring notes";
        MapTokenModel token;
        token.metadata.id = "current_sharing_token";
        token.name = "Public scout";
        token.x = 1; token.y = 1;
        draft.tokens.push_back(token);
        MapGeneratedFeature place;
        place.id = "current_sharing_place";
        place.kind = MapGeneratedFeatureKind::Settlement;
        place.name = "Public village";
        place.points = {{4.5f, 3.5f}};
        draft.generation.graph.features.push_back(place);
        const auto baseline = captureMapCreatorPlayerSettings(draft);
        std::vector<MapModel> committed{draft};
        committed.front().cells.front().kind = MapTileKind::Water;
        const auto request = gm::mapui::mapCreatorPlayerPreviewRequest("en");
        MapRenderPlanCache plans;
        gm::mapui::MapCreatorPlayerPreviewCache sources;
        auto output = makeMapCreatorPlayerOutputSnapshot(draft, baseline, committed);
        ok &= require(!gm::mapui::mapCreatorPlayerPreviewCanShowMap(output),
                      "An unreleased current map must remain hidden in the authoring player preview");
        const auto privatePlan = plans.getOrCompile(output, request, 31, 8.0f);

        // Presentation decisions need not increment map revision or the
        // authoring serial. Inherited draft values must follow them immediately.
        committed.front().visibility = MapVisibility::PlayersVisible;
        committed.front().playerRelease = {true, true, true, true, true};
        output = makeMapCreatorPlayerOutputSnapshot(draft, baseline, committed);
        const auto releasedPlan = plans.getOrCompile(output, request, 31, 8.0f);
        ok &= require(gm::mapui::mapCreatorPlayerPreviewCanShowMap(output) &&
                          releasedPlan != privatePlan &&
                          findPrimitive(*releasedPlan, MapRenderPrimitiveKind::Token, token.metadata.id) &&
                          findPrimitive(*releasedPlan, MapRenderPrimitiveKind::GeneratedSymbol, place.id) &&
                          findPrimitive(*releasedPlan, MapRenderPrimitiveKind::Label, place.id) &&
                          sources.filteredSource(output, *releasedPlan).tokens.size() == 1,
                      "External grants must immediately expose permitted symbols/labels in player preview without another Save");
        ok &= require(output.cells.front().kind == MapTileKind::Forest &&
                          output.description == draft.description && output.revision == draft.revision &&
                          draft.visibility == MapVisibility::GameMasterOnly && !draft.playerRelease.labels &&
                          !baseline.release.labels && committed.front().cells.front().kind == MapTileKind::Water,
                      "Player output reconciliation must preserve authored content without mutating the draft, baseline or canonical geometry");

        committed.front().playerRelease = {};
        output = makeMapCreatorPlayerOutputSnapshot(draft, baseline, committed);
        const auto revokedPlan = plans.getOrCompile(output, request, 31, 8.0f);
        ok &= require(revokedPlan != releasedPlan &&
                          !findPrimitive(*revokedPlan, MapRenderPrimitiveKind::Token, token.metadata.id) &&
                          !findPrimitive(*revokedPlan, MapRenderPrimitiveKind::GeneratedSymbol, place.id) &&
                          !findPrimitive(*revokedPlan, MapRenderPrimitiveKind::Label, place.id) &&
                          sources.filteredSource(output, *revokedPlan).tokens.empty(),
                      "External category revocations must invalidate both plan and sanitized preview caches with unchanged revisions");
        committed.front().visibility = MapVisibility::GameMasterOnly;
        output = makeMapCreatorPlayerOutputSnapshot(draft, baseline, committed);
        ok &= require(!gm::mapui::mapCreatorPlayerPreviewCanShowMap(output),
                      "External whole-map revocation must immediately hide the draft player preview");

        MapModel publicOriginal = draft;
        publicOriginal.visibility = MapVisibility::PlayersVisible;
        publicOriginal.playerRelease = {true, true, true, true, true};
        const auto publicBaseline = captureMapCreatorPlayerSettings(publicOriginal);
        MapModel restrictedDraft = publicOriginal;
        restrictedDraft.visibility = MapVisibility::GameMasterOnly;
        restrictedDraft.playerRelease.labels = false;
        restrictedDraft.playerRelease.tokensAndAnnotations = false;
        const auto restrictedOutput = makeMapCreatorPlayerOutputSnapshot(
            restrictedDraft, publicBaseline, {publicOriginal});
        ok &= require(!gm::mapui::mapCreatorPlayerPreviewCanShowMap(restrictedOutput) &&
                          !restrictedOutput.playerRelease.labels && !restrictedOutput.playerRelease.tokensAndAnnotations &&
                          restrictedOutput.playerRelease.placesAndMarkers,
                      "Current canonical grants must never relax intentionally stricter unsaved draft visibility or categories");
        MapModel ungranted = publicOriginal;
        ungranted.playerRelease = {};
        const auto unsavedGrant = makeMapCreatorPlayerOutputSnapshot(
            publicOriginal, captureMapCreatorPlayerSettings(ungranted), {ungranted});
        ok &= require(!unsavedGrant.playerRelease.labels && !unsavedGrant.playerRelease.tokensAndAnnotations,
                      "Unsaved draft grants must not broaden current canonical player permissions");
        const auto unknownBaseline = makeMapCreatorPlayerOutputSnapshot(restrictedDraft, {}, {publicOriginal});
        const auto missingMap = makeMapCreatorPlayerOutputSnapshot(publicOriginal, publicBaseline, {});
        ok &= require(!gm::mapui::mapCreatorPlayerPreviewCanShowMap(unknownBaseline) &&
                          !unknownBaseline.playerRelease.labels &&
                          !gm::mapui::mapCreatorPlayerPreviewCanShowMap(missingMap) && !missingMap.playerRelease.labels,
                      "Unknown baseline restrictions and deleted/unsaved map identities must fail closed for player output");
    }

    {
        MapModel routeMap;
        routeMap.metadata.id = "route_presentation_map";
        routeMap.revision = 4;
        routeMap.atlasLevel = MapLevel::Region;
        routeMap.width = 9;
        routeMap.height = 7;
        resizeMapCells(routeMap);
        for (int y = 0; y < routeMap.height; ++y)
        {
            MapCellModel *water = mapCellAt(routeMap, 4, y);
            applyMapCellTerrainDefaults(*water, MapTileKind::Water);
            water->wayKind = MapWayKind::River;
        }
        const std::vector<std::pair<int, int>> roadCells = {
            {1, 1}, {2, 1}, {3, 1}, {3, 2}, {3, 3},
            {4, 3}, {5, 3}, {5, 4}, {5, 5}, {6, 5}, {7, 5}};
        for (const auto [x, y] : roadCells)
        {
            MapCellModel *cell = mapCellAt(routeMap, x, y);
            if (x == 4)
                applyMapCellTerrainDefaults(*cell, MapTileKind::Bridge);
            cell->wayKind = MapWayKind::DirtRoad;
        }

        MapGeneratedFeature roadNetwork;
        roadNetwork.id = "curved_road";
        roadNetwork.kind = MapGeneratedFeatureKind::Road;
        roadNetwork.points = {{1.5f, 1.5f}, {3.5f, 1.5f}, {3.5f, 3.5f},
                              {5.5f, 3.5f}, {5.5f, 5.5f}, {7.5f, 5.5f}};
        MapGeneratedFeature bridge;
        bridge.id = "river_bridge";
        bridge.kind = MapGeneratedFeatureKind::Bridge;
        bridge.points = {{3.5f, 3.5f}, {5.5f, 3.5f}};
        routeMap.generation.graph.features = {roadNetwork, bridge};

        MapRouteModel journey;
        journey.metadata.id = "journey";
        journey.startX = 1;
        journey.startY = 1;
        journey.endX = 7;
        journey.endY = 5;
        routeMap.routes.push_back(journey);

        const gm::map_route_presentation::Path first =
            gm::map_route_presentation::build(routeMap, journey);
        const gm::map_route_presentation::Path repeated =
            gm::map_route_presentation::build(routeMap, journey);
        const bool reachesBridge = std::any_of(
            first.points.begin(), first.points.end(),
            [](const MapFeaturePoint &point) {
                return static_cast<int>(std::floor(point.x)) == 4 &&
                       static_cast<int>(std::floor(point.y)) == 3;
            });
        const bool everyPointUsesWayNetwork = std::all_of(
            first.points.begin(), first.points.end(),
            [&](const MapFeaturePoint &point) {
                const MapCellModel *cell = mapCellAt(
                    routeMap, static_cast<int>(std::floor(point.x)),
                    static_cast<int>(std::floor(point.y)));
                return cell &&
                       (cell->kind == MapTileKind::Bridge ||
                        cell->kind == MapTileKind::Road ||
                        cell->wayKind == MapWayKind::Trail ||
                        cell->wayKind == MapWayKind::DirtRoad ||
                        cell->wayKind == MapWayKind::StoneRoad);
            });
        const bool bendsAwayFromDirectLine = std::any_of(
            first.points.begin() + std::min<std::size_t>(1, first.points.size()),
            first.points.end(),
            [&](const MapFeaturePoint &point) {
                const MapFeaturePoint &a = first.points.front();
                const MapFeaturePoint &b = first.points.back();
                return std::abs((b.x - a.x) * (point.y - a.y) -
                                (b.y - a.y) * (point.x - a.x)) > 0.01f;
            });
        ok &= require(first.source ==
                          gm::map_route_presentation::Source::RoadNetwork &&
                          first.points.size() >= 4 && reachesBridge &&
                          everyPointUsesWayNetwork,
                      "route presentation must follow the connected road network through its valid bridge");
        ok &= require(!gm::map_route_presentation::crossesUnbridgedWater(
                          routeMap, first.points) && bendsAwayFromDirectLine,
                      "network presentation must not draw a straight cut through water");
        ok &= require(first.source == repeated.source &&
                          first.points.size() == repeated.points.size() &&
                          std::equal(first.points.begin(), first.points.end(),
                                     repeated.points.begin(), samePoint),
                      "route presentation geometry must be deterministic");

        MapModel graphOnlyMap = routeMap;
        for (int y = 0; y < graphOnlyMap.height; ++y)
            for (int x = 0; x < graphOnlyMap.width; ++x)
            {
                MapCellModel *cell = mapCellAt(graphOnlyMap, x, y);
                if (x == 4)
                {
                    applyMapCellTerrainDefaults(*cell, MapTileKind::Water);
                    cell->wayKind = MapWayKind::River;
                }
                else
                    applyMapCellTerrainDefaults(*cell, MapTileKind::Empty);
            }
        const gm::map_route_presentation::Path graphOnlyPath =
            gm::map_route_presentation::build(graphOnlyMap, journey);
        ok &= require(graphOnlyPath.source ==
                          gm::map_route_presentation::Source::RoadNetwork &&
                          !gm::map_route_presentation::crossesUnbridgedWater(
                              graphOnlyMap, graphOnlyPath.points),
                      "feature-graph roads and bridges must form a presentation network without raster projections");

        MapRenderPlanRequest routePlanRequest;
        routePlanRequest.layers = MapRenderPlanRoutes;
        routePlanRequest.applyFog = false;
        const MapRenderPlan routePlan = compileMapRenderPlan(
            routeMap, routePlanRequest);
        const MapRenderPrimitive *routePrimitive = findPrimitive(
            routePlan, MapRenderPrimitiveKind::Route, "journey");
        ok &= require(routePrimitive &&
                          routePrimitive->assetRole == "route_network" &&
                          routePrimitive->points.size() == first.points.size() &&
                          std::equal(routePrimitive->points.begin(),
                                     routePrimitive->points.end(),
                                     first.points.begin(), samePoint),
                      "MapRenderPlan must retain the shared route presentation geometry");

        MapModel playerRouteMap = routeMap;
        playerRouteMap.visibility = MapVisibility::PlayersVisible;
        playerRouteMap.playerRelease.waysAndRoutes = true;
        for (MapGeneratedFeature &feature :
             playerRouteMap.generation.graph.features)
            feature.gmOnly = true;
        MapRenderPlanRequest playerRouteRequest;
        playerRouteRequest.audience = MapRenderAudience::Player;
        playerRouteRequest.layers = MapRenderPlanWays | MapRenderPlanRoutes;
        playerRouteRequest.applyFog = true;
        const MapRenderPlan playerRoutePlan = compileMapRenderPlan(
            playerRouteMap, playerRouteRequest);
        const MapRenderPrimitive *playerRoutePrimitive = findPrimitive(
            playerRoutePlan, MapRenderPrimitiveKind::Route, "journey");
        const MapModel playerRouteProjection = filterMapForRenderPlan(
            playerRouteMap, playerRoutePlan);
        ok &= require(playerRoutePrimitive &&
                          playerRoutePrimitive->assetRole != "route_network" &&
                          playerRoutePlan.generatedFeatureIds.empty() &&
                          std::none_of(playerRouteProjection.cells.begin(),
                                       playerRouteProjection.cells.end(),
                                       [](const MapCellModel &cell) {
                                           return cell.kind == MapTileKind::Road ||
                                                  cell.kind == MapTileKind::Bridge ||
                                                  cell.wayKind != MapWayKind::None;
                                       }),
                      "a public route must not reveal a GM-only feature network through its presentation geometry");

        MapModel safeFallbackMap;
        safeFallbackMap.width = 7;
        safeFallbackMap.height = 5;
        resizeMapCells(safeFallbackMap);
        applyMapCellTerrainDefaults(*mapCellAt(safeFallbackMap, 3, 2),
                                    MapTileKind::Water);
        MapRouteModel fallbackRoute;
        fallbackRoute.startX = 1;
        fallbackRoute.startY = 2;
        fallbackRoute.endX = 5;
        fallbackRoute.endY = 2;
        const gm::map_route_presentation::Path safeFallback =
            gm::map_route_presentation::build(safeFallbackMap, fallbackRoute);
        ok &= require(safeFallback.source ==
                          gm::map_route_presentation::Source::TerrainFallback &&
                          safeFallback.points.size() > 2 &&
                          !gm::map_route_presentation::crossesUnbridgedWater(
                              safeFallbackMap, safeFallback.points),
                      "fallback presentation must visibly detour around water when no road network exists");

        MapModel disconnectedMap = safeFallbackMap;
        for (int y = 0; y < disconnectedMap.height; ++y)
            applyMapCellTerrainDefaults(*mapCellAt(disconnectedMap, 3, y),
                                        MapTileKind::Water);
        const gm::map_route_presentation::Path directFallback =
            gm::map_route_presentation::build(disconnectedMap, fallbackRoute);
        ok &= require(directFallback.source ==
                          gm::map_route_presentation::Source::DirectFallback &&
                          gm::map_route_presentation::assetRole(
                              directFallback.source) ==
                              "route_direct_fallback" &&
                          gm::map_route_presentation::crossesUnbridgedWater(
                              disconnectedMap, directFallback.points),
                      "an unavoidable disconnected crossing must be explicitly classified as a dashed direct fallback");
    }

    ok &= require(gm::map_presentation::generatedName("City Gate", "de-DE") ==
                      "Stadttor" &&
                      gm::map_presentation::generatedName("Stadttor", "en-US") ==
                          "City Gate",
                  "known exact generator names must support explicit DE/EN presentation");
    ok &= require(gm::map_presentation::generatedName("Lake 12", "de") ==
                      "See 12" &&
                      gm::map_presentation::generatedName("See 12", "en") ==
                          "Lake 12",
                  "known numeric generator prefixes must support explicit DE/EN presentation");
    ok &= require(gm::map_presentation::generatedName("Lake Zurich", "de") ==
                      "Lake Zurich" &&
                      gm::map_presentation::generatedName("City Gatehouse", "de") ==
                          "City Gatehouse",
                  "user-authored names must not be translated by prefix or substring");

    {
        MapModel localeMap;
        localeMap.metadata.id = "locale_plan_map";
        localeMap.revision = 3;
        localeMap.atlasLevel = MapLevel::World;
        localeMap.width = 8;
        localeMap.height = 6;
        resizeMapCells(localeMap);

        MapGeneratedFeature gate;
        gate.id = "generated_gate";
        gate.kind = MapGeneratedFeatureKind::Settlement;
        gate.name = "City Gate";
        gate.points = {{2.5f, 2.5f}};
        MapGeneratedFeature authored = gate;
        authored.id = "authored_gatehouse";
        authored.kind = MapGeneratedFeatureKind::Landmark;
        authored.name = "City Gatehouse";
        authored.points = {{5.5f, 2.5f}};
        localeMap.generation.graph.features = {gate, authored};

        MapFeatureModel gateAnnotation;
        gateAnnotation.metadata.id = "generated_gate_annotation";
        gateAnnotation.kind = MapFeatureKind::Door;
        gateAnnotation.name = "City Gate";
        gateAnnotation.x = 3;
        gateAnnotation.y = 3;
        localeMap.features.push_back(gateAnnotation);
        MapFeatureModel authoredAnnotation = gateAnnotation;
        authoredAnnotation.metadata.id = "authored_gatehouse_annotation";
        authoredAnnotation.name = "City Gatehouse";
        authoredAnnotation.x = 4;
        localeMap.features.push_back(authoredAnnotation);

        MapRegionModel start;
        start.metadata.id = "generated_party_start";
        start.name = "Party Start";
        start.x = 0;
        start.y = 0;
        start.width = 2;
        start.height = 2;
        localeMap.regions.push_back(start);

        MapRouteModel route;
        route.metadata.id = "generated_tactical_route";
        route.name = "Tactical Route 3";
        route.startX = 0;
        route.startY = 4;
        route.endX = 7;
        route.endY = 4;
        localeMap.routes.push_back(route);

        MapRenderPlanRequest germanRequest;
        germanRequest.locale = "de-DE";
        germanRequest.applyFog = false;
        const MapRenderPlan germanPlan = compileMapRenderPlan(localeMap, germanRequest);
        MapRenderPlanRequest englishRequest = germanRequest;
        englishRequest.locale = "en-US";
        const MapRenderPlan englishPlan = compileMapRenderPlan(localeMap, englishRequest);

        const MapRenderPrimitive *germanGate = findPrimitive(
            germanPlan, MapRenderPrimitiveKind::Label, "generated_gate");
        const MapRenderPrimitive *englishGate = findPrimitive(
            englishPlan, MapRenderPrimitiveKind::Label, "generated_gate");
        const MapRenderPrimitive *germanAuthored = findPrimitive(
            germanPlan, MapRenderPrimitiveKind::Label, "authored_gatehouse");
        const MapRenderPrimitive *germanStart = findPrimitive(
            germanPlan, MapRenderPrimitiveKind::AnnotationRegion,
            "generated_party_start");
        const MapRenderPrimitive *germanRoute = findPrimitive(
            germanPlan, MapRenderPrimitiveKind::Route,
            "generated_tactical_route");
        const MapRenderPrimitive *germanGateAnnotation = findPrimitive(
            germanPlan, MapRenderPrimitiveKind::AnnotationPoint,
            "generated_gate_annotation");
        const MapRenderPrimitive *germanAuthoredAnnotation = findPrimitive(
            germanPlan, MapRenderPrimitiveKind::AnnotationPoint,
            "authored_gatehouse_annotation");
        ok &= require(germanGate && germanGate->label == "Stadttor" &&
                          englishGate && englishGate->label == "City Gate" &&
                          germanPlan.contentHash != englishPlan.contentHash,
                      "MapRenderPlan.locale must control known generated labels and its content hash");
        ok &= require(germanAuthored && germanAuthored->label == "City Gatehouse",
                      "MapRenderPlan must retain user-authored names verbatim");
        ok &= require(germanGateAnnotation &&
                          germanGateAnnotation->label == "Stadttor" &&
                          germanAuthoredAnnotation &&
                          germanAuthoredAnnotation->label == "City Gatehouse",
                      "known generator defaults in annotation records must localize without rewriting authored names");
        ok &= require(germanStart && germanStart->label == "Start der Gruppe" &&
                          germanRoute && germanRoute->label == "Taktische Route 3",
                      "MapRenderPlan locale must cover stable tactical region and route names");
        const MapModel germanProjection = filterMapForRenderPlan(localeMap, germanPlan);
        ok &= require(germanProjection.generation.graph.features.front().name ==
                          "City Gate" &&
                          germanProjection.routes.front().name == "Tactical Route 3",
                      "presentation localization must not rewrite filtered semantic map data");
    }

    ok &= require(gm::mapui::mapCreatorWorkflowColumnCount(1000.0f, 150.0f) == 5,
                  "wide workflow navigation must keep all five steps on one row");
    ok &= require(gm::mapui::mapCreatorWorkflowColumnCount(500.0f, 180.0f) == 2,
                  "medium workflow navigation must wrap into two columns");
    ok &= require(gm::mapui::mapCreatorWorkflowColumnCount(760.0f, 180.0f) == 3,
                  "workflow navigation must prefer a balanced three-plus-two layout");
    ok &= require(gm::mapui::mapCreatorWorkflowColumnCount(300.0f, 180.0f) == 1,
                  "narrow workflow navigation must stack without clipping labels");
    ok &= require(!gm::mapui::mapCreatorUsesStackedWorkspace(1500.0f, 16.0f),
                  "wide workspaces should retain the control-and-canvas layout");
    ok &= require(gm::mapui::mapCreatorUsesStackedWorkspace(1010.0f, 24.0f),
                  "minimum-window content at 150 percent text scaling must stack");
    ok &= require(gm::mapui::mapCreatorCandidateColumnCount(650.0f, 24.0f, 3) == 1,
                  "scaled candidate cards must stack when only one remains readable");
    ok &= require(gm::mapui::mapCreatorCandidateColumnCount(1000.0f, 24.0f, 3) == 2,
                  "medium scaled candidate views should use two readable columns");
    ok &= require(gm::mapui::mapCreatorCandidateColumnCount(1000.0f, 24.0f, 3, true) == 2,
                  "full-width stacked workspaces should compare two readable designs per row");
    ok &= require(gm::mapui::mapCreatorCandidateColumnCount(1200.0f, 16.0f, 3) == 3,
                  "wide candidate views should show all three designs side by side");
    ok &= require(!gm::mapui::mapCreatorShowsSeparateControlRail(true, true) &&
                      gm::mapui::mapCreatorShowsSeparateControlRail(true, false) &&
                      gm::mapui::mapCreatorShowsSeparateControlRail(false, true),
                  "only the stacked design step should fold its redundant control rail into the preview");
    ok &= require(gm::mapui::mapCreatorPairedActionColumnCount(
                      620.0f, 120.0f, 180.0f, 8.0f, 4.0f) == 2 &&
                      gm::mapui::mapCreatorPairedActionColumnCount(
                          280.0f, 120.0f, 180.0f, 8.0f, 4.0f) == 1,
                  "sticky design actions must remain visible by stacking when their labels do not fit");
    ok &= require(gm::mapui::mapCreatorPreviewViewportWidth(
                      1600.0f, 500.0f, 4.0f / 3.0f) < 800.0f &&
                      gm::mapui::mapCreatorPreviewViewportWidth(
                          620.0f, 500.0f, 4.0f / 3.0f) == 620.0f,
                  "design preview frames must avoid ultrawide empty gutters without exceeding narrow views");
    ok &= require(std::abs(gm::mapui::mapCreatorPreviewZoomAfterStep(1.0f, 1) - 1.25f) < 0.001f &&
                      std::abs(gm::mapui::mapCreatorPreviewZoomAfterWheel(1.25f, -1.0f) - 1.0f) < 0.001f &&
                      std::abs(gm::mapui::mapCreatorPreviewZoomAfterStep(4.0f, 1) - 4.0f) < 0.001f &&
                      std::abs(gm::mapui::mapCreatorPreviewZoomAfterStep(1.0f, -1) - 1.0f) < 0.001f,
                  "preview wheel and button zoom must use the same bounded, predictable steps");
    ok &= require(std::abs(gm::mapui::mapCreatorPreviewScrollForPointerAnchor(
                              100.0f, 200.0f, 800.0f, 1600.0f) - 400.0f) < 0.001f &&
                      gm::mapui::mapCreatorPreviewScrollForPointerAnchor(
                          50.0f, 20.0f, 0.0f, 900.0f) == 0.0f,
                  "cursor-anchored preview zoom must preserve the inspected map position safely");
    ok &= require(gm::mapui::mapCreatorPreviewLabelsNeedZoom(8, 620.0f, 1.0f) &&
                      !gm::mapui::mapCreatorPreviewLabelsNeedZoom(4, 620.0f, 1.0f) &&
                      !gm::mapui::mapCreatorPreviewLabelsNeedZoom(8, 620.0f, 1.5f),
                  "dense fitted labels should prompt inspection without warning after zooming in");
    ok &= require(std::abs(gm::mapui::mapCreatorPreviewContentGutter(
                              false, 1000.0f, 600.0f) - 4.0f) < 0.001f &&
                      std::abs(gm::mapui::mapCreatorPreviewContentGutter(
                                   true, 1000.0f, 600.0f) - 36.0f) < 0.001f &&
                      std::abs(gm::mapui::mapCreatorPreviewContentGutter(
                                   true, 200.0f, 120.0f) - 10.0f) < 0.001f,
                  "labelled previews must reserve bounded edge-label space without shrinking comparison cards");

    // Run the exact production candidate path before inspecting roles. A
    // hand-authored fixture would prove only the RenderPlan mapping, whereas
    // this gate ensures every design that can actually reach the chooser stays
    // within World-scale assets.
    {
        const std::string worldNote =
            "Three nations named Alderreach, Veyra, and Kharad share a great river. "
            "There is a large capital named Crosswater.";
        const MapWorldBrief worldBrief = MapWorldBriefInterpreter::interpret(worldNote);
        MapGenerationOptions worldOptions;
        worldOptions.generatorVersion = 5;
        worldOptions.recipeVersion = 3;
        worldOptions.scope = MapScope::World;
        worldOptions.scale = MapLevel::World;
        worldOptions.creationIntent = MapCreationIntent::WorldOverview;
        worldOptions.sizePreset = MapSizePreset::Standard;
        worldOptions.purpose = MapGenerationPurpose::Region;
        worldOptions.width = 72;
        worldOptions.height = 48;
        worldOptions.seed = 930002u;
        worldOptions.waterLevel = 18;
        worldOptions.mountainDensity = 12;
        worldOptions.forestDensity = 20;
        worldOptions.settlementDensity = 3;
        worldOptions.routeDensity = 28;
        worldOptions.roadNetwork = MapRoadNetworkMode::Connected;
        worldOptions.edgeConnectionCount = 4;
        worldOptions.requestedSettlements = 4;
        worldOptions.requestedLandmarks = 0;
        worldOptions.qualityAttempts = 24;
        worldOptions.nationNames = worldBrief.nationNames;
        worldOptions.facts = worldBrief.facts;
        worldOptions.knownFacts.push_back(worldNote);

        MapGenerationConstraint capital;
        capital.id = "brief_capital";
        capital.label = worldBrief.capitalName;
        capital.featureKind = MapGeneratedFeatureKind::Settlement;
        capital.x = static_cast<float>(worldOptions.width) * 0.52f;
        capital.y = static_cast<float>(worldOptions.height) * 0.48f;
        capital.required = true;
        capital.locked = true;
        worldOptions.constraints.push_back(std::move(capital));

        const MapGenerationBatchResult worldBatch =
            generateSemanticMapCandidateBatch(worldOptions, 3);
        ok &= require(worldBatch.succeeded() && !worldBatch.candidates.empty(),
                      "the World role gate requires at least one accepted production candidate");
        for (const MapGenerationResult &candidate : worldBatch.candidates)
        {
            const MapRenderPlan worldPlan =
                gm::mapui::compileMapCreatorPreviewRenderPlan(candidate.map, "de");
            const bool semanticGraphIsScaleSafe = std::none_of(
                candidate.map.generation.graph.features.begin(),
                candidate.map.generation.graph.features.end(),
                [](const MapGeneratedFeature &feature) {
                    return feature.kind == MapGeneratedFeatureKind::Vegetation ||
                           feature.kind == MapGeneratedFeatureKind::BuildingGroup ||
                           feature.kind == MapGeneratedFeatureKind::District ||
                           feature.kind == MapGeneratedFeatureKind::TacticalSetpiece;
                });
            ok &= require(candidate.validation.eligible &&
                              candidate.validation.blockers.empty(),
                          "only hard-valid candidates may be inspected as accepted World designs");
            ok &= require(semanticGraphIsScaleSafe &&
                              !worldPlanContainsForbiddenScaleRole(worldPlan),
                          "accepted World RenderPlans must never contain tactical vegetation, building, district, dungeon, cave, or detail roles");
        }
    }

    MapModel map;
    map.metadata.id = "preview_parity_map";
    map.visibility = MapVisibility::PlayersVisible;
    map.playerRelease = {true, true, true, true, true};
    map.revision = 17;
    map.atlasLevel = MapLevel::World;
    map.width = 4;
    map.height = 3;
    map.cells.resize(12);
    map.cells[1].kind = MapTileKind::Water;
    map.cells[6].fogged = true;
    map.generation.enabled = true;
    map.generation.continuousRendering = true;
    map.generation.recipe.styleSnapshotId = "fantasy_neutral_test_snapshot";

    MapGeneratedFeature river;
    river.id = "main_river";
    river.kind = MapGeneratedFeatureKind::River;
    river.name = "Hauptstrom";
    river.materialId = "river_deep";
    river.points = {{0.5f, 0.5f}, {3.5f, 2.5f}};

    MapGeneratedFeature gmLandmark;
    gmLandmark.id = "gm_landmark";
    gmLandmark.kind = MapGeneratedFeatureKind::Landmark;
    gmLandmark.name = "Verborgener Hain";
    gmLandmark.materialId = "discovery";
    gmLandmark.gmOnly = true;
    gmLandmark.points = {{2.5f, 1.5f}};

    MapGeneratedFeature road;
    road.id = "trade_road";
    road.kind = MapGeneratedFeatureKind::Road;
    road.name = "Handelsweg";
    road.materialId = "road_dirt";
    road.points = {{0.5f, 2.5f}, {3.5f, 0.5f}};

    MapGeneratedFeature settlement;
    settlement.id = "public_town";
    settlement.kind = MapGeneratedFeatureKind::Settlement;
    settlement.name = "Auheim";
    settlement.materialId = "town";
    settlement.points = {{1.5f, 1.5f}};

    MapGeneratedFeature territory;
    territory.id = "territory_a";
    territory.kind = MapGeneratedFeatureKind::TerrainRegion;
    territory.name = "Auenbund";
    territory.materialId = "territory.auenbund";
    territory.closed = true;
    territory.points = {{0.0f, 0.0f}, {2.0f, 0.0f},
                        {2.0f, 3.0f}, {0.0f, 3.0f}};

    // Deliberately scramble source order. The immutable plan must still group
    // water, ways, symbols, political geometry, and labels consistently.
    map.generation.graph.features.push_back(settlement);
    map.generation.graph.features.push_back(road);
    map.generation.graph.features.push_back(territory);
    map.generation.graph.features.push_back(river);
    map.generation.graph.features.push_back(gmLandmark);

    MapFeatureModel publicNote;
    publicNote.metadata.id = "public_note";
    publicNote.name = "Bekannter Treffpunkt";
    publicNote.x = 1;
    publicNote.y = 0;
    publicNote.discovered = true;
    map.features.push_back(publicNote);
    MapFeatureModel gmNote = publicNote;
    gmNote.metadata.id = "gm_note";
    gmNote.name = "Geheimer Treffpunkt";
    gmNote.gmOnly = true;
    map.features.push_back(gmNote);

    MapRegionModel publicRegion;
    publicRegion.metadata.id = "public_region";
    publicRegion.name = "Öffentliche Zone";
    publicRegion.x = 0;
    publicRegion.y = 1;
    publicRegion.width = 2;
    publicRegion.height = 2;
    map.regions.push_back(publicRegion);
    MapRegionModel gmRegion = publicRegion;
    gmRegion.metadata.id = "gm_region";
    gmRegion.gmOnly = true;
    map.regions.push_back(gmRegion);

    MapRouteModel publicRoute;
    publicRoute.metadata.id = "public_route";
    publicRoute.name = "Offene Route";
    publicRoute.startX = 0;
    publicRoute.startY = 2;
    publicRoute.endX = 3;
    publicRoute.endY = 2;
    publicRoute.discovered = true;
    map.routes.push_back(publicRoute);
    MapRouteModel hiddenRoute = publicRoute;
    hiddenRoute.metadata.id = "hidden_route";
    hiddenRoute.name = "Geheime Route";
    hiddenRoute.discovered = false;
    map.routes.push_back(hiddenRoute);

    MapTokenModel publicToken;
    publicToken.metadata.id = "public_token";
    publicToken.name = "Gruppe";
    publicToken.assetId = "token.party";
    publicToken.x = 2.0f;
    publicToken.y = 0.0f;
    map.tokens.push_back(publicToken);
    MapTokenModel hiddenToken = publicToken;
    hiddenToken.metadata.id = "hidden_token";
    hiddenToken.name = "Verborgener Gegner";
    hiddenToken.hiddenFromPlayers = true;
    map.tokens.push_back(hiddenToken);

    const MapRenderPlanRequest request =
        gm::mapui::mapCreatorPreviewRenderPlanRequest("de");
    const MapRenderPlanRequest thumbnailRequest =
        gm::mapui::mapCreatorCandidateThumbnailRenderPlanRequest("de");
    const MapRenderPlan preview =
        gm::mapui::compileMapCreatorPreviewRenderPlan(map, "de");
    const MapRenderPlan equivalentEditorPlan = compileMapRenderPlan(map, request);

    ok &= require(request.audience == MapRenderAudience::GameMaster,
                  "generation previews must use the GM audience");
    ok &= require(request.layers == gm::mapui::mapCreatorPreviewLayers,
                  "generation preview layer mask must stay canonical");
    ok &= require((request.layers & MapRenderPlanTerrain) != 0 &&
                      (request.layers & MapRenderPlanWater) != 0 &&
                      (request.layers & MapRenderPlanWays) != 0 &&
                      (request.layers & MapRenderPlanSymbols) != 0 &&
                      (request.layers & MapRenderPlanLabels) != 0,
                  "preview must contain the production presentation layers");
    ok &= require((request.layers & (MapRenderPlanAnnotations | MapRenderPlanRoutes |
                                     MapRenderPlanTokens | MapRenderPlanFog |
                                     MapRenderPlanGrid)) == 0,
                  "candidate preview must not contain editing/runtime overlays");
    ok &= require((thumbnailRequest.layers &
                   (MapRenderPlanTerrain | MapRenderPlanWater | MapRenderPlanWays |
                    MapRenderPlanSymbols)) ==
                      (MapRenderPlanTerrain | MapRenderPlanWater | MapRenderPlanWays |
                       MapRenderPlanSymbols) &&
                      (thumbnailRequest.layers &
                       (MapRenderPlanLabels | MapRenderPlanPolitical)) == 0,
                  "comparison thumbnails must retain composition while reserving labels and borders for the selected preview");
    ok &= require(!request.applyFog && !request.showGrid &&
                      !request.showPoliticalOverlay,
                  "candidate preview must have no hidden debug presentation state");
    ok &= require(preview.contentHash == equivalentEditorPlan.contentHash,
                  "preview and equivalent editor presentation must compile identically");
    ok &= require(samePrimitiveSequence(preview.primitives,
                                        equivalentEditorPlan.primitives),
                  "preview and editor must retain the same ordered primitive semantics");
    ok &= require(hasCanonicalLayerOrder(preview),
                  "preview primitives must follow the canonical back-to-front layer order");
    ok &= require(preview.styleSnapshotId == map.generation.recipe.styleSnapshotId,
                  "preview must remain pinned to the candidate production snapshot");
    ok &= require(mapRenderPlanContainsGeneratedFeature(preview, "main_river") &&
                      mapRenderPlanContainsGeneratedFeature(preview, "gm_landmark"),
                  "GM preview must consume the same generated feature graph");
    ok &= require(mapRenderPlanMatchesMap(preview, map),
                  "fresh preview plan must match its immutable map revision");
    MapModel silentlyChangedPreviewMap = map;
    silentlyChangedPreviewMap.generation.graph.features.front().points.front().x += 0.25f;
    ok &= require(mapRenderPlanMatchesMap(preview, silentlyChangedPreviewMap) &&
                      !mapRenderPlanContentMatchesMap(preview,
                                                     silentlyChangedPreviewMap),
                  "one-shot output validation must reject content changes even without a revision bump");

    const MapRenderPrimitive *riverPrimitive = findPrimitive(
        preview, MapRenderPrimitiveKind::GeneratedGeometry, "main_river");
    const MapRenderPrimitive *roadPrimitive = findPrimitive(
        preview, MapRenderPrimitiveKind::GeneratedGeometry, "trade_road");
    const MapRenderPrimitive *settlementPrimitive = findPrimitive(
        preview, MapRenderPrimitiveKind::GeneratedSymbol, "public_town");
    ok &= require(riverPrimitive && riverPrimitive->layer == MapRenderPlanWater &&
                      riverPrimitive->assetRole == "curve_profile" &&
                      riverPrimitive->assetId == "river_deep" &&
                      riverPrimitive->points.size() == 2 &&
                      samePoint(riverPrimitive->points.front(), {0.5f, 0.5f}),
                  "river primitive must pin layer, asset semantics, and map coordinates");
    ok &= require(roadPrimitive && roadPrimitive->layer == MapRenderPlanWays &&
                      roadPrimitive->assetRole == "curve_profile" &&
                      roadPrimitive->assetId == "road_dirt",
                  "road primitive must retain its curve-profile asset semantics");
    ok &= require(settlementPrimitive &&
                      settlementPrimitive->layer == MapRenderPlanSymbols &&
                      settlementPrimitive->assetRole == "map_symbol" &&
                      settlementPrimitive->assetId == "town" &&
                      samePoint(settlementPrimitive->points.front(), {1.5f, 1.5f}),
                  "world settlement primitive must remain a scale-safe map symbol");
    ok &= require(findPrimitive(preview, MapRenderPrimitiveKind::Label,
                                "territory_a") != nullptr,
                  "labels must remain an explicit ordered primitive pass");

    MapRenderPlanRequest taintedPreviewRequest = request;
    taintedPreviewRequest.layers |= MapRenderPlanInteractiveHelpers;
    const MapRenderPlan sanitizedPreview = compileMapRenderPlan(map, taintedPreviewRequest);
    ok &= require((sanitizedPreview.layers & MapRenderPlanInteractiveHelpers) == 0 &&
                      sanitizedPreview.contentHash == preview.contentHash &&
                      samePrimitiveSequence(sanitizedPreview.primitives,
                                            preview.primitives),
                  "interactive-only helpers must be stripped from immutable preview plans");

    map.importedReference.enabled = true;
    map.importedReference.projectRelativePath = "campaign-assets/original.png";
    map.importedReference.contentHash = "original-a";
    map.importedReference.assetId = "original_asset";
    map.importedReference.workingProxyGenerated = true;
    map.importedReference.workingProxyProjectRelativePath = "campaign-assets/proxy.png";
    map.importedReference.workingProxyContentHash = "proxy-a";
    map.importedReference.workingProxyAssetId = "proxy_asset";
    const MapRenderPlan proxyPlan =
        gm::mapui::compileMapCreatorPreviewRenderPlan(map, "de");
    ok &= require(proxyPlan.backgroundInteractiveAssetId == "proxy_asset" &&
                      proxyPlan.backgroundInteractiveProjectRelativePath ==
                          "campaign-assets/proxy.png" &&
                      proxyPlan.backgroundInteractiveContentHash == "proxy-a" &&
                      proxyPlan.backgroundExportAssetId == "original_asset" &&
                      proxyPlan.backgroundExportProjectRelativePath ==
                          "campaign-assets/original.png" &&
                      proxyPlan.backgroundExportContentHash == "original-a",
                  "one reviewed plan must bind both its interactive proxy and high-resolution export original");
    MapModel rotatedPreviewMap = map;
    rotatedPreviewMap.importedReference.crop = {0.1f, 0.2f, 0.7f, 0.6f};
    rotatedPreviewMap.importedReference.rotationDegrees = 90.0f;
    const MapRenderPlan rotatedPreviewPlan =
        gm::mapui::compileMapCreatorPreviewRenderPlan(rotatedPreviewMap, "de");
    const MapImageImport::NormalizedSamplingTransform rotatedPreviewTransform =
        MapImageImport::makeNormalizedSamplingTransform(
            rotatedPreviewMap.importedReference.crop,
            rotatedPreviewMap.importedReference.rotationDegrees);
    float rotatedU = 0.0f;
    float rotatedV = 0.0f;
    ok &= require(rotatedPreviewPlan.contentHash != proxyPlan.contentHash &&
                      MapImageImport::sampleNormalizedImage(
                          rotatedPreviewTransform, 0.25f, 0.75f,
                          rotatedU, rotatedV) &&
                      std::abs(rotatedU - 0.625f) < 0.00001f &&
                      std::abs(rotatedV - 0.65f) < 0.00001f,
                  "preview plans must bind crop/rotation and use the same exact quarter-turn source geometry as export");
    map.importedReference.contentHash = "original-b";
    const MapRenderPlan changedOriginalPlan =
        gm::mapui::compileMapCreatorPreviewRenderPlan(map, "de");
    ok &= require(proxyPlan.contentHash != changedOriginalPlan.contentHash &&
                      !mapRenderPlanContentMatchesMap(proxyPlan, map),
                  "an original-only identity change must invalidate a reviewed proxy/export plan");
    map.importedReference.contentHash = "original-a";
    map.importedReference.workingProxyContentHash = "proxy-b";
    const MapRenderPlan changedProxyPlan =
        gm::mapui::compileMapCreatorPreviewRenderPlan(map, "de");
    ok &= require(proxyPlan.contentHash != changedProxyPlan.contentHash &&
                      !mapRenderPlanContentMatchesMap(proxyPlan, map),
                  "a proxy-only identity change must invalidate the reviewed proxy/export plan");

    MapRenderPlanRequest gmOutputRequest;
    gmOutputRequest.audience = MapRenderAudience::GameMaster;
    gmOutputRequest.locale = "de";
    gmOutputRequest.layers = MapRenderPlanAll | MapRenderPlanInteractiveHelpers;
    gmOutputRequest.applyFog = true;
    gmOutputRequest.showGrid = true;
    gmOutputRequest.showPoliticalOverlay = true;
    const MapRenderPlan gmOutput = compileMapRenderPlan(map, gmOutputRequest);
    ok &= require((gmOutput.layers & MapRenderPlanInteractiveHelpers) == 0 &&
                      hasCanonicalLayerOrder(gmOutput),
                  "export-ready GM plan must be ordered and free of interactive helpers");
    const MapRenderPrimitive *background = findPrimitive(
        gmOutput, MapRenderPrimitiveKind::Background, "original_asset");
    const MapRenderPrimitive *annotation = findPrimitive(
        gmOutput, MapRenderPrimitiveKind::AnnotationPoint, "public_note");
    const MapRenderPrimitive *routePrimitive = findPrimitive(
        gmOutput, MapRenderPrimitiveKind::Route, "public_route");
    const MapRenderPrimitive *tokenPrimitive = findPrimitive(
        gmOutput, MapRenderPrimitiveKind::Token, "public_token");
    const MapRenderPrimitive *fogPrimitive = findPrimitive(
        gmOutput, MapRenderPrimitiveKind::FogCell,
        "preview_parity_map:fog:2:1");
    ok &= require(background && background->assetRole == "background" &&
                      background->assetId == "proxy_asset" &&
                      background->points.size() == 2 &&
                      samePoint(background->points.back(), {4.0f, 3.0f}),
                  "background primitive must identify the preferred asset and map extent");
    ok &= require(annotation && annotation->points.size() == 1 &&
                      samePoint(annotation->points.front(), {1.5f, 0.5f}),
                  "manual annotation must use the shared cell-centre convention");
    ok &= require(routePrimitive && routePrimitive->points.size() >= 2 &&
                      samePoint(routePrimitive->points.front(), {0.5f, 2.5f}) &&
                      samePoint(routePrimitive->points.back(), {3.5f, 2.5f}),
                  "route primitive must retain ordered cell-centre endpoints around its derived presentation path");
    ok &= require(tokenPrimitive && tokenPrimitive->assetId == "token.party" &&
                      tokenPrimitive->points.size() == 1 &&
                      samePoint(tokenPrimitive->points.front(), {2.5f, 0.5f}),
                  "token primitive must pin its asset and shared cell-centre coordinate");
    ok &= require(fogPrimitive && fogPrimitive->layer == MapRenderPlanFog,
                  "fog must be an explicit final content primitive");

    MapRenderPlanRequest playerOutputRequest = gmOutputRequest;
    playerOutputRequest.audience = MapRenderAudience::Player;
    playerOutputRequest.applyFog = false; // Player output must force it back on.
    const MapRenderPlan playerOutput = compileMapRenderPlan(map, playerOutputRequest);
    const MapRenderPlan equivalentPlayerExport =
        compileMapRenderPlan(map, playerOutputRequest);
    ok &= require(playerOutput.applyFog && !playerOutput.backgroundVisible &&
                      (playerOutput.layers & MapRenderPlanInteractiveHelpers) == 0 &&
                      samePrimitiveSequence(playerOutput.primitives,
                                            equivalentPlayerExport.primitives),
                  "player display and equivalent export must share ordered fail-closed primitives");
    ok &= require(findPrimitive(playerOutput, MapRenderPrimitiveKind::GeneratedSymbol,
                                "gm_landmark") == nullptr &&
                      findPrimitive(playerOutput, MapRenderPrimitiveKind::AnnotationPoint,
                                    "gm_note") == nullptr &&
                      findPrimitive(playerOutput, MapRenderPrimitiveKind::AnnotationRegion,
                                    "gm_region") == nullptr &&
                      findPrimitive(playerOutput, MapRenderPrimitiveKind::Route,
                                    "hidden_route") == nullptr &&
                      findPrimitive(playerOutput, MapRenderPrimitiveKind::Token,
                                    "hidden_token") == nullptr,
                  "private generated, annotation, route, and token primitives must not leak to player plans");
    ok &= require(findPrimitive(playerOutput, MapRenderPrimitiveKind::AnnotationPoint,
                                "public_note") != nullptr &&
                      findPrimitive(playerOutput, MapRenderPrimitiveKind::Route,
                                    "public_route") != nullptr &&
                      findPrimitive(playerOutput, MapRenderPrimitiveKind::Token,
                                    "public_token") != nullptr,
                  "public reviewed primitives must remain available to player output");

    MapModel privatePlayerMap = map;
    privatePlayerMap.visibility = MapVisibility::GameMasterOnly;
    const MapRenderPlan privatePlayerPlan =
        compileMapRenderPlan(privatePlayerMap, playerOutputRequest);
    ok &= require(privatePlayerPlan.layers == MapRenderPlanFog &&
                      privatePlayerPlan.applyFog && !privatePlayerPlan.backgroundVisible &&
                      privatePlayerPlan.generatedFeatureIds.empty() &&
                      privatePlayerPlan.manualFeatureIds.empty() &&
                      privatePlayerPlan.regionIds.empty() &&
                      privatePlayerPlan.routeIds.empty() &&
                      privatePlayerPlan.tokenIds.empty() &&
                      privatePlayerPlan.atlasInk.empty() &&
                      privatePlayerPlan.atlasWaterInk.empty() &&
                      std::all_of(privatePlayerPlan.primitives.begin(),
                                  privatePlayerPlan.primitives.end(),
                                  [](const MapRenderPrimitive &primitive) {
                                      return primitive.kind == MapRenderPrimitiveKind::FogCell;
                                  }),
                  "map-level privacy must withhold every public content primitive even when all category grants are enabled");
    ok &= require(!mapRenderPlanMatchesMap(playerOutput, privatePlayerMap),
                  "revoking map-level visibility must invalidate a previously reviewed public plan");

    MapModel incompletePlayerMap;
    incompletePlayerMap.metadata.id = "incomplete_player_grid";
    incompletePlayerMap.visibility = MapVisibility::PlayersVisible;
    incompletePlayerMap.width = 2;
    incompletePlayerMap.height = 1;
    incompletePlayerMap.cells.resize(1);
    MapRenderPlanRequest narrowPlayerRequest;
    narrowPlayerRequest.audience = MapRenderAudience::Player;
    narrowPlayerRequest.layers = MapRenderPlanTerrain;
    narrowPlayerRequest.applyFog = false;
    const MapRenderPlan incompletePlayerPlan =
        compileMapRenderPlan(incompletePlayerMap, narrowPlayerRequest);
    ok &= require(incompletePlayerPlan.applyFog &&
                      (incompletePlayerPlan.layers & MapRenderPlanFog) != 0 &&
                      findPrimitive(incompletePlayerPlan,
                                    MapRenderPrimitiveKind::FogCell,
                                    "incomplete_player_grid:fog:1:0") != nullptr,
                  "player plans must force fog and treat absent semantic cells as unrevealed");

    const std::uint64_t firstCacheKey =
        mapRenderPlanCacheKey(playerOutput, 2, 3, 4);
    ok &= require(firstCacheKey == mapRenderPlanCacheKey(playerOutput, 2, 3, 4),
                  "render cache keys must be deterministic");
    ok &= require(firstCacheKey != mapRenderPlanCacheKey(playerOutput, 3, 3, 4) &&
                      firstCacheKey != mapRenderPlanCacheKey(playerOutput, 2, 4, 4) &&
                      firstCacheKey != mapRenderPlanCacheKey(gmOutput, 2, 3, 4),
                  "LOD, chunk coordinates, and audience changes must invalidate render cache keys");
    MapRenderPlanRequest englishPlayerRequest = playerOutputRequest;
    englishPlayerRequest.locale = "en";
    const MapRenderPlan englishPlayer = compileMapRenderPlan(map, englishPlayerRequest);
    ok &= require(firstCacheKey != mapRenderPlanCacheKey(englishPlayer, 2, 3, 4),
                  "locale changes must invalidate label-bearing render cache keys");
    MapModel movedTokenMap = map;
    movedTokenMap.tokens.front().x += 0.25f;
    const MapRenderPlan movedTokenPlan = compileMapRenderPlan(
        movedTokenMap, playerOutputRequest);
    ok &= require(firstCacheKey != mapRenderPlanCacheKey(movedTokenPlan, 2, 3, 4),
                  "primitive coordinate changes must invalidate render cache keys even before a revision bump");
    MapModel changedPresentationMap = map;
    changedPresentationMap.generation.recipe.seed += 1u;
    const MapRenderPlan changedPresentationPlan = compileMapRenderPlan(
        changedPresentationMap, playerOutputRequest);
    ok &= require(changedPresentationPlan.contentHash != playerOutput.contentHash &&
                      !mapRenderPlanMatchesMap(playerOutput, changedPresentationMap) &&
                      firstCacheKey != mapRenderPlanCacheKey(changedPresentationPlan, 2, 3, 4),
                  "render seed changes must invalidate immutable plans and render cache keys before a revision bump");

    MapRenderPlanCache planCache(2);

    MapModel tacticalAnnotations = map;
    tacticalAnnotations.atlasLevel = MapLevel::Tactical;
    tacticalAnnotations.regions.front().name = "Party Start";
    MapRenderPlanRequest annotationRequest = playerOutputRequest;
    annotationRequest.layers = MapRenderPlanAnnotations;
    tacticalAnnotations.playerRelease.tokensAndAnnotations = true;
    const auto partyPlan = compileMapRenderPlan(tacticalAnnotations, annotationRequest);
    ok &= require(mapRenderPlanContentMatchesMap(partyPlan, tacticalAnnotations) &&
                      partyPlan.regionIds.size() == 1,
                  "The tactical annotation fixture must contain its public region with text labels disabled");
    tacticalAnnotations.regions.front().name = "Opposition Start";
    const auto oppositionPlan = compileMapRenderPlan(tacticalAnnotations, annotationRequest);
    ok &= require(partyPlan.contentHash != oppositionPlan.contentHash &&
                      !mapRenderPlanContentMatchesMap(partyPlan, tacticalAnnotations),
                  "Tactical region subtype edits must invalidate reviewed marker geometry even without text labels or a revision bump");
    tacticalAnnotations.regions.back().name = "Objective 1";
    ok &= require(mapRenderPlanContentMatchesMap(oppositionPlan, tacticalAnnotations),
                  "An excluded GM region subtype must not change the reviewed player plan");

    const auto cachedFirst = planCache.getOrCompile(map, request, 101u, 2.0f);
    const auto cachedAgain = planCache.getOrCompile(map, request, 101u, 2.0f);
    ok &= require(cachedFirst == cachedAgain && planCache.compileCount() == 1 &&
                      planCache.hitCount() == 1 && planCache.size() == 1,
                  "unchanged preview frames must reuse one compiled plan");

    MapRenderPlanCache routePlanCache(1);
    MapRenderPlanRequest routeRequest = request;
    routeRequest.layers = MapRenderPlanRoutes | MapRenderPlanLabels;
    const std::uint64_t routeTopologyVersion =
        gm::map_route_presentation::fingerprint(map);
    const auto cachedRoutePlan = routePlanCache.getOrCompile(
        map, routeRequest, routeTopologyVersion, 2.0f);
    const auto cachedRoutePlanAgain = routePlanCache.getOrCompile(
        map, routeRequest, routeTopologyVersion, 2.0f);
    ok &= require(cachedRoutePlan == cachedRoutePlanAgain &&
                      routePlanCache.compileCount() == 1 &&
                      findPrimitive(*cachedRoutePlan, MapRenderPrimitiveKind::Route,
                                    publicRoute.metadata.id) != nullptr,
                  "unchanged route overlays must reuse their compiled presentation path");
    MapModel editedRouteTopology = map;
    editedRouteTopology.routes.front().endY += 1;
    ok &= require(gm::map_route_presentation::fingerprint(editedRouteTopology) !=
                      routeTopologyVersion,
                  "interactive route endpoint edits must invalidate the route presentation cache before commit");
    editedRouteTopology = map;
    editedRouteTopology.cells.front().wayKind = MapWayKind::StoneRoad;
    ok &= require(gm::map_route_presentation::fingerprint(editedRouteTopology) !=
                      routeTopologyVersion,
                  "interactive road-network edits must invalidate the route presentation cache before commit");

    const MapRenderPlanCacheKey baseInputKey =
        makeMapRenderPlanCacheKey(map, request, 101u, 2.0f);
    MapRenderPlanRequest layerVariantRequest = request;
    layerVariantRequest.layers &= ~static_cast<std::uint32_t>(MapRenderPlanLabels);
    MapModel visibilityVariantMap = map;
    visibilityVariantMap.visibility = MapVisibility::GameMasterOnly;
    MapModel snapshotVariantMap = map;
    snapshotVariantMap.generation.recipe.styleSnapshotId = "another_snapshot";
    MapModel presentationVariantMap = map;
    presentationVariantMap.generation.continuousRendering =
        !presentationVariantMap.generation.continuousRendering;
    MapModel backgroundIdentityVariantMap = map;
    backgroundIdentityVariantMap.importedReference.contentHash = "original-cache-variant";
    MapRenderPlanRequest playerVariantRequest = request;
    playerVariantRequest.audience = MapRenderAudience::Player;
    ok &= require(!(baseInputKey == makeMapRenderPlanCacheKey(
                                      map, layerVariantRequest, 101u, 2.0f)) &&
                      !(baseInputKey == makeMapRenderPlanCacheKey(
                                           visibilityVariantMap, request, 101u, 2.0f)) &&
                      !(baseInputKey == makeMapRenderPlanCacheKey(
                                           snapshotVariantMap, request, 101u, 2.0f)) &&
                      !(baseInputKey == makeMapRenderPlanCacheKey(
                                           presentationVariantMap, request, 101u, 2.0f)) &&
                      !(baseInputKey == makeMapRenderPlanCacheKey(
                                           backgroundIdentityVariantMap, request,
                                           101u, 2.0f)) &&
                      !(baseInputKey == makeMapRenderPlanCacheKey(
                                           map, playerVariantRequest, 101u, 2.0f)),
                  "layers, visibility, render mode, production snapshot, imported-background identity, and audience must be part of the plan-cache key");

    MapRenderPlanRequest cachedEnglishRequest = request;
    cachedEnglishRequest.locale = "en";
    const auto cachedEnglish = planCache.getOrCompile(
        map, cachedEnglishRequest, 101u, 2.0f);
    ok &= require(cachedEnglish != cachedFirst && planCache.compileCount() == 2,
                  "locale changes must compile a distinct cached plan");
    const auto overviewLod = planCache.getOrCompile(map, request, 101u, 8.0f);
    ok &= require(overviewLod != cachedFirst && planCache.compileCount() == 3 &&
                      planCache.size() == planCache.maximumEntries(),
                  "render LOD changes must invalidate derived preview data within the bounded cache");

    MapModel changedCachedMap = map;
    changedCachedMap.generation.graph.features.front().name = "Changed in a new generator batch";
    const auto changedCachedPlan = planCache.getOrCompile(
        changedCachedMap, request, 102u, 2.0f);
    ok &= require(changedCachedPlan->contentHash != cachedFirst->contentHash &&
                      planCache.compileCount() == 4 && planCache.size() == 2,
                  "a new caller content version must never reuse stale same-revision output");

    MapRenderPlanRequest customPolicyRequest = request;
    customPolicyRequest.additionalGeneratedFeatureVisibility =
        [](const MapGeneratedFeature &) { return true; };
    const MapRenderPlan customPolicyPlan =
        compileMapRenderPlan(map, customPolicyRequest);
    ok &= require(mapRenderPlanContentMatchesMap(customPolicyPlan, map),
                  "strong plan validation must reproduce a captured generated-feature allow-list without retaining its callback");
    const std::size_t entriesBeforeCustomPolicy = planCache.size();
    planCache.getOrCompile(map, customPolicyRequest, 101u, 2.0f);
    planCache.getOrCompile(map, customPolicyRequest, 101u, 2.0f);
    ok &= require(planCache.compileCount() == 6 &&
                      planCache.size() == entriesBeforeCustomPolicy,
                  "unkeyable captured visibility policies must compile safely without entering the cache");

    planCache.invalidateMap(map.metadata.id);
    ok &= require(planCache.size() == 0,
                  "map invalidation must remove every locale, layer, and LOD variant");

    ++map.revision;
    ok &= require(!mapRenderPlanMatchesMap(preview, map),
                  "a plan from an older map revision must be rejected");

    if (!ok)
        return 1;
    std::cout << "Map preview parity tests passed.\n";
    return 0;
}
