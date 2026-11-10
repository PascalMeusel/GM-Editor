#include "Application.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Views/Editor/Widgets/UiInteractionTrace.hpp"
#include "imgui_internal.h"
#include "map_benchmark_capture.hpp"
#include <chrono>
#include <iostream>
#include <memory>
struct WorkspaceInteractionProbe
{
    static MapCreatorController &map(EditorWindowController &e)
    {
        return *e._mapCreator;
    }
    static MapCreatorView &view(MapCreatorController &c)
    {
        return *c._view;
    }
    static void box(MapCreatorController &c)
    {
        c._activeTool = MapCreatorController::Tool::InspectSelect;
        c._view->_boxSelectMode = true;
    }
    static ImVec2 at(MapCreatorController &c, float x, float y)
    {
        auto &v = *c._view;
        return {v._interactionCanvasOrigin.x + x * v._interactionCanvasCellSize,
                v._interactionCanvasOrigin.y + y * v._interactionCanvasCellSize};
    }
    static bool ready(MapCreatorController &c)
    {
        return c._view->_canvasPreviewCompiler.state().current;
    }
    static std::size_t count(MapCreatorController &c)
    {
        return gm::mapui::selectedAreaCount(c._view->_areaCandidates);
    }
    static bool riverSelected(MapCreatorController &c)
    {
        for (const auto &i : c._view->_areaCandidates)
            if (i.name == "Test river")
                return i.selected;
        throw std::runtime_error("River missing from sidebar");
    }
    static MapModel draft(MapCreatorController &c)
    {
        return c._draftMap;
    }
    static void undo(MapCreatorController &c)
    {
        c.undoLastAction();
    }
    static void redo(MapCreatorController &c)
    {
        c.redoLastAction();
    }
};
struct ApplicationQaHarness
{
    static MapCreatorController &map(Application &a)
    {
        return WorkspaceInteractionProbe::map(*a.editor);
    }
    static void load(Application &a, Campaign c)
    {
        const auto id = c.maps[0].metadata.id;
        a.editor->setCurrentCampaign(std::move(c));
        a.activeWindowController = a.editor;
        a.editor->setWindowOpacity(true);
        if (!a.editor->editWorkspaceMap(id))
            throw std::runtime_error("Map unavailable");
    }
    static void stop(Application &a)
    {
        glfwSetWindowShouldClose(a.mainWindow.get(), GLFW_TRUE);
    }
};
int main(int argc, char **argv)
{
    try
    {
        if (argc < 2)
            throw std::runtime_error("Expected isolated output directory");
        const std::filesystem::path out = argv[1];
        std::filesystem::create_directories(out);
        MapModel map;
        map.name = "Crossing edit interaction";
        map.width = 24;
        map.height = 20;
        resizeMapCells(map);
        map.atlasLevel = MapLevel::Region;
        map.generation.continuousRendering = true;
        auto &r = map.generation.recipe;
        r.generatorVersion = 12;
        r.recipeVersion = 7;
        r.styleId = "illustrated_atlas";
        r.styleVersion = 6;
        r.styleSnapshotId = "illustrated_atlas_v6";
        for (auto &c : map.cells)
        {
            c.kind = MapTileKind::Dirt;
            c.fogged = false;
        }
        for (int y = 7; y < 10; ++y)
            for (int x = 4; x < 7; ++x)
                mapCellAt(map, x, y)->kind = MapTileKind::Forest;
        MapGeneratedFeature road;
        road.id = "qa-road";
        road.name = "Test road";
        road.kind = MapGeneratedFeatureKind::Road;
        road.width = .8f;
        road.points = {{3, 5}, {18, 5}};
        MapGeneratedFeature river;
        river.id = "qa-river";
        river.name = "Test river";
        river.kind = MapGeneratedFeatureKind::River;
        river.width = 1;
        river.points = {{12, 2}, {13, 8}, {12, 16}};
        map.generation.graph.features = {road, river};
        projectGeneratedWayFeatureToCells(map, river, false);
        projectGeneratedWayFeatureToCells(map, road, false);
        mapGenFillDiagonalRiverCornerGaps(map);
        mapCellAt(map, 2, 18)->kind = MapTileKind::Water;
        Campaign campaign;
        campaign.name = "Map editing UI QA";
        campaign.ruleSet = std::make_shared<DND5e>();
        campaign.maps = {map};
        std::unique_ptr<UiInteractionTrace> trace;
        std::uint64_t base = 0;
        bool complete = false;
        std::size_t initialCount = 0;
        ImVec2 checkbox;
        const auto start = std::chrono::steady_clock::now();
        const auto require = [](bool ok, const char *message) {
            if (!ok)
            {
                std::cerr << message << std::endl;
                throw std::runtime_error(message);
            }
        };
        ApplicationRunOptions options;
        options.width = argc > 3 ? std::stoi(argv[3]) : 1600;
        options.height = argc > 4 ? std::stoi(argv[4]) : 1000;
        options.showWindow = false;
        options.maximizeOnStart = false;
        options.maximumFrames = 10000;
        options.initialized = [&](Application &app) {
            gm::localization::LocalizationService::instance().setPreference(
                argc > 2 && std::string(argv[2]) == "de-DE" ? gm::localization::LocalePreference::German
                                                            : gm::localization::LocalePreference::English,
                false);
            auto &save = ProjectSaveService::getInstance();
            save.setBackupSnapshotsEnabled(false);
            save.setRecoverySnapshotsEnabled(false);
            CampaignRepository::saveCampaign(campaign);
            ApplicationQaHarness::load(app, campaign);
            trace = std::make_unique<UiInteractionTrace>(ImGui::GetCurrentContext());
        };
        options.beforeFrame = [&](Application &app, std::uint64_t frame) {
            require(std::chrono::steady_clock::now() - start < std::chrono::seconds(90), "UI QA timed out");
            auto &c = ApplicationQaHarness::map(app);
            auto &io = ImGui::GetIO();
            if (!base)
            {
                if (frame > 30 && WorkspaceInteractionProbe::ready(c))
                    base = frame;
                else
                    return;
            }
            const auto step = frame - base;
            const auto pos = [&](float x, float y) {
                const auto p = WorkspaceInteractionProbe::at(c, x, y);
                io.AddMousePosEvent(p.x, p.y);
            };
            if (step == 0)
            {
                WorkspaceInteractionProbe::box(c);
                pos(2, 4);
            }
            if (step == 1)
                io.AddMouseButtonEvent(0, true);
            if (step == 3)
                pos(19, 11);
            if (step == 5)
                io.AddMouseButtonEvent(0, false);
            if (step == 8)
            {
                initialCount = WorkspaceInteractionProbe::count(c);
                require(initialCount == 3, "Box did not select road, river and forest together");
                require(!ImGui::GetTopMostPopupModal(), "Box selection opened a modal");
                bool found = false;
                for (const auto &[id, item] : trace->items())
                    if (item.label.find("Test river - ") == 0)
                    {
                        checkbox = {(item.minimum.x + item.maximum.x) * .5f, (item.minimum.y + item.maximum.y) * .5f};
                        found = true;
                    }
                require(found, "River checkbox not rendered in sidebar");
                io.AddMousePosEvent(checkbox.x, checkbox.y);
            }
            if (step == 9)
                io.AddMouseButtonEvent(0, true);
            if (step == 10)
                io.AddMouseButtonEvent(0, false);
            if (step == 13)
            {
                require(!WorkspaceInteractionProbe::riverSelected(c), "Sidebar click did not deselect river");
                require(WorkspaceInteractionProbe::count(c) == initialCount - 1,
                        "Sidebar click discarded other selections");
                pos(7, 5);
            }
            if (step == 14)
                io.AddMouseButtonEvent(0, true);
            if (step == 16)
                pos(7, 7);
            if (step == 18)
                io.AddMouseButtonEvent(0, false);
            if (step == 22)
            {
                const auto draft = WorkspaceInteractionProbe::draft(c);
                require(draft.generation.graph.features[0].points[0].y == 7, "Group drag failed to move road");
                require(draft.generation.graph.features[1].points[0].y == 2, "Deselected river moved");
                require(mapCellAt(draft, 4, 11)->kind == MapTileKind::Forest &&
                            mapCellAt(draft, 4, 7)->kind != MapTileKind::Forest,
                        "Group drag failed to move forest");
                WorkspaceInteractionProbe::undo(c);
            }
            if (step == 25)
            {
                const auto draft = WorkspaceInteractionProbe::draft(c);
                require(draft.generation.graph.features[0].points[0].y == 5 &&
                            mapCellAt(draft, 4, 7)->kind == MapTileKind::Forest,
                        "Single undo did not restore whole selection");
                WorkspaceInteractionProbe::redo(c);
            }
            if (step == 30)
                pos(13, 8);
            if (step == 31)
                io.AddMouseButtonEvent(0, true);
            if (step == 33)
                pos(16, 8);
            if (step == 35)
                io.AddMouseButtonEvent(0, false);
            if (step == 39)
            {
                const auto draft = WorkspaceInteractionProbe::draft(c);
                require(std::abs(draft.generation.graph.features[1].points.front().x - 15.f) < .05f,
                        "Actual river drag did not move by three cells");
                for (int y = 3; y < 16; ++y)
                    for (int x = 11; x <= 13; ++x)
                        require(mapCellAt(draft, x, y)->kind != MapTileKind::Water &&
                                    mapCellAt(draft, x, y)->kind != MapTileKind::Bridge,
                                "Actual river drag left old water/bridge cells");
                require(mapCellAt(draft, 2, 18)->kind == MapTileKind::Water, "River drag erased genuine pond");
                WorkspaceInteractionProbe::undo(c);
            }
            if (step == 42)
            {
                require(WorkspaceInteractionProbe::draft(c).generation.graph.features[1].points.front().x == 12,
                        "River undo failed");
                WorkspaceInteractionProbe::redo(c);
            }
            if (step > 45 && WorkspaceInteractionProbe::ready(c))
            {
                complete = true;
                ApplicationQaHarness::stop(app);
            }
            trace->beginFrame();
        };
        options.afterRender = [&](Application &, std::uint64_t frame) {
            if (!base)
                return;
            const auto step = frame - base;
            if (step == 7 || step == 12 || step == 17 || step == 19 || step == 34 || step == 36 || complete)
                captureMapBenchmarkFramebuffer(out /
                                               (complete ? "final.bmp" : "step-" + std::to_string(step) + ".bmp"));
            if (complete)
                trace.reset();
        };
        {
            auto app = std::make_shared<Application>(options);
            try
            {
                app->run();
            }
            catch (...)
            {
                if (ImGui::GetCurrentContext())
                    trace.reset();
                else
                    trace.release();
                throw;
            }
            if (ImGui::GetCurrentContext())
                trace.reset();
            else
                trace.release();
        }
        require(complete, "UI run did not finish");
        std::ofstream report(out / "result.json");
        report << "{\"passed\":true,\"actual_mouse_input\":true,\"modal_opened\":false,\"box_members\":3,\"sidebar_"
                  "deselection\":true,\"atomic_move_undo_redo\":true,\"river_drag_no_water_ghosts\":true}\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
