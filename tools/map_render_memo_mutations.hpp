#pragma once
#include "map_render_plan_workload.hpp"
#include <functional>

namespace render_plan_test {
using MemoMutation=std::pair<std::string,std::function<void(MapModel &)>>;
// Input changes are deliberately independent of the production key writer.
// A key must distinguish even values whose current render output is equal.
inline std::vector<MemoMutation> memoMutations() {
    std::vector<MemoMutation> result;
#define MUTATE(field,value) result.push_back({#field,[](MapModel &m){m.field=value;}})
    MUTATE(metadata.id,"different-map"); MUTATE(metadata.createdAtUtc,"different-created");
    MUTATE(metadata.updatedAtUtc,"different-updated"); MUTATE(metadata.tags,std::vector<std::string>{"tag"});
    MUTATE(metadata.customProperties["key"],"value"); MUTATE(name,std::string("A\0B",3)); MUTATE(description,"different-description");
    MUTATE(scope,MapScope::World); MUTATE(atlasLevel,MapLevel::Region); MUTATE(mapKind,MapKind::Dungeon);
    MUTATE(atlasStatus,MapAtlasStatus::Linked); MUTATE(visibility,MapVisibility::GameMasterOnly);
    MUTATE(creationIntent,MapCreationIntent::TravelScene); MUTATE(sizePreset,MapSizePreset::Custom);
    MUTATE(atlasPlacement.parentId,"parent"); MUTATE(atlasPlacement.parentRect.x,.1f);
    MUTATE(atlasPlacement.parentRect.y,.2f); MUTATE(atlasPlacement.parentRect.width,.3f);
    MUTATE(atlasPlacement.parentRect.height,.4f); MUTATE(atlasPlacement.parentRevision,2); MUTATE(atlasPlacement.sourceRevision,3);
    MUTATE(spatialReference.calibrated,true); MUTATE(spatialReference.extentWidth,31.f);
    MUTATE(spatialReference.extentHeight,32.f); MUTATE(spatialReference.unit,MapDistanceUnit::Miles);
    MUTATE(spatialReference.originX,-0.f); MUTATE(spatialReference.originY,.2f); MUTATE(spatialReference.rotationDegrees,10.f);
    MUTATE(gridType,GridType::Hex); MUTATE(width,15); MUTATE(height,11); MUTATE(cellSize,2.f);
    MUTATE(isMainMap,true); MUTATE(parentMapId,"parent"); MUTATE(backgroundImagePath,"Resources/Maps/other.png");
    MUTATE(backgroundImageOpacity,.25f);
    MUTATE(playerRelease.politicalBorders,false); MUTATE(playerRelease.waysAndRoutes,false);
    MUTATE(playerRelease.placesAndMarkers,false); MUTATE(playerRelease.labels,false); MUTATE(playerRelease.tokensAndAnnotations,false);
    MUTATE(importedReference.enabled,false); MUTATE(importedReference.assetId,"other-original");
    MUTATE(importedReference.projectRelativePath,"Resources/Maps/other-original.png"); MUTATE(importedReference.sourceFileName,"other.png");
    MUTATE(importedReference.contentHash,"other-hash"); MUTATE(importedReference.sourceWidth,1024); MUTATE(importedReference.sourceHeight,768);
    MUTATE(importedReference.workingProxyAssetId,"other-proxy"); MUTATE(importedReference.workingProxyProjectRelativePath,"Resources/Maps/other-proxy.png");
    MUTATE(importedReference.workingProxyContentHash,"other-proxy-hash"); MUTATE(importedReference.workingProxyWidth,256);
    MUTATE(importedReference.workingProxyHeight,192); MUTATE(importedReference.exifOrientation,2);
    MUTATE(importedReference.workingProxyGenerated,false); MUTATE(importedReference.crop.x,.125f); MUTATE(importedReference.crop.y,.25f);
    MUTATE(importedReference.crop.width,.5f); MUTATE(importedReference.crop.height,.75f); MUTATE(importedReference.rotationDegrees,180.f);
    MUTATE(importedReference.gmOnly,true); MUTATE(importedReference.semanticallyTraced,true);
    MUTATE(cells[0].kind,MapTileKind::Hill); MUTATE(cells[0].rotationQuarterTurns,1); MUTATE(cells[0].rotationLocked,true);
    MUTATE(cells[0].symbolVariantMode,MapSymbolVariantMode::Individual); MUTATE(cells[0].wayKind,MapWayKind::Trail);
    MUTATE(cells[0].movementCost,2); MUTATE(cells[0].difficultTerrain,true); MUTATE(cells[0].blocksLineOfSight,true);
    MUTATE(cells[0].lighting,MapLightingMode::Dim); MUTATE(cells[0].fogged,true); MUTATE(cells[0].elevation,1);
    MUTATE(cells[0].movementBlockedMask,1); MUTATE(cells[0].lineOfSightBlockedMask,2);
    MUTATE(cells[0].movementOneWayBlockedMask,4); MUTATE(cells[0].lineOfSightOneWayBlockedMask,8);
    MUTATE(layers[0].metadata.id,"other-layer"); MUTATE(layers[0].name,"Changed layer");
    MUTATE(layers[0].visible,false); MUTATE(layers[0].locked,true); MUTATE(layers[0].sortOrder,3);
    MUTATE(tokens[0].metadata.id,"other-token"); MUTATE(tokens[0].name,"Changed token"); MUTATE(tokens[0].kind,MapTokenKind::NPC);
    MUTATE(tokens[0].linkedEntityType,"NPC"); MUTATE(tokens[0].linkedEntityId,"npc"); MUTATE(tokens[0].assetId,"asset");
    MUTATE(tokens[0].layerId,"hidden-layer"); MUTATE(tokens[0].x,2.5f); MUTATE(tokens[0].y,5.75f);
    MUTATE(tokens[0].rotation,13.f); MUTATE(tokens[0].size,2.f); MUTATE(tokens[0].hiddenFromPlayers,true);
    MUTATE(regions[0].metadata.id,"other-region"); MUTATE(regions[0].name,"Changed region"); MUTATE(regions[0].description,"description");
    MUTATE(regions[0].x,3); MUTATE(regions[0].y,7); MUTATE(regions[0].width,4); MUTATE(regions[0].height,3);
    MUTATE(regions[0].movementCost,2); MUTATE(regions[0].lighting,MapLightingMode::Bright);
    MUTATE(regions[0].difficultTerrain,true); MUTATE(regions[0].blocksLineOfSight,true); MUTATE(regions[0].gmOnly,true);
    MUTATE(features[0].metadata.id,"other-pin"); MUTATE(features[0].kind,MapFeatureKind::Door); MUTATE(features[0].name,"Changed pin");
    MUTATE(features[0].description,"description"); MUTATE(features[0].gmNotes,"Changed private note");
    MUTATE(features[0].linkedEntityId,"entity"); MUTATE(features[0].targetMapId,"map"); MUTATE(features[0].x,5); MUTATE(features[0].y,6);
    MUTATE(features[0].discovered,false); MUTATE(features[0].gmOnly,true); MUTATE(features[0].difficultyClass,12);
    MUTATE(routes[0].metadata.id,"other-route"); MUTATE(routes[0].name,"Changed route"); MUTATE(routes[0].fromMapId,"origin");
    MUTATE(routes[0].toMapId,"destination"); MUTATE(routes[0].startX,2); MUTATE(routes[0].startY,5);
    MUTATE(routes[0].endX,13); MUTATE(routes[0].endY,5); MUTATE(routes[0].distanceMiles,2.5f);
    MUTATE(routes[0].terrain,"terrain"); MUTATE(routes[0].notes,"Changed route note"); MUTATE(routes[0].discovered,false);
    MUTATE(generation.enabled,true); MUTATE(generation.continuousRendering,false); MUTATE(generation.graph.schemaVersion,3);
    MUTATE(generation.graph.features[0].id,"other-generated"); MUTATE(generation.graph.features[0].kind,MapGeneratedFeatureKind::Trail);
    MUTATE(generation.graph.features[0].name,"Changed generated"); MUTATE(generation.graph.features[0].materialId,"dirt");
    MUTATE(generation.graph.features[0].width,1.f); MUTATE(generation.graph.features[0].closed,true);
    MUTATE(generation.graph.features[0].locked,true); MUTATE(generation.graph.features[0].discovered,false);
    MUTATE(generation.graph.features[0].gmOnly,true); MUTATE(generation.graph.features[0].points[0].x,1.25f);
    MUTATE(generation.graph.features[0].points[0].y,3.25f); MUTATE(generation.graph.features[0].lineage.sourceMapId,"source");
    MUTATE(generation.graph.features[0].lineage.sourceFeatureId,"feature"); MUTATE(generation.graph.features[0].lineage.sourceRevision,2);
    MUTATE(generation.recipe.generatorVersion,11); MUTATE(generation.recipe.recipeVersion,3); MUTATE(generation.recipe.recipeId,"different-recipe");
    MUTATE(generation.recipe.styleId,"fantasy_neutral"); MUTATE(generation.recipe.styleVersion,5);
    MUTATE(generation.recipe.styleSnapshotId,"snapshot"); MUTATE(generation.recipe.scale,MapLevel::World);
    MUTATE(generation.recipe.purpose,MapGenerationPurpose::TravelArea); MUTATE(generation.recipe.landscape,MapLandscapeProfile::Highlands);
    MUTATE(generation.recipe.landform,MapLandform::SingleIsland); MUTATE(generation.recipe.waterLevel,30);
    MUTATE(generation.recipe.mountainDensity,20); MUTATE(generation.recipe.forestDensity,40);
    MUTATE(generation.recipe.settlementDensity,10); MUTATE(generation.recipe.routeDensity,30); MUTATE(generation.recipe.tacticalDensity,10);
    MUTATE(generation.recipe.travelScenePurpose,0); MUTATE(generation.recipe.travelSceneBiome,"forest");
    MUTATE(generation.recipe.travelSceneDangerLevel,2); MUTATE(generation.recipe.travelSceneCombatReady,true);
    MUTATE(generation.recipe.travelSceneSeed,4); MUTATE(generation.recipe.quality,MapGenerationQualityLevel::Draft);
    MUTATE(generation.recipe.creationIntent,MapCreationIntent::TacticalEncounter); MUTATE(generation.recipe.sizePreset,MapSizePreset::Custom);
    MUTATE(generation.recipe.contentLanguage,"en"); MUTATE(generation.recipe.nameCulture,"changed");
    MUTATE(generation.recipe.allowAdditionalNations,true); MUTATE(generation.recipe.seed,612346);
    MUTATE(generation.recipe.settlementCount,4); MUTATE(generation.recipe.landmarkCount,5); MUTATE(generation.recipe.edgeConnectionCount,3);
    MUTATE(generation.recipe.knownFacts,std::vector<std::string>{"a"}); MUTATE(generation.recipe.requiredElements,std::vector<std::string>{"a"});
    MUTATE(generation.recipe.optionalElements,std::vector<std::string>{"a"}); MUTATE(generation.recipe.excludedElements,std::vector<std::string>{"a"});
    MUTATE(generation.recipe.lockedFeatureIds,std::vector<std::string>{"a"});
    MUTATE(generation.recipe.constraints,std::vector<MapGenerationConstraint>{MapGenerationConstraint{}});
    MUTATE(generation.recipe.facts,std::vector<MapGenerationFact>{MapGenerationFact{}});
#undef MUTATE
#define ORDER(field) result.push_back({#field " order",[](MapModel &m){std::swap(m.field[0],m.field[1]);}})
    ORDER(tokens); ORDER(features); ORDER(regions); ORDER(routes); ORDER(layers); ORDER(generation.graph.features);
#undef ORDER
    result.push_back({"cell count",[](MapModel &m){m.cells.push_back({});}});
    result.push_back({"point count",[](MapModel &m){m.generation.graph.features[0].points.push_back({13,3});}});
    return result;
}
} // namespace render_plan_test
