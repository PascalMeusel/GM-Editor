#pragma once

#include "Models/Core/EntityMetadata.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

enum class MapScope
{
    World,
    Local,
    Dungeon
};

// Atlas classification is deliberately additive to MapScope. MapScope keeps
// describing the legacy renderer/editor behavior while MapLevel describes a
// map's place in a multi-scale atlas.
enum class MapLevel
{
    World,
    Region,
    Site,
    Tactical
};

// The user-facing creation contract.  Legacy MapScope/MapKind values remain
// serialized for compatibility, but new authoring flows derive them from this
// single intent so combinations such as "World / Region purpose" cannot occur.
enum class MapCreationIntent
{
    WorldOverview,
    RegionExploration,
    Settlement,
    TravelScene,
    Dungeon,
    Cave,
    TacticalEncounter,
    ImportedReference
};

enum class MapSizePreset
{
    Small,
    Standard,
    Large,
    Custom
};

enum class MapDistanceUnit
{
    Unspecified,
    Kilometers,
    Miles,
    Meters,
    Feet
};

enum class MapKind
{
    General,
    Geographic,
    Settlement,
    Dungeon,
    Interior,
    Encounter
};

enum class MapAtlasStatus
{
    Standalone,
    Linked,
    Stale,
    Invalid,
    UpdateAvailable,
    Conflict,
    Unpositioned
};

enum class MapVisibility
{
    GameMasterOnly,
    PlayersKnown,
    PlayersVisible
};

// Optional map content is never published merely because the map itself is
// marked PlayersVisible.  The GM must explicitly release each category.  New
// and legacy maps therefore fail closed while the geographic base (terrain,
// coasts, and water) remains available to the reviewed player RenderPlan.
enum class MapPlayerReleaseCategory
{
    PoliticalBorders,
    WaysAndRoutes,
    PlacesAndMarkers,
    Labels,
    TokensAndAnnotations
};

struct MapPlayerReleaseProfile
{
    bool politicalBorders = false;
    bool waysAndRoutes = false;
    bool placesAndMarkers = false;
    bool labels = false;
    bool tokensAndAnnotations = false;
};

inline bool mapPlayerReleaseCategoryEnabled(const MapPlayerReleaseProfile &profile,
                                            MapPlayerReleaseCategory category)
{
    switch (category)
    {
    case MapPlayerReleaseCategory::PoliticalBorders: return profile.politicalBorders;
    case MapPlayerReleaseCategory::WaysAndRoutes: return profile.waysAndRoutes;
    case MapPlayerReleaseCategory::PlacesAndMarkers: return profile.placesAndMarkers;
    case MapPlayerReleaseCategory::Labels: return profile.labels;
    case MapPlayerReleaseCategory::TokensAndAnnotations: return profile.tokensAndAnnotations;
    }
    return false;
}

inline bool setMapPlayerReleaseCategory(MapPlayerReleaseProfile &profile,
                                        MapPlayerReleaseCategory category,
                                        bool enabled)
{
    bool *value = nullptr;
    switch (category)
    {
    case MapPlayerReleaseCategory::PoliticalBorders: value = &profile.politicalBorders; break;
    case MapPlayerReleaseCategory::WaysAndRoutes: value = &profile.waysAndRoutes; break;
    case MapPlayerReleaseCategory::PlacesAndMarkers: value = &profile.placesAndMarkers; break;
    case MapPlayerReleaseCategory::Labels: value = &profile.labels; break;
    case MapPlayerReleaseCategory::TokensAndAnnotations: value = &profile.tokensAndAnnotations; break;
    }
    if (value == nullptr || *value == enabled)
        return false;
    *value = enabled;
    return true;
}

struct MapNormalizedRect
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
};

struct MapAtlasPlacement
{
    EntityId parentId;
    MapNormalizedRect parentRect;
    std::uint64_t parentRevision = 0;
    std::uint64_t sourceRevision = 0;
};

struct MapSpatialReference
{
    // Atlas transforms always use normalized coordinates. Physical dimensions
    // are optional and only become authoritative after explicit calibration.
    bool calibrated = false;
    float extentWidth = 0.0f;
    float extentHeight = 0.0f;
    MapDistanceUnit unit = MapDistanceUnit::Unspecified;
    float originX = 0.0f;
    float originY = 0.0f;
    float rotationDegrees = 0.0f;
};

struct MapImportedReference
{
    bool enabled = false;
    // The original file is retained byte-for-byte for future export and
    // recalibration. These legacy-named fields therefore always describe the
    // original managed campaign asset.
    EntityId assetId;
    std::string projectRelativePath;
    std::string sourceFileName;
    std::string contentHash;
    int sourceWidth = 0;
    int sourceHeight = 0;
    // Large originals receive a separately managed, EXIF-oriented PNG working
    // proxy. Renderers prefer it while export/package code retains both.
    EntityId workingProxyAssetId;
    std::string workingProxyProjectRelativePath;
    std::string workingProxyContentHash;
    int workingProxyWidth = 0;
    int workingProxyHeight = 0;
    int exifOrientation = 1;
    bool workingProxyGenerated = false;
    MapNormalizedRect crop;
    float rotationDegrees = 0.0f;
    bool gmOnly = true;
    bool semanticallyTraced = false;
};

