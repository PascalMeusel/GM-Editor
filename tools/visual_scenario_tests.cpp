#include "Models/UX/VisualScenarioCatalog.hpp"
#include "Models/UX/ResponsiveLayout.hpp"

#include <iostream>
#include <set>
#include <string>

namespace
{
bool require(bool condition, const std::string &message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}
}

int main()
{
    using namespace gm::ux;
    bool ok = true;
    const auto &scenarios = visualScenarios();
    const auto &viewports = visualViewports();
    const auto captures = visualCaptureMatrix();

    ok &= require(!scenarios.empty(), "visual scenario catalog must not be empty");
    ok &= require(viewports.size() == 4, "the acceptance viewport matrix must contain four layouts");
    ok &= require(captures.size() == scenarios.size() * viewports.size() * 2,
                  "every scenario must be reviewed in both locales at every viewport");

    std::set<std::string> scenarioIds;
    std::set<std::string> captureIds;
    bool hasPlayerDisplay = false;
    bool hasGermanPlayerDisplay = false;
    bool hasMapGenerationPreview = false;
    bool hasIntegratedLiveCombat = false;
    std::set<std::string> liveStates;
    for (const VisualScenario &scenario : scenarios)
    {
        ok &= require(!scenario.id.empty() && scenarioIds.insert(scenario.id).second,
                      "scenario IDs must be non-empty and unique");
        ok &= require(!scenario.surface.empty() && !scenario.seedProfile.empty() && !scenario.state.empty(),
                      scenario.id + " must define surface, seed, and state");
        if (scenario.surface == "Live")
            liveStates.insert(scenario.state);
        if (scenario.id == "map-generation-preview")
        {
            hasMapGenerationPreview = true;
            ok &= require(scenario.surface == "World" &&
                              scenario.state.find("Three ranked designs") != std::string::npos &&
                              scenario.state.find("legend") != std::string::npos,
                          "map-generation-preview must capture the understandable three-design selection state");
        }
        if (scenario.id == "live-combat")
        {
            hasIntegratedLiveCombat = scenario.surface == "Live" &&
                                      scenario.state.find("embedded turn controls") != std::string::npos &&
                                      scenario.state.find("next action") != std::string::npos &&
                                      scenario.state.find("support drawer") != std::string::npos;
        }
        if (scenario.surface == "Player Display")
        {
            hasPlayerDisplay = true;
            ok &= require(scenario.playerSafe, "Player Display fixtures must be explicitly player-safe");
        }
    }

    for (const VisualCaptureCase &capture : captures)
    {
        ok &= require(captureIds.insert(capture.id).second, "capture IDs must be unique");
        ok &= require(capture.viewport.width >= 1280 && capture.viewport.height >= 720,
                      capture.id + " is below the supported minimum layout");
        if (capture.scenario.surface == "Player Display" && capture.locale == VisualLocale::German)
            hasGermanPlayerDisplay = true;
    }

    ok &= require(liveStates.size() == 6, "all six Live phases need deterministic scenarios");
    ok &= require(hasMapGenerationPreview, "map creation needs a deterministic three-candidate visual regression scenario");
    ok &= require(hasIntegratedLiveCombat, "Live combat must capture the embedded, guided combat workspace rather than a detached legacy window");
    ok &= require(hasPlayerDisplay && hasGermanPlayerDisplay, "Player Display needs English and German safety review cases");

    const ResponsiveTwoActionLayout wideActions = responsiveTwoActionLayout(420.0f, 10.0f, 170.0f, 150.0f);
    ok &= require(wideActions.inlineActions && wideActions.firstWidth == 170.0f && wideActions.secondWidth == 150.0f,
                  "two translated modal actions should remain inline when both measured labels fit");
    const ResponsiveTwoActionLayout narrowActions = responsiveTwoActionLayout(280.0f, 10.0f, 170.0f, 150.0f);
    ok &= require(!narrowActions.inlineActions && narrowActions.firstWidth == 280.0f && narrowActions.secondWidth == 280.0f,
                  "modal actions must stack at full width when text scaling leaves insufficient row width");

    ok &= require(responsiveColumnCount(1000.0f, 8.0f, 500.0f, 3) == 1,
                  "nested live panels must stack at the supported narrow workspace width");
    ok &= require(responsiveColumnCount(1220.0f, 8.0f, 500.0f, 3) == 2,
                  "nested live panels should use two readable columns at medium workspace widths");
    ok &= require(responsiveColumnCount(1550.0f, 8.0f, 500.0f, 3) == 3,
                  "nested live panels should recover the full three-column layout on wide workspaces");

    const ResponsiveWindowLayout minimumViewportWindow = responsiveWindowLayout(
        1280.0f, 720.0f, 1080.0f, 720.0f, 640.0f, 480.0f);
    ok &= require(minimumViewportWindow.width == 1080.0f && minimumViewportWindow.height == 648.0f,
                  "large dialogs must fit inside 90 percent of the supported minimum viewport");
    ok &= require(minimumViewportWindow.maximumWidth == 1152.0f && minimumViewportWindow.maximumHeight == 648.0f,
                  "responsive dialog constraints must track the viewport work area");

    const ResponsiveWindowLayout undersizedViewportWindow = responsiveWindowLayout(
        300.0f, 200.0f, 640.0f, 480.0f, 480.0f, 320.0f);
    ok &= require(undersizedViewportWindow.width == 270.0f && undersizedViewportWindow.height == 180.0f &&
                      undersizedViewportWindow.minimumWidth == undersizedViewportWindow.maximumWidth &&
                      undersizedViewportWindow.minimumHeight == undersizedViewportWindow.maximumHeight,
                  "responsive dialog constraints must remain valid below the supported minimum viewport");
    return ok ? 0 : 1;
}
