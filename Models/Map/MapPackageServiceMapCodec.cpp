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
void readMap(Reader &reader, MapModel &map, std::uint32_t formatVersion)
{
    readMetadata(reader, map.metadata);
    map.name = reader.string(); map.description = reader.string();
    map.scope = reader.enumeration<MapScope>(2); map.atlasLevel = reader.enumeration<MapLevel>(3);
    map.mapKind = reader.enumeration<MapKind>(5); map.atlasStatus = reader.enumeration<MapAtlasStatus>(6);
    map.visibility = reader.enumeration<MapVisibility>(2); map.creationIntent = reader.enumeration<MapCreationIntent>(7);
    map.sizePreset = reader.enumeration<MapSizePreset>(3); map.revision = reader.u64();
    map.atlasPlacement.parentId = reader.string(); readRect(reader, map.atlasPlacement.parentRect);
    map.atlasPlacement.parentRevision = reader.u64(); map.atlasPlacement.sourceRevision = reader.u64();
    map.spatialReference.calibrated = reader.boolean(); map.spatialReference.extentWidth = reader.floating();
    map.spatialReference.extentHeight = reader.floating();
    map.spatialReference.unit = reader.enumeration<MapDistanceUnit>(4);
    map.spatialReference.originX = reader.floating(); map.spatialReference.originY = reader.floating();
    map.spatialReference.rotationDegrees = reader.floating();
    map.gridType = reader.enumeration<GridType>(2); map.width = reader.i32(); map.height = reader.i32();
    map.cellSize = reader.floating(); map.isMainMap = reader.boolean(); map.parentMapId = reader.string();
    map.backgroundImagePath = reader.pathString(); map.backgroundImageOpacity = reader.floating();
    map.importedReference.enabled = reader.boolean(); map.importedReference.assetId = reader.string();
    map.importedReference.projectRelativePath = reader.pathString();
    map.importedReference.sourceFileName = reader.pathString();
    map.importedReference.contentHash = reader.string(); map.importedReference.sourceWidth = reader.i32();
    map.importedReference.sourceHeight = reader.i32(); readRect(reader, map.importedReference.crop);
    map.importedReference.rotationDegrees = reader.floating(); map.importedReference.gmOnly = reader.boolean();
    map.importedReference.semanticallyTraced = reader.boolean();
    if (formatVersion >= 2)
    {
        map.importedReference.workingProxyAssetId = reader.string();
        map.importedReference.workingProxyProjectRelativePath = reader.pathString();
        map.importedReference.workingProxyContentHash = reader.string();
        map.importedReference.workingProxyWidth = reader.i32();
        map.importedReference.workingProxyHeight = reader.i32();
        map.importedReference.exifOrientation = reader.i32();
        map.importedReference.workingProxyGenerated = reader.boolean();
    }
    map.playerRelease = {};
    if (formatVersion >= 4)
    {
        map.playerRelease.politicalBorders = reader.boolean();
        map.playerRelease.waysAndRoutes = reader.boolean();
        map.playerRelease.placesAndMarkers = reader.boolean();
        map.playerRelease.labels = reader.boolean();
        map.playerRelease.tokensAndAnnotations = reader.boolean();
    }

    map.cells.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
    {
        MapCellModel cell;
        cell.kind = reader.enumeration<MapTileKind>(27); cell.rotationQuarterTurns = reader.i32();
        cell.rotationLocked = reader.boolean(); cell.symbolVariantMode = reader.enumeration<MapSymbolVariantMode>(2);
        cell.wayKind = reader.enumeration<MapWayKind>(7); cell.movementCost = reader.i32();
        cell.difficultTerrain = reader.boolean(); cell.blocksLineOfSight = reader.boolean();
        cell.lighting = reader.enumeration<MapLightingMode>(3); cell.fogged = reader.boolean();
        cell.elevation = reader.i32(); cell.movementBlockedMask = reader.i32();
        cell.lineOfSightBlockedMask = reader.i32(); cell.movementOneWayBlockedMask = reader.i32();
        cell.lineOfSightOneWayBlockedMask = reader.i32();
        if (reader.good()) map.cells.push_back(cell);
    }

    map.layers.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
    {
        MapLayerModel layer; readMetadata(reader, layer.metadata); layer.name = reader.string();
        layer.visible = reader.boolean(); layer.locked = reader.boolean(); layer.sortOrder = reader.i32();
        if (reader.good()) map.layers.push_back(std::move(layer));
    }
    map.tokens.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
    {
        MapTokenModel token; readMetadata(reader, token.metadata); token.name = reader.string();
        token.kind = reader.enumeration<MapTokenKind>(6); token.linkedEntityType = reader.string();
        token.linkedEntityId = reader.string(); token.assetId = reader.string(); token.layerId = reader.string();
        token.x = reader.floating(); token.y = reader.floating(); token.rotation = reader.floating();
        token.size = reader.floating(); token.hiddenFromPlayers = reader.boolean();
        if (reader.good()) map.tokens.push_back(std::move(token));
    }
    map.regions.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
    {
        MapRegionModel region; readMetadata(reader, region.metadata); region.name = reader.string();
        region.description = reader.string(); region.x = reader.i32(); region.y = reader.i32();
        region.width = reader.i32(); region.height = reader.i32(); region.movementCost = reader.i32();
        region.lighting = reader.enumeration<MapLightingMode>(3); region.difficultTerrain = reader.boolean();
        region.blocksLineOfSight = reader.boolean(); region.gmOnly = reader.boolean();
        if (reader.good()) map.regions.push_back(std::move(region));
    }
    map.features.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
    {
        MapFeatureModel feature; readMetadata(reader, feature.metadata);
        feature.kind = reader.enumeration<MapFeatureKind>(5); feature.name = reader.string();
        feature.description = reader.string(); feature.gmNotes = reader.string();
        feature.linkedEntityId = reader.string(); feature.targetMapId = reader.string();
        feature.x = reader.i32(); feature.y = reader.i32(); feature.discovered = reader.boolean();
        feature.gmOnly = reader.boolean(); feature.difficultyClass = reader.i32();
        if (reader.good()) map.features.push_back(std::move(feature));
    }
    map.routes.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
    {
        MapRouteModel route; readMetadata(reader, route.metadata); route.name = reader.string();
        route.fromMapId = reader.string(); route.toMapId = reader.string(); route.startX = reader.i32();
        route.startY = reader.i32(); route.endX = reader.i32(); route.endY = reader.i32();
        route.distanceMiles = reader.floating(); route.terrain = reader.string(); route.notes = reader.string();
        route.discovered = reader.boolean();
        if (reader.good()) map.routes.push_back(std::move(route));
    }

    map.generation.enabled = reader.boolean(); map.generation.continuousRendering = reader.boolean();
    MapGenerationRecipe &recipe = map.generation.recipe;
    recipe.generatorVersion = reader.i32();
    // V1/V2 already described the V3 recipe structure but accidentally did
    // not persist its explicit version. Preserve compatibility while making
    // V3+ packages fully reproducible.
    recipe.recipeVersion = formatVersion >= 3 ? reader.i32() : 3;
    recipe.recipeId = reader.string(); recipe.styleId = reader.string();
    recipe.styleVersion = reader.i32(); recipe.styleSnapshotId = reader.string();
    recipe.scale = reader.enumeration<MapLevel>(3); recipe.purpose = reader.enumeration<MapGenerationPurpose>(3);
    recipe.landscape = reader.enumeration<MapLandscapeProfile>(4);
    recipe.landform = formatVersion >= 5 ? mapLandformFromToken(reader.string()) : MapLandform::Automatic;
    if (formatVersion >= 5)
    {
        recipe.waterLevel = reader.i32(); recipe.mountainDensity = reader.i32(); recipe.forestDensity = reader.i32();
        recipe.settlementDensity = reader.i32(); recipe.routeDensity = reader.i32(); recipe.tacticalDensity = reader.i32();
    }
    if (formatVersion >= 6)
    {
        recipe.travelScenePurpose = std::clamp(reader.i32(), -1, 4); recipe.travelSceneBiome = reader.string();
        recipe.travelSceneDangerLevel = std::clamp(reader.i32(), 0, 5);
        recipe.travelSceneCombatReady = reader.boolean(); recipe.travelSceneSeed = reader.u32();
    }
    recipe.quality = reader.enumeration<MapGenerationQualityLevel>(2);
    recipe.creationIntent = reader.enumeration<MapCreationIntent>(7);
    recipe.sizePreset = reader.enumeration<MapSizePreset>(3); recipe.contentLanguage = reader.string();
    recipe.nameCulture = reader.string(); recipe.allowAdditionalNations = reader.boolean(); recipe.seed = reader.u32();
    recipe.settlementCount = reader.i32(); recipe.landmarkCount = reader.i32();
    recipe.edgeConnectionCount = reader.i32(); readStrings(reader, recipe.knownFacts);
    readStrings(reader, recipe.requiredElements); readStrings(reader, recipe.optionalElements);
    readStrings(reader, recipe.excludedElements); readStrings(reader, recipe.lockedFeatureIds);
    recipe.constraints.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
    {
        MapGenerationConstraint constraint; constraint.id = reader.string(); constraint.label = reader.string();
        constraint.featureKind = reader.enumeration<MapGeneratedFeatureKind>(14);
        constraint.x = reader.floating(); constraint.y = reader.floating(); constraint.required = reader.boolean();
        constraint.locked = reader.boolean();
        if (reader.good()) recipe.constraints.push_back(std::move(constraint));
    }
    recipe.facts.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
    {
        MapGenerationFact fact; fact.id = reader.string(); fact.type = reader.enumeration<MapGenerationFactType>(16);
        fact.strength = reader.enumeration<MapGenerationFactStrength>(2); fact.label = reader.string();
        readStrings(reader, fact.entityRefs); fact.value = reader.string(); fact.sourceText = reader.string();
        fact.confidence = reader.floating(); fact.confirmed = reader.boolean();
        if (reader.good()) recipe.facts.push_back(std::move(fact));
    }

    map.generation.graph.schemaVersion = reader.i32(); map.generation.graph.features.clear();
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
    {
        MapGeneratedFeature feature; feature.id = reader.string();
        feature.kind = reader.enumeration<MapGeneratedFeatureKind>(14); feature.name = reader.string();
        feature.materialId = reader.string(); feature.width = reader.floating(); feature.closed = reader.boolean();
        feature.locked = reader.boolean(); feature.discovered = reader.boolean(); feature.gmOnly = reader.boolean();
        for (std::uint32_t points = reader.count(); points && reader.good(); --points)
        {
            MapFeaturePoint point; point.x = reader.floating(); point.y = reader.floating();
            if (reader.good()) feature.points.push_back(point);
        }
        feature.lineage.sourceMapId = reader.string(); feature.lineage.sourceFeatureId = reader.string();
        feature.lineage.sourceRevision = reader.u64();
        if (reader.good()) map.generation.graph.features.push_back(std::move(feature));
    }
}
}
