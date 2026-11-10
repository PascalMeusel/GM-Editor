#include "ApplicationInternal.hpp"

using namespace gm::application_detail;

void Application::run()
{
    std::uint64_t appliedLocalizationRevision = 0;
    std::uint64_t frameNumber = 0;
    startupDiagnostics.record("main_loop.enter");
    while (!glfwWindowShouldClose(mainWindow.get()))
    {
        // Only trace startup frames; errors are separately capped by the GLFW
        // callback. No continuous per-frame file writes during ordinary use.
        const bool traceStartupFrame = frameNumber < 3 || frameNumber == 59;
        const auto trace = [&](const char *phase, const std::string &detail = std::string())
        {
            if (traceStartupFrame)
                startupDiagnostics.record("frame." + std::to_string(frameNumber) + "." + phase, detail);
        };
        trace("poll.begin");
        glfwPollEvents();
        trace("poll.complete");
        const auto &localization = gm::localization::LocalizationService::instance();
        if (appliedLocalizationRevision != localization.revision())
        {
            glfwSetWindowTitle(mainWindow.get(), ProductInfo::windowTitle);
            appliedLocalizationRevision = localization.revision();
        }
        if (glfwWindowShouldClose(mainWindow.get()) &&
            activeWindowController == editor)
        {
            glfwSetWindowShouldClose(mainWindow.get(), false);
            requestEditorAppCommand(EditorAppCommand::ExitProgram);
        }

        // A retired map job must also drain while its workspace is hidden or
        // the main menu is active. This tick never renders or publishes output.
        if (editor) editor->pollRetiredMapWork();
        if (activeWindowController == editor)
            EditorLayoutProfileService::getInstance().beginFrameForCampaign(editor->currentCampaign());
        else
            EditorLayoutProfileService::getInstance().suspend();

        ensureMainContext(mainWindow.get(), startupDiagnostics);
        trace("renderer_new_frame.begin");
        ImGui_ImplOpenGL3_NewFrame();
        trace("platform_new_frame.begin");
        ImGui_ImplGlfw_NewFrame();
        trace("imgui_new_frame.begin");
        if (runOptions.beforeFrame) runOptions.beforeFrame(*this, frameNumber);
        ImGui::NewFrame();
        trace("ui.begin");
        ImGuiIO &io = ImGui::GetIO();
        ApplyAccessibilityPreferences(io);

        // --- Create the main, full-screen dockspace ---
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // Begin the dockspace window
        ImGui::Begin("MainDockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(3); // Pop the 3 style vars we pushed

        // Create the actual dockspace ID and layout
        ImGuiID dockspace_id = ImGui::GetID(kEditorDockSpaceName);
        ImGuiID editorWorkDockId = dockspace_id;
        const bool utilityRailRequested = false;
        const std::vector<ImGuiDockNode *> utilityLeaves = utilityRailLeaves(dockspace_id);
        ImGuiDockNode *sidebarLeaf = sidebarShellLeaf(dockspace_id);
        if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr || sidebarLeaf == nullptr || editorUtilityRailLayoutActive != utilityRailRequested || (utilityRailRequested && utilityLeaves.size() < 3))
        {
            editorWorkDockId = buildEditorShellDockLayout(dockspace_id, viewport->WorkSize, utilityRailRequested);
            editorUtilityRailLayoutActive = utilityRailRequested;
        }
        else if (utilityRailRequested)
        {
            if (ImGuiDockNode *centralNode = ImGui::DockBuilderGetCentralNode(dockspace_id))
            {
                editorWorkDockId = centralNode->ID;
                dockUtilityShellWindows(utilityLeaves[0]->ID, utilityLeaves[1]->ID, utilityLeaves[2]->ID);
            }
        }
        else if (ImGuiDockNode *centralNode = ImGui::DockBuilderGetCentralNode(dockspace_id))
        {
            editorWorkDockId = centralNode->ID;
        }
        if (ImGuiDockNode *activeSidebarLeaf = sidebarShellLeaf(dockspace_id))
        {
            DockableWidget::setSidebarDockId(activeSidebarLeaf->ID);
            ImGui::DockBuilderDockWindow("Window Selector", activeSidebarLeaf->ID);
            ImGui::DockBuilderDockWindow("Workflow Sidebar###Window Selector", activeSidebarLeaf->ID);
        }
        applyEditorShellRootDockFlags(dockspace_id);
        DockableWidget::setDefaultDockId(editorWorkDockId);
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F1))
            accessibilityPreferences().showKeyboardHelp = !accessibilityPreferences().showKeyboardHelp;
        if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Equal))
            adjustAccessibleTextScale(0.05f);
        if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Minus))
            adjustAccessibleTextScale(-0.05f);
        if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_0))
            setAccessibleTextScale(1.0f);
        if (!io.WantTextInput && io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_C))
            toggleCompactTables();

        if (activeWindowController == editor)
        {
            const auto saveAndCommitActiveText = [&](const std::string &reason) {
                if (!editor->saveCurrentCampaign(reason))
                    return false;
                // InputText keeps an activation-time edit buffer. Once the
                // backing campaign has been saved, drop that buffer so Escape
                // cannot restore its pre-save value on the next frame.
                ImGui::ClearActiveID();
                return true;
            };
            const bool blackoutPressed = ImGui::IsKeyPressed(ImGuiKey_B, false);
            const LocalPlayerDisplayModel &playerOutput =
                editor->currentCampaign().localPlayerDisplay;
            const bool playerOutputMayBeVisible =
                playerOutput.enabled && !playerOutput.blackout;
            // Blackout is fail-closed and must not be delayed by a focused
            // text box. Consume only this shortcut's B/b character so the
            // field itself remains byte-for-byte unchanged; other queued
            // input is left intact.
            handleGlobalPlayerDisplayBlackoutShortcut(
                io.InputQueueCharacters,
                blackoutPressed,
                playerOutputMayBeVisible,
                io.WantTextInput,
                [&]() { editor->blackoutLocalPlayerDisplayOutput(); });
            const bool mapEditing = editor->navigationContext().current().editorRoute == "show:map_creator";
            if (!mapEditing && !io.WantTextInput && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal))
                editor->undoCommand();
            if (!mapEditing && !io.WantTextInput && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal))
                editor->redoCommand();
            if (io.KeyCtrl && !io.KeyAlt && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S))
                saveAndCommitActiveText("keyboard shortcut");
            if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
                requestEditorAppCommand(EditorAppCommand::LoadGame);
            if (!io.WantTextInput && io.KeyCtrl && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_M))
                requestEditorAppCommand(EditorAppCommand::BackToMainMenu);
            if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q))
                requestEditorAppCommand(EditorAppCommand::ExitProgram);
            if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F))
                editor->showGlobalSearch();
            if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P))
                editor->showCommandPalette();
            if (io.KeyCtrl && !io.KeyAlt && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_K))
                editor->showCommandPalette();
            if (!io.WantTextInput && io.KeyCtrl && !io.KeyAlt && !io.KeyShift)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_1)) editor->activateWorkspaceByIndex(0);
                else if (ImGui::IsKeyPressed(ImGuiKey_2)) editor->activateWorkspaceByIndex(1);
                else if (ImGui::IsKeyPressed(ImGuiKey_3)) editor->activateWorkspaceByIndex(2);
                else if (ImGui::IsKeyPressed(ImGuiKey_4)) editor->activateWorkspaceByIndex(3);
                else if (ImGui::IsKeyPressed(ImGuiKey_5)) editor->activateWorkspaceByIndex(4);
                else if (ImGui::IsKeyPressed(ImGuiKey_6)) editor->activateWorkspaceByIndex(5);
                else if (ImGui::IsKeyPressed(ImGuiKey_7)) editor->activateWorkspaceByIndex(6);
                else if (ImGui::IsKeyPressed(ImGuiKey_8)) editor->activateWorkspaceByIndex(7);
            }
            if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R))
                editor->quickRollD20();
            if (!io.WantTextInput && io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
                editor->advanceCombatTurn();
            if (!io.WantTextInput && io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_N))
                editor->openCampaignNotes();
            // Ctrl+Alt+M returns to Campaigns. Keep the plain mute shortcut
            // mutually exclusive so one key chord never triggers two actions.
            if (!io.WantTextInput && io.KeyCtrl && !io.KeyAlt && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_M))
                editor->toggleAudioMute();

            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu(gm::localization::tr(gm::localization::TextId::MenuFile)))
                {
                    if (ImGui::MenuItem(gm::localization::tr(gm::localization::TextId::MenuSaveCampaign), "Ctrl+S"))
                        saveAndCommitActiveText("menu save");
                    if (ImGui::MenuItem(gm::localization::tr(gm::localization::TextId::MenuOpenCampaign), "Ctrl+O"))
                        requestEditorAppCommand(EditorAppCommand::LoadGame);
                    if (ImGui::MenuItem(gm::localization::tr(gm::localization::TextId::MenuBackToCampaigns), "Ctrl+Alt+M"))
                        requestEditorAppCommand(EditorAppCommand::BackToMainMenu);
                    ImGui::Separator();
                    if (ImGui::MenuItem(gm::localization::tr(gm::localization::TextId::MenuExitProgram), "Ctrl+Q"))
                        requestEditorAppCommand(EditorAppCommand::ExitProgram);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(gm::localization::tr(gm::localization::TextId::MenuEdit)))
                {
                    ProjectCommandManager &commands = ProjectCommandManager::getInstance();
                    const std::string undoBase = gm::localization::tr(gm::localization::TextId::CommonUndo);
                    const std::string redoBase = gm::localization::tr(gm::localization::TextId::CommonRedo);
                    const std::string undoLabel = commands.undoLabel().empty() ? undoBase : undoBase + " " + commands.undoLabel();
                    const std::string redoLabel = commands.redoLabel().empty() ? redoBase : redoBase + " " + commands.redoLabel();
                    if (mapEditing)
                        ImGui::TextUnformatted(gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                            ? "Kartenverlauf: Rückgängig/Wiederholen im Karteneditor" : "Map history: use Undo/Redo in the map editor");
                    if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, !mapEditing && commands.canUndo()))
                        editor->undoCommand();
                    if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, !mapEditing && commands.canRedo()))
                        editor->redoCommand();
                    if (!commands.lastError().empty())
                    { ImGui::Separator(); ImGui::TextWrapped("%s", commands.lastError().c_str()); }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(gm::localization::tr(gm::localization::TextId::MenuView)))
                {
                    RenderAccessibilityMenu();
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(gm::localization::tr(gm::localization::TextId::MenuHelp)))
                {
                    if (ImGui::MenuItem(gm::localization::tr(gm::localization::TextId::MenuSearchCommands), "Ctrl+K"))
                        editor->showCommandPalette();
                    if (ImGui::MenuItem(gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                                            ? "Erste Schritte"
                                            : "Quick Start"))
                        quickStartWindowOpen = true;
                    if (ImGui::MenuItem(gm::localization::tr(gm::localization::TextId::CommonKeyboardHelp), "F1"))
                        accessibilityPreferences().showKeyboardHelp = true;
                    ImGui::Separator();
                    if (ImGui::MenuItem(gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                                            ? "Über & Support"
                                            : "About & Support"))
                        supportWindowOpen = true;
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
        }

        // Update all of our registered window views
        if (activeWindowController->getWindowActive())
        {
            activeWindowController->update(mainWindow);
        }
        else
        {
            switch (activeWindowController->getWindowType())
            {
            case WindowTypes::MainMenuWindow:
            {
                const bool opened=editor->setCurrentCampaign(CampaignRepository::activeCampaign());
                // A refused checkpoint preserves the old editor and its visible
                // error; the requested campaign has not been opened here.
                activeWindowController = editor;
                if(!opened)editor->setWindowOpacity(true);
                break;
            }
            case WindowTypes::EditorWindow:
            {
                activeWindowController = mainMenu;
                break;
            }
            }
            activeWindowController->setWindowOpacity(true);
            activeWindowController->update(mainWindow);
        }
        renderEditorAppCommandPrompt();
        RenderKeyboardHelpOverlay();
        renderQuickStartWindow();
        renderSupportWindow();
        ImGui::End();
        trace("ui.complete");

        EditorLayoutProfileService::getInstance().saveIfRequested();

        trace("render.begin");
        ImGui::Render();
        // A detached/native player window may have used another context while
        // the controllers updated. Always draw the main UI into its own one.
        ensureMainContext(mainWindow.get(), startupDiagnostics);
        // All UI producers, including EndFrame hooks, have registered visible
        // frozen images. Upload at most one live completion on the main context;
        // existing draw lists stay unchanged and use it on a later frame.
        processAsyncImageTextures(static_cast<std::uint64_t>(ImGui::GetFrameCount()));
        int display_w, display_h;
        glfwGetFramebufferSize(mainWindow.get(), &display_w, &display_h);
        if (traceStartupFrame)
        {
            const ImDrawData *drawData = ImGui::GetDrawData();
            int windowWidth = 0, windowHeight = 0;
            glfwGetWindowSize(mainWindow.get(), &windowWidth, &windowHeight);
            trace("main_draw.begin", "window=" + std::to_string(windowWidth) + "x" + std::to_string(windowHeight) +
                  " framebuffer=" + std::to_string(display_w) + "x" + std::to_string(display_h) +
                  " vertices=" + std::to_string(drawData ? drawData->TotalVtxCount : 0) +
                  " indices=" + std::to_string(drawData ? drawData->TotalIdxCount : 0) +
                  " viewports=" + std::to_string(ImGui::GetPlatformIO().Viewports.Size) +
                  " " + windowPresentationState(mainWindow.get()));
        }
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.105f, 0.112f, 0.120f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (traceStartupFrame)
            trace("main_draw.complete", "gl_error=" + std::to_string(glGetError()));

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            trace("platform_windows.begin");
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            ensureMainContext(mainWindow.get(), startupDiagnostics);
            trace("platform_windows.complete");
        }

        if (runOptions.afterRender) runOptions.afterRender(*this, frameNumber);
        // Hooks/platform windows may change the current context. Scoped image
        // retirement happens only after all viewport draws and swaps below.
        ensureMainContext(mainWindow.get(), startupDiagnostics);
        trace("swap.begin");
        glfwSwapBuffers(mainWindow.get());
        // A return from SwapBuffers is evidence of progress, not proof that a
        // compositor/driver actually displayed the submitted image.
        trace("swap.returned");
        collectExpiredImageTextures();
        if (frameNumber == 0 && runOptions.showWindow)
        {
            // Warm the drawable with a complete frame before exposing it.
            // Show must precede Maximize to use GLFW's visible native resize
            // path. Do this once only: subsequent user restores/resizes stand.
            startupDiagnostics.record("window.present", "begin after_first_swap " + windowPresentationState(mainWindow.get()));
            glfwShowWindow(mainWindow.get());
            if (runOptions.maximizeOnStart) glfwMaximizeWindow(mainWindow.get());
            glfwPollEvents();
            ensureMainContext(mainWindow.get(), startupDiagnostics);
            startupDiagnostics.record("window.present", "complete " + windowPresentationState(mainWindow.get()));
        }
        ++frameNumber;
        if (runOptions.maximumFrames && frameNumber >= runOptions.maximumFrames) break;
    }
    startupDiagnostics.record("main_loop.exit", "frames=" + std::to_string(frameNumber));
}
