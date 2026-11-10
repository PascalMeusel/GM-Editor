#pragma once

#include "Models/Map/MapStyleRuntimeAssets.hpp"

inline const std::vector<MapStylePackedRuntimeAsset> &mapStylePackedRuntimeAssets();

inline const char *mapStyleScaleKey(MapLevel level)
{
    switch (level)
    {
    case MapLevel::World: return "world";
    case MapLevel::Region: return "region";
    case MapLevel::Site: return "site";
    case MapLevel::Tactical: return "tactical";
    }
    return "region";
}

inline const MapRenderScaleProfile &mapRenderScaleProfile(MapLevel level)
{
    static const MapRenderScaleProfile world{
        MapLevel::World, 12.0f, 0.035f, 8.0f, 9.0f, 0.16f, 0.18f, 3.20f, 1.4f, true, false, false, false};
    static const MapRenderScaleProfile region{
        MapLevel::Region, 8.0f, 0.075f, 5.0f, 6.0f, 0.32f, 0.76f, 1.55f, 1.35f, true, false, false, false};
    static const MapRenderScaleProfile site{
        MapLevel::Site, 5.0f, 0.15f, 3.0f, 4.0f, 0.42f, 0.92f, 3.05f, 2.65f, true, false, false, true};
    static const MapRenderScaleProfile tactical{
        MapLevel::Tactical, 3.0f, 0.28f, 2.0f, 3.0f, 0.48f, 1.0f, 2.35f, 1.9f, false, true, true, true};
    switch (level)
    {
    case MapLevel::World: return world;
    case MapLevel::Region: return region;
    case MapLevel::Site: return site;
    case MapLevel::Tactical: return tactical;
    }
    return region;
}

// Detailed material atlases are intentionally an LOD, not a prerequisite for
// drawing the map. At fit-to-window zoom an 80x80 tactical map would otherwise
// submit thousands of tiny textured quads that contribute no readable detail.
// The continuous colour field, curves and scale-appropriate symbols remain
// visible; zooming in crosses the threshold and restores the painterly surface.
inline bool mapRenderUsesDetailedTerrainMaterials(MapLevel level, float cellSize)
{
    switch (level)
    {
    // The accepted World "macro materials" are complete painted swatches,
    // not truly seamless continental fills. Repeating them exposes their
    // framing as 6-12 cell squares (most visibly across oceans). World maps
    // therefore use the continuous low-frequency colour field at every zoom;
    // scale-specific symbols, curves, and mountain clusters retain the art
    // direction without turning the atlas into a tiled battle map.
    case MapLevel::World: return false;
    case MapLevel::Region: return cellSize >= 16.0f;
    case MapLevel::Site: return cellSize >= 18.0f;
    case MapLevel::Tactical: return cellSize >= 18.0f;
    }
    return cellSize >= 16.0f;
}

inline bool mapStyleAssetSupportsScale(const MapStylePackedRuntimeAsset &asset, MapLevel level)
{
    if (asset.supportedScales.empty())
        return true;
    const std::string key = mapStyleScaleKey(level);
    return std::find(asset.supportedScales.begin(), asset.supportedScales.end(), key) != asset.supportedScales.end() ||
           (level == MapLevel::Site && std::find(asset.supportedScales.begin(), asset.supportedScales.end(), "dungeon") != asset.supportedScales.end());
}

inline std::string mapStyleProductionLookupKey(MapLevel level, const std::string &role, const std::string &semanticRole)
{
    return std::string(mapStyleScaleKey(level)) + "|" + role + "|" + semanticRole;
}

inline const std::unordered_map<std::string, std::vector<const MapStylePackedRuntimeAsset *>> &mapStyleProductionAssetLookup()
{
    static const std::unordered_map<std::string, std::vector<const MapStylePackedRuntimeAsset *>> lookup = [] {
        std::unordered_map<std::string, std::vector<const MapStylePackedRuntimeAsset *>> result;
        constexpr MapLevel levels[] = {MapLevel::World, MapLevel::Region, MapLevel::Site, MapLevel::Tactical};
        for (const MapStylePackedRuntimeAsset &asset : mapStylePackedRuntimeAssets())
        {
            if (asset.assetClass.rfind("production_v3", 0) != 0 || asset.status != "accepted")
                continue;
            for (MapLevel level : levels)
            {
                if (!mapStyleAssetSupportsScale(asset, level))
                    continue;
                result[mapStyleProductionLookupKey(level, asset.role, asset.semanticRole)].push_back(&asset);
                if (!asset.v31SemanticRoleOverride.empty() &&
                    asset.v31SemanticRoleOverride != asset.semanticRole)
                    result[mapStyleProductionLookupKey(level, asset.role, asset.v31SemanticRoleOverride)].push_back(&asset);
                if (!asset.semanticRole.empty())
                    result[mapStyleProductionLookupKey(level, asset.role, {})].push_back(&asset);
            }
        }
        return result;
    }();
    return lookup;
}

inline const std::string &mapStyleBaseProductionSnapshotId();
inline const std::string &mapStyleV31ProductionSnapshotId();
inline const std::string &mapStyleV32ProductionSnapshotId();
inline const std::string &mapStyleV33ProductionSnapshotId();
inline const std::string &mapStyleCurrentProductionSnapshotId();

inline bool mapStyleSnapshotAcceptsAsset(const std::string &requestedSnapshotId,
                                         const MapStylePackedRuntimeAsset &asset)
{
    const std::string &snapshotId = requestedSnapshotId.empty() ? mapStyleCurrentProductionSnapshotId() : requestedSnapshotId;
    if (snapshotId.empty())
        return false;
    if (snapshotId == mapStyleBaseProductionSnapshotId())
        return asset.assetClass == "production_v3";
    if (snapshotId == mapStyleV31ProductionSnapshotId())
        return asset.assetClass == "production_v3" || asset.assetClass == "production_v3_1";
    if (snapshotId == mapStyleV32ProductionSnapshotId())
        return asset.assetClass == "production_v3" || asset.assetClass == "production_v3_1" ||
               asset.assetClass == "production_v3_2";
    if (snapshotId == mapStyleV33ProductionSnapshotId())
        return asset.assetClass == "production_v3" || asset.assetClass == "production_v3_1" ||
               asset.assetClass == "production_v3_2" || asset.assetClass == "production_v3_3";
    // Never silently substitute the newest art for an unknown immutable pin.
    return false;
}

inline bool mapStyleSnapshotAcceptsSemanticRole(const std::string &snapshotId,
                                                const MapStylePackedRuntimeAsset &asset,
                                                const std::string &requestedSemanticRole)
{
    if (requestedSemanticRole.empty() || asset.assetClass != "production_v3" ||
        asset.v31SemanticRoleOverride.empty())
        return true;
    const bool inheritsV31 = snapshotId == mapStyleV31ProductionSnapshotId() ||
                             snapshotId == mapStyleV32ProductionSnapshotId() ||
                             snapshotId == mapStyleV33ProductionSnapshotId();
    return inheritsV31
               ? requestedSemanticRole == asset.v31SemanticRoleOverride
               : requestedSemanticRole == asset.semanticRole;
}

inline const MapStylePackedRuntimeAsset *resolveMapStyleProductionAssetForSnapshot(
    const std::string &snapshotId,
    MapLevel level,
    const std::string &role,
    const std::string &semanticRole,
    int x,
    int y,
    int salt = 0,
    float zoom = 1.0f)
{
    const std::string &resolvedSnapshotId = snapshotId.empty() ? mapStyleCurrentProductionSnapshotId() : snapshotId;
    const bool v32Snapshot = !mapStyleV32ProductionSnapshotId().empty() &&
                             resolvedSnapshotId == mapStyleV32ProductionSnapshotId();
    const bool v33Snapshot = !mapStyleV33ProductionSnapshotId().empty() &&
                             resolvedSnapshotId == mapStyleV33ProductionSnapshotId();
    const bool correctedWorldSnapshot = v32Snapshot || v33Snapshot;
    // Corrective snapshot selection exclusions: a World atlas communicates forests
    // through continuous macro biome colour, never through individual tree /
    // bush stamps or the close-range fern floor texture. The old generic
    // discovery painting also depicts a planted local prop, so World landmarks
    // use the cartographic vector mark instead. Keeping these exclusions in the
    // resolver prevents an overlooked preview or export path from reintroducing
    // the scale mismatch. Earlier immutable snapshot pins retain their original
    // resolver contract.
    if (correctedWorldSnapshot && level == MapLevel::World &&
        (role == "vegetation_stamp" ||
         (role == "terrain_material" && semanticRole == "forest_floor") ||
         (role == "map_symbol" && semanticRole == "discovery")))
        return nullptr;

    const auto &lookup = mapStyleProductionAssetLookup();
    const auto found = lookup.find(mapStyleProductionLookupKey(level, role, semanticRole));
    if (found == lookup.end() && !semanticRole.empty())
        return resolveMapStyleProductionAssetForSnapshot(snapshotId, level, role, {}, x, y, salt, zoom);
    if (found == lookup.end())
        return nullptr;
    std::uint32_t hash = 2166136261u;
    const std::string key = resolvedSnapshotId + "|" + role + "|" + semanticRole + "|" + std::to_string(x) + "|" +
                            std::to_string(y) + "|" + std::to_string(salt);
    for (const unsigned char character : key)
    {
        hash ^= character;
        hash *= 16777619u;
    }
    const std::vector<const MapStylePackedRuntimeAsset *> &candidates = found->second;
    const bool preferScaleAtlas = level == MapLevel::World || level == MapLevel::Region;
    const bool replaceWorldRidges = correctedWorldSnapshot && level == MapLevel::World &&
                                    role == "landscape_stamp" && semanticRole == "connected_ridge";
    const char *preferredAssetClass = replaceWorldRidges
                                          ? (v33Snapshot ? "production_v3_3" : "production_v3_2")
                                          : (preferScaleAtlas &&
                                                     (resolvedSnapshotId == mapStyleV31ProductionSnapshotId() || correctedWorldSnapshot)
                                                 ? "production_v3_1"
                                                 : nullptr);
    int preferredCount = 0;
    int fallbackCount = 0;
    for (const MapStylePackedRuntimeAsset *asset : candidates)
    {
        if (!asset || !mapStyleSnapshotAcceptsAsset(resolvedSnapshotId, *asset) ||
            !mapStyleSnapshotAcceptsSemanticRole(resolvedSnapshotId, *asset, semanticRole) ||
            zoom < asset->minimumZoom || zoom > asset->maximumZoom ||
            (replaceWorldRidges && asset->assetClass != preferredAssetClass))
            continue;
        fallbackCount += std::max(1, asset->selectionWeight);
        if (preferredAssetClass && asset->assetClass == preferredAssetClass)
            preferredCount += std::max(1, asset->selectionWeight);
    }
    const bool usePreferred = preferredCount > 0;
    const int matchCount = usePreferred ? preferredCount : fallbackCount;
    if (matchCount == 0)
        return nullptr;
    int selected = static_cast<int>(hash % static_cast<std::uint32_t>(matchCount));
    for (const MapStylePackedRuntimeAsset *asset : candidates)
    {
        if (!asset || !mapStyleSnapshotAcceptsAsset(resolvedSnapshotId, *asset) ||
            !mapStyleSnapshotAcceptsSemanticRole(resolvedSnapshotId, *asset, semanticRole) ||
            zoom < asset->minimumZoom || zoom > asset->maximumZoom ||
            (replaceWorldRidges && asset->assetClass != preferredAssetClass) ||
            (usePreferred && asset->assetClass != preferredAssetClass))
            continue;
        const int weight = std::max(1, asset->selectionWeight);
        if (selected < weight)
            return asset;
        selected -= weight;
    }
    return nullptr;
}

inline const MapStylePackedRuntimeAsset *resolveMapStyleProductionAsset(
    MapLevel level,
    const std::string &role,
    const std::string &semanticRole,
    int x,
    int y,
    int salt = 0,
    float zoom = 1.0f)
{
    return resolveMapStyleProductionAssetForSnapshot(mapStyleCurrentProductionSnapshotId(), level, role,
                                                     semanticRole, x, y, salt, zoom);
}

inline bool mapStyleProductionSnapshotAvailable()
{
    static const bool available = std::any_of(
        mapStylePackedRuntimeAssets().begin(),
        mapStylePackedRuntimeAssets().end(),
        [](const MapStylePackedRuntimeAsset &asset) {
            return asset.assetClass.rfind("production_v3", 0) == 0 && asset.status == "accepted";
        });
    return available;
}

inline bool mapStyleProductionSnapshotAvailable(const std::string &snapshotId)
{
    const std::string &currentSnapshotId = mapStyleCurrentProductionSnapshotId();
    const std::string &baseSnapshotId = mapStyleBaseProductionSnapshotId();
    const std::string &v31SnapshotId = mapStyleV31ProductionSnapshotId();
    const std::string &v32SnapshotId = mapStyleV32ProductionSnapshotId();
    const std::string &v33SnapshotId = mapStyleV33ProductionSnapshotId();
    const std::string &resolved = snapshotId.empty() ? currentSnapshotId : snapshotId;
    if (resolved != baseSnapshotId && resolved != v31SnapshotId && resolved != v32SnapshotId &&
        resolved != v33SnapshotId)
        return false;

    static const bool v3Available = std::any_of(
        mapStylePackedRuntimeAssets().begin(),
        mapStylePackedRuntimeAssets().end(),
        [](const MapStylePackedRuntimeAsset &asset) {
            return asset.status == "accepted" && asset.assetClass == "production_v3";
        });
    static const bool v31Available = std::any_of(
        mapStylePackedRuntimeAssets().begin(),
        mapStylePackedRuntimeAssets().end(),
        [](const MapStylePackedRuntimeAsset &asset) {
            return asset.status == "accepted" && asset.assetClass == "production_v3_1";
        });
    static const bool v32Available = std::any_of(
        mapStylePackedRuntimeAssets().begin(),
        mapStylePackedRuntimeAssets().end(),
        [](const MapStylePackedRuntimeAsset &asset) {
            return asset.status == "accepted" && asset.assetClass == "production_v3_2";
        });
    static const bool v33Available = std::any_of(
        mapStylePackedRuntimeAssets().begin(),
        mapStylePackedRuntimeAssets().end(),
        [](const MapStylePackedRuntimeAsset &asset) {
            return asset.status == "accepted" && asset.assetClass == "production_v3_3";
        });
    if (resolved == baseSnapshotId)
        return v3Available;
    if (resolved == v31SnapshotId)
        return v3Available && v31Available;
    if (resolved == v32SnapshotId)
        return v3Available && v31Available && v32Available;
    return v3Available && v31Available && v32Available && v33Available;
}
