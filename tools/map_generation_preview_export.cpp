#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapWorldBrief.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/TravelSceneGenerator.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorPreviewContract.hpp"

#include <algorithm>
#include <cstdint>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
enum class PreviewPreset
{
    Balanced,
    City,
    Forest,
    MountainArea,
    Travel,
    Tactical,
    Dungeon,
    Cave
};

struct ExportRequest
{
    std::filesystem::path outputPath;
    std::filesystem::path pngPath;
    int pixelsPerCell = 32;
    PreviewPreset preset = PreviewPreset::Balanced;
    TacticalMapTemplate tacticalTemplate = TacticalMapTemplate::OpenGround;
    int travelSchema = 2;
    int travelPurpose = -1;
    std::string travelBiome;
    int tacticalObjectives = 1;
    int tacticalFootprint = 2;
    int explicitTacticalCover = -1;
    std::vector<std::string> tacticalRequired;
    MapGenerationOptions options;
    bool explicitSeed = false;
    bool explicitContentLanguage = false;
    bool qaSeedBasis = false;
    std::uint32_t qaSeedBase = 700000u;
    int qaGroupIndex = 0;
    int qaLocalIndex = 0;
    bool showGrid = false;
    bool showPolitical = false;
    // Runtime/travel routes are presentation overlays, not generated roads.
    // Normal design previews hide them unless the caller opts in explicitly.
    bool showRoutes = false;
    bool playerSafe = false;
    bool playerShowcase = false;
    // Candidate previews mirror the editor design surface: fog is absent
    // unless a caller explicitly asks to exercise the published/player path.
    bool applyFog = false;
    std::string locale = "en";
    std::string worldNote;
    bool explicitRequestedSettlements = false;
    bool explicitRequestedLandmarks = false;
};

struct ProductionRoleEvidence
{
    std::string role;
    std::string semanticRole;
    int semanticOccurrences = 0;
    int unresolvedOccurrences = 0;
    std::set<std::string> resolvedAssetIds;
};

std::string jsonEscape(const std::string &value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char character : value)
    {
        switch (character)
        {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    return escaped;
}

std::string lowerAscii(std::string value)
{
    for (char &character : value)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return value;
}

std::string parseRenderLocale(std::string value)
{
    value = lowerAscii(std::move(value));
    std::replace(value.begin(), value.end(), '_', '-');
    if (value == "de" || value.rfind("de-", 0) == 0)
        return "de";
    if (value == "en" || value.rfind("en-", 0) == 0)
        return "en";
    throw std::runtime_error("--locale must be de, de-DE, en, or en-US");
}

void hashBytes(std::uint64_t &hash, const void *data, std::size_t size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
}

template <typename T>
void hashValue(std::uint64_t &hash, const T &value)
{
    hashBytes(hash, &value, sizeof(value));
}

void hashString(std::uint64_t &hash, const std::string &value)
{
    hashBytes(hash, value.data(), value.size());
    const unsigned char separator = 0xff;
    hashBytes(hash, &separator, 1);
}

std::uint64_t rasterHash(const MapModel &map)
{
    std::uint64_t hash = 1469598103934665603ull;
    hashValue(hash, map.width);
    hashValue(hash, map.height);
    hashValue(hash, map.scope);
    hashValue(hash, map.atlasLevel);
    for (const MapCellModel &cell : map.cells)
    {
        hashValue(hash, cell.kind);
        hashValue(hash, cell.wayKind);
        hashValue(hash, cell.rotationQuarterTurns);
        hashValue(hash, cell.rotationLocked);
        hashValue(hash, cell.symbolVariantMode);
        hashValue(hash, cell.movementCost);
        hashValue(hash, cell.difficultTerrain);
        hashValue(hash, cell.blocksLineOfSight);
        hashValue(hash, cell.elevation);
        hashValue(hash, cell.movementBlockedMask);
        hashValue(hash, cell.lineOfSightBlockedMask);
    }
    return hash;
}

std::uint64_t graphHash(const MapModel &map)
{
    std::uint64_t hash = 1469598103934665603ull;
    hashValue(hash, map.generation.graph.schemaVersion);
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        hashString(hash, feature.id);
        hashValue(hash, feature.kind);
        hashString(hash, feature.name);
        hashString(hash, feature.materialId);
        hashValue(hash, feature.width);
        hashValue(hash, feature.closed);
        hashValue(hash, feature.locked);
        hashValue(hash, feature.discovered);
        hashValue(hash, feature.gmOnly);
        for (const MapFeaturePoint &point : feature.points)
        {
            hashValue(hash, point.x);
            hashValue(hash, point.y);
        }
    }
    return hash;
}

std::uint64_t outputHash(const MapModel &map)
{
    std::uint64_t hash = rasterHash(map);
    const std::uint64_t graph = graphHash(map);
    hashValue(hash, graph);
    return hash;
}

std::string hashHex(std::uint64_t value)
{
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}

const char *tacticalTemplateLabel(TacticalMapTemplate mapTemplate)
{
    switch (mapTemplate)
    {
    case TacticalMapTemplate::OpenGround: return "open_ground";
    case TacticalMapTemplate::ForestRoad: return "forest_road";
    case TacticalMapTemplate::RiverCrossing: return "river_crossing";
    case TacticalMapTemplate::Ruin: return "ruin";
    case TacticalMapTemplate::VillageStreet: return "village_street";
    case TacticalMapTemplate::CitySquare: return "city_square";
    case TacticalMapTemplate::Cave: return "cave";
    case TacticalMapTemplate::StoneDungeon: return "stone_dungeon";
    }
    return "open_ground";
}

const char *qualityIssueSeverityLabel(MapGenerationIssueSeverity severity)
{
    switch (severity)
    {
    case MapGenerationIssueSeverity::Info: return "info";
    case MapGenerationIssueSeverity::Warning: return "warning";
    case MapGenerationIssueSeverity::Critical: return "critical";
    }
    return "info";
}

const char *previewTerrainSemanticRole(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Water: return "water_shallow";
    case MapTileKind::Dirt: return "earth";
    case MapTileKind::Stone:
    case MapTileKind::Room: return "dressed_stone_floor";
    case MapTileKind::Cave: return "cave_floor";
    case MapTileKind::Forest: return "forest_floor";
    case MapTileKind::Swamp: return "mud";
    case MapTileKind::Road: return "gravel";
    default: return "grass_temperate";
    }
}

bool previewGeneratedPointSymbolVisible(const MapModel &map, int x, int y, bool playerSafe)
{
    if (!playerSafe)
        return true;
    bool matched = false;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if ((feature.kind != MapGeneratedFeatureKind::Settlement &&
             feature.kind != MapGeneratedFeatureKind::Landmark &&
             feature.kind != MapGeneratedFeatureKind::BuildingGroup &&
             feature.kind != MapGeneratedFeatureKind::TacticalSetpiece) ||
            feature.points.empty())
            continue;
        const int featureX = std::clamp(static_cast<int>(std::floor(feature.points.front().x)), 0, std::max(0, map.width - 1));
        const int featureY = std::clamp(static_cast<int>(std::floor(feature.points.front().y)), 0, std::max(0, map.height - 1));
        if (featureX != x || featureY != y)
            continue;
        matched = true;
        if (!feature.gmOnly && feature.discovered)
            return true;
    }
    return !matched;
}

using ProductionRoleEvidenceMap = std::map<std::string, ProductionRoleEvidence>;

void addProductionRoleEvidence(ProductionRoleEvidenceMap &evidence, const MapModel &map,
                               const std::string &role, const std::string &semanticRole,
                               int x, int y, int salt)
{
    const std::string key = role + "\x1f" + semanticRole;
    ProductionRoleEvidence &entry = evidence[key];
    entry.role = role;
    entry.semanticRole = semanticRole;
    ++entry.semanticOccurrences;
    const MapStylePackedRuntimeAsset *asset = resolveMapStyleProductionAssetForSnapshot(
        map.generation.recipe.styleSnapshotId, map.atlasLevel, role, semanticRole, x, y, salt);
    if (!asset || !std::filesystem::is_regular_file(asset->atlasPath))
    {
        ++entry.unresolvedOccurrences;
        return;
    }
    entry.resolvedAssetIds.insert(asset->id);
}

ProductionRoleEvidenceMap collectUsedProductionRoles(const MapModel &map, bool playerSafe)
{
    ProductionRoleEvidenceMap evidence;
    // Illustrated snapshots resolve versioned vector ink in MapRenderPlan;
    // they do not request the packed production atlas assets below.
    if (map.generation.recipe.styleVersion < 3 || (gm::map_render::usesIllustratedAtlas(map) || gm::map_render::usesIllustratedLocalV6(map)))
        return evidence;

    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell)
                continue;
            const MapTileKind terrain = mapVisualGroundKind(map, x, y);
            if (!(map.scope == MapScope::Dungeon && (terrain == MapTileKind::Empty || terrain == MapTileKind::Wall)))
                addProductionRoleEvidence(evidence, map, "terrain_material",
                                          map.atlasLevel == MapLevel::World && terrain == MapTileKind::Forest
                                              ? "grass_temperate"
                                              : previewTerrainSemanticRole(terrain),
                                          x, y,
                                          static_cast<int>(terrain) * 97 + static_cast<int>(map.generation.recipe.seed & 0x7fffu));

            // World forests are macro terrain regions.  Per-cell tree/bush
            // stamps create unreadable noise at this scale and must not enter
            // the editor-equivalent production-role evidence.
            if (cell->kind == MapTileKind::Forest && map.atlasLevel != MapLevel::World)
                addProductionRoleEvidence(evidence, map, "vegetation_stamp", "forest_edge", x, y, 1501);

            const bool pointSymbolVisible = previewGeneratedPointSymbolVisible(map, x, y, playerSafe);
            if (pointSymbolVisible && (cell->kind == MapTileKind::Settlement || cell->kind == MapTileKind::Building))
            {
                const bool visibleAtScale = cell->kind != MapTileKind::Building || map.atlasLevel == MapLevel::Site || map.atlasLevel == MapLevel::Tactical;
                if (visibleAtScale)
                {
                    const bool overview = map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region;
                    addProductionRoleEvidence(evidence, map, overview ? "map_symbol" : "settlement_stamp",
                                              overview ? (cell->kind == MapTileKind::Settlement ? "city" : "town")
                                                       : mapSettlementVisualSemanticRole(map, cell->kind),
                                              x, y, static_cast<int>(cell->kind));
                }
            }
            else if (pointSymbolVisible && cell->kind == MapTileKind::Landmark && map.atlasLevel != MapLevel::World)
                addProductionRoleEvidence(evidence, map, "map_symbol", "discovery", x, y, 1701);
            else if (pointSymbolVisible && map.atlasLevel == MapLevel::Tactical &&
                     (cell->kind == MapTileKind::Cover || cell->kind == MapTileKind::Rubble || cell->kind == MapTileKind::Hazard))
                addProductionRoleEvidence(evidence, map, "tactical_prop",
                                          cell->kind == MapTileKind::Rubble ? "rubble" :
                                          (cell->kind == MapTileKind::Hazard ? "barricade" : "well_log_cover"),
                                          x, y, 1801);

            if (map.scope == MapScope::Dungeon && (cell->kind == MapTileKind::Door || cell->kind == MapTileKind::Stairs))
                addProductionRoleEvidence(evidence, map, "dungeon_stamp", "dungeon_architecture", x, y,
                                          static_cast<int>(cell->kind) * 43);
        }
    }

    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (playerSafe && (feature.gmOnly || !feature.discovered))
            continue;
        if (feature.points.empty())
            continue;
        const int x = std::clamp(static_cast<int>(std::floor(feature.points.front().x)), 0, std::max(0, map.width - 1));
        const int y = std::clamp(static_cast<int>(std::floor(feature.points.front().y)), 0, std::max(0, map.height - 1));
        // The shared renderer uses clean continuous vector bands on World,
        // Site and Tactical maps. The current painterly curve-profile images
        // are emitted only for Region, where sparse accents remain readable;
        // QA evidence must describe that actual rendering contract.
        if (map.atlasLevel == MapLevel::Region &&
            (feature.kind == MapGeneratedFeatureKind::River || feature.kind == MapGeneratedFeatureKind::Road ||
             feature.kind == MapGeneratedFeatureKind::Trail))
        {
            const std::string semantic = feature.kind == MapGeneratedFeatureKind::River
                ? (feature.width >= 1.35f ? "river_deep" : "river")
                : (feature.kind == MapGeneratedFeatureKind::Trail
                       ? "path"
                       : (feature.materialId.find("stone") != std::string::npos ? "road_stone" : "road_dirt"));
            addProductionRoleEvidence(evidence, map, "curve_profile", semantic, x, y, 1200);
        }
        else if (feature.kind == MapGeneratedFeatureKind::Bridge && feature.points.size() >= 2)
            addProductionRoleEvidence(evidence, map, "bridge_stamp",
                                      feature.materialId.find("stone") != std::string::npos ? "bridge_stone_rough" : "bridge_wood_light",
                                      x, y, 1901);
    }
    return evidence;
}

struct WorldNoteQaResult
{
    bool applicable = false;
    MapWorldBrief brief;
    bool namedNationsPresent = false;
    bool namedCapitalPresent = false;
    bool noGenericFeatureNames = false;
    bool roadNetworkPresent = false;
    bool riverNetworkPresent = false;
    bool sharedGreatRiverValid = false;
    bool settlementsOnLand = false;
    bool roadWaterCrossingsValid = false;
    bool worldScaleNoTacticalVegetationRoles = false;
    bool worldScaleNoDecorativeLandmarkAssets = false;
    bool worldScaleNoCloseRangeTerrainRoles = false;
    bool worldScaleNoCurveProfileStamps = false;
    int roadFeatureCount = 0;
    int riverFeatureCount = 0;
    int roadCellCount = 0;
    std::vector<std::string> territoryNames;
    std::vector<std::string> genericFeatureNames;

    bool passed() const
    {
        return applicable && namedNationsPresent && namedCapitalPresent && noGenericFeatureNames &&
               roadNetworkPresent && riverNetworkPresent && sharedGreatRiverValid &&
               settlementsOnLand && roadWaterCrossingsValid && worldScaleNoTacticalVegetationRoles &&
               worldScaleNoDecorativeLandmarkAssets && worldScaleNoCloseRangeTerrainRoles &&
               worldScaleNoCurveProfileStamps;
    }
};

WorldNoteQaResult analyzeWorldNoteQa(const ExportRequest &request, const MapGenerationResult &result,
                                     const ProductionRoleEvidenceMap &usedRoles)
{
    WorldNoteQaResult qa;
    qa.applicable = !request.worldNote.empty();
    qa.worldScaleNoTacticalVegetationRoles = result.map.atlasLevel == MapLevel::World;
    qa.worldScaleNoDecorativeLandmarkAssets = result.map.atlasLevel == MapLevel::World;
    qa.worldScaleNoCloseRangeTerrainRoles = result.map.atlasLevel == MapLevel::World;
    qa.worldScaleNoCurveProfileStamps = result.map.atlasLevel == MapLevel::World;
    for (const auto &pair : usedRoles)
    {
        const ProductionRoleEvidence &role = pair.second;
        if (role.role == "tactical_prop" || role.role == "vegetation_stamp")
            qa.worldScaleNoTacticalVegetationRoles = false;
        if (role.role == "map_symbol" && role.semanticRole == "discovery")
            qa.worldScaleNoDecorativeLandmarkAssets = false;
        if (role.role == "terrain_material" && role.semanticRole == "forest_floor")
            qa.worldScaleNoCloseRangeTerrainRoles = false;
        if (role.role == "curve_profile")
            qa.worldScaleNoCurveProfileStamps = false;
    }
    if (!qa.applicable)
        return qa;
    qa.brief = MapWorldBriefInterpreter::interpret(request.worldNote);
    qa.settlementsOnLand = true;
    qa.roadWaterCrossingsValid = result.quality.bridgeValidityPercent == 100;

    std::set<std::string> territories;
    bool riverGeometryContinuous = true;
    for (const MapGeneratedFeature &feature : result.map.generation.graph.features)
    {
        if (feature.kind == MapGeneratedFeatureKind::TerrainRegion && feature.materialId.find("territory.") != std::string::npos)
            territories.insert(feature.name);
        if (feature.kind == MapGeneratedFeatureKind::Settlement && feature.name == qa.brief.capitalName && feature.locked)
            qa.namedCapitalPresent = true;
        if (feature.kind == MapGeneratedFeatureKind::Settlement && !feature.points.empty())
        {
            const int settlementX = std::clamp(static_cast<int>(std::floor(feature.points.front().x)), 0, std::max(0, result.map.width - 1));
            const int settlementY = std::clamp(static_cast<int>(std::floor(feature.points.front().y)), 0, std::max(0, result.map.height - 1));
            const MapCellModel *settlementCell = mapCellAt(result.map, settlementX, settlementY);
            qa.settlementsOnLand = qa.settlementsOnLand && settlementCell &&
                                   settlementCell->kind != MapTileKind::Water &&
                                   settlementCell->kind != MapTileKind::Bridge &&
                                   settlementCell->wayKind != MapWayKind::River;
        }
        if ((feature.kind == MapGeneratedFeatureKind::Settlement && feature.name.rfind("Settlement ", 0) == 0) ||
            (feature.kind == MapGeneratedFeatureKind::Landmark && feature.name.rfind("Landmark ", 0) == 0))
            qa.genericFeatureNames.push_back(feature.name);
        if (feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail)
            ++qa.roadFeatureCount;
        if (feature.kind == MapGeneratedFeatureKind::River)
        {
            ++qa.riverFeatureCount;
            riverGeometryContinuous = riverGeometryContinuous && feature.points.size() >= 2;
            for (std::size_t index = 1; index < feature.points.size(); ++index)
            {
                const float dx = feature.points[index].x - feature.points[index - 1].x;
                const float dy = feature.points[index].y - feature.points[index - 1].y;
                riverGeometryContinuous = riverGeometryContinuous && std::sqrt(dx * dx + dy * dy) <= 3.2f;
            }
        }
    }
    qa.territoryNames.assign(territories.begin(), territories.end());
    qa.namedNationsPresent = std::all_of(qa.brief.nationNames.begin(), qa.brief.nationNames.end(), [&](const std::string &name) {
        return territories.find(name) != territories.end();
    });
    if (qa.brief.capitalName.empty())
        qa.namedCapitalPresent = true;
    qa.noGenericFeatureNames = qa.genericFeatureNames.empty();
    for (const MapCellModel &cell : result.map.cells)
        qa.roadCellCount += cell.wayKind == MapWayKind::Trail || cell.wayKind == MapWayKind::DirtRoad || cell.wayKind == MapWayKind::StoneRoad ? 1 : 0;
    // A connected minimum road network for N settlements contains N-1 road
    // features. The former hard-coded three-feature threshold therefore
    // reported every valid three-settlement spanning tree as failed while a
    // two-cell stub could still receive an optimistic generator score. Keep
    // preview QA on the same scale-aware gate used by the V5 generator.
    qa.roadNetworkPresent = result.map.generation.recipe.generatorVersion >= 5 &&
                                    result.map.atlasLevel == MapLevel::World
                                ? mapGenV5WorldRoadNetworkPercent(result.map) >= 80
                                : qa.roadFeatureCount >= 3 &&
                                      qa.roadCellCount >= std::max(12, result.map.width / 3) &&
                                      result.quality.wayConnectivityPercent >= 80 &&
                                      result.quality.roadEfficiencyPercent >= 45;
    qa.riverNetworkPresent = qa.riverFeatureCount > 0 && riverGeometryContinuous && result.quality.hydrologyPercent >= 80;
    qa.sharedGreatRiverValid = !qa.brief.requiresSharedGreatRiver || mapGenV4SharedGreatRiverIsValid(result.map);

    return qa;
}

std::map<std::string, std::set<std::string>> collectResolvableProductionRoles(const MapModel &map)
{
    std::map<std::string, std::set<std::string>> roles;
    for (const MapStylePackedRuntimeAsset &asset : mapStylePackedRuntimeAssets())
    {
        if (asset.role.empty() || asset.status != "accepted" || !mapStyleSnapshotAcceptsAsset(map.generation.recipe.styleSnapshotId, asset) ||
            !mapStyleAssetSupportsScale(asset, map.atlasLevel) || asset.minimumZoom > 1.0f || asset.maximumZoom < 1.0f ||
            !std::filesystem::is_regular_file(asset.atlasPath))
            continue;
        roles[asset.role + "\x1f" + asset.semanticRole].insert(asset.id);
    }
    return roles;
}

MapLandscapeProfile parseLandscape(const std::string &value)
{
    const std::string normalized = lowerAscii(value);
    if (normalized == "forest" || normalized == "forest_frontier")
        return MapLandscapeProfile::ForestFrontier;
    if (normalized == "highlands")
        return MapLandscapeProfile::Highlands;
    if (normalized == "mountain_pass" || normalized == "pass")
        return MapLandscapeProfile::MountainPass;
    if (normalized == "lake_country" || normalized == "lakes")
        return MapLandscapeProfile::LakeCountry;
    return MapLandscapeProfile::RiverValley;
}

PreviewPreset parsePreset(const std::string &value)
{
    const std::string normalized = lowerAscii(value);
    if (normalized == "balanced")
        return PreviewPreset::Balanced;
    if (normalized == "city")
        return PreviewPreset::City;
    if (normalized == "forest")
        return PreviewPreset::Forest;
    if (normalized == "mountain" || normalized == "mountainarea" || normalized == "mountain_area")
        return PreviewPreset::MountainArea;
    if (normalized == "travel" || normalized == "journey")
        return PreviewPreset::Travel;
    if (normalized == "tactical" || normalized == "battle" || normalized == "battlemap")
        return PreviewPreset::Tactical;
    if (normalized == "dungeon")
        return PreviewPreset::Dungeon;
    if (normalized == "cave")
        return PreviewPreset::Cave;
    return PreviewPreset::Balanced;
}

const char *presetLabel(PreviewPreset preset)
{
    switch (preset)
    {
    case PreviewPreset::Balanced:
        return "Balanced";
    case PreviewPreset::City:
        return "City";
    case PreviewPreset::Forest:
        return "Forest";
    case PreviewPreset::MountainArea:
        return "Mountain Area";
    case PreviewPreset::Travel:
        return "Travel";
    case PreviewPreset::Tactical:
        return "Tactical";
    case PreviewPreset::Dungeon:
        return "Dungeon";
    case PreviewPreset::Cave:
        return "Cave";
    }
    return "Balanced";
}

MapScope parseScope(const std::string &value)
{
    const std::string normalized = lowerAscii(value);
    if (normalized == "local")
        return MapScope::Local;
    if (normalized == "dungeon")
        return MapScope::Dungeon;
    return MapScope::World;
}

MapMountainPattern parseMountainPattern(const std::string &value)
{
    const std::string normalized = lowerAscii(value);
    if (normalized == "ranges")
        return MapMountainPattern::Ranges;
    if (normalized == "individual" || normalized == "individualpeaks" || normalized == "individual_peaks")
        return MapMountainPattern::IndividualPeaks;
    return MapMountainPattern::Mixed;
}

MapRoadNetworkMode parseRoadNetwork(const std::string &value)
{
    const std::string normalized = lowerAscii(value);
    if (normalized == "none")
        return MapRoadNetworkMode::None;
    if (normalized == "sparse")
        return MapRoadNetworkMode::Sparse;
    if (normalized == "grid")
        return MapRoadNetworkMode::Grid;
    return MapRoadNetworkMode::Connected;
}

TacticalMapTemplate parseTacticalTemplate(const std::string &value)
{
    const std::string normalized = lowerAscii(value);
    if (normalized == "forest" || normalized == "forest_road") return TacticalMapTemplate::ForestRoad;
    if (normalized == "river" || normalized == "river_crossing") return TacticalMapTemplate::RiverCrossing;
    if (normalized == "ruin" || normalized == "ruins") return TacticalMapTemplate::Ruin;
    if (normalized == "village" || normalized == "village_street") return TacticalMapTemplate::VillageStreet;
    if (normalized == "city" || normalized == "city_square") return TacticalMapTemplate::CitySquare;
    if (normalized == "cave") return TacticalMapTemplate::Cave;
    if (normalized == "dungeon" || normalized == "stone_dungeon") return TacticalMapTemplate::StoneDungeon;
    return TacticalMapTemplate::OpenGround;
}

int parseIntArg(const char *value, const std::string &name)
{
    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        throw std::runtime_error("Invalid integer for " + name + ": " + value);
    }
}

std::uint32_t parseSeedArg(const char *value, const std::string &name)
{
    try {
        const std::string input(value);std::size_t used=0;
        if(input.empty() || input.front()=='-')throw std::invalid_argument("negative seed");
        const auto parsed=std::stoull(input,&used,10);
        if(used!=input.size() || parsed>std::numeric_limits<std::uint32_t>::max())throw std::out_of_range("seed");
        return static_cast<std::uint32_t>(parsed);
    } catch (...) {
        throw std::runtime_error("Invalid unsigned seed for "+name+": "+value);
    }
}

void applyPreset(MapGenerationOptions &options, PreviewPreset preset)
{
    options.generatorVersion = std::max(4, options.generatorVersion);
    if (preset == PreviewPreset::City)
    {
        options.creationIntent = MapCreationIntent::Settlement;
        options.scope = MapScope::Local;
        options.scale = MapLevel::Site;
        options.purpose = MapGenerationPurpose::SettlementEnvirons;
        options.mountainDensity = 0;
        if(options.generatorVersion < 10)
            options.forestDensity = std::min(options.forestDensity, 10);
        if(options.generatorVersion < 10)
            options.settlementDensity = std::max(options.settlementDensity, 34);
        options.roadNetwork = MapRoadNetworkMode::Grid;
    }
    else if (preset == PreviewPreset::Forest)
    {
        options.forestDensity = std::max(options.forestDensity, 45);
        options.waterLevel = std::max(options.waterLevel, 24);
    }
    else if (preset == PreviewPreset::MountainArea)
    {
        options.mountainDensity = std::max(options.mountainDensity, 52);
        if (options.mountainPattern == MapMountainPattern::Mixed)
            options.mountainPattern = MapMountainPattern::Ranges;
    }
    else if (preset == PreviewPreset::Tactical)
    {
        options.creationIntent = MapCreationIntent::TacticalEncounter;
        options.scope = MapScope::Local;
        options.scale = MapLevel::Tactical;
        options.purpose = MapGenerationPurpose::Wilderness;
        options.requestedSettlements = 0;
        options.tacticalDensity = std::max(options.tacticalDensity, 24);
    }
    else if (preset == PreviewPreset::Travel)
    {
        options.creationIntent = MapCreationIntent::TravelScene;
        options.scope = MapScope::Local;
        options.scale = MapLevel::Region;
        options.purpose = MapGenerationPurpose::TravelArea;
        options.requestedSettlements = 0;
        options.requestedLandmarks = std::max(2, options.requestedLandmarks);
    }
    else if (preset == PreviewPreset::Dungeon)
    {
        options.creationIntent = MapCreationIntent::Dungeon;
        options.scope = MapScope::Dungeon;
        options.scale = MapLevel::Tactical;
    }
    else if (preset == PreviewPreset::Cave)
    {
        options.creationIntent = MapCreationIntent::Cave;
        options.scope = MapScope::Dungeon;
        options.scale = MapLevel::Tactical;
        options.caveNetwork = true;
        options.roadNetwork = MapRoadNetworkMode::Connected;
    }
    else if (options.scope == MapScope::World)
    {
        options.creationIntent = MapCreationIntent::WorldOverview;
        options.scale = MapLevel::World;
        options.purpose = MapGenerationPurpose::Region;
    }
}

void applyWorldNote(ExportRequest &request)
{
    if (request.worldNote.empty())
        return;

    const MapWorldBrief brief = MapWorldBriefInterpreter::interpret(request.worldNote);
    if (!brief.hasFacts())
        throw std::runtime_error("--world-note did not contain any recognized world facts");

    MapGenerationOptions &options = request.options;
    options.generatorVersion = std::max(4, options.generatorVersion);
    options.creationIntent = MapCreationIntent::WorldOverview;
    options.scope = MapScope::World;
    options.scale = MapLevel::World;
    options.purpose = MapGenerationPurpose::Region;
    options.nationNames = brief.nationNames;
    // Preserve the editor's typed-fact contract. Names alone are insufficient:
    // Must/Prefer/Auto strength, exclusions, shared-river requirements and
    // malformed-note blockers must reach the same request validator used by
    // the in-app flow.
    options.facts = brief.facts;
    mapGenApplyFactsToOptions(options);
    options.knownFacts.push_back(request.worldNote);
    for (const std::string &nationName : brief.nationNames)
        options.knownFacts.push_back("Nation: " + nationName);

    const int namedSettlementCount = static_cast<int>(brief.cityNames.size()) + (brief.capitalName.empty() ? 0 : 1);
    if (request.explicitRequestedSettlements)
        options.requestedSettlements = std::max(options.requestedSettlements, namedSettlementCount);
    else
        options.requestedSettlements = namedSettlementCount;
    if (!request.explicitRequestedLandmarks)
        options.requestedLandmarks = 0;
    options.edgeConnectionCount = std::max(options.edgeConnectionCount, 3);
    if (options.roadNetwork == MapRoadNetworkMode::None)
        options.roadNetwork = MapRoadNetworkMode::Connected;

    if (!brief.capitalName.empty())
    {
        MapGenerationConstraint capital;
        capital.id = "brief_capital";
        capital.label = brief.capitalName;
        capital.featureKind = MapGeneratedFeatureKind::Settlement;
        capital.x = static_cast<float>(options.width) * 0.52f;
        capital.y = static_cast<float>(options.height) * 0.48f;
        capital.required = true;
        capital.locked = true;
        options.constraints.push_back(std::move(capital));
    }
    for (std::size_t index = 0; index < brief.cityNames.size(); ++index)
    {
        MapGenerationConstraint city;
        city.id = "brief_city_" + std::to_string(index);
        city.label = brief.cityNames[index];
        city.featureKind = MapGeneratedFeatureKind::Settlement;
        const float fraction = static_cast<float>(index + 1) / static_cast<float>(brief.cityNames.size() + 1);
        city.x = static_cast<float>(options.width) * (0.2f + fraction * 0.6f);
        city.y = static_cast<float>(options.height) * (index % 2 == 0 ? 0.36f : 0.64f);
        city.required = true;
        city.locked = true;
        options.constraints.push_back(std::move(city));
    }
}

ExportRequest parseArgs(int argc, char **argv)
{
    ExportRequest request;
    request.outputPath = std::filesystem::current_path() / "generated_map_preview.json";
    // The feedback gallery and ad-hoc QA exporter must exercise the same
    // generator generation as the editor.  Keeping the historical V4 default
    // here made visually reviewed previews bypass the V5 world constraints
    // unless every caller remembered a hidden command-line switch.
    request.options.generatorVersion = 5;
    // The editor's normal "Balanced" flow evaluates a fixed 24 candidates.
    // A one-attempt exporter produced different availability and could reject
    // a recipe that the editor correctly offered, so keep the default contract
    // identical even for ad-hoc CLI previews.
    request.options.qualityAttempts = 24;

    for (int index = 1; index < argc; ++index)
    {
        const std::string arg = argv[index];
        auto requireValue = [&]() -> const char * {
            if (index + 1 >= argc)
                throw std::runtime_error("Missing value for " + arg);
            return argv[++index];
        };

        if (arg == "--out")
            request.outputPath = requireValue();
        else if (arg == "--png")
            request.pngPath = requireValue();
        else if (arg == "--pixels-per-cell")
            request.pixelsPerCell = parseIntArg(requireValue(), arg);
        else if (arg == "--generator-version")
            request.options.generatorVersion = parseIntArg(requireValue(), arg);
        else if (arg == "--recipe-version")
            request.options.recipeVersion = parseIntArg(requireValue(), arg);
        else if (arg == "--quality-attempts")
        {
            request.options.qualityAttempts = std::clamp(parseIntArg(requireValue(), arg), 1, 48);
            request.options.quality = request.options.qualityAttempts >= 48
                                          ? MapGenerationQualityLevel::Polished
                                          : (request.options.qualityAttempts >= 24
                                                 ? MapGenerationQualityLevel::Balanced
                                                 : MapGenerationQualityLevel::Draft);
        }
        else if (arg == "--nation" || arg == "--city")
        {
            const std::string value = requireValue();
            MapGenerationFact fact;
            fact.id = "cli_named_" + std::to_string(request.options.facts.size());
            fact.type = arg == "--nation" ? MapGenerationFactType::Nation : MapGenerationFactType::Settlement;
            fact.strength = MapGenerationFactStrength::Must;
            fact.label = value; fact.value = value; fact.confirmed = true;
            request.options.facts.push_back(fact);
            if (arg == "--nation") request.options.nationNames.push_back(value);
            else
            {
                MapGenerationConstraint city;
                city.id=fact.id;city.label=value;city.featureKind=MapGeneratedFeatureKind::Settlement;
                city.required=true;city.locked=false;
                request.options.constraints.push_back(city);
            }
        }
        else if (arg == "--landform")
        {
            const std::string value = requireValue();
            const auto landform = mapLandformFromToken(value);
            if (landform == MapLandform::Automatic && value != "automatic")
                throw std::runtime_error("Unknown landform: " + value);
            request.options.landform = landform;
        }
        else if (arg == "--landscape")
            request.options.landscape = parseLandscape(requireValue());
        else if (arg == "--requested-settlements")
        {
            request.options.requestedSettlements = parseIntArg(requireValue(), arg);
            request.explicitRequestedSettlements = true;
        }
        else if (arg == "--requested-landmarks")
        {
            request.options.requestedLandmarks = parseIntArg(requireValue(), arg);
            request.explicitRequestedLandmarks = true;
        }
        else if (arg == "--edge-connections")
            request.options.edgeConnectionCount = parseIntArg(requireValue(), arg);
        else if (arg == "--preset")
            request.preset = parsePreset(requireValue());
        else if (arg == "--scope")
            request.options.scope = parseScope(requireValue());
        else if (arg == "--travel-schema")
            request.travelSchema = parseIntArg(requireValue(), arg);
        else if (arg == "--travel-purpose")
            request.travelPurpose = parseIntArg(requireValue(), arg);
        else if (arg == "--travel-biome")
            request.travelBiome = requireValue();
        else if (arg == "--width")
            request.options.width = parseIntArg(requireValue(), arg);
        else if (arg == "--height")
            request.options.height = parseIntArg(requireValue(), arg);
        else if (arg == "--seed")
        {
            request.options.seed = parseSeedArg(requireValue(), arg);
            request.explicitSeed = true;
        }
        else if (arg == "--qa-seed-base")
        {
            request.qaSeedBase = parseSeedArg(requireValue(), arg);
            request.qaSeedBasis = true;
        }
        else if (arg == "--qa-group-index")
        {
            request.qaGroupIndex = parseIntArg(requireValue(), arg);
            request.qaSeedBasis = true;
        }
        else if (arg == "--qa-local-index")
        {
            request.qaLocalIndex = parseIntArg(requireValue(), arg);
            request.qaSeedBasis = true;
        }
        else if (arg == "--water")
            request.options.waterLevel = parseIntArg(requireValue(), arg);
        else if (arg == "--mountains")
            request.options.mountainDensity = parseIntArg(requireValue(), arg);
        else if (arg == "--forests")
            request.options.forestDensity = parseIntArg(requireValue(), arg);
        else if (arg == "--settlements")
            request.options.settlementDensity = parseIntArg(requireValue(), arg);
        else if (arg == "--routes")
            request.options.routeDensity = parseIntArg(requireValue(), arg);
        else if (arg == "--tactics")
        {
            request.options.tacticalDensity = parseIntArg(requireValue(), arg);
            request.explicitTacticalCover = request.options.tacticalDensity;
        }
        else if (arg == "--tactical-objectives")
            request.tacticalObjectives = std::clamp(parseIntArg(requireValue(), arg), 1, 3);
        else if (arg == "--tactical-footprint")
            request.tacticalFootprint = std::clamp(parseIntArg(requireValue(), arg), 1, 4);
        else if (arg == "--tactical-required")
            request.tacticalRequired.push_back(requireValue());
        else if (arg == "--mountain-pattern")
            request.options.mountainPattern = parseMountainPattern(requireValue());
        else if (arg == "--road-network")
            request.options.roadNetwork = parseRoadNetwork(requireValue());
        else if (arg == "--tactical-template")
            request.tacticalTemplate = parseTacticalTemplate(requireValue());
        else if (arg == "--style-id")
            request.options.styleId = requireValue();
        else if (arg == "--world-note")
            request.worldNote = requireValue();
        else if (arg == "--show-grid")
            request.showGrid = true;
        else if (arg == "--show-political")
            request.showPolitical = true;
        else if (arg == "--show-routes")
            request.showRoutes = true;
        else if (arg == "--player-safe")
            request.playerSafe = true;
        else if (arg == "--player-showcase")
        {
            request.playerSafe = true;
            request.playerShowcase = true;
        }
        else if (arg == "--locale")
            request.locale = parseRenderLocale(requireValue());
        else if (arg == "--content-language")
        {
            const std::string language = requireValue();
            if (language != "de" && language != "en")
                throw std::runtime_error("--content-language must be de or en");
            request.options.contentLanguage = language;
            request.explicitContentLanguage = true;
        }
        else if (arg == "--apply-fog")
            request.applyFog = true;
        else if (arg == "--no-fog")
            request.applyFog = false;
        else
            throw std::runtime_error("Unknown argument: " + arg);
    }

    if (request.qaSeedBasis)
    {
        if (request.explicitSeed)
            throw std::runtime_error("--seed cannot be combined with the QA seed-basis options");
        if (request.qaGroupIndex < 0 || request.qaGroupIndex > 4 || request.qaLocalIndex < 0)
            throw std::runtime_error("QA seed basis requires --qa-group-index 0..4 and a non-negative --qa-local-index");
        const std::uint64_t computed = static_cast<std::uint64_t>(request.qaSeedBase) +
                                       static_cast<std::uint64_t>(request.qaGroupIndex) * 1000003ull +
                                       static_cast<std::uint64_t>(request.qaLocalIndex) * 7919ull;
        if (computed > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("QA seed basis exceeds the supported 32-bit seed range");
        request.options.seed = static_cast<std::uint32_t>(computed);
    }

    if (!request.explicitContentLanguage)
        request.options.contentLanguage = request.locale;
    applyPreset(request.options, request.preset);
    applyWorldNote(request);
    return request;
}

MapRenderPlan compileRequestedRenderPlan(const MapModel &map,
                                         const ExportRequest &request)
{
    MapRenderPlanRequest planRequest =
        gm::mapui::mapCreatorPreviewRenderPlanRequest(request.locale);
    planRequest.audience = request.playerSafe ? MapRenderAudience::Player
                                              : MapRenderAudience::GameMaster;
    planRequest.applyFog = request.applyFog;
    if (request.applyFog || request.playerSafe)
        planRequest.layers |= MapRenderPlanFog;
    if (request.showGrid)
    {
        planRequest.layers |= MapRenderPlanGrid;
        planRequest.showGrid = true;
    }
    if (request.showPolitical)
    {
        planRequest.layers |= MapRenderPlanPolitical;
        planRequest.showPoliticalOverlay = true;
    }
    if (request.showRoutes)
        planRequest.layers |= MapRenderPlanRoutes;
    return compileMapRenderPlan(map, planRequest);
}

void writeJson(const std::filesystem::path &path, const ExportRequest &request,
               const MapGenerationResult &result,
               const MapRenderPlan &renderPlan)
{
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());

    std::ofstream out(path);
    if (!out)
        throw std::runtime_error("Could not open output path: " + path.string());

    const MapModel &map = result.map;
    const MapGenerationRecipe &recipe = map.generation.recipe;
    const MapGenerationQualityReport &quality = result.quality;
    const MapRenderScaleProfile &scaleProfile = mapRenderScaleProfile(map.atlasLevel);
    const ProductionRoleEvidenceMap usedRoles = collectUsedProductionRoles(map, request.playerSafe);
    const WorldNoteQaResult worldNoteQa = analyzeWorldNoteQa(request, result, usedRoles);
    const std::map<std::string, std::set<std::string>> resolvableRoles = collectResolvableProductionRoles(map);
    int unresolvedUsedRoleCount = 0;
    for (const auto &entry : usedRoles)
        unresolvedUsedRoleCount += entry.second.unresolvedOccurrences > 0 ? 1 : 0;
    const int effectivePixelsPerCell = request.pixelsPerCell;
    const char *generatorPath = request.preset == PreviewPreset::Tactical ? "TacticalMapGenerator" :
                                (request.preset == PreviewPreset::Travel ? "TravelSceneGenerator" : "generateSemanticMap");

    out << "{\n";
    out << "  \"schema_version\": 2,\n";
    out << "  \"preset\": \"" << presetLabel(request.preset) << "\",\n";
    if (request.preset == PreviewPreset::Tactical)
        out << "  \"tactical_template\": \"" << tacticalTemplateLabel(request.tacticalTemplate) << "\",\n";
    else
        out << "  \"tactical_template\": null,\n";
    out << "  \"generator_path\": \"" << generatorPath << "\",\n";
    out << "  \"generator_version\": " << recipe.generatorVersion << ",\n";
    out << "  \"recipe_version\": " << recipe.recipeVersion << ",\n";
    out << "  \"style_id\": \"" << jsonEscape(recipe.styleId) << "\",\n";
    out << "  \"style_version\": " << recipe.styleVersion << ",\n";
    out << "  \"packed_asset_diagnostics_applicable\": " << ((gm::map_render::usesIllustratedAtlas(map) || gm::map_render::usesIllustratedLocalV6(map)) ? "false" : "true") << ",\n";
    out << "  \"cartography_revision\": " << gm::map_render::kOverviewCartographyRevision << ",\n";
    out << "  \"visual_acceptance\": \"not_reviewed\",\n";
    const auto compositionRevision = map.metadata.customProperties.find("tactical.composition_revision");
    if (compositionRevision != map.metadata.customProperties.end())
        out << "  \"tactical_composition_revision\": \"" << jsonEscape(compositionRevision->second) << "\",\n";
    out << "  \"style_snapshot_id\": \"" << jsonEscape(recipe.styleSnapshotId) << "\",\n";
    out << "  \"requested_seed\": " << request.options.seed << ",\n";
    out << "  \"seed\": " << recipe.seed << ",\n";
    out << "  \"seed_basis\": {\"mode\":\"" << (request.qaSeedBasis ? "qa_formula" : "explicit_or_default") << "\"";
    if (request.qaSeedBasis)
        out << ",\"base\":" << request.qaSeedBase << ",\"group_index\":" << request.qaGroupIndex
            << ",\"local_index\":" << request.qaLocalIndex << ",\"group_stride\":1000003,\"local_stride\":7919";
    out << "},\n";
    out << "  \"scope\": " << static_cast<int>(map.scope) << ",\n";
    out << "  \"atlas_level\": \"" << mapStyleScaleKey(map.atlasLevel) << "\",\n";
    out << "  \"width\": " << map.width << ",\n";
    out << "  \"height\": " << map.height << ",\n";
    out << "  \"hashes\": {\"algorithm\":\"fnv1a64\",\"contract_version\":2,\"graph_hash\":\""
        << hashHex(graphHash(map)) << "\",\"raster_hash\":\"" << hashHex(rasterHash(map))
        << "\",\"output_hash\":\"" << hashHex(outputHash(map)) << "\"},\n";
    out << "  \"quality\": {\n";
    out << "    \"score_purpose\": \"structural_diagnostics\",\n";
    out << "    \"score\": " << quality.score << ",\n";
    out << "    \"validation_eligible\": " << (result.validation.eligible ? "true" : "false") << ",\n";
    out << "    \"candidate_floor_passed\": "
        << (mapGenQualityMeetsCandidateFloor(map, quality) ? "true" : "false") << ",\n";
    out << "    \"candidates_evaluated\": " << quality.candidatesEvaluated << ",\n";
    out << "    \"selected_candidate_index\": " << quality.selectedCandidateIndex << ",\n";
    out << "    \"selected_seed\": " << quality.selectedSeed << ",\n";
    out << "    \"critical_issue_count\": " << quality.criticalIssueCount << ",\n";
    out << "    \"critical_route_connected\": " << (quality.criticalRouteConnected ? "true" : "false") << ",\n";
    out << "    \"categories\": {"
        << "\"traversable_connectivity\":" << quality.traversableConnectivityPercent
        << ",\"way_connectivity\":" << quality.wayConnectivityPercent
        << ",\"structure_access\":" << quality.structureAccessPercent
        << ",\"coherent_terrain\":" << quality.coherentTerrainPercent
        << ",\"road_efficiency\":" << quality.roadEfficiencyPercent
        << ",\"hydrology\":" << quality.hydrologyPercent
        << ",\"bridge_validity\":" << quality.bridgeValidityPercent
        << ",\"settlement_suitability\":" << quality.settlementSuitabilityPercent
        << ",\"composition\":" << quality.compositionPercent
        << ",\"visual_hierarchy\":" << quality.visualHierarchyPercent
        << ",\"scale_correctness\":" << quality.scaleCorrectnessPercent
        << ",\"asset_coverage\":" << quality.assetCoveragePercent << "},\n";
    out << "    \"issues\": [";
    for (std::size_t index = 0; index < quality.issues.size(); ++index)
    {
        if (index > 0)
            out << ',';
        const MapGenerationIssue &issue = quality.issues[index];
        out << "{\"code\":\"" << jsonEscape(issue.code) << "\",\"severity\":\""
            << qualityIssueSeverityLabel(issue.severity) << "\",\"message\":\"" << jsonEscape(issue.message)
            << "\",\"suggested_action\":\"" << jsonEscape(issue.suggestedAction) << "\"}";
    }
    out << "]\n  },\n";
    out << "  \"world_facts\": {\n";
    out << "    \"applied\": " << (worldNoteQa.applicable ? "true" : "false") << ",\n";
    out << "    \"source_note\": ";
    if (worldNoteQa.applicable)
        out << '"' << jsonEscape(request.worldNote) << '"';
    else
        out << "null";
    out << ",\n";
    out << "    \"stated_nation_count\": " << worldNoteQa.brief.statedNationCount << ",\n";
    out << "    \"requires_shared_great_river\": "
        << (worldNoteQa.brief.requiresSharedGreatRiver ? "true" : "false") << ",\n";
    out << "    \"nation_names\": [";
    for (std::size_t index = 0; index < worldNoteQa.brief.nationNames.size(); ++index)
    {
        if (index > 0)
            out << ',';
        out << '"' << jsonEscape(worldNoteQa.brief.nationNames[index]) << '"';
    }
    out << "],\n    \"city_names\": [";
    for (std::size_t index = 0; index < worldNoteQa.brief.cityNames.size(); ++index)
    {
        if (index > 0)
            out << ',';
        out << '"' << jsonEscape(worldNoteQa.brief.cityNames[index]) << '"';
    }
    out << "],\n    \"capital_name\": ";
    if (!worldNoteQa.brief.capitalName.empty())
        out << '"' << jsonEscape(worldNoteQa.brief.capitalName) << '"';
    else
        out << "null";
    out << ",\n    \"generated_territory_names\": [";
    for (std::size_t index = 0; index < worldNoteQa.territoryNames.size(); ++index)
    {
        if (index > 0)
            out << ',';
        out << '"' << jsonEscape(worldNoteQa.territoryNames[index]) << '"';
    }
    out << "]\n  },\n";
    out << "  \"qa_checks\": {\n";
    out << "    \"applicable\": " << (worldNoteQa.applicable ? "true" : "false") << ",\n";
    out << "    \"passed\": " << (worldNoteQa.passed() ? "true" : "false") << ",\n";
    out << "    \"named_nations_present\": " << (worldNoteQa.namedNationsPresent ? "true" : "false") << ",\n";
    out << "    \"named_capital_present\": " << (worldNoteQa.namedCapitalPresent ? "true" : "false") << ",\n";
    out << "    \"no_generic_feature_names\": " << (worldNoteQa.noGenericFeatureNames ? "true" : "false") << ",\n";
    out << "    \"road_network_present\": " << (worldNoteQa.roadNetworkPresent ? "true" : "false") << ",\n";
    out << "    \"river_network_present\": " << (worldNoteQa.riverNetworkPresent ? "true" : "false") << ",\n";
    out << "    \"shared_great_river_valid\": " << (worldNoteQa.sharedGreatRiverValid ? "true" : "false") << ",\n";
    out << "    \"settlements_on_land\": " << (worldNoteQa.settlementsOnLand ? "true" : "false") << ",\n";
    out << "    \"road_water_crossings_valid\": " << (worldNoteQa.roadWaterCrossingsValid ? "true" : "false") << ",\n";
    out << "    \"world_scale_no_tactical_vegetation_roles\": "
        << (worldNoteQa.worldScaleNoTacticalVegetationRoles ? "true" : "false") << ",\n";
    out << "    \"world_scale_no_vegetation_stamp_roles\": "
        << (worldNoteQa.worldScaleNoTacticalVegetationRoles ? "true" : "false") << ",\n";
    out << "    \"world_scale_no_decorative_landmark_assets\": "
        << (worldNoteQa.worldScaleNoDecorativeLandmarkAssets ? "true" : "false") << ",\n";
    out << "    \"world_scale_no_close_range_terrain_roles\": "
        << (worldNoteQa.worldScaleNoCloseRangeTerrainRoles ? "true" : "false") << ",\n";
    out << "    \"world_scale_no_curve_profile_stamps\": "
        << (worldNoteQa.worldScaleNoCurveProfileStamps ? "true" : "false") << ",\n";
    out << "    \"road_feature_count\": " << worldNoteQa.roadFeatureCount << ",\n";
    out << "    \"river_feature_count\": " << worldNoteQa.riverFeatureCount << ",\n";
    out << "    \"road_cell_count\": " << worldNoteQa.roadCellCount << ",\n";
    out << "    \"generic_feature_names\": [";
    for (std::size_t index = 0; index < worldNoteQa.genericFeatureNames.size(); ++index)
    {
        if (index > 0)
            out << ',';
        out << '"' << jsonEscape(worldNoteQa.genericFeatureNames[index]) << '"';
    }
    out << "]\n  },\n";
    out << "  \"production_assets\": {\n";
    out << "    \"evidence_kind\": \"semantic usage plus production-registry resolution probes\",\n";
    out << "    \"snapshot_available\": " << (mapStyleProductionSnapshotAvailable(recipe.styleSnapshotId) ? "true" : "false") << ",\n";
    out << "    \"used_role_count\": " << usedRoles.size() << ",\n";
    out << "    \"unresolved_used_role_count\": " << unresolvedUsedRoleCount << ",\n";
    out << "    \"used_roles\": [";
    std::size_t roleIndex = 0;
    for (const auto &pair : usedRoles)
    {
        const ProductionRoleEvidence &role = pair.second;
        if (roleIndex++ > 0)
            out << ',';
        out << "{\"role\":\"" << jsonEscape(role.role) << "\",\"semantic_role\":\"" << jsonEscape(role.semanticRole)
            << "\",\"semantic_occurrences\":" << role.semanticOccurrences
            << ",\"unresolved_occurrences\":" << role.unresolvedOccurrences << ",\"resolved\":"
            << (role.unresolvedOccurrences == 0 && !role.resolvedAssetIds.empty() ? "true" : "false") << ",\"resolution_probe_asset_ids\":[";
        std::size_t assetIndex = 0;
        for (const std::string &assetId : role.resolvedAssetIds)
        {
            if (assetIndex++ > 0)
                out << ',';
            out << '"' << jsonEscape(assetId) << '"';
        }
        out << "]}";
    }
    out << "],\n";
    out << "    \"resolvable_roles\": [";
    roleIndex = 0;
    for (const auto &pair : resolvableRoles)
    {
        const std::size_t separator = pair.first.find('\x1f');
        const std::string role = pair.first.substr(0, separator);
        const std::string semanticRole = separator == std::string::npos ? std::string() : pair.first.substr(separator + 1);
        if (roleIndex++ > 0)
            out << ',';
        out << "{\"role\":\"" << jsonEscape(role) << "\",\"semantic_role\":\"" << jsonEscape(semanticRole)
            << "\",\"asset_count\":" << pair.second.size() << "}";
    }
    out << "]\n  },\n";
    out << "  \"render\": {\n";
    out << "    \"backend\": \"" << (request.pngPath.empty() ? "not_requested" : "MapPngExport CPU rasterizer") << "\",\n";
    out << "    \"contract\": \"Editor preview and PNG consume the same explicitly compiled immutable MapRenderPlan for audience, locale, layers, visibility allow-lists and content hash. Dear ImGui/OpenGL and CPU PNG remain separate rasterizers; pixel identity is not established.\",\n";
    out << "    \"pixel_identical_to_imgui_editor\": false,\n";
    out << "    \"compiled_plan_consumed_by_png\": "
        << (!request.pngPath.empty() ? "true" : "false") << ",\n";
    out << "    \"render_plan_content_hash\": \""
        << hashHex(renderPlan.contentHash) << "\",\n";
    out << "    \"render_plan_map_id\": \""
        << jsonEscape(renderPlan.mapId) << "\",\n";
    out << "    \"render_plan_map_revision\": " << renderPlan.mapRevision << ",\n";
    out << "    \"render_plan_style_snapshot_id\": \""
        << jsonEscape(renderPlan.styleSnapshotId) << "\",\n";
    out << "    \"render_plan_layers\": " << renderPlan.layers << ",\n";
    out << "    \"render_plan_primitive_count\": "
        << renderPlan.primitives.size() << ",\n";
    out << "    \"audience\": \""
        << (renderPlan.audience == MapRenderAudience::Player ? "player" : "gm")
        << "\",\n";
    out << "    \"source_is_synthetic_preview\": true,\n";
    out << "    \"persisted_campaign_modified\": false,\n";
    out << "    \"synthetic_preview_marked_players_visible\": "
        << (request.playerSafe ? "true" : "false") << ",\n";
    out << "    \"synthetic_public_layers_released\": "
        << (request.playerShowcase ? "true" : "false") << ",\n";
    out << "    \"requested_pixels_per_cell\": " << request.pixelsPerCell << ",\n";
    out << "    \"effective_pixels_per_cell\": " << effectivePixelsPerCell << ",\n";
    out << "    \"pixel_width\": " << map.width * effectivePixelsPerCell << ",\n";
    out << "    \"pixel_height\": " << map.height * effectivePixelsPerCell << ",\n";
    out << "    \"locale\": \"" << jsonEscape(renderPlan.locale) << "\",\n";
    out << "    \"show_grid\": " << (renderPlan.showGrid ? "true" : "false") << ",\n";
    out << "    \"show_routes\": "
        << ((renderPlan.layers & MapRenderPlanRoutes) != 0 ? "true" : "false") << ",\n";
    const std::size_t visibleRouteOverlayCount = renderPlan.routeIds.size();
    out << "    \"available_route_overlay_count\": " << map.routes.size() << ",\n";
    out << "    \"visible_route_overlay_count\": " << visibleRouteOverlayCount << ",\n";
    out << "    \"player_safe\": "
        << (renderPlan.audience == MapRenderAudience::Player ? "true" : "false") << ",\n";
    out << "    \"apply_fog\": "
        << (renderPlan.applyFog && (renderPlan.layers & MapRenderPlanFog) != 0
                ? "true" : "false") << ",\n";
    out << "    \"continuous_rendering\": " << (map.generation.continuousRendering ? "true" : "false") << ",\n";
    out << "    \"scale_profile\": {\"material_cells_per_repeat\":" << scaleProfile.materialCellsPerRepeat
        << ",\"symbol_density\":" << scaleProfile.symbolDensity
        << ",\"vegetation_cluster_distance\":" << scaleProfile.vegetationClusterDistance
        << ",\"mountain_cluster_distance\":" << scaleProfile.mountainClusterDistance
        << ",\"road_width_cells\":" << scaleProfile.roadWidthCells
        << ",\"river_width_cells\":" << scaleProfile.riverWidthCells
        << ",\"show_entity_labels\":" << (scaleProfile.showEntityLabels ? "true" : "false")
        << ",\"show_grid_by_default\":" << (scaleProfile.showGridByDefault ? "true" : "false")
        << ",\"allow_tactical_props\":" << (scaleProfile.allowTacticalProps ? "true" : "false")
        << ",\"allow_individual_buildings\":" << (scaleProfile.allowIndividualBuildings ? "true" : "false") << "}\n";
    out << "  },\n";
    out << "  \"feature_count\": " << map.generation.graph.features.size() << ",\n";
    out << "  \"cells\": [\n";
    for (int y = 0; y < map.height; ++y)
    {
        out << "    [";
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (x > 0)
                out << ", ";
            out << "{\"kind\":" << static_cast<int>(cell ? cell->kind : MapTileKind::Empty)
                << ",\"way\":" << static_cast<int>(cell ? cell->wayKind : MapWayKind::None)
                << ",\"rotation\":" << (cell ? cell->rotationQuarterTurns : 0)
                << ",\"rotation_locked\":" << ((cell && cell->rotationLocked) ? "true" : "false")
                << ",\"symbol_mode\":" << static_cast<int>(cell ? cell->symbolVariantMode : MapSymbolVariantMode::Auto)
                << ",\"difficult\":" << ((cell && cell->difficultTerrain) ? "true" : "false")
                << ",\"elevation\":" << (cell ? cell->elevation : 0)
                << "}";
        }
        out << "]";
        if (y + 1 < map.height)
            out << ",";
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"warnings\": [";
    for (size_t index = 0; index < result.warnings.size(); ++index)
    {
        if (index > 0)
            out << ", ";
        out << "\"" << jsonEscape(result.warnings[index]) << "\"";
    }
    out << "]\n";
    out << "}\n";
}

const char *batchFailureLabel(MapGenerationBatchFailureCode failure)
{
    switch (failure)
    {
    case MapGenerationBatchFailureCode::InvalidRequest: return "invalid request";
    case MapGenerationBatchFailureCode::NoEligibleCandidate: return "no eligible candidate";
    case MapGenerationBatchFailureCode::NoCandidateMetQualityFloor: return "no candidate met the quality floor";
    case MapGenerationBatchFailureCode::Cancelled: return "cancelled";
    case MapGenerationBatchFailureCode::None:
    default: return "no candidate returned";
    }
}

MapGenerationResult takeExportableCandidate(MapGenerationBatchResult batch,
                                            const char *context)
{
    if (batch.candidates.empty())
    {
        std::ostringstream message;
        message << context << " produced no exportable candidate ("
                << batchFailureLabel(batch.failure) << ", "
                << batch.attemptsEvaluated << " attempt(s))";
        for (const auto &rejected : batch.rejectedCandidates) for (const auto &issue : rejected.validation.blockers) message << " [" << issue.code << "]";
        throw std::runtime_error(message.str());
    }
    return std::move(batch.candidates.front());
}

void requireExportableCandidate(MapGenerationResult &result)
{
    result.validation = validateMapGeneration(result.map, result.quality);
    const bool shapeValid = result.map.width > 0 && result.map.height > 0 &&
                            result.map.cells.size() ==
                                static_cast<std::size_t>(result.map.width * result.map.height);
    if (!shapeValid || !result.validation.eligible ||
        result.quality.criticalIssueCount != 0 ||
        !mapGenQualityMeetsCandidateFloor(result.map, result.quality))
    {
        std::ostringstream message;
        message << "refusing to export a non-candidate map (score="
                << result.quality.score << ", validation="
                << (result.validation.eligible ? "eligible" : "blocked")
                << ", critical=" << result.quality.criticalIssueCount << ')';

        throw std::runtime_error(message.str());
    }
}
}

int main(int argc, char **argv)
{
    try
    {
        const ExportRequest request = parseArgs(argc, argv);
        MapGenerationResult result;
        if (request.preset == PreviewPreset::Tactical)
        {
            TacticalMapRecipe tactical;
            tactical.generatorVersion = request.options.generatorVersion;
            tactical.illustrationRecipeVersion = std::max(3, request.options.recipeVersion);
            tactical.mapTemplate = request.tacticalTemplate;
            tactical.width = request.options.width;
            tactical.height = request.options.height;
            tactical.seed = request.options.seed;
            tactical.coverDensity = request.explicitTacticalCover >= 0 ? request.explicitTacticalCover : request.options.tacticalDensity;
            tactical.objectiveCount = request.tacticalObjectives;
            tactical.minimumCreatureFootprint = request.tacticalFootprint;
            tactical.requiredElements = request.tacticalRequired;
            tactical.quality = request.options.quality;
            const std::vector<TacticalMapCandidate> candidates = TacticalMapGenerator::generate(tactical, nullptr, 1);
            if (candidates.empty())
                throw std::runtime_error("Tactical generator returned no candidate");
            result.map = candidates.front().map;
            result.quality = candidates.front().quality;
            result.warnings = candidates.front().readinessIssues;
        }
        else if (request.preset == PreviewPreset::Travel)
        {
            MapGenerationOptions sourceOptions = request.options;
            sourceOptions.scope = MapScope::World;
            sourceOptions.scale = MapLevel::World;
            sourceOptions.purpose = MapGenerationPurpose::Region;
            sourceOptions.width = std::max(32, request.options.width);
            sourceOptions.height = std::max(22, request.options.height);
            sourceOptions.seed ^= 0x51a3u;
            MapGenerationResult source = takeExportableCandidate(
                generateSemanticMapCandidateBatch(sourceOptions, 1),
                "Travel preview source generation");
            // Generated preview sources do not have a persisted campaign ID;
            // EntityMetadata therefore assigns a fresh ID for every process.
            // Travel generation deliberately includes its real source-map ID
            // in the seed, so pin the synthetic preview source deterministically
            // to keep identical CLI recipes reproducible across exporter runs.
            source.map.metadata.id = "preview_travel_source_" + std::to_string(request.options.seed);
            TravelSceneRecipe travel;
            travel.schemaVersion = request.travelSchema;
            travel.biome = request.travelBiome;
            travel.width = request.options.width;
            travel.height = request.options.height;
            travel.contentLanguage = request.options.contentLanguage;
            travel.seed = request.options.seed;
            travel.routeId = "preview_route";
            travel.routeName = "Preview Journey";
            travel.purpose = static_cast<TravelScenePurpose>(request.travelPurpose < 0 ? request.options.seed % 5u : std::clamp(request.travelPurpose, 0, 4));
            travel.combatReady = travel.purpose == TravelScenePurpose::Danger;
            const std::vector<TravelSceneCandidate> candidates = TravelSceneGenerator::generate(source.map, nullptr, travel, 1);
            if (candidates.empty())
                throw std::runtime_error("Travel generator returned no candidate");
            result.map = candidates.front().localMap;
            result.quality = candidates.front().quality;
        }
        else
        {
            result = takeExportableCandidate(
                generateSemanticMapCandidateBatch(request.options, 1),
                "Preview generation");
        }
        // `generateSemanticMap()` deliberately retains a diagnostic fallback
        // for legacy tests. This user-facing exporter must never serialize or
        // render such a rejected map, regardless of which specialised path
        // produced it.
        requireExportableCandidate(result);
        // This command only generates a fresh in-memory fixture. --player-safe
        // explicitly requests its player presentation, so mark that synthetic
        // fixture visible before compiling the normal audience-filtered plan.
        // Do not weaken mapPermitsPngExport or modify a persisted campaign.
        if(request.playerSafe)result.map.visibility=MapVisibility::PlayersVisible;
        if(request.playerShowcase)
        {
            // This additional opt-in is the equivalent of revealing a
            // disposable showcase during review. Normal --player-safe keeps
            // the model's conservative release profile. Neither option loads
            // or edits user content, and GM-only feature flags stay intact.
            result.map.playerRelease.waysAndRoutes=true;
            result.map.playerRelease.placesAndMarkers=true;
            result.map.playerRelease.labels=true;
            result.map.playerRelease.politicalBorders=request.showPolitical;
            for(auto &cell:result.map.cells)cell.fogged=false;
            for(auto &feature:result.map.generation.graph.features)
                if(!feature.gmOnly)feature.discovered=true;
        }
        const MapRenderPlan renderPlan = compileRequestedRenderPlan(result.map, request);
        if (!request.pngPath.empty())
        {
            std::string error;
            if (!exportMapPng(result.map, request.pngPath, renderPlan,
                              request.pixelsPerCell, &error))
                throw std::runtime_error("Could not export PNG: " + error);
        }
        writeJson(request.outputPath, request, result, renderPlan);
        std::cout << "Generated " << result.map.width << "x" << result.map.height
                  << " " << presetLabel(request.preset)
                  << " preview JSON: " << request.outputPath;
        if (!request.pngPath.empty())
            std::cout << ", PNG: " << request.pngPath;
        std::cout << '\n';
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "map_generation_preview_export failed: " << e.what() << '\n';
        return 1;
    }
}
