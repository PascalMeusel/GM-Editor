#include "ApplicationInternal.hpp"

using namespace gm::application_detail;

void destroyGlfwWindow(GLFWwindow *window)
{
    if (window)
        glfwDestroyWindow(window);
}

void Application::Initialize()
{
    startupDiagnostics.open(ApplicationPaths::writableRoot());
    startupDiagnostics.record("startup.begin", ProductInfo::windowTitle);
    startupDiagnostics.record("startup.log", startupDiagnostics.path().string());
    recordedGlfwErrors.store(0);
    activeStartupDiagnostics.store(&startupDiagnostics);
    previousGlfwErrorCallback = glfwSetErrorCallback(recordGlfwError);
    glfwErrorCallbackInstalled = true;
    startupDiagnostics.record("settings.load", "begin");
    auto &localization = gm::localization::LocalizationService::instance();
    std::string localizationError;
    const std::filesystem::path settingsPath = gm::localization::LocalizationSettingsStore::defaultPath();
    if (!localization.initialize(settingsPath, &localizationError) && !localizationError.empty())
        startupDiagnostics.record("settings.localization", localizationError);
    loadAccessibilityPreferences(settingsPath);
    startupDiagnostics.record("settings.load", "complete");
    requireStartupStep(startupDiagnostics, "glfw.initialize", [] { return glfwInit() == GLFW_TRUE; },
                       "GLFW konnte nicht initialisiert werden / Failed to initialize GLFW");
    glfwInitialized = true;

    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);

    // Build the normal drawable while hidden. run() submits and swaps its
    // first complete frame before the one-time native show/maximize sequence.
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_FALSE);
    startupDiagnostics.record("window.create", "begin visible=0 maximized=0");
    GLFWwindow *raw_window = glfwCreateWindow(runOptions.width, runOptions.height, ProductInfo::windowTitle, nullptr, nullptr);
    // Window hints persist globally; detached player/ImGui windows keep their
    // independent sizing and must not inherit the main-window startup policy.
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    if (!raw_window)
    {
        throw std::runtime_error("Das Programmfenster konnte nicht erstellt werden / Failed to create the application window");
    }
    mainWindow = std::shared_ptr<GLFWwindow>(raw_window, destroyGlfwWindow);
    glfwSetWindowSizeLimits(mainWindow.get(), 1280, 720, GLFW_DONT_CARE, GLFW_DONT_CARE);
    startupDiagnostics.record("window.create", "complete " + windowPresentationState(mainWindow.get()));
    startupDiagnostics.record("opengl.context", "begin");
    ensureMainContext(mainWindow.get(), startupDiagnostics);
    glfwSwapInterval(1); // Enable vsync
    const auto glText = [](GLenum name)
    {
        const GLubyte *value = glGetString(name);
        return value ? reinterpret_cast<const char *>(value) : "unavailable";
    };
    startupDiagnostics.record("opengl.vendor", glText(GL_VENDOR));
    startupDiagnostics.record("opengl.renderer", glText(GL_RENDERER));
    startupDiagnostics.record("opengl.version", glText(GL_VERSION));
    if (!glGetString(GL_VERSION))
        throw std::runtime_error("OpenGL-Kontext ist nicht verwendbar / The OpenGL context is not usable");
    startupDiagnostics.record("opengl.context", "complete vsync=1");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imguiContextCreated = true;
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;

    startupDiagnostics.record("imgui.fonts", "begin");
    LoadEditorFont(io);
    startupDiagnostics.record("imgui.fonts", "complete");
    ApplyEditorTheme(io);
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    requireStartupStep(startupDiagnostics, "imgui.platform", [&] { return ImGui_ImplGlfw_InitForOpenGL(mainWindow.get(), true); },
                       "Fensterdarstellung konnte nicht initialisiert werden / Failed to initialize the window backend");
    imguiPlatformInitialized = true;
    requireStartupStep(startupDiagnostics, "imgui.renderer", [&] { return ImGui_ImplOpenGL3_Init(glsl_version); },
                       "Grafikdarstellung konnte nicht initialisiert werden / Failed to initialize the graphics backend");
    imguiRendererInitialized = true;
    requireStartupStep(startupDiagnostics, "imgui.device_objects", [] { return ImGui_ImplOpenGL3_CreateDeviceObjects(); },
                       "Grafik-Shader konnten nicht erstellt werden / Failed to create graphics shaders");

    // --- Create and add our window views ---
    startupDiagnostics.record("controllers.editor", "begin");
    editor = std::make_shared<EditorWindowController>();
    startupDiagnostics.record("controllers.editor", "complete");
    startupDiagnostics.record("controllers.launcher", "begin");
    mainMenu = std::make_shared<MainMenuWindowController>();
    startupDiagnostics.record("controllers.launcher", "complete");
    activeWindowController = mainMenu;
    editor->setWindowOpacity(false);

    startupDiagnostics.record("window.present.deferred", "until first completed buffer swap " + windowPresentationState(mainWindow.get()));
    initializationComplete = true;
    startupDiagnostics.record("startup.initialized");
}

void Application::Shutdown()
{
    startupDiagnostics.record("shutdown.begin");
    if (initializationComplete)
    {
        try
        {
            persistAccessibilityPreferences();
            EditorLayoutProfileService::getInstance().saveCurrentProfile();
        }
        catch (const std::exception &error)
        {
            startupDiagnostics.record("shutdown.preferences", error.what());
        }
        catch (...)
        {
            startupDiagnostics.record("shutdown.preferences", "unknown failure");
        }
        initializationComplete = false;
    }

    if (mainWindow)
        glfwMakeContextCurrent(mainWindow.get());
    // Controllers can own native player windows and graphics/audio resources.
    // Release them before the main context and GLFW are destroyed.
    activeWindowController.reset();
    editor.reset();
    mainMenu.reset();
    if (mainWindow)
        glfwMakeContextCurrent(mainWindow.get());
    // Decode owns no GL state. Join it before decoder statics and renderer
    // resources can be destroyed; normal review cancellation never joins it.
    shutdownImageDecodeWorker();
    if (imguiContextCreated)
    {
        clearImageTextureCache();
        if (imguiRendererInitialized)
        {
            ImGui_ImplOpenGL3_Shutdown();
            imguiRendererInitialized = false;
        }
        if (imguiPlatformInitialized)
        {
            ImGui_ImplGlfw_Shutdown();
            imguiPlatformInitialized = false;
        }
        ImGui::DestroyContext();
        imguiContextCreated = false;
    }

    mainWindow.reset();
    if (glfwInitialized)
    {
        glfwTerminate();
        glfwInitialized = false;
    }
    if (glfwErrorCallbackInstalled)
    {
        glfwSetErrorCallback(previousGlfwErrorCallback);
        glfwErrorCallbackInstalled = false;
        activeStartupDiagnostics.store(nullptr);
    }
    startupDiagnostics.record("shutdown.complete");
}
