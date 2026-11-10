#include "Views/Editor/Widgets/UiPortrait.hpp"
#include "Views/Editor/Widgets/UiInteractionTrace.hpp"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "fixtures/QaFramebufferCapture.hpp"
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
ImVec2 centre(const UiInteractionTrace::Item &item)
{ return {(item.minimum.x + item.maximum.x) * .5f, (item.minimum.y + item.maximum.y) * .5f}; }
}

int main(int argc, char **argv)
{
    try
    {
        require(argc == 2, "Expected a new isolated output directory");
        const auto output = std::filesystem::absolute(argv[1]);
        require(!std::filesystem::exists(output), "Refusing to overwrite an earlier run");
        std::filesystem::create_directories(output);
#ifdef _WIN32
        _putenv_s("TTRPG_DATA_ROOT", output.string().c_str());
        _putenv_s("TTRPG_DB_PATH", (output / "TTRPG.db").string().c_str());
#else
        setenv("TTRPG_DATA_ROOT", output.string().c_str(), 1);
        setenv("TTRPG_DB_PATH", (output / "TTRPG.db").string().c_str(), 1);
#endif
        require(glfwInit() != 0, "GLFW initialization failed");
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        auto *window = glfwCreateWindow(480, 720, "Portrait picker QA", nullptr, nullptr);
        require(window, "Could not create hidden QA window");
        glfwMakeContextCurrent(window);
        ImGui::CreateContext();
        auto &io = ImGui::GetIO(); io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigInputTrickleEventQueue = false;
        const auto font = ApplicationPaths::executableDirectory() / "Resources/Fonts/Inter/Inter-Regular.ttf";
        require(std::filesystem::exists(font), "Bundled Inter font is missing");
        io.Fonts->AddFontFromFileTTF(font.u8string().c_str(), 18.f);
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, false); ImGui_ImplOpenGL3_Init("#version 130");
        const auto &art = getCachedImageTexture(PortraitUi::artworkPath(0));
        require(art.loaded, "Human portrait atlas did not load");
        std::ofstream report(output / "checks.txt");
        report << "Production portrait picker component, real atlas and OpenGL.\n"
                  "This is component interaction evidence, not a complete application walkthrough.\n";
        {
            UiInteractionTrace trace(ImGui::GetCurrentContext());
            for (bool german : {false, true}) for (bool compact : {false, true})
            {
                const float panelWidth = compact ? 240.f : 400.f;
                const int viewportWidth = compact ? 320 : 480;
                glfwSetWindowSize(window, viewportWidth, 720); glfwPollEvents();
                int identity = 0; ImGuiID comboId = 0;
                bool textureAvailable = true;
                const CachedImageTexture missing;
                const std::string name = std::string(german ? "de" : "en") + (compact ? "-compact" : "-standard");
                auto frame = [&] {
                    trace.beginFrame();
                    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame();
                    io.DeltaTime = 1.f / 60; ImGui::NewFrame();
                    ImGui::SetNextWindowPos({40, 25}); ImGui::SetNextWindowSize({panelWidth, 670});
                    ImGui::Begin("Portrait picker", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
                    ImGui::SetWindowFontScale(compact ? 1.5f : 1.f);
                    ImGui::TextUnformatted(german ? "Porträt wählen" : "Choose a portrait");
                    const auto crop = gm::portraits::artworkRegion(identity, art.width, art.height);
                    ImGui::Image(art.textureId, {120, 180}, {crop.u0, crop.v0}, {crop.u1, crop.v1});
                    comboId = ImGui::GetID("###PortraitIdentity");
                    PortraitUi::identityPicker(identity, textureAvailable ? art : missing, german);
                    ImGui::End(); ImGui::Render();
                    int w, h; glfwGetFramebufferSize(window, &w, &h);
                    glViewport(0, 0, w, h); glClearColor(.035f, .05f, .065f, 1); glClear(GL_COLOR_BUFFER_BIT);
                    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); glFinish();
                };
                const auto click = [&](ImVec2 p) {
                    io.AddMousePosEvent(p.x, p.y); io.AddMouseButtonEvent(0, true); frame();
                    io.AddMouseButtonEvent(0, false); frame(); frame();
                };
                frame(); frame(); frame();
                const auto combo = trace.items().at(comboId);
                click(centre(combo)); frame();
                std::vector<UiInteractionTrace::Item> thumbnails;
                for (const auto &[id, item] : trace.items())
                    if (item.window.rfind("##Combo", 0) == 0 && item.label != item.window && item.maximum.x - item.minimum.x > 30 &&
                        item.maximum.y - item.minimum.y > (item.maximum.x - item.minimum.x) * 1.2f)
                        thumbnails.push_back(item);
                gm::qa::captureFramebuffer(window, output / (name + "-open.bmp"));
                if (thumbnails.size() != 8) {
                    std::ofstream diagnostic(output / (name + "-items.txt"));
                    for (const auto &[id, item] : trace.items())
                        diagnostic << id << ' ' << item.window << ' ' << item.label << ' '
                                   << item.minimum.x << ',' << item.minimum.y << ' '
                                   << item.maximum.x << ',' << item.maximum.y << '\n';
                    throw std::runtime_error("Picker did not expose eight thumbnails; found " + std::to_string(thumbnails.size()));
                }
                std::sort(thumbnails.begin(), thumbnails.end(), [](const auto &a, const auto &b) {
                    return std::abs(a.minimum.y-b.minimum.y) > 1.f ? a.minimum.y < b.minimum.y : a.minimum.x < b.minimum.x;
                });
                for (const auto &item : thumbnails)
                    require(item.minimum.x >= 0 && item.maximum.x <= viewportWidth && item.minimum.y >= 0 && item.maximum.y <= 720,
                            "Thumbnail is outside the supported QA viewport");
                click(centre(thumbnails[5]));
                require(identity == 5, "Click did not select the sixth portrait");
                gm::qa::captureFramebuffer(window, output / (name + "-selected.bmp"));
                click(centre(trace.items().at(comboId)));
                io.AddKeyEvent(ImGuiKey_Escape, true); frame(); io.AddKeyEvent(ImGuiKey_Escape, false); frame();
                require(identity == 5 && ImGui::GetCurrentContext()->OpenPopupStack.empty(), "Cancel changed the portrait or left the popup open");
                click(centre(trace.items().at(comboId)));
                io.AddMousePosEvent(-100.f, -100.f); frame();
                io.AddKeyEvent(ImGuiKey_LeftArrow, true); frame();
                io.AddKeyEvent(ImGuiKey_LeftArrow, false); frame();
                io.AddKeyEvent(ImGuiKey_Enter, true); frame();
                io.AddKeyEvent(ImGuiKey_Enter, false); frame();
                require(identity == 4 && ImGui::GetCurrentContext()->OpenPopupStack.empty(),
                        "Keyboard navigation did not select the adjacent portrait");
                textureAvailable = false; frame();
                click(centre(trace.items().at(comboId)));
                const std::string lastLabel = german ? "Porträt 8" : "Portrait 8";
                const auto last = std::find_if(trace.items().begin(), trace.items().end(), [&](const auto &entry) {
                    return entry.second.window.rfind("##Combo", 0) == 0 && entry.second.label == lastLabel;
                });
                require(last != trace.items().end(), "Missing-texture fallback did not expose the eighth identity");
                gm::qa::captureFramebuffer(window, output / (name + "-fallback.bmp"));
                click(centre(last->second));
                require(identity == 7 && ImGui::GetCurrentContext()->OpenPopupStack.empty(),
                        "Missing-texture fallback did not select the eighth portrait");
                report << name << ": 8 visible thumbnails, selected identity 6, Escape retained choice; Left/Enter selected identity 5; missing-image fallback selected identity 8\n";
            }
        }
        clearImageTextureCache();
        ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
        glfwDestroyWindow(window); glfwTerminate();
        std::cout << "Portrait picker: four locale/size profiles passed.\n";
        return 0;
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
