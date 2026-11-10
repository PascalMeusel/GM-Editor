#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapTacticalCoverInk.hpp"
#include "Models/Map/MapSettlementSiteInk.hpp"
#include "Models/Map/MapTacticalStructureInk.hpp"
#include "Models/Map/MapTravelInk.hpp"
#include "MapRenderPlan.hpp"
#include "Models/Map/MapAtlasWaterInk.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapFeaturePresentationGeometry.hpp"

#include "Models/Map/MapImportedReferencePolicy.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include "Models/Map/MapCaveInk.hpp"
#include "Models/Map/MapGeneratedNamePresentation.hpp"
#include "Models/Map/MapRoutePresentation.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "Models/Map/MapRenderPlanInternal.hpp"

namespace MapRenderPlanInternal
{
void hashBytes(std::uint64_t &hash, const void *data, std::size_t size)
{
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
}

void hashString(std::uint64_t &hash, const std::string &value)
{
    hashBytes(hash, value.data(), value.size());
    const unsigned char terminator = 0xff;
    hashBytes(hash, &terminator, 1);
}

bool generatedFeatureLayerEnabled(const MapGeneratedFeature &feature, std::uint32_t layers)
{
    const bool labels = (layers & MapRenderPlanLabels) != 0 && !feature.name.empty();
    switch (feature.kind)
    {
    case MapGeneratedFeatureKind::Road:
    case MapGeneratedFeatureKind::Trail:
    case MapGeneratedFeatureKind::Bridge:
        return (layers & MapRenderPlanWays) != 0 || labels;
    case MapGeneratedFeatureKind::River:
    case MapGeneratedFeatureKind::WaterBody:
    case MapGeneratedFeatureKind::Coastline:
        return (layers & MapRenderPlanWater) != 0 || labels;
    case MapGeneratedFeatureKind::Nation:
        return (layers & MapRenderPlanPolitical) != 0 || labels;
    case MapGeneratedFeatureKind::TerrainRegion:
        return (layers & (feature.materialId.find("territory.") != std::string::npos
                              ? MapRenderPlanPolitical
                              : MapRenderPlanTerrain)) != 0 || labels;
    case MapGeneratedFeatureKind::Vegetation:
    case MapGeneratedFeatureKind::MountainRange:
        return (layers & MapRenderPlanTerrain) != 0 || labels;
    default:
        return (layers & MapRenderPlanSymbols) != 0 || labels;
    }
}

std::uint32_t generatedFeatureContentLayer(const MapGeneratedFeature &feature)
{
    switch (feature.kind)
    {
    case MapGeneratedFeatureKind::River:
    case MapGeneratedFeatureKind::Coastline:
    case MapGeneratedFeatureKind::WaterBody:
        return MapRenderPlanWater;
    case MapGeneratedFeatureKind::Road:
    case MapGeneratedFeatureKind::Trail:
    case MapGeneratedFeatureKind::Bridge:
        return MapRenderPlanWays;
    case MapGeneratedFeatureKind::Nation:
        return MapRenderPlanPolitical;
    case MapGeneratedFeatureKind::TerrainRegion:
        if (feature.materialId.find("water") != std::string::npos)
            return MapRenderPlanWater;
        if (feature.materialId.find("territory.") != std::string::npos)
            return MapRenderPlanPolitical;
        return MapRenderPlanTerrain;
    case MapGeneratedFeatureKind::Vegetation:
    case MapGeneratedFeatureKind::MountainRange:
        return MapRenderPlanTerrain;
    case MapGeneratedFeatureKind::District:
        return MapRenderPlanSymbols;
    default:
        return MapRenderPlanSymbols;
    }
}

const char *generatedFeatureAssetRole(const MapModel &map,
                                      const MapGeneratedFeature &feature)
{
    switch (feature.kind)
    {
    case MapGeneratedFeatureKind::River:
    case MapGeneratedFeatureKind::Road:
    case MapGeneratedFeatureKind::Trail:
        return "curve_profile";
    case MapGeneratedFeatureKind::Bridge:
        return "bridge_stamp";
    case MapGeneratedFeatureKind::Coastline:
    case MapGeneratedFeatureKind::WaterBody:
        return "water_material";
    case MapGeneratedFeatureKind::TerrainRegion:
        return feature.materialId.find("territory.") != std::string::npos
                   ? "political_overlay"
                   : "terrain_material";
    case MapGeneratedFeatureKind::Nation:
    case MapGeneratedFeatureKind::Landmark:
        return "map_symbol";
    case MapGeneratedFeatureKind::Settlement:
        return map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region
                   ? "map_symbol"
                   : "settlement_stamp";
    case MapGeneratedFeatureKind::Vegetation:
        return "vegetation_stamp";
    case MapGeneratedFeatureKind::MountainRange:
        return "landscape_stamp";
    case MapGeneratedFeatureKind::District:
        return "district_overlay";
    case MapGeneratedFeatureKind::BuildingGroup:
        return "settlement_stamp";
    case MapGeneratedFeatureKind::TacticalSetpiece:
        return "tactical_prop";
    }
    return "generated_feature";
}

MapRenderPrimitiveKind primitiveKindForLayer(std::uint32_t layer)
{
    switch (layer)
    {
    case MapRenderPlanWater: return MapRenderPrimitiveKind::GeneratedGeometry;
    case MapRenderPlanWays: return MapRenderPrimitiveKind::GeneratedGeometry;
    case MapRenderPlanPolitical: return MapRenderPrimitiveKind::PoliticalGeometry;
    case MapRenderPlanSymbols: return MapRenderPrimitiveKind::GeneratedSymbol;
    default: return MapRenderPrimitiveKind::GeneratedGeometry;
    }
}

bool planContainsId(const std::vector<EntityId> &ids, const EntityId &id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

bool playerMapCellIsRevealed(const MapModel &map, int x, int y)
{
    if (x < 0 || y < 0 || x >= map.width || y >= map.height)
        return false;
    const std::size_t index = static_cast<std::size_t>(y * map.width + x);
    return index < map.cells.size() && !map.cells[index].fogged;
}

bool playerMapPositionIsRevealed(const MapModel &map, float x, float y)
{
    return std::isfinite(x) && std::isfinite(y) &&
           playerMapCellIsRevealed(map, static_cast<int>(std::floor(x)),
                                   static_cast<int>(std::floor(y)));
}

bool playerMapLayerIsVisible(const MapModel &map, const EntityId &layerId)
{
    if (layerId.empty())
        return true;
    const auto found = std::find_if(map.layers.begin(), map.layers.end(),
                                    [&](const MapLayerModel &layer) {
                                        return layer.metadata.id == layerId;
                                    });
    return found != map.layers.end() && found->visible;
}

bool playerMapBackgroundIsFullyRevealed(const MapModel &map)
{
    if (map.width <= 0 || map.height <= 0)
        return false;
    const std::size_t width = static_cast<std::size_t>(map.width);
    const std::size_t height = static_cast<std::size_t>(map.height);
    if (height != 0 && width > (std::numeric_limits<std::size_t>::max)() / height)
        return false;
    const std::size_t required = width * height;
    return map.cells.size() >= required &&
           std::none_of(map.cells.begin(),
                        map.cells.begin() + static_cast<std::ptrdiff_t>(required),
                        [](const MapCellModel &cell) { return cell.fogged; });
}

bool generatedFeatureUsesPointProjection(MapGeneratedFeatureKind kind)
{
    return kind == MapGeneratedFeatureKind::Settlement ||
           kind == MapGeneratedFeatureKind::Landmark ||
           kind == MapGeneratedFeatureKind::BuildingGroup ||
           kind == MapGeneratedFeatureKind::TacticalSetpiece;
}

bool projectedCellMatchesFeature(const MapCellModel &cell,
                                 MapGeneratedFeatureKind kind)
{
    switch (kind)
    {
    case MapGeneratedFeatureKind::Settlement:
        return cell.kind == MapTileKind::Settlement;
    case MapGeneratedFeatureKind::Landmark:
        return cell.kind == MapTileKind::Landmark;
    case MapGeneratedFeatureKind::BuildingGroup:
        return cell.kind == MapTileKind::Building;
    case MapGeneratedFeatureKind::TacticalSetpiece:
        return cell.kind == MapTileKind::Cover ||
               cell.kind == MapTileKind::Rubble ||
               cell.kind == MapTileKind::Hazard ||
               cell.kind == MapTileKind::Landmark;
    default:
        return false;
    }
}

void suppressUnreviewedPointProjections(MapModel &filtered,
                                        const MapModel &source,
                                        const MapRenderPlan &plan)
{
    const bool symbolsEnabled = (plan.layers & MapRenderPlanSymbols) != 0;
    for (const MapGeneratedFeature &feature : source.generation.graph.features)
    {
        if (!generatedFeatureUsesPointProjection(feature.kind) ||
            feature.points.empty())
            continue;
        const int x = static_cast<int>(std::floor(feature.points.front().x));
        const int y = static_cast<int>(std::floor(feature.points.front().y));
        if (x < 0 || y < 0 || x >= filtered.width || y >= filtered.height)
            continue;

        const bool reviewed = symbolsEnabled &&
                              planContainsId(plan.generatedFeatureIds, feature.id) &&
                              (plan.audience != MapRenderAudience::Player ||
                               (static_cast<std::size_t>(y * filtered.width + x) < filtered.cells.size() &&
                                !filtered.cells[static_cast<std::size_t>(y * filtered.width + x)].fogged));
        if (reviewed)
            continue;

        // Do not erase a public symbol which deliberately shares an anchor
        // with a private or layer-disabled feature.
        const bool retainedAtAnchor = std::any_of(
            source.generation.graph.features.begin(),
            source.generation.graph.features.end(),
            [&](const MapGeneratedFeature &candidate) {
                if (!symbolsEnabled || &candidate == &feature || candidate.points.empty() ||
                    !generatedFeatureUsesPointProjection(candidate.kind) ||
                    !planContainsId(plan.generatedFeatureIds, candidate.id))
                    return false;
                if (plan.audience == MapRenderAudience::Player &&
                    (static_cast<std::size_t>(y * filtered.width + x) >= filtered.cells.size() ||
                     filtered.cells[static_cast<std::size_t>(y * filtered.width + x)].fogged))
                    return false;
                return static_cast<int>(std::floor(candidate.points.front().x)) == x &&
                       static_cast<int>(std::floor(candidate.points.front().y)) == y;
            });
        if (retainedAtAnchor)
            continue;

        const std::size_t index = static_cast<std::size_t>(y * filtered.width + x);
        if (index >= filtered.cells.size() ||
            !projectedCellMatchesFeature(filtered.cells[index], feature.kind))
            continue;
        // The raster cell is only a synchronized visual/gameplay projection;
        // the feature graph remains authoritative. Empty is the conservative
        // terrain fallback when the old projection did not retain its prior
        // underlying biome.
        if (feature.kind == MapGeneratedFeatureKind::BuildingGroup &&
            gm::map_render::supportsFootprintRoofInk(source))
        {
            // A V10 roof spans the complete blocked footprint. Hiding only its
            // centre stamp would expose the remaining private building cells.
            // Touching footprints without distinct provenance fail closed as
            // one component; an explicitly public shared anchor was handled
            // above before reaching this suppression path.
            struct FootprintCell { int x; int y; };
            std::vector<FootprintCell> pending{{x,y}};
            std::vector<unsigned char> visited(source.cells.size(),0);
            visited[index]=1;
            for(std::size_t next=0;next<pending.size();++next)
            {
                const auto point=pending[next];
                const auto cellIndex=static_cast<std::size_t>(mapCellIndex(source,point.x,point.y));
                filtered.cells[cellIndex].kind=suppressedBuildingUnderlay(source);
                filtered.cells[cellIndex].wayKind=MapWayKind::None;
                for(auto delta:{FootprintCell{1,0},FootprintCell{-1,0},FootprintCell{0,1},FootprintCell{0,-1}})
                {
                    const int nx=point.x+delta.x,ny=point.y+delta.y;
                    const auto *neighbor=mapCellAt(source,nx,ny);
                    if(!neighbor || neighbor->kind!=MapTileKind::Building) continue;
                    const auto neighborIndex=static_cast<std::size_t>(mapCellIndex(source,nx,ny));
                    if(!visited[neighborIndex]) {visited[neighborIndex]=1;pending.push_back({nx,ny});}
                }
            }
        }
        else
        {
            filtered.cells[index].kind = MapTileKind::Empty;
            filtered.cells[index].wayKind = MapWayKind::None;
        }
    }
}

void suppressUnreviewedMountainProjections(MapModel &filtered,
                                           const MapModel &source,
                                           const MapRenderPlan &plan)
{
    // Painted relief is resolved from cells. A hidden generated range must
    // not reappear as painted relief after its graph entry is filtered out.
    // Old raster projections have no per-cell ownership: suppress the attached
    // mountain component, while keeping separate hand-painted areas intact.
    std::vector<unsigned char> visited;
    std::vector<MapCellCoordinate> pending;
    for (const auto &feature : source.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::MountainRange ||
            planContainsId(plan.generatedFeatureIds, feature.id))
            continue;
        if (visited.empty()) visited.resize(source.cells.size(), 0);
        const auto append = [&](int x, int y) {
            const auto *cell = mapCellAt(source, x, y);
            if (!cell || cell->kind != MapTileKind::Mountain) return;
            const auto index = static_cast<std::size_t>(mapCellIndex(source, x, y));
            if (visited[index]) return;
            visited[index] = 1;
            pending.push_back({x, y});
        };
        for (const auto &cell : mapGeneratedFeatureRasterCells(source, feature))
            append(cell.x, cell.y);
        for (std::size_t next = 0; next < pending.size(); ++next)
        {
            const auto cell = pending[next];
            mapCellAt(filtered, cell.x, cell.y)->kind = MapTileKind::Dirt;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (dx || dy) append(cell.x + dx, cell.y + dy);
        }
        pending.clear();
    }
}

void suppressPlayerRasterLinearProjections(MapModel &filtered)
{
    // Resolve underlying atlas terrain while river provenance still exists.
    // Diagonal rasterization also creates Water/None corner helpers; erasing
    // the neighbouring River flags first makes those helpers look like lakes
    // to the player's terrain/coast pass. This keeps only resolved ground,
    // never the private river graph or its raster way flags.
    const auto overviewGround = gm::map_render::overviewUsesAtlasTerrain(filtered)
        ? gm::map_render::buildOverviewGroundProjection(filtered)
        : std::vector<MapTileKind>{};
    for (std::size_t index = 0; index < filtered.cells.size(); ++index)
    {
        MapCellModel &cell = filtered.cells[index];
        const MapWayKind projectedWay = cell.wayKind;
        if (cell.kind == MapTileKind::Water && index < overviewGround.size() &&
            gm::map_render::overviewGroundKindIsLand(overviewGround[index]))
            cell.kind = overviewGround[index];
        // Raster ways have no stable source-feature provenance. Once any
        // category is released, retaining them could reveal a private road or
        // river that was correctly omitted from generatedFeatureIds. Player
        // output therefore renders only reviewed graph/route primitives.
        cell.wayKind = MapWayKind::None;
        if (cell.kind == MapTileKind::Road || cell.kind == MapTileKind::Bridge ||
            ((projectedWay == MapWayKind::River || projectedWay == MapWayKind::Canal) &&
             cell.kind == MapTileKind::Water))
            cell.kind = MapTileKind::Empty;
    }
}
}
