#include "Application.hpp"
#include "Models/Map/CurrentMapArtRecipe.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "imgui_internal.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
#include "map_benchmark_capture.hpp"
#include "Models/Map/MapRenderTimings.hpp"

struct WorkspaceInteractionProbe
{
    static MapCreatorController &map(EditorWindowController &editor)
    {
        return *editor._mapCreator;
    }
    static MapPreviewCompilation preview(MapCreatorController &c)
    {
        return c._view->_canvasPreviewCompiler.state();
    }
    static void paint(MapCreatorController &c, bool alternate)
    {
        const auto palette = c.getPalette();
        const bool underground = c._draftMap.scope == MapScope::Dungeon;
        const auto wanted = underground ? (alternate ? MapTileKind::Room : MapTileKind::Cave)
                                        : (alternate ? MapTileKind::Forest : MapTileKind::Dirt);
        const auto found = std::find(palette.begin(), palette.end(), wanted);
        if (found == palette.end())
            throw std::runtime_error("Required paint terrain unavailable");
        c._editing.brushSize = 8;
        c.selectTileFromLegend(static_cast<int>(std::distance(palette.begin(), found)));
    }
    static void finishInteraction(MapCreatorController &c)
    {
        c._view->finishGeneratedFeatureGeometryEdit();
        c.endBrushStroke();
        c.finishPropertyEditUndo();
    }
    static void undo(MapCreatorController &c)
    {
        finishInteraction(c);
        c.undoLastAction();
    }
    static void redo(MapCreatorController &c)
    {
        finishInteraction(c);
        c.redoLastAction();
    }
    static bool exporting(MapCreatorController &c)
    {
        return c._transfer.pngExportTask.busy();
    }
    static bool recovering(MapCreatorController &c)
    {
        return c._history.crashJournalPending;
    }
    static MapModel draft(MapCreatorController &c)
    {
        return c._draftMap;
    }
    static void exportPng(MapCreatorController &c, const std::filesystem::path &path)
    {
        MapPngExportJob job;
        job.map = c._draftMap;
        job.options.pixelsPerCell = 16;
        job.options.applyFog = false;
        job.options.locale = "en";
        job.destination = path;
        job.authoringSerial = c._editing.authoringSerial;
        c.startPngExport(std::move(job));
    }
    static std::uint64_t cells(const MapCreatorController &c)
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (const auto &cell : c._draftMap.cells)
            for (auto value : {static_cast<int>(cell.kind), static_cast<int>(cell.wayKind), cell.elevation})
            {
                hash ^= static_cast<unsigned>(value);
                hash *= 1099511628211ull;
            }
        return hash;
    }
};
struct ApplicationQaHarness
{
    static void load(Application &app, Campaign campaign)
    {
        const auto id = campaign.maps[0].metadata.id;
        app.editor->setCurrentCampaign(std::move(campaign));
        app.activeWindowController = app.editor;
        app.editor->setWindowOpacity(true);
        if (!app.editor->editWorkspaceMap(id))
            throw std::runtime_error("Map editing unavailable");
    }
    static MapCreatorController &map(Application &app)
    {
        return WorkspaceInteractionProbe::map(*app.editor);
    }
    static void stop(Application &app)
    {
        glfwSetWindowShouldClose(app.mainWindow.get(), GLFW_TRUE);
    }
};
int main(int argc, char **argv)
{
    try
    {
        if (argc != 3 && argc != 4)
            throw std::runtime_error("Usage: map_application_benchmark world|region|tactical|city|ruins|cave|dungeon output-directory [recipe-version]");
        const std::string scope = argv[1];
        const std::filesystem::path output = argv[2];
        if (scope != "world" && scope != "region" && scope != "tactical" && scope != "city" && scope != "ruins" && scope != "cave" && scope != "dungeon")
            throw std::runtime_error("Unknown scope");
        std::filesystem::create_directories(output);
        MapGenerationOptions generation;
        generation.generatorVersion = scope == "world" ? 11 : 12;
        generation.recipeVersion = argc == 4 ? std::stoi(argv[3]) : kCurrentMapIllustrationRecipe;
        if (generation.recipeVersion < 3 || generation.recipeVersion > kCurrentMapIllustrationRecipe)
            throw std::runtime_error("Recipe version must be between 3 and the current illustration recipe");
        generation.styleId = "illustrated_atlas";
        generation.width = generation.height = 80;
        // The ruin composition is a compact encounter; stretching its fixed
        // wall pattern to 80 cells violates its own readiness floor.
        if (scope == "ruins" || scope == "cave" || scope == "dungeon") { generation.width = 32; generation.height = 24; }
        generation.seed = 612345;
        generation.qualityAttempts = 1;
        generation.scale = scope == "world"    ? MapLevel::World
                           : scope == "region" ? MapLevel::Region
                                               : MapLevel::Tactical;
        generation.scope = scope == "world" ? MapScope::World : MapScope::Local;
        generation.creationIntent = scope == "world"    ? MapCreationIntent::WorldOverview
                                    : scope == "region" ? MapCreationIntent::RegionExploration
                                                        : MapCreationIntent::TacticalEncounter;
        generation.contentLanguage = "en";
        Campaign campaign;
        campaign.name = "Generated map responsiveness benchmark";
        campaign.ruleSet = std::make_shared<DND5e>();
        std::cout << "map_dimensions=" << generation.width << 'x' << generation.height << '\n';
        if (scope == "city" || scope == "ruins" || scope == "cave" || scope == "dungeon")
        {
            auto recipe = TacticalMapGenerator::recipeFromGenerationOptions(generation);
            recipe.mapTemplate = scope == "city" ? TacticalMapTemplate::CitySquare :
                                 scope == "cave" ? TacticalMapTemplate::Cave :
                                 scope == "dungeon" ? TacticalMapTemplate::StoneDungeon : TacticalMapTemplate::Ruin;
            const auto candidates = TacticalMapGenerator::generate(recipe, nullptr, 1);
            if (candidates.empty() || candidates.front().quality.criticalIssueCount > 0 ||
                !candidates.front().quality.criticalRouteConnected)
                throw std::runtime_error("Material benchmark did not produce a playable template");
            campaign.maps.push_back(candidates.front().map);
        }
        else
            campaign.maps.push_back(generateSemanticMap(generation).map);
        std::map<std::string, std::vector<double>> timings;
        std::map<std::string, std::vector<double>> cpuTimings;
        MapRenderTimings renderTimings;
        mapRenderTimingSink = &renderTimings;
        std::ofstream samples(output / "samples.csv");
        samples << "phase,frame,complete_frame_ms,terrain_cpu_ms,features_cpu_ms,symbols_cpu_ms\n";
        std::string phase = "initial-compilation";
        const auto began = std::chrono::steady_clock::now();
        auto start = began;
        std::uint64_t base = 0, beforePaint = 0, afterPaint = 0, settled = 0;
        std::uint64_t strokeBefore = 0;
        std::shared_ptr<const MapRenderPlan> finalPlan;
        bool completed = false;
        std::chrono::steady_clock::time_point exportStarted;
        bool exportMeasured = false;
        MapModel exportedMap;
        MapModel paintedMap;
        ApplicationRunOptions options;
        options.width = 1280;
        options.height = 720;
        options.showWindow = false;
        options.maximizeOnStart = false;
        options.maximumFrames = 20000;
        options.initialized = [&](Application &app) {
            auto &save = ProjectSaveService::getInstance();
            save.setBackupSnapshotsEnabled(false);
            save.setRecoverySnapshotsEnabled(false);
            CampaignRepository::saveCampaign(campaign);
            ApplicationQaHarness::load(app, std::move(campaign));
            // Consume each scripted transition in its intended frame. The
            // backend's physical mouse state must not backlog synthetic events
            // into later phases and paint during the undo/redo measurements.
            ImGui::GetIO().ConfigInputTrickleEventQueue = false;
            ImGui::GetPlatformIO().Platform_ShowWindow = [](ImGuiViewport *) {};
            std::cout << "renderer=" << glGetString(GL_RENDERER) << "\nvendor=" << glGetString(GL_VENDOR)
                      << "\ngl_version=" << glGetString(GL_VERSION) << "\nviewport=1280x720\n";
        };
        options.beforeFrame = [&](Application &app, std::uint64_t frame) {
            const auto now = std::chrono::steady_clock::now();
            if (now - began > std::chrono::seconds(120))
                throw std::runtime_error("Benchmark watchdog expired");
            if (frame)
            {
                const double milliseconds = std::chrono::duration<double, std::milli>(now - start).count();
                timings[phase].push_back(milliseconds);
                samples << phase << ',' << frame - 1 << ',' << milliseconds << ',' << renderTimings.terrain << ','
                        << renderTimings.features << ',' << renderTimings.symbols << '\n';
                cpuTimings[phase + "-terrain"].push_back(renderTimings.terrain);
                cpuTimings[phase + "-features"].push_back(renderTimings.features);
                cpuTimings[phase + "-symbols"].push_back(renderTimings.symbols);
            }
            renderTimings = {};
            start = now;
            auto &controller = ApplicationQaHarness::map(app);
            const auto preview = WorkspaceInteractionProbe::preview(controller);
            if (!base && preview.current && frame > 20)
            {
                base = frame + 30;
                std::cout << "initial_compilation_ms=" << preview.compilationMilliseconds << '\n';
            }
            if (!base || frame < base)
            {
                phase = "initial-compilation";
                return;
            }
            const auto step = frame - base;
            if (step == 360)
            {
                if (!ImGui::GetCurrentContext()->OpenPopupStack.empty())
                    throw std::runtime_error("Selection dialog was not dismissed before painting");
                beforePaint = WorkspaceInteractionProbe::cells(controller);
            }
            if (step > 360 && step <= 480 && step % 3 == 0 &&
                strokeBefore == WorkspaceInteractionProbe::cells(controller))
                throw std::runtime_error("A measured stroke did not change semantic cells");
            if (step == 480)
            {
                afterPaint = WorkspaceInteractionProbe::cells(controller);
                paintedMap = WorkspaceInteractionProbe::draft(controller);
                if (beforePaint == afterPaint)
                    throw std::runtime_error("Actual painting did not change semantic cells");
            }
            if (step == 600)
            {
                if (WorkspaceInteractionProbe::cells(controller) != afterPaint)
                {
                    const auto current = WorkspaceInteractionProbe::draft(controller);
                    int differences = 0;
                    for (std::size_t i = 0; i < current.cells.size(); ++i)
                    {
                        const auto &a = paintedMap.cells[i], &b = current.cells[i];
                        if (a.kind != b.kind || a.wayKind != b.wayKind || a.elevation != b.elevation)
                        {
                            if (differences++ < 12)
                                std::cerr << "Cell " << i << " before=" << static_cast<int>(a.kind) << ','
                                          << static_cast<int>(a.wayKind) << ',' << a.elevation
                                          << " after=" << static_cast<int>(b.kind) << ',' << static_cast<int>(b.wayKind)
                                          << ',' << b.elevation << '\n';
                        }
                    }
                    throw std::runtime_error("Undo/redo changed final content: " + std::to_string(differences) +
                                             " cells");
                }
                exportStarted = std::chrono::steady_clock::now();
                WorkspaceInteractionProbe::exportPng(controller, output / "export.png");
                exportedMap = WorkspaceInteractionProbe::draft(controller);
            }
            phase = step < 120    ? "pan"
                    : step < 240  ? "zoom"
                    : step < 360  ? "selection"
                    : step < 480  ? "paint"
                    : step < 600  ? "undo-redo"
                    : step == 600 ? "export-submit"
                                  : "background-completion";
            auto &io = ImGui::GetIO();
            io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
            ImGuiWindow *canvas = nullptr;
            for (auto *window : ImGui::GetCurrentContext()->Windows)
                if (window->Active && std::string(window->Name).find("/MapCreatorCanvas_") != std::string::npos)
                    canvas = window;
            if (!canvas)
                throw std::runtime_error("Actual map canvas unavailable");
            const auto &rect = canvas->InnerClipRect;
            io.AddMousePosEvent((rect.Min.x + rect.Max.x) * .5f + static_cast<float>(step % 20) * 2,
                                (rect.Min.y + rect.Max.y) * .5f);
            if (step == 0)
                io.AddMouseButtonEvent(ImGuiMouseButton_Middle, true);
            if (step == 119)
                io.AddMouseButtonEvent(ImGuiMouseButton_Middle, false);
            if (step >= 120 && step < 240)
                io.AddMouseWheelEvent(0, step % 30 < 15 ? .15f : -.15f);
            if (step >= 240 && step < 360)
            {
                io.AddMouseButtonEvent(0, step % 4 == 0);
                // Terrain selection may open the owning workspace's picker.
                // Dismiss it through its real Escape action before the next click.
                io.AddKeyEvent(ImGuiKey_Escape, step % 4 == 2);
            }
            if (step >= 360 && step < 480 && step % 3 == 0)
            {
                strokeBefore = WorkspaceInteractionProbe::cells(controller);
                WorkspaceInteractionProbe::paint(controller, ((step / 3) % 2 != 0) ^ (scope != "tactical"));
            }
            if (step >= 360 && step < 480)
            {
                io.AddMouseButtonEvent(0, step % 3 != 2);
                if (step % 3 == 2)
                    phase = "stroke-release";
            }
            if (step >= 480 && step < 600)
            {
                io.AddMouseButtonEvent(0, false);
                if (step % 2)
                    WorkspaceInteractionProbe::redo(controller);
                else
                    WorkspaceInteractionProbe::undo(controller);
                const auto actual = WorkspaceInteractionProbe::cells(controller);
                if ((step % 2 && actual != afterPaint) || (!(step % 2) && actual == afterPaint))
                    throw std::runtime_error("Measured undo/redo did not execute a real reversible change");
            }
            if (step > 600 && !exportMeasured && !WorkspaceInteractionProbe::exporting(controller))
            {
                std::cout << "export_completion_ms="
                          << std::chrono::duration<double, std::milli>(now - exportStarted).count() << '\n';
                exportMeasured = true;
            }
            if (step > 600 && preview.current && !WorkspaceInteractionProbe::exporting(controller) &&
                !WorkspaceInteractionProbe::recovering(controller))
            {
                phase = "steady";
                if (!settled)
                    std::cout << "all_background_settled_after_export_ms="
                              << std::chrono::duration<double, std::milli>(now - exportStarted).count() << '\n';
                if (++settled == 60)
                {
                    if (!std::filesystem::exists(output / "export.png"))
                        throw std::runtime_error("Current export was not published");
                    std::cout << "final_compilation_ms=" << preview.compilationMilliseconds
                              << "\ndiscarded_previews=" << preview.discardedResults << '\n';
                    finalPlan = preview.plan;
                    completed = true;
                    ApplicationQaHarness::stop(app);
                }
            }
        };
        options.afterRender = [&](Application &, std::uint64_t frame) {
            glFinish();
            if (frame == 10)
            {
                float xscale = 0, yscale = 0;
                int width = 0, height = 0;
                glfwGetWindowContentScale(glfwGetCurrentContext(), &xscale, &yscale);
                glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
                std::cout << "content_scale=" << xscale << ',' << yscale << "\nframebuffer=" << width << 'x' << height
                          << '\n';
            }
            if (completed)
                captureMapBenchmarkFramebuffer(output / "capture.bmp");
        };
        {
            auto app = std::make_shared<Application>(std::move(options));
            app->run();
        }
        mapRenderTimingSink = nullptr;
        if (!completed)
            throw std::runtime_error("Benchmark did not reach final preview and export");
        if (!finalPlan || !mapRenderPlanContentMatchesMap(*finalPlan, exportedMap))
            throw std::runtime_error("Final preview is stale");
        MapPngExportOptions control;
        control.pixelsPerCell = 16;
        control.applyFog = false;
        control.locale = "en";
        std::string error;
        if (!exportMapPng(exportedMap, output / "export-control.png", control, &error))
            throw std::runtime_error(error);
        auto bytes = [](const std::filesystem::path &path) {
            std::ifstream f(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(f), {});
        };
        if (bytes(output / "export.png") != bytes(output / "export-control.png"))
            throw std::runtime_error("Export differs from final authoritative content");
        bool targets = true;
        for (auto &entry : timings)
        {
            std::sort(entry.second.begin(), entry.second.end());
            const auto count = entry.second.size();
            const double p95 = entry.second[std::min(count - 1, (count * 95 + 99) / 100 - 1)];
            std::cout << entry.first << ",n=" << count << ",p50_ms=" << entry.second[count / 2] << ",p95_ms=" << p95
                      << '\n';
            if (entry.first != "initial-compilation" && p95 > 33)
                targets = false;
        }
        std::cout << "engineering_frame_target=" << (targets ? "passed" : "failed") << '\n';
        for (auto &entry : cpuTimings)
        {
            std::sort(entry.second.begin(), entry.second.end());
            const auto count = entry.second.size();
            std::cout << "cpu-" << entry.first << ",n=" << count
                      << ",p95_ms=" << entry.second[std::min(count - 1, (count * 95 + 99) / 100 - 1)] << '\n';
        }
        return targets ? 0 : 2;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
