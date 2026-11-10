#include "Models/Combat/EncounterAtlasService.hpp"

#include <algorithm>
#include <cmath>

namespace
{
std::string stableTacticalMapId(const MapModel &source, const EncounterAtlasTacticalRequest &request)
{
    if (!request.tacticalMapId.empty())
        return request.tacticalMapId;
    auto quantize = [](float value) {
        return static_cast<long long>(std::llround(value * 1000000.0f));
    };
    return source.metadata.id + "::tactical::" +
           std::to_string(quantize(request.sourceRect.x)) + "_" +
           std::to_string(quantize(request.sourceRect.y)) + "_" +
           std::to_string(quantize(request.sourceRect.width)) + "_" +
           std::to_string(quantize(request.sourceRect.height)) + "_" +
           std::to_string(request.width) + "x" + std::to_string(request.height);
}

bool sourceLevelCanProduceTactical(const MapModel &source)
{
    return source.atlasLevel == MapLevel::Region || source.atlasLevel == MapLevel::Site;
}

void projectSourceCells(const MapModel &source, const MapAtlasPlacement &placement, MapModel &target)
{
    resizeMapCells(target);
    for (int y = 0; y < target.height; ++y)
    {
        for (int x = 0; x < target.width; ++x)
        {
            const MapAtlasPoint childPoint{
                (static_cast<float>(x) + 0.5f) / static_cast<float>(target.width),
                (static_cast<float>(y) + 0.5f) / static_cast<float>(target.height)};
            const MapAtlasPoint sourcePoint = MapAtlasService::childToParentNormalized(placement, childPoint);
            const int sourceX = std::clamp(static_cast<int>(std::floor(sourcePoint.x * static_cast<float>(source.width))), 0, source.width - 1);
            const int sourceY = std::clamp(static_cast<int>(std::floor(sourcePoint.y * static_cast<float>(source.height))), 0, source.height - 1);
            const MapCellModel *sourceCell = mapCellAt(source, sourceX, sourceY);
            MapCellModel *targetCell = mapCellAt(target, x, y);
            if (sourceCell && targetCell)
                *targetCell = *sourceCell;
        }
    }
}
}

namespace EncounterAtlasService
{
EncounterAtlasTacticalResult deriveTacticalMap(const MapModel &source, const EncounterAtlasTacticalRequest &request)
{
    EncounterAtlasTacticalResult result;
    if (source.metadata.id.empty())
        result.errors.push_back("The source map requires a stable ID.");
    if (!sourceLevelCanProduceTactical(source))
        result.errors.push_back("Tactical maps can only be derived from Region or Site maps.");
    if (!MapAtlasService::isNormalizedRectValid(request.sourceRect))
        result.errors.push_back("The tactical source rectangle must be a non-empty normalized excerpt.");
    if (source.width <= 0 || source.height <= 0)
        result.errors.push_back("The source map dimensions must be positive.");
    if (request.width < 4 || request.height < 4 ||
        request.width > encounterMaximumGridDimension || request.height > encounterMaximumGridDimension)
        result.errors.push_back("The tactical map dimensions must be between 4 and 512 cells per side.");
    if (!std::isfinite(request.cellSize) || request.cellSize <= 0.0f)
        result.errors.push_back("The tactical cell size must be positive.");
    if (!result.errors.empty())
        return result;

    MapAtlasPlacement placement;
    placement.parentId = source.metadata.id;
    placement.parentRect = request.sourceRect;
    placement.parentRevision = source.revision;
    placement.sourceRevision = request.revision;

    const MapAtlasExtractionResult extraction = MapAtlasService::extractFromParent(source, placement, request.width, request.height);
    if (!extraction.valid)
    {
        result.errors = extraction.errors;
        return result;
    }

    MapModel tactical;
    tactical.metadata.id = stableTacticalMapId(source, request);
    tactical.name = request.name.empty() ? source.name + " - Tactical" : request.name;
    tactical.description = "Deterministic tactical excerpt of " + source.name + ".";
    tactical.scope = MapScope::Local;
    tactical.atlasLevel = MapLevel::Tactical;
    tactical.mapKind = MapKind::Encounter;
    tactical.atlasStatus = MapAtlasStatus::Linked;
    tactical.visibility = source.visibility;
    tactical.revision = request.revision;
    tactical.atlasPlacement = placement;
    tactical.gridType = GridType::Square;
    tactical.width = request.width;
    tactical.height = request.height;
    tactical.cellSize = request.cellSize;
    tactical.parentMapId = source.metadata.id;
    tactical.generation.enabled = source.generation.enabled;
    tactical.generation.continuousRendering = source.generation.continuousRendering;
    tactical.generation.recipe = source.generation.recipe;
    tactical.generation.recipe.constraints = extraction.generationConstraints;
    tactical.generation.graph = extraction.graph;
    projectSourceCells(source, placement, tactical);
    ensureDefaultMapLayer(tactical);

    result.boundaryConstraints = extraction.generationConstraints;
    result.tacticalMap = std::move(tactical);
    result.valid = true;
    return result;
}

bool transferToEncounter(
    const MapModel &source,
    const MapModel &tacticalMap,
    EncounterModel &encounter,
    std::vector<std::string> *errors)
{
    std::vector<std::string> localErrors;
    if (source.metadata.id.empty() || tacticalMap.metadata.id.empty())
        localErrors.push_back("Source and tactical maps require stable IDs.");
    if (tacticalMap.atlasLevel != MapLevel::Tactical || tacticalMap.mapKind != MapKind::Encounter)
        localErrors.push_back("Only an Encounter-kind Tactical map can be transferred to an encounter.");
    if (tacticalMap.atlasPlacement.parentId != source.metadata.id)
        localErrors.push_back("The tactical map does not originate from the supplied source map.");
    if (!MapAtlasService::isNormalizedRectValid(tacticalMap.atlasPlacement.parentRect))
        localErrors.push_back("The tactical map source rectangle is invalid.");
    if (!encounterMapGridSupported(tacticalMap))
        localErrors.push_back("The tactical map requires a complete grid between 4 and 512 cells per side.");
    if (!localErrors.empty())
    {
        if (errors)
            *errors = std::move(localErrors);
        return false;
    }

    encounter.sourceMapId = source.metadata.id;
    encounter.sourceMapRect = tacticalMap.atlasPlacement.parentRect;
    encounter.sourceMapRevision = tacticalMap.atlasPlacement.parentRevision;
    encounter.tacticalMapId = tacticalMap.metadata.id;
    encounter.mapId = tacticalMap.metadata.id;
    encounter.mapContextName = tacticalMap.name;
    encounter.battlefieldWidth = tacticalMap.width;
    encounter.battlefieldHeight = tacticalMap.height;
    encounter.battlefieldCells = tacticalMap.cells;
    encounter.useMapCells = true;
    return true;
}

EncounterAtlasTacticalResult deriveTacticalMap(
    const MapModel &source,
    const EncounterAtlasTacticalRequest &request,
    EncounterModel *encounter)
{
    EncounterAtlasTacticalResult result = deriveTacticalMap(source, request);
    if (!result.valid || !encounter)
        return result;
    std::vector<std::string> transferErrors;
    if (!transferToEncounter(source, result.tacticalMap, *encounter, &transferErrors))
    {
        result.valid = false;
        result.errors.insert(result.errors.end(), transferErrors.begin(), transferErrors.end());
    }
    return result;
}
}
