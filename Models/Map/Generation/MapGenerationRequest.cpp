#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
#include "Models/Map/MapCaveCompositionV12.hpp"
#include "Models/Map/MapDungeonCompositionV12.hpp"
#include "Models/Map/MapTileSemantics.hpp"
#include "Models/Map/MapMountainPassRouting.hpp"
#include "Models/Map/MapRoadClearance.hpp"
#include "Models/Map/MapRegionComposition.hpp"
#include "Models/Map/MapWorldLandform.hpp"

MapGenerationOptions mapGenerationOptionsFromCreationRequest(const MapCreationRequest &request)
{
    MapGenerationOptions options;
    if (request.generatorVersion >= 11)
    {
        options.landform = request.landform;
        options.waterLevel = request.waterLevel;
        options.mountainDensity = request.mountainDensity;
        options.forestDensity = request.forestDensity;
        options.settlementDensity = request.settlementDensity;
        options.routeDensity = request.routeDensity;
        options.tacticalDensity = request.tacticalDensity;
    }
    options.generatorVersion = request.generatorVersion;
    options.recipeVersion = request.recipeVersion;
    options.styleId = request.styleId;
    options.styleVersion = request.styleVersion;
    options.styleSnapshotId = request.styleSnapshotId;
    options.creationIntent = request.intent;
    options.sizePreset = request.sizePreset;
    options.scale = mapLevelForCreationIntent(request.intent);
    options.scope = mapLegacyScopeForCreationIntent(request.intent);
    const MapDimensions dimensions = request.sizePreset == MapSizePreset::Custom
                                         ? request.dimensions
                                         : mapDimensionsForPreset(request.intent, request.sizePreset);
    options.width = std::max(4, dimensions.width);
    options.height = std::max(4, dimensions.height);
    options.seed = request.seed;
    options.quality = request.quality;
    options.qualityAttempts = request.quality == MapGenerationQualityLevel::Draft
                                  ? 12
                                  : (request.quality == MapGenerationQualityLevel::Polished ? 48 : 24);
    options.contentLanguage = request.contentLanguage;
    options.nameCulture = request.nameCulture;
    options.allowAdditionalNations = request.allowAdditionalNations;
    options.facts = request.facts;
    options.constraints = request.constraints;
    switch (request.intent)
    {
    case MapCreationIntent::WorldOverview:
    case MapCreationIntent::RegionExploration:
        options.purpose = MapGenerationPurpose::Region;
        break;
    case MapCreationIntent::Settlement:
        options.purpose = MapGenerationPurpose::SettlementEnvirons;
        break;
    case MapCreationIntent::TravelScene:
        options.purpose = MapGenerationPurpose::TravelArea;
        break;
    default:
        options.purpose = MapGenerationPurpose::Wilderness;
        break;
    }
    for (const MapGenerationFact &fact : request.facts)
    {
        const std::string value = !fact.label.empty() ? fact.label : fact.value;
        if (fact.type == MapGenerationFactType::ExcludedElement && !value.empty())
            options.excludedElements.push_back(value);
        else if (fact.type == MapGenerationFactType::IncludedElement &&
                 fact.strength == MapGenerationFactStrength::Must && !value.empty())
            options.requiredElements.push_back(value);
        if (fact.type == MapGenerationFactType::Nation && !value.empty())
            options.nationNames.push_back(value);
    }
    return options;
}

bool mapGenIsAtlasBoundaryConstraint(const MapGenerationConstraint &constraint)
{
    return constraint.id.rfind("atlas-edge::", 0) == 0;
}

void mapGenUpsertPreservedFeature(MapModel &map, const MapGeneratedFeature &preserved)
{
    auto &features = map.generation.graph.features;
    auto existing = std::find_if(features.begin(), features.end(),
                                 [&](const MapGeneratedFeature &feature) { return feature.id == preserved.id; });
    if (existing == features.end())
        features.push_back(preserved);
    else
    {
        *existing = preserved;
        features.erase(std::remove_if(std::next(existing), features.end(),
                                      [&](const MapGeneratedFeature &feature) { return feature.id == preserved.id; }),
                       features.end());
    }

    if (preserved.locked &&
        std::find(map.generation.recipe.lockedFeatureIds.begin(), map.generation.recipe.lockedFeatureIds.end(),
                  preserved.id) == map.generation.recipe.lockedFeatureIds.end())
        map.generation.recipe.lockedFeatureIds.push_back(preserved.id);
}

void mapGenProjectPreservedFeaturesToCells(MapModel &map, const std::vector<MapGeneratedFeature> &preservedFeatures)
{
    // Water first, then traversable ways, then point locations. This keeps a
    // road/river crossing a bridge while a settlement endpoint remains a
    // settlement with the route semantics layered onto its cell.
    for (const MapGeneratedFeature &feature : preservedFeatures)
        if (feature.kind == MapGeneratedFeatureKind::River)
            projectGeneratedWayFeatureToCells(map, feature, false);
    for (const MapGeneratedFeature &feature : preservedFeatures)
        if (feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail)
            projectGeneratedWayFeatureToCells(map, feature, false);
    for (const MapGeneratedFeature &feature : preservedFeatures)
    {
        if (feature.points.empty() ||
            (feature.kind != MapGeneratedFeatureKind::Settlement && feature.kind != MapGeneratedFeatureKind::Landmark))
            continue;
        const int x = std::clamp(static_cast<int>(std::floor(feature.points.front().x)), 0, map.width - 1);
        const int y = std::clamp(static_cast<int>(std::floor(feature.points.front().y)), 0, map.height - 1);
        MapCellModel *cell = mapCellAt(map, x, y);
        const MapWayKind inheritedWay = cell ? cell->wayKind : MapWayKind::None;
        mapGenSetCell(map, x, y,
                      feature.kind == MapGeneratedFeatureKind::Settlement ? MapTileKind::Settlement
                                                                          : MapTileKind::Landmark);
        if (MapCellModel *projected = mapCellAt(map, x, y))
            projected->wayKind = inheritedWay;
    }
}

void mapGenInstallPreservedFeatures(MapModel &map, const std::vector<MapGeneratedFeature> &preservedFeatures)
{
    for (const MapGeneratedFeature &preserved : preservedFeatures)
        if (!preserved.id.empty())
            mapGenUpsertPreservedFeature(map, preserved);
    mapGenProjectPreservedFeaturesToCells(map, preservedFeatures);
}

void mapGenInstallPreservedFeatures(MapModel &map, const MapGenerationOptions &options)
{
    mapGenInstallPreservedFeatures(map, options.preservedFeatures);
}

void mapGenRestorePreservedFeatures(MapModel &map, const MapGenerationOptions &options)
{
    // Re-upsert after all generators and repair passes. This is a safety net;
    // the same records are installed before planning so generated features can
    // actually route around/connect to them.
    mapGenInstallPreservedFeatures(map, options.preservedFeatures);
}

std::string mapGenNormalizedNameCulture(std::string culture)
{
    std::transform(culture.begin(), culture.end(), culture.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    for (char &character : culture)
        if (character == '-' || character == ' ')
            character = '_';
    if (culture == "de" || culture == "german" || culture == "germanic" || culture == "germanic_central_europe" ||
        culture == "germanic_central_european")
        return "germanic_central_european";
    if (culture == "nordic" || culture == "norse" || culture == "scandinavian")
        return "nordic";
    if (culture == "romance" || culture == "romanic" || culture == "romanisch")
        return "romance";
    return "neutral_fantasy";
}

std::string mapGenV4SettlementName(std::uint32_t seed, int index, const std::string &nameCulture)
{
    // Proper names stay stable across locales and are more useful at the table
    // than persisted UI placeholders such as "Settlement 3". The coprime step
    // avoids repeats for every settlement count currently supported by recipes.
    static constexpr std::array<const char *, 32> neutralNames = {
        "Alderfall",   "Ambercross", "Ashenford",  "Bellmere", "Brackenholt", "Caerwyn",    "Dawnmere",   "Dunmar",
        "Eldergate",   "Emberwick",  "Fairharbor", "Fenwatch", "Greyhaven",   "Highwater",  "Ironvale",   "Kestrelford",
        "Lindenreach", "Moonbridge", "Northmere",  "Oakrest",  "Redwillow",   "Silverfen",  "Stonehaven", "Thornwall",
        "Valebrook",   "Westermere", "Willowgate", "Windrest", "Wolfden",     "Wyvernford", "Yarrowmere", "Zephyrgate"};
    static constexpr std::array<const char *, 32> germanicNames = {
        "Auenbruck",   "Bergheim",    "Dornfurt",   "Eichenau",    "Falkenried",   "Felsenruh",   "GrÃ¼nhain",
        "Hagenwacht",  "Hohenfels",   "Kaltenried", "KirchbrÃ¼ck", "Lichtenau",    "Lindenfels",  "Moosbach",
        "Morgenfurt",  "Nebelheim",   "Rabenstein", "Rosenhain",   "Schattenried", "Silberquell", "Sonnenberg",
        "SteinbrÃ¼ck", "Tannengrund", "Tiefenau",   "Waldkirch",   "Weidenfurt",   "Wiesenfels",  "Winterbach",
        "Wolfshagen",  "Zinnenberg",  "Abendroth",  "Quellenheim"};
    static constexpr std::array<const char *, 32> nordicNames = {
        "Aldvik",    "Bergdal",  "Bjorndal",  "Eirholm",   "Frosthavn",  "Glimfjord", "Grimnes",  "Hrafnvik",
        "Isenfell",  "Jarnborg", "Jorund",    "Kaldnes",   "Kveldhavn",  "Ljosdal",   "Myrkholm", "Nordgard",
        "Ravnfjord", "Runedal",  "Skarnheim", "Skjoldvik", "Solstrand",  "Stenholm",  "Stormdal", "Svanvik",
        "Thorshavn", "Ulvnes",   "Vardholm",  "Vindfjord", "Vintergard", "Yngdal",    "Eldervik", "Havnsund"};
    static constexpr std::array<const char *, 32> romanceNames = {
        "Altavela",  "Bellacosta", "Casteloro", "Doramonte", "Estrella",  "Fonteluce",  "Granvero",    "Lunacorte",
        "Maravento", "Montesera",  "Novariva",  "Olivara",   "Portodoro", "Roccalba",   "Santera",     "Valdoro",
        "Villaluna", "Aurelia",    "Belmonte",  "Corvella",  "Fiorenza",  "Lagoverde",  "Miracosta",   "Pietralba",
        "Ravenna",   "Solavalle",  "Terranova", "Ventomare", "Veraluna",  "Villaclara", "Castelverde", "Riomoro"};
    const std::string culture = mapGenNormalizedNameCulture(nameCulture);
    if (culture == "germanic_central_european")
        return mapGenPickCultureName(germanicNames, seed, index, 0x19b4a761u, 9u);
    if (culture == "nordic")
        return mapGenPickCultureName(nordicNames, seed, index, 0xa5c31d27u, 9u);
    if (culture == "romance")
        return mapGenPickCultureName(romanceNames, seed, index, 0x73e2b149u, 9u);
    return mapGenPickCultureName(neutralNames, seed, index, 0x6d2b79f5u, 9u);
}

std::string mapGenV4LandmarkName(std::uint32_t seed, int index, const std::string &nameCulture)
{
    static constexpr std::array<const char *, 24> neutralNames = {
        "Asterion", "Belisar", "Caelith", "Dornath", "Eryndor", "Faelun", "Galdris", "Halverin",
        "Ilyra",    "Jorath",  "Kaelor",  "Lumea",   "Myrden",  "Norath", "Orelis",  "Pyranor",
        "Quelis",   "Rhyven",  "Selanor", "Theryn",  "Uldras",  "Vaelor", "Wyrdan",  "Zephra"};
    static constexpr std::array<const char *, 24> germanicNames = {
        "Adlerklamm", "Alte Warte",   "BÃ¤renstein",     "Drachenzahn",  "Eibenhain",       "Falkentor",
        "Graufels",   "Heidenquelle", "Irrlichtmoor",    "KÃ¶nigsgrat",  "Mondstein",       "Nebelpass",
        "Rabenwacht", "Riesenruh",    "Schwarze Quelle", "Sonnenklippe", "Steinerne Krone", "Totenhain",
        "Wachtfels",  "Wolfsgrund",   "ZwillingshÃ¶he",  "Eisental",     "Glutkamm",        "Silbergrat"};
    static constexpr std::array<const char *, 24> nordicNames = {
        "Askr Crag",   "Bjorn Stone", "Eir Spring",  "Frost Gate",  "Gull Peak",    "Hrafn Watch",
        "Icefjord",    "Jarn Crown",  "Kveld Moor",  "Ljos Beacon", "Myrk Pass",    "Nord Cairn",
        "Ravn Cliff",  "Rune Stone",  "Skadi Ridge", "Skarn Gate",  "Storm Watch",  "Svan Spring",
        "Thorn Fjell", "Ulf Hollow",  "Vard Peak",   "Vind Arch",   "Winter Cairn", "Ymir Step"};
    static constexpr std::array<const char *, 24> romanceNames = {
        "Arco Antico",    "Bosco d'Oro", "Cima Rossa",   "Fontechiara",    "Grotta Luna",   "Isola Verde",
        "Monte Alba",     "Passo Nero",  "Pietra Reale", "Porta del Sole", "Rocca Vecchia", "Valle Ombra",
        "Aquila Alta",    "Cascata Blu", "Colle Dorato", "Faro Antico",    "Lago Sereno",   "Ponte Rotto",
        "Santuario Alto", "Torre Vento", "Via Perduta",  "Bosco Luna",     "Riva Bianca",   "Monte Ferro"};
    const std::string culture = mapGenNormalizedNameCulture(nameCulture);
    if (culture == "germanic_central_european")
        return mapGenPickCultureName(germanicNames, seed, index, 0xc13fa9a9u, 5u);
    if (culture == "nordic")
        return mapGenPickCultureName(nordicNames, seed, index, 0x91e10da5u, 5u);
    if (culture == "romance")
        return mapGenPickCultureName(romanceNames, seed, index, 0x7f4a7c15u, 5u);
    return mapGenPickCultureName(neutralNames, seed, index, 0x9e3779b9u, 5u);
}

bool mapGenV4TextRequestsSharedGreatRiver(const std::vector<std::string> &knownFacts,
                                          const std::vector<std::string> &requiredElements)
{
    const auto matches = [](std::string fact) {
        for (char &character : fact)
        {
            const unsigned char byte = static_cast<unsigned char>(character);
            if (byte < 128)
                character = static_cast<char>(std::tolower(byte));
        }
        const bool river = fact.find("river") != std::string::npos || fact.find("fluss") != std::string::npos;
        const bool shared = fact.find("share") != std::string::npos || fact.find("teilen") != std::string::npos;
        const bool great = fact.find("great") != std::string::npos || fact.find("large") != std::string::npos ||
                           fact.find("gro\xC3\x9F"
                                     "en") != std::string::npos ||
                           fact.find("gro\xC3\x9F"
                                     "er") != std::string::npos ||
                           fact.find("groÃŸen") != std::string::npos || fact.find("grossen") != std::string::npos ||
                           fact.find("groÃŸer") != std::string::npos || fact.find("grosser") != std::string::npos;
        return river && shared && great;
    };
    return std::any_of(knownFacts.begin(), knownFacts.end(), matches) ||
           std::any_of(requiredElements.begin(), requiredElements.end(), matches);
}

bool mapGenV4RequestsSharedGreatRiver(const MapGenerationOptions &options)
{
    return mapGenV4TextRequestsSharedGreatRiver(options.knownFacts, options.requiredElements);
}

bool mapGenV4RequestsSharedGreatRiver(const MapModel &map)
{
    return mapGenV4TextRequestsSharedGreatRiver(map.generation.recipe.knownFacts,
                                                map.generation.recipe.requiredElements);
}
