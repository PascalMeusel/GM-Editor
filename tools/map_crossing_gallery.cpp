#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapPngExport.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv)
{
    try {
        if(argc!=2)throw std::runtime_error("Expected an external output directory");
        const std::filesystem::path output(argv[1]);std::filesystem::create_directories(output);
        MapModel map;map.name="Authored mountain crossings";map.width=32;map.height=24;resizeMapCells(map);
        map.atlasLevel=MapLevel::Region;map.scope=MapScope::Local;
        map.generation.continuousRendering=true;
        auto &recipe=map.generation.recipe;recipe.seed=914263;recipe.generatorVersion=12;
        recipe.styleId="illustrated_atlas";recipe.styleVersion=6;recipe.styleSnapshotId="illustrated_atlas_v6";
        for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
            auto &cell=map.cells[y*map.width+x];cell.fogged=false;
            cell.kind=(y>=6 && y<=17 && x>=2 && x<=29)?MapTileKind::Mountain:MapTileKind::Empty;
            if(y>=22)cell.kind=MapTileKind::Water;
        }
        const auto feature=[&](const char *id,MapGeneratedFeatureKind kind,std::vector<MapFeaturePoint> points,float width) {
            MapGeneratedFeature f;f.id=id;f.kind=kind;f.points=std::move(points);f.width=width;f.discovered=true;f.locked=true;
            map.generation.graph.features.push_back(std::move(f));
        };
        feature("ridge",MapGeneratedFeatureKind::MountainRange,{{2,10},{10,12},{21,10},{29,12}},3.f);
        feature("road",MapGeneratedFeatureKind::Road,{{0,4},{8,6},{9,12},{10,18},{16,20}},.9f);
        feature("river",MapGeneratedFeatureKind::River,{{21,3},{20,7},{22,12},{20,17},{22,22}},.8f);
        MapPngExportOptions options;options.pixelsPerCell=48;options.showGrid=false;options.applyFog=false;options.showLabels=false;
        for(int version:{6,7,8,9}) {
            recipe.recipeVersion=version;std::string error;
            if(!exportMapPng(map,output/("recipe-"+std::to_string(version)+".png"),options,&error))throw std::runtime_error(error);
        }
        for(int turn=1;turn<=3;++turn) {
            const auto previous=map;
            map.width=previous.height;map.height=previous.width;resizeMapCells(map);
            for(int y=0;y<previous.height;++y)for(int x=0;x<previous.width;++x)
                *mapCellAt(map,previous.height-1-y,x)=*mapCellAt(previous,x,y);
            for(auto &f:map.generation.graph.features)for(auto &p:f.points)
                p={float(previous.height)-p.y,p.x};
            std::string error;
            if(!exportMapPng(map,output/("rotated-"+std::to_string(turn*90)+".png"),options,&error))throw std::runtime_error(error);
        }
        std::ofstream manifest(output/"fixture.txt");
        manifest<<"Same authored geometry and terrain; recipe 6/7/8/9 comparison plus three rotated recipe-9 cases. Mountain road and river corridors, spring and outlet. Human art approval pending.\n";
        return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
