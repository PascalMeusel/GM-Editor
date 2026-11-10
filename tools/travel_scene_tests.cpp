#include "Models/Combat/EncounterAtlasService.hpp"
#include "Models/Map/TravelSceneGenerator.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <tuple>
#include <vector>

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

MapModel sourceMap()
{
    MapModel map;
    map.metadata.id = "north-road-region";
    map.name = "North Road";
    map.atlasLevel = MapLevel::Region;
    map.mapKind = MapKind::Geographic;
    map.revision = 4;
    map.width = 40;
    map.height = 28;
    map.generation.recipe.seed = 77;
    resizeMapCells(map);
    MapRouteModel route;
    route.metadata.id = "route-north";
    route.name = "Old North Road";
    route.terrain = "forest hills";
    route.distanceMiles = 28.0f;
    map.routes.push_back(route);
    return map;
}

TravelSceneRecipe recipe()
{
    TravelSceneRecipe value;
    value.sourceMapId = "north-road-region";
    value.routeId = "route-north";
    value.routeName = "Old North Road";
    value.biome = "forest hills";
    value.purpose = TravelScenePurpose::Danger;
    value.dangerLevel = 3;
    value.seed = 9911;
    value.contentLanguage = "en";
    value.nameCulture = "germanic_central_european";
    value.requiredElements = {"abandoned tower", "river crossing"};
    value.excludedElements = {"city"};
    value.combatReady = true;
    return value;
}

bool sameCells(const MapModel &left, const MapModel &right)
{
    if (left.cells.size() != right.cells.size())
        return false;
    for (std::size_t index = 0; index < left.cells.size(); ++index)
        if (left.cells[index].kind != right.cells[index].kind || left.cells[index].wayKind != right.cells[index].wayKind)
            return false;
    return true;
}

bool sameGeneratedFeature(const MapGeneratedFeature &left, const MapGeneratedFeature &right)
{
    if (left.id != right.id || left.kind != right.kind || left.name != right.name ||
        left.materialId != right.materialId || std::abs(left.width - right.width) > 0.001f ||
        left.closed != right.closed || left.locked != right.locked ||
        left.discovered != right.discovered || left.gmOnly != right.gmOnly ||
        left.lineage.sourceMapId != right.lineage.sourceMapId ||
        left.lineage.sourceFeatureId != right.lineage.sourceFeatureId ||
        left.lineage.sourceRevision != right.lineage.sourceRevision ||
        left.points.size() != right.points.size())
        return false;
    for (std::size_t index = 0; index < left.points.size(); ++index)
        if (std::abs(left.points[index].x - right.points[index].x) > 0.001f ||
            std::abs(left.points[index].y - right.points[index].y) > 0.001f)
            return false;
    return true;
}

bool riversNeverClimb(const MapModel &map)
{
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::River)
            continue;
        int previousElevation = std::numeric_limits<int>::max();
        for (const MapFeaturePoint &point : feature.points)
        {
            const int x = std::clamp(static_cast<int>(std::floor(point.x)), 0, std::max(0, map.width - 1));
            const int y = std::clamp(static_cast<int>(std::floor(point.y)), 0, std::max(0, map.height - 1));
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell)
                continue;
            if (cell->elevation > previousElevation)
                return false;
            previousElevation = cell->elevation;
        }
    }
    return true;
}

std::pair<int, int> tacticalCoverClusterEvidence(const MapModel &map)
{
    std::vector<MapFeaturePoint> points;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::TacticalSetpiece &&
            (feature.materialId == "fantasy_neutral.tactical.cover" ||
             feature.materialId == "fantasy_neutral.tactical.rubble") &&
            !feature.points.empty())
            points.push_back(feature.points.front());
    int adjacent = 0;
    for (std::size_t first = 0; first < points.size(); ++first)
        for (std::size_t second = 0; second < points.size(); ++second)
        {
            if (first == second)
                continue;
            const float dx = std::abs(points[first].x - points[second].x);
            const float dy = std::abs(points[first].y - points[second].y);
            if (dx <= 1.01f && dy <= 1.01f)
            {
                ++adjacent;
                break;
            }
        }
    return {static_cast<int>(points.size()), adjacent};
}
}

int main()
{
    const MapModel source = sourceMap();
    const TravelSceneRecipe request = recipe();
    const auto first = TravelSceneGenerator::generate(source, &source.routes.front(), request, 3);
    const auto repeated = TravelSceneGenerator::generate(source, &source.routes.front(), request, 3);

    bool ok = true;
    ok &= require(first.size() == 3 && repeated.size() == 3, "Travel generator should return three candidates");
    if (first.size() == 3 && repeated.size() == 3)
    {
        ok &= require(first.front().seed == repeated.front().seed && sameCells(first.front().localMap, repeated.front().localMap),
                      "Travel generation must be deterministic");
        ok &= require(first.front().localMap.atlasLevel == MapLevel::Site && first.front().localMap.parentMapId == source.metadata.id,
                      "A Region travel scene should become a linked Site map");
        const MapDimensions siteStandard = mapDimensionsForPreset(MapCreationIntent::TravelScene, MapSizePreset::Standard);
        ok &= require(first.front().localMap.gridType == GridType::None &&
                          first.front().localMap.width == siteStandard.width &&
                          first.front().localMap.height == siteStandard.height,
                      "Travel scenes should stay gridless and use the standard Site preset");
        ok &= require(first.front().localMap.generation.recipe.generatorVersion == 5 &&
                          first.front().localMap.generation.graph.schemaVersion == 2,
                      "Travel scenes should use generator v5 and feature graph v2");
        ok &= require(first.front().localMap.creationIntent == MapCreationIntent::TravelScene &&
                          first.front().localMap.sizePreset == MapSizePreset::Standard &&
                          first.front().localMap.generation.recipe.creationIntent == MapCreationIntent::TravelScene &&
                          first.front().localMap.generation.recipe.sizePreset == MapSizePreset::Standard,
                      "Travel scenes should retain their canonical V5 intent and size preset");
        ok &= require(first.front().localMap.generation.recipe.contentLanguage == "en" &&
                          first.front().localMap.generation.recipe.nameCulture == "germanic_central_european",
                      "Travel scenes should persist content language and normalized name culture");
        ok &= require(first.front().quality.candidatesEvaluated == 24 &&
                          mapGenQualityMeetsCandidateFloor(first.front().localMap, first.front().quality) &&
                          validateMapGeneration(first.front().localMap, first.front().quality).eligible,
                      "Three-design travel search should evaluate 24 attempts and expose only valid quality candidates");
        ok &= require(first.front().localMap.generation.recipe.requiredElements == request.requiredElements &&
                          first.front().localMap.generation.recipe.excludedElements == request.excludedElements,
                      "Custom must/include constraints should remain in the recipe");
        ok &= require(first.front().canPrepareEncounter, "Danger scenes should support encounter preparation");
        ok &= require(first.front().title == repeated.front().title &&
                          first.front().summary == repeated.front().summary &&
                          first.front().discovery == repeated.front().discovery &&
                          first.front().complication == repeated.front().complication &&
                          first.front().gmSecret == repeated.front().gmSecret,
                      "Generated travel narrative should be deterministic");
        ok &= require(first.front().title.find("Danger") != std::string::npos &&
                          first.front().summary.rfind("A dangerous", 0) == 0,
                      "English travel recipes should generate English titles and summaries");

        EncounterAtlasTacticalRequest tacticalRequest;
        tacticalRequest.sourceRect = {0.0f, 0.0f, 1.0f, 1.0f};
        tacticalRequest.width = 24;
        tacticalRequest.height = 18;
        tacticalRequest.cellSize = 5.0f;
        EncounterModel encounter;
        const auto tactical = EncounterAtlasService::deriveTacticalMap(first.front().localMap, tacticalRequest, &encounter);
        ok &= require(tactical.valid && tactical.tacticalMap.gridType == GridType::Square && encounter.useMapCells,
                      "A selected travel scene should transfer to a square-grid tactical encounter");
    }
    TravelSceneRecipe germanRequest = request;
    germanRequest.routeName = "Alte Nordstraße";
    germanRequest.contentLanguage = "de-DE";
    const auto germanCandidates = TravelSceneGenerator::generate(source, &source.routes.front(), germanRequest, 3);
    ok &= require(germanCandidates.size() == 3, "German travel generation should return three strict candidates");
    if (germanCandidates.size() == 3 && first.size() == 3)
    {
        ok &= require(germanCandidates.front().seed == first.front().seed &&
                          sameCells(germanCandidates.front().localMap, first.front().localMap),
                      "Content language should not alter deterministic travel geometry");
        ok &= require(germanCandidates.front().title.find("Gefahr") != std::string::npos &&
                          germanCandidates.front().summary.rfind("Eine gefährliche", 0) == 0 &&
                          germanCandidates.front().discovery.find("The ") == std::string::npos &&
                          germanCandidates.front().complication.find("The ") == std::string::npos &&
                          germanCandidates.front().gmSecret.find("The ") == std::string::npos,
                      "German travel recipes should not persist English narrative text");
        const auto focus = std::find_if(
            germanCandidates.front().localMap.generation.graph.features.begin(),
            germanCandidates.front().localMap.generation.graph.features.end(),
            [](const MapGeneratedFeature &feature) { return feature.materialId == "fantasy_neutral.travel.danger"; });
        ok &= require(focus != germanCandidates.front().localMap.generation.graph.features.end() &&
                          focus->name == "Gefahrenstelle",
                      "Generated travel focus labels should follow the content language");
        ok &= require(germanCandidates.front().localMap.generation.recipe.contentLanguage == "de",
                      "Regional language tags should normalize to a stable persisted language code");
    }
    const TravelSceneRecipe backwardCompatibleDefaults;
    ok &= require(backwardCompatibleDefaults.schemaVersion == 2 &&
                      backwardCompatibleDefaults.contentLanguage == "en" &&
                      backwardCompatibleDefaults.nameCulture == "neutral_fantasy",
                  "New travel recipe language fields should preserve historical English/neutral defaults");

    // A blank/open brief used to inherit the RiverValley enum's two-river
    // composition even though the GM had not requested water.  It also stored
    // quiet and danger foci only in the graph, so the shared render plan hid
    // the unmatched point projection and the scene looked empty.
    TravelSceneRecipe openQuiet;
    openQuiet.routeId = source.routes.front().metadata.id;
    openQuiet.routeName = source.routes.front().name;
    openQuiet.biome = "open plains";
    openQuiet.purpose = TravelScenePurpose::Quiet;
    openQuiet.seed = 42005u;
    const auto openCandidates = TravelSceneGenerator::generate(
        source, &source.routes.front(), openQuiet, 1);
    ok &= require(openCandidates.size() == 1,
                  "An open quiet travel brief should produce one valid scene");
    if (!openCandidates.empty())
    {
        const MapModel &openMap = openCandidates.front().localMap;
        ok &= require(std::none_of(openMap.generation.graph.features.begin(),
                                   openMap.generation.graph.features.end(),
                                   [](const MapGeneratedFeature &feature) {
                                       return feature.kind == MapGeneratedFeatureKind::River;
                                   }),
                      "An open travel brief must not invent prominent rivers");
        const auto focus = std::find_if(
            openMap.generation.graph.features.begin(),
            openMap.generation.graph.features.end(),
            [](const MapGeneratedFeature &feature) {
                return feature.materialId == "fantasy_neutral.travel.rest";
            });
        ok &= require(focus != openMap.generation.graph.features.end() &&
                          !focus->points.empty(),
                      "A quiet travel scene should publish its visible focus");
        if (focus != openMap.generation.graph.features.end() && !focus->points.empty())
        {
            const int focusX = static_cast<int>(std::floor(focus->points.front().x));
            const int focusY = static_cast<int>(std::floor(focus->points.front().y));
            const MapCellModel *focusCell = mapCellAt(openMap, focusX, focusY);
            int nearestRoad = openMap.width + openMap.height;
            for (int y = 0; y < openMap.height; ++y)
                for (int x = 0; x < openMap.width; ++x)
                    if (const MapCellModel *cell = mapCellAt(openMap, x, y);
                        cell && mapGenWayKindIsRoadLike(cell->wayKind))
                        nearestRoad = std::min(nearestRoad,
                                               std::abs(focusX - x) +
                                                   std::abs(focusY - y));
            ok &= require(focusCell && focusCell->kind == MapTileKind::Landmark,
                          "A travel focus must have a matching raster projection");
            ok &= require(nearestRoad >= 1 && nearestRoad <= 3,
                          "A travel focus should be composed beside the route");
        }
    }

    TravelSceneRecipe contradictory = request;
    contradictory.requiredElements = {"sealed shrine"};
    contradictory.excludedElements = {"sealed shrine"};
    ok &= require(TravelSceneGenerator::generate(source, &source.routes.front(), contradictory, 3).empty(),
                  "Contradictory travel requirements should return no candidates instead of a bad fallback");
    ok &= require(TravelSceneGenerator::parseElementList("tower, river; no bandits\ntower").size() == 3,
                  "Custom element parser should normalize separators and duplicates");
    int tacticalProgressEvents = 0;
    MapGenerationProgress tacticalLastProgress;
    MapGenerationOptions tacticalEditorOptions;
    tacticalEditorOptions.generatorVersion = 5;
    tacticalEditorOptions.recipeVersion = 3;
    tacticalEditorOptions.creationIntent = MapCreationIntent::TacticalEncounter;
    tacticalEditorOptions.scale = MapLevel::Tactical;
    tacticalEditorOptions.scope = MapScope::Local;
    tacticalEditorOptions.sizePreset = MapSizePreset::Custom;
    tacticalEditorOptions.quality = MapGenerationQualityLevel::Balanced;
    tacticalEditorOptions.qualityAttempts = 24;
    tacticalEditorOptions.purpose = MapGenerationPurpose::TravelArea;
    tacticalEditorOptions.landscape = MapLandscapeProfile::RiverValley;
    tacticalEditorOptions.seed = 5511;
    tacticalEditorOptions.width = 24;
    tacticalEditorOptions.height = 18;
    tacticalEditorOptions.tacticalDensity = 24;
    tacticalEditorOptions.requestedLandmarks = 1;
    tacticalEditorOptions.requiredElements = {"broken wagon"};
    tacticalEditorOptions.progressCallback = [&](const MapGenerationProgress &progress) {
        ++tacticalProgressEvents;
        tacticalLastProgress = progress;
    };
    const MapGenerationBatchResult battleMaps =
        TacticalMapGenerator::generateBatch(tacticalEditorOptions, &source, 3);
    ok &= require(battleMaps.succeeded() && battleMaps.candidates.size() == 3 &&
                      battleMaps.recommendedIndex == 0 && battleMaps.attemptsEvaluated == 24,
                  "The editor tactical adapter should return three valid designs from the fixed balanced search");
    ok &= require(tacticalProgressEvents > 0 && tacticalLastProgress.step == tacticalLastProgress.totalSteps &&
                      tacticalLastProgress.stage == "Complete",
                  "The editor tactical adapter should report semantic search and composition progress to completion");
    if (!battleMaps.candidates.empty())
    {
        const MapGenerationResult &battleMap = battleMaps.candidates.front();
        ok &= require(battleMap.map.atlasLevel == MapLevel::Tactical && battleMap.map.mapKind == MapKind::Encounter &&
                          battleMap.map.gridType == GridType::Square && battleMap.map.cellSize == 5.0f,
                      "Battle-map candidates should use square 5-foot tactical semantics");
        ok &= require(battleMap.map.creationIntent == MapCreationIntent::TacticalEncounter &&
                          battleMap.map.generation.recipe.generatorVersion == 5 &&
                          battleMap.map.generation.recipe.requiredElements == tacticalEditorOptions.requiredElements &&
                          battleMap.quality.candidatesEvaluated == 24,
                      "Balanced battle-map candidates should use the canonical V5 contract and 24 attempts");
        ok &= require(battleMap.map.regions.size() >= 2 && battleMap.warnings.empty() && battleMap.validation.eligible,
                      "Battle maps should provide two valid start zones and pass readiness checks");
        ok &= require(mapGenQualityMeetsCandidateFloor(battleMap.map, battleMap.quality) &&
                          validateMapGeneration(battleMap.map, battleMap.quality).eligible,
                      "Battle-map candidates should pass hard validation and every relevant quality floor");
        ok &= require(battleMap.map.name.rfind("River Crossing", 0) == 0 &&
                          battleMap.map.parentMapId == source.metadata.id,
                      "Editor tactical generation should use the release template path and retain its atlas source");
    }

    bool cancelTacticalEditorSearch = true;
    MapGenerationOptions cancelledTacticalOptions = tacticalEditorOptions;
    cancelledTacticalOptions.progressCallback = {};
    cancelledTacticalOptions.cancellationRequested = [&]() { return cancelTacticalEditorSearch; };
    const MapGenerationBatchResult cancelledTactical =
        TacticalMapGenerator::generateBatch(cancelledTacticalOptions, &source, 3);
    ok &= require(cancelledTactical.failure == MapGenerationBatchFailureCode::Cancelled &&
                      cancelledTactical.candidates.empty(),
                  "A cancelled editor tactical search must not expose a partially completed battlefield");

    TacticalMapRecipe tacticalDraft;
    tacticalDraft.mapTemplate = TacticalMapTemplate::OpenGround;
    tacticalDraft.seed = 7351u;
    tacticalDraft.width = 24;
    tacticalDraft.height = 18;
    tacticalDraft.quality = MapGenerationQualityLevel::Draft;
    MapGenerationOptions draftEditorOptions;
    draftEditorOptions.generatorVersion = 5;
    draftEditorOptions.creationIntent = MapCreationIntent::TacticalEncounter;
    draftEditorOptions.scale = MapLevel::Tactical;
    draftEditorOptions.scope = MapScope::Local;
    draftEditorOptions.sizePreset = MapSizePreset::Custom;
    draftEditorOptions.quality = MapGenerationQualityLevel::Draft;
    draftEditorOptions.qualityAttempts = 12;
    draftEditorOptions.purpose = MapGenerationPurpose::Wilderness;
    draftEditorOptions.landscape = MapLandscapeProfile::Highlands;
    draftEditorOptions.seed = tacticalDraft.seed;
    draftEditorOptions.width = tacticalDraft.width;
    draftEditorOptions.height = tacticalDraft.height;
    draftEditorOptions.requestedLandmarks = 1;
    const MapGenerationBatchResult draftTactical =
        TacticalMapGenerator::generateBatch(draftEditorOptions, &source, 1);
    const MapGenerationBatchResult repeatedDraftTactical =
        TacticalMapGenerator::generateBatch(draftEditorOptions, &source, 1);
    ok &= require(draftTactical.candidates.size() == 1 && repeatedDraftTactical.candidates.size() == 1 &&
                      draftTactical.attemptsEvaluated == 12,
                  "Draft tactical search should find one strict candidate deterministically");
    if (!draftTactical.candidates.empty() && !repeatedDraftTactical.candidates.empty())
        ok &= require(draftTactical.candidates.front().quality.candidatesEvaluated == 12 &&
                          draftTactical.candidates.front().quality.selectedSeed == repeatedDraftTactical.candidates.front().quality.selectedSeed &&
                          sameCells(draftTactical.candidates.front().map, repeatedDraftTactical.candidates.front().map),
                      "Draft tactical search should use 12 deterministic attempts");

    MapGenerationOptions polishedEditorOptions = draftEditorOptions;
    polishedEditorOptions.quality = MapGenerationQualityLevel::Polished;
    polishedEditorOptions.qualityAttempts = 48;
    const MapGenerationBatchResult polishedTactical =
        TacticalMapGenerator::generateBatch(polishedEditorOptions, &source, 1);
    ok &= require(polishedTactical.candidates.size() == 1 && polishedTactical.attemptsEvaluated == 48 &&
                      polishedTactical.candidates.front().quality.candidatesEvaluated == 48,
                  "Polished tactical search should use the fixed 48-attempt V5 budget");

    MapGenerationOptions currentTactical = tacticalEditorOptions;
    currentTactical.generatorVersion = 7;
    ok &= require(TacticalMapGenerator::recipeFromGenerationOptions(currentTactical).generatorVersion == 7,
                  "The tactical editor adapter must propagate explicit V7 composition requests");
    std::vector<TacticalMapCandidate> legacyForest;
    for (const int version : {5, 6, 7})
    {
        TacticalMapRecipe forest;
        forest.generatorVersion = version;
        forest.mapTemplate = TacticalMapTemplate::ForestRoad;
        forest.width = 28; forest.height = 20; forest.seed = 803119u; forest.coverDensity = 18;
        const auto candidates = TacticalMapGenerator::generate(forest, nullptr, 1);
        ok &= require(candidates.size() == 1, "The actual forest-road gallery request must retain an eligible tactical candidate");
        if (candidates.empty()) continue;
        const auto &candidate = candidates.front();
        const auto &map = candidate.map;
        const auto mainRoad = std::find_if(map.generation.graph.features.begin(), map.generation.graph.features.end(), [](const auto &feature) {
            return feature.id.find("::forest_road") != std::string::npos;
        });
        const auto bypass = std::find_if(map.generation.graph.features.begin(), map.generation.graph.features.end(), [](const auto &feature) {
            return feature.id.find("::forest_trail") != std::string::npos;
        });
        ok &= require(mainRoad != map.generation.graph.features.end() && bypass != map.generation.graph.features.end(),
                      "A forest battlefield must retain both editable road and alternate trail geometry");
        if (mainRoad == map.generation.graph.features.end() || bypass == map.generation.graph.features.end()) continue;
        if (version < 7)
        {
            ok &= require(candidate.quality.selectedSeed == 148729748u && bypass->points.size() == 5 &&
                              bypass->points.front().x == 5.5f && bypass->points.back().x == 22.5f &&
                              map.metadata.customProperties.at("tactical.composition_revision") == "2",
                          "V5/V6 must reproduce the actual old forest-road candidate and its stored composition pin");
            if (version == 5) legacyForest = candidates;
            else if (!legacyForest.empty())
                ok &= require(sameCells(map, legacyForest.front().map) &&
                                  map.generation.graph.features.size() == legacyForest.front().map.generation.graph.features.size() &&
                                  std::equal(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                                             legacyForest.front().map.generation.graph.features.begin(), sameGeneratedFeature),
                              "Adding V7 must not alter legacy tactical cells or graph controls");
        }
        else
        {
            const float left = bypass->points.front().x, right = bypass->points.back().x;
            const float roadY = bypass->points.front().y;
            const bool endpointsOnRoad = std::any_of(mainRoad->points.begin(), mainRoad->points.end(), [&](auto p) {
                return p.x == left && p.y == roadY;
            }) && std::any_of(mainRoad->points.begin(), mainRoad->points.end(), [&](auto p) {
                return p.x == right && p.y == bypass->points.back().y;
            });
            int shelteredInterior = 0;
            for (int y = static_cast<int>(roadY) - 3; y < static_cast<int>(roadY); ++y)
                for (int x = static_cast<int>(left) + 1; x < static_cast<int>(right); ++x)
                    if (const auto *cell = mapCellAt(map, x, y); cell && cell->blocksLineOfSight &&
                        (cell->kind == MapTileKind::Forest || cell->kind == MapTileKind::Cover))
                        ++shelteredInterior;
            ok &= require(endpointsOnRoad && right - left <= map.width * 0.36f && shelteredInterior >= 2 &&
                              mainRoad->width > bypass->width,
                          "V7 must replace the empty map-spanning forest loop with a short sheltered bypass joined to the primary road");
            ok &= require(bypass->points.size() == 6 &&
                              std::abs(bypass->points[1].y - bypass->points[4].y) >= 1.0f &&
                              bypass->points[1].x > left,
                          "The covered bypass must follow unequal grove shoulders rather than form a symmetrical arch with two identical stubs");
            ok &= require(map.generation.recipe.generatorVersion == 7 &&
                              map.metadata.customProperties.at("tactical.composition_revision") == "3" &&
                              candidate.readinessIssues.empty() && mapGenQualityMeetsCandidateFloor(map, candidate.quality),
                          "The revised forest encounter must persist V7 provenance and pass the unchanged playable-route gates");
        }
    }

    for (int templateIndex = 0; templateIndex < 8; ++templateIndex)
    {
        TacticalMapRecipe templateRecipe;
        templateRecipe.mapTemplate = static_cast<TacticalMapTemplate>(templateIndex);
        templateRecipe.seed = 88001u + static_cast<std::uint32_t>(templateIndex) * 977u;
        templateRecipe.width = 32;
        templateRecipe.height = 24;
        templateRecipe.coverDensity = 28;
        templateRecipe.objectiveCount = 2;
        const auto templateCandidates = TacticalMapGenerator::generate(templateRecipe, &source, 1);
        if (templateCandidates.empty())
        {
            std::cerr << "Tactical template " << templateIndex << " returned no candidate\n";
            ok = false;
            continue;
        }
        const TacticalMapCandidate &candidate = templateCandidates.front();
        const auto compositionRevision = candidate.map.metadata.customProperties.find("tactical.composition_revision");
        ok &= require(compositionRevision != candidate.map.metadata.customProperties.end() &&
                          compositionRevision->second == std::to_string(kTacticalCompositionRevision),
                      "New tactical maps must retain their composition revision without changing the recipe schema");
        if (!candidate.readinessIssues.empty())
        {
            std::cerr << "Tactical template " << templateIndex << " readiness: "
                      << candidate.readinessIssues.front() << '\n';
            ok = false;
        }
        ok &= require(candidate.quality.coherentTerrainPercent >= 80,
                      "Every tactical template should retain coherent base terrain");
        ok &= require(candidate.quality.traversableConnectivityPercent >= 80,
                      "Every tactical template should retain connected playable floor");
        ok &= require(candidate.quality.candidatesEvaluated == 24 &&
                          mapGenQualityMeetsCandidateFloor(candidate.map, candidate.quality) &&
                          validateMapGeneration(candidate.map, candidate.quality).eligible,
                      "Every balanced tactical template should pass the strict V5 24-attempt gate");
        const int routeRegionCount = static_cast<int>(std::count_if(
            candidate.map.regions.begin(), candidate.map.regions.end(), [](const MapRegionModel &region) {
                return region.name.rfind("Tactical Route ", 0) == 0;
            }));
        ok &= require(routeRegionCount >= 2,
                      "Every tactical template should publish two distinct validated route regions");
        if (templateRecipe.mapTemplate == TacticalMapTemplate::OpenGround ||
            templateRecipe.mapTemplate == TacticalMapTemplate::ForestRoad ||
            templateRecipe.mapTemplate == TacticalMapTemplate::RiverCrossing)
        {
            bool hasInteriorBend = false;
            bool hasBoundaryConnector = false;
            for (const auto &feature : candidate.map.generation.graph.features)
            {
                if (feature.kind != MapGeneratedFeatureKind::Road &&
                    feature.kind != MapGeneratedFeatureKind::Trail)
                    continue;
                for (std::size_t index = 1; index < feature.points.size(); ++index)
                {
                    const auto &a = feature.points[index - 1];
                    const auto &b = feature.points[index];
                    hasInteriorBend |= std::abs(a.x - b.x) > 0.5f &&
                                       std::abs(a.y - b.y) > 0.5f;
                    hasBoundaryConnector |= std::abs(a.x - b.x) < 0.1f &&
                        std::abs(a.y - b.y) > 3.0f &&
                        (a.x <= 3.5f || a.x >= candidate.map.width - 3.5f);
                }
            }
            ok &= require(hasInteriorBend && !hasBoundaryConnector,
                          "Outdoor tactical routes should branch through terrain without a rectangular boundary road loop");
        }
        if (templateRecipe.mapTemplate == TacticalMapTemplate::OpenGround)
        {
            const auto [coverCount, clusteredCoverCount] =
                tacticalCoverClusterEvidence(candidate.map);
            ok &= require(coverCount >= 6 && coverCount <= 12,
                          "Tactical cover density should stay within the readable production range");
            ok &= require(clusteredCoverCount * 100 >= coverCount * 60,
                          "Most tactical cover should form readable encounter clusters instead of isolated prop confetti");
        }
        if (templateRecipe.mapTemplate == TacticalMapTemplate::Cave ||
            templateRecipe.mapTemplate == TacticalMapTemplate::StoneDungeon)
        {
            const int blockedBackground = static_cast<int>(std::count_if(
                candidate.map.cells.begin(), candidate.map.cells.end(), [](const MapCellModel &cell) {
                    return cell.kind == MapTileKind::Wall;
                }));
            ok &= require(candidate.map.scope == MapScope::Dungeon && blockedBackground > 0,
                          "Subterranean tactical templates should preserve intentional rock or wall background");
        }
    }

    // Fixed release-QA regressions: a quiet scene used to paint its clearing
    // over a river sample, and a social scene could create an isolated bridge
    // strip instead of joining the existing travel network.
    for (const auto &[localIndex, seed, purpose] :
         std::vector<std::tuple<int, std::uint32_t, TravelScenePurpose>>{
             {265, 3798538u, TravelScenePurpose::Quiet},
             {3007, 25512436u, TravelScenePurpose::Social}})
    {
        MapModel qaSource;
        qaSource.metadata.id = "qa_source_world";
        qaSource.atlasLevel = MapLevel::World;
        qaSource.revision = 7;
        MapRouteModel qaRoute;
        qaRoute.metadata.id = "qa_route";
        qaRoute.name = "QA Journey";
        TravelSceneRecipe qaRecipe;
        qaRecipe.routeId = qaRoute.metadata.id;
        qaRecipe.routeName = qaRoute.name;
        qaRecipe.biome = localIndex % 3 == 0 ? "Wald" : "Flusstal";
        qaRecipe.seed = seed;
        qaRecipe.purpose = purpose;
        qaRecipe.combatReady = localIndex % 4 == 0;
        const auto candidates = TravelSceneGenerator::generate(qaSource, &qaRoute, qaRecipe, 1);
        ok &= require(candidates.size() == 1, "Release travel regression should return one candidate");
        if (!candidates.empty())
        {
            ok &= require(candidates.front().quality.candidatesEvaluated == 12,
                          "Single-result travel QA should use the fixed 12-attempt search");
            ok &= require(riversNeverClimb(candidates.front().localMap),
                          "Travel composition must preserve downhill river samples");
            ok &= require(candidates.front().quality.wayConnectivityPercent >= 80,
                          "Generated travel bridges must join the visible route network");
        }
    }

    // Fixed release case tactical_outdoor_6274: the editable road spline must
    // visibly cross the river at a right angle so every bridge is valid.
    TacticalMapRecipe bridgeRegression;
    bridgeRegression.mapTemplate = static_cast<TacticalMapTemplate>(6274 % 8);
    bridgeRegression.width = 24 + 6274 % 41;
    bridgeRegression.height = 18 + (6274 / 3) % 37;
    bridgeRegression.seed = 53383815u;
    bridgeRegression.coverDensity = 16 + 6274 % 42;
    MapModel bridgeSource;
    bridgeSource.metadata.id = "qa_source_region";
    bridgeSource.atlasLevel = MapLevel::Region;
    bridgeSource.revision = 4;
    const auto bridgeCandidates = TacticalMapGenerator::generate(bridgeRegression, &bridgeSource, 1);
    ok &= require(bridgeCandidates.size() == 1, "Release bridge regression should return one candidate");
    if (!bridgeCandidates.empty())
    {
        ok &= require(bridgeCandidates.front().quality.bridgeValidityPercent == 100,
                      "Tactical river-crossing bridges must align with water and approach roads");
        ok &= require(bridgeCandidates.front().quality.criticalIssueCount == 0,
                      "A tactical river crossing must not retain a critical bridge issue");
    }


    TacticalMapRecipe inheritedGeometry;
    inheritedGeometry.mapTemplate = TacticalMapTemplate::OpenGround;
    inheritedGeometry.width = 32;
    inheritedGeometry.height = 24;
    inheritedGeometry.seed = 31877u;
    inheritedGeometry.quality = MapGenerationQualityLevel::Draft;
    inheritedGeometry.coverDensity = 24;
    MapGeneratedFeature inheritedRiver;
    inheritedRiver.id = "river-parent@atlas_0";
    inheritedRiver.kind = MapGeneratedFeatureKind::River;
    inheritedRiver.name = "Inherited River";
    inheritedRiver.materialId = "fantasy_neutral.water.river";
    inheritedRiver.width = 1.4f;
    inheritedRiver.locked = true;
    inheritedRiver.points = {{16.5f, 0.5f}, {16.5f, 23.5f}};
    inheritedRiver.lineage = {source.metadata.id, "river-parent", source.revision};
    MapGeneratedFeature inheritedRoad;
    inheritedRoad.id = "road-parent@atlas_0";
    inheritedRoad.kind = MapGeneratedFeatureKind::Road;
    inheritedRoad.name = "Inherited Road";
    inheritedRoad.materialId = "fantasy_neutral.road.dirt";
    inheritedRoad.width = 0.8f;
    inheritedRoad.locked = true;
    inheritedRoad.points = {{0.5f, 4.5f}, {31.5f, 4.5f}};
    inheritedRoad.lineage = {source.metadata.id, "road-parent", source.revision};
    inheritedGeometry.preservedFeatures = {inheritedRiver, inheritedRoad};

    const auto inheritedCandidates = TacticalMapGenerator::generate(inheritedGeometry, &source, 1);
    const auto inheritedRepeated = TacticalMapGenerator::generate(inheritedGeometry, &source, 1);
    ok &= require(inheritedCandidates.size() == 1 && inheritedRepeated.size() == 1,
                  "Tactical generation should compose a valid map around inherited atlas geometry");
    if (!inheritedCandidates.empty() && !inheritedRepeated.empty())
    {
        const MapModel &map = inheritedCandidates.front().map;
        for (const MapGeneratedFeature &expected : inheritedGeometry.preservedFeatures)
        {
            const auto actual = std::find_if(map.generation.graph.features.begin(),
                                             map.generation.graph.features.end(),
                                             [&](const MapGeneratedFeature &feature) {
                                                 return feature.id == expected.id;
                                             });
            ok &= require(actual != map.generation.graph.features.end() &&
                              sameGeneratedFeature(*actual, expected),
                          "Tactical output must retain inherited IDs, geometry, lineage and locks exactly");
            ok &= require(std::find(map.generation.recipe.lockedFeatureIds.begin(),
                                    map.generation.recipe.lockedFeatureIds.end(), expected.id) !=
                              map.generation.recipe.lockedFeatureIds.end(),
                          "Inherited tactical features must remain explicitly locked");
        }
        const MapCellModel *crossing = mapCellAt(map, 16, 4);
        ok &= require(crossing && crossing->kind == MapTileKind::Bridge &&
                          mapGenWayKindIsRoadLike(crossing->wayKind),
                      "Inherited road/river intersections must become traversable tactical crossings");
        const auto party = std::find_if(map.regions.begin(), map.regions.end(), [](const MapRegionModel &region) {
            return region.name == "Party Start";
        });
        const auto opposition = std::find_if(map.regions.begin(), map.regions.end(), [](const MapRegionModel &region) {
            return region.name == "Opposition Start";
        });
        ok &= require(party != map.regions.end() && opposition != map.regions.end() &&
                          std::abs((party->y + party->height / 2) - 4) <= 1 &&
                          std::abs((opposition->y + opposition->height / 2) - 4) <= 1,
                      "Inherited road boundary entries must guide tactical deployment placement");
        ok &= require(inheritedCandidates.front().readinessIssues.empty() &&
                          sameCells(map, inheritedRepeated.front().map),
                      "Inherited tactical composition must remain playable and deterministic");
    }
    for (const int generationVersion : {8,9})
    for (const auto mapTemplate : {TacticalMapTemplate::ForestRoad,TacticalMapTemplate::RiverCrossing,TacticalMapTemplate::Ruin})
        for (const auto seed : {803119u,1542562u,1137291u})
        {
            TacticalMapRecipe recipe;
            recipe.generatorVersion=generationVersion; recipe.mapTemplate=mapTemplate;
            recipe.width=32;recipe.height=24;recipe.seed=seed;
            recipe.quality=MapGenerationQualityLevel::Draft;
            const auto candidates=TacticalMapGenerator::generate(recipe,nullptr,1);
            ok &= require(candidates.size()==1,"V8/V9 outdoor templates must retain an eligible design across fixed seeds");
            if (candidates.empty()) continue;
            const auto &candidate=candidates.front();
            ok &= require(candidate.readinessIssues.empty() && mapGenQualityMeetsCandidateFloor(candidate.map,candidate.quality),
                          "V8/V9 art/composition changes must retain deployment, objectives, access and creature clearance");
            ok &= require(candidate.map.metadata.customProperties.at("tactical.composition_revision") ==
                          (generationVersion == 9 ? "5" : "4"),
                          "Tactical composition changes must persist their explicit revision");
            if (mapTemplate==TacticalMapTemplate::RiverCrossing)
                ok &= require(std::count_if(candidate.map.generation.graph.features.begin(),candidate.map.generation.graph.features.end(),
                    [](const auto &feature){return feature.kind==MapGeneratedFeatureKind::Bridge;})>=2,
                    "The crossing composition must retain two usable bridge choices");
        }
    return ok ? 0 : 1;
}
