#include "Models/Map/MapPackageService.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include "Models/Project/ApplicationPaths.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <unordered_set>
#include <unordered_map>

#include "Models/Map/MapPackageServiceInternal.hpp"

namespace MapPackageServiceInternal
{
void writeMetadata(Writer &writer, const EntityMetadata &metadata)
{
    writer.string(metadata.id);
    writer.string(metadata.createdAtUtc);
    writer.string(metadata.updatedAtUtc);
    writer.u32(static_cast<std::uint32_t>(metadata.tags.size()));
    for (const std::string &tag : metadata.tags) writer.string(tag);
    writer.u32(static_cast<std::uint32_t>(metadata.customProperties.size()));
    for (const auto &property : metadata.customProperties)
    {
        writer.string(property.first);
        writer.string(property.second);
    }
}

void readMetadata(Reader &reader, EntityMetadata &metadata)
{
    metadata.id = reader.string();
    metadata.createdAtUtc = reader.string();
    metadata.updatedAtUtc = reader.string();
    metadata.tags.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
        metadata.tags.push_back(reader.string());
    metadata.customProperties.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
    {
        const std::string key = reader.string();
        const std::string value = reader.string();
        if (reader.good()) metadata.customProperties.emplace(key, value);
    }
}

void writeStrings(Writer &writer, const std::vector<std::string> &values)
{
    writer.u32(static_cast<std::uint32_t>(values.size()));
    for (const std::string &value : values) writer.string(value);
}

void readStrings(Reader &reader, std::vector<std::string> &values)
{
    values.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
        values.push_back(reader.string());
}

void writeRect(Writer &writer, const MapNormalizedRect &rect)
{
    writer.floating(rect.x); writer.floating(rect.y); writer.floating(rect.width); writer.floating(rect.height);
}

void readRect(Reader &reader, MapNormalizedRect &rect)
{
    rect.x = reader.floating(); rect.y = reader.floating(); rect.width = reader.floating(); rect.height = reader.floating();
}

void writeMap(Writer &writer, const MapModel &map)
{
    writeMetadata(writer, map.metadata);
    writer.string(map.name); writer.string(map.description);
    writeEnum(writer, map.scope); writeEnum(writer, map.atlasLevel); writeEnum(writer, map.mapKind);
    writeEnum(writer, map.atlasStatus); writeEnum(writer, map.visibility); writeEnum(writer, map.creationIntent);
    writeEnum(writer, map.sizePreset); writer.u64(map.revision);
    writer.string(map.atlasPlacement.parentId); writeRect(writer, map.atlasPlacement.parentRect);
    writer.u64(map.atlasPlacement.parentRevision); writer.u64(map.atlasPlacement.sourceRevision);
    writer.boolean(map.spatialReference.calibrated); writer.floating(map.spatialReference.extentWidth);
    writer.floating(map.spatialReference.extentHeight); writeEnum(writer, map.spatialReference.unit);
    writer.floating(map.spatialReference.originX); writer.floating(map.spatialReference.originY);
    writer.floating(map.spatialReference.rotationDegrees);
    writeEnum(writer, map.gridType); writer.i32(map.width); writer.i32(map.height); writer.floating(map.cellSize);
    writer.boolean(map.isMainMap); writer.string(map.parentMapId); writer.string(map.backgroundImagePath);
    writer.floating(map.backgroundImageOpacity);
    writer.boolean(map.importedReference.enabled); writer.string(map.importedReference.assetId);
    writer.string(map.importedReference.projectRelativePath); writer.string(map.importedReference.sourceFileName);
    writer.string(map.importedReference.contentHash); writer.i32(map.importedReference.sourceWidth);
    writer.i32(map.importedReference.sourceHeight); writeRect(writer, map.importedReference.crop);
    writer.floating(map.importedReference.rotationDegrees); writer.boolean(map.importedReference.gmOnly);
    writer.boolean(map.importedReference.semanticallyTraced);
    writer.string(map.importedReference.workingProxyAssetId);
    writer.string(map.importedReference.workingProxyProjectRelativePath);
    writer.string(map.importedReference.workingProxyContentHash);
    writer.i32(map.importedReference.workingProxyWidth);
    writer.i32(map.importedReference.workingProxyHeight);
    writer.i32(map.importedReference.exifOrientation);
    writer.boolean(map.importedReference.workingProxyGenerated);
    writer.boolean(map.playerRelease.politicalBorders);
    writer.boolean(map.playerRelease.waysAndRoutes);
    writer.boolean(map.playerRelease.placesAndMarkers);
    writer.boolean(map.playerRelease.labels);
    writer.boolean(map.playerRelease.tokensAndAnnotations);

    writer.u32(static_cast<std::uint32_t>(map.cells.size()));
    for (const MapCellModel &cell : map.cells)
    {
        writeEnum(writer, cell.kind); writer.i32(cell.rotationQuarterTurns); writer.boolean(cell.rotationLocked);
        writeEnum(writer, cell.symbolVariantMode); writeEnum(writer, cell.wayKind); writer.i32(cell.movementCost);
        writer.boolean(cell.difficultTerrain); writer.boolean(cell.blocksLineOfSight); writeEnum(writer, cell.lighting);
        writer.boolean(cell.fogged); writer.i32(cell.elevation); writer.i32(cell.movementBlockedMask);
        writer.i32(cell.lineOfSightBlockedMask); writer.i32(cell.movementOneWayBlockedMask);
        writer.i32(cell.lineOfSightOneWayBlockedMask);
    }

    writer.u32(static_cast<std::uint32_t>(map.layers.size()));
    for (const MapLayerModel &layer : map.layers)
    {
        writeMetadata(writer, layer.metadata); writer.string(layer.name); writer.boolean(layer.visible);
        writer.boolean(layer.locked); writer.i32(layer.sortOrder);
    }
    writer.u32(static_cast<std::uint32_t>(map.tokens.size()));
    for (const MapTokenModel &token : map.tokens)
    {
        writeMetadata(writer, token.metadata); writer.string(token.name); writeEnum(writer, token.kind);
        writer.string(token.linkedEntityType); writer.string(token.linkedEntityId); writer.string(token.assetId);
        writer.string(token.layerId); writer.floating(token.x); writer.floating(token.y); writer.floating(token.rotation);
        writer.floating(token.size); writer.boolean(token.hiddenFromPlayers);
    }
    writer.u32(static_cast<std::uint32_t>(map.regions.size()));
    for (const MapRegionModel &region : map.regions)
    {
        writeMetadata(writer, region.metadata); writer.string(region.name); writer.string(region.description);
        writer.i32(region.x); writer.i32(region.y); writer.i32(region.width); writer.i32(region.height);
        writer.i32(region.movementCost); writeEnum(writer, region.lighting); writer.boolean(region.difficultTerrain);
        writer.boolean(region.blocksLineOfSight); writer.boolean(region.gmOnly);
    }
    writer.u32(static_cast<std::uint32_t>(map.features.size()));
    for (const MapFeatureModel &feature : map.features)
    {
        writeMetadata(writer, feature.metadata); writeEnum(writer, feature.kind); writer.string(feature.name);
        writer.string(feature.description); writer.string(feature.gmNotes); writer.string(feature.linkedEntityId);
        writer.string(feature.targetMapId); writer.i32(feature.x); writer.i32(feature.y);
        writer.boolean(feature.discovered); writer.boolean(feature.gmOnly); writer.i32(feature.difficultyClass);
    }
    writer.u32(static_cast<std::uint32_t>(map.routes.size()));
    for (const MapRouteModel &route : map.routes)
    {
        writeMetadata(writer, route.metadata); writer.string(route.name); writer.string(route.fromMapId);
        writer.string(route.toMapId); writer.i32(route.startX); writer.i32(route.startY); writer.i32(route.endX);
        writer.i32(route.endY); writer.floating(route.distanceMiles); writer.string(route.terrain);
        writer.string(route.notes); writer.boolean(route.discovered);
    }

    writer.boolean(map.generation.enabled); writer.boolean(map.generation.continuousRendering);
    const MapGenerationRecipe &recipe = map.generation.recipe;
    writer.i32(recipe.generatorVersion); writer.i32(recipe.recipeVersion);
    writer.string(recipe.recipeId); writer.string(recipe.styleId);
    writer.i32(recipe.styleVersion); writer.string(recipe.styleSnapshotId); writeEnum(writer, recipe.scale);
    writeEnum(writer, recipe.purpose); writeEnum(writer, recipe.landscape);
    writer.string(mapLandformToken(recipe.landform));
    writer.i32(recipe.waterLevel); writer.i32(recipe.mountainDensity); writer.i32(recipe.forestDensity);
    writer.i32(recipe.settlementDensity); writer.i32(recipe.routeDensity); writer.i32(recipe.tacticalDensity);
    writer.i32(recipe.travelScenePurpose); writer.string(recipe.travelSceneBiome);
    writer.i32(recipe.travelSceneDangerLevel); writer.boolean(recipe.travelSceneCombatReady); writer.u32(recipe.travelSceneSeed);
    writeEnum(writer, recipe.quality);
    writeEnum(writer, recipe.creationIntent); writeEnum(writer, recipe.sizePreset); writer.string(recipe.contentLanguage);
    writer.string(recipe.nameCulture); writer.boolean(recipe.allowAdditionalNations); writer.u32(recipe.seed);
    writer.i32(recipe.settlementCount); writer.i32(recipe.landmarkCount); writer.i32(recipe.edgeConnectionCount);
    writeStrings(writer, recipe.knownFacts); writeStrings(writer, recipe.requiredElements);
    writeStrings(writer, recipe.optionalElements); writeStrings(writer, recipe.excludedElements);
    writeStrings(writer, recipe.lockedFeatureIds);
    writer.u32(static_cast<std::uint32_t>(recipe.constraints.size()));
    for (const MapGenerationConstraint &constraint : recipe.constraints)
    {
        writer.string(constraint.id); writer.string(constraint.label); writeEnum(writer, constraint.featureKind);
        writer.floating(constraint.x); writer.floating(constraint.y); writer.boolean(constraint.required);
        writer.boolean(constraint.locked);
    }
    writer.u32(static_cast<std::uint32_t>(recipe.facts.size()));
    for (const MapGenerationFact &fact : recipe.facts)
    {
        writer.string(fact.id); writeEnum(writer, fact.type); writeEnum(writer, fact.strength); writer.string(fact.label);
        writeStrings(writer, fact.entityRefs); writer.string(fact.value); writer.string(fact.sourceText);
        writer.floating(fact.confidence); writer.boolean(fact.confirmed);
    }

    writer.i32(map.generation.graph.schemaVersion);
    writer.u32(static_cast<std::uint32_t>(map.generation.graph.features.size()));
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        writer.string(feature.id); writeEnum(writer, feature.kind); writer.string(feature.name);
        writer.string(feature.materialId); writer.floating(feature.width); writer.boolean(feature.closed);
        writer.boolean(feature.locked); writer.boolean(feature.discovered); writer.boolean(feature.gmOnly);
        writer.u32(static_cast<std::uint32_t>(feature.points.size()));
        for (const MapFeaturePoint &point : feature.points)
        {
            writer.floating(point.x); writer.floating(point.y);
        }
        writer.string(feature.lineage.sourceMapId); writer.string(feature.lineage.sourceFeatureId);
        writer.u64(feature.lineage.sourceRevision);
    }
}
}
