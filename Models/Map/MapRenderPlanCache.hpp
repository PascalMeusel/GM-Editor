#pragma once

#include "Models/Map/MapRenderPlan.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>

// Small UI-side cache for the immutable, renderer-independent map plan.  A
// plan is expensive enough to matter in thumbnail strips because compilation
// walks every feature/cell and calculates its content hash.  The cache keeps
// that work off unchanged frames while deliberately leaving GPU handles and
// campaign data outside the model.
//
// `contentVersion` is a caller-owned monotonic version for immutable previews
// which can be replaced without changing MapModel::revision (for example a
// newly completed generator batch).  Editable maps should increment their
// revision or pass a new contentVersion whenever their renderable content is
// changed.  Requests with a custom visibility callback are not cached: its
// captured policy cannot be compared safely.
struct MapRenderPlanCacheKey
{
    EntityId mapId;
    std::uint64_t mapRevision = 0;
    std::uint64_t contentVersion = 0;
    int width = 0, height = 0;
    std::string styleId;
    int generatorVersion = 0;
    MapLandscapeProfile landscape = MapLandscapeProfile::RiverValley;
    MapLevel level = MapLevel::World;
    MapScope scope = MapScope::World;
    GridType gridType = GridType::Square;
    MapVisibility visibility = MapVisibility::GameMasterOnly;
    MapRenderAudience audience = MapRenderAudience::GameMaster;
    std::string locale;
    std::string styleSnapshotId;
    int styleVersion = 0;
    int recipeVersion = 0;
    std::uint32_t generationSeed = 0;
    bool continuousRendering = false;
    EntityId backgroundOriginalAssetId;
    std::string backgroundOriginalProjectRelativePath;
    std::string backgroundOriginalContentHash;
    EntityId backgroundProxyAssetId;
    std::string backgroundProxyProjectRelativePath;
    std::string backgroundProxyContentHash;
    bool backgroundWorkingProxyGenerated = false;
    std::uint32_t layers = MapRenderPlanAll;
    std::uint32_t playerReleaseLayers = 0;
    bool applyFog = true;
    bool showPoliticalOverlay = false;
    bool showGrid = false;
    int lod = 0;

    bool operator==(const MapRenderPlanCacheKey &other) const
    {
        return mapId == other.mapId && mapRevision == other.mapRevision &&
               contentVersion == other.contentVersion && width == other.width && height == other.height &&
               styleId == other.styleId && generatorVersion == other.generatorVersion && landscape == other.landscape && level == other.level &&
               scope == other.scope && gridType == other.gridType &&
               visibility == other.visibility && audience == other.audience &&
               locale == other.locale && styleSnapshotId == other.styleSnapshotId &&
               styleVersion == other.styleVersion && recipeVersion == other.recipeVersion &&
               generationSeed == other.generationSeed &&
               continuousRendering == other.continuousRendering &&
               backgroundOriginalAssetId == other.backgroundOriginalAssetId &&
               backgroundOriginalProjectRelativePath ==
                   other.backgroundOriginalProjectRelativePath &&
               backgroundOriginalContentHash == other.backgroundOriginalContentHash &&
               backgroundProxyAssetId == other.backgroundProxyAssetId &&
               backgroundProxyProjectRelativePath ==
                   other.backgroundProxyProjectRelativePath &&
               backgroundProxyContentHash == other.backgroundProxyContentHash &&
               backgroundWorkingProxyGenerated ==
                   other.backgroundWorkingProxyGenerated &&
               layers == other.layers && playerReleaseLayers == other.playerReleaseLayers &&
               applyFog == other.applyFog &&
               showPoliticalOverlay == other.showPoliticalOverlay &&
               showGrid == other.showGrid && lod == other.lod;
    }
};

inline int mapRenderPlanLodForScale(float cellSize)
{
    if (!std::isfinite(cellSize) || cellSize <= 0.0f)
        return 0;
    if (cellSize < 2.75f)
        return 2;
    if (cellSize < 6.5f)
        return 1;
    return 0;
}

inline MapRenderPlanCacheKey makeMapRenderPlanCacheKey(
    const MapModel &map,
    const MapRenderPlanRequest &request,
    std::uint64_t contentVersion,
    float cellSize)
{
    MapRenderPlanCacheKey key;
    key.mapId = map.metadata.id;
    key.mapRevision = map.revision;
    key.contentVersion = contentVersion;
    key.width = map.width;
    key.height = map.height;
    key.styleId = map.generation.recipe.styleId;
    key.recipeVersion = map.generation.recipe.recipeVersion;
    key.generatorVersion = map.generation.recipe.generatorVersion;
    key.landscape = map.generation.recipe.landscape;
    key.level = map.atlasLevel;
    key.scope = map.scope;
    key.gridType = map.gridType;
    key.visibility = map.visibility;
    key.audience = request.audience;
    key.locale = request.locale;
    key.styleSnapshotId = map.generation.recipe.styleSnapshotId;
    key.styleVersion = map.generation.recipe.styleVersion;
    key.generationSeed = map.generation.recipe.seed;
    key.continuousRendering = map.generation.continuousRendering;
    if (map.importedReference.enabled)
    {
        key.backgroundOriginalAssetId = map.importedReference.assetId;
        key.backgroundOriginalProjectRelativePath =
            map.importedReference.projectRelativePath;
        key.backgroundOriginalContentHash = map.importedReference.contentHash;
        key.backgroundProxyAssetId = map.importedReference.workingProxyAssetId;
        key.backgroundProxyProjectRelativePath =
            map.importedReference.workingProxyProjectRelativePath;
        key.backgroundProxyContentHash =
            map.importedReference.workingProxyContentHash;
        key.backgroundWorkingProxyGenerated =
            map.importedReference.workingProxyGenerated;
    }
    else
    {
        key.backgroundOriginalProjectRelativePath = map.backgroundImagePath;
    }
    key.layers = request.layers;
    key.playerReleaseLayers = request.audience == MapRenderAudience::Player
                                  ? mapPlayerReleasedRenderLayers(map)
                                  : 0u;
    key.applyFog = request.applyFog;
    key.showPoliticalOverlay = request.showPoliticalOverlay;
    key.showGrid = request.showGrid;
    key.lod = mapRenderPlanLodForScale(cellSize);
    return key;
}

class MapRenderPlanCache
{
public:
    explicit MapRenderPlanCache(std::size_t maximumEntries = 24)
        : _maximumEntries(std::max<std::size_t>(1, maximumEntries))
    {
    }

    std::shared_ptr<const MapRenderPlan> getOrCompile(
        const MapModel &map,
        const MapRenderPlanRequest &request,
        std::uint64_t contentVersion = 0,
        float cellSize = 0.0f)
    {
        // std::function does not expose a stable identity for captured policy.
        // Compiling these uncommon requests is safer than serving stale or
        // incorrectly published content.
        if (request.additionalGeneratedFeatureVisibility)
        {
            ++_missCount;
            ++_compileCount;
            return std::make_shared<MapRenderPlan>(compileMapRenderPlan(map, request));
        }

        const MapRenderPlanCacheKey key =
            makeMapRenderPlanCacheKey(map, request, contentVersion, cellSize);
        const auto found = std::find_if(_entries.begin(), _entries.end(),
                                        [&](const Entry &entry) {
                                            return entry.key == key;
                                        });
        if (found != _entries.end())
        {
            // A belt-and-braces guard for externally changed visibility,
            // snapshot, release consent, or atlas revision.
            if (mapRenderPlanMatchesMap(*found->plan, map))
            {
                ++_hitCount;
                _entries.splice(_entries.begin(), _entries, found);
                return _entries.front().plan;
            }
            _entries.erase(found);
        }

        ++_missCount;
        ++_compileCount;
        auto plan = std::make_shared<MapRenderPlan>(compileMapRenderPlan(map, request));
        _entries.push_front({key, plan});
        while (_entries.size() > _maximumEntries)
            _entries.pop_back();
        return plan;
    }

    void invalidateMap(const EntityId &mapId)
    {
        _entries.remove_if([&](const Entry &entry) {
            return entry.key.mapId == mapId;
        });
    }

    void clear() { _entries.clear(); }
    std::size_t size() const { return _entries.size(); }
    std::size_t maximumEntries() const { return _maximumEntries; }
    std::uint64_t hitCount() const { return _hitCount; }
    std::uint64_t missCount() const { return _missCount; }
    std::uint64_t compileCount() const { return _compileCount; }

private:
    struct Entry
    {
        MapRenderPlanCacheKey key;
        std::shared_ptr<const MapRenderPlan> plan;
    };

    std::size_t _maximumEntries = 24;
    std::list<Entry> _entries;
    std::uint64_t _hitCount = 0;
    std::uint64_t _missCount = 0;
    std::uint64_t _compileCount = 0;
};
