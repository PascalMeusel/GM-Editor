#pragma once

#include "Models/Map/MapRenderPlan.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

enum class MapRenderPlanCompileStage
{
    Total,
    Selection,
    InitialPrimitives,
    AudienceFilter,
    FilteredPrimitives,
    FeatureInk,
    WaterInk,
    TerrainPreparation,
    TerrainInk,
    LabelInkMask,
    ContentHash,
    Count
};

inline const char *mapRenderPlanCompileStageName(MapRenderPlanCompileStage stage)
{
    constexpr const char *names[] = {
        "total_inclusive", "selection", "initial_primitives", "audience_filter",
        "filtered_primitives", "feature_ink", "water_ink", "terrain_preparation",
        "terrain_ink", "label_ink_mask", "content_hash"};
    static_assert(sizeof(names) / sizeof(names[0]) ==
                  static_cast<std::size_t>(MapRenderPlanCompileStage::Count));
    const auto index = static_cast<std::size_t>(stage);
    return index < static_cast<std::size_t>(MapRenderPlanCompileStage::Count) ? names[index] : "unknown";
}

// Caller-owned, accumulating CPU wall-clock samples. Total includes the other
// stages and must not be summed with them. Other stages do not overlap; work
// between scopes and measurement overhead are included only in Total.
// Use a fresh value per compilation, or deliberately aggregate multiple calls.
// A sink must not be shared by concurrent compilations without caller locking.
struct MapRenderPlanMeasurements
{
    struct Sample
    {
        std::uint64_t nanoseconds = 0;
        std::uint64_t calls = 0;
    };
    std::array<Sample, static_cast<std::size_t>(MapRenderPlanCompileStage::Count)> values{};
};

// The ordinary two-argument compiler remains the default and performs no clock
// reads. This distinct entry point observes the same compilation work; it does
// not change publication authority, caching, output, or caller-owned inputs.
MapRenderPlan compileMapRenderPlanMeasured(const MapModel &map,
                                          const MapRenderPlanRequest &request,
                                          MapRenderPlanMeasurements &measurements);
