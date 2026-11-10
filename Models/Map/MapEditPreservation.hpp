#pragma once

#include "Models/Map/MapEditEquality.hpp"

inline bool mapGenerationPatchPreservesContract(const MapGenerationPatch &patch)
{
    const MapModel &before = patch.before;
    const MapModel &after = patch.after;
    if (patch.targetMapId != before.metadata.id || patch.sourceRevision != before.revision ||
        after.metadata.id != before.metadata.id || after.revision != before.revision + 1 ||
        before.cells.size() != after.cells.size() ||
        !mapGenerationPreservationSetDeclaresCompleteContract(patch.preservation) ||
        !mapGenerationEntityIdsEqual(before.layers, patch.preservation.layerIds) ||
        !mapGenerationEntityIdsEqual(before.tokens, patch.preservation.tokenIds) ||
        !mapGenerationEntityIdsEqual(before.regions, patch.preservation.regionIds) ||
        !mapGenerationEntityIdsEqual(before.features, patch.preservation.annotationIds) ||
        !mapGenerationEntityIdsEqual(before.routes, patch.preservation.routeIds) ||
        patch.preservation.authoredCellStateIndices.size() != before.cells.size())
        return false;

    for (std::size_t index = 0; index < before.cells.size(); ++index)
        if (patch.preservation.authoredCellStateIndices[index] != index)
            return false;

    if (!std::is_sorted(patch.preservation.byteIdenticalCellIndices.begin(),
                        patch.preservation.byteIdenticalCellIndices.end()) ||
        std::adjacent_find(patch.preservation.byteIdenticalCellIndices.begin(),
                           patch.preservation.byteIdenticalCellIndices.end()) !=
            patch.preservation.byteIdenticalCellIndices.end())
        return false;

    std::vector<std::string> expectedPreservedFeatureIds;
    for (const MapGeneratedFeature &feature : before.generation.graph.features)
    {
        const bool locked = feature.locked ||
                            std::find(before.generation.recipe.lockedFeatureIds.begin(),
                                      before.generation.recipe.lockedFeatureIds.end(), feature.id) !=
                                before.generation.recipe.lockedFeatureIds.end();
        if (locked || !mapGenerationSelectionTouchesFeature(patch.impact, before, feature))
            expectedPreservedFeatureIds.push_back(feature.id);
    }
    if (expectedPreservedFeatureIds != patch.preservation.generatedFeatureIds)
        return false;

    const bool replacesAnyProjection = patch.impact.replaceTerrainProjection ||
                                       patch.impact.replaceWayProjection ||
                                       patch.impact.replacePlaceProjection ||
                                       (patch.impact.useArea && patch.impact.featureKinds.empty());
    const int left = patch.impact.useArea
                         ? std::clamp(static_cast<int>(patch.impact.normalizedArea.x * before.width), 0, before.width)
                         : 0;
    const int top = patch.impact.useArea
                        ? std::clamp(static_cast<int>(patch.impact.normalizedArea.y * before.height), 0, before.height)
                        : 0;
    const int right = patch.impact.useArea
                          ? std::clamp(static_cast<int>(std::ceil((patch.impact.normalizedArea.x + patch.impact.normalizedArea.width) * before.width)), 0, before.width)
                          : before.width;
    const int bottom = patch.impact.useArea
                           ? std::clamp(static_cast<int>(std::ceil((patch.impact.normalizedArea.y + patch.impact.normalizedArea.height) * before.height)), 0, before.height)
                           : before.height;
    const auto promisesByteIdentity = [&](std::size_t index) {
        return std::binary_search(patch.preservation.byteIdenticalCellIndices.begin(),
                                  patch.preservation.byteIdenticalCellIndices.end(), index);
    };
    for (int y = 0; y < before.height; ++y)
        for (int x = 0; x < before.width; ++x)
            if (!replacesAnyProjection || x < left || x >= right || y < top || y >= bottom)
            {
                const int index = mapCellIndex(before, x, y);
                if (index < 0 || !promisesByteIdentity(static_cast<std::size_t>(index)))
                    return false;
            }

    for (const std::string &id : expectedPreservedFeatureIds)
    {
        const MapGeneratedFeature *feature = mapGenerationFindFeatureById(before, id);
        if (!feature)
            return false;
        for (const MapCellCoordinate &coordinate : mapGeneratedFeatureRasterCells(before, *feature))
        {
            const int index = mapCellIndex(before, coordinate.x, coordinate.y);
            if (index < 0 || !promisesByteIdentity(static_cast<std::size_t>(index)))
                return false;
        }
    }

    for (MapGenerationPreservedContent content : patch.preservation.completeContents)
    {
        switch (content)
        {
        case MapGenerationPreservedContent::MapIdentity:
            if (!mapGenerationMapIdentityEqual(before, after)) return false;
            break;
        case MapGenerationPreservedContent::MapPresentation:
            if (before.name != after.name || before.description != after.description) return false;
            break;
        case MapGenerationPreservedContent::MapClassificationAndGrid:
            if (before.scope != after.scope || before.atlasLevel != after.atlasLevel ||
                before.mapKind != after.mapKind || before.atlasStatus != after.atlasStatus ||
                before.visibility != after.visibility || before.creationIntent != after.creationIntent ||
                before.sizePreset != after.sizePreset || before.gridType != after.gridType ||
                before.width != after.width || before.height != after.height ||
                before.cellSize != after.cellSize || before.isMainMap != after.isMainMap)
                return false;
            break;
        case MapGenerationPreservedContent::PlayerRelease:
            if (!mapGenerationPlayerReleaseEqual(before.playerRelease, after.playerRelease)) return false;
            break;
        case MapGenerationPreservedContent::AtlasLineage:
            if (!mapGenerationAtlasLineageEqual(before, after)) return false;
            break;
        case MapGenerationPreservedContent::ImportedReference:
            if (before.backgroundImagePath != after.backgroundImagePath ||
                before.backgroundImageOpacity != after.backgroundImageOpacity ||
                !mapGenerationImportedReferenceEqual(before.importedReference, after.importedReference))
                return false;
            break;
        case MapGenerationPreservedContent::GenerationRecipe:
            if (before.generation.enabled != after.generation.enabled ||
                before.generation.continuousRendering != after.generation.continuousRendering ||
                before.generation.graph.schemaVersion != after.generation.graph.schemaVersion ||
                !mapGenerationRecipeEqual(before.generation.recipe, after.generation.recipe))
                return false;
            break;
        case MapGenerationPreservedContent::Layers:
            if (!mapGenerationVectorEqual(before.layers, after.layers, mapGenerationLayerEqual)) return false;
            break;
        case MapGenerationPreservedContent::Tokens:
            if (!mapGenerationVectorEqual(before.tokens, after.tokens, mapGenerationTokenEqual)) return false;
            break;
        case MapGenerationPreservedContent::Regions:
            if (!mapGenerationVectorEqual(before.regions, after.regions, mapGenerationRegionEqual)) return false;
            break;
        case MapGenerationPreservedContent::Annotations:
            if (!mapGenerationVectorEqual(before.features, after.features, mapGenerationAnnotationEqual)) return false;
            break;
        case MapGenerationPreservedContent::Routes:
            if (!mapGenerationVectorEqual(before.routes, after.routes, mapGenerationRouteEqual)) return false;
            break;
        case MapGenerationPreservedContent::Fog:
            for (std::size_t index : patch.preservation.authoredCellStateIndices)
                if (index >= before.cells.size() || before.cells[index].fogged != after.cells[index].fogged)
                    return false;
            break;
        case MapGenerationPreservedContent::TacticalCellState:
            for (std::size_t index : patch.preservation.authoredCellStateIndices)
                if (index >= before.cells.size() ||
                    !mapGenerationAuthoredCellStateEqual(before.cells[index], after.cells[index]))
                    return false;
            break;
        }
    }

    for (std::size_t index : patch.preservation.byteIdenticalCellIndices)
        if (index >= before.cells.size() || !mapGenerationCellEqual(before.cells[index], after.cells[index]))
            return false;

    for (const std::string &id : patch.preservation.generatedFeatureIds)
    {
        const MapGeneratedFeature *oldFeature = mapGenerationFindFeatureById(before, id);
        const MapGeneratedFeature *newFeature = mapGenerationFindFeatureById(after, id);
        if (!oldFeature || !newFeature || !mapGenerationFeatureEqual(*oldFeature, *newFeature))
            return false;
    }
    return true;
}

inline bool mapGenerationMapsEqual(const MapModel &left, const MapModel &right)
{
    return mapGenerationMetadataEqual(left.metadata, right.metadata) &&
           left.name == right.name && left.description == right.description &&
           left.scope == right.scope && left.atlasLevel == right.atlasLevel &&
           left.mapKind == right.mapKind && left.atlasStatus == right.atlasStatus &&
           left.visibility == right.visibility &&
           mapGenerationPlayerReleaseEqual(left.playerRelease, right.playerRelease) &&
           left.creationIntent == right.creationIntent && left.sizePreset == right.sizePreset &&
           left.revision == right.revision && mapGenerationAtlasLineageEqual(left, right) &&
           left.gridType == right.gridType && left.width == right.width && left.height == right.height &&
           left.cellSize == right.cellSize && left.isMainMap == right.isMainMap &&
           left.backgroundImagePath == right.backgroundImagePath &&
           left.backgroundImageOpacity == right.backgroundImageOpacity &&
           mapGenerationImportedReferenceEqual(left.importedReference, right.importedReference) &&
           mapGenerationVectorEqual(left.cells, right.cells, mapGenerationCellEqual) &&
           mapGenerationVectorEqual(left.layers, right.layers, mapGenerationLayerEqual) &&
           mapGenerationVectorEqual(left.tokens, right.tokens, mapGenerationTokenEqual) &&
           mapGenerationVectorEqual(left.regions, right.regions, mapGenerationRegionEqual) &&
           mapGenerationVectorEqual(left.features, right.features, mapGenerationAnnotationEqual) &&
           mapGenerationVectorEqual(left.routes, right.routes, mapGenerationRouteEqual) &&
           left.generation.enabled == right.generation.enabled &&
           left.generation.continuousRendering == right.generation.continuousRendering &&
           left.generation.graph.schemaVersion == right.generation.graph.schemaVersion &&
           mapGenerationRecipeEqual(left.generation.recipe, right.generation.recipe) &&
           mapGenerationVectorEqual(left.generation.graph.features,
                                    right.generation.graph.features,
                                    mapGenerationFeatureEqual);
}
