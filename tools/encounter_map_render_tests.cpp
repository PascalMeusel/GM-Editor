#include "Models/Combat/EncounterMapRenderSource.hpp"
#include <iostream>

int main()
{
    int failures = 0;
    auto check = [&](bool passed, const char *message) {
        if (!passed) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
    };
    MapModel map;
    map.metadata.id = "bound-tactical";
    map.atlasLevel = MapLevel::Tactical;
    map.mapKind = MapKind::Encounter;
    map.scope = MapScope::Local;
    map.width = 4; map.height = 3;
    map.cells.resize(12);
    map.cells[4].kind = MapTileKind::Mountain;
    map.cells[4].blocksLineOfSight = true;
    map.cells[4].fogged = true;
    EncounterModel encounter;
    encounter.useMapCells = true;
    encounter.tacticalMapId = map.metadata.id;
    encounter.mapId = "unrelated-world";
    encounter.battlefieldWidth = 4; encounter.battlefieldHeight = 3;
    encounter.battlefieldCells = map.cells;
    MapModel unrelated = map; unrelated.metadata.id = "unrelated-world";
    std::vector<MapModel> maps{unrelated, map};
    check(encounterMapRenderSource(encounter, maps) == &maps[1], "bound tactical ID beats unrelated dropdown/world map");
    auto changed = maps;
    changed[1].cells[4].kind = MapTileKind::Empty;
    check(!encounterMapRenderSource(encounter, changed), "changed terrain cannot replace committed encounter grid");
    changed = maps; changed[1].cells[4].fogged = false;
    check(!encounterMapRenderSource(encounter, changed), "fog mismatch falls back to encounter snapshot");
    changed = maps; changed[1].cells[4].lineOfSightOneWayBlockedMask = 1;
    check(!encounterMapRenderSource(encounter, changed), "directional LOS mismatch rejected");
    changed = maps; changed[1].width = 6; changed[1].height = 2;
    check(!encounterMapRenderSource(encounter, changed), "equal-area different geometry rejected");
    changed = maps; changed.erase(changed.begin() + 1);
    check(!encounterMapRenderSource(encounter, changed), "missing tactical link cannot fall through to world map");
    encounter.tacticalMapId.reset(); encounter.mapId = map.metadata.id;
    check(encounterMapRenderSource(encounter, maps) == &maps[1], "legacy exact map ID resolves");
    MapModel city = map;
    city.metadata.id = "city-map";
    city.atlasLevel = MapLevel::Site;
    city.mapKind = MapKind::Settlement;
    encounter.mapId = city.metadata.id;
    encounter.tacticalMapId = city.metadata.id;
    encounter.battlefieldCells = city.cells;
    std::vector<MapModel> cityMaps{city};
    check(isEncounterScaleMap(city), "city maps are eligible encounter battlefields");
    check(encounterMapRenderSource(encounter, cityMaps) == &cityMaps[0], "city map art resolves for combat");
    MapModel dungeon = city;
    dungeon.metadata.id = "dungeon-map";
    dungeon.mapKind = MapKind::Dungeon;
    dungeon.scope = MapScope::Dungeon;
    check(isEncounterScaleMap(dungeon), "dungeon maps are eligible encounter battlefields");
    MapModel world = city;
    world.metadata.id = "world-map";
    world.atlasLevel = MapLevel::World;
    world.mapKind = MapKind::Geographic;
    check(!isEncounterScaleMap(world), "world maps are excluded from encounter selection");
    encounter.tacticalMapId = map.metadata.id;
    encounter.mapId = map.metadata.id;
    encounter.battlefieldCells = map.cells;
    encounter.theaterOfMind = true;
    check(!encounterMapRenderSource(encounter, maps), "narrative combat does not resolve grid art");
    encounter.theaterOfMind = false;
    const auto request = encounterGameMasterArtRequest("en");
    check(request.audience == MapRenderAudience::GameMaster && !request.applyFog, "base request explicitly GM-only");
    check((request.layers & (MapRenderPlanTokens | MapRenderPlanFog | MapRenderPlanGrid)) == 0, "encounter overlays not duplicated by atlas plan");
    const auto plan = compileMapRenderPlan(map, request);
    check(mapRenderPlanMatchesMap(plan, map), "shared plan retains exact map identity and geometry");
    check(map.cells[4].fogged && encounter.battlefieldCells[4].fogged, "compilation does not modify either fog snapshot");
    check(plan.audience == MapRenderAudience::GameMaster, "combat plan cannot be mistaken for player approval");
    std::cout << "Encounter map render contracts: " << failures << " failures\n";
    return failures ? 1 : 0;
}
