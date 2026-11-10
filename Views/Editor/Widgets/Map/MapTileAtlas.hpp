#pragma once

#include "Models/Map/MapTileSemantics.hpp"
#include "imgui.h"

#include <filesystem>
#include <string>

struct MapTileAtlas
{
    ImTextureID textureId = 0;
    int width = 0;
    int height = 0;
    bool loaded = false;
};

// Resolve the packed asset, texture and UVs in one pass. The old convenience
// calls remain for compatibility, but hot render paths should use this bundle
// so one stamp does not repeat asset/path/atlas lookup three times.
struct MapRuntimeAtlasAssetDrawData
{
    ImTextureID textureId = 0;
    ImVec2 uv0{};
    ImVec2 uv1{1.0f, 1.0f};
};

bool hasAtlasTile(MapTileKind kind);
ImTextureID getAtlasTileTextureId(MapTileKind kind);
ImTextureID getAtlasTileTextureId(MapTileKind kind, bool sameNorth, bool sameEast, bool sameSouth, bool sameWest, int tileX, int tileY);
ImTextureID getAtlasTileTextureId(const MapTileRenderContext &context);
void getAtlasTileUV(MapTileKind kind, ImVec2 &uv0, ImVec2 &uv1);
void getAtlasTileUV(MapTileKind kind, ImVec2 &uv0, ImVec2 &uv1, bool sameNorth, bool sameEast, bool sameSouth, bool sameWest, int tileX, int tileY);
void getAtlasTileUV(const MapTileRenderContext &context, ImVec2 &uv0, ImVec2 &uv1);
int getTerrainTransitionOverlayCount(const MapTileRenderContext &context);
ImTextureID getTerrainTransitionOverlayTextureId(const MapTileRenderContext &context, int overlayIndex);
void getTerrainTransitionOverlayUV(const MapTileRenderContext &context, int overlayIndex, ImVec2 &uv0, ImVec2 &uv1);
bool hasIntegratedWayPrototypeTile(const MapWayRenderContext &context);
ImTextureID getIntegratedWayPrototypeTextureId();
void getIntegratedWayPrototypeUV(const MapWayRenderContext &context, ImVec2 &uv0, ImVec2 &uv1);
bool hasRuntimeAtlasAsset(const std::string &assetId, bool allowCandidatePreview = false);
bool getRuntimeAtlasAssetDrawData(const std::string &assetId, MapRuntimeAtlasAssetDrawData &data, bool allowCandidatePreview = false);
ImTextureID getRuntimeAtlasAssetTextureId(const std::string &assetId, bool allowCandidatePreview = false);
void getRuntimeAtlasAssetUV(const std::string &assetId, ImVec2 &uv0, ImVec2 &uv1, bool allowCandidatePreview = false);

// Loads a standalone renderer surface (for example the one-shot World
// watercolor modulation) through the same GL texture cache as packed atlas
// pages. The caller owns neither the returned texture nor its lifetime.
ImTextureID getMapStandaloneTextureId(const std::filesystem::path &path);
