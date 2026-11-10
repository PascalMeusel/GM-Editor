#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/MapAtlasWaterInk.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapGouacheAtlasInk.hpp"
#include <iostream>
#include <random>

namespace
{
bool sameInk(const std::vector<gm::map_render::AtlasInkPrimitive> &left,
             const std::vector<gm::map_render::AtlasInkPrimitive> &right)
{
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        const auto &a = left[index];
        const auto &b = right[index];
        if (a.filled != b.filled || a.strokeCells != b.strokeCells ||
            a.color.r != b.color.r || a.color.g != b.color.g ||
            a.color.b != b.color.b || a.color.a != b.color.a ||
            a.points.size() != b.points.size()) return false;
        if (a.vertexColors.size()!=b.vertexColors.size()) return false;
        for(std::size_t i=0;i<a.vertexColors.size();++i) {
            const auto ac=a.vertexColors[i],bc=b.vertexColors[i];
            if(ac.r!=bc.r||ac.g!=bc.g||ac.b!=bc.b||ac.a!=bc.a)return false;
        }
        for (std::size_t point = 0; point < a.points.size(); ++point)
            if (a.points[point].x != b.points[point].x || a.points[point].y != b.points[point].y)
                return false;
    }
    return true;
}

bool samePoints(const std::vector<MapFeaturePoint> &left, const std::vector<MapFeaturePoint> &right)
{
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i)
        if (left[i].x != right[i].x || left[i].y != right[i].y) return false;
    return true;
}

// The interactive/PNG river contract is three endpoint-preserving quarter
// cuts. Keep this independent of the shared resolver: the former artwork
// used two 28% cuts, which drifted away from the rendered bank at a bend.
std::vector<MapFeaturePoint> expectedRiverPoints(std::vector<MapFeaturePoint> points)
{
    for (int pass = 0; pass < 3; ++pass)
    {
        std::vector<MapFeaturePoint> next{points.front()};
        for (std::size_t i = 1; i < points.size(); ++i)
        {
            const auto a = points[i-1], b = points[i];
            next.push_back({a.x*.75f+b.x*.25f,a.y*.75f+b.y*.25f});
            next.push_back({a.x*.25f+b.x*.75f,a.y*.25f+b.y*.75f});
        }
        next.push_back(points.back());
        points.swap(next);
    }
    return points;
}
}

int main()
{
    using namespace gm::map_render;
    int failures = 0;
    const auto check = [&](bool ok, const char *message) { if (!ok) { std::cerr << message << '\n'; ++failures; } };
    MapModel map;
    map.width = 32; map.height = 24; map.atlasLevel = MapLevel::World;
    map.cells.resize(map.width * map.height);
    map.generation.continuousRendering = true;
    map.generation.recipe.styleId = kIllustratedAtlasStyleId;
    map.generation.recipe.styleVersion = 1;
    map.generation.recipe.styleSnapshotId = kIllustratedAtlasSnapshotId;
    MapGeneratedFeature ridge;
    ridge.id = "ridge"; ridge.kind = MapGeneratedFeatureKind::MountainRange;
    ridge.points = {{4,12},{28,12}}; ridge.discovered = true;
    map.generation.graph.features.push_back(ridge);
    const auto ink = buildIllustratedAtlasInk(map, {});
    check(!ink.empty(), "World atlas must actually contain ridge artwork");
    MapGeneratedFeature road;
    road.id = "manual_way_pass"; road.kind = MapGeneratedFeatureKind::Road;
    road.points = {{16,5},{16,19}}; road.locked = true; road.discovered = true;
    map.generation.graph.features.push_back(road);
    const auto corridors = illustratedAtlasRoadCorridors(map);
    const auto passInk = buildIllustratedAtlasInk(map, corridors);
    check(!passInk.empty() && passInk.size() < ink.size(), "A crossing must open a pass while retaining the range");
    for (const auto &primitive : passInk)
    {
        if (primitive.filled)
            check(atlasSilhouetteClearsRoads(primitive.points, corridors), "A filled mountain face intersects the pass");
        for (std::size_t i = 1; i < primitive.points.size(); ++i)
            check(!gm::map_geometry::roadCorridorIntersectsSegment(corridors.front(), primitive.points[i-1], primitive.points[i], primitive.strokeCells*.5f),
                "Mountain ink intersects a final road shoulder");
    }
    const std::vector<MapFeaturePoint> enclosing{{15,9},{17,9},{17,15},{15,15}};
    gm::map_geometry::MapRoadCorridor shortRoad{"short",{{15.9f,11.9f},{16.1f,12.1f}},.1f};
    check(!atlasSilhouetteClearsRoads(enclosing,{shortRoad}), "A short road wholly inside a silhouette must be detected");
    map.atlasLevel = MapLevel::Region;
    check(!buildIllustratedAtlasInk(map,corridors).empty(), "Region maps also need illustrated ranges");
    MapRenderPlanRequest request;
    request.applyFog = false;
    const auto plan = compileMapRenderPlan(map, request);
    check(!plan.atlasInk.empty(), "RenderPlan must own resolved artwork");
    check(mapRenderPlanContentMatchesMap(plan,map), "Unchanged art plan must remain valid");
    const auto firstHash = plan.contentHash;
    check(firstHash == compileMapRenderPlan(map,request).contentHash, "Art placement must be deterministic");
    map.generation.recipe.styleId = "fantasy_neutral";
    check(!usesIllustratedAtlas(map), "Saved production style must not opt into new artwork");
    check(!mapRenderPlanMatchesMap(plan,map), "Changing styleId alone must invalidate cached art");
    check(compileMapRenderPlan(map,request).atlasInk.empty(), "Legacy pins must preserve their rendering path");
    // Revoked road consent must not leave a revealing pass in player relief.
    map.generation.recipe.styleId = kIllustratedAtlasStyleId;
    map.visibility = MapVisibility::PlayersVisible;
    request.audience = MapRenderAudience::Player;
    map.playerRelease.waysAndRoutes = false;
    const auto hiddenRoadPlan = compileMapRenderPlan(map, request);
    map.generation.graph.features.pop_back();
    const auto absentRoadPlan = compileMapRenderPlan(map, request);
    check(hiddenRoadPlan.atlasInk.size() == absentRoadPlan.atlasInk.size(), "Hidden roads must not influence player art");
    // A named road intentionally survives the feature allow-list for its
    // released label. That is not consent to expose its route through gaps in
    // mountain or forest artwork. Compare complete geometry/color, not counts.
    road.name = "Named mountain pass";
    road.gmOnly = false;
    road.discovered = true;
    map.playerRelease.labels = true;
    for (const MapLevel level : {MapLevel::World, MapLevel::Region})
    {
        map.atlasLevel = level;
        map.playerRelease.waysAndRoutes = false;
        const auto noRoad = compileMapRenderPlan(map, request);
        map.generation.graph.features.push_back(road);
        const auto namedLabelOnly = compileMapRenderPlan(map, request);
        check(mapRenderPlanContainsGeneratedFeature(namedLabelOnly, road.id) &&
                  !mapRenderPlanContainsGeneratedGeometry(namedLabelOnly, road.id) &&
                  std::any_of(namedLabelOnly.primitives.begin(), namedLabelOnly.primitives.end(), [&](const auto &primitive) {
                      return primitive.kind == MapRenderPrimitiveKind::Label && primitive.sourceId == road.id &&
                             primitive.label == road.name;
                  }),
              "Named-road regression must retain the approved label while Ways geometry is withheld");
        check(!noRoad.atlasInk.empty() && sameInk(namedLabelOnly.atlasInk, noRoad.atlasInk),
              "A labels-only road must leave exactly the same player artwork as an absent road");
        map.playerRelease.waysAndRoutes = true;
        const auto releasedWays = compileMapRenderPlan(map, request);
        check(mapRenderPlanContainsGeneratedGeometry(releasedWays, road.id) &&
                  !releasedWays.atlasInk.empty() && !sameInk(releasedWays.atlasInk, noRoad.atlasInk),
              "Explicit Ways release must open the actual crossing while retaining the surrounding illustrated range");
        map.generation.graph.features.pop_back();
    }
    // A new art revision is deliberate and must preserve the old resolver.
    map.atlasLevel = MapLevel::World;
    request.audience = MapRenderAudience::GameMaster;
    const auto legacyArt = compileMapRenderPlan(map,request);
    map.generation.recipe.styleVersion = 2;
    map.generation.recipe.styleSnapshotId = kIllustratedAtlasV2SnapshotId;
    check(usesIllustratedAtlasV2(map), "The exact V2 pin must resolve new atlas artwork");
    const auto revisedArt = compileMapRenderPlan(map,request);
    check(!revisedArt.atlasInk.empty() && !sameInk(legacyArt.atlasInk,revisedArt.atlasInk) &&
              revisedArt.contentHash != legacyArt.contentHash,
          "V2 must have distinct, cache-separated artwork");
    const auto originalHeight = illustratedAtlasElevation(map,7.25f,9.5f);
    const auto originalWash = illustratedAtlasTerrainColor(map,7.25f,9.5f);
    for (auto &cell : map.cells) cell.elevation = 0;
    map.cells[9*map.width+7].elevation = 99;
    const auto alteredWash = illustratedAtlasTerrainColor(map,7.25f,9.5f);
    check(originalHeight == illustratedAtlasElevation(map,7.25f,9.5f) &&
              originalWash.r == alteredWash.r && originalWash.g == alteredWash.g && originalWash.b == alteredWash.b,
          "Private road/place gameplay-height stamps must not change the public terrain wash");
    ++map.generation.recipe.seed;
    check(originalHeight != illustratedAtlasElevation(map,7.25f,9.5f), "The terrain wash must describe the saved generated relief");
    --map.generation.recipe.seed;
    for (const auto level : {MapLevel::World,MapLevel::Region})
    {
        map.atlasLevel = level;
        for (int y = 2; y < 20; ++y)
            for (int x = 2; x < 30; ++x)
                map.cells[y*map.width+x].kind = MapTileKind::Forest;
        map.generation.graph.features.push_back(road);
        const auto v2Corridors = illustratedAtlasRoadCorridors(map);
        const auto woodedPass = buildIllustratedAtlasInk(map,v2Corridors);
        check(!woodedPass.empty() && sameInk(woodedPass,buildIllustratedAtlasInk(map,v2Corridors)),
              "V2 woodland and relief must be nonempty and deterministic in both scales");
        for (const auto &primitive : woodedPass)
        {
            if (primitive.filled)
                check(atlasSilhouetteClearsRoads(primitive.points,v2Corridors), "V2 fill crosses the full road shoulder");
            for (std::size_t i = 1; i < primitive.points.size(); ++i)
                check(!gm::map_geometry::roadCorridorIntersectsSegment(v2Corridors.front(),primitive.points[i-1],primitive.points[i],primitive.strokeCells*.5f),
                      "V2 relief or canopy ink crosses the road shoulder");
        }
        request.audience = MapRenderAudience::Player;
        map.playerRelease.waysAndRoutes = false;
        const auto privateRoad = compileMapRenderPlan(map,request);
        map.generation.graph.features.pop_back();
        check(sameInk(privateRoad.atlasInk,compileMapRenderPlan(map,request).atlasInk),
              "V2 player artwork must not reveal a private road through canopy/relief gaps");
    }
    // Test the final bank geometry, not just the authored control polygon.
    // The bend passes through both woodland and an illustrated mountain range.
    MapModel riverMap = map;
    riverMap.generation.graph.features = {ridge};
    for (auto &cell : riverMap.cells) cell.kind = MapTileKind::Empty;
    for (int y = 3; y < 22; ++y)
        for (int x = 4; x < 30; ++x)
            if (x < 11 || y >= 18) riverMap.cells[y*riverMap.width+x].kind = MapTileKind::Forest;
    MapGeneratedFeature river;
    river.id = "bent_public_river"; river.kind = MapGeneratedFeatureKind::River;
    river.name = "Bent river"; river.points = {{7,3},{7,21},{29,21}};
    river.locked = true; river.discovered = true; river.width = 3.f;
    const auto expectedRiver = expectedRiverPoints(river.points);
    const gm::map_geometry::MapRoadCorridor riverBank{
        river.id, expectedRiver, std::max(.62f,river.width*.12f+.24f)};
    MapRenderPlanRequest riverRequest;
    riverRequest.audience = MapRenderAudience::Player;
    riverRequest.layers = MapRenderPlanTerrain | MapRenderPlanWater | MapRenderPlanLabels;
    riverMap.playerRelease.labels = true;
    for (const auto level : {MapLevel::World,MapLevel::Region})
    {
        riverMap.atlasLevel = level;
        const auto absentRiver = compileMapRenderPlan(riverMap,riverRequest);
        riverMap.generation.graph.features.push_back(river);
        const auto publicRiver = compileMapRenderPlan(riverMap,riverRequest);
        const auto *resolvedRiver = mapRenderPlanGeneratedGeometry(publicRiver,river.id);
        check(resolvedRiver && samePoints(*resolvedRiver,expectedRiver) &&
                  samePoints(illustratedAtlasFinalRiverPoints(riverMap,river),expectedRiver),
              "V2 plan and artwork must use the actual three-pass quarter-cut river geometry");
        check(!publicRiver.atlasInk.empty() && !sameInk(publicRiver.atlasInk,absentRiver.atlasInk),
              "A released river must open a real valley through existing woodland/relief");
        bool clearsRiver = true;
        for (const auto &primitive : publicRiver.atlasInk)
        {
            if (primitive.filled)
                clearsRiver = clearsRiver && atlasSilhouetteClearsRoads(primitive.points,{riverBank},0.f);
            for (std::size_t i = 1; i < primitive.points.size(); ++i)
                clearsRiver = clearsRiver && !gm::map_geometry::roadCorridorIntersectsSegment(
                    riverBank,primitive.points[i-1],primitive.points[i],primitive.strokeCells*.5f);
        }
        check(clearsRiver,"Every V2 canopy, mountain face, fold and hatch must clear the final river bank");
        auto labelsOnlyRequest = riverRequest;
        labelsOnlyRequest.layers &= ~MapRenderPlanWater;
        const auto labelsOnlyRiver = compileMapRenderPlan(riverMap,labelsOnlyRequest);
        check(mapRenderPlanContainsGeneratedFeature(labelsOnlyRiver,river.id) &&
                  !mapRenderPlanContainsGeneratedGeometry(labelsOnlyRiver,river.id),
              "The river layer fixture must retain its released label without water geometry");
        riverMap.generation.graph.features.back().gmOnly = true;
        const auto privateRiver = compileMapRenderPlan(riverMap,riverRequest);
        riverMap.generation.graph.features.pop_back();
        check(sameInk(labelsOnlyRiver.atlasInk,compileMapRenderPlan(riverMap,labelsOnlyRequest).atlasInk),
              "A labels-only river must not leave a revealing valley in player artwork");
        check(!mapRenderPlanContainsGeneratedFeature(privateRiver,river.id) &&
                  sameInk(privateRiver.atlasInk,absentRiver.atlasInk),
              "A private river must have exactly the same player artwork as an absent river");
    }
    // Forest-only terrain has no relief-dependent rolling marks, so changing
    // the saved landscape can alter its wash while all ink remains identical.
    // Both texture and preview/review caches must still notice that change.
    MapModel washMap = map;
    washMap.generation.graph.features.clear();
    for (auto &cell : washMap.cells) cell.kind = MapTileKind::Forest;
    washMap.generation.recipe.landscape = MapLandscapeProfile::Highlands;
    const auto highlandWash = illustratedAtlasTerrainColor(washMap,7.25f,9.5f);
    const auto highlandHash = overviewTerrainContentHash(washMap);
    const auto highlandPlan = compileMapRenderPlan(washMap,request);
    washMap.generation.recipe.landscape = MapLandscapeProfile::RiverValley;
    const auto valleyWash = illustratedAtlasTerrainColor(washMap,7.25f,9.5f);
    const auto valleyPlan = compileMapRenderPlan(washMap,request);
    check(sameInk(highlandPlan.atlasInk,valleyPlan.atlasInk) &&
              (highlandWash.r != valleyWash.r || highlandWash.g != valleyWash.g || highlandWash.b != valleyWash.b),
          "Landscape cache fixture must change the terrain wash without changing artwork");
    check(highlandHash != overviewTerrainContentHash(washMap) &&
              highlandPlan.contentHash != valleyPlan.contentHash && !mapRenderPlanMatchesMap(highlandPlan,washMap),
          "Changing the V2 relief landscape must invalidate texture and render-plan caches");
    // V3 must retain exact confluences after smoothing and enforce the same
    // privacy/road-clearance contract as the earlier illustrated snapshots.
    map.generation.recipe.styleVersion = 3;
    map.generation.recipe.styleSnapshotId = "illustrated_atlas_v3";
    map.generation.recipe.generatorVersion = 8;
    map.generation.graph.features.clear();
    MapGeneratedFeature trunk;
    trunk.id="trunk"; trunk.kind=MapGeneratedFeatureKind::River;
    trunk.points={{3,3},{8,8},{12,9},{20,16},{29,19}}; trunk.discovered=true;
    MapGeneratedFeature tributary=trunk;
    tributary.id="tributary"; tributary.points={{20,3},{16,6},{10,8.5f}};
    map.generation.graph.features={trunk,tributary,road};
    const auto resolvedTrunk=illustratedAtlasFinalRiverPoints(map,trunk);
    const auto resolvedBranch=illustratedAtlasFinalRiverPoints(map,tributary);
    check(std::any_of(resolvedTrunk.begin(),resolvedTrunk.end(),[&](const auto p){
        return p.x==resolvedBranch.back().x && p.y==resolvedBranch.back().y;
    }),"V3 smoothing must preserve an exact river confluence inside a receiving segment");
    check(resolvedTrunk.front().x==trunk.points.front().x && resolvedTrunk.back().y==trunk.points.back().y,
          "V3 river endpoints must remain exact");
    request.audience=MapRenderAudience::Player; map.visibility=MapVisibility::PlayersVisible;
    map.playerRelease.waysAndRoutes=false; map.playerRelease.labels=true;
    road.name="Secret pass"; map.generation.graph.features.back()=road;
    const auto v3HiddenRoad=compileMapRenderPlan(map,request);
    map.generation.graph.features.pop_back();
    check(sameInk(v3HiddenRoad.atlasInk,compileMapRenderPlan(map,request).atlasInk),
          "V3 hidden roads must not influence player forest or mountain placement");
    request.audience=MapRenderAudience::GameMaster;
    map.generation.graph.features.push_back(road);
    const auto v3Corridors=illustratedAtlasRoadCorridors(map);
    const auto v3Art=buildIllustratedAtlasInk(map,v3Corridors);
    check(!v3Art.empty() && sameInk(v3Art,buildIllustratedAtlasInk(map,v3Corridors)),
          "V3 artwork must be nonempty and deterministic");
    for (const auto &primitive:v3Art)
    {
        if (primitive.filled)
            check(atlasSilhouetteClearsRoads(primitive.points,v3Corridors),"V3 filled artwork overlaps a road corridor");
        else
            for (const auto &corridor:v3Corridors)
                for (std::size_t i=1;i<primitive.points.size();++i)
                    check(!gm::map_geometry::roadCorridorIntersectsSegment(corridor,
                        primitive.points[i-1],primitive.points[i],primitive.strokeCells*.5f),
                        "V3 stroke overlaps a road corridor");
    }
    // Atlas symbol artwork must obey the same audience and category contract
    // as the underlying settlement/bridge, including label-only publication.
    MapModel symbols = map;
    symbols.generation.graph.features.clear();
    for (auto &cell : symbols.cells) cell.kind = MapTileKind::Empty;
    MapGeneratedFeature town;
    town.id="town"; town.kind=MapGeneratedFeatureKind::Settlement;
    town.name="Hidden town"; town.points={{8.5f,8.5f}};
    town.materialId="fantasy_neutral.settlement.city"; town.discovered=true;
    symbols.cells[8*symbols.width+8].kind=MapTileKind::Settlement;
    symbols.generation.graph.features.push_back(town);
    MapGeneratedFeature bridge;
    bridge.id="bridge"; bridge.kind=MapGeneratedFeatureKind::Bridge;
    bridge.points={{16,10},{19,10}}; bridge.discovered=true;
    symbols.generation.graph.features.push_back(bridge);
    MapRenderPlanRequest symbolRequest;
    symbolRequest.applyFog=false;
    const auto symbolPlan=compileMapRenderPlan(symbols,symbolRequest);
    check(!symbolPlan.atlasFeatureInk.empty() &&
          sameInk(symbolPlan.atlasFeatureInk,compileMapRenderPlan(symbols,symbolRequest).atlasFeatureInk),
          "V3 settlements and bridges must have deterministic shared artwork");
    auto changedSymbols=symbols;
    changedSymbols.generation.graph.features.front().materialId="fantasy_neutral.settlement.village";
    check(!mapRenderPlanContentMatchesMap(symbolPlan,changedSymbols),
          "Changing the town silhouette must invalidate reviewed publication");
    symbolRequest.layers=MapRenderPlanLabels;
    check(compileMapRenderPlan(symbols,symbolRequest).atlasFeatureInk.empty(),
          "Labels-only plans must not contain bridge or town geometry");
    symbolRequest.layers=MapRenderPlanAll;
    symbolRequest.audience=MapRenderAudience::Player;
    symbols.playerRelease.waysAndRoutes=true;
    symbols.playerRelease.placesAndMarkers=true;
    symbols.generation.graph.features.front().gmOnly=true;
    symbols.generation.graph.features.back().gmOnly=true;
    check(compileMapRenderPlan(symbols,symbolRequest).atlasFeatureInk.empty(),
          "Private bridge and settlement glyphs must not leak into player art");
    auto tactical=map;
    tactical.atlasLevel=MapLevel::Tactical;
    tactical.generation.recipe.styleId="fantasy_neutral";
    tactical.generation.graph.features={trunk};
    const auto tacticalPlan=compileMapRenderPlan(tactical,symbolRequest);
    const auto detailedRiver=std::find_if(tacticalPlan.primitives.begin(),tacticalPlan.primitives.end(),
        [](const auto &p){return p.sourceId=="trunk" && p.kind==MapRenderPrimitiveKind::GeneratedGeometry;});
    check(detailedRiver!=tacticalPlan.primitives.end() && !detailedRiver->detailInk.empty(),
          "V8 tactical water depth/current strokes must be resolved by the shared plan");
    tactical.generation.graph.features.front().gmOnly=true;
    const auto privateTactical=compileMapRenderPlan(tactical,symbolRequest);
    check(std::none_of(privateTactical.primitives.begin(),privateTactical.primitives.end(),
        [](const auto &p){return !p.detailInk.empty();}),
        "Private rivers must not leave tactical water detail in a player plan");
    // V4 is an explicit art opt-in. Its taller silhouettes still obey the
    // same public-layer and full-footprint clearance contract as V3.
    auto fantasy = map;
    fantasy.generation.recipe.styleVersion = 4;
    fantasy.generation.recipe.styleSnapshotId = "illustrated_atlas_v4";
    check(usesIllustratedAtlasV4(fantasy) && !usesIllustratedAtlasV3(fantasy),
          "The V4 atlas pin must not alias the immutable V3 style");
    const auto fantasyCorridors = illustratedAtlasRoadCorridors(fantasy);
    const auto fantasyArt = buildIllustratedAtlasInk(fantasy, fantasyCorridors);
    check(!fantasyArt.empty() && sameInk(fantasyArt, buildIllustratedAtlasInk(fantasy, fantasyCorridors)),
          "V4 terrain artwork must be deterministic");
    for (const auto &primitive : fantasyArt)
    {
        if (primitive.filled)
            check(atlasSilhouetteClearsRoads(primitive.points, fantasyCorridors),
                  "V4 snow, mountain and canopy fills must clear the full road corridor");
        else
            for (const auto &corridor : fantasyCorridors)
                for (std::size_t i = 1; i < primitive.points.size(); ++i)
                    check(!gm::map_geometry::roadCorridorIntersectsSegment(corridor,
                        primitive.points[i-1], primitive.points[i], primitive.strokeCells * .5f),
                        "V4 terrain strokes must clear the full road corridor");
    }
    symbols.generation.recipe.styleVersion = 4;
    symbols.generation.recipe.styleSnapshotId = "illustrated_atlas_v4";
    MapGeneratedFeature secretSite;
    secretSite.id="secret-site";
    secretSite.kind=MapGeneratedFeatureKind::Landmark;
    secretSite.name="A deliberately secret landmark";
    secretSite.points={{12.5f,12.5f}};
    secretSite.gmOnly=true;
    secretSite.discovered=true;
    symbols.generation.graph.features.push_back(secretSite);
    symbols.cells[12*symbols.width+12].kind=MapTileKind::Landmark;
    check(compileMapRenderPlan(symbols, symbolRequest).atlasFeatureInk.empty(),
          "V4 spires, rosettes and pennants must not reveal private sites or bridges");
    auto oceanSite=symbols;
    oceanSite.generation.graph.features.clear();
    for (auto &cell:oceanSite.cells) cell.kind=MapTileKind::Water;
    oceanSite.cells[12*oceanSite.width+12].kind=MapTileKind::Landmark;
    check(buildIllustratedAtlasFeatureInk(oceanSite,false,true).empty(),
          "V4 landmarks suppressed over ocean must not reappear as unlabelled rosettes");
    for (auto &cell:oceanSite.cells) cell.kind=MapTileKind::Empty;
    oceanSite.cells[12*oceanSite.width+12].kind=MapTileKind::Landmark;
    check(!buildIllustratedAtlasFeatureInk(oceanSite,false,true).empty(),
          "V4 land-based landmarks must retain their illustrated marker");
    symbolRequest.audience = MapRenderAudience::GameMaster;
    symbolRequest.layers = MapRenderPlanLabels;
    check(compileMapRenderPlan(symbols, symbolRequest).atlasFeatureInk.empty(),
          "V4 labels-only export must not contain symbol art");
    MapGeneratedFeature edgeRiver;
    edgeRiver.kind = MapGeneratedFeatureKind::River;
    edgeRiver.width = 1.65f;
    edgeRiver.points = {{10.5f,.5f},{10.5f,static_cast<float>(tactical.height)-.5f}};
    tactical.generation.recipe.generatorVersion = 9;
    const auto edgePoints = tacticalRiverDetailPoints(tactical, edgeRiver);
    check(edgePoints.front().y < 0 && edgePoints.back().y > tactical.height,
          "V9 edge-reaching rivers must continue beyond the clipped viewport without round end caps");
    edgeRiver.points = {{10.5f,3.5f},{10.5f,static_cast<float>(tactical.height)-3.5f}};
    const auto inlandPoints = tacticalRiverDetailPoints(tactical, edgeRiver);
    check(inlandPoints.front().y == edgeRiver.points.front().y && inlandPoints.back().y == edgeRiver.points.back().y,
          "V9 river artwork must not extend inland sources or mouths");
    tactical.generation.recipe.generatorVersion = 8;
    edgeRiver.points = {{10.5f,.5f},{10.5f,static_cast<float>(tactical.height)-.5f}};
    const auto legacyEdgePoints = tacticalRiverDetailPoints(tactical, edgeRiver);
    check(legacyEdgePoints.front().y == .5f && legacyEdgePoints.back().y == tactical.height-.5f,
          "Existing V8 rivers must retain their exact display endpoints");
    auto v5=fantasy;v5.generation.recipe.styleVersion=5;v5.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
    check(usesIllustratedAtlasV5(v5) && !usesIllustratedAtlasV4(v5),"V5 must be distinct from immutable V4");
    const auto v5Roads=illustratedAtlasRoadCorridors(v5);
    const auto v5Ink=buildIllustratedAtlasInk(v5,v5Roads);
    check(!v5Ink.empty() && sameInk(v5Ink,buildIllustratedAtlasInk(v5,v5Roads)),"V5 artwork must be deterministic");
    for(const auto &primitive:v5Ink)
    {
        if(primitive.filled)check(atlasSilhouetteClearsRoads(primitive.points,v5Roads),"V5 fill overlaps a visible road");
        else for(const auto &corridor:v5Roads)for(std::size_t i=1;i<primitive.points.size();++i)
            check(!gm::map_geometry::roadCorridorIntersectsSegment(corridor,primitive.points[i-1],primitive.points[i],primitive.strokeCells*.5f),
                "V5 stroke overlaps a visible road");
    }
    check(!illustratedAtlasLabelClearsOwnSymbol({10,10},8,9.8f,12,10.8f,false),
        "A clamped label must not cover its own landmark symbol");
    check(illustratedAtlasLabelClearsOwnSymbol({10,10},8,7,12,8.5f,true),
        "A conventional above-castle label must remain available");
    check(illustratedAtlasLabelLeader({10,10},8,7,12,8.8f).empty(),"Normal above-symbol names do not need a leader");
    check(illustratedAtlasLabelLeader({10,10},12,11,16,12).size()==2,"Displaced names must retain a visible link to their symbol");
    auto secretV5=symbols;secretV5.generation.recipe.styleVersion=5;secretV5.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
    symbolRequest.audience=MapRenderAudience::Player;symbolRequest.layers=MapRenderPlanAll;
    check(compileMapRenderPlan(secretV5,symbolRequest).atlasFeatureInk.empty(),
        "V5 private places must not disclose houses, cultivation parcels or landmark ink");
    // Region cultivation is a new recipe opt-in; it must not change World V5
    // or expose a private town through fields/orchards in player artwork.
    auto parcels = symbols;
    parcels.generation.graph.features = {town};
    // Remove the unrelated landmark cell from the earlier private-site fixture.
    // Leaving an unreferenced public landmark would legitimately emit its own ink.
    for (auto &cell : parcels.cells) cell.kind = MapTileKind::Empty;
    parcels.cells[8 * parcels.width + 8].kind = MapTileKind::Settlement;
    parcels.generation.graph.features.front().gmOnly = false;
    parcels.generation.recipe.styleVersion = 5;
    parcels.generation.recipe.styleSnapshotId = "illustrated_atlas_v5";
    parcels.generation.recipe.generatorVersion = 11;
    parcels.atlasLevel = MapLevel::World;
    const auto worldParcels = buildIllustratedAtlasFeatureInk(parcels,false,true);
    parcels.generation.recipe.generatorVersion = 12;
    check(sameInk(worldParcels,buildIllustratedAtlasFeatureInk(parcels,false,true)),
        "Regional cultivation must preserve World V5 geometry");
    parcels.atlasLevel = MapLevel::Region;
    parcels.generation.recipe.styleVersion = 6;
    parcels.generation.recipe.styleSnapshotId = "illustrated_atlas_v6";
    const auto regionalParcels = buildIllustratedAtlasFeatureInk(parcels,false,true);
    check(!regionalParcels.empty() && sameInk(regionalParcels,buildIllustratedAtlasFeatureInk(parcels,false,true)),
        "Regional fields and orchards must remain deterministic");
    parcels.generation.recipe.generatorVersion = 11;
    check(!sameInk(regionalParcels,buildIllustratedAtlasFeatureInk(parcels,false,true)),
        "Regional cultivation must require generation 12");
    parcels.generation.recipe.generatorVersion = 12;
    parcels.generation.graph.features.front().gmOnly = true;
    check(compileMapRenderPlan(parcels,symbolRequest).atlasFeatureInk.empty(),
        "A private regional town must not leave cultivated parcels or orchards");
    parcels.generation.graph.features.front().gmOnly = false;
    for(auto &cell:parcels.cells) cell.fogged=true;
    auto fogParcelRequest=symbolRequest;fogParcelRequest.applyFog=true;
    check(compileMapRenderPlan(parcels,fogParcelRequest).atlasFeatureInk.empty(),
        "Fogged regional towns must not reveal orchards or field geometry");
    // Standing-water mouths use the actual continuous shore, while inland
    // junctions and stored source geometry retain their exact coordinates.
    for (const auto level:{MapLevel::World,MapLevel::Region}) for (bool lake:{false,true})
    {
        MapModel outlet;
        outlet.width=40; outlet.height=30; outlet.atlasLevel=level;
        outlet.cells.resize(1200); outlet.generation.continuousRendering=true;
        outlet.generation.recipe.styleId="illustrated_atlas";
        outlet.generation.recipe.styleVersion=5;
        outlet.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
        outlet.generation.recipe.generatorVersion=11;
        for (int y=0;y<30;++y) for(int x=0;x<40;++x)
            outlet.cells[y*40+x].kind=x>=20 && (!lake || (x<33 && y>6 && y<24))?MapTileKind::Water:MapTileKind::Empty;
        MapGeneratedFeature flow;
        flow.id="outlet"; flow.kind=MapGeneratedFeatureKind::River; flow.width=1.2f;
        flow.points={{5,15},{12,15},{22,15}}; flow.discovered=true;
        outlet.generation.graph.features={flow};
        auto final=illustratedAtlasFinalRiverPoints(outlet,flow);
        check(final.front().x==5.f && final.front().y==15.f && final.back().x>19.f && final.back().x<21.f,
            "World/Region ocean and lake mouths must end at the continuous shoreline without moving the source");
        check(outlet.generation.graph.features.front().points.back().x==22.f,
            "Mouth presentation must not rewrite saved river geometry");
        const auto mouthInk=buildIllustratedAtlasRiverMouthInk(outlet,flow);
        check(!mouthInk.empty() && sameInk(mouthInk,buildIllustratedAtlasRiverMouthInk(outlet,flow)),
            "A standing-water mouth needs deterministic estuary ink");
        check(!mouthInk.empty() && std::all_of(mouthInk.begin(),mouthInk.end(),[](const auto &p){return p.vertexColors.size()==3;}),
            "Estuary must interpolate shared vertex colors rather than draw independent flat bands");
        float seaward=-1.f;
        for(const auto &p:mouthInk)for(const auto v:p.points) seaward=std::max(seaward,v.x);
        for(const auto &p:mouthInk)for(std::size_t i=0;i<p.points.size();++i)
            if(std::abs(p.points[i].x-seaward)<.0001f)
                check(p.vertexColors[i].a==0,"Estuary must reach full transparency at its actual seaward edge");
        const auto continuousInk=buildIllustratedAtlasRiverInk(outlet,flow);
        MapRenderPlanRequest outletRequest; outletRequest.applyFog=false;
        const auto outletPlan=compileMapRenderPlan(outlet,outletRequest);
        const auto *planGeometry=mapRenderPlanGeneratedGeometry(outletPlan,flow.id);
        check(planGeometry && samePoints(*planGeometry,final),"Editor and export must share resolved mouth geometry");
        const auto primitive=std::find_if(outletPlan.primitives.begin(),outletPlan.primitives.end(),[&](const auto &p){
            return p.sourceId==flow.id && p.kind==MapRenderPrimitiveKind::GeneratedGeometry;
        });
        check(primitive!=outletPlan.primitives.end() && sameInk(primitive->detailInk,continuousInk),
            "Shared plan must contain the mouth transition ink");
        // A two-cell convex water corner can recede in the continuous
        // coastline, even though the semantic terminal is already in a lake.
        auto recessed=outlet;
        for(int y=14;y<=15;++y)for(int x=18;x<=19;++x) recessed.cells[y*40+x].kind=MapTileKind::Water;
        auto recessedFlow=flow; recessedFlow.points={{5,14.5f},{12,14.5f},{18.5f,14.5f}};
        recessed.generation.graph.features={recessedFlow};
        int recessedCases=0;
        for(unsigned seed=1;seed<=10;++seed)
        {
            recessed.generation.recipe.seed=seed;
            const auto projected=buildOverviewGroundProjection(recessed);
            std::vector<std::uint8_t> mask(projected.size());
            for(std::size_t i=0;i<mask.size();++i) mask[i]=overviewGroundKindIsLand(projected[i])?1:0;
            if(detail::overviewLandCoverageFromMask(recessed,mask,18.5f,14.5f)<.5f) continue;
            ++recessedCases;
            auto resolved=illustratedAtlasUntrimmedRiverPoints(recessed,recessedFlow);
            const auto recessedMouth=trimIllustratedAtlasRiverMouth(recessed,resolved);
            check(recessedMouth.joinsWater && resolved.back().x>18.5f && resolved.back().x<20.9f,
                "A standing-water endpoint hidden by coast smoothing must still reach its displayed lake");
            check(recessed.generation.graph.features.front().points.back().x==18.5f &&
                  resolved.front().x==5.f && !buildIllustratedAtlasRiverMouthInk(recessed,recessedFlow).empty(),
                "Resolving a recessed lake mouth must preserve the stored river and its inland source");
        }
        check(recessedCases>0,"Recessed lake fixture must exercise a semantic water terminal displayed as land");
        // Manual mouse-up may be many cells offshore. Keep that editable
        // endpoint, while clipping the whole visible shaft to its first shore.
        auto manualFlow=flow;manualFlow.id="manual_stroke_offshore";manualFlow.width=3.f;
        manualFlow.points={{5,15},{12,15},{30,15}};
        auto manualMap=outlet;manualMap.generation.graph.features={manualFlow};
        const auto manualFinal=illustratedAtlasFinalRiverPoints(manualMap,manualFlow);
        check(manualFinal.back().x>19.f && manualFinal.back().x<21.f && manualFlow.points.back().x==30.f,
            "A long manual river must reach its first standing-water shore without rewriting its saved offshore tail");
        const auto manualInk=buildIllustratedAtlasRiverInk(manualMap,manualFlow);
        check(!manualInk.empty(),"A long manual river retains its continuous visible mesh");
        for(const auto &p:manualInk)for(const auto point:p.points)
            check(point.x<=manualFinal.back().x+1.81f,
                "A manual river outlet must blend within a bounded reach of the shore, not extend to mouse-up");
        auto returningFlow=manualFlow;
        returningFlow.points={{5,15},{24,15},{12,15},{30,15}};
        manualMap.generation.graph.features={returningFlow};
        const auto returningFinal=illustratedAtlasFinalRiverPoints(manualMap,returningFlow);
        check(returningFinal.back().x>19.f && returningFinal.back().x<21.f &&
              std::all_of(returningFinal.begin(),returningFinal.end(),[](auto p){return p.x<21.f;}),
            "A manual stroke that leaves and reenters water must use its first mouth, not draw across the lake");
        flow.points.back()={15,15};
        const auto inland=illustratedAtlasFinalRiverPoints(outlet,flow);
        check(inland.back().x==15.f && inland.back().y==15.f && buildIllustratedAtlasRiverMouthInk(outlet,flow).empty(),
            "An inland confluence must not acquire a mouth or move");
        outlet.generation.graph.features={flow};
        const auto unjoinedInk=buildIllustratedAtlasRiverInk(outlet,flow);
        MapGeneratedFeature receiver=flow;receiver.id="receiving_channel";receiver.points={{15,10},{15,20}};
        outlet.generation.graph.features.push_back(receiver);
        const auto joinedInk=buildIllustratedAtlasRiverInk(outlet,flow);
        bool opaqueUnjoinedCap=false,opaqueJoinedCap=false;
        const auto capBank=[&](const auto &ink,bool &opaque) {
            for(const auto &p:ink)for(std::size_t i=0;i<p.points.size();++i)
                if(i<p.vertexColors.size() && p.points[i].x>15.001f && p.vertexColors[i].g==82 && p.vertexColors[i].a>20) opaque=true;
        };
        capBank(unjoinedInk,opaqueUnjoinedCap);capBank(joinedInk,opaqueJoinedCap);
        check(opaqueUnjoinedCap && !opaqueJoinedCap,"A tributary must not draw its dark end bank across the receiving channel");
        outlet.visibility=MapVisibility::PlayersVisible;
        outlet.generation.graph.features.back().gmOnly=true;
        MapRenderPlanRequest safeRequest;safeRequest.audience=MapRenderAudience::Player;
        safeRequest.layers=MapRenderPlanWater;safeRequest.applyFog=false;
        const auto hiddenReceiver=compileMapRenderPlan(outlet,safeRequest);
        outlet.generation.graph.features.pop_back();
        const auto absentReceiver=compileMapRenderPlan(outlet,safeRequest);
        const auto riverInk=[&](const auto &plan) {
            for(const auto &p:plan.primitives)if(p.sourceId==flow.id && p.kind==MapRenderPrimitiveKind::GeneratedGeometry) return p.detailInk;
            return std::vector<AtlasInkPrimitive>{};
        };
        check(!riverInk(hiddenReceiver).empty() && sameInk(riverInk(hiddenReceiver),riverInk(absentReceiver)),
            "A private receiving channel must not alter a public tributary cap");
        flow.points.back()={22,15}; outlet.generation.recipe.styleVersion=3;
        outlet.generation.recipe.styleSnapshotId="illustrated_atlas_v3";
        check(illustratedAtlasFinalRiverPoints(outlet,flow).back().x==22.f,
            "Historical illustrated snapshots must keep their river endpoints");
    }
    // Brush relief must compose a patch without introducing row baselines or
    // changing an existing authored range or any semantic terrain cell.
    {
        MapModel painted;painted.width=40;painted.height=30;painted.atlasLevel=MapLevel::World;
        painted.cells.resize(1200);painted.generation.continuousRendering=true;
        painted.generation.recipe.styleId="illustrated_atlas";
        painted.generation.recipe.styleVersion=5;
        painted.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
        painted.generation.recipe.seed=4129;
        for(auto &cell:painted.cells)cell.kind=MapTileKind::Dirt;
        MapGeneratedFeature locked;locked.id="locked_geography";locked.kind=MapGeneratedFeatureKind::MountainRange;
        locked.points={{3,5},{11,6},{17,5}};locked.locked=true;locked.discovered=true;
        painted.generation.graph.features.push_back(locked);
        const auto authored=buildOverviewRidgePaths(painted);
        for(int y=13;y<25;++y)for(int x=20;x<35;++x)painted.cells[y*40+x].kind=MapTileKind::Mountain;
        const auto before=painted.cells;
        const auto ridges=buildOverviewRidgePaths(painted),again=buildOverviewRidgePaths(painted);
        bool unchanged=!authored.empty() && !ridges.empty() && samePoints(authored.front().spine,ridges.front().spine) &&
            authored.front().marks.size()==ridges.front().marks.size();
        if(unchanged)for(std::size_t i=0;i<authored.front().marks.size();++i)
            unchanged=unchanged && authored.front().marks[i].peak.x==ridges.front().marks[i].peak.x &&
                authored.front().marks[i].peak.y==ridges.front().marks[i].peak.y;
        check(unchanged,"Painted mountain composition must preserve locked range geography and peak placement");
        bool deterministic=ridges.size()==again.size();std::size_t bent=0;
        for(std::size_t i=0;i<ridges.size();++i)
        {
            if(i<again.size())deterministic=deterministic && samePoints(ridges[i].spine,again[i].spine);
            if(i<authored.size() || ridges[i].spine.empty())continue;
            float low=ridges[i].spine.front().y,high=low;
            for(const auto p:ridges[i].spine){low=std::min(low,p.y);high=std::max(high,p.y);}
            if(high-low>.08f)++bent;
        }
        check(deterministic && bent>5,"A broad mountain brush patch needs deterministic independently bent ridges, not horizontal rows");
        bool cellsUnchanged=before.size()==painted.cells.size();
        for(std::size_t i=0;i<before.size();++i)cellsUnchanged=cellsUnchanged && before[i].kind==painted.cells[i].kind;
        check(cellsUnchanged && painted.generation.graph.features.size()==1,
            "Composed brush artwork must not edit semantic cells or add saved range objects");
        painted.generation.graph.features.clear();
        for(auto &cell:painted.cells)cell.kind=MapTileKind::Forest;
        const auto trees=buildOverviewForestGlyphs(painted),treesAgain=buildOverviewForestGlyphs(painted);
        float low=1.f,high=0.f;bool valid=trees.size()==treesAgain.size() && !trees.empty();
        for(std::size_t i=0;i<trees.size();++i)
        {
            const auto p=trees[i].center;
            low=std::min(low,p.y-std::floor(p.y));high=std::max(high,p.y-std::floor(p.y));
            valid=valid && overviewGroundKindForRender(painted,static_cast<int>(std::floor(p.x)),
                static_cast<int>(std::floor(p.y)))==MapTileKind::Forest;
            if(i<treesAgain.size())valid=valid && p.x==treesAgain[i].center.x && p.y==treesAgain[i].center.y;
        }
        check(valid && high-low>.5f,"Forest brush crowns must leave cell-center rows while staying deterministic and on forest ground");
    }
    // A successful one-cell brush edit must survive the continuous coast
    // reconstruction in both directions, without secretly growing its cells.
    for (const auto level:{MapLevel::World,MapLevel::Region}) for (bool paintWater:{false,true})
    {
        MapModel painted; painted.width=40; painted.height=30; painted.atlasLevel=level;
        painted.cells.resize(1200); painted.generation.continuousRendering=true;
        painted.generation.recipe.styleId="illustrated_atlas";
        painted.generation.recipe.styleVersion=5;
        painted.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
        painted.generation.recipe.generatorVersion=11;
        for (auto &cell:painted.cells) cell.kind=paintWater?MapTileKind::Dirt:MapTileKind::Water;
        const auto original=painted;
        const auto kind=paintWater?MapTileKind::Water:MapTileKind::Dirt;
        const auto coverage=[&](float x,float y) {
            const auto ground=buildOverviewGroundProjection(painted);
            std::vector<std::uint8_t> mask(ground.size());
            for(std::size_t i=0;i<ground.size();++i) mask[i]=overviewGroundKindIsLand(ground[i])?1:0;
            return detail::overviewLandCoverageFromMask(painted,mask,x,y);
        };
        applyMapTerrainBrush(painted,mapBrushFootprint(painted,20,9,1),kind,0,false);
        for(unsigned seed=1;seed<=20;++seed) {
            painted.generation.recipe.seed=seed;
            check(paintWater?coverage(20.5f,9.5f)<.05f:coverage(20.5f,9.5f)>.95f,
                "Single-cell paint must visibly change continuous land/water at its actual centre for every seed");
        }
        for(int x=21;x<=30;++x) applyMapTerrainBrush(painted,mapBrushFootprint(painted,x,9,1),kind,0,false);
        for(float x=20.5f;x<=30.5f;x+=.25f)
            check(paintWater?coverage(x,9.5f)<.05f:coverage(x,9.5f)>.95f,
                "A one-cell-wide brush drag must remain a continuous visible stroke");
        check(painted.cells[8*40+20].kind==original.cells[8*40+20].kind &&
              painted.cells[10*40+20].kind==original.cells[10*40+20].kind,
            "Making paint visible must not enlarge the semantic brush footprint");
        painted=original;
        for(int y=0;y<30;++y)for(int x=20;x<40;++x) painted.cells[y*40+x].kind=kind;
        check(!overviewHasThinTerrainCore(painted,19,15)&&!overviewHasThinTerrainCore(painted,20,15),
            "Broad coastline must retain its existing smooth reconstruction");
        painted=original; applyMapTerrainBrush(painted,mapBrushFootprint(painted,20,9,1),kind,0,false);
        painted.atlasLevel=MapLevel::Tactical;
        check(!overviewHasThinTerrainCore(painted,20,9),"Tactical appearance must not change with atlas brush fix");
        painted.atlasLevel=level; painted.generation.recipe.styleVersion=3;
        painted.generation.recipe.styleSnapshotId="illustrated_atlas_v3";
        check(!overviewHasThinTerrainCore(painted,20,9),"Historical atlas snapshots must retain their terrain reconstruction");
    }
    for(const auto level:{MapLevel::World,MapLevel::Region})
    {
        MapModel source;source.width=24;source.height=18;resizeMapCells(source);
        source.atlasLevel=level;source.generation.continuousRendering=true;
        source.generation.recipe.generatorVersion=12;
        source.generation.recipe.styleId="illustrated_atlas";
        source.generation.recipe.styleVersion=5;source.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
        for(auto &cell:source.cells){cell.kind=MapTileKind::Dirt;cell.fogged=false;}
        MapGeneratedFeature river;river.id="test-source";river.kind=MapGeneratedFeatureKind::River;
        river.width=2.f;river.points={{4.5f,5.5f},{8,8},{14,6},{19,12}};
        source.generation.graph.features={river};
        check(atlasRiverEndpointKind(source,river,river.points.front())==AtlasRiverEndpointKind::OpenGround,
              "Open land source must receive a spring treatment");
        const auto spring=buildIllustratedAtlasRiverInk(source,river);
        check(sameInk(spring,buildIllustratedAtlasRiverInk(source,river)),"River ornament placement must be deterministic");
        mapCellAt(source,4,4)->kind=MapTileKind::Mountain;
        check(atlasRiverEndpointKind(source,river,river.points.front())==AtlasRiverEndpointKind::Mountain &&
              !sameInk(spring,buildIllustratedAtlasRiverInk(source,river)),"Mountain source must have distinct rock-cleft artwork");
        mapCellAt(source,4,4)->kind=MapTileKind::Dirt;
        MapGeneratedFeature receiver=river;receiver.id="receiver";receiver.points={{2,5.5f},{8,5.5f}};
        source.generation.graph.features.push_back(receiver);
        check(atlasRiverEndpointKind(source,river,river.points.front())==AtlasRiverEndpointKind::Confluence,
              "A river junction must not gain a false spring or terminal marsh");
        check(atlasRiverEndpointKind(source,river,{0,5})==AtlasRiverEndpointKind::Continuation,
              "Map-edge continuations must not acquire invented endpoint assets");
        source.generation.graph.features.pop_back();
        mapCellAt(source,19,12)->kind=MapTileKind::Water;
        check(atlasRiverEndpointKind(source,river,river.points.back())==AtlasRiverEndpointKind::Water,
              "Standing-water endpoints must receive the water-mouth treatment");
        check(source.generation.graph.features.front().points.front().x==4.5f && source.generation.graph.features.size()==1,
              "Illustration must preserve saved river controls and entities");
        for(const auto &primitive:spring)for(const auto p:primitive.points)
            check(std::isfinite(p.x)&&std::isfinite(p.y),"River ornaments must contain finite geometry");
    }
    {
        MapModel pass;pass.width=16;pass.height=12;resizeMapCells(pass);
        pass.atlasLevel=MapLevel::World;pass.visibility=MapVisibility::PlayersVisible;
        pass.generation.continuousRendering=true;
        pass.generation.recipe.generatorVersion=12;pass.generation.recipe.styleId="illustrated_atlas";
        pass.generation.recipe.styleVersion=5;pass.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
        for(auto &cell:pass.cells){cell.kind=MapTileKind::Mountain;cell.fogged=false;}
        MapGeneratedFeature road;road.id="private-mountain-route";road.kind=MapGeneratedFeatureKind::Road;
        road.points={{8,1},{8,11}};road.width=1.f;road.discovered=true;road.gmOnly=true;
        MapRenderPlanRequest player;player.audience=MapRenderAudience::Player;player.applyFog=false;
        const auto withoutRoad=compileMapRenderPlan(pass,player);
        pass.generation.graph.features.push_back(road);
        const auto ink=buildIllustratedAtlasMountainPassInk(pass,illustratedAtlasRoadCorridors(pass));
        check(!ink.empty() && sameInk(ink,buildIllustratedAtlasMountainPassInk(pass,illustratedAtlasRoadCorridors(pass))),
              "Mountain pass walls need deterministic shared artwork");
        for(const auto &primitive:ink)for(const auto p:primitive.points)
            check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x-8.f)>.14f,
                  "Pass cliff faces must leave the road centreline clear");
        const auto privateRoad=compileMapRenderPlan(pass,player);
        check(sameInk(withoutRoad.atlasInk,privateRoad.atlasInk),
              "A private road must not reveal itself as a pass in public mountain terrain");
        pass.generation.recipe.recipeVersion=4;
        const auto revisedInk=buildIllustratedAtlasMountainPassInk(pass,illustratedAtlasRoadCorridors(pass));
        check(!revisedInk.empty(),"Recipe4 must retain connected mountain pass walls");
        for(const auto &primitive:revisedInk)
        {
            if(primitive.filled)
                check(atlasSilhouetteClearsRoads(primitive.points,illustratedAtlasRoadCorridors(pass)),
                      "Recipe4 mountain faces must clear the full road corridor");
            else
                for(const auto &corridor:illustratedAtlasRoadCorridors(pass))
                    for(std::size_t i=1;i<primitive.points.size();++i)
                        check(!gm::map_geometry::roadCorridorIntersectsSegment(corridor,
                            primitive.points[i-1],primitive.points[i],primitive.strokeCells*.5f),
                            "Recipe4 mountain strokes must clear the full road corridor");
        }
        const auto revisedPrivate=compileMapRenderPlan(pass,player);
        pass.generation.graph.features.clear();
        check(sameInk(revisedPrivate.atlasInk,compileMapRenderPlan(pass,player).atlasInk),
              "Recipe4 must not disclose a private mountain route");
    }
    {
        MapModel fields; fields.width=32; fields.height=24; resizeMapCells(fields);
        fields.atlasLevel=MapLevel::Region; fields.generation.continuousRendering=true;
        fields.generation.recipe.generatorVersion=12; fields.generation.recipe.styleId="illustrated_atlas";
        fields.generation.recipe.styleVersion=6; fields.generation.recipe.styleSnapshotId="illustrated_atlas_v6";
        for(auto &cell:fields.cells) { cell.kind=MapTileKind::Dirt; cell.fogged=false; }
        MapGeneratedFeature town; town.id="farm-town"; town.kind=MapGeneratedFeatureKind::Settlement;
        town.points={{16.5f,10.5f}}; town.materialId="town"; town.discovered=true;
        fields.generation.graph.features={town}; mapCellAt(fields,16,10)->kind=MapTileKind::Settlement;
        auto isParcel=[](const AtlasInkPrimitive &p) {
            return p.filled && p.color.a==185 && (p.color.r==191 || p.color.r==213);
        };
        const auto originalFields=buildIllustratedAtlasFeatureInk(fields,false,true);
        const auto parcel=std::find_if(originalFields.begin(),originalFields.end(),isParcel);
        check(parcel!=originalFields.end(),"Cultivation control must contain parcels");
        if(parcel!=originalFields.end())
        {
            MapFeaturePoint center{};
            for(auto p:parcel->points) { center.x+=p.x/parcel->points.size(); center.y+=p.y/parcel->points.size(); }
            MapGeneratedFeature river; river.id="parcel-water"; river.kind=MapGeneratedFeatureKind::River;
            river.width=.7f; river.points={{center.x-2,center.y},{center.x+2,center.y}}; river.discovered=true;
            fields.generation.graph.features.push_back(river);
            const auto historical=buildIllustratedAtlasFeatureInk(fields,false,true);
            fields.generation.recipe.recipeVersion=4;
            const auto revised=buildIllustratedAtlasFeatureInk(fields,false,true);
            check(std::count_if(revised.begin(),revised.end(),isParcel)<std::count_if(historical.begin(),historical.end(),isParcel),
                  "Recipe4 must remove cultivated parcels crossing a river between dry corners");
            const gm::map_geometry::MapRoadCorridor water{river.id,mapFeaturePresentationPoints(fields,river),.22f};
            for(const auto &p:revised) if(isParcel(p))
                check(atlasSilhouetteClearsRoads(p.points,{water},.07f),"Cultivated polygon intersects river corridor");
            fields.generation.recipe.recipeVersion=3;
            check(sameInk(historical,buildIllustratedAtlasFeatureInk(fields,false,true)),"Historical recipe appearance changed");
        }
        MapGeneratedFeature road; road.id="label-road"; road.kind=MapGeneratedFeatureKind::Road;
        road.points={{5,4},{5,8}};
        fields.generation.graph.features={road};
        const float oldCost=illustratedAtlasLabelPlacementCost(fields,{2,6},4,5,6,7);
        fields.generation.recipe.recipeVersion=4;
        check(illustratedAtlasLabelPlacementCost(fields,{2,6},4,5,6,7)>=oldCost+18,
              "Text rectangle overlap must be penalized even when leader ends on the route");
        const std::vector<gm::map_geometry::MapRoadCorridor> hiddenRoutes;
        const auto hiddenCost=illustratedAtlasLabelPlacementCost(fields,{2,6},4,5,6,7,&hiddenRoutes);
        fields.generation.graph.features.clear();
        check(hiddenCost==illustratedAtlasLabelPlacementCost(fields,{2,6},4,5,6,7,&hiddenRoutes),
              "Hidden routes must not influence public label placement");
        const auto plan=compileMapRenderPlan(fields,request);
        fields.generation.recipe.recipeVersion=3;
        check(!mapRenderPlanMatchesMap(plan,fields),"Plans must reject changed cartography recipe versions");
    }
    {
        std::mt19937 random(78231);
        auto point=[&] {return MapFeaturePoint{static_cast<float>(random()%4000)/100-20,
                                              static_cast<float>(random()%4000)/100-20};};
        for(int sample=0;sample<2000;++sample)
        {
            gm::map_geometry::MapRoadCorridor corridor{"differential",{point(),point(),point(),point()},
                static_cast<float>(sample%7-1)*.2f};
            const auto a=point(),b=point();
            bool reference=false;
            for(std::size_t i=1;i<corridor.centerline.size();++i)
                reference=reference || gm::map_geometry::roadSegmentDistanceSquared(a,b,corridor.centerline[i-1],corridor.centerline[i])
                    <=corridor.radiusCells*corridor.radiusCells;
            check(reference==gm::map_geometry::roadCorridorIntersectsSegment(corridor,a,b),
                  "Broad-phase clearance must preserve the original exact segment result");
        }
    }
    {
        MapModel edge;edge.width=10;edge.height=10;resizeMapCells(edge);
        for(auto& cell:edge.cells){cell.kind=MapTileKind::Empty;cell.fogged=false;}
        for(int y=0;y<10;++y)mapCellAt(edge,5,y)->kind=MapTileKind::Forest;
        edge.generation.recipe.recipeVersion=4;
        check(illustratedAtlasLabelTerrainCost(edge,3.5f,3.1f,5.05f,3.8f)==0.f,
              "Older label placement must retain its interior sampling");
        edge.generation.recipe.recipeVersion=5;
        check(illustratedAtlasLabelTerrainCost(edge,3.5f,3.1f,5.05f,3.8f)>0.f,
              "A canopy touching the glyph edge must affect new label placement");
        check(illustratedAtlasLabelTerrainCost(edge,2.f,3.1f,3.f,3.8f)==0.f,
              "Neighboring terrain outside the label plate must not affect placement");
        for(auto& cell:edge.cells)cell.kind=MapTileKind::Empty;
        mapCellAt(edge,4,5)->kind=MapTileKind::Mountain;
        edge.atlasLevel=MapLevel::Region;
        edge.generation.recipe.styleId="illustrated_atlas";
        edge.generation.recipe.styleVersion=6;
        edge.generation.recipe.styleSnapshotId="illustrated_atlas_v6";
        const std::vector<AtlasInkPrimitive> relief{{{{4,5},{4,2},{6,5}},{150,150,130,255},.04f,true}};
        const auto mask=buildAtlasLabelInkMask(edge,relief);
        check(illustratedAtlasLabelInkCost(mask,4.2f,3.1f,4.6f,3.8f)>0.f,
              "Actual summit overhang must discourage a label above its terrain anchor");
        check(illustratedAtlasLabelInkCost(mask,2.f,3.1f,3.f,3.8f)==0.f,
              "Clear space beside relief must not inherit an approximate mountain penalty");
        const auto hiddenMask=buildAtlasLabelInkMask(edge,{});
        check(illustratedAtlasLabelInkCost(hiddenMask,4.2f,3.1f,4.6f,3.8f)==0.f,
              "Relief excluded from reviewed artwork must not influence label placement");
        edge.generation.recipe.recipeVersion=4;
        check(buildAtlasLabelInkMask(edge,relief).covered.empty(),"Old recipe labels must not opt into new ink scoring");
        edge.generation.recipe.recipeVersion=5;
        mapCellAt(edge,4,5)->fogged=true;
        check(illustratedAtlasLabelTerrainCost(edge,3.5f,3.1f,4.5f,3.8f)==0.f,
              "Unrevealed mountain terrain must not move a public label");
        mapCellAt(edge,4,4)->kind=MapTileKind::Mountain;
        mapCellAt(edge,4,4)->fogged=true;
        check(illustratedAtlasLabelTerrainCost(edge,3.5f,3.1f,4.5f,3.8f)==0.f,
              "The nearest hidden summit must also be excluded from new label scoring");
    }
    {
        MapModel valley;valley.width=24;valley.height=18;resizeMapCells(valley);
        valley.atlasLevel=MapLevel::Region;valley.generation.recipe.styleId="illustrated_atlas";
        valley.generation.recipe.styleVersion=6;valley.generation.recipe.styleSnapshotId="illustrated_atlas_v6";
        valley.generation.recipe.generatorVersion=12;valley.generation.recipe.recipeVersion=6;
        for(auto &cell:valley.cells){cell.kind=MapTileKind::Mountain;cell.fogged=false;}
        const std::vector<gm::map_geometry::MapRoadCorridor> cuts{
            {"river",{{2,4},{8,8},{15,9},{22,14}},.65f},
            {"road",{{12,1},{13,6},{11,12},{13,17}},.60f}};
        const auto relief=buildAtlasValleyReliefInk(valley,cuts);
        check(!relief.empty(),"New valley crossing must retain visible mountain relief");
        check(sameInk(relief,buildAtlasValleyReliefInk(valley,cuts)),"Valley relief changed without a map edit");
        for(const auto &primitive:relief) {
            if(primitive.filled)check(atlasSilhouetteClearsRoads(primitive.points,cuts),"New valley face overlaps river or road");
            for(const auto &cut:cuts)for(std::size_t i=1;i<primitive.points.size();++i)
                check(!gm::map_geometry::roadCorridorIntersectsSegment(cut,primitive.points[i-1],primitive.points[i],primitive.strokeCells*.5f),
                    "Valley contour overlaps a crossing");
        }
        for(auto &cell:valley.cells)cell.kind=MapTileKind::Water;
        check(buildAtlasValleyReliefInk(valley,cuts).empty(),"Valley mountains appeared over water");
    }
    {
        MapModel gouache; gouache.width=32; gouache.height=24; resizeMapCells(gouache);
        gouache.atlasLevel=MapLevel::World;
        gouache.generation.recipe.styleId="illustrated_atlas";
        gouache.generation.recipe.styleVersion=6;
        gouache.generation.recipe.styleSnapshotId="illustrated_atlas_v6";
        gouache.generation.recipe.recipeVersion=11; gouache.generation.recipe.seed=612345;
        MapGeneratedFeature way; way.id="gouache-road"; way.kind=MapGeneratedFeatureKind::Road;
        way.points={{2,4},{2,4},{8,6},{13,3},{19,8}}; way.discovered=true;
        const auto original=way.points;
        const auto material=buildGouacheAtlasRoadInk(gouache,way);
        check(!material.empty() && sameInk(material,buildGouacheAtlasRoadInk(gouache,way)),
              "Gouache road material must be deterministic");
        check(samePoints(original,way.points),"Road artwork must not edit the saved route");
        const auto validInk=[&](const std::vector<AtlasInkPrimitive>& items) {
            for(const auto &item:items) {
                check(item.vertexColors.empty() || item.vertexColors.size()==item.points.size(),"Road mesh colors must match vertices");
                for(auto p:item.points) check(std::isfinite(p.x)&&std::isfinite(p.y),"Gouache geometry must remain finite");
            }
        };
        validInk(material);
        std::reverse(way.points.begin(),way.points.end());
        validInk(buildGouacheAtlasRoadInk(gouache,way));
        way.points={{4,4},{4,4}};
        check(buildGouacheAtlasRoadInk(gouache,way).empty(),"Zero-length roads must not create invalid caps");
        const auto town=buildGouacheAtlasSettlementInk(gouache,{12,12},"town");
        check(!town.empty()&&sameInk(town,buildGouacheAtlasSettlementInk(gouache,{12,12},"town")),
              "Town artwork must be deterministic");
        validInk(town);
        check(!sameInk(town,buildGouacheAtlasSettlementInk(gouache,{12,12},"village")),
              "Village and town symbols must communicate different settlement scale");
        for(const auto &item:town)for(auto p:item.points)
            check(p.x>=11.2f&&p.x<=12.8f&&p.y>=10.62f&&p.y<=12.38f,"Town artwork exceeded its reserved label footprint");
        gouache.generation.recipe.recipeVersion=10;
        way.points=original;
        check(buildGouacheAtlasRoadInk(gouache,way).empty()&&buildGouacheAtlasSettlementInk(gouache,{12,12},"town").empty(),
              "Saved older maps must not opt into gouache artwork");
    }
    map.generation.recipe.styleSnapshotId = "illustrated_atlas_v999";
    check(!usesIllustratedAtlas(map), "Unknown future snapshots must not silently use today's artwork");
    return failures == 0 ? 0 : 1;
}
