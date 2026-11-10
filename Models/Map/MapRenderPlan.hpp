#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapIllustratedAtlas.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

enum class MapRenderAudience
{
    GameMaster,
    Player
};

enum MapRenderPlanLayer : std::uint32_t
{
    MapRenderPlanBackground = 1u << 0,
    MapRenderPlanTerrain = 1u << 1,
    MapRenderPlanWater = 1u << 2,
    MapRenderPlanWays = 1u << 3,
    MapRenderPlanSymbols = 1u << 4,
    MapRenderPlanPolitical = 1u << 5,
    MapRenderPlanLabels = 1u << 6,
    MapRenderPlanAnnotations = 1u << 7,
    MapRenderPlanRoutes = 1u << 8,
    MapRenderPlanTokens = 1u << 9,
    MapRenderPlanFog = 1u << 10,
    MapRenderPlanGrid = 1u << 11,
    MapRenderPlanAll = (1u << 12) - 1u,
    // Selection frames, control points, quality markers, brush previews, and
    // other authoring chrome are deliberately outside the immutable content
    // plan. Even a caller that supplies this bit cannot leak those helpers to
    // preview, PNG, or player output.
    MapRenderPlanInteractiveHelpers = 1u << 12
};

enum class MapRenderPrimitiveKind
{
    Background,
    TerrainSurface,
    WaterSurface,
    GeneratedGeometry,
    GeneratedSymbol,
    PoliticalGeometry,
    Label,
    AnnotationPoint,
    AnnotationRegion,
    Route,
    Token,
    FogCell,
    Grid
};

// Renderer-independent draw semantics in their required back-to-front order.
// Coordinates remain in map-cell space, so Dear ImGui/OpenGL previews and the
// CPU PNG exporter can be compared without pretending their pixels are
// already identical.
struct MapRenderPrimitive
{
    MapRenderPrimitiveKind kind = MapRenderPrimitiveKind::TerrainSurface;
    std::uint32_t layer = 0;
    EntityId sourceId;
    std::string assetRole;
    EntityId assetId;
    std::string label;
    bool closed = false;
    std::vector<MapFeaturePoint> points;
    std::vector<gm::map_render::AtlasInkPrimitive> detailInk;
};

struct MapRenderPlanRequest
{
    MapRenderAudience audience = MapRenderAudience::GameMaster;
    std::string locale = "de";
    std::uint32_t layers = MapRenderPlanAll;
    bool applyFog = true;
    bool showPoliticalOverlay = false;
    bool showGrid = false;
    // Callers may apply stricter campaign-level publication rules (for
    // example a hidden WorldPlace). This predicate can only hide features;
    // it never bypasses the built-in fail-closed player checks.
    std::function<bool(const MapGeneratedFeature &)> additionalGeneratedFeatureVisibility;
};

struct MapRenderPlan
{
    EntityId mapId;
    std::uint64_t mapRevision = 0;
    MapLevel level = MapLevel::World;
    MapScope scope = MapScope::World;
    GridType gridType = GridType::Square;
    // Visibility is presentation state, not atlas geometry. Snapshot it so an
    // already reviewed plan cannot survive a publication-state change.
    MapVisibility mapVisibility = MapVisibility::GameMasterOnly;
    MapRenderAudience audience = MapRenderAudience::GameMaster;
    std::string locale;
    std::string styleId;
    std::string styleSnapshotId;
    int styleVersion = 0;
    std::uint32_t generationSeed = 0;
    int generatorVersion = 0;
    int recipeVersion = 3;
    MapLandscapeProfile terrainLandscape = MapLandscapeProfile::RiverValley;
    bool continuousRendering = false;
    std::uint32_t layers = MapRenderPlanAll;
    // Snapshot of the per-map consent mask used to compile a Player plan.
    // This is zero for GM plans and makes category changes stale without
    // abusing the geographic/atlas revision lineage.
    std::uint32_t playerReleaseLayers = 0;
    bool applyFog = true;
    bool showPoliticalOverlay = false;
    bool showGrid = false;
    bool backgroundVisible = false;
    // A managed imported map may use a bounded working proxy on interactive
    // surfaces and the immutable full-resolution original for PNG output.
    // Bind both identities into one review plan so neither backend performs
    // an unreviewed path selection after the GM approves the preview.
    EntityId backgroundInteractiveAssetId;
    std::string backgroundInteractiveProjectRelativePath;
    std::string backgroundInteractiveContentHash;
    EntityId backgroundExportAssetId;
    std::string backgroundExportProjectRelativePath;
    std::string backgroundExportContentHash;
    std::vector<EntityId> generatedFeatureIds;
    std::vector<EntityId> manualFeatureIds;
    std::vector<EntityId> regionIds;
    std::vector<EntityId> routeIds;
    std::vector<EntityId> tokenIds;
    std::vector<MapRenderPrimitive> primitives;
    std::vector<gm::map_render::AtlasInkPrimitive> atlasInk;
    std::vector<gm::map_render::AtlasInkPrimitive> atlasWaterInk;
    std::vector<gm::map_render::AtlasInkPrimitive> atlasFeatureInk;
    gm::map_render::AtlasLabelInkMask labelInkMask;
    std::uint64_t contentHash = 0;
};

// Maximum layer set a player-facing renderer may request for this map. This
// clamp is shared by review, direct PNG export, and any future player surface;
// callers cannot bypass persisted category consent with MapRenderPlanAll.
std::uint32_t mapPlayerReleasedRenderLayers(const MapModel &map);
MapRenderPlan compileMapRenderPlan(const MapModel &map, const MapRenderPlanRequest &request);
MapModel filterMapForRenderPlan(const MapModel &map, const MapRenderPlan &plan);
bool mapRenderPlanContainsGeneratedFeature(const MapRenderPlan &plan, const EntityId &id);
inline const std::vector<MapFeaturePoint> *mapRenderPlanGeneratedGeometry(const MapRenderPlan &plan, const EntityId &id)
{
    for (const auto &primitive : plan.primitives)
        if (primitive.sourceId == id && primitive.kind == MapRenderPrimitiveKind::GeneratedGeometry)
            return &primitive.points;
    return nullptr;
}
// An ID may survive solely for its released label. Geometry renderers must
// require a content primitive rather than treating name consent as a line,
// polygon, or symbol grant.
inline bool mapRenderPlanContainsGeneratedGeometry(const MapRenderPlan &plan, const EntityId &id)
{
    for (const auto &primitive : plan.primitives)
        if (primitive.sourceId == id &&
            (primitive.kind == MapRenderPrimitiveKind::GeneratedGeometry ||
             primitive.kind == MapRenderPrimitiveKind::GeneratedSymbol ||
             primitive.kind == MapRenderPrimitiveKind::PoliticalGeometry))
            return true;
    return false;
}
bool mapRenderPlanMatchesMap(const MapRenderPlan &plan, const MapModel &map);
// Strong one-shot validation for exports and publication boundaries. The
// inexpensive matcher above is suitable for every-frame rendering and relies
// on callers bumping MapModel::revision. This variant also recompiles the
// captured audience/layer/locale contract and compares its content hash, so a
// silently mutated model cannot be exported through an already reviewed plan.
bool mapRenderPlanContentMatchesMap(const MapRenderPlan &plan, const MapModel &map);
std::uint64_t mapRenderPlanCacheKey(const MapRenderPlan &plan,
                                    int lod,
                                    int chunkX,
                                    int chunkY);
