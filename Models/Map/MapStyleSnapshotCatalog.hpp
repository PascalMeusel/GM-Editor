#pragma once

#include "Models/Map/MapStyleProductionLookup.hpp"

inline const std::string &mapStyleBaseProductionSnapshotId()
{
    static const std::string snapshotId = [] {
        const std::filesystem::path v3Root = mapStyleFindFantasyNeutralV3GeneratedRoot();
        return v3Root.empty() ? std::string() : mapStyleJsonStringField(mapStyleReadTextFile(v3Root / "production_style_snapshot.json"), "snapshot_id");
    }();
    return snapshotId;
}

inline const std::string &mapStyleV31ProductionSnapshotId()
{
    static const std::string snapshotId = [] {
        const std::filesystem::path root = mapStyleFindFantasyNeutralV31GeneratedRoot();
        if (root.empty())
            return std::string();
        const std::string json = mapStyleReadTextFile(root / "production_style_snapshot.json");
        if (mapStyleJsonStringField(json, "state") != "production" ||
            mapStyleJsonStringField(json, "base_snapshot_id") != mapStyleBaseProductionSnapshotId())
            return std::string();
        return mapStyleJsonStringField(json, "snapshot_id");
    }();
    return snapshotId;
}

inline const std::string &mapStyleV32ProductionSnapshotId()
{
    static const std::string snapshotId = [] {
        const std::filesystem::path root = mapStyleFindFantasyNeutralV32GeneratedRoot();
        if (root.empty())
            return std::string();
        const std::string json = mapStyleReadTextFile(root / "production_style_snapshot.json");
        if (mapStyleJsonStringField(json, "state") != "production" ||
            mapStyleJsonStringField(json, "base_snapshot_id") != mapStyleV31ProductionSnapshotId())
            return std::string();
        return mapStyleJsonStringField(json, "snapshot_id");
    }();
    return snapshotId;
}

inline const std::string &mapStyleV33ProductionSnapshotId()
{
    static const std::string snapshotId = [] {
        const std::filesystem::path root = mapStyleFindFantasyNeutralV33GeneratedRoot();
        if (root.empty())
            return std::string();
        const std::string json = mapStyleReadTextFile(root / "production_style_snapshot.json");
        if (mapStyleJsonStringField(json, "state") != "production" ||
            mapStyleJsonStringField(json, "base_snapshot_id") != mapStyleV32ProductionSnapshotId())
            return std::string();
        return mapStyleJsonStringField(json, "snapshot_id");
    }();
    return snapshotId;
}

inline const std::string &mapStyleCurrentProductionSnapshotId()
{
    static const std::string snapshotId = [] {
        if (!mapStyleV33ProductionSnapshotId().empty())
            return mapStyleV33ProductionSnapshotId();
        if (!mapStyleV32ProductionSnapshotId().empty())
            return mapStyleV32ProductionSnapshotId();
        if (!mapStyleV31ProductionSnapshotId().empty())
            return mapStyleV31ProductionSnapshotId();
        return mapStyleBaseProductionSnapshotId();
    }();
    return snapshotId;
}

inline const std::vector<MapStyleContextualAssetCoverage> &mapStyleContextualAssetCoverage()
{
    static const std::vector<MapStyleContextualAssetCoverage> coverage = loadMapStyleContextualAssetCoverage();
    return coverage;
}

inline const std::vector<MapStylePackedRuntimeAsset> &mapStylePackedRuntimeAssets()
{
    static const std::vector<MapStylePackedRuntimeAsset> assets = loadMapStylePackedRuntimeAssets();
    return assets;
}

inline const MapStyleContextualAssetCoverage *findMapStyleContextualAssetCoverage(const std::string &styleId, MapStyleContextualAssetClass assetClass, const std::string &owner)
{
    for (const MapStyleContextualAssetCoverage &coverage : mapStyleContextualAssetCoverage())
    {
        if (coverage.styleId == styleId && coverage.assetClass == assetClass && coverage.owner == owner)
            return &coverage;
    }
    return nullptr;
}

inline const MapStylePackedRuntimeAsset *findMapStylePackedRuntimeAsset(const std::string &assetId)
{
    const std::vector<MapStylePackedRuntimeAsset> &assets = mapStylePackedRuntimeAssets();
    const auto iterator = std::lower_bound(
        assets.begin(),
        assets.end(),
        assetId,
        [](const MapStylePackedRuntimeAsset &asset, const std::string &id) {
            return asset.id < id;
        });
    return iterator != assets.end() && iterator->id == assetId ? &(*iterator) : nullptr;
}

inline bool mapStylePackedRuntimeAssetDrawable(const MapStylePackedRuntimeAsset &asset, bool allowCandidatePreview)
{
    return asset.status == "accepted" || (allowCandidatePreview && asset.status == "candidate");
}

inline std::string mapStyleContextualDrawableAtlasAssetId(const MapStyleContextualAssetResolution &resolution, bool allowCandidatePreview)
{
    if (resolution.hasExactAcceptedAsset)
        return resolution.selectedAssetId;
    if (allowCandidatePreview && resolution.hasCandidateAsset)
        return resolution.candidatePreviewAssetId;
    return {};
}

inline bool mapStyleRuntimeCandidatePreviewAllowed(MapStyleContextualAssetClass assetClass)
{
    switch (assetClass)
    {
    case MapStyleContextualAssetClass::IntegratedWayGround:
    case MapStyleContextualAssetClass::StructureContextMatrix:
    case MapStyleContextualAssetClass::BridgeSequence:
    case MapStyleContextualAssetClass::ComponentRoleSet:
        return true;
    }
    return false;
}

inline int mapStyleContextualTargetAssetCount(const std::string &styleId)
{
    int total = 0;
    for (const MapStyleContextualAssetCoverage &coverage : mapStyleContextualAssetCoverage())
    {
        if (coverage.styleId == styleId)
            total += coverage.targetAssetCount;
    }
    return total;
}

inline int mapStyleContextualAcceptedAssetCount(const std::string &styleId)
{
    int total = 0;
    for (const MapStyleContextualAssetCoverage &coverage : mapStyleContextualAssetCoverage())
    {
        if (coverage.styleId == styleId)
            total += coverage.acceptedAssetCount;
    }
    return total;
}

inline int mapStyleContextualCandidateAssetCount(const std::string &styleId)
{
    int total = 0;
    for (const MapStyleContextualAssetCoverage &coverage : mapStyleContextualAssetCoverage())
    {
        if (coverage.styleId == styleId)
            total += coverage.candidateAssetCount;
    }
    return total;
}

inline int mapStyleContextualCompletionPercent(const std::string &styleId)
{
    const int target = mapStyleContextualTargetAssetCount(styleId);
    if (target <= 0)
        return 0;
    return (mapStyleContextualAcceptedAssetCount(styleId) * 100) / target;
}
