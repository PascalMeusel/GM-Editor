#pragma once

#include "Models/Map/MapModels.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <string_view>
#include <vector>

// MapRouteModel deliberately persists only stable semantic endpoints.  The
// visible line is derived from the current road/trail/bridge network so old
// saves gain believable routes without a destructive schema migration.

namespace gm::map_route_presentation
{
enum class Source
{
    RoadNetwork,
    TerrainFallback,
    DirectFallback
};

struct Path
{
    Source source = Source::DirectFallback;
    std::vector<MapFeaturePoint> points;

    bool followsRoadNetwork() const { return source == Source::RoadNetwork; }
    bool isFallback() const { return source != Source::RoadNetwork; }
};

struct BuildOptions
{
    // Legacy maps have no authoritative feature graph and therefore derive
    // their network from road-like raster cells. Player plans with a feature
    // graph can disable that projection so a hidden road's cells cannot leak
    // its geometry through an otherwise public route.
    bool includeRasterNetwork = true;
    const std::vector<EntityId> *allowedGeneratedFeatureIds = nullptr;
};

inline std::string_view assetRole(Source source)
{
    switch (source)
    {
    case Source::RoadNetwork: return "route_network";
    case Source::TerrainFallback: return "route_terrain_fallback";
    case Source::DirectFallback: return "route_direct_fallback";
    }
    return "route_direct_fallback";
}

inline Source sourceFromAssetRole(std::string_view role)
{
    if (role == "route_network")
        return Source::RoadNetwork;
    if (role == "route_terrain_fallback")
        return Source::TerrainFallback;
    return Source::DirectFallback;
}

// Stable, allocation-free key for UI caches. Route presentation depends on
// semantic endpoints, the road/bridge graph, and passability/water projected
// into cells. Editors may change those values interactively before committing
// a new MapModel revision, so revision alone is not a sufficient cache key.
inline std::uint64_t fingerprint(const MapModel &map)
{
    std::uint64_t hash = 1469598103934665603ull;
    const auto appendBytes = [&](const void *data, std::size_t size) {
        const auto *bytes = static_cast<const unsigned char *>(data);
        for (std::size_t index = 0; index < size; ++index)
        {
            hash ^= bytes[index];
            hash *= 1099511628211ull;
        }
    };
    const auto appendUint64 = [&](std::uint64_t value) {
        appendBytes(&value, sizeof(value));
    };
    const auto appendString = [&](const std::string &value) {
        appendBytes(value.data(), value.size());
        const unsigned char terminator = 0xffu;
        appendBytes(&terminator, 1u);
    };
    const auto appendFloat = [&](float value) {
        std::uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        appendUint64(bits);
    };

    appendUint64(static_cast<std::uint64_t>(static_cast<std::int64_t>(map.width)));
    appendUint64(static_cast<std::uint64_t>(static_cast<std::int64_t>(map.height)));
    appendUint64(static_cast<std::uint64_t>(map.cells.size()));
    for (const MapCellModel &cell : map.cells)
    {
        appendUint64(static_cast<std::uint64_t>(cell.kind));
        appendUint64(static_cast<std::uint64_t>(cell.wayKind));
        appendUint64(static_cast<std::uint64_t>(static_cast<std::int64_t>(cell.movementCost)));
    }

    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::Road &&
            feature.kind != MapGeneratedFeatureKind::Trail &&
            feature.kind != MapGeneratedFeatureKind::Bridge)
            continue;
        appendString(feature.id);
        appendUint64(static_cast<std::uint64_t>(feature.kind));
        appendUint64(static_cast<std::uint64_t>(feature.points.size()));
        for (const MapFeaturePoint &point : feature.points)
        {
            appendFloat(point.x);
            appendFloat(point.y);
        }
    }

    appendUint64(static_cast<std::uint64_t>(map.routes.size()));
    for (const MapRouteModel &route : map.routes)
    {
        appendString(route.metadata.id);
        appendUint64(static_cast<std::uint64_t>(static_cast<std::int64_t>(route.startX)));
        appendUint64(static_cast<std::uint64_t>(static_cast<std::int64_t>(route.startY)));
        appendUint64(static_cast<std::uint64_t>(static_cast<std::int64_t>(route.endX)));
        appendUint64(static_cast<std::uint64_t>(static_cast<std::int64_t>(route.endY)));
    }
    return hash == 0 ? 1 : hash;
}
} // namespace gm::map_route_presentation

