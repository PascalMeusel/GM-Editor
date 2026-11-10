#pragma once

#include "Models/Map/MapStyleJson.hpp"

inline MapStyleContextualAssetClass mapStyleContextualAssetClassFromLabel(const std::string &label)
{
    if (label == "bridge_sequence")
        return MapStyleContextualAssetClass::BridgeSequence;
    if (label == "component_role_set")
        return MapStyleContextualAssetClass::ComponentRoleSet;
    if (label == "structure_context_matrix")
        return MapStyleContextualAssetClass::StructureContextMatrix;
    return MapStyleContextualAssetClass::IntegratedWayGround;
}

inline int mapStyleRequiredSlotCount(const std::string &workOrderJson, const std::string &contextJson)
{
    const std::vector<std::string> explicitSlots = mapStyleJsonStringArrayField(workOrderJson, "required_masks_or_roles");
    if (!explicitSlots.empty())
        return static_cast<int>(explicitSlots.size());

    const std::vector<std::string> grounds = mapStyleJsonStringOrArrayField(contextJson, "grounds");
    const std::vector<std::string> entrances = mapStyleJsonStringOrArrayField(contextJson, "entrance_types");
    if (!grounds.empty() && !entrances.empty())
        return static_cast<int>(grounds.size() * entrances.size());
    return 1;
}

inline std::string mapStyleBridgeCrossingManifestKey(std::string value)
{
    if (value.rfind("way_", 0) == 0)
        value = value.substr(4);
    return value;
}

inline std::string mapStyleJoinBridgeCrossings(const std::vector<std::string> &crossings)
{
    if (crossings.empty())
        return "";

    std::string joined;
    for (size_t index = 0; index < crossings.size(); ++index)
    {
        if (index > 0)
            joined += "_or_";
        joined += mapStyleBridgeCrossingManifestKey(crossings[index]);
    }
    return joined;
}

inline std::string mapStyleContextualCoverageOwnerKey(
    MapStyleContextualAssetClass assetClass,
    const std::string &manifestOwner,
    const std::string &contextJson)
{
    if (assetClass == MapStyleContextualAssetClass::IntegratedWayGround)
    {
        const std::string ground = mapStyleJsonStringField(contextJson, "ground");
        return ground.empty() ? manifestOwner : manifestOwner + ":" + ground;
    }

    if (assetClass == MapStyleContextualAssetClass::BridgeSequence)
    {
        const std::string crossing = mapStyleJoinBridgeCrossings(mapStyleJsonStringOrArrayField(contextJson, "crossing"));
        return crossing.empty() ? manifestOwner : manifestOwner + ":" + crossing;
    }

    return manifestOwner;
}

struct MapStyleManifestAssetRecord
{
    std::string id;
    std::string owner;
    std::string status;
    std::string workOrderId;
    MapStyleContextualAssetMetadata metadata;
};

struct MapStyleWorkOrderManifest
{
    std::string name;
    std::filesystem::path path;
    std::string json;
    std::vector<std::string> workOrders;
};

inline void mapStyleAppendManifestAssetRecords(
    std::vector<MapStyleManifestAssetRecord> &records,
    const std::filesystem::path &manifestPath)
{
    const std::string json = mapStyleReadTextFile(manifestPath);
    if (json.empty())
        return;

    for (const std::string &assetJson : mapStyleJsonObjectArrayEntries(json, "assets"))
    {
        MapStyleManifestAssetRecord record;
        record.id = mapStyleJsonStringField(assetJson, "id");
        record.owner = mapStyleJsonStringField(assetJson, "owner");
        record.status = mapStyleJsonStringField(assetJson, "status");
        record.workOrderId = mapStyleJsonStringField(assetJson, "work_order_id");
        record.metadata.role = mapStyleJsonStringField(assetJson, "structure_role");
        if (record.metadata.role.empty())
            record.metadata.role = mapStyleJsonStringField(assetJson, "role");
        record.metadata.spanPosition = mapStyleJsonStringField(assetJson, "span_position");
        const std::string contextJson = mapStyleJsonObjectField(assetJson, "context");
        if (record.metadata.spanPosition.empty())
            record.metadata.spanPosition = mapStyleJsonStringField(contextJson, "span_position");
        record.metadata.contextGround = mapStyleJsonStringField(contextJson, "ground");
        record.metadata.contextEntranceType = mapStyleJsonStringField(contextJson, "entrance_type");
        record.metadata.contextOrientation = mapStyleJsonStringField(contextJson, "orientation");
        if (!record.id.empty())
            records.push_back(std::move(record));
    }
}

inline std::filesystem::path mapStyleFindFantasyNeutralV2SourceRoot()
{
    for (const std::filesystem::path &cursor : mapStyleResourceSearchRoots())
    {
        const std::filesystem::path candidate = cursor / "Resources" / "Maps" / "Tilesets" / "Source" / "FantasyNeutral" / "v2";
        if (std::filesystem::exists(candidate / "ways.json") && std::filesystem::exists(candidate / "taxonomy.json"))
            return candidate;
    }
    return {};
}

inline std::filesystem::path mapStyleFindFantasyNeutralV2GeneratedRoot()
{
    for (const std::filesystem::path &cursor : mapStyleResourceSearchRoots())
    {
        const std::filesystem::path candidate = cursor / "Resources" / "Maps" / "Tilesets" / "Generated" / "FantasyNeutral" / "v2";
        if (std::filesystem::exists(candidate / "packed_runtime_manifest.json"))
            return candidate;
    }
    return {};
}

inline std::filesystem::path mapStyleFindFantasyNeutralV3GeneratedRoot()
{
    for (const std::filesystem::path &cursor : mapStyleResourceSearchRoots())
    {
        const std::filesystem::path candidate = cursor / "Resources" / "Maps" / "Tilesets" / "Generated" / "FantasyNeutral" / "v3";
        if (std::filesystem::exists(candidate / "pack_manifest.json") &&
            std::filesystem::exists(candidate / "production_style_snapshot.json"))
            return candidate;
    }
    return {};
}

inline std::filesystem::path mapStyleFindFantasyNeutralV31GeneratedRoot()
{
    for (const std::filesystem::path &cursor : mapStyleResourceSearchRoots())
    {
        const std::filesystem::path candidate = cursor / "Resources" / "Maps" / "Tilesets" / "Generated" / "FantasyNeutral" / "v3_1";
        if (std::filesystem::exists(candidate / "pack_manifest.json") &&
            std::filesystem::exists(candidate / "production_style_snapshot.json"))
            return candidate;
    }
    return {};
}

inline std::filesystem::path mapStyleFindFantasyNeutralV32GeneratedRoot()
{
    for (const std::filesystem::path &cursor : mapStyleResourceSearchRoots())
    {
        const std::filesystem::path candidate = cursor / "Resources" / "Maps" / "Tilesets" / "Generated" / "FantasyNeutral" / "v3_2";
        if (std::filesystem::exists(candidate / "pack_manifest.json") &&
            std::filesystem::exists(candidate / "production_style_snapshot.json"))
            return candidate;
    }
    return {};
}

inline std::filesystem::path mapStyleFindFantasyNeutralV33GeneratedRoot()
{
    for (const std::filesystem::path &cursor : mapStyleResourceSearchRoots())
    {
        const std::filesystem::path candidate = cursor / "Resources" / "Maps" / "Tilesets" / "Generated" / "FantasyNeutral" / "v3_3";
        if (std::filesystem::exists(candidate / "pack_manifest.json") &&
            std::filesystem::exists(candidate / "production_style_snapshot.json"))
            return candidate;
    }
    return {};
}

inline std::filesystem::path mapStyleResolveExistingPath(const std::filesystem::path &path)
{
    if (path.empty())
        return {};

    // Map generation runs on a worker while the editor may resolve the same
    // immutable asset paths for rendering.  A process-global unordered_map
    // would be mutated concurrently during first use.  Keep a small cache per
    // thread so lookup stays lock-free without introducing a data race.
    static thread_local std::unordered_map<std::string, std::filesystem::path> resolvedPathCache;
    const std::string cacheKey = path.lexically_normal().string();
    const auto cached = resolvedPathCache.find(cacheKey);
    if (cached != resolvedPathCache.end())
        return cached->second;

    std::filesystem::path resolved;
    if (path.is_absolute())
    {
        resolved = std::filesystem::exists(path) ? path : std::filesystem::path();
        resolvedPathCache[cacheKey] = resolved;
        return resolved;
    }

    for (const std::filesystem::path &cursor : mapStyleResourceSearchRoots())
    {
        const std::filesystem::path candidate = cursor / path;
        if (std::filesystem::exists(candidate))
        {
            resolvedPathCache[cacheKey] = candidate;
            return candidate;
        }
    }
    resolvedPathCache[cacheKey] = resolved;
    return resolved;
}

inline std::vector<MapStyleContextualAssetCoverage> loadMapStyleContextualAssetCoverage()
{
    std::vector<MapStyleContextualAssetCoverage> coverageItems;
    const std::filesystem::path sourceRoot = mapStyleFindFantasyNeutralV2SourceRoot();
    if (sourceRoot.empty())
        return coverageItems;

    const std::vector<std::pair<std::string, std::filesystem::path>> manifestPaths = {
        {"tiles.json", sourceRoot / "tiles.json"},
        {"transition_pairs.json", sourceRoot / "transition_pairs.json"},
        {"ways.json", sourceRoot / "ways.json"},
        {"taxonomy.json", sourceRoot / "taxonomy.json"}};

    std::vector<MapStyleWorkOrderManifest> workOrderManifests;
    for (const auto &manifest : manifestPaths)
    {
        MapStyleWorkOrderManifest loadedManifest;
        loadedManifest.name = manifest.first;
        loadedManifest.path = manifest.second;
        loadedManifest.json = mapStyleReadTextFile(manifest.second);
        if (loadedManifest.json.empty())
            continue;

        for (const std::string &workOrderJson : mapStyleJsonObjectArrayEntries(loadedManifest.json, "production_work_orders"))
        {
            const std::string workOrderId = mapStyleJsonStringField(workOrderJson, "id");
            if (!workOrderId.empty())
                loadedManifest.workOrders.push_back(workOrderId);
        }
        workOrderManifests.push_back(std::move(loadedManifest));
    }

    std::vector<MapStyleManifestAssetRecord> assetRecords;
    for (const MapStyleWorkOrderManifest &manifest : workOrderManifests)
    {
        if (!manifest.workOrders.empty())
            mapStyleAppendManifestAssetRecords(assetRecords, manifest.path);
    }

    for (const MapStyleWorkOrderManifest &manifest : workOrderManifests)
    {
        if (manifest.workOrders.empty())
            continue;

        for (const std::string &workOrderJson : mapStyleJsonObjectArrayEntries(manifest.json, "production_work_orders"))
        {
            const std::string assetClassLabel = mapStyleJsonStringField(workOrderJson, "asset_class");
            const MapStyleContextualAssetClass assetClass = mapStyleContextualAssetClassFromLabel(assetClassLabel);
            const std::string manifestOwner = mapStyleJsonStringField(workOrderJson, "owner");
            const std::string contextJson = mapStyleJsonObjectField(workOrderJson, "required_context");
            const int targetVariants = std::max(1, mapStyleJsonIntField(workOrderJson, "target_variants", 0));

            MapStyleContextualAssetCoverage coverage;
            coverage.styleId = defaultMapStyleId();
            coverage.sourceManifest = manifest.name;
            coverage.workOrderId = mapStyleJsonStringField(workOrderJson, "id");
            coverage.assetClass = assetClass;
            coverage.owner = mapStyleContextualCoverageOwnerKey(assetClass, manifestOwner, contextJson);
            coverage.targetAssetCount = std::max(1, mapStyleRequiredSlotCount(workOrderJson, contextJson)) * targetVariants;
            coverage.qaLabels = mapStyleJsonStringArrayField(workOrderJson, "qa_labels");

            for (const MapStyleManifestAssetRecord &asset : assetRecords)
            {
                const bool hasExplicitWorkOrder = !asset.workOrderId.empty();
                const bool matchesWorkOrder = hasExplicitWorkOrder && asset.workOrderId == coverage.workOrderId;
                const bool matchesOwner = !hasExplicitWorkOrder && (asset.owner == manifestOwner || asset.owner == coverage.owner);
                if (!matchesWorkOrder && !matchesOwner)
                    continue;

                if (asset.status == "accepted")
                    coverage.acceptedAssetIds.push_back(asset.id);
                else if (asset.status == "candidate")
                    coverage.candidateAssetIds.push_back(asset.id);
                coverage.assetMetadataById[asset.id] = asset.metadata;
            }

            std::sort(coverage.acceptedAssetIds.begin(), coverage.acceptedAssetIds.end());
            std::sort(coverage.candidateAssetIds.begin(), coverage.candidateAssetIds.end());
            coverage.acceptedAssetCount = static_cast<int>(coverage.acceptedAssetIds.size());
            coverage.candidateAssetCount = static_cast<int>(coverage.candidateAssetIds.size());
            coverageItems.push_back(std::move(coverage));
        }
    }

    return coverageItems;
}
