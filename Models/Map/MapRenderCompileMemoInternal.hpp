#pragma once

#include "Models/Map/MapRenderCompileMemo.hpp"
#include <optional>
#include <vector>

namespace gm::map_render
{
using MapRenderCompileKey = std::vector<std::uint8_t>;

// Internal seam, invoked only after selection has finished. No callbacks are
// invoked here, including on failure; the compiler remains their sole owner.
struct MapRenderCompileMemoAccess
{
    static std::optional<MapRenderCompileKey> key(
        MapRenderCompileMemo &, const MapModel &, const MapRenderPlanRequest &,
        const MapRenderPlan &) noexcept;
    static bool reuse(MapRenderCompileMemo &, const MapRenderCompileKey &,
                      MapRenderPlan &) noexcept;
    static void retain(MapRenderCompileMemo &, MapRenderCompileKey,
                       const MapRenderPlan &) noexcept;
};

MapRenderCompileKey encodeMapRenderCompileKey(
    const MapModel &, const MapRenderPlanRequest &, const MapRenderPlan &,
    std::size_t maximumBytes);
} // namespace gm::map_render
