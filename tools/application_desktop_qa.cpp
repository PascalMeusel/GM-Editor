#include "Application.hpp"
#include "Models/Settings/AccessibilitySettings.hpp"
#include "fixtures/DeterministicSessionFixture.hpp"
#include "fixtures/ArtShowcaseFixture.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/UX/VisualScenarioCatalog.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include "Models/Localization/LocalizationService.hpp"
#include "Views/Editor/Widgets/UiInteractionTrace.hpp"
#include "imgui_internal.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

struct ApplicationQaHarness
{
    static void load(Application &app, Campaign campaign)
    {
        app.editor->setCurrentCampaign(std::move(campaign));
        app.activeWindowController = app.editor;
        app.editor->setWindowOpacity(true);
    }
    static GLFWwindow *window(Application &app)
    {
        return app.mainWindow.get();
    }
    static bool live(Application &app)
    {
        return app.editor->activeSession() != nullptr;
    }
    static const LocalPlayerDisplayModel &display(Application &app)
    {
        return app.editor->currentCampaign().localPlayerDisplay;
    }
    static EditorWindowController &editor(Application &app)
    {
        return *app.editor;
    }
    static std::string persistedBlade(Application &app)
    {
        for (const auto &campaign : CampaignRepository::loadAllCampaigns())
            if (campaign.metadata.id == app.editor->currentCampaign().metadata.id)
                for (const auto &item : campaign.customItems)
                    if (item.metadata.id == "qa-session-v1/blade")
                        return item.name;
        throw std::runtime_error("Generated blade missing from SQLite");
    }
    static void changePublicMap(Application &app)
    {
        const auto result = app.editor->applyWorldInspectorMutation(
            [](Campaign &campaign) {
                for (auto &map : campaign.maps)
                    if (map.metadata.id == campaign.localPlayerDisplay.publicMapId)
                    {
                        map.name += " revised";
                        ++map.revision;
                        return WorldInspector::MutationResult{
                            WorldInspector::MutationStatus::Applied, {}, map.metadata.id};
                    }
                return WorldInspector::MutationResult{};
            },
            "QA post-publication content change");
        if (!result.applied())
            throw std::runtime_error("Public-map mutation failed");
    }
};

namespace
{
void write32(std::ostream &out, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        out.put(static_cast<char>(value >> shift));
}
void write16(std::ostream &out, std::uint16_t value)
{
    out.put(static_cast<char>(value));
    out.put(static_cast<char>(value >> 8));
}
void capture(Application &app, const std::filesystem::path &output, GLFWwindow *target = nullptr)
{
    std::filesystem::create_directories(output);
    auto *window = target ? target : ApplicationQaHarness::window(app);
    struct RestoreContext
    {
        GLFWwindow *previous = glfwGetCurrentContext();
        ~RestoreContext()
        {
            glfwMakeContextCurrent(previous);
        }
    } restoreContext;
    glfwMakeContextCurrent(window);
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0)
        throw std::runtime_error("No framebuffer");
    const std::size_t stride = (static_cast<std::size_t>(width) * 3 + 3) & ~std::size_t(3);
    std::vector<unsigned char> pixels(stride * height);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(target ? GL_FRONT : GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("Framebuffer read failed");
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            std::swap(pixels[y * stride + x * 3], pixels[y * stride + x * 3 + 2]);
    std::ofstream bitmap(output / "capture.bmp", std::ios::binary);
    bitmap.write("BM", 2);
    write32(bitmap, static_cast<std::uint32_t>(54 + pixels.size()));
    write32(bitmap, 0);
    write32(bitmap, 54);
    write32(bitmap, 40);
    write32(bitmap, width);
    write32(bitmap, height);
    write16(bitmap, 1);
    write16(bitmap, 24);
    write32(bitmap, 0);
    write32(bitmap, static_cast<std::uint32_t>(pixels.size()));
    write32(bitmap, 2835);
    write32(bitmap, 2835);
    write32(bitmap, 0);
    write32(bitmap, 0);
    bitmap.write(reinterpret_cast<const char *>(pixels.data()), pixels.size());
    if (!bitmap)
        throw std::runtime_error("Framebuffer write failed");
    std::ofstream report(output / "capture.txt");
    float xscale = 0, yscale = 0;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    report << "text_scale=" << ImGui::GetStyle().FontScaleMain << "\n";
    report << "framebuffer=" << width << 'x' << height << "\ncontent_scale=" << xscale << ',' << yscale
           << "\nvendor=" << reinterpret_cast<const char *>(glGetString(GL_VENDOR))
           << "\nrenderer=" << reinterpret_cast<const char *>(glGetString(GL_RENDERER))
           << "\nversion=" << reinterpret_cast<const char *>(glGetString(GL_VERSION)) << '\n';
}
}

int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--list-cases")
        {
            std::cout << "id,scenario,profile,width,height,ui_scale,locale\n";
            for (const auto &item : gm::ux::visualCaptureMatrix())
                std::cout << item.id << ',' << item.scenario.id << ',' << item.scenario.seedProfile << ','
                          << item.viewport.width << ',' << item.viewport.height << ',' << item.viewport.uiScale << ','
                          << gm::ux::visualLocaleCode(item.locale) << '\n';
            return 0;
        }
        if (argc < 2 || argc > 5)
            throw std::runtime_error("Expected isolated output directory and optional de-DE locale");
        if (argc >= 3 && std::string(argv[2]) != "en-US" && std::string(argv[2]) != "de-DE")
            throw std::runtime_error("Locale must be en-US or de-DE");
        const bool german = argc >= 3 && std::string(argv[2]) == "de-DE";
        const bool compact = argc >= 4 && std::string(argv[3]) == "--compact";
        const std::string profile=argc>=4?argv[3]:"";
        const bool recordFrames=argc==5 && std::string(argv[4])=="--record-frames";
        const bool artShowcase=profile=="--art-world" || profile=="--art-region" || profile=="--art-tactical" ||
            profile=="--art-forest" || profile=="--art-river" || profile=="--art-ruins" || profile=="--art-city" ||
            profile=="--art-cave" || profile=="--art-dungeon";
        const std::string artKind=artShowcase?profile.substr(6):"";
        if (argc >= 4 && !compact && !artShowcase) throw std::runtime_error("Unknown desktop profile");
        if(argc==5 && (!recordFrames || !artShowcase)) throw std::runtime_error("Frame recording requires an art showcase profile");
        bool artShowcaseCompleted=false;
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output);
        gm::settings::AccessibilitySettings textSettings;
        textSettings.textScale = compact ? 1.5f : 1.0f;
        std::string settingsError;
        if (!gm::settings::AccessibilitySettingsStore::save(ApplicationPaths::settingsPath(), textSettings, &settingsError)) throw std::runtime_error(settingsError);
        std::unique_ptr<UiInteractionTrace> trace;
        ImVec2 runButton;
        ImVec2 scriptedPointer;
        bool hasScriptedPointer = false;
        int itemStep = 0;
        int advancementChoiceStep = 0;
        int advancementReviewRetries = 0;
        int typingSettleFrames = 0;
        int savedTextSettleFrames = 0;
        int failedSaveSettleFrames = 0, retrySettleFrames = 0;
        int scrollSearchItemStep = -1;
        float scrollSearchDirection = 0.0f;
        std::unique_ptr<SQLite::Database> control;
        ApplicationRunOptions options;
        options.width = 1280;
        options.height = 720;
        options.showWindow = false;
        options.maximizeOnStart = false;
        options.maximumFrames = artShowcase?140:720;
        options.initialized = [&](Application &app) {
            glfwSetWindowPos(ApplicationQaHarness::window(app), 32, 32);

            gm::localization::LocalizationService::instance().setPreference(
                german ? gm::localization::LocalePreference::German : gm::localization::LocalePreference::English,
                false);
            auto &save = ProjectSaveService::getInstance();
            save.setBackupSnapshotsEnabled(false);
            save.setRecoverySnapshotsEnabled(false);
            auto campaign = artShowcase?gm::qa::artShowcaseCampaign(artKind,german):gm::qa::threeSceneCampaign(german);
            CampaignRepository::saveCampaign(campaign);
            ApplicationQaHarness::load(app, std::move(campaign));
            if(artShowcase) {
                ApplicationQaHarness::editor(app).setLocalPlayerDisplayMapOnly(true);
                std::ofstream proof(output/"showcase.json");
                proof<<"{\"kind\":\""<<artKind<<"\",\"recipe\":"<<kCurrentMapIllustrationRecipe<<",\"synthetic_fixture\":true,"
                     <<"\"backend\":\"Actual application OpenGL framebuffer\",\"physical_monitor_acceptance\":false,"
                     <<"\"campaign_launch_authorized\":false}";
            }
            trace = std::make_unique<UiInteractionTrace>(ImGui::GetCurrentContext());
            // Keep synthetic transitions in their intended frame. Hidden-window
            // backend mouse reports must not queue scripted clicks into later
            // actions; the production input policy is unchanged.
            ImGui::GetIO().ConfigInputTrickleEventQueue = false;
            // Keep QA platform windows hidden too; their actual OpenGL renderer
            // still draws and swaps them. No physical-monitor acceptance implied.
            ImGui::GetPlatformIO().Platform_ShowWindow = [](ImGuiViewport *) {};
        };
        options.beforeFrame = [&](Application &app, std::uint64_t frame) {
            if (frame > 1 && std::abs(ImGui::GetStyle().FontScaleMain - textSettings.textScale) >= .001f)
                throw std::runtime_error("Production text scale did not match requested profile");
            // Capture runs use a fixed UI clock for tooltips and notifications.
            // This harness does not measure real-time interaction performance.
            ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
            // Hidden QA windows have no OS hovered-window report. Bind the
            // injected pointer to its actual ImGui viewport after backend input.
            // A hidden test window cannot own OS focus. Pair the synthetic
            // pointer with synthetic focus after backend events so another app
            // cannot clear the scripted press/release sequence mid-click.
            // This affects the harness only, not the production focus policy.
            ImGui::GetIO().AddFocusEvent(true);
            ImGui::GetIO().AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
            if (hasScriptedPointer)
                ImGui::GetIO().AddMousePosEvent(scriptedPointer.x, scriptedPointer.y);
            const auto pointAt = [&](const char *label) {
                auto buttons = trace->find(label);
                if (auto *modal = ImGui::GetTopMostPopupModal())
                    buttons.erase(std::remove_if(buttons.begin(), buttons.end(),
                                                 [&](const auto &item) {
                                                       auto *window = ImGui::FindWindowByName(item.window.c_str());
                                                       while (window && window != modal) window = window->ParentWindow;
                                                       return window != modal;
                                                   }),
                                  buttons.end());
                if (buttons.size() != 1)
                {
                    for (const auto &entry : trace->items())
                        if (!entry.second.label.empty())
                            std::cerr << entry.second.window << ": " << entry.second.label << '\n';
                    throw std::runtime_error(std::string("Widget unavailable or ambiguous: ") + label);
                }
                scriptedPointer = ImVec2((buttons[0].minimum.x + buttons[0].maximum.x) * .5f,
                                         (buttons[0].minimum.y + buttons[0].maximum.y) * .5f);
                hasScriptedPointer = true;
                ImGui::GetIO().AddMousePosEvent(scriptedPointer.x, scriptedPointer.y);
                std::cout << "Point " << frame << ' ' << label << " window=" << buttons[0].window
                          << " rect=" << buttons[0].minimum.x << ',' << buttons[0].minimum.y << ':'
                          << buttons[0].maximum.x << ',' << buttons[0].maximum.y << '\n';
            };
            if (frame == 9)
            {
                const auto buttons = trace->find(german ? "Sitzung starten" : "Run session");
                if (buttons.size() != 1)
                    throw std::runtime_error("Run session widget unavailable or ambiguous");
                runButton = ImVec2((buttons[0].minimum.x + buttons[0].maximum.x) * .5f,
                                   (buttons[0].minimum.y + buttons[0].maximum.y) * .5f);
                scriptedPointer = runButton;
                hasScriptedPointer = true;
                ImGui::GetIO().AddMousePosEvent(runButton.x, runButton.y);
            }
            if (frame == 10)
                ImGui::GetIO().AddMouseButtonEvent(0, true);
            if (frame == 11)
                ImGui::GetIO().AddMouseButtonEvent(0, false);
            if (frame == 20 && !ApplicationQaHarness::live(app))
                throw std::runtime_error("Actual Run session click failed");
            if (frame == 25)
                pointAt(german ? "Spieleranzeige prüfen" : "Review player display");
            if (frame == 26 || frame == 36)
                ImGui::GetIO().AddMouseButtonEvent(0, true);
            if (frame == 27 || frame == 37)
                ImGui::GetIO().AddMouseButtonEvent(0, false);
            if (frame == 35)
                pointAt(german ? "Spielerfenster öffnen…" : "Open player window…");
            if (frame == 45 && !ImGui::GetTopMostPopupModal())
                throw std::runtime_error("Player review modal did not open from UI input");
            if (frame == 50)
                pointAt(german ? "Spielerfenster öffnen" : "Open player window");
            if (frame == 51)
                ImGui::GetIO().AddMouseButtonEvent(0, true);
            if (frame == 52)
                ImGui::GetIO().AddMouseButtonEvent(0, false);
            if (frame == 60)
            {
                if(artShowcase)
                    for(auto *window:ImGui::GetCurrentContext()->Windows)
                        if(std::string(window->Name).find("###LocalPlayerDisplay")!=std::string::npos)
                            ImGui::SetWindowSize(window,ImVec2(1280,960),ImGuiCond_Always);
                const auto &display = ApplicationQaHarness::display(app);
                std::cout << "Arm state enabled=" << display.enabled << " blackout=" << display.blackout
                          << " published=" << display.published << '\n';
            }
            if (frame == 70 || frame == 80)
                pointAt(gm::localization::trUi("Show to players"));
            if (frame == 71 || frame == 81)
                ImGui::GetIO().AddMouseButtonEvent(0, true);
            if (frame == 72 || frame == 82)
                ImGui::GetIO().AddMouseButtonEvent(0, false);
            if (frame == 90 && !ApplicationQaHarness::display(app).published)
                throw std::runtime_error("Reviewed publication click failed");
            if (frame == 105)
                ApplicationQaHarness::changePublicMap(app);
            if (frame == 115 &&
                (ApplicationQaHarness::display(app).published || !ApplicationQaHarness::display(app).blackout))
                throw std::runtime_error("Content mutation did not revoke player publication");
            // Space actions across rendered frames so requested navigation and
            // scrolling can settle before the next scripted transition.
            if (frame >= 130 && (frame - 130) % 4 == 0)
            {
                auto &editor = ApplicationQaHarness::editor(app);
                auto &io = ImGui::GetIO();
                const auto scrollWindow = [&](ImGuiWindow *window, float direction) {
                    scriptedPointer = window->InnerClipRect.GetCenter();
                    hasScriptedPointer = true;
                    io.AddMousePosEvent(scriptedPointer.x, scriptedPointer.y);
                    io.AddMouseWheelEvent(0, direction * 3.f);
                };
                const auto pointVisible = [&](const char *label, int searchDirection = 0) {
                    auto items = trace->find(label);
                    if (auto *modal = ImGui::GetTopMostPopupModal())
                        items.erase(std::remove_if(items.begin(), items.end(),
                                                   [&](const auto &item) {
                                                       auto *window = ImGui::FindWindowByName(item.window.c_str());
                                                       while (window && window != modal) window = window->ParentWindow;
                                                       return window != modal;
                                                   }),
                                    items.end());
                    // ImGui applies a requested tab selection on the following
                    // frame. Wait within the bounded script budget for its form.
                    if (items.empty())
                    {
                        if (searchDirection)
                        {
                            for (auto *window : ImGui::GetCurrentContext()->Windows)
                            {
                                const std::string name(window->Name);
                                const bool itemForm =
                                    name.find("###Workspace/EmbeddedEditorContent_") != std::string::npos;
                                const bool combatForm =
                                    name.find("BattleTrackerCompactControls") != std::string::npos ||
                                    name.find("CombatControlsScroll") != std::string::npos;
                                if (window->Active && window->ScrollMax.y > 0 &&
                                    (itemStep >= 30 ? itemForm : (itemStep < 23 ? itemForm : combatForm)))
                                {
                                    if (scrollSearchItemStep != itemStep)
                                    {
                                        scrollSearchItemStep = itemStep;
                                        scrollSearchDirection = static_cast<float>(searchDirection);
                                    }
                                    if (scrollSearchDirection < 0.0f && window->Scroll.y >= window->ScrollMax.y - 1.0f)
                                        scrollSearchDirection = 1.0f;
                                    else if (scrollSearchDirection > 0.0f && window->Scroll.y <= 1.0f)
                                        scrollSearchDirection = -1.0f;
                                    scrollWindow(window, scrollSearchDirection);
                                    break;
                                }
                            }
                        }
                        return false;
                    }
                    if (items.size() != 1)
                    {
                        pointAt(label);
                        return false;
                    }
                    auto *window = ImGui::FindWindowByName(items[0].window.c_str());
                    if (!window)
                        throw std::runtime_error("Input target window missing");
                    const ImVec2 center((items[0].minimum.x + items[0].maximum.x) * .5f,
                                        (items[0].minimum.y + items[0].maximum.y) * .5f);
                    if (!window->InnerClipRect.Contains(center))
                    {
                        if (window->ScrollMax.y > 0.0f)
                        {
                            const float targetScroll = std::clamp(
                                window->Scroll.y + center.y - window->InnerClipRect.GetCenter().y,
                                0.0f,
                                window->ScrollMax.y);
                            ImGui::SetScrollY(window, targetScroll);
                        }
                        else
                            scrollWindow(window, center.y > window->InnerClipRect.Max.y ? -1.f : 1.f);
                        return false;
                    }
                    pointAt(label);
                    return true;
                };
                bool advance = true;
                switch (itemStep)
                {
                case 0:
                    if (!editor.openWorkspaceEntity("Item", "qa-session-v1/blade"))
                        throw std::runtime_error("Owning item workspace did not open");
                    break;
                case 1:
                    advance = pointVisible("##Name", -1);
                    break;
                case 2:
                case 10:
                case 15:
                case 20:
                case 26:
                case 33: case 36: case 39: case 42: case 45: case 49: case 52:
                    io.AddMouseButtonEvent(0, true);
                    break;
                case 3:
                case 11:
                case 16:
                case 21:
                case 27:
                case 34: case 37: case 40: case 43: case 46: case 50: case 53:
                    io.AddMouseButtonEvent(0, false);
                    break;
                case 4:
                    io.AddKeyEvent(ImGuiMod_Ctrl, true);
                    io.AddKeyEvent(ImGuiKey_A, true);
                    break;
                case 5:
                    io.AddKeyEvent(ImGuiKey_A, false);
                    io.AddKeyEvent(ImGuiMod_Ctrl, false);
                    break;
                case 6:
                    io.AddInputCharactersUTF8("Retained through actual application input");
                    break;
                case 7:
                    if (typingSettleFrames++ < 2)
                    {
                        advance = false;
                        break;
                    }
                    if (!editor.hasUnsavedChanges())
                        throw std::runtime_error("Application text input did not create a dirty draft");
                    break;
                case 8:
                    editor.showSpellLibrary();
                    break;
                case 9:
                    advance = pointVisible(german ? "Zurück zur vorherigen Aufgabe: Kompendium" : "Return to previous: Compendium");
                    break;
                case 12:
                    if (editor.navigationContext().current().editorRoute != "show:item_creator" ||
                        !editor.hasUnsavedChanges())
                        throw std::runtime_error("Library return lost the application draft or route");
                    break;
                case 13:
                    control = std::make_unique<SQLite::Database>(ApplicationPaths::databasePath().string(),
                                                                 SQLite::OPEN_READWRITE);
                    control->exec(
                        "CREATE TRIGGER reject_app_item BEFORE INSERT ON custom_items BEGIN SELECT RAISE(ABORT, 'injected application save failure'); END;");
                    break;
                case 14:
                    advance = pointVisible(gm::localization::trUi("Save Item"), -1);
                    break;
                case 17:
                    // Keep the real SQLite fault armed until the queued click
                    // has reached the save service, rather than accepting an
                    // unchanged draft before a save was actually attempted.
                    if (ProjectSaveService::getInstance().state().lastError.empty() && failedSaveSettleFrames++ < 4)
                    {
                        advance = false;
                        break;
                    }
                    if (ProjectSaveService::getInstance().state().lastError.find("injected application save failure") ==
                        std::string::npos)
                        throw std::runtime_error("Application click did not reach the injected SQLite failure");
                    if (!editor.hasUnsavedChanges() ||
                        ApplicationQaHarness::persistedBlade(app) != (german ? "Wachschwert" : "Watch sword"))
                        throw std::runtime_error("Failed application save changed disk or cleared the draft");
                    break;
                case 18:
                    control->exec("DROP TRIGGER reject_app_item;");
                    break;
                case 19:
                    advance = pointVisible(german ? "Speicherung wiederholen" : "Retry saving", 1);
                    break;
                case 22:
                    if (editor.hasUnsavedChanges() && retrySettleFrames++ < 4)
                    {
                        advance = false;
                        break;
                    }
                    if (editor.hasUnsavedChanges() ||
                        ApplicationQaHarness::persistedBlade(app) != "Retained through actual application input")
                        throw std::runtime_error("Application Retry did not commit the retained draft");
                    break;
                case 23:
                    if (!editor.stageEncounterForLive("qa-session-v1/encounter"))
                        throw std::runtime_error("Prepared encounter could not return to Live");
                    editor.setLiveSessionPhase(LiveSessionDashboardPhase::Combat);
                    break;
                case 24:
                    break; // Allow the Live tab and nested combat view to settle.
                case 25:
                    advance = pointVisible(gm::localization::trUi("Begin Combat"), -1);
                    break;
                case 28:
                    break; // Drain the ordinary ImGui button event queue.
                case 29:
                    if (editor.currentCampaign().encounters[0].combatPhase != CombatPhase::ActiveTurn)
                        throw std::runtime_error("Actual Begin Combat click failed");
                    break;
                case 30:
                    if (!editor.openWorkspaceEntity("character", editor.currentCampaign().characters.front().metadata.id))
                        throw std::runtime_error("Character workspace did not open");
                    break;
                case 31: break;
                case 32: advance = pointVisible(gm::localization::trUi("Profile"), -1); break;
                case 35: advance = pointVisible(gm::localization::trUi("Level Up"), -1); break;
                case 38: case 44:
                    advance = pointVisible(gm::localization::trUi("Next")); break;
                case 41:
                    advance = false;
                    switch (advancementChoiceStep) {
                    case 0: if (pointVisible(gm::localization::trUi("New feats"))) ++advancementChoiceStep; break;
                    case 1: io.AddMouseButtonEvent(0, true); ++advancementChoiceStep; break;
                    case 2: io.AddMouseButtonEvent(0, false); ++advancementChoiceStep; break;
                    case 3: io.AddKeyEvent(ImGuiMod_Ctrl, true); io.AddKeyEvent(ImGuiKey_A, true); ++advancementChoiceStep; break;
                    case 4: io.AddKeyEvent(ImGuiKey_A, false); io.AddKeyEvent(ImGuiMod_Ctrl, false); ++advancementChoiceStep; break;
                    case 5: io.AddInputCharactersUTF8("Generated feat"); ++advancementChoiceStep; break;
                    default: advance = pointVisible(gm::localization::trUi("Next")); break;
                    }
                    break;
                case 47:
                    if (!ImGui::GetTopMostPopupModal()) throw std::runtime_error("Advancement review disappeared");
                    if (trace->find(gm::localization::trUi("I reviewed these changes and feature prerequisites")).empty()) {
                        // Verify the visible transition, not a frame count. A native
                        // focus change can consume a click while the modal remains open.
                        if (++advancementReviewRetries > 2) throw std::runtime_error("Next did not open advancement review");
                        std::cout << "Retry native review transition " << advancementReviewRetries << '\n';
                        itemStep = 44; advance = false;
                    }
                    break;
                case 48: advance = pointVisible(gm::localization::trUi("I reviewed these changes and feature prerequisites")); break;
                case 51: advance = pointVisible(gm::localization::trUi("Apply Level Up")); break;
                case 54: break;
                case 55: {
                    const auto &advanced = editor.currentCampaign().characters.front();
                    if (ImGui::GetTopMostPopupModal() || advanced.level != 4 || advanced.maxHitPoints != 35 ||
                        std::find(advanced.feats.begin(), advanced.feats.end(), "Generated feat") == advanced.feats.end())
                        throw std::runtime_error("Native advancement input did not commit the reviewed choices");
                    bool persisted = false;
                    for (const auto &saved : CampaignRepository::loadAllCampaigns())
                        if (saved.metadata.id == editor.currentCampaign().metadata.id)
                            for (const auto &character : saved.characters)
                                if (character.metadata.id == advanced.metadata.id && character.level == 4 && character.classLevels.at("Fighter") == 4)
                                    persisted = true;
                    if (!persisted) throw std::runtime_error("Native advancement was not persisted");
                    break;
                }
                case 56:
                    if (!editor.openWorkspaceEntity("Item", "qa-session-v1/blade"))
                        throw std::runtime_error("Saved-input regression could not reopen the item editor");
                    break;
                case 57:
                    advance = pointVisible("##Name", -1);
                    break;
                case 58:
                    io.AddMouseButtonEvent(0, true);
                    break;
                case 59:
                    io.AddMouseButtonEvent(0, false);
                    break;
                case 60:
                    io.AddKeyEvent(ImGuiMod_Ctrl, true);
                    io.AddKeyEvent(ImGuiKey_A, true);
                    break;
                case 61:
                    io.AddKeyEvent(ImGuiKey_A, false);
                    io.AddKeyEvent(ImGuiMod_Ctrl, false);
                    break;
                case 62:
                    io.AddInputCharactersUTF8("Saved through Escape and navigation");
                    break;
                case 63:
                    if (savedTextSettleFrames++ < 2)
                    {
                        advance = false;
                        break;
                    }
                    if (!editor.hasUnsavedChanges())
                        throw std::runtime_error("Saved-input regression text did not make the campaign dirty");
                    break;
                case 64:
                    io.AddKeyEvent(ImGuiMod_Ctrl, true);
                    io.AddKeyEvent(ImGuiKey_S, true);
                    break;
                case 65:
                    io.AddKeyEvent(ImGuiKey_S, false);
                    io.AddKeyEvent(ImGuiMod_Ctrl, false);
                    break;
                case 66:
                    if (editor.hasUnsavedChanges() ||
                        ApplicationQaHarness::persistedBlade(app) != "Saved through Escape and navigation")
                        throw std::runtime_error("Ctrl+S did not persist the focused application input");
                    io.AddKeyEvent(ImGuiKey_Escape, true);
                    break;
                case 67:
                    io.AddKeyEvent(ImGuiKey_Escape, false);
                    break;
                case 68:
                    editor.showSpellLibrary();
                    break;
                case 69:
                    break; // Let the source editor unmount before reopening it.
                case 70:
                    if (!editor.openWorkspaceEntity("Item", "qa-session-v1/blade"))
                        throw std::runtime_error("Saved-input regression could not navigate back to the item editor");
                    break;
                case 71:
                    break; // Let the reopened editor render from its current model.
                case 72:
                    if (editor.hasUnsavedChanges() ||
                        ApplicationQaHarness::persistedBlade(app) != "Saved through Escape and navigation")
                        throw std::runtime_error("Escape after successful Ctrl+S reverted text during navigation");
                    break;
                default:
                    break;
                }
                if (advance && itemStep < 73)
                    ++itemStep;
            }
            trace->beginFrame();
        };
        options.afterRender = [&](Application &app, std::uint64_t frame) {
            if(recordFrames && frame>=8 && frame<=125) {
                const auto number=std::to_string(1000000+frame-8).substr(1);
                capture(app,output/"recording/main"/number);
                if(frame>=65) {
                    for(auto *window:ImGui::GetCurrentContext()->Windows)
                        if(std::string(window->Name).find("###LocalPlayerDisplay")!=std::string::npos &&
                           window->Viewport && window->Viewport!=ImGui::GetMainViewport() && window->Viewport->PlatformHandle) {
                            const auto playerNumber=std::to_string(1000000+frame-65).substr(1);
                            capture(app,output/"recording/player"/playerNumber,static_cast<GLFWwindow *>(window->Viewport->PlatformHandle));
                            break;
                        }
                }
            }
            if (frame == 719)
            {
                std::cerr << "Unfinished item step: " << itemStep << '\n';
                capture(app, output / "unfinished-input");
            }
            if (frame >= 130 && itemStep >= 2 && itemStep <= 7)
            {
                const auto &context = *ImGui::GetCurrentContext();
                std::cout << "Item input step=" << itemStep << " pos=" << ImGui::GetIO().MousePos.x << ','
                          << ImGui::GetIO().MousePos.y
                          << " hovered=" << (context.HoveredWindow ? context.HoveredWindow->Name : "none")
                          << " active=" << context.ActiveId << '\n';
                if (itemStep == 4 || itemStep == 7)
                    capture(app, output / (itemStep == 4 ? "item-focus" : "item-typed"));
            }
            if (frame >= 50 && frame <= 53)
            {
                const auto &context = *ImGui::GetCurrentContext();
                std::cout << "Input " << frame << " pos=" << ImGui::GetIO().MousePos.x << ','
                          << ImGui::GetIO().MousePos.y << " down=" << ImGui::GetIO().MouseDown[0]
                          << " hovered=" << (context.HoveredWindow ? context.HoveredWindow->Name : "none")
                          << " hoveredID=" << context.HoveredId << " activeID=" << context.ActiveId << '\n';
            }
            if (frame == 8)
                capture(app, output / "prepared");
            if (frame == 23)
                capture(app, output / "live-after-click");
            if (frame == 47)
                capture(app, output / "player-review");
            if (frame == 65 || frame == 100 || frame == 125)
            {
                if (frame == 65)
                    capture(app, output / "after-arm-main");
                const auto &display = ApplicationQaHarness::display(app);
                if (frame == 65 && (!display.enabled || !display.blackout || display.published))
                    throw std::runtime_error("Arming did not retain blackout");
                ImGuiWindow *player = nullptr;
                for (auto *window : ImGui::GetCurrentContext()->Windows)
                    if (std::string(window->Name).find("###LocalPlayerDisplay") != std::string::npos)
                        player = window;
                if (!player || !player->Viewport || player->Viewport == ImGui::GetMainViewport() ||
                    !player->Viewport->PlatformHandle)
                    throw std::runtime_error("Player Display has no independent platform framebuffer");
                capture(app,
                        output / (frame == 65    ? "player-blackout"
                                  : frame == 100 ? "player-published"
                                                 : "player-revoked"),
                        static_cast<GLFWwindow *>(player->Viewport->PlatformHandle));
                if(artShowcase && frame==125) {
                    artShowcaseCompleted=true;
                    glfwSetWindowShouldClose(ApplicationQaHarness::window(app),true);
                }
            }
            if (itemStep == 18)
            {
                capture(app, output / "failed-item-save");
            }
            if (itemStep == 23)
            {
                capture(app, output / "retried-item-save");
            }
            if (itemStep == 38) capture(app, output / "advancement-class");
            if (itemStep == 41) capture(app, output / "advancement-choices");
            if (itemStep == 44) capture(app, output / "advancement-magic");
            if (itemStep == 47 && !trace->find(gm::localization::trUi("I reviewed these changes and feature prerequisites")).empty()) capture(app, output / "advancement-review");
            if (itemStep == 73) { capture(app, output / "saved-input-retained"); glfwSetWindowShouldClose(ApplicationQaHarness::window(app), true); }
            if (itemStep == 30)
            {
                capture(app, output / "combat-after-click");

            }
        };
        auto app = std::make_shared<Application>(std::move(options));
        try
        {
            app->run();
        }
        catch (...)
        {
            trace.reset();
            throw;
        }
        if (artShowcase ? !artShowcaseCompleted : itemStep != 73)
        {
            trace.reset();
            throw std::runtime_error("Scripted application interaction exceeded its frame budget");
        }
        trace.reset();
        std::cout << "Actual application framebuffer captured; no reference approval asserted.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
