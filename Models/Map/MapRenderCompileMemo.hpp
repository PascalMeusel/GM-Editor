#pragma once

#include "Models/Map/MapRenderPlan.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

struct MapRenderPlanMeasurements;

namespace gm::map_render
{
struct MapRenderCompileMemoLimits
{
    std::size_t maximumKeyBytes = 16u * 1024u * 1024u;
    std::size_t maximumDerivedBytes = 64u * 1024u * 1024u;
};

struct MapRenderCompileMemoStats
{
    // Lifetime counters; clear() does not reset them. A valid key miss can
    // also bypass admission, so these are not disjoint outcome buckets.
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t bypasses = 0;
};

struct MapRenderCompileMemoAccess;

// A single operation/review owns this synchronous, non-thread-safe memo.
// Exact bytes, including freshly evaluated visibility selections, authorize
// reuse of derived drawing data only. This is never publication authority.
// There is no filesystem access, global cache, revision shortcut or clock.
// Inputs/ambient locale must remain stable during a call, as for the ordinary
// compiler. A caller releases private retained data with clear()/destruction.
class MapRenderCompileMemo
{
  public:
    MapRenderCompileMemo() noexcept;
    explicit MapRenderCompileMemo(MapRenderCompileMemoLimits limits) noexcept;
    ~MapRenderCompileMemo();
    MapRenderCompileMemo(const MapRenderCompileMemo &) = delete;
    MapRenderCompileMemo &operator=(const MapRenderCompileMemo &) = delete;
    void clear() noexcept;
    bool empty() const noexcept;
    const MapRenderCompileMemoStats &stats() const noexcept { return stats_; }

  private:
    friend struct MapRenderCompileMemoAccess;
    struct Entry;
    std::unique_ptr<Entry> entry_;
    MapRenderCompileMemoLimits limits_;
    MapRenderCompileMemoStats stats_;
};
} // namespace gm::map_render

// Every call still performs the normal selection/callback loop and content
// hash. A bounded-key/admission/copy failure compiles normally. Callback and
// ordinary compilation exceptions keep their existing behavior.
MapRenderPlan compileMapRenderPlanWithMemo(
    const MapModel &map, const MapRenderPlanRequest &request,
    gm::map_render::MapRenderCompileMemo &memo,
    MapRenderPlanMeasurements *measurements = nullptr);

enum class MapRenderCompileStatus { Hit, Miss, Unavailable };
struct MapRenderCompileAttempt
{
    MapRenderCompileStatus status = MapRenderCompileStatus::Unavailable;
    std::optional<MapRenderPlan> plan;
};
// Fresh selection and hash on a hit; never performs geometry or admission on
// a miss. Used to adopt background preparation without a foreground rebuild.
MapRenderCompileAttempt tryCompileMapRenderPlanWithMemo(
    const MapModel &, const MapRenderPlanRequest &, gm::map_render::MapRenderCompileMemo &,
    MapRenderPlanMeasurements *measurements = nullptr);
