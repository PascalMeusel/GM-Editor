#include "Models/Map/MapPngExport.hpp"
#include "Models/Media/RgbaImageLoader.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Fixture
{
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("gm-tactical-road-ink-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture() { std::filesystem::create_directories(root); }
    ~Fixture() { std::error_code error; std::filesystem::remove_all(root,error); }
};

using Color = std::array<unsigned char,4>;
Color pixel(const LoadedRgbaImage &image, int x, int y)
{
    require(x >= 0 && y >= 0 && x < image.width && y < image.height,"Invalid fixture pixel");
    const auto offset = static_cast<std::size_t>(y*image.width+x)*4;
    return {image.pixels[offset],image.pixels[offset+1],image.pixels[offset+2],image.pixels[offset+3]};
}

LoadedRgbaImage read(const std::filesystem::path &path)
{
    LoadedRgbaImage image;
    require(loadRgbaImage(path,image),"Could not decode exported road image");
    return image;
}

Color composite(Color base, Color ink)
{
    for (int channel = 0; channel < 3; ++channel)
        base[channel] = static_cast<unsigned char>((ink[channel]*ink[3]+base[channel]*(255-ink[3]))/255);
    base[3] = 255;
    return base;
}

MapRenderPlanRequest request()
{
    MapRenderPlanRequest result;
    result.layers = MapRenderPlanWays;
    result.applyFog = false;
    return result;
}

MapModel roadMap()
{
    MapModel map;
    map.metadata.id = "translucent_tactical_road";
    map.width = 16; map.height = 8; map.atlasLevel = MapLevel::Tactical;
    map.visibility = MapVisibility::PlayersVisible;
    map.playerRelease.waysAndRoutes = true;
    map.generation.recipe.generatorVersion = 7;
    map.generation.recipe.styleVersion = 1;
    map.generation.recipe.styleSnapshotId.clear();
    resizeMapCells(map);
    for (auto &cell : map.cells) { cell.kind = MapTileKind::Dirt; cell.fogged = false; }
    MapGeneratedFeature road;
    road.id = "road"; road.kind = MapGeneratedFeatureKind::Road;
    road.points = {{2,4},{14,4}}; road.width = 1.f; road.locked = true; road.discovered = true;
    map.generation.graph.features.push_back(road);
    return map;
}

LoadedRgbaImage render(const MapModel &map, const std::filesystem::path &path, int detail = 32,
                       const MapRenderPlanRequest &options = request())
{
    std::string error;
    require(exportMapPng(map,path,compileMapRenderPlan(map,options),detail,&error),error.c_str());
    return read(path);
}

void requireSubimage(const LoadedRgbaImage &whole, const LoadedRgbaImage &part,
                     const MapPngExportRect &rect, int detail)
{
    require(part.width == rect.width*detail && part.height == rect.height*detail,"Incorrect crop dimensions");
    for (int y = 0; y < part.height; ++y)
        for (int x = 0; x < part.width; ++x)
            require(pixel(part,x,y) == pixel(whole,x+rect.x*detail,y+rect.y*detail),
                    "Cropping or tiling changed tactical road coverage/alpha");
}

void testEachPassBlendsOnce(const std::filesystem::path &root)
{
    auto map = roadMap();
    auto emptyMap = map;
    emptyMap.generation.graph.features.clear();
    const auto empty = render(emptyMap,root/"empty.png");
    const auto road = render(map,root/"road.png");
    const auto base = pixel(empty,256,128);
    const auto shoulder = composite(base,{104,85,59,30});
    const auto surface = composite(shoulder,{156,124,80,60});
    const auto track = composite(surface,{112,86,55,80});
    require(pixel(road,256,138) == shoulder,"Road shoulder accumulated alpha along its raster walk");
    require(pixel(road,256,128) == surface,"Road surface accumulated alpha instead of revealing the ground");
    // At 32px/cell this width is 18px; the two offset tracks are at +3/-4px.
    require(pixel(road,256,131) == track && pixel(road,256,124) == track,
            "Each wheel-track pass must composite exactly once");

    map.generation.graph.features.front().points = {{2,4},{4,4},{6,4},{8,4},{10,4},{12,4},{14,4}};
    require(render(map,root/"subdivided-road.png").pixels == road.pixels,
            "Adding collinear editable controls changed road opacity or track coverage");
    map.generation.graph.features.front().kind = MapGeneratedFeatureKind::Trail;
    map.generation.graph.features.front().points = {{2,4},{14,4}};
    const auto trail = render(map,root/"trail.png");
    require(pixel(trail,256,128) == composite(shoulder,{125,99,65,50}),
            "Trail surface must blend once with its own lower opacity");
    map.generation.graph.features.front().points = {{2,4},{4,4},{8,4},{8,4},{8,4},{12,4},{14,4}};
    require(render(map,root/"duplicate-trail.png").pixels == trail.pixels,
            "Duplicate/subdivided trail points accumulated extra translucent ink");

    map = roadMap();
    map.generation.recipe.generatorVersion = 6;
    const auto legacy6 = render(map,root/"legacy6.png");
    require(pixel(legacy6,256,128) == Color{156,124,80,255} &&
                pixel(legacy6,256,138) == Color{104,85,59,255},
            "Existing V6 tactical roads must retain their opaque legacy appearance");
    map.generation.recipe.generatorVersion = 5;
    require(render(map,root/"legacy5.png").pixels == legacy6.pixels,
            "The V7-only ink correction changed older tactical version behavior");
}

void testCurvesCroppingAndPrivacy(const std::filesystem::path &root)
{
    auto map = roadMap();
    map.generation.graph.features.front().points = {{1.5f,5.5f},{5.5f,2.5f},{9.5f,5.5f},{14.5f,2.5f}};
    const auto plan = compileMapRenderPlan(map,request());
    const auto whole = render(map,root/"curve.png");
    std::string error;
    const MapPngExportRect rect{4,2,8,4};
    require(exportMapPngRegion(map,root/"curve-crop.png",plan,rect,32,&error),error.c_str());
    requireSubimage(whole,read(root/"curve-crop.png"),rect,32);
    auto player = request();
    player.audience = MapRenderAudience::Player;
    require(render(map,root/"public.png",32,player).pixels == whole.pixels,
            "Released tactical roads should preserve the same reviewed coverage");
    map.generation.graph.features.front().gmOnly = true;
    const auto hidden = render(map,root/"private.png",32,player);
    map.generation.graph.features.clear();
    require(hidden.pixels == render(map,root/"absent.png",32,player).pixels,
            "Union coverage exposed a private tactical road");
}

void testActualTileBoundary(const std::filesystem::path &root)
{
    auto map = roadMap();
    map.width = 2047; // Whole image fits; the real one-cell tile overlap needs two tiles.
    resizeMapCells(map);
    for (auto &cell : map.cells) { cell.kind = MapTileKind::Dirt; cell.fogged = false; }
    map.generation.graph.features.front().points = {{2038.5f,5.5f},{2041.5f,2.5f},{2044.5f,5.5f},{2046.5f,2.5f}};
    const auto plan = compileMapRenderPlan(map,request());
    const auto whole = render(map,root/"tile-whole.png",8);
    std::string error;
    MapPngTiledExportPlan tiled;
    require(exportMapPngTiled(map,root/"tiles","road",plan,8,false,&tiled,&error),error.c_str());
    require(tiled.tiles.size() == 2,"The road fixture did not cross a real tile boundary");
    for (const auto &tile : tiled.tiles)
        requireSubimage(whole,read(root/"tiles"/tile.fileName),tile.render,8);
}
}

int main()
{
    try
    {
        Fixture fixture;
        auto illustrated=roadMap();illustrated.scope=MapScope::Local;
        illustrated.generation.continuousRendering=true;
        illustrated.generation.recipe.generatorVersion=12;
        illustrated.generation.recipe.styleId="illustrated_atlas";
        illustrated.generation.recipe.styleVersion=6;
        illustrated.generation.recipe.styleSnapshotId="illustrated_atlas_v6";
        for(float width:{.8f,1.f,1.2f,1.5f}) {
            illustrated.generation.graph.features.front().width=width;
            const auto image=render(illustrated,fixture.root/("v6-"+std::to_string(width)+".png"),16);
            int bank=0,surface=0;
            for(int y=0;y<image.height;++y){const auto c=pixel(image,8*16,y);
                bank+=c==Color{149,128,87,255};surface+=c==Color{196,174,125,255};}
            require(bank>=2&&surface>0,"Illustrated road lost its edge due to integer stroke rounding");
        }
        illustrated.generation.graph.features.clear();
        for(int y=0;y<illustrated.height;++y)for(int x=0;x<illustrated.width;++x)
            mapCellAt(illustrated,x,y)->kind=x>7?MapTileKind::Forest:y>3?MapTileKind::Water:MapTileKind::Dirt;
        auto terrain=request();terrain.layers=MapRenderPlanTerrain|MapRenderPlanWater;
        const auto terrainPlan=compileMapRenderPlan(illustrated,terrain);
        const auto full=render(illustrated,fixture.root/"v6-terrain.png",16,terrain);
        const MapPngExportRect crop{4,2,8,4};std::string cropError;
        require(exportMapPngRegion(illustrated,fixture.root/"v6-crop.png",terrainPlan,crop,16,&cropError),cropError.c_str());
        requireSubimage(full,read(fixture.root/"v6-crop.png"),crop,16);
        testEachPassBlendsOnce(fixture.root);
        testCurvesCroppingAndPrivacy(fixture.root);
        testActualTileBoundary(fixture.root);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
