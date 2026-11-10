#include "Models/Map/MapRenderCompileMemoInternal.hpp"
#include <stdexcept>
#include <utility>

namespace gm::map_render
{
namespace
{
struct Derived
{
    std::vector<MapRenderPrimitive> primitives;
    std::vector<AtlasInkPrimitive> ink, waterInk, featureInk;
    AtlasLabelInkMask labelInkMask;

    explicit Derived(const MapRenderPlan &plan)
        : primitives(plan.primitives), ink(plan.atlasInk), waterInk(plan.atlasWaterInk),
          featureInk(plan.atlasFeatureInk), labelInkMask(plan.labelInkMask) {}

    void install(MapRenderPlan &plan) && noexcept
    {
        plan.primitives = std::move(primitives);
        plan.atlasInk = std::move(ink);
        plan.atlasWaterInk = std::move(waterInk);
        plan.atlasFeatureInk = std::move(featureInk);
        plan.labelInkMask = std::move(labelInkMask);
    }
};

class Budget
{
    std::size_t remaining_;
  public:
    explicit Budget(std::size_t bytes) : remaining_(bytes) {}
    void add(std::size_t count, std::size_t size = 1)
    {
        if (size != 0 && count > remaining_ / size)
            throw std::length_error("Derived map drawing exceeds memo budget.");
        remaining_ -= count * size;
    }
    void string(const std::string &value) { add(value.capacity()); add(1); }
    template<class T> void vector(const std::vector<T> &values) { add(values.capacity(), sizeof(T)); }
    void ink(const std::vector<AtlasInkPrimitive> &values)
    {
        vector(values);
        for (const auto &value : values) { vector(value.points); vector(value.vertexColors); }
    }
    void primitives(const std::vector<MapRenderPrimitive> &values)
    {
        vector(values);
        for (const auto &value : values)
        {
            string(value.sourceId); string(value.assetRole); string(value.assetId); string(value.label);
            vector(value.points); ink(value.detailInk);
        }
    }
};

template<class Ink1, class Ink2, class Ink3>
void checkBudget(std::size_t maximum, const std::vector<MapRenderPrimitive> &primitives,
                 const Ink1 &ink, const Ink2 &water, const Ink3 &features,
                 const AtlasLabelInkMask &mask)
{
    Budget budget(maximum);
    budget.add(sizeof(Derived));
    budget.primitives(primitives);
    budget.ink(ink); budget.ink(water); budget.ink(features);
    budget.vector(mask.covered);
}
} // namespace

struct MapRenderCompileMemo::Entry
{
    MapRenderCompileKey key;
    Derived derived;
    Entry(MapRenderCompileKey bytes, const MapRenderPlan &plan)
        : key(std::move(bytes)), derived(plan) {}
};

MapRenderCompileMemo::MapRenderCompileMemo() noexcept = default;
MapRenderCompileMemo::MapRenderCompileMemo(MapRenderCompileMemoLimits limits) noexcept : limits_(limits) {}
MapRenderCompileMemo::~MapRenderCompileMemo() = default;
void MapRenderCompileMemo::clear() noexcept { entry_.reset(); }
bool MapRenderCompileMemo::empty() const noexcept { return !entry_; }

std::optional<MapRenderCompileKey> MapRenderCompileMemoAccess::key(
    MapRenderCompileMemo &memo, const MapModel &map, const MapRenderPlanRequest &request,
    const MapRenderPlan &plan) noexcept
{
    try { return encodeMapRenderCompileKey(map, request, plan, memo.limits_.maximumKeyBytes); }
    catch (...) { ++memo.stats_.bypasses; return std::nullopt; }
}

bool MapRenderCompileMemoAccess::reuse(MapRenderCompileMemo &memo,
    const MapRenderCompileKey &key, MapRenderPlan &plan) noexcept
{
    if (!memo.entry_ || memo.entry_->key != key)
    {
        ++memo.stats_.misses;
        return false;
    }
    try
    {
        // Strong exception guarantee: never leave a partial reused plan if a
        // copy allocation fails. Fresh selection/base fields are untouched.
        auto copy = memo.entry_->derived;
        std::move(copy).install(plan);
        ++memo.stats_.hits;
        return true;
    }
    catch (...) { ++memo.stats_.bypasses; return false; }
}

void MapRenderCompileMemoAccess::retain(MapRenderCompileMemo &memo,
    MapRenderCompileKey key, const MapRenderPlan &plan) noexcept
{
    try
    {
        checkBudget(memo.limits_.maximumDerivedBytes, plan.primitives, plan.atlasInk,
                    plan.atlasWaterInk, plan.atlasFeatureInk, plan.labelInkMask);
        auto next = std::make_unique<MapRenderCompileMemo::Entry>(std::move(key), plan);
        const auto &copy = next->derived;
        checkBudget(memo.limits_.maximumDerivedBytes, copy.primitives, copy.ink,
                    copy.waterInk, copy.featureInk, copy.labelInkMask);
        memo.entry_ = std::move(next);
    }
    catch (...) { ++memo.stats_.bypasses; }
}
} // namespace gm::map_render
