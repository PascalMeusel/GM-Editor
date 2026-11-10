#include "Models/Map/TravelSceneGenerator.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>
#include "Models/Map/TravelSceneInternal.hpp"


namespace TravelSceneGenerator
{
using namespace detail;

std::vector<std::string> parseElementList(const std::string &text)
{
    std::vector<std::string> result;
    std::string normalized = text;
    for (char &c : normalized)
        if (c == ';' || c == '\n')
            c = ',';
    std::stringstream stream(normalized);
    std::string value;
    while (std::getline(stream, value, ','))
    {
        value = trim(value);
        if (!value.empty() && std::find(result.begin(), result.end(), value) == result.end())
            result.push_back(std::move(value));
    }
    return result;
}

std::vector<TravelSceneCandidate> generate(
    const MapModel &sourceMap,
    const MapRouteModel *route,
    const TravelSceneRecipe &recipe,
    int candidateCount,
    const MapGenerationCancellationCallback &cancellationRequested,
    const MapGenerationProgressCallback &progressCallback)
{
    if (cancellationRequested && cancellationRequested())
        return {};
    if (sourceMap.metadata.id.empty() || sourceMap.atlasLevel == MapLevel::Tactical)
        return {};
    candidateCount = std::clamp(candidateCount, 1, 3);

    const bool german = usesGermanNarrative(recipe);
    const std::string contentLanguage = normalizedContentLanguage(recipe.contentLanguage);
    const std::string nameCulture = normalizedNameCulture(recipe);
    const bool composedTravel = recipe.schemaVersion >= 3;
    const MapSizePreset sizePreset = composedTravel ? MapSizePreset::Custom : MapSizePreset::Standard;
    const MapDimensions dimensions = mapDimensionsForPreset(MapCreationIntent::TravelScene, sizePreset);
    const int attemptBudget = candidateCount == 1 ? 12 : 24;

    MapGenerationOptions options;
    options.cancellationRequested = cancellationRequested;
    options.progressCallback = progressCallback;
    options.generatorVersion = recipe.schemaVersion >= 4 ? 12 : composedTravel ? 11 : 5;
    options.recipeVersion = 3;
    options.styleId = recipe.schemaVersion >= 4 ? "illustrated_atlas" : defaultMapStyleId();
    options.styleVersion = recipe.schemaVersion >= 4 ? 6 : composedTravel ? 5 : 3;
    options.styleSnapshotId = recipe.schemaVersion >= 4 ? "illustrated_atlas_v6" : mapStyleCurrentProductionSnapshotId();
    options.scope = MapScope::Local;
    options.scale = MapLevel::Site;
    options.creationIntent = MapCreationIntent::TravelScene;
    options.sizePreset = sizePreset;
    options.purpose = MapGenerationPurpose::TravelArea;
    options.landscape = landscapeFor(recipe);
    options.quality = MapGenerationQualityLevel::Balanced;
    options.width = composedTravel ? std::clamp(recipe.width > 0 ? recipe.width : 48, 24, 80) : dimensions.width;
    options.height = composedTravel ? std::clamp(recipe.height > 0 ? recipe.height : 32, 18, 80) : dimensions.height;
    options.seed = recipe.seed ^ stableHash(sourceMap.metadata.id) ^ stableHash(recipe.routeId);
    options.contentLanguage = contentLanguage;
    options.nameCulture = nameCulture;
    // An unspecified or dry/open travel brief must not silently become a map
    // with two parallel rivers merely because RiverValley is the historical
    // fallback enum.  Water is prominent only when the biome or a required
    // element actually asks for it.
    options.waterLevel = options.landscape == MapLandscapeProfile::LakeCountry
                             ? 48
                             : (travelRecipeRequestsWater(recipe) ? 28 : 4);
    options.mountainDensity = options.landscape == MapLandscapeProfile::MountainPass ? (composedTravel ? 30 : 55) : (composedTravel ? 8 : 18);
    options.forestDensity = options.landscape == MapLandscapeProfile::ForestFrontier ? 58 : 30;
    options.settlementDensity = recipe.purpose == TravelScenePurpose::Social ? 16 : 4;
    options.routeDensity = 34;
    options.tacticalDensity = recipe.schemaVersion >= 4 ? 0 : recipe.purpose == TravelScenePurpose::Danger ? 42 : 20;
    if(composedTravel) {
        // A travel vignette needs one readable watercourse, not several
        // parallel canal-like streams from the broad landscape fallback.
        options.waterLevel=std::min(options.waterLevel,35);
        if(travelRecipeRequestsWater(recipe) && options.landscape==MapLandscapeProfile::RiverValley)
            options.landscape=MapLandscapeProfile::Highlands;
    }
    if(travelWaterCompositionV4(recipe)) {
        // Compose an actual shore or basin after the dry route skeleton exists.
        // Old recipes keep their historical drainage and incidental ponds.
        options.waterLevel=0;options.landscape=MapLandscapeProfile::Highlands;
        options.mountainDensity=3;
        options.forestDensity=travelWaterCompositionV4(recipe)==1?22:35;
    }
    options.roadNetwork = MapRoadNetworkMode::Sparse;
    options.qualityAttempts = attemptBudget;
    // The purpose composer supplies the meeting cart; only historical recipes
    // retain the incidental hamlet from the landscape pass.
    options.requestedSettlements = recipe.schemaVersion < 4 && recipe.purpose == TravelScenePurpose::Social ? 1 : 0;
    options.requestedLandmarks = recipe.schemaVersion >= 4 ? static_cast<int>(recipe.requiredElements.size()) : std::max(2, static_cast<int>(recipe.requiredElements.size()));
    options.edgeConnectionCount = 2;
    options.knownFacts = {recipe.routeName, recipe.biome, recipe.season, recipe.timeOfDay, recipe.weather};
    options.requiredElements = recipe.requiredElements;
    options.excludedElements = recipe.excludedElements;

    for (std::size_t index = 0; index < recipe.requiredElements.size(); ++index)
    {
        const float fraction = static_cast<float>(index + 1) / static_cast<float>(recipe.requiredElements.size() + 1);
        MapGenerationConstraint constraint;
        constraint.id = "travel_required_" + std::to_string(index);
        constraint.label = recipe.requiredElements[index];
        constraint.featureKind = MapGeneratedFeatureKind::Landmark;
        constraint.x = 5.0f + fraction * static_cast<float>(options.width - 10);
        constraint.y = static_cast<float>(options.height) * (0.34f + 0.28f * static_cast<float>((index * 7) % 3) / 2.0f);
        constraint.required = true;
        constraint.locked = true;
        options.constraints.push_back(std::move(constraint));
    }

    // Run the complete fixed batch before composing the travel focus. A base
    // map that passed hard validation but missed only a soft pre-composition
    // floor may be repaired internally; every finished scene is validated and
    // scored again, so no rejected base leaks out as a visible fallback.
    MapGenerationBatchResult batch = generateSemanticMapCandidateBatch(options, attemptBudget);
    if (batch.failure == MapGenerationBatchFailureCode::InvalidRequest ||
        batch.failure == MapGenerationBatchFailureCode::Cancelled)
        return {};

    std::vector<MapGenerationResult> generated = std::move(batch.candidates);
    for (MapGenerationResult &rejected : batch.rejectedCandidates)
        if (rejected.validation.eligible)
            generated.push_back(std::move(rejected));
    if (generated.empty())
        return {};

    std::vector<MapGenerationResult> composed;
    composed.reserve(generated.size());
    for (MapGenerationResult &result : generated)
    {
        if (cancellationRequested && cancellationRequested())
            return {};
        const std::uint32_t selectedSeed = result.quality.selectedSeed;
        const int selectedCandidateIndex = result.quality.selectedCandidateIndex;
        MapModel &map = result.map;
        map.metadata.id = sourceMap.metadata.id + "::travel::" + stableIdPart(recipe.routeId) + "::" +
                          std::to_string(selectedSeed);
        map.creationIntent = MapCreationIntent::TravelScene;
        map.sizePreset = sizePreset;
        map.scope = MapScope::Local;
        map.atlasLevel = MapLevel::Site;
        map.mapKind = MapKind::Geographic;
        map.gridType = GridType::None;
        map.atlasStatus = MapAtlasStatus::Linked;
        map.visibility = MapVisibility::GameMasterOnly;
        map.parentMapId = sourceMap.metadata.id;
        map.revision = 1;
        map.atlasPlacement.parentId = sourceMap.metadata.id;
        map.atlasPlacement.parentRevision = sourceMap.revision;
        map.atlasPlacement.sourceRevision = map.revision;
        map.atlasPlacement.parentRect = {0.1f, 0.1f, 0.8f, 0.8f};
        map.generation.recipe.generatorVersion = options.generatorVersion;
        map.generation.recipe.recipeVersion = 3;
        map.generation.recipe.styleId = options.styleId;
        map.generation.recipe.styleVersion = options.styleVersion;
        map.generation.recipe.styleSnapshotId = options.styleSnapshotId;
        map.generation.recipe.scale = MapLevel::Site;
        map.generation.recipe.quality = MapGenerationQualityLevel::Balanced;
        map.generation.recipe.creationIntent = MapCreationIntent::TravelScene;
        map.generation.recipe.sizePreset = sizePreset;
        map.generation.recipe.contentLanguage = contentLanguage;
        map.generation.recipe.nameCulture = nameCulture;
        map.generation.recipe.requiredElements = recipe.requiredElements;
        map.generation.recipe.excludedElements = recipe.excludedElements;
        if(!composeTravelWaterV4(map, recipe, selectedSeed))continue;
        if(travelWaterCompositionV4(recipe)) {
            auto &features=map.generation.graph.features;
            for(auto &f:features)if(!f.locked && (f.kind==MapGeneratedFeatureKind::Vegetation || f.kind==MapGeneratedFeatureKind::MountainRange)) {
                f.points.erase(std::remove_if(f.points.begin(),f.points.end(),[&](auto p){
                    const auto *c=mapCellAt(map,int(p.x),int(p.y));return !c || c->kind==MapTileKind::Water || c->kind==MapTileKind::Bridge;
                }),f.points.end());
            }
            features.erase(std::remove_if(features.begin(),features.end(),[](const auto &f){return !f.locked && f.points.empty();}),features.end());
        }
        composeTravelRoad(map, recipe, selectedSeed);
        composeTravelPurpose(map, recipe, selectedSeed);
        composeTravelFurnishingsV4(map, recipe);
        if(composedTravel) {
            std::vector<MapCellCoordinate> passages;
            for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x)
                if(mapGenWayKindIsRoadLike(mapCellAt(map,x,y)->wayKind))passages.push_back({x,y});
            for(const auto &p:passages)for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox) {
                auto *c=mapCellAt(map,p.x+ox,p.y+oy);
                if(c && c->kind==MapTileKind::Mountain)applyMapCellTerrainDefaults(*c,MapTileKind::Stone);
            }
        }

        result.quality = analyzeMapGenerationQuality(map);
        result.quality.candidatesEvaluated = attemptBudget;
        result.quality.selectedCandidateIndex = selectedCandidateIndex;
        result.quality.selectedSeed = selectedSeed;
        result.validation = validateMapGeneration(map, result.quality);
        if (!result.validation.eligible || !mapGenQualityMeetsCandidateFloor(map, result.quality))
            continue;
        composed.push_back(std::move(result));
    }

    composed = mapGenSelectDiverseCandidates(std::move(composed), candidateCount);
    if (composed.empty())
        return {};

    static constexpr std::array<const char *, 6> discoveriesEn = {
        "A weathered trail marker points toward a forgotten shortcut.",
        "Fresh tracks reveal that another group passed here recently.",
        "An unusual natural formation offers shelter and a clear view.",
        "A half-buried relic links this route to an older settlement.",
        "A local traveler shares a useful rumor about the road ahead.",
        "A hidden spring makes this a valuable resting place."};
    static constexpr std::array<const char *, 6> discoveriesDe = {
        "Ein verwitterter Wegweiser weist auf eine vergessene Abkürzung.",
        "Frische Spuren zeigen, dass vor Kurzem eine andere Gruppe hier vorbeikam.",
        "Eine ungewöhnliche Felsformation bietet Schutz und eine gute Aussicht.",
        "Ein halb verschüttetes Relikt verbindet diesen Weg mit einer älteren Siedlung.",
        "Ein ortskundiger Reisender teilt ein nützliches Gerücht über den Weg voraus.",
        "Eine verborgene Quelle macht diesen Ort zu einem wertvollen Rastplatz."};
    static constexpr std::array<const char *, 6> complicationsEn = {
        "Changing weather makes the safest path slower than expected.",
        "The direct crossing is damaged and demands a deliberate choice.",
        "Signs of territorial creatures force the party to move carefully.",
        "A blocked section creates two routes with different risks.",
        "A stranger asks for help before the party can continue.",
        "The landmark is occupied, but its inhabitants have not noticed the party."};
    static constexpr std::array<const char *, 6> complicationsDe = {
        "Wechselndes Wetter macht den sichersten Weg langsamer als erwartet.",
        "Der direkte Übergang ist beschädigt und verlangt eine bewusste Entscheidung.",
        "Spuren territorialer Kreaturen zwingen die Gruppe zu vorsichtigem Vorgehen.",
        "Ein blockierter Abschnitt eröffnet zwei Wege mit unterschiedlichen Risiken.",
        "Eine fremde Person bittet um Hilfe, bevor die Gruppe weiterziehen kann.",
        "Der Orientierungspunkt ist besetzt, doch seine Bewohner haben die Gruppe noch nicht bemerkt."};
    static constexpr std::array<const char *, 6> secretsEn = {
        "The apparent shortcut was marked recently to lure travelers away from the road.",
        "The tracks belong to someone connected to an unresolved campaign thread.",
        "A concealed cache contains evidence, not ordinary treasure.",
        "The local guide knows more about the danger than they admit.",
        "The obstacle can be bypassed by interacting with the nearby landmark.",
        "The area will become important again on the return journey."};
    static constexpr std::array<const char *, 6> secretsDe = {
        "Die vermeintliche Abkürzung wurde erst kürzlich markiert, um Reisende vom Weg fortzulocken.",
        "Die Spuren gehören zu jemandem, der mit einem offenen Handlungsfaden verbunden ist.",
        "Ein verborgenes Lager enthält Beweise statt gewöhnlicher Schätze.",
        "Der ortskundige Führer weiß mehr über die Gefahr, als er zugibt.",
        "Das Hindernis lässt sich durch den nahen Orientierungspunkt umgehen.",
        "Dieses Gebiet wird auf der Rückreise erneut wichtig werden."};

    std::vector<TravelSceneCandidate> candidates;
    candidates.reserve(composed.size());
    for (int index = 0; index < static_cast<int>(composed.size()); ++index)
    {
        MapGenerationResult &result = composed[static_cast<std::size_t>(index)];
        TravelSceneCandidate candidate;
        candidate.rank = index + 1;
        candidate.seed = result.quality.selectedSeed;
        const std::string routeName = !recipe.routeName.empty()
                                          ? recipe.routeName
                                          : (route && !route->name.empty()
                                                 ? route->name
                                                 : (german ? "Reise" : "Journey"));
        candidate.title = routeName + " – " + travelPurposeName(recipe.purpose, german) + " " +
                          std::to_string(index + 1);
        candidate.summary = travelSummary(recipe.purpose, german);
        candidate.discovery = german ? pick(discoveriesDe, candidate.seed, index + 1)
                                     : pick(discoveriesEn, candidate.seed, index + 1);
        candidate.complication = recipe.purpose == TravelScenePurpose::Quiet
                                     ? (german
                                            ? "Keine erzwungene Komplikation; dieser Ort bietet Raum für Charaktermomente."
                                            : "No forced complication; use this space for character moments.")
                                     : (german ? pick(complicationsDe, candidate.seed, index + 11)
                                               : pick(complicationsEn, candidate.seed, index + 11));
        candidate.gmSecret = german ? pick(secretsDe, candidate.seed, index + 23)
                                    : pick(secretsEn, candidate.seed, index + 23);
        candidate.localMap = std::move(result.map);
        candidate.localMap.name = candidate.title;
        candidate.localMap.description = candidate.summary;
        candidate.quality = result.quality;
        candidate.canPrepareEncounter = recipe.combatReady || recipe.purpose == TravelScenePurpose::Danger ||
                                        recipe.purpose == TravelScenePurpose::Obstacle;
        candidates.push_back(std::move(candidate));
    }
    if (cancellationRequested && cancellationRequested())
        return {};
    return candidates;
}

std::vector<TravelSceneCandidate> generateStandalone(
    const TravelSceneRecipe &recipe,
    int candidateCount,
    const MapGenerationCancellationCallback &cancellationRequested,
    const MapGenerationProgressCallback &progressCallback)
{
    // A deterministic internal context supplies seed identity only; it is never
    // persisted as a campaign parent or exposed as a missing atlas reference.
    MapModel context;
    context.metadata.id = "standalone_travel_scene";
    context.atlasLevel = MapLevel::Region;
    TravelSceneRecipe standalone = recipe;
    standalone.sourceMapId.clear();
    standalone.routeId.clear();
    auto candidates = generate(context, nullptr, standalone, candidateCount,
                               cancellationRequested, progressCallback);
    for (auto &candidate : candidates)
    {
        candidate.localMap.parentMapId.clear();
        candidate.localMap.atlasPlacement = {};
        candidate.localMap.atlasStatus = MapAtlasStatus::Standalone;
    }
    return candidates;
}
}
