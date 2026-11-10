#pragma once
#include <chrono>

struct MapRenderTimings
{
    double terrain = 0, features = 0, symbols = 0;
};

// Optional thread-local instrumentation for the signed-in benchmark host.
// Normal rendering does not read a clock; CPU workers never share this sink.
inline thread_local MapRenderTimings *mapRenderTimingSink = nullptr;
template <class Draw> void measureMapRenderPhase(double MapRenderTimings::*field, Draw draw)
{
    if (!mapRenderTimingSink)
    {
        draw();
        return;
    }
    const auto started = std::chrono::steady_clock::now();
    draw();
    mapRenderTimingSink->*field +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}
