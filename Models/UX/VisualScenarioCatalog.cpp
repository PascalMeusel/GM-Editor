#include "VisualScenarioCatalog.hpp"

#include <sstream>

namespace gm::ux
{
const std::vector<VisualViewport> &visualViewports()
{
    static const std::vector<VisualViewport> viewports = {
        {1280, 720, 1.0f, "1280x720"},
        {1920, 1080, 1.0f, "1920x1080"},
        {3440, 1440, 1.0f, "ultrawide"},
        {1920, 1080, 1.5f, "1920x1080-150pct"},
    };
    return viewports;
}

const std::vector<VisualScenario> &visualScenarios()
{
    // Seed profiles name the deterministic fixture generator to run before a
    // capture. Keeping state selection separate from pixels makes this catalog
    // usable by both a manual review run and a future framebuffer-diff runner.
    static const std::vector<VisualScenario> scenarios = {
        {"launcher-empty", "Campaigns", "empty-library", "No recent campaigns"},
        {"launcher-recent", "Campaigns", "current-feature-tour", "Recent campaign cards"},
        {"home-empty", "Session", "empty-campaign", "Create a prepared session from the empty state"},
        {"home-populated", "Session", "current-feature-tour", "Ordered scenes, selected scene draft, rough notes and linked material; compact pane switching"},
        {"cast-directory", "Cast", "current-feature-tour", "Character selected"},
        {"ruleset-library", "Rulesets", "clockwork-bloodline", "Published versions and drafts"},
        {"ruleset-studio", "Ruleset Studio", "clockwork-bloodline", "Core Lists section"},
        {"story-board", "Story", "blackwater", "Scene selected and staged"},
        {"world-map-edit", "World", "current-feature-tour", "Persistent Edit workspace with contextual tool inspector and visible save/undo/zoom"},
        {"map-generation-preview", "World", "current-feature-tour", "Three ranked designs with legend and factual requirement status in Choose design; numeric diagnostics remain Advanced"},
        {"world-shop", "World", "current-feature-tour", "Shop inventory and transaction"},
        {"world-dungeon", "World", "current-feature-tour", "Dungeon rooms and connections"},
        {"encounter-prepare", "Encounters", "current-feature-tour", "Readiness validation"},
        {"live-setup", "Live", "current-feature-tour", "Setup phase"},
        {"live-scene", "Live", "current-feature-tour", "Scene phase"},
        {"live-exploration", "Live", "current-feature-tour", "Exploration phase"},
        {"live-combat", "Live", "current-feature-tour", "Combat phase with embedded turn controls, battlefield, next action, and support drawer"},
        {"live-break", "Live", "current-feature-tour", "Break phase"},
        {"live-wrap-up", "Live", "current-feature-tour", "Wrap-up phase"},
        {"compendium-items", "Compendium", "current-feature-tour", "Item selected"},
        {"compendium-audio", "Compendium", "current-feature-tour", "Audio library"},
        {"settings-health", "Settings", "current-feature-tour", "Project Health"},
        {"player-display", "Player Display", "current-feature-tour", "Public scene, map, and clock only", true},
    };
    return scenarios;
}

const char *visualLocaleCode(VisualLocale locale)
{
    return locale == VisualLocale::German ? "de-DE" : "en-US";
}

std::vector<VisualCaptureCase> visualCaptureMatrix()
{
    std::vector<VisualCaptureCase> cases;
    cases.reserve(visualScenarios().size() * visualViewports().size() * 2);
    for (const VisualScenario &scenario : visualScenarios())
    {
        for (const VisualViewport &viewport : visualViewports())
        {
            for (const VisualLocale locale : {VisualLocale::English, VisualLocale::German})
            {
                std::ostringstream id;
                id << scenario.id << "__" << viewport.key << "__" << visualLocaleCode(locale);
                cases.push_back({id.str(), scenario, viewport, locale});
            }
        }
    }
    return cases;
}
}
