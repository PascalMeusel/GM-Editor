#include "Models/Combat/EncounterAtlasService.hpp"

#include <iostream>
#include <string>

namespace
{
bool require(bool condition, const std::string &message)
{
    if (condition)
        return true;
    std::cerr << message << std::endl;
    return false;
}

MapModel makeSourceMap()
{
    MapModel source;
    source.metadata.id = "region-source";
    source.name = "River March";
    source.scope = MapScope::World;
    source.atlasLevel = MapLevel::Region;
    source.mapKind = MapKind::Geographic;
    source.visibility = MapVisibility::PlayersKnown;
    source.revision = 12;
    source.width = 4;
    source.height = 4;
    resizeMapCells(source);
    for (int y = 0; y < source.height; ++y)
    {
        for (int x = 0; x < source.width; ++x)
        {
            MapCellModel *cell = mapCellAt(source, x, y);
            applyMapCellTerrainDefaults(*cell, MapTileKind::Dirt);
            cell->elevation = y;
        }
    }
    MapCellModel *river = mapCellAt(source, 2, 0);
    applyMapCellTerrainDefaults(*river, MapTileKind::Water);
    river->wayKind = MapWayKind::River;
    MapCellModel *road = mapCellAt(source, 3, 0);
    applyMapCellTerrainDefaults(*road, MapTileKind::Road);
    road->wayKind = MapWayKind::DirtRoad;

    source.generation.enabled = true;
    source.generation.continuousRendering = true;
    MapGeneratedFeature feature;
    feature.id = "regional-road";
    feature.kind = MapGeneratedFeatureKind::Road;
    feature.points = {{1.0f, 2.0f}, {4.0f, 2.0f}};
    source.generation.graph.features.push_back(feature);
    return source;
}

EncounterAtlasTacticalRequest makeRequest()
{
    EncounterAtlasTacticalRequest request;
    request.sourceRect = {0.5f, 0.0f, 0.5f, 1.0f};
    request.width = 4;
    request.height = 4;
    request.cellSize = 5.0f;
    request.tacticalMapId = "tactical-river-crossing";
    request.name = "River Crossing";
    request.revision = 3;
    return request;
}

bool testDeterministicDerivationAndProvenance()
{
    const MapModel source = makeSourceMap();
    const MapCellModel originalFirstCell = source.cells.front();
    const std::size_t originalFeatureCount = source.generation.graph.features.size();
    const EncounterAtlasTacticalResult result = EncounterAtlasService::deriveTacticalMap(source, makeRequest());

    bool ok = true;
    ok &= require(result.valid, "A Region excerpt should derive a tactical map");
    const MapModel &tactical = result.tacticalMap;
    ok &= require(tactical.metadata.id == "tactical-river-crossing" && tactical.atlasLevel == MapLevel::Tactical &&
                      tactical.mapKind == MapKind::Encounter && tactical.scope == MapScope::Local,
                  "Derived map should have stable tactical encounter classification");
    ok &= require(tactical.parentMapId == source.metadata.id && tactical.atlasPlacement.parentId == source.metadata.id &&
                      tactical.atlasPlacement.parentRevision == source.revision &&
                      tactical.atlasPlacement.sourceRevision == makeRequest().revision && tactical.revision == makeRequest().revision,
                  "Derived map should preserve parent, placement, and revision provenance");
    ok &= require(tactical.width == 4 && tactical.height == 4 && tactical.cells.size() == 16,
                  "Derived map should use the requested tactical grid dimensions");
    const MapCellModel *river = mapCellAt(tactical, 0, 0);
    const MapCellModel *road = mapCellAt(tactical, 2, 0);
    ok &= require(river && river->kind == MapTileKind::Water && river->wayKind == MapWayKind::River,
                  "Cell projection should preserve water and river semantics");
    ok &= require(road && road->kind == MapTileKind::Road && road->wayKind == MapWayKind::DirtRoad,
                  "Cell projection should preserve terrain and road overlays");
    ok &= require(!tactical.generation.graph.features.empty() &&
                      tactical.generation.graph.features.front().lineage.sourceMapId == source.metadata.id,
                  "Derived feature excerpts should retain source-map lineage");
    ok &= require(source.cells.size() == 16 && source.cells.front().kind == originalFirstCell.kind &&
                      source.cells.front().wayKind == originalFirstCell.wayKind &&
                      source.generation.graph.features.size() == originalFeatureCount && source.revision == 12,
                  "Tactical derivation must not mutate the parent map");
    return ok;
}

bool testOptionalEncounterTransfer()
{
    const MapModel source = makeSourceMap();
    EncounterModel encounter;
    encounter.metadata.id = "encounter-crossing";
    const EncounterAtlasTacticalResult result = EncounterAtlasService::deriveTacticalMap(source, makeRequest(), &encounter);

    bool ok = true;
    ok &= require(result.valid, "Derivation with an encounter target should succeed");
    ok &= require(encounter.sourceMapId == source.metadata.id && encounter.sourceMapRevision == source.revision &&
                      encounter.sourceMapRect.x == makeRequest().sourceRect.x,
                  "Encounter should record immutable source excerpt provenance");
    ok &= require(encounter.tacticalMapId && *encounter.tacticalMapId == result.tacticalMap.metadata.id &&
                      encounter.mapId == result.tacticalMap.metadata.id,
                  "Encounter should reference the derived tactical map");
    ok &= require(encounter.battlefieldWidth == 4 && encounter.battlefieldHeight == 4 && encounter.battlefieldCells.size() == 16 && encounter.useMapCells,
                  "Encounter transfer should install the tactical battlefield cells and dimensions");
    return ok;
}

bool testInvalidInputsDoNotMutateEncounter()
{
    MapModel source = makeSourceMap();
    EncounterModel encounter;
    encounter.mapId = "unchanged";
    encounter.battlefieldWidth = 9;
    EncounterAtlasTacticalRequest request = makeRequest();
    request.sourceRect = {0.9f, 0.9f, 0.2f, 0.2f};
    const EncounterAtlasTacticalResult badRect = EncounterAtlasService::deriveTacticalMap(source, request, &encounter);

    bool ok = true;
    ok &= require(!badRect.valid && !badRect.errors.empty(), "Out-of-range normalized excerpts should be rejected");
    ok &= require(encounter.mapId == "unchanged" && encounter.battlefieldWidth == 9 && !encounter.tacticalMapId,
                  "Invalid derivation must not partially mutate an encounter");
    source.atlasLevel = MapLevel::World;
    ok &= require(!EncounterAtlasService::deriveTacticalMap(source, makeRequest()).valid,
                  "World maps should not derive tactical encounters directly");
    request = makeRequest();
    request.width = 0;
    source.atlasLevel = MapLevel::Site;
    ok &= require(!EncounterAtlasService::deriveTacticalMap(source, request).valid,
                  "Non-positive tactical dimensions should be rejected");
    return ok;
}
}

int main()
{
    bool ok = true;
    ok &= testDeterministicDerivationAndProvenance();
    ok &= testOptionalEncounterTransfer();
    ok &= testInvalidInputsDoNotMutateEncounter();
    return ok ? 0 : 1;
}
