#pragma once

#include "Models/Map/MapEditSelectionClipping.hpp"

inline bool mapGenerationMetadataEqual(const EntityMetadata &left, const EntityMetadata &right)
{
    return left.id == right.id && left.createdAtUtc == right.createdAtUtc &&
           left.updatedAtUtc == right.updatedAtUtc && left.tags == right.tags &&
           left.customProperties == right.customProperties;
}

inline bool mapGenerationRectEqual(const MapNormalizedRect &left, const MapNormalizedRect &right)
{
    return left.x == right.x && left.y == right.y && left.width == right.width &&
           left.height == right.height;
}

inline bool mapGenerationSpatialReferenceEqual(const MapSpatialReference &left,
                                               const MapSpatialReference &right)
{
    return left.calibrated == right.calibrated && left.extentWidth == right.extentWidth &&
           left.extentHeight == right.extentHeight && left.unit == right.unit &&
           left.originX == right.originX && left.originY == right.originY &&
           left.rotationDegrees == right.rotationDegrees;
}

inline bool mapGenerationImportedReferenceEqual(const MapImportedReference &left,
                                                const MapImportedReference &right)
{
    return left.enabled == right.enabled && left.assetId == right.assetId &&
           left.projectRelativePath == right.projectRelativePath &&
           left.sourceFileName == right.sourceFileName && left.contentHash == right.contentHash &&
           left.sourceWidth == right.sourceWidth && left.sourceHeight == right.sourceHeight &&
           left.workingProxyAssetId == right.workingProxyAssetId &&
           left.workingProxyProjectRelativePath == right.workingProxyProjectRelativePath &&
           left.workingProxyContentHash == right.workingProxyContentHash &&
           left.workingProxyWidth == right.workingProxyWidth &&
           left.workingProxyHeight == right.workingProxyHeight &&
           left.exifOrientation == right.exifOrientation &&
           left.workingProxyGenerated == right.workingProxyGenerated &&
           mapGenerationRectEqual(left.crop, right.crop) &&
           left.rotationDegrees == right.rotationDegrees && left.gmOnly == right.gmOnly &&
           left.semanticallyTraced == right.semanticallyTraced;
}

inline bool mapGenerationCellEqual(const MapCellModel &left, const MapCellModel &right)
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

inline bool mapGenerationAuthoredCellStateEqual(const MapCellModel &left,
                                                const MapCellModel &right)
{
    // Terrain, way, generated symbol orientation and generated elevation may
    // change inside the impact set. Session/gameplay authoring never may.
    return left.movementCost == right.movementCost &&
           left.difficultTerrain == right.difficultTerrain &&
           left.blocksLineOfSight == right.blocksLineOfSight &&
           left.lighting == right.lighting && left.fogged == right.fogged &&
           left.movementBlockedMask == right.movementBlockedMask &&
           left.lineOfSightBlockedMask == right.lineOfSightBlockedMask &&
           left.movementOneWayBlockedMask == right.movementOneWayBlockedMask &&
           left.lineOfSightOneWayBlockedMask == right.lineOfSightOneWayBlockedMask;
}

inline bool mapGenerationFactEqual(const MapGenerationFact &left, const MapGenerationFact &right)
{
    return left.id == right.id && left.type == right.type && left.strength == right.strength &&
           left.label == right.label && left.entityRefs == right.entityRefs &&
           left.value == right.value && left.sourceText == right.sourceText &&
           left.confidence == right.confidence && left.confirmed == right.confirmed;
}

inline bool mapGenerationConstraintEqual(const MapGenerationConstraint &left,
                                         const MapGenerationConstraint &right)
{
    return left.id == right.id && left.label == right.label &&
           left.featureKind == right.featureKind && left.x == right.x && left.y == right.y &&
           left.required == right.required && left.locked == right.locked;
}

template <typename T, typename Equal>
inline bool mapGenerationVectorEqual(const std::vector<T> &left,
                                     const std::vector<T> &right,
                                     Equal equal)
{
    if (left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index)
        if (!equal(left[index], right[index]))
            return false;
    return true;
}

inline bool mapGenerationRecipeEqual(const MapGenerationRecipe &left,
                                     const MapGenerationRecipe &right)
{
    return left.generatorVersion == right.generatorVersion &&
           left.recipeVersion == right.recipeVersion && left.recipeId == right.recipeId &&
           left.styleId == right.styleId && left.styleVersion == right.styleVersion &&
           left.styleSnapshotId == right.styleSnapshotId && left.scale == right.scale &&
           left.purpose == right.purpose && left.landscape == right.landscape && left.landform == right.landform &&
           left.waterLevel == right.waterLevel && left.mountainDensity == right.mountainDensity &&
           left.forestDensity == right.forestDensity && left.settlementDensity == right.settlementDensity &&
           left.routeDensity == right.routeDensity && left.tacticalDensity == right.tacticalDensity &&
           left.quality == right.quality && left.creationIntent == right.creationIntent &&
           left.sizePreset == right.sizePreset &&
           left.contentLanguage == right.contentLanguage && left.nameCulture == right.nameCulture &&
           left.allowAdditionalNations == right.allowAdditionalNations && left.seed == right.seed &&
           left.settlementCount == right.settlementCount &&
           left.landmarkCount == right.landmarkCount &&
           left.edgeConnectionCount == right.edgeConnectionCount &&
           left.knownFacts == right.knownFacts && left.requiredElements == right.requiredElements &&
           left.optionalElements == right.optionalElements &&
           left.excludedElements == right.excludedElements &&
           left.lockedFeatureIds == right.lockedFeatureIds &&
           mapGenerationVectorEqual(left.constraints, right.constraints, mapGenerationConstraintEqual) &&
           mapGenerationVectorEqual(left.facts, right.facts, mapGenerationFactEqual);
}

inline bool mapGenerationFeatureEqual(const MapGeneratedFeature &left,
                                      const MapGeneratedFeature &right)
{
    if (left.id != right.id || left.kind != right.kind || left.name != right.name ||
        left.materialId != right.materialId || left.width != right.width ||
        left.closed != right.closed || left.locked != right.locked ||
        left.discovered != right.discovered || left.gmOnly != right.gmOnly ||
        left.points.size() != right.points.size() ||
        left.lineage.sourceMapId != right.lineage.sourceMapId ||
        left.lineage.sourceFeatureId != right.lineage.sourceFeatureId ||
        left.lineage.sourceRevision != right.lineage.sourceRevision)
        return false;
    for (std::size_t index = 0; index < left.points.size(); ++index)
        if (left.points[index].x != right.points[index].x ||
            left.points[index].y != right.points[index].y)
            return false;
    return true;
}

inline bool mapGenerationLayerEqual(const MapLayerModel &left, const MapLayerModel &right)
{
    return mapGenerationMetadataEqual(left.metadata, right.metadata) && left.name == right.name &&
           left.visible == right.visible && left.locked == right.locked &&
           left.sortOrder == right.sortOrder;
}

inline bool mapGenerationTokenEqual(const MapTokenModel &left, const MapTokenModel &right)
{
    return mapGenerationMetadataEqual(left.metadata, right.metadata) && left.name == right.name &&
           left.kind == right.kind && left.linkedEntityType == right.linkedEntityType &&
           left.linkedEntityId == right.linkedEntityId && left.assetId == right.assetId &&
           left.layerId == right.layerId && left.x == right.x && left.y == right.y &&
           left.rotation == right.rotation && left.size == right.size &&
           left.hiddenFromPlayers == right.hiddenFromPlayers;
}

inline bool mapGenerationRegionEqual(const MapRegionModel &left, const MapRegionModel &right)
{
    return mapGenerationMetadataEqual(left.metadata, right.metadata) && left.name == right.name &&
           left.description == right.description && left.x == right.x && left.y == right.y &&
           left.width == right.width && left.height == right.height &&
           left.movementCost == right.movementCost && left.lighting == right.lighting &&
           left.difficultTerrain == right.difficultTerrain &&
           left.blocksLineOfSight == right.blocksLineOfSight && left.gmOnly == right.gmOnly;
}

inline bool mapGenerationAnnotationEqual(const MapFeatureModel &left, const MapFeatureModel &right)
{
    return mapGenerationMetadataEqual(left.metadata, right.metadata) && left.kind == right.kind &&
           left.name == right.name && left.description == right.description &&
           left.gmNotes == right.gmNotes && left.linkedEntityId == right.linkedEntityId &&
           left.targetMapId == right.targetMapId && left.x == right.x && left.y == right.y &&
           left.discovered == right.discovered && left.gmOnly == right.gmOnly &&
           left.difficultyClass == right.difficultyClass;
}

inline bool mapGenerationRouteEqual(const MapRouteModel &left, const MapRouteModel &right)
{
    return mapGenerationMetadataEqual(left.metadata, right.metadata) && left.name == right.name &&
           left.fromMapId == right.fromMapId && left.toMapId == right.toMapId &&
           left.startX == right.startX && left.startY == right.startY &&
           left.endX == right.endX && left.endY == right.endY &&
           left.distanceMiles == right.distanceMiles && left.terrain == right.terrain &&
           left.notes == right.notes && left.discovered == right.discovered;
}

inline const MapGeneratedFeature *mapGenerationFindFeatureById(const MapModel &map,
                                                               const std::string &id)
{
    const auto found = std::find_if(map.generation.graph.features.begin(),
                                    map.generation.graph.features.end(),
                                    [&](const MapGeneratedFeature &feature) {
                                        return feature.id == id;
                                    });
    return found == map.generation.graph.features.end() ? nullptr : &*found;
}

inline bool mapGenerationContainsPreservedContent(const MapGenerationPreservationSet &set,
                                                  MapGenerationPreservedContent content)
{
    return std::find(set.completeContents.begin(), set.completeContents.end(), content) !=
           set.completeContents.end();
}

inline bool mapGenerationPreservationSetDeclaresCompleteContract(
    const MapGenerationPreservationSet &set)
{
    const MapGenerationPreservedContent required[] = {
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
    if (set.completeContents.size() != sizeof(required) / sizeof(required[0]))
        return false;
    for (MapGenerationPreservedContent content : required)
        if (std::count(set.completeContents.begin(), set.completeContents.end(), content) != 1)
            return false;
    return true;
}

template <typename T>
inline bool mapGenerationEntityIdsEqual(const std::vector<T> &entities,
                                        const std::vector<EntityId> &ids)
{
    if (entities.size() != ids.size())
        return false;
    for (std::size_t index = 0; index < entities.size(); ++index)
        if (entities[index].metadata.id != ids[index])
            return false;
    return true;
}

inline bool mapGenerationMapIdentityEqual(const MapModel &left, const MapModel &right)
{
    // updatedAtUtc and revision intentionally change exactly once when the
    // patch commits. Stable identity and extension metadata do not.
    return left.metadata.id == right.metadata.id &&
           left.metadata.createdAtUtc == right.metadata.createdAtUtc &&
           left.metadata.tags == right.metadata.tags &&
           left.metadata.customProperties == right.metadata.customProperties;
}

inline bool mapGenerationPlayerReleaseEqual(const MapPlayerReleaseProfile &left,
                                            const MapPlayerReleaseProfile &right)
{
    return left.politicalBorders == right.politicalBorders &&
           left.waysAndRoutes == right.waysAndRoutes &&
           left.placesAndMarkers == right.placesAndMarkers && left.labels == right.labels &&
           left.tokensAndAnnotations == right.tokensAndAnnotations;
}

inline bool mapGenerationAtlasLineageEqual(const MapModel &left, const MapModel &right)
{
    return left.atlasPlacement.parentId == right.atlasPlacement.parentId &&
           mapGenerationRectEqual(left.atlasPlacement.parentRect,
                                  right.atlasPlacement.parentRect) &&
           left.atlasPlacement.parentRevision == right.atlasPlacement.parentRevision &&
           left.atlasPlacement.sourceRevision == right.atlasPlacement.sourceRevision &&
           mapGenerationSpatialReferenceEqual(left.spatialReference, right.spatialReference) &&
           left.parentMapId == right.parentMapId;
}
