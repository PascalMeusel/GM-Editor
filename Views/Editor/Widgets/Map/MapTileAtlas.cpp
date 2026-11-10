#include "Views/Editor/Widgets/Map/MapTileAtlasCrops.hpp"

ImTextureID getMapStandaloneTextureId(const std::filesystem::path &path)
{
    return standaloneTextureId(path);
}

bool hasAtlasTile(MapTileKind kind)
{
    AtlasCrop crop;
    if (!cropForTile(kind, crop))
        return false;
    return atlasForTileset(crop.tileset).loaded;
}

ImTextureID getAtlasTileTextureId(MapTileKind kind)
{
    return getAtlasTileTextureId(kind, false, false, false, false, 0, 0);
}

ImTextureID getAtlasTileTextureId(MapTileKind kind, bool sameNorth, bool sameEast, bool sameSouth, bool sameWest, int tileX, int tileY)
{
    return getAtlasTileTextureId(contextFromLegacyMask(kind, sameNorth, sameEast, sameSouth, sameWest, tileX, tileY));
}

ImTextureID getAtlasTileTextureId(const MapTileRenderContext &context)
{
    AtlasCrop crop;
    if (!autoCropForTile(context, crop))
        cropForTile(context.kind, crop);
    if (!atlasForTileset(crop.tileset).loaded)
        return 0;
    return atlasForTileset(crop.tileset).textureId;
}

void getAtlasTileUV(MapTileKind kind, ImVec2 &uv0, ImVec2 &uv1)
{
    getAtlasTileUV(kind, uv0, uv1, false, false, false, false, 0, 0);
}

void getAtlasTileUV(MapTileKind kind, ImVec2 &uv0, ImVec2 &uv1, bool sameNorth, bool sameEast, bool sameSouth, bool sameWest, int tileX, int tileY)
{
    getAtlasTileUV(contextFromLegacyMask(kind, sameNorth, sameEast, sameSouth, sameWest, tileX, tileY), uv0, uv1);
}

void getAtlasTileUV(const MapTileRenderContext &context, ImVec2 &uv0, ImVec2 &uv1)
{
    AtlasCrop crop;
    if (!autoCropForTile(context, crop))
        cropForTile(context.kind, crop);

    if (!atlasForTileset(crop.tileset).loaded)
    {
        uv0 = ImVec2(0.0f, 0.0f);
        uv1 = ImVec2(1.0f, 1.0f);
        return;
    }

    uv0 = ImVec2(crop.u0, crop.v0);
    uv1 = ImVec2(crop.u1, crop.v1);
}

int getTerrainTransitionOverlayCount(const MapTileRenderContext &context)
{
    return static_cast<int>(transitionOverlayCrops(context).size());
}

ImTextureID getTerrainTransitionOverlayTextureId(const MapTileRenderContext &context, int overlayIndex)
{
    const std::vector<AtlasCrop> crops = transitionOverlayCrops(context);
    if (overlayIndex < 0 || overlayIndex >= static_cast<int>(crops.size()))
        return 0;

    const AtlasCrop &crop = crops[static_cast<size_t>(overlayIndex)];
    return atlasForTileset(crop.tileset).textureId;
}

void getTerrainTransitionOverlayUV(const MapTileRenderContext &context, int overlayIndex, ImVec2 &uv0, ImVec2 &uv1)
{
    const std::vector<AtlasCrop> crops = transitionOverlayCrops(context);
    if (overlayIndex < 0 || overlayIndex >= static_cast<int>(crops.size()))
    {
        uv0 = ImVec2(0.0f, 0.0f);
        uv1 = ImVec2(1.0f, 1.0f);
        return;
    }

    const AtlasCrop &crop = crops[static_cast<size_t>(overlayIndex)];
    uv0 = ImVec2(crop.u0, crop.v0);
    uv1 = ImVec2(crop.u1, crop.v1);
}

bool hasIntegratedWayPrototypeTile(const MapWayRenderContext &context)
{
    (void)context;
    // The generated prototype sheet is preserved as source art, but its cells are not mask-accurate enough for runtime roads.
    return false;
}

ImTextureID getIntegratedWayPrototypeTextureId()
{
    return atlasForTileset(TilesetKind::IntegratedWayPrototype).textureId;
}

void getIntegratedWayPrototypeUV(const MapWayRenderContext &context, ImVec2 &uv0, ImVec2 &uv1)
{
    const AtlasCrop crop = cropFromCell(
        TilesetKind::IntegratedWayPrototype,
        integratedWayPrototypeColumn(context),
        integratedWayPrototypeRow(context));
    uv0 = ImVec2(crop.u0, crop.v0);
    uv1 = ImVec2(crop.u1, crop.v1);
}

bool hasRuntimeAtlasAsset(const std::string &assetId, bool allowCandidatePreview)
{
    return runtimeAtlasAssetForId(assetId, allowCandidatePreview) != nullptr;
}

bool getRuntimeAtlasAssetDrawData(const std::string &assetId, MapRuntimeAtlasAssetDrawData &data, bool allowCandidatePreview)
{
    data = {};
    const MapTileAtlas *atlas = nullptr;
    const MapStylePackedRuntimeAsset *asset = runtimeAtlasAssetForId(assetId, allowCandidatePreview, &atlas);
    if (!asset || !atlas || !atlas->textureId || atlas->width <= 0 || atlas->height <= 0)
        return false;

    data.textureId = atlas->textureId;
    data.uv0 = ImVec2(static_cast<float>(asset->x) / static_cast<float>(atlas->width),
                      static_cast<float>(asset->y) / static_cast<float>(atlas->height));
    data.uv1 = ImVec2(static_cast<float>(asset->x + asset->width) / static_cast<float>(atlas->width),
                      static_cast<float>(asset->y + asset->height) / static_cast<float>(atlas->height));
    return true;
}

ImTextureID getRuntimeAtlasAssetTextureId(const std::string &assetId, bool allowCandidatePreview)
{
    MapRuntimeAtlasAssetDrawData data;
    return getRuntimeAtlasAssetDrawData(assetId, data, allowCandidatePreview) ? data.textureId : 0;
}

void getRuntimeAtlasAssetUV(const std::string &assetId, ImVec2 &uv0, ImVec2 &uv1, bool allowCandidatePreview)
{
    MapRuntimeAtlasAssetDrawData data;
    if (!getRuntimeAtlasAssetDrawData(assetId, data, allowCandidatePreview))
    {
        uv0 = ImVec2(0.0f, 0.0f);
        uv1 = ImVec2(1.0f, 1.0f);
        return;
    }
    uv0 = data.uv0;
    uv1 = data.uv1;
}
