#pragma once

#include "Models/Map/MapModels.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4005)
#endif
#include <windows.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#else
#include <unistd.h>
#endif

struct MapStylePackInfo
{
    std::string styleId;
    std::string displayName;
    std::string version;
    std::vector<std::string> supportedScales;
    std::vector<MapTileKind> supportedTileKinds;
};

struct MapStyleContextualAssetMetadata
{
    std::string role;
    std::string spanPosition;
    std::string contextGround;
    std::string contextEntranceType;
    std::string contextOrientation;
};

enum class MapStyleContextualAssetClass
{
    IntegratedWayGround,
    BridgeSequence,
    ComponentRoleSet,
    StructureContextMatrix
};

struct MapStyleContextualAssetCoverage
{
    std::string styleId;
    std::string sourceManifest;
    std::string workOrderId;
    MapStyleContextualAssetClass assetClass = MapStyleContextualAssetClass::IntegratedWayGround;
    std::string owner;
    int targetAssetCount = 0;
    int acceptedAssetCount = 0;
    int candidateAssetCount = 0;
    std::vector<std::string> qaLabels;
    std::vector<std::string> acceptedAssetIds;
    std::vector<std::string> candidateAssetIds;
    std::unordered_map<std::string, MapStyleContextualAssetMetadata> assetMetadataById;
};

struct MapStyleContextualAssetRequest
{
    std::string styleId = "fantasy_neutral";
    MapStyleContextualAssetClass assetClass = MapStyleContextualAssetClass::IntegratedWayGround;
    std::string owner;
    std::string semanticContext;
    int tileX = 0;
    int tileY = 0;
};

struct MapStyleContextualAssetResolution
{
    bool hasExactAcceptedAsset = false;
    bool hasCandidateAsset = false;
    bool usedFallback = true;
    std::string requestOwner;
    std::string requestSemanticContext;
    std::string workOrderId;
    std::string selectedAssetId;
    std::string candidatePreviewAssetId;
    std::string fallbackReason;
    std::string qaLabel;
    int targetAssetCount = 0;
    int acceptedAssetCount = 0;
    int candidateAssetCount = 0;
};

struct MapStylePackedRuntimeAsset
{
    std::string id;
    std::string workOrderId;
    std::string assetClass;
    std::string owner;
    std::string atlasPath;
    std::string status;
    std::string role;
    std::string semanticRole;
    // V3.1 corrected a small set of mislabeled V3 source paintings. Keep the
    // base semantic role intact so an explicitly pinned V3 snapshot remains
    // immutable; V3.1 and descendants apply this alternate role at resolve time.
    std::string v31SemanticRoleOverride;
    std::vector<std::string> supportedScales;
    float minimumZoom = 0.0f;
    float maximumZoom = 1000.0f;
    int footprintWidth = 1;
    int footprintHeight = 1;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    // Direction of the authored asset's long axis in source-image space.
    // Rotated feature stamps subtract this angle from their map-space spine
    // angle. Keeping it in the immutable manifest avoids renderer-specific
    // assumptions when a corrective art overlay changes its composition.
    float sourceAxisRadians = 0.0f;
    int selectionWeight = 1;
};

inline std::vector<MapTileKind> currentMapLogicalTileKinds()
{
    return {
        MapTileKind::Empty,
        MapTileKind::Water,
        MapTileKind::Forest,
        MapTileKind::Mountain,
        MapTileKind::Hill,
        MapTileKind::Road,
        MapTileKind::Settlement,
        MapTileKind::Building,
        MapTileKind::Room,
        MapTileKind::Wall,
        MapTileKind::Door,
        MapTileKind::Cave,
        MapTileKind::Stairs,
        MapTileKind::Hazard,
        MapTileKind::Landmark,
        MapTileKind::Dirt,
        MapTileKind::Stone,
        MapTileKind::Snow,
        MapTileKind::Desert,
        MapTileKind::Swamp,
        MapTileKind::Lava,
        MapTileKind::Ice,
        MapTileKind::Pit,
        MapTileKind::Rubble,
        MapTileKind::Cover,
        MapTileKind::Chasm,
        MapTileKind::Bridge};
}

inline const std::string &defaultMapStyleId()
{
    static const std::string styleId = "fantasy_neutral";
    return styleId;
}

inline const std::vector<MapStylePackInfo> &builtInMapStylePacks()
{
    static const std::vector<MapStylePackInfo> styles = {
        {"fantasy_neutral", "Fantasy Neutral", "0.1.0", {"world", "local", "city", "dungeon", "battle"}, currentMapLogicalTileKinds()},
        {"illustrated_atlas", "Illustrated Atlas", "6.0.0", {"world", "local", "region", "site", "tactical", "dungeon"}, currentMapLogicalTileKinds()},
        {"fantasy_dark_dungeon", "Fantasy Dark Dungeon", "planned", {"local", "dungeon", "battle"}, {}},
        {"parchment_symbolic", "Parchment Symbolic", "planned", {"world", "local"}, {}},
        {"sci_fi_frontier", "Sci-Fi Frontier", "planned", {"world", "local", "city", "dungeon", "battle"}, {}},
        {"post_apocalypse", "Post Apocalypse", "planned", {"world", "local", "city", "dungeon", "battle"}, {}}};
    return styles;
}

inline const MapStylePackInfo *findMapStylePack(const std::string &styleId)
{
    for (const MapStylePackInfo &style : builtInMapStylePacks())
    {
        if (style.styleId == styleId)
            return &style;
    }
    return nullptr;
}

inline bool mapStyleSupportsTileKind(const std::string &styleId, MapTileKind kind)
{
    const MapStylePackInfo *style = findMapStylePack(styleId);
    if (!style)
        return false;

    for (MapTileKind supported : style->supportedTileKinds)
    {
        if (supported == kind)
            return true;
    }
    return false;
}

inline const char *mapStyleContextualAssetClassLabel(MapStyleContextualAssetClass assetClass)
{
    switch (assetClass)
    {
    case MapStyleContextualAssetClass::IntegratedWayGround:
        return "integrated_way_ground";
    case MapStyleContextualAssetClass::BridgeSequence:
        return "bridge_sequence";
    case MapStyleContextualAssetClass::ComponentRoleSet:
        return "component_role_set";
    case MapStyleContextualAssetClass::StructureContextMatrix:
        return "structure_context_matrix";
    }
    return "unknown";
}

inline const char *mapStylePrimaryQaLabel(MapStyleContextualAssetClass assetClass)
{
    switch (assetClass)
    {
    case MapStyleContextualAssetClass::IntegratedWayGround:
        return "road_overlay_shortcut";
    case MapStyleContextualAssetClass::BridgeSequence:
        return "bridge_middle_repetition";
    case MapStyleContextualAssetClass::ComponentRoleSet:
        return "range_not_multitile";
    case MapStyleContextualAssetClass::StructureContextMatrix:
        return "tower_cave_grass_halo";
    }
    return "style_family_fallback";
}

inline const char *mapStyleFallbackAssetId(MapStyleContextualAssetClass assetClass)
{
    switch (assetClass)
    {
    case MapStyleContextualAssetClass::IntegratedWayGround:
        return "procedural_way_overlay";
    case MapStyleContextualAssetClass::BridgeSequence:
        return "runtime_v1:bridge_single_tile";
    case MapStyleContextualAssetClass::ComponentRoleSet:
        return "runtime_v1:mountain_contextual_symbol";
    case MapStyleContextualAssetClass::StructureContextMatrix:
        return "runtime_v1:structure_symbol";
    }
    return "runtime_v1:style_family_fallback";
}

inline const char *mapStyleFallbackReason(MapStyleContextualAssetClass assetClass)
{
    switch (assetClass)
    {
    case MapStyleContextualAssetClass::IntegratedWayGround:
        return "missing_integrated_way_on_ground_asset";
    case MapStyleContextualAssetClass::BridgeSequence:
        return "missing_bridge_sequence_assets";
    case MapStyleContextualAssetClass::ComponentRoleSet:
        return "missing_mountain_component_role_asset";
    case MapStyleContextualAssetClass::StructureContextMatrix:
        return "missing_structure_ground_context_asset";
    }
    return "missing_style_family_coverage";
}

inline const char *mapStyleTerrainFamilyKey(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty:
        return "grass_open";
    case MapTileKind::Dirt:
    case MapTileKind::Road:
        return "dirt";
    case MapTileKind::Stone:
        return "stone_paved";
    case MapTileKind::Water:
        return "water_shallow";
    case MapTileKind::Room:
        return "dungeon_floor_stone";
    case MapTileKind::Cave:
        return "cave_floor";
    case MapTileKind::Snow:
        return "snow";
    case MapTileKind::Desert:
        return "desert_dune";
    case MapTileKind::Swamp:
        return "swamp";
    case MapTileKind::Ice:
        return "ice";
    case MapTileKind::Chasm:
        return "void_chasm";
    default:
        return "";
    }
}

inline const char *mapStyleWayFamilyKey(MapWayKind kind)
{
    switch (kind)
    {
    case MapWayKind::Trail:
        return "way_trail_narrow";
    case MapWayKind::DirtRoad:
        return "way_dirt_road";
    case MapWayKind::StoneRoad:
        return "way_stone_road";
    case MapWayKind::River:
        return "way_river_stream";
    case MapWayKind::Canal:
        return "way_canal";
    case MapWayKind::DungeonCorridor:
        return "way_dungeon_corridor";
    case MapWayKind::CaveTunnel:
        return "way_cave_tunnel";
    case MapWayKind::None:
        return "";
    }
    return "";
}

inline const char *mapStyleWayGroundFamilyKey(MapWayKind wayKind, MapTileKind groundKind)
{
    if (groundKind == MapTileKind::Road && wayKind == MapWayKind::StoneRoad)
        return "stone_paved";
    return mapStyleTerrainFamilyKey(groundKind);
}

inline std::string mapStyleIntegratedWayOwnerKey(MapWayKind wayKind, MapTileKind groundKind)
{
    const std::string way = mapStyleWayFamilyKey(wayKind);
    const std::string ground = mapStyleWayGroundFamilyKey(wayKind, groundKind);
    if (way.empty() || ground.empty())
        return "";
    return way + ":" + ground;
}

inline std::string mapStyleBridgeSequenceOwnerKey(MapWayKind approachWayKind, MapTileKind crossingKind)
{
    if (crossingKind != MapTileKind::Water && crossingKind != MapTileKind::Swamp && crossingKind != MapTileKind::Chasm && crossingKind != MapTileKind::Bridge)
        return "";
    if (approachWayKind == MapWayKind::StoneRoad || approachWayKind == MapWayKind::Canal)
        return "bridge_stone:water_shallow_or_canal";
    return "bridge_wood_simple:water_shallow";
}

inline std::string mapStyleComponentRoleOwnerKey(MapTileKind kind, MapSymbolVariantMode mode)
{
    if (kind == MapTileKind::Mountain && mode != MapSymbolVariantMode::Individual)
        return "mountain_range_rocky";
    return "";
}

inline std::string mapStyleStructureContextOwnerKey(MapTileKind kind)
{
    if (kind == MapTileKind::Settlement)
        return "village_cluster";
    if (kind == MapTileKind::Building)
        return "settlement_house";
    if (kind == MapTileKind::Landmark)
        return "guard_tower";
    return "";
}
