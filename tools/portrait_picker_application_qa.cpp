#include "Application.hpp"
#include "Models/Character/PortraitRecipe.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Settings/AccessibilitySettings.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Views/Editor/Widgets/UiInteractionTrace.hpp"
#include "fixtures/DeterministicSessionFixture.hpp"
#include "fixtures/QaFramebufferCapture.hpp"
#include "imgui_internal.h"
#include <iostream>

struct ApplicationQaHarness
{
    static GLFWwindow *window(Application &app) { return app.mainWindow.get(); }
    static EditorWindowController &editor(Application &app) { return *app.editor; }
    static void load(Application &app, Campaign campaign) {
        app.editor->setCurrentCampaign(std::move(campaign));
        app.activeWindowController = app.editor; app.editor->setWindowOpacity(true);
    }
};
struct WorkspaceInteractionProbe
{
    static NPCModel &npc(EditorWindowController &editor) { return editor._npcCreator->getModel(); }
};

int main(int argc, char **argv)
{
    try
    {
        if (argc != 4) throw std::runtime_error("Expected a new output directory, en/de and standard/compact");
        if ((std::string(argv[2]) != "en" && std::string(argv[2]) != "de") ||
            (std::string(argv[3]) != "standard" && std::string(argv[3]) != "compact"))
            throw std::runtime_error("Unsupported locale or display profile");
        const auto output = std::filesystem::absolute(argv[1]);
        if (std::filesystem::exists(output)) throw std::runtime_error("Refusing to overwrite earlier evidence");
        const bool german = std::string(argv[2]) == "de", compact = std::string(argv[3]) == "compact";
        std::filesystem::create_directories(output);
#ifdef _WIN32
        _putenv_s("TTRPG_DATA_ROOT", output.string().c_str());
        _putenv_s("TTRPG_DB_PATH", (output / "TTRPG.db").string().c_str());
        _putenv_s("TTRPG_SETTINGS_PATH", (output / "GMEditor.settings").string().c_str());
#else
        setenv("TTRPG_DATA_ROOT", output.string().c_str(), 1);
        setenv("TTRPG_DB_PATH", (output / "TTRPG.db").string().c_str(), 1);
        setenv("TTRPG_SETTINGS_PATH", (output / "GMEditor.settings").string().c_str(), 1);
#endif
        gm::settings::AccessibilitySettings settings; settings.textScale = compact ? 1.5f : 1.f;
        settings.compactTables = compact;
        std::string error;
        if (!gm::settings::AccessibilitySettingsStore::save(output / "GMEditor.settings", settings, &error))
            throw std::runtime_error(error);
        ApplicationRunOptions options; options.showWindow = false; options.maximizeOnStart = false;
        options.width = compact ? 1280 : 1600; options.height = compact ? 720 : 1000;
        options.maximumFrames = 400;
        std::unique_ptr<UiInteractionTrace> trace;
        int step = 0; bool releaseMouse = false, releaseEscape = false, opened = false, selected = false, passed = false;
        int openFrames = 0, selectedFrames = 0;
        ImVec2 pointer{-100, -100};
        ImGuiID pointerViewport = 0;
        options.initialized = [&](Application &app) {
            const auto &fonts = ImGui::GetIO().Fonts->Fonts;
            std::ofstream fontReport(output / "fonts.txt");
            for (auto *font : fonts) fontReport << font->GetDebugName() << '\n';
            fontReport.flush();
            if (fonts.Size < 3 || std::string(fonts[0]->GetDebugName()).find("Inter-Regular") == std::string::npos ||
                std::string(fonts[1]->GetDebugName()).find("Inter-SemiBold") == std::string::npos ||
                std::string(fonts[2]->GetDebugName()) != "GM Atlas Serif")
                throw std::runtime_error("The installed application did not load its bundled UI and atlas fonts");
            gm::localization::LocalizationService::instance().setPreference(german ? gm::localization::LocalePreference::German : gm::localization::LocalePreference::English, false);
            auto campaign = gm::qa::threeSceneCampaign(german); CampaignRepository::saveCampaign(campaign);
            ApplicationQaHarness::load(app, std::move(campaign));
            auto &editor = ApplicationQaHarness::editor(app); editor.showNPCCreator();
            auto &npc = WorkspaceInteractionProbe::npc(editor);
            npc.name = "Mara Voss"; npc.role = german ? "Hüterin der Brücke" : "Keeper of the crossing";
            npc.description = german ? "Eine aufmerksame Reiseführerin mit einem guten Gedächtnis." : "A watchful guide with a good memory.";
            npc.metadata.customProperties["qa.preserve"] = "unchanged";
            gm::portraits::Recipe recipe; gm::portraits::write(npc.metadata, recipe);
            npc.metadata.customProperties.erase("portrait.collection");
            if(gm::portraits::read(npc.metadata).collection!=0) throw std::runtime_error("Legacy portrait changed collection");
            ImGui::GetIO().ConfigInputTrickleEventQueue = false;
            ImGui::GetPlatformIO().Platform_ShowWindow = [](ImGuiViewport *) {};
            trace = std::make_unique<UiInteractionTrace>(ImGui::GetCurrentContext());
            pointerViewport = ImGui::GetMainViewport()->ID;
        };
        options.beforeFrame = [&](Application &app, std::uint64_t frame) {
            auto &io = ImGui::GetIO(); io.DeltaTime = 1.f / 60.f;
            // Hidden QA windows have no OS hover/focus. Supply the viewport
            // belonging to the observed widget, including detached popups.
            if (!ImGui::FindViewportByID(pointerViewport)) pointerViewport = ImGui::GetMainViewport()->ID;
            io.AddFocusEvent(true); io.AddMouseViewportEvent(pointerViewport); io.AddMousePosEvent(pointer.x, pointer.y);
            if (releaseMouse) { io.AddMouseButtonEvent(0, false); releaseMouse = false; }
            if (releaseEscape) { io.AddKeyEvent(ImGuiKey_Escape, false); releaseEscape = false; }
            const auto click = [&](const char *label) {
                const auto items = trace->find(label);
                if (items.size() != 1) {
                    for (auto *w : ImGui::GetCurrentContext()->Windows)
                        if (w->Active && w->ScrollMax.y > 0 && std::string(w->Name).find("###Workspace/EmbeddedEditorContent_") != std::string::npos) {
                            pointer = w->InnerClipRect.GetCenter(); pointerViewport = w->Viewport->ID;
                            io.AddMouseViewportEvent(pointerViewport); io.AddMousePosEvent(pointer.x, pointer.y); io.AddMouseWheelEvent(0, -2.f); break;
                        }
                    return false;
                }
                const auto &item = items.front(); auto *w = ImGui::FindWindowByName(item.window.c_str());
                const ImVec2 p{(item.minimum.x + item.maximum.x) * .5f, (item.minimum.y + item.maximum.y) * .5f};
                if (!w || !w->InnerClipRect.Contains(p)) {
                    if (w) ImGui::SetScrollY(w, std::clamp(w->Scroll.y + p.y - w->InnerClipRect.GetCenter().y, 0.f, w->ScrollMax.y));
                    return false;
                }
                pointer = p; pointerViewport = w->Viewport->ID; io.AddMouseViewportEvent(pointerViewport);
                io.AddMousePosEvent(p.x, p.y); io.AddMouseButtonEvent(0, true); releaseMouse = true; return true;
            };
            if (frame > 15 && frame % 6 == 0) {
                auto &npc = WorkspaceInteractionProbe::npc(ApplicationQaHarness::editor(app));
                if (step == 0 && click(german ? "Porträt wählen" : "Choose portrait")) ++step;
                else if (step == 1 && click("##PortraitCollection")) ++step;
                else if (step == 2 && click(german ? "Neue Gefährten" : "New company")) ++step;
                else if (step == 3 && click("PortraitIdentity")) ++step;
                else if (step == 4 && opened && click(german ? "Porträt 6" : "Portrait 6")) ++step;
                else if (step == 5 && selected && click("PortraitIdentity")) ++step;
                else if (step == 6) { io.AddKeyEvent(ImGuiKey_Escape, true); releaseEscape = true; ++step; }
                else if (step == 7) {
                    const auto recipe = gm::portraits::read(npc.metadata);
                    if (recipe.collection != 1 || recipe.expression != 5 || recipe.species != 0 || npc.name != "Mara Voss" || npc.metadata.customProperties.at("qa.preserve") != "unchanged")
                        throw std::runtime_error("Portrait selection changed unrelated character data");
                    if (!ImGui::GetCurrentContext()->OpenPopupStack.empty()) throw std::runtime_error("Escape left the portrait popup open");
                    passed = true; glfwSetWindowShouldClose(ApplicationQaHarness::window(app), true);
                }
            }
            trace->beginFrame();
        };
        options.afterRender = [&](Application &app, std::uint64_t frame) {
            if(frame==390) {
                gm::qa::captureFramebuffer(ApplicationQaHarness::window(app),output/"unfinished.bmp");
                std::ofstream diagnostic(output/"unfinished-widgets.txt");
                for(const auto &[id,item]:trace->items()) diagnostic<<id<<' '<<item.label<<' '<<item.window<<' '<<item.minimum.x<<','<<item.minimum.y<<' '<<item.maximum.x<<','<<item.maximum.y<<'\n';
            }
            if (step == 4 && !opened && !trace->find(german ? "Porträt 6" : "Portrait 6").empty()) {
                if (++openFrames >= 3) {
                    if (std::abs(ImGui::GetStyle().FontScaleMain - (compact ? 1.5f : 1.f)) > .01f)
                        throw std::runtime_error("Requested text scale was not applied");
                    std::ofstream diagnostic(output / "popup-diagnostic.txt");
                    for (auto *w : ImGui::GetCurrentContext()->Windows)
                        if (w->Active && (w->Flags & ImGuiWindowFlags_Popup))
                            diagnostic << w->Name << " pos=" << w->Pos.x << ',' << w->Pos.y << " size=" << w->Size.x << ',' << w->Size.y
                                       << " hidden=" << w->Hidden << " viewport=" << w->Viewport->ID << " main=" << ImGui::GetMainViewport()->ID
                                       << " viewport_pos=" << w->Viewport->Pos.x << ',' << w->Viewport->Pos.y << '\n';
                    gm::qa::captureFramebuffer(ApplicationQaHarness::window(app), output / "picker-open.bmp");
                    const auto item = trace->find(german ? "Porträt 6" : "Portrait 6").front();
                    auto *popup = ImGui::FindWindowByName(item.window.c_str());
                    if (popup && popup->Viewport->ID != ImGui::GetMainViewport()->ID) {
                        auto *popupWindow = static_cast<GLFWwindow *>(popup->Viewport->PlatformHandle);
                        if (!popupWindow) throw std::runtime_error("Detached picker has no native window");
                        gm::qa::captureFramebuffer(popupWindow, output / "picker-popup.bmp", true);
                    }
                    opened = true;
                }
            }
            if (step == 5 && !selected && gm::portraits::read(WorkspaceInteractionProbe::npc(ApplicationQaHarness::editor(app)).metadata).expression == 5) {
                if (++selectedFrames >= 3) {
                    gm::qa::captureFramebuffer(ApplicationQaHarness::window(app), output / "picker-selected.bmp"); selected = true;
                }
            }
        };
        {
            auto app = std::make_shared<Application>(std::move(options));
            try { app->run(); } catch (...) { trace.reset(); throw; }
            trace.reset();
        }
        if (!passed || !opened || !selected) throw std::runtime_error("Native portrait workflow did not complete; step " + std::to_string(step));
        std::ofstream report(output / "checks.txt");
        report << "Native NPC editor: preserve legacy collection, select New company, open visual picker, select identity 6, reopen and cancel.\n"
               << "Collection, species, identity, name and unrelated metadata preserved. No save or personal campaign used.\n"
               << "locale=" << argv[2] << " profile=" << argv[3] << '\n';
        std::cout << "Native portrait workflow passed.\n"; return 0;
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
