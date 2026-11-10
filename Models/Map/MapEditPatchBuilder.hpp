#pragma once

#include "Models/Map/MapEditPreservation.hpp"

inline void mapGenerationSortAndUnique(std::vector<std::size_t> &values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

inline MapGenerationPatch buildMapGenerationPatch(const MapModel &current,
                                                  const MapModel &replacement,
                                                  const MapGenerationSelection &selection)
{
    MapGenerationPatch patch;
    patch.targetMapId = current.metadata.id;
    patch.sourceRevision = current.revision;
    patch.impact = selection;
    patch.before = current;
    patch.after = current;
    if (current.width != replacement.width || current.height != replacement.height)
    {
        patch.errorCode = "map.regeneration.dimension_mismatch";
        return patch;
    }
    if (selection.useArea &&
        (!std::isfinite(selection.normalizedArea.x) ||
         !std::isfinite(selection.normalizedArea.y) ||
         !std::isfinite(selection.normalizedArea.width) ||
         !std::isfinite(selection.normalizedArea.height) ||
         selection.normalizedArea.width <= 0.0f || selection.normalizedArea.height <= 0.0f ||
         selection.normalizedArea.x < 0.0f || selection.normalizedArea.y < 0.0f ||
         selection.normalizedArea.x + selection.normalizedArea.width > 1.0001f ||
         selection.normalizedArea.y + selection.normalizedArea.height > 1.0001f))
    {
        patch.errorCode = "map.regeneration.invalid_selection";
        return patch;
    }

    const auto currentFeatureLocked = [&](const MapGeneratedFeature &feature) {
        return feature.locked ||
               std::find(current.generation.recipe.lockedFeatureIds.begin(),
                         current.generation.recipe.lockedFeatureIds.end(), feature.id) !=
                   current.generation.recipe.lockedFeatureIds.end();
    };
    if (selection.useArea)
    {
        const auto unsupportedCurrent = std::find_if(
            current.generation.graph.features.begin(), current.generation.graph.features.end(),
            [&](const MapGeneratedFeature &feature) {
                return !currentFeatureLocked(feature) &&
                       mapGenerationClosedFeatureCrossesSelection(selection, current, feature);
            });
        const auto unsupportedReplacement = std::find_if(
            replacement.generation.graph.features.begin(), replacement.generation.graph.features.end(),
            [&](const MapGeneratedFeature &feature) {
                return mapGenerationClosedFeatureCrossesSelection(selection, replacement, feature);
            });
        if (unsupportedCurrent != current.generation.graph.features.end() ||
            unsupportedReplacement != replacement.generation.graph.features.end())
        {
            // Silently deleting or inserting the whole polygon would mutate
            // geometry outside the selected area. Until polygon splitting is
            // implemented, fail closed and let the UI expand the selection or
            // regenerate that family globally.
            patch.errorCode = "map.regeneration.closed_feature_crosses_selection";
            return patch;
        }
    }

    patch.preservation.completeContents = {
        MapGenerationPreservedContent::MapIdentity,
        MapGenerationPreservedContent::MapPresentation,
        MapGenerationPreservedContent::MapClassificationAndGrid,
        MapGenerationPreservedContent::PlayerRelease,
        MapGenerationPreservedContent::AtlasLineage,
        MapGenerationPreservedContent::ImportedReference,
        MapGenerationPreservedContent::GenerationRecipe,
        MapGenerationPreservedContent::Layers,
        MapGenerationPreservedContent::Tokens,
        MapGenerationPreservedContent::Regions,
        MapGenerationPreservedContent::Annotations,
        MapGenerationPreservedContent::Routes,
        MapGenerationPreservedContent::Fog,
        MapGenerationPreservedContent::TacticalCellState};
    const auto appendEntityIds = [](const auto &entities, std::vector<EntityId> &ids) {
        ids.reserve(entities.size());
        for (const auto &entity : entities)
            ids.push_back(entity.metadata.id);
    };
    appendEntityIds(current.layers, patch.preservation.layerIds);
    appendEntityIds(current.tokens, patch.preservation.tokenIds);
    appendEntityIds(current.regions, patch.preservation.regionIds);
    appendEntityIds(current.features, patch.preservation.annotationIds);
    appendEntityIds(current.routes, patch.preservation.routeIds);
    patch.preservation.authoredCellStateIndices.reserve(current.cells.size());
    for (std::size_t index = 0; index < current.cells.size(); ++index)
        patch.preservation.authoredCellStateIndices.push_back(index);

    for (const MapGeneratedFeature &feature : current.generation.graph.features)
        if (currentFeatureLocked(feature) ||
            !mapGenerationSelectionTouchesFeature(selection, current, feature))
            patch.preservation.generatedFeatureIds.push_back(feature.id);

    const int left = selection.useArea
                         ? std::clamp(static_cast<int>(selection.normalizedArea.x * current.width), 0, current.width)
                         : 0;
    const int top = selection.useArea
                        ? std::clamp(static_cast<int>(selection.normalizedArea.y * current.height), 0, current.height)
                        : 0;
    const int right = selection.useArea
                          ? std::clamp(static_cast<int>(std::ceil((selection.normalizedArea.x + selection.normalizedArea.width) * current.width)), 0, current.width)
                          : current.width;
    const int bottom = selection.useArea
                           ? std::clamp(static_cast<int>(std::ceil((selection.normalizedArea.y + selection.normalizedArea.height) * current.height)), 0, current.height)
                           : current.height;
    const bool replacesAnyProjection = selection.replaceTerrainProjection ||
                                       selection.replaceWayProjection ||
                                       selection.replacePlaceProjection ||
                                       (selection.useArea && selection.featureKinds.empty());
    for (int y = 0; y < current.height; ++y)
        for (int x = 0; x < current.width; ++x)
            if (!replacesAnyProjection || x < left || x >= right || y < top || y >= bottom)
            {
                const int index = mapCellIndex(current, x, y);
                if (index >= 0)
                    patch.preservation.byteIdenticalCellIndices.push_back(
                        static_cast<std::size_t>(index));
            }

    auto &features = patch.after.generation.graph.features;
    std::vector<MapGeneratedFeature> preservedCrossingPieces;
    features.erase(std::remove_if(features.begin(), features.end(), [&](const MapGeneratedFeature &feature) {
        if (!mapGenerationSelectionTouchesFeature(selection, current, feature))
            return false;
        if (currentFeatureLocked(feature))
        {
            patch.preservedLockedFeatureIds.push_back(feature.id);
            return false;
        }
        patch.removedFeatureIds.push_back(feature.id);
        if (selection.useArea && mapGenerationFeatureIsLinear(feature.kind))
        {
            const auto pieces = mapGenerationClipPolyline(
                feature.points, mapGenerationSelectionBounds(selection, current), false);
            for (std::size_t pieceIndex = 0; pieceIndex < pieces.size(); ++pieceIndex)
            {
                MapGeneratedFeature preserved = feature;
                preserved.points = pieces[pieceIndex];
                // The first remaining segment retains the feature identity;
                // further segments receive deterministic child identities.
                if (pieceIndex > 0)
                    preserved.id = feature.id + "__outside_" + std::to_string(pieceIndex + 1);
                preservedCrossingPieces.push_back(std::move(preserved));
            }
        }
        return true;
    }), features.end());
    features.insert(features.end(), preservedCrossingPieces.begin(), preservedCrossingPieces.end());

    // Capture the features retained from the current map before inserting the
    // replacement candidates. Their raster footprints are authoritative: a
    // family- or area-scoped regeneration must not repaint through locked,
    // inherited, out-of-scope, or clipped outside geometry.
    const std::vector<MapGeneratedFeature> preservedCurrentFeatures = features;

    for (const MapGeneratedFeature &candidate : replacement.generation.graph.features)
    {
        if (!mapGenerationSelectionTouchesFeature(selection, replacement, candidate))
            continue;
        if (candidate.points.empty())
            continue;
        std::vector<std::vector<MapFeaturePoint>> candidatePieces;
        if (selection.useArea && mapGenerationFeatureIsLinear(candidate.kind))
            candidatePieces = mapGenerationClipPolyline(
                candidate.points, mapGenerationSelectionBounds(selection, replacement), true);
        else
            candidatePieces.push_back(candidate.points);
        for (std::size_t pieceIndex = 0; pieceIndex < candidatePieces.size(); ++pieceIndex)
        {
            if (candidatePieces[pieceIndex].empty())
                continue;
            MapGeneratedFeature inserted = candidate;
            inserted.points = candidatePieces[pieceIndex];
            inserted.id = mapGenerationDeterministicInsertedFeatureId(
                features, candidate, current.revision + 1, pieceIndex);
            if (inserted.lineage.sourceMapId.empty())
            {
                inserted.lineage.sourceMapId = current.metadata.id;
                inserted.lineage.sourceFeatureId = candidate.id;
                inserted.lineage.sourceRevision = current.revision;
            }
            inserted.locked = false;
            patch.addedFeatureIds.push_back(inserted.id);
            features.push_back(std::move(inserted));
        }
    }

    const auto roadWay = [](MapWayKind kind) {
        return kind == MapWayKind::Trail || kind == MapWayKind::DirtRoad || kind == MapWayKind::StoneRoad;
    };
    const auto waterWay = [](MapWayKind kind) {
        return kind == MapWayKind::River || kind == MapWayKind::Canal;
    };
    const auto placeTile = [](MapTileKind kind) {
        return kind == MapTileKind::Settlement || kind == MapTileKind::Building ||
               kind == MapTileKind::Landmark || kind == MapTileKind::Bridge;
    };
    const auto copyGeneratedTerrain = [](MapCellModel &target, const MapCellModel &source) {
        target.kind = source.kind;
        target.rotationQuarterTurns = source.rotationQuarterTurns;
        target.rotationLocked = source.rotationLocked;
        target.symbolVariantMode = source.symbolVariantMode;
        target.elevation = source.elevation;
    };
    std::vector<bool> preservedProjectionCells(patch.after.cells.size(), false);
    for (const MapGeneratedFeature &feature : preservedCurrentFeatures)
    {
        for (const MapCellCoordinate &coordinate :
             mapGeneratedFeatureRasterCells(patch.after, feature))
        {
            const int index = mapCellIndex(patch.after, coordinate.x, coordinate.y);
            if (index >= 0 && static_cast<std::size_t>(index) < preservedProjectionCells.size())
            {
                preservedProjectionCells[static_cast<std::size_t>(index)] = true;
                patch.preservation.byteIdenticalCellIndices.push_back(
                    static_cast<std::size_t>(index));
            }
        }
    }

    // A selected generated family owns only its generated cell projection.
    // Never assign a complete MapCellModel here: fog, tactical blockers,
    // lighting and movement cost are authored/session state.
    if (selection.replaceTerrainProjection || selection.replaceWayProjection ||
        selection.replacePlaceProjection || (selection.useArea && selection.featureKinds.empty()))
    {
        for (int y = top; y < bottom; ++y)
            for (int x = left; x < right; ++x)
            {
                const MapCellModel *source = mapCellAt(replacement, x, y);
                MapCellModel *target = mapCellAt(patch.after, x, y);
                if (!source || !target)
                    continue;
                const int targetIndex = mapCellIndex(patch.after, x, y);
                if (targetIndex >= 0 &&
                    static_cast<std::size_t>(targetIndex) < preservedProjectionCells.size() &&
                    preservedProjectionCells[static_cast<std::size_t>(targetIndex)])
                    continue;

                if (selection.featureKinds.empty())
                {
                    copyGeneratedTerrain(*target, *source);
                    target->wayKind = source->wayKind;
                    continue;
                }

                if (selection.replaceTerrainProjection)
                    copyGeneratedTerrain(*target, *source);

                if (selection.replaceWayProjection)
                {
                    const bool wantsRivers = mapGenerationSelectionMatchesKind(selection, MapGeneratedFeatureKind::River);
                    const bool wantsRoads = mapGenerationSelectionMatchesKind(selection, MapGeneratedFeatureKind::Road) ||
                                            mapGenerationSelectionMatchesKind(selection, MapGeneratedFeatureKind::Trail);
                    if ((wantsRivers && (waterWay(target->wayKind) || waterWay(source->wayKind))) ||
                        (wantsRoads && (roadWay(target->wayKind) || roadWay(source->wayKind))))
                        target->wayKind = source->wayKind;
                }

                if (selection.replacePlaceProjection && (placeTile(target->kind) || placeTile(source->kind)))
                    copyGeneratedTerrain(*target, *source);
            }
    }

    // Every other authored collection intentionally comes from current. This
    // is the preservation guarantee for tokens, notes, regions, routes, fog,
    // layers and manual feature overrides.
    patch.after.revision = current.revision + 1;
    patch.after.metadata.touch();
    mapGenerationSortAndUnique(patch.preservation.byteIdenticalCellIndices);
    mapGenerationSortAndUnique(patch.preservation.authoredCellStateIndices);
    patch.preservationVerified = mapGenerationPatchPreservesContract(patch);
    if (!patch.preservationVerified)
    {
        patch.errorCode = "map.regeneration.preservation_violation";
        return patch;
    }
    patch.valid = true;
    return patch;
}
