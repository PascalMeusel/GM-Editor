#pragma once

#include "Models/Map/MapStyleManifestCoverage.hpp"

inline void mapStyleAppendPackedRuntimeAssetsFromManifest(std::vector<MapStylePackedRuntimeAsset> &assets, const std::filesystem::path &manifestPath)
{
    const std::string json = mapStyleReadTextFile(manifestPath);
    if (json.empty())
        return;

    for (const std::string &assetJson : mapStyleJsonObjectArrayEntries(json, "assets"))
    {
        MapStylePackedRuntimeAsset asset;
        asset.id = mapStyleJsonStringField(assetJson, "id");
        asset.workOrderId = mapStyleJsonStringField(assetJson, "work_order_id");
        asset.assetClass = mapStyleJsonStringField(assetJson, "asset_class");
        asset.owner = mapStyleJsonStringField(assetJson, "owner");
        asset.atlasPath = mapStyleJsonStringField(assetJson, "atlas_path");
        asset.status = mapStyleJsonStringField(assetJson, "status");
        asset.x = mapStyleJsonIntField(assetJson, "x");
        asset.y = mapStyleJsonIntField(assetJson, "y");
        asset.width = mapStyleJsonIntField(assetJson, "width");
        asset.height = mapStyleJsonIntField(assetJson, "height");
        if (!asset.id.empty() && !asset.atlasPath.empty() && asset.width > 0 && asset.height > 0)
            assets.push_back(std::move(asset));
    }
}

inline void mapStyleAppendV3ProductionAssets(std::vector<MapStylePackedRuntimeAsset> &assets)
{
    const std::filesystem::path root = mapStyleFindFantasyNeutralV3GeneratedRoot();
    if (root.empty())
        return;
    const std::string snapshotJson = mapStyleReadTextFile(root / "production_style_snapshot.json");
    const std::string manifestJson = mapStyleReadTextFile(root / "pack_manifest.json");
    if (snapshotJson.empty() || manifestJson.empty() ||
        mapStyleJsonStringField(snapshotJson, "state") != "production" ||
        mapStyleJsonIntField(snapshotJson, "style_version", 0) != 3)
        return;

    std::unordered_map<std::string, bool> productionIds;
    for (const std::string &assetJson : mapStyleJsonObjectArrayEntries(snapshotJson, "assets"))
    {
        const std::string id = mapStyleJsonStringField(assetJson, "id");
        if (!id.empty())
            productionIds[id] = true;
    }

    for (const std::string &assetJson : mapStyleJsonObjectArrayEntries(manifestJson, "assets"))
    {
        MapStylePackedRuntimeAsset asset;
        asset.id = mapStyleJsonStringField(assetJson, "id");
        if (asset.id.empty() || productionIds.find(asset.id) == productionIds.end() ||
            mapStyleJsonStringField(assetJson, "status") != "accepted")
            continue;
        const std::string file = mapStyleJsonStringField(assetJson, "file");
        const std::string dimensions = mapStyleJsonObjectField(assetJson, "dimensions");
        const std::string footprint = mapStyleJsonObjectField(assetJson, "footprint");
        const std::string zoomRange = mapStyleJsonObjectField(assetJson, "zoom_range");
        asset.atlasPath = (root / file).lexically_normal().string();
        asset.status = "accepted";
        asset.assetClass = "production_v3";
        asset.role = mapStyleJsonStringField(assetJson, "role");
        asset.semanticRole = mapStyleJsonStringField(assetJson, "semantic_role");
        if (asset.role == "landscape_stamp" && asset.semanticRole == "connected_ridge")
            asset.sourceAxisRadians = -0.58800260355f;
        asset.supportedScales = mapStyleJsonStringArrayField(assetJson, "allowed_map_types");
        asset.minimumZoom = mapStyleJsonFloatField(zoomRange, "minimum", 0.35f);
        asset.maximumZoom = mapStyleJsonFloatField(zoomRange, "maximum", 4.0f);
        asset.footprintWidth = std::max(1, mapStyleJsonIntField(footprint, "width", 1));
        asset.footprintHeight = std::max(1, mapStyleJsonIntField(footprint, "height", 1));
        asset.width = std::max(1, mapStyleJsonIntField(dimensions, "width", 512));
        asset.height = std::max(1, mapStyleJsonIntField(dimensions, "height", 512));
        assets.push_back(std::move(asset));
    }
}

inline void mapStyleApplyV31ProductionOverlay(std::vector<MapStylePackedRuntimeAsset> &assets)
{
    const std::filesystem::path root = mapStyleFindFantasyNeutralV31GeneratedRoot();
    if (root.empty())
        return;
    const std::string snapshotJson = mapStyleReadTextFile(root / "production_style_snapshot.json");
    const std::string manifestJson = mapStyleReadTextFile(root / "pack_manifest.json");
    if (snapshotJson.empty() || manifestJson.empty() ||
        mapStyleJsonStringField(snapshotJson, "state") != "production" ||
        mapStyleJsonIntField(snapshotJson, "style_version", 0) != 3)
        return;

    std::unordered_map<std::string, std::string> semanticOverrides;
    for (const std::string &overrideJson : mapStyleJsonObjectArrayEntries(manifestJson, "base_asset_overrides"))
    {
        const std::string id = mapStyleJsonStringField(overrideJson, "id");
        const std::string semantic = mapStyleJsonStringField(overrideJson, "semantic_role");
        if (!id.empty() && !semantic.empty())
            semanticOverrides[id] = semantic;
    }
    for (MapStylePackedRuntimeAsset &asset : assets)
    {
        const auto replacement = semanticOverrides.find(asset.id);
        if (replacement != semanticOverrides.end())
            asset.v31SemanticRoleOverride = replacement->second;
    }

    std::unordered_map<std::string, bool> productionIds;
    for (const std::string &assetJson : mapStyleJsonObjectArrayEntries(snapshotJson, "assets"))
    {
        const std::string id = mapStyleJsonStringField(assetJson, "id");
        if (!id.empty())
            productionIds[id] = true;
    }
    for (const std::string &assetJson : mapStyleJsonObjectArrayEntries(manifestJson, "assets"))
    {
        MapStylePackedRuntimeAsset asset;
        asset.id = mapStyleJsonStringField(assetJson, "id");
        if (asset.id.empty() || productionIds.find(asset.id) == productionIds.end() ||
            mapStyleJsonStringField(assetJson, "status") != "accepted")
            continue;
        asset.atlasPath = (root / mapStyleJsonStringField(assetJson, "file")).lexically_normal().string();
        asset.status = "accepted";
        asset.assetClass = "production_v3_1";
        asset.role = mapStyleJsonStringField(assetJson, "role");
        asset.semanticRole = mapStyleJsonStringField(assetJson, "semantic_role");
        if (asset.role == "landscape_stamp" && asset.semanticRole == "connected_ridge")
            asset.sourceAxisRadians = -0.58800260355f;
        asset.supportedScales = mapStyleJsonStringArrayField(assetJson, "allowed_map_types");
        const std::string dimensions = mapStyleJsonObjectField(assetJson, "dimensions");
        const std::string footprint = mapStyleJsonObjectField(assetJson, "footprint");
        const std::string zoomRange = mapStyleJsonObjectField(assetJson, "zoom_range");
        asset.minimumZoom = mapStyleJsonFloatField(zoomRange, "minimum", 0.2f);
        asset.maximumZoom = mapStyleJsonFloatField(zoomRange, "maximum", 3.0f);
        asset.footprintWidth = std::max(1, mapStyleJsonIntField(footprint, "width", 1));
        asset.footprintHeight = std::max(1, mapStyleJsonIntField(footprint, "height", 1));
        asset.width = std::max(1, mapStyleJsonIntField(dimensions, "width", 512));
        asset.height = std::max(1, mapStyleJsonIntField(dimensions, "height", 512));
        assets.push_back(std::move(asset));
    }

}

inline void mapStyleAppendV32ProductionOverlay(std::vector<MapStylePackedRuntimeAsset> &assets)
{
    const std::filesystem::path root = mapStyleFindFantasyNeutralV32GeneratedRoot();
    if (root.empty())
        return;
    const std::string snapshotJson = mapStyleReadTextFile(root / "production_style_snapshot.json");
    const std::string manifestJson = mapStyleReadTextFile(root / "pack_manifest.json");
    if (snapshotJson.empty() || manifestJson.empty() ||
        mapStyleJsonStringField(snapshotJson, "state") != "production" ||
        mapStyleJsonIntField(snapshotJson, "style_version", 0) != 3 ||
        mapStyleJsonStringField(snapshotJson, "snapshot_id") != mapStyleJsonStringField(manifestJson, "snapshot_id") ||
        mapStyleJsonStringField(snapshotJson, "base_snapshot_id") != mapStyleJsonStringField(manifestJson, "base_snapshot_id"))
        return;

    std::unordered_map<std::string, bool> productionIds;
    for (const std::string &assetJson : mapStyleJsonObjectArrayEntries(snapshotJson, "assets"))
    {
        const std::string id = mapStyleJsonStringField(assetJson, "id");
        if (!id.empty() && mapStyleJsonStringField(assetJson, "status") == "accepted")
            productionIds[id] = true;
    }
    for (const std::string &assetJson : mapStyleJsonObjectArrayEntries(manifestJson, "assets"))
    {
        MapStylePackedRuntimeAsset asset;
        asset.id = mapStyleJsonStringField(assetJson, "id");
        if (asset.id.empty() || productionIds.find(asset.id) == productionIds.end() ||
            mapStyleJsonStringField(assetJson, "status") != "accepted")
            continue;
        const std::string file = mapStyleJsonStringField(assetJson, "file");
        const std::string dimensions = mapStyleJsonObjectField(assetJson, "dimensions");
        const std::string footprint = mapStyleJsonObjectField(assetJson, "footprint");
        const std::string zoomRange = mapStyleJsonObjectField(assetJson, "zoom_range");
        asset.atlasPath = (root / file).lexically_normal().string();
        asset.status = "accepted";
        asset.assetClass = "production_v3_2";
        asset.role = mapStyleJsonStringField(assetJson, "role");
        asset.semanticRole = mapStyleJsonStringField(assetJson, "semantic_role");
        if (asset.role == "landscape_stamp" && asset.semanticRole == "connected_ridge")
            asset.sourceAxisRadians = -0.58800260355f;
        asset.supportedScales = mapStyleJsonStringArrayField(assetJson, "allowed_map_types");
        asset.minimumZoom = mapStyleJsonFloatField(zoomRange, "minimum", 0.1f);
        asset.maximumZoom = mapStyleJsonFloatField(zoomRange, "maximum", 4.5f);
        asset.footprintWidth = std::max(1, mapStyleJsonIntField(footprint, "width", 1));
        asset.footprintHeight = std::max(1, mapStyleJsonIntField(footprint, "height", 1));
        asset.width = std::max(1, mapStyleJsonIntField(dimensions, "width", 512));
        asset.height = std::max(1, mapStyleJsonIntField(dimensions, "height", 512));
        assets.push_back(std::move(asset));
    }
}

inline void mapStyleAppendV33ProductionOverlay(std::vector<MapStylePackedRuntimeAsset> &assets)
{
    const std::filesystem::path root = mapStyleFindFantasyNeutralV33GeneratedRoot();
    if (root.empty())
        return;
    const std::string snapshotJson = mapStyleReadTextFile(root / "production_style_snapshot.json");
    const std::string manifestJson = mapStyleReadTextFile(root / "pack_manifest.json");
    if (snapshotJson.empty() || manifestJson.empty() ||
        mapStyleJsonStringField(snapshotJson, "state") != "production" ||
        mapStyleJsonIntField(snapshotJson, "style_version", 0) != 3 ||
        mapStyleJsonStringField(snapshotJson, "snapshot_id") != mapStyleJsonStringField(manifestJson, "snapshot_id") ||
        mapStyleJsonStringField(snapshotJson, "base_snapshot_id") != mapStyleJsonStringField(manifestJson, "base_snapshot_id"))
        return;

    std::unordered_map<std::string, bool> productionIds;
    for (const std::string &assetJson : mapStyleJsonObjectArrayEntries(snapshotJson, "assets"))
    {
        const std::string id = mapStyleJsonStringField(assetJson, "id");
        if (!id.empty() && mapStyleJsonStringField(assetJson, "status") == "accepted")
            productionIds[id] = true;
    }
    for (const std::string &assetJson : mapStyleJsonObjectArrayEntries(manifestJson, "assets"))
    {
        MapStylePackedRuntimeAsset asset;
        asset.id = mapStyleJsonStringField(assetJson, "id");
        if (asset.id.empty() || productionIds.find(asset.id) == productionIds.end() ||
            mapStyleJsonStringField(assetJson, "status") != "accepted")
            continue;
        const std::string file = mapStyleJsonStringField(assetJson, "file");
        const std::string dimensions = mapStyleJsonObjectField(assetJson, "dimensions");
        const std::string footprint = mapStyleJsonObjectField(assetJson, "footprint");
        const std::string zoomRange = mapStyleJsonObjectField(assetJson, "zoom_range");
        asset.atlasPath = (root / file).lexically_normal().string();
        asset.status = "accepted";
        asset.assetClass = "production_v3_3";
        asset.role = mapStyleJsonStringField(assetJson, "role");
        asset.semanticRole = mapStyleJsonStringField(assetJson, "semantic_role");
        asset.supportedScales = mapStyleJsonStringArrayField(assetJson, "allowed_map_types");
        asset.minimumZoom = mapStyleJsonFloatField(zoomRange, "minimum", 0.1f);
        asset.maximumZoom = mapStyleJsonFloatField(zoomRange, "maximum", 4.5f);
        asset.footprintWidth = std::max(1, mapStyleJsonIntField(footprint, "width", 1));
        asset.footprintHeight = std::max(1, mapStyleJsonIntField(footprint, "height", 1));
        asset.width = std::max(1, mapStyleJsonIntField(dimensions, "width", 512));
        asset.height = std::max(1, mapStyleJsonIntField(dimensions, "height", 512));
        asset.sourceAxisRadians = mapStyleJsonFloatField(assetJson, "source_axis_radians", 0.0f);
        asset.selectionWeight = std::max(1, mapStyleJsonIntField(assetJson, "selection_weight", 1));
        assets.push_back(std::move(asset));
    }
}

inline std::vector<MapStylePackedRuntimeAsset> loadMapStylePackedRuntimeAssets()
{
    std::vector<MapStylePackedRuntimeAsset> assets;
    mapStyleAppendV3ProductionAssets(assets);
    mapStyleApplyV31ProductionOverlay(assets);
    mapStyleAppendV32ProductionOverlay(assets);
    mapStyleAppendV33ProductionOverlay(assets);
    const std::filesystem::path generatedRoot = mapStyleFindFantasyNeutralV2GeneratedRoot();
    if (!generatedRoot.empty())
    {
        mapStyleAppendPackedRuntimeAssetsFromManifest(assets, generatedRoot / "packed_runtime_manifest.json");
        mapStyleAppendPackedRuntimeAssetsFromManifest(assets, generatedRoot / "packed_candidate_manifest.json");
    }

    std::sort(assets.begin(), assets.end(), [](const MapStylePackedRuntimeAsset &left, const MapStylePackedRuntimeAsset &right) {
        return left.id < right.id;
    });
    assets.erase(std::unique(
                     assets.begin(),
                     assets.end(),
                     [](const MapStylePackedRuntimeAsset &left, const MapStylePackedRuntimeAsset &right) {
                         return left.id == right.id;
                     }),
                 assets.end());
    return assets;
}
