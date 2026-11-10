#include "MapTileAtlas.hpp"
#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Media/RgbaImageLoader.hpp"

#include <GLFW/glfw3.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace
{
enum class TilesetKind
{
    Terrain,
    Nature,
    Travel,
    Settlement,
    Dungeon,
    GrassAuto,
    DirtAuto,
    WaterAuto,
    RoadAuto,
    RoadMaskPrototype,
    StoneAuto,
    SnowAuto,
    DesertAuto,
    SwampAuto,
    IceAuto,
    DungeonFloorAuto,
    DungeonWallAuto,
    HazardAuto,
    LavaAuto,
    PitAuto,
    ChasmAuto,
    TransitionGrassWater,
    TransitionGrassDesert,
    TransitionGrassSnow,
    TransitionGrassRock,
    TransitionGrassSwamp,
    TransitionWaterSand,
    TransitionSnowRock,
    TransitionDungeonFloorWall,
    IntegratedWayPrototype,
    MountainRangePrototype,
    StructureContextPrototype,
    Battlefield,
    Count
};

struct AtlasCrop
{
    TilesetKind tileset = TilesetKind::Terrain;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

std::filesystem::path findTilesetPath(const std::string &fileName)
{
    return mapStyleResolveExistingPath(std::filesystem::path("Resources") / "Maps" / "Tilesets" / fileName);
}

bool loadPngTexture(const std::filesystem::path &path, unsigned int &textureId, int &width, int &height)
{
    LoadedRgbaImage image;
    if (!loadRgbaImage(path, image))
        return false;

    width = image.width;
    height = image.height;
    if (width <= 0 || height <= 0)
        return false;

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

MapTileAtlas &atlasForTileset(TilesetKind kind)
{
    static std::array<MapTileAtlas, static_cast<size_t>(TilesetKind::Count)> atlases;
    static std::array<bool, static_cast<size_t>(TilesetKind::Count)> attemptedLoad{};

    const size_t index = static_cast<size_t>(kind);
    if (!attemptedLoad[index])
    {
        attemptedLoad[index] = true;

        const char *fileName = "";
        switch (kind)
        {
        case TilesetKind::Terrain:
            fileName = "IsometricTerrainTiles.png";
            break;
        case TilesetKind::Nature:
            fileName = "IsometricNatureTiles.png";
            break;
        case TilesetKind::Travel:
            fileName = "IsometricTravelAndMapTiles.png";
            break;
        case TilesetKind::Settlement:
            fileName = "IsometricSettlementTiles.png";
            break;
        case TilesetKind::Dungeon:
            fileName = "IsometricDungeonTiles.png";
            break;
        case TilesetKind::GrassAuto:
            fileName = "IsometricAutoGrassTiles.png";
            break;
        case TilesetKind::DirtAuto:
            fileName = "IsometricAutoDirtTiles.png";
            break;
        case TilesetKind::WaterAuto:
            fileName = "IsometricAutoWaterTiles.png";
            break;
        case TilesetKind::RoadAuto:
            fileName = "IsometricAutoRoadTiles.png";
            break;
        case TilesetKind::RoadMaskPrototype:
            fileName = "Generated/FantasyNeutralPrototype/MaskCorrectRoadTiles.png";
            break;
        case TilesetKind::StoneAuto:
            fileName = "IsometricAutoStoneTiles.png";
            break;
        case TilesetKind::SnowAuto:
            fileName = "IsometricAutoSnowTiles.png";
            break;
        case TilesetKind::DesertAuto:
            fileName = "IsometricAutoDesertTiles.png";
            break;
        case TilesetKind::SwampAuto:
            fileName = "IsometricAutoSwampTiles.png";
            break;
        case TilesetKind::IceAuto:
            fileName = "IsometricAutoIceTiles.png";
            break;
        case TilesetKind::DungeonFloorAuto:
            fileName = "IsometricAutoDungeonFloorTiles.png";
            break;
        case TilesetKind::DungeonWallAuto:
            fileName = "IsometricAutoDungeonWallTiles.png";
            break;
        case TilesetKind::HazardAuto:
            fileName = "IsometricAutoHazardTiles.png";
            break;
        case TilesetKind::LavaAuto:
            fileName = "IsometricAutoLavaTiles.png";
            break;
        case TilesetKind::PitAuto:
            fileName = "IsometricAutoPitTiles.png";
            break;
        case TilesetKind::ChasmAuto:
            fileName = "IsometricAutoChasmTiles.png";
            break;
        case TilesetKind::TransitionGrassWater:
            fileName = "IsometricTransitionGrassWaterTiles.png";
            break;
        case TilesetKind::TransitionGrassDesert:
            fileName = "IsometricTransitionGrassDesertTiles.png";
            break;
        case TilesetKind::TransitionGrassSnow:
            fileName = "IsometricTransitionGrassSnowTiles.png";
            break;
        case TilesetKind::TransitionGrassRock:
            fileName = "IsometricTransitionGrassRockTiles.png";
            break;
        case TilesetKind::TransitionGrassSwamp:
            fileName = "IsometricTransitionGrassSwampTiles.png";
            break;
        case TilesetKind::TransitionWaterSand:
            fileName = "IsometricTransitionWaterSandTiles.png";
            break;
        case TilesetKind::TransitionSnowRock:
            fileName = "IsometricTransitionSnowRockTiles.png";
            break;
        case TilesetKind::TransitionDungeonFloorWall:
            fileName = "IsometricTransitionDungeonFloorWallTiles.png";
            break;
        case TilesetKind::IntegratedWayPrototype:
            fileName = "Generated/FantasyNeutralPrototype/IntegratedWayPrototypeTiles.png";
            break;
        case TilesetKind::MountainRangePrototype:
            fileName = "Generated/FantasyNeutralPrototype/MountainRangePrototypeTiles.png";
            break;
        case TilesetKind::StructureContextPrototype:
            fileName = "Generated/FantasyNeutralPrototype/StructureContextPrototypeTiles.png";
            break;
        case TilesetKind::Battlefield:
            fileName = "IsometricBattlefieldTiles.png";
            break;
        case TilesetKind::Count:
            break;
        }

        const std::filesystem::path path = findTilesetPath(fileName);
        unsigned int textureId = 0;
        if (!path.empty() && loadPngTexture(path, textureId, atlases[index].width, atlases[index].height))
        {
            atlases[index].textureId = static_cast<ImTextureID>(textureId);
            atlases[index].loaded = true;
        }
    }

    return atlases[index];
}

MapTileAtlas &runtimeAtlasForPath(const std::filesystem::path &path)
{
    static std::unordered_map<std::string, MapTileAtlas> atlases;
    static std::unordered_map<std::string, bool> attemptedLoad;

    const std::string key = path.lexically_normal().string();
    if (!attemptedLoad[key])
    {
        attemptedLoad[key] = true;
        unsigned int textureId = 0;
        MapTileAtlas &atlas = atlases[key];
        if (!path.empty() && loadPngTexture(path, textureId, atlas.width, atlas.height))
        {
            atlas.textureId = static_cast<ImTextureID>(textureId);
            atlas.loaded = true;
        }
    }
    return atlases[key];
}

ImTextureID standaloneTextureId(const std::filesystem::path &path)
{
    if (path.empty())
        return 0;
    const MapTileAtlas &atlas = runtimeAtlasForPath(path);
    return atlas.loaded ? atlas.textureId : 0;
}

const MapStylePackedRuntimeAsset *runtimeAtlasAssetForId(const std::string &assetId,
                                                         bool allowCandidatePreview,
                                                         const MapTileAtlas **resolvedAtlas = nullptr)
{
    if (resolvedAtlas)
        *resolvedAtlas = nullptr;
    const MapStylePackedRuntimeAsset *asset = findMapStylePackedRuntimeAsset(assetId);
    if (!asset)
        return nullptr;
    if (!mapStylePackedRuntimeAssetDrawable(*asset, allowCandidatePreview))
        return nullptr;

    const std::filesystem::path atlasPath = mapStyleResolveExistingPath(asset->atlasPath);
    if (atlasPath.empty())
        return nullptr;

    const MapTileAtlas &atlas = runtimeAtlasForPath(atlasPath);
    if (!atlas.loaded)
        return nullptr;
    if (resolvedAtlas)
        *resolvedAtlas = &atlas;

    return asset;
}

AtlasCrop cropFromCell(TilesetKind tileset, int column, int row);
AtlasCrop mountainPrototypeCrop(const MapTileRenderContext &context);
AtlasCrop structurePrototypeCrop(const MapTileRenderContext &context);

} // namespace
