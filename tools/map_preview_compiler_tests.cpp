#include "Models/Map/MapPreviewCompiler.hpp"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
void require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
struct Gate
{
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false, released = false;
    void block()
    {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        changed.notify_all();
        require(changed.wait_for(lock, std::chrono::seconds(10), [&] { return released; }), "Worker gate timed out");
    }
    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        require(changed.wait_for(lock, std::chrono::seconds(10), [&] { return entered; }), "Worker did not start");
    }
    void release()
    {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        changed.notify_all();
    }
};
template <class Predicate> void eventually(Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!predicate())
    {
        require(std::chrono::steady_clock::now() < deadline, "Worker did not publish its eligible result");
        std::this_thread::yield();
    }
}
MapModel fixture(const char *name)
{
    MapModel map;
    map.metadata.id = "preview-worker-fixture";
    map.name = name;
    map.width = map.height = 8;
    map.cells.resize(64);
    return map;
}
}

int main()
{
    try
    {
        Gate first;
        std::mutex seenMutex;
        std::vector<std::string> seen;
        MapPreviewCompiler worker([&](const MapModel &map, const MapRenderPlanRequest &request) {
            {
                std::lock_guard<std::mutex> lock(seenMutex);
                seen.push_back(map.name);
            }
            if (map.name == "A")
                first.block();
            if (map.name == "throw")
                throw std::runtime_error("Injected compiler failure");
            return compileMapRenderPlan(map, request);
        });
        MapRenderPlanRequest request;
        request.applyFog = false;
        auto a = fixture("A"), b = fixture("B"), c = fixture("C");
        worker.request(a, request, 1);
        first.wait();
        worker.request(b, request, 2);
        worker.request(c, request, 3);
        c.name = "mutated after submit";
        c.width = 9;
        c.cells.resize(72);
        require(worker.state().refreshing && !worker.state().current, "Blocked preview was reported current");
        first.release();
        eventually([&] { return worker.state().current; });
        auto ready = worker.state();
        require(ready.source && ready.source->name == "C" && ready.source->width == 8,
                "The worker retained mutable caller data or adopted an obsolete result");
        require(ready.discardedResults == 1, "Obsolete completion was not discarded");
        {
            std::lock_guard<std::mutex> lock(seenMutex);
            require(seen == std::vector<std::string>({"A", "C"}),
                    "Pending work was not coalesced to the latest request");
        }
        require(ready.plan->contentHash == compileMapRenderPlan(*ready.source, request).contentHash,
                "Background and authoritative compilation differ");

        // Dimensions are part of identity even if an outside caller forgot to
        // bump its serial. A plan for the old bounds must not be shown as current.
        worker.request(c, request, 3);
        eventually([&] {
            auto state = worker.state();
            return state.current && state.source->width == 9;
        });
        c.name = "throw";
        worker.request(c, request, 4);
        eventually([&] { return !worker.state().error.empty(); });
        require(!worker.state().current, "A compiler failure was marked current");
        worker.retry();
        c.name = "retry succeeded";
        worker.request(c, request, 4);
        eventually([&] { return worker.state().current; });
        require(worker.state().source->name == c.name && worker.state().error.empty(),
                "Compiler retry did not recover");
        auto player = request;
        player.audience = MapRenderAudience::Player;
        bool rejected = false;
        try
        {
            worker.request(c, player, 5);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        require(rejected, "The GM preview worker accepted player publication work");
        auto capturedPolicy = request;
        capturedPolicy.additionalGeneratedFeatureVisibility = [](const MapGeneratedFeature &) { return true; };
        rejected = false;
        try
        {
            worker.request(c, capturedPolicy, 5);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        require(rejected, "The worker accepted a callback with arbitrary captured state");

        Gate obsolete;
        MapPreviewCompiler cancelled([&](const MapModel &map, const MapRenderPlanRequest &options) {
            if (map.name == "obsolete failure")
            {
                obsolete.block();
                throw std::runtime_error("An obsolete error must not reach the new map");
            }
            return compileMapRenderPlan(map, options);
        });
        cancelled.request(fixture("obsolete failure"), request, 1);
        obsolete.wait();
        cancelled.clear();
        require(!cancelled.state().plan && !cancelled.state().refreshing, "Clearing retained a previous map");
        auto next = fixture("next map");
        next.metadata.id = "different-map";
        cancelled.request(next, request, 2);
        obsolete.release();
        eventually([&] { return cancelled.state().current; });
        require(cancelled.state().error.empty() && cancelled.state().source->metadata.id == next.metadata.id &&
                    cancelled.state().discardedResults == 1,
                "Cancellation leaked an obsolete failure or map into the new context");
        std::cout
            << "Immutable preview, latest request, cancellation, failure/retry and authoritative parity passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
