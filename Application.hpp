#pragma once
#include "ApplicationRunOptions.hpp"
#include <vector>
#include <memory>

#include "Views/IWindowView.hpp"
#include "WindowTypes.hpp"
#include "Models/Project/StartupDiagnostics.hpp"

#include "Controllers/MainMenu/MainMenuWindowController.hpp"
#include "Controllers/Editor/EditorWindowController.hpp"
// Forward declare GLFWwindow to avoid including glfw3.h in the header
struct GLFWwindow;

class Application : public std::enable_shared_from_this<Application>
{
public:
    explicit Application(ApplicationRunOptions options = {});
    ~Application();

    void run();
private:
    friend struct ApplicationQaHarness;
    ApplicationRunOptions runOptions;
    enum class EditorAppCommand
    {
        None,
        LoadGame,
        BackToMainMenu,
        ExitProgram
    };

    void Initialize();
    void Shutdown();
    void requestEditorAppCommand(EditorAppCommand command);
    void executeEditorAppCommand(EditorAppCommand command);
    void renderEditorAppCommandPrompt();
    void renderQuickStartWindow();
    void renderSupportWindow();

    std::shared_ptr<GLFWwindow> mainWindow;
    std::shared_ptr<IWindowController> activeWindowController;

    std::shared_ptr<EditorWindowController> editor;
    std::shared_ptr<MainMenuWindowController> mainMenu;
    StartupDiagnostics startupDiagnostics;
    bool glfwInitialized = false;
    bool imguiContextCreated = false;
    bool imguiPlatformInitialized = false;
    bool imguiRendererInitialized = false;
    bool initializationComplete = false;
    bool glfwErrorCallbackInstalled = false;
    void (*previousGlfwErrorCallback)(int, const char *) = nullptr;
    EditorAppCommand pendingEditorAppCommand = EditorAppCommand::None;
    bool editorAppCommandPromptRequested = false;
    bool editorUtilityRailLayoutActive = false;
    bool quickStartWindowOpen = false;
    bool supportWindowOpen = false;
};
