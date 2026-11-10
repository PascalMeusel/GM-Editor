#pragma once
#include <cstdint>
#include <functional>
class Application;

// Optional embedding/QA host controls. Defaults retain ordinary desktop behavior.
// Callbacks run on the application thread; the renderer and controllers remain
// the production implementations. A host must isolate settings/database paths.
struct ApplicationRunOptions
{
    int width = 1280;
    int height = 720;
    bool showWindow = true;
    bool maximizeOnStart = true;
    std::uint64_t maximumFrames = 0;
    std::function<void(Application &)> initialized;
    // Runs after backend input collection, immediately before ImGui::NewFrame.
    std::function<void(Application &, std::uint64_t)> beforeFrame;
    // Main back buffer is rendered; platform viewports have already swapped.
    std::function<void(Application &, std::uint64_t)> afterRender;
};
