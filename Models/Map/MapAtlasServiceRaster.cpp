#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/MapAuthoring.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "Models/Map/MapAtlasServiceInternal.hpp"

namespace MapAtlasServiceInternal
{
bool cellEqual(const MapCellModel &left, const MapCellModel &right)
{
    return left.kind == right.kind &&
           left.rotationQuarterTurns == right.rotationQuarterTurns &&
           left.rotationLocked == right.rotationLocked &&
           left.symbolVariantMode == right.symbolVariantMode &&
           left.wayKind == right.wayKind && left.movementCost == right.movementCost &&
           left.difficultTerrain == right.difficultTerrain &&
           left.blocksLineOfSight == right.blocksLineOfSight &&
           left.lighting == right.lighting && left.fogged == right.fogged &&
           left.elevation == right.elevation &&
           left.movementBlockedMask == right.movementBlockedMask &&
           left.lineOfSightBlockedMask == right.lineOfSightBlockedMask &&
           left.movementOneWayBlockedMask == right.movementOneWayBlockedMask &&
           left.lineOfSightOneWayBlockedMask == right.lineOfSightOneWayBlockedMask;
}

bool authoredCellStateEqual(const MapCellModel &left, const MapCellModel &right)
{
    return left.rotationQuarterTurns == right.rotationQuarterTurns &&
           left.rotationLocked == right.rotationLocked &&
           left.symbolVariantMode == right.symbolVariantMode &&
           left.movementCost == right.movementCost &&
           left.difficultTerrain == right.difficultTerrain &&
           left.blocksLineOfSight == right.blocksLineOfSight &&
           left.lighting == right.lighting && left.fogged == right.fogged &&
           left.elevation == right.elevation &&
           left.movementBlockedMask == right.movementBlockedMask &&
           left.lineOfSightBlockedMask == right.lineOfSightBlockedMask &&
           left.movementOneWayBlockedMask == right.movementOneWayBlockedMask &&
           left.lineOfSightOneWayBlockedMask == right.lineOfSightOneWayBlockedMask;
}

std::optional<MapTileKind> atlasFeatureTileKind(const MapGeneratedFeature &feature)
{
    switch (feature.kind)
    {
    case MapGeneratedFeatureKind::WaterBody: return MapTileKind::Water;
    case MapGeneratedFeatureKind::Vegetation: return MapTileKind::Forest;
    case MapGeneratedFeatureKind::MountainRange: return MapTileKind::Mountain;
    case MapGeneratedFeatureKind::Settlement: return MapTileKind::Settlement;
    case MapGeneratedFeatureKind::Landmark: return MapTileKind::Landmark;
    case MapGeneratedFeatureKind::BuildingGroup: return MapTileKind::Building;
    case MapGeneratedFeatureKind::Bridge: return MapTileKind::Bridge;
    case MapGeneratedFeatureKind::TacticalSetpiece:
        if (feature.materialId.find("rubble") != std::string::npos) return MapTileKind::Rubble;
        if (feature.materialId.find("hazard") != std::string::npos) return MapTileKind::Hazard;
        return MapTileKind::Cover;
    case MapGeneratedFeatureKind::TerrainRegion:
        if (feature.materialId.find("forest") != std::string::npos) return MapTileKind::Forest;
        if (feature.materialId.find("mountain") != std::string::npos ||
            feature.materialId.find("rock") != std::string::npos) return MapTileKind::Mountain;
        if (feature.materialId.find("water") != std::string::npos ||
            feature.materialId.find("lake") != std::string::npos ||
            feature.materialId.find("sea") != std::string::npos) return MapTileKind::Water;
        if (feature.materialId.find("dirt") != std::string::npos ||
            feature.materialId.find("soil") != std::string::npos) return MapTileKind::Dirt;
        if (feature.materialId.find("stone") != std::string::npos) return MapTileKind::Stone;
        if (feature.materialId.find("snow") != std::string::npos) return MapTileKind::Snow;
        if (feature.materialId.find("desert") != std::string::npos ||
            feature.materialId.find("sand") != std::string::npos) return MapTileKind::Desert;
        if (feature.materialId.find("swamp") != std::string::npos) return MapTileKind::Swamp;
        return std::nullopt;
    default: return std::nullopt;
    }
}

int projectionPass(const MapGeneratedFeature &feature)
{
    if (feature.kind == MapGeneratedFeatureKind::TerrainRegion ||
        feature.kind == MapGeneratedFeatureKind::WaterBody ||
        feature.kind == MapGeneratedFeatureKind::Vegetation ||
        feature.kind == MapGeneratedFeatureKind::MountainRange)
        return 0;
    if (feature.kind == MapGeneratedFeatureKind::River)
        return 1;
    if (feature.kind == MapGeneratedFeatureKind::Road ||
        feature.kind == MapGeneratedFeatureKind::Trail)
        return 2;
    if (feature.kind == MapGeneratedFeatureKind::Bridge)
        return 3;
    return 4;
}

bool affectedCell(const std::vector<int> &affected, int index)
{
    return std::binary_search(affected.begin(), affected.end(), index);
}

void clearInheritedFeatureProjection(MapModel &map,
                                     const MapGeneratedFeature &feature,
                                     const std::vector<int> &affected)
{
    const std::vector<MapCellCoordinate> footprint = mapGeneratedFeatureRasterCells(map, feature);
    for (const MapCellCoordinate &coordinate : footprint)
    {
        const int cellIndex = mapCellIndex(map, coordinate.x, coordinate.y);
        if (!affectedCell(affected, cellIndex))
            continue;
        MapCellModel *cell = mapCellAt(map, coordinate.x, coordinate.y);
        if (!cell)
            continue;
        if (feature.kind == MapGeneratedFeatureKind::River)
        {
            const MapWayKind expected = mapGeneratedFeatureWayKind(feature);
            if (cell->wayKind == expected)
            {
                cell->wayKind = MapWayKind::None;
                if (cell->kind == MapTileKind::Water)
                    cell->kind = MapTileKind::Empty;
            }
            continue;
        }
        if (feature.kind == MapGeneratedFeatureKind::Road ||
            feature.kind == MapGeneratedFeatureKind::Trail)
        {
            const MapWayKind expected = mapGeneratedFeatureWayKind(feature);
            if (cell->wayKind == expected)
            {
                cell->wayKind = MapWayKind::None;
                if (cell->kind == MapTileKind::Bridge)
                    cell->kind = MapTileKind::Empty;
            }
            continue;
        }
        const std::optional<MapTileKind> expected = atlasFeatureTileKind(feature);
        if (expected && cell->kind == *expected)
        {
            cell->kind = MapTileKind::Empty;
            if (feature.kind == MapGeneratedFeatureKind::Bridge &&
                cell->wayKind == mapGeneratedFeatureWayKind(feature))
                cell->wayKind = MapWayKind::None;
        }
    }
}

void projectFeatureSemantics(MapModel &map,
                             const MapGeneratedFeature &feature,
                             const std::vector<int> &affected)
{
    const std::vector<MapCellCoordinate> footprint = mapGeneratedFeatureRasterCells(map, feature);
    for (const MapCellCoordinate &coordinate : footprint)
    {
        const int cellIndex = mapCellIndex(map, coordinate.x, coordinate.y);
        if (!affectedCell(affected, cellIndex))
            continue;
        MapCellModel *cell = mapCellAt(map, coordinate.x, coordinate.y);
        if (!cell)
            continue;
        if (feature.kind == MapGeneratedFeatureKind::River)
        {
            cell->kind = MapTileKind::Water;
            cell->wayKind = mapGeneratedFeatureWayKind(feature);
            continue;
        }
        if (feature.kind == MapGeneratedFeatureKind::Road ||
            feature.kind == MapGeneratedFeatureKind::Trail)
        {
            if (cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Swamp)
                cell->kind = MapTileKind::Bridge;
            cell->wayKind = mapGeneratedFeatureWayKind(feature);
            continue;
        }
        if (feature.kind == MapGeneratedFeatureKind::Bridge)
        {
            cell->kind = MapTileKind::Bridge;
            cell->wayKind = mapGeneratedFeatureWayKind(feature);
            continue;
        }
        const std::optional<MapTileKind> tileKind = atlasFeatureTileKind(feature);
        if (tileKind)
            cell->kind = *tileKind;
    }
}

void synchronizeRasterProjection(const MapModel &before,
                                 const MapModel &parent,
                                 MapModel &candidate,
                                 const std::vector<int> &affected)
{
    resizeMapCells(candidate);
    for (const MapGeneratedFeature &feature : before.generation.graph.features)
        if (inheritedFrom(feature, parent))
            clearInheritedFeatureProjection(candidate, feature, affected);

    for (int pass = 0; pass <= 4; ++pass)
        for (const MapGeneratedFeature &feature : candidate.generation.graph.features)
            if (projectionPass(feature) == pass)
                projectFeatureSemantics(candidate, feature, affected);
}

bool validateRasterSynchronization(const MapModel &before,
                                   const MapModel &parent,
                                   const MapModel &candidate,
                                   const std::vector<int> &affected,
                                   std::vector<std::string> &errors)
{
    const std::size_t expectedCells = static_cast<std::size_t>(std::max(0, candidate.width)) *
                                      static_cast<std::size_t>(std::max(0, candidate.height));
    if (candidate.cells.size() != expectedCells)
    {
        errors.push_back("The rebased raster has an invalid cell count.");
        return false;
    }
    if (before.cells.size() == candidate.cells.size())
    {
        for (std::size_t index = 0; index < candidate.cells.size(); ++index)
        {
            if (!affectedCell(affected, static_cast<int>(index)) &&
                !cellEqual(before.cells[index], candidate.cells[index]))
            {
                errors.push_back("The atlas rebase changed a raster cell outside its impact set.");
                return false;
            }
            if (!authoredCellStateEqual(before.cells[index], candidate.cells[index]))
            {
                errors.push_back("The atlas rebase changed fog or authored tactical cell state.");
                return false;
            }
        }
    }

    std::vector<int> finalProjectionOwner(candidate.cells.size(), -1);
    for (int pass = 0; pass <= 4; ++pass)
        for (std::size_t featureIndex = 0;
             featureIndex < candidate.generation.graph.features.size(); ++featureIndex)
        {
            const MapGeneratedFeature &feature =
                candidate.generation.graph.features[featureIndex];
            if (projectionPass(feature) != pass ||
                (feature.kind != MapGeneratedFeatureKind::River &&
                 feature.kind != MapGeneratedFeatureKind::Road &&
                 feature.kind != MapGeneratedFeatureKind::Trail &&
                 !atlasFeatureTileKind(feature)))
                continue;
            for (const MapCellCoordinate &coordinate :
                 mapGeneratedFeatureRasterCells(candidate, feature))
            {
                const int index = mapCellIndex(candidate, coordinate.x, coordinate.y);
                if (affectedCell(affected, index) && index >= 0 &&
                    static_cast<std::size_t>(index) < finalProjectionOwner.size())
                    finalProjectionOwner[static_cast<std::size_t>(index)] =
                        static_cast<int>(featureIndex);
            }
        }

    for (std::size_t featureIndex = 0;
         featureIndex < candidate.generation.graph.features.size(); ++featureIndex)
    {
        const MapGeneratedFeature &feature = candidate.generation.graph.features[featureIndex];
        if (!inheritedFrom(feature, parent))
            continue;
        const std::vector<MapCellCoordinate> footprint = mapGeneratedFeatureRasterCells(candidate, feature);
        for (const MapCellCoordinate &coordinate : footprint)
        {
            const MapCellModel *cell = mapCellAt(candidate, coordinate.x, coordinate.y);
            if (!cell)
            {
                errors.push_back("An inherited atlas feature projects outside the child raster.");
                return false;
            }
            if (feature.kind == MapGeneratedFeatureKind::River &&
                cell->kind != MapTileKind::Water && cell->kind != MapTileKind::Bridge)
            {
                errors.push_back("An inherited river is missing from the child raster projection.");
                return false;
            }
            if ((feature.kind == MapGeneratedFeatureKind::Road ||
                 feature.kind == MapGeneratedFeatureKind::Trail) &&
                cell->wayKind != MapWayKind::Trail &&
                cell->wayKind != MapWayKind::DirtRoad &&
                cell->wayKind != MapWayKind::StoneRoad)
            {
                errors.push_back("An inherited route is missing from the child raster projection.");
                return false;
            }
            if (feature.kind == MapGeneratedFeatureKind::Bridge &&
                cell->kind != MapTileKind::Bridge)
            {
                errors.push_back("An inherited bridge is missing from the child raster projection.");
                return false;
            }
            const std::optional<MapTileKind> expected = atlasFeatureTileKind(feature);
            const int cellIndex = mapCellIndex(candidate, coordinate.x, coordinate.y);
            if (expected && feature.kind != MapGeneratedFeatureKind::Bridge &&
                feature.kind != MapGeneratedFeatureKind::River &&
                cellIndex >= 0 && static_cast<std::size_t>(cellIndex) <
                                      finalProjectionOwner.size() &&
                finalProjectionOwner[static_cast<std::size_t>(cellIndex)] ==
                    static_cast<int>(featureIndex) && cell->kind != *expected &&
                cell->kind != MapTileKind::Bridge)
            {
                errors.push_back("An inherited atlas feature has a stale child raster projection.");
                return false;
            }
        }
    }
    return true;
}
}
