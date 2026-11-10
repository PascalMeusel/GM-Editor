#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapRegionComposition.hpp"
#include <iostream>

namespace
{
bool check(bool condition, const char *message)
{
    if (!condition) std::cerr << message << '\n';
    return condition;
}
bool sameMap(const MapModel &a, const MapModel &b)
{
    if (a.cells.size() != b.cells.size() || a.generation.graph.features.size() != b.generation.graph.features.size()) return false;
    for (std::size_t i=0;i<a.cells.size();++i)
        if (a.cells[i].kind != b.cells[i].kind || a.cells[i].wayKind != b.cells[i].wayKind ||
            a.cells[i].elevation != b.cells[i].elevation) return false;
    for (std::size_t i=0;i<a.generation.graph.features.size();++i)
    {
        const auto &x=a.generation.graph.features[i], &y=b.generation.graph.features[i];
        if (x.id != y.id || x.kind != y.kind || x.name != y.name || x.materialId != y.materialId || x.points.size()!=y.points.size()) return false;
        for (std::size_t j=0;j<x.points.size();++j)
            if (x.points[j].x != y.points[j].x || x.points[j].y != y.points[j].y) return false;
    }
    return true;
}
}

int main()
{
    bool ok=true;
    for (unsigned seed : {17u, 9041u, 2073941524u})
        for (int y=0;y<24;++y) for(int x=0;x<36;++x)
        {
            const auto sample=gm::map_generation::region::terrain(seed,x,y,36,24,
                MapLandscapeProfile::RiverValley,0,0,0,.65f,.8f);
            ok &= check(sample.kind==MapTileKind::Empty,"Zero mountain/forest/water requests gained invented terrain");
            ok &= check(std::isfinite(sample.elevation) && sample.elevation>=0.f && sample.elevation<=1.f,"Composed elevation outside normalized range");
        }
    ok &= check(gm::map_generation::region::landmarkName("Vaelor",31u,0,"en")=="Vaelor Watch","English generated landmark naming");
    ok &= check(gm::map_generation::region::landmarkName("Vaelor",31u,0,"de")=="Wacht Vaelor","German generated landmark naming");
    MapGenerationOptions options;
    options.generatorVersion=10; options.scope=MapScope::Local; options.scale=MapLevel::Region;
    options.styleId="illustrated_atlas"; options.width=48; options.height=34;
    options.requestedSettlements=4; options.requestedLandmarks=4;
    options.qualityAttempts=24; options.contentLanguage="en";
    for (int profile=0;profile<5;++profile)
    {
        options.seed=703119u+static_cast<unsigned>(profile)*104729u;
        options.landscape=static_cast<MapLandscapeProfile>(profile);
        options.mountainDensity=profile==3?65:30; options.forestDensity=profile==1?70:35;
        options.waterLevel=profile==4?78:35;
        const auto batch=generateSemanticMapCandidateBatch(options,1);
        ok &= check(batch.candidates.size()==1,"Regional profile produced no eligible candidate");
        if(batch.candidates.empty()) continue;
        const auto &result=batch.candidates.front(); const auto &map=result.map;
        ok &= check(result.validation.eligible && result.quality.criticalIssueCount==0,"Regional candidate failed structural validation");
        ok &= check(map.generation.recipe.styleVersion==5 && map.generation.recipe.styleSnapshotId=="illustrated_atlas_v5","New regional recipe did not pin v5 artwork");
        std::vector<MapCellCoordinate> towns;
        for(const auto &feature:map.generation.graph.features)
            if(feature.kind==MapGeneratedFeatureKind::Settlement && !feature.points.empty())
                towns.push_back({static_cast<int>(feature.points.front().x),static_cast<int>(feature.points.front().y)});
        ok &= check(towns.size()==4,"Requested regional town count changed");
        for(std::size_t i=1;i<towns.size();++i)
            ok &= check(mapGenCoordinatesConnected(map,towns.front(),towns[i],true),"A requested town was disconnected from the travel network");
        for(const auto &landmark:map.generation.graph.features)
            if(landmark.kind==MapGeneratedFeatureKind::Landmark && !landmark.points.empty())
                for(const auto &lake:map.generation.graph.features)
                    if(lake.kind==MapGeneratedFeatureKind::TerrainRegion && lake.closed && lake.materialId.find("water.lake")!=std::string::npos)
                        ok &= check(!mapGenV4PointInsidePolygon(landmark.points.front(),lake.points),"Automatic landmark was generated inside a lake");
        if(profile==3)
        {
            const auto repeated=generateSemanticMapCandidateBatch(options,1);
            ok &= check(!repeated.candidates.empty() && sameMap(map,repeated.candidates.front().map),"V10 regional generation is not deterministic");
        }
    }
    // R07/10 placed Dornath Stones in a lake while structural QA still passed.
    // Test the feature polygon rather than the overwritten raster cell.
    options.landscape=MapLandscapeProfile::LakeCountry; options.seed=1415765205u;
    options.width=80; options.height=56; options.waterLevel=62;
    options.mountainDensity=38; options.forestDensity=60; options.requestedLandmarks=6;
    const auto lakeRegression=generateSemanticMapCandidateBatch(options,1);
    ok &= check(!lakeRegression.candidates.empty(),"Known lake-landmark recipe became unavailable");
    if(!lakeRegression.candidates.empty())
        for(const auto &landmark:lakeRegression.candidates.front().map.generation.graph.features)
            if(landmark.kind==MapGeneratedFeatureKind::Landmark && !landmark.points.empty())
                for(const auto &lake:lakeRegression.candidates.front().map.generation.graph.features)
                    if(lake.kind==MapGeneratedFeatureKind::TerrainRegion && lake.closed && lake.materialId.find("water.lake")!=std::string::npos)
                        ok &= check(!mapGenV4PointInsidePolygon(landmark.points.front(),lake.points),"R07 lake-landmark regression returned");
    // R13 lake country added a square optional bridge detour around a short
    // headwater although the existing town road already goes around it.
    options.seed=1281844545u; options.width=56; options.height=42;
    options.waterLevel=63; options.mountainDensity=10; options.forestDensity=40;
    options.requestedSettlements=6; options.requestedLandmarks=4; options.routeDensity=28;
    const auto bridgeRegression=generateSemanticMapCandidateBatch(options,1);
    ok &= check(!bridgeRegression.candidates.empty(),"Bridge-detour regression produced no map");
    if(!bridgeRegression.candidates.empty())
    {
        const auto &map=bridgeRegression.candidates.front().map;
        std::vector<MapCellCoordinate> towns;
        for(const auto &f:map.generation.graph.features)
        {
            ok &= check(f.name!="Regional river crossing","R13 unnecessary optional bridge loop returned");
            if(f.kind==MapGeneratedFeatureKind::Settlement && !f.points.empty())
                towns.push_back({static_cast<int>(f.points.front().x),static_cast<int>(f.points.front().y)});
        }
        ok &= check(towns.size()==6,"Optional bridge removal changed requested towns");
        for(std::size_t i=1;i<towns.size();++i)
            ok &= check(mapGenCoordinatesConnected(map,towns.front(),towns[i],true),"Optional bridge removal broke the town network");
    }
    // R15's diagonal headwaters became three adjacent edge gutters and a
    // sequence of unnecessary bridges. Verify the exported graph, not the
    // implementation's source-scoring constants.
    options.seed=1577987303u; options.width=55; options.height=39;
    options.landscape=MapLandscapeProfile::RiverValley; options.waterLevel=50;
    options.mountainDensity=15; options.forestDensity=45;
    options.requestedSettlements=4; options.requestedLandmarks=3;
    const auto riverRegression=generateSemanticMapCandidateBatch(options,1);
    ok &= check(!riverRegression.candidates.empty(),"Adjacent-outlet regression produced no map");
    if(!riverRegression.candidates.empty())
    {
        const auto &map=riverRegression.candidates.front().map;
        std::vector<MapFeaturePoint> mouths;
        for(const auto &f:map.generation.graph.features)
            if(f.kind==MapGeneratedFeatureKind::River && !f.points.empty())
            {
                const auto p=f.points.back();
                if(p.x<1.5f || p.y<1.5f || p.x>map.width-1.5f || p.y>map.height-1.5f)
                    mouths.push_back(p);
            }
        for(std::size_t i=0;i<mouths.size();++i) for(std::size_t j=i+1;j<mouths.size();++j)
            ok &= check(std::hypot(mouths[i].x-mouths[j].x,mouths[i].y-mouths[j].y)>=4.f,
                        "R15 near-duplicate independent edge outlets returned");
    }
    // R16's forest frontier requested density65 but contained only ~7% forest.
    // The composition must retain a usable woodland even with dry base noise.
    options.seed=2005184279u; options.width=48; options.height=34;
    options.landscape=MapLandscapeProfile::ForestFrontier; options.waterLevel=25;
    options.mountainDensity=20; options.forestDensity=65;
    options.requestedSettlements=4; options.requestedLandmarks=6;
    const auto forestRegression=generateSemanticMapCandidateBatch(options,1);
    ok &= check(!forestRegression.candidates.empty(),"Dense-frontier regression produced no map");
    if(!forestRegression.candidates.empty())
    {
        const auto &cells=forestRegression.candidates.front().map.cells;
        const auto woods=std::count_if(cells.begin(),cells.end(),[](const auto &c){return c.kind==MapTileKind::Forest;});
        ok &= check(woods*100>=cells.size()*15,"Dense forest frontier again became a nearly treeless plain");
    }
    // R50's Lake Country requested forest60 but rendered only 3.7% woods.
    // The requested woodland province must work across regional profiles.
    options.seed=1913125088u; options.width=80; options.height=56;
    options.landscape=MapLandscapeProfile::LakeCountry; options.waterLevel=62;
    options.mountainDensity=38; options.forestDensity=60;
    options.requestedSettlements=4; options.requestedLandmarks=6; options.routeDensity=28;
    const auto regionalWoodlandRegression=generateSemanticMapCandidateBatch(options,1);
    ok &= check(!regionalWoodlandRegression.candidates.empty(),"Dense lake-country regression produced no map");
    if(!regionalWoodlandRegression.candidates.empty())
    {
        const auto &map=regionalWoodlandRegression.candidates.front().map;
        const auto woods=std::count_if(map.cells.begin(),map.cells.end(),[](const auto &c){return c.kind==MapTileKind::Forest;});
        ok &= check(woods*100>=map.cells.size()*15,"Dense lake-country request again became a nearly treeless plain");
        std::vector<MapCellCoordinate> towns;
        for(const auto &f:map.generation.graph.features)
            if(f.kind==MapGeneratedFeatureKind::Settlement && !f.points.empty())
                towns.push_back({static_cast<int>(f.points.front().x),static_cast<int>(f.points.front().y)});
        ok &= check(towns.size()==4,"Woodland correction changed requested town count");
        for(std::size_t i=1;i<towns.size();++i)
            ok &= check(mapGenCoordinatesConnected(map,towns.front(),towns[i],true),"Woodland correction disconnected a town");
    }
    options.width=48; options.height=34; options.waterLevel=35; options.requestedSettlements=4;
    options.landscape=MapLandscapeProfile::ForestFrontier; options.seed=947133u;
    MapGeneratedFeature inherited;
    inherited.id="region_locked_inherited"; inherited.kind=MapGeneratedFeatureKind::Settlement;
    inherited.name="KEEP EXACT: Hidden Haven"; inherited.materialId="fantasy_neutral.settlement.village";
    inherited.locked=true; inherited.gmOnly=true; inherited.points={{7.5f,11.5f}};
    options.preservedFeatures.push_back(inherited);
    const auto inheritedBatch=generateSemanticMapCandidateBatch(options,1);
    ok &= check(!inheritedBatch.candidates.empty(),"Inherited locked settlement blocked regional generation");
    if(!inheritedBatch.candidates.empty())
    {
        const auto &features=inheritedBatch.candidates.front().map.generation.graph.features;
        const auto found=std::find_if(features.begin(),features.end(),[&](const auto &f){return f.id==inherited.id;});
        ok &= check(found!=features.end(),"Inherited place disappeared");
        if(found!=features.end())
            ok &= check(found->name==inherited.name && found->locked && found->gmOnly && found->points.front().x==7.5f && found->points.front().y==11.5f,"Inherited name/visibility/lock/position changed");
    }
    options.preservedFeatures.clear(); options.generatorVersion=9;
    const auto legacy=generateSemanticMapCandidateBatch(options,1);
    ok &= check(!legacy.candidates.empty() && legacy.candidates.front().map.generation.recipe.styleVersion==4,"Historical regional style silently upgraded");
    if(ok) std::cout << "Regional composition, connectivity, determinism, inherited locks, language and legacy version checks passed.\n";
    return ok?0:1;
}
