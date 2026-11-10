// Internal renderWindowSelector continuation: primary navigation and workflow groups.
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    if (DockableWidget::sidebarDockId() != 0)
        ImGui::SetNextWindowDockID(DockableWidget::sidebarDockId(), ImGuiCond_Always);
    ImGui::Begin(localizedUiLabel("Workflow Sidebar###Window Selector").c_str(), nullptr, flags);

    const Campaign &shellCampaign = _controller.currentCampaign();
    const ProjectSaveState &shellSaveState = ProjectSaveService::getInstance().state();
    const bool shellUnsaved = _controller.hasUnsavedChanges();

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    pushBoldFont();
    ImGui::PushStyleColor(ImGuiCol_Text, WorkspaceUi::Text);
    ImGui::TextUnformatted(gm::localization::tr(gm::localization::TextId::ProductName));
    ImGui::PopStyleColor();
    popBoldFont();
    ImGui::PushStyleColor(ImGuiCol_Text, WorkspaceUi::Muted);
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                       shellCampaign.name.empty()
                           ? gm::localization::tr(gm::localization::TextId::CommonUntitledCampaign)
                           : shellCampaign.name.c_str());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    const std::string searchLabel =
        std::string(gm::localization::tr(gm::localization::TextId::CommonSearch)) + "   Ctrl+K";
    if (drawUiButton(searchLabel.c_str(), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                     "Find campaign entities, open a workspace, or run a common action."))
    {
        _universalSearchOpen = true;
    }
    renderEditorDraftRecoveryLauncher();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    const bool germanNav =
        gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const char *prepareModeLabel = germanNav ? "Vorbereiten" : "Prepare";
    const char *liveModeLabel = germanNav ? "Live leiten" : "Run live";
    const char *prepareModeDescription = germanNav ? "Zur Kampagnenvorbereitung wechseln."
                                                    : "Switch to campaign preparation.";
    const char *liveModeDescription = germanNav ? "Die vorbereitete Sitzung leiten."
                                                 : "Run the prepared session.";
    bool prepareModePressed = false;
    bool liveModePressed = false;
    if (TaskUi::equalColumnButtonsFit(prepareModeLabel, liveModeLabel,
                                      WorkspaceUi::NavigationHorizontalPadding))
    {
        if (ImGui::BeginTable("PrepareRunMode", 2, ImGuiTableFlags_SizingStretchSame |
                                                    ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextColumn();
            prepareModePressed = WorkspaceUi::navigationItem(
                prepareModeLabel, "PR", _activeWorkspace != EditorWorkspace::Live, prepareModeDescription);
            ImGui::TableNextColumn();
            liveModePressed = WorkspaceUi::navigationItem(
                liveModeLabel, "LI", _activeWorkspace == EditorWorkspace::Live, liveModeDescription);
            ImGui::EndTable();
        }
    }
    else
    {
        prepareModePressed = WorkspaceUi::navigationItem(
            prepareModeLabel, "PR", _activeWorkspace != EditorWorkspace::Live, prepareModeDescription);
        liveModePressed = WorkspaceUi::navigationItem(
            liveModeLabel, "LI", _activeWorkspace == EditorWorkspace::Live, liveModeDescription);
    }
    if (prepareModePressed)
        activateWorkspace(_activeWorkspace == EditorWorkspace::Live ? EditorWorkspace::Home : _activeWorkspace);
    if (liveModePressed)
        activateWorkspace(EditorWorkspace::Live);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::TextDisabled("%s", germanNav ? "Sitzung" : "Session");
    if (WorkspaceUi::navigationItem(editorWorkspaceLabel(EditorWorkspace::Home), "SE",
                                    _activeWorkspace == EditorWorkspace::Home,
                                    germanNav ? "Szenen und Material für die nächste Sitzung vorbereiten."
                                              : "Prepare scenes and material for the next session."))
        activateWorkspace(EditorWorkspace::Home);

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    ImGui::TextDisabled("%s", germanNav ? "Kampagne" : "Campaign");
    for (const EditorWorkspace workspace : {EditorWorkspace::Story, EditorWorkspace::Cast,
                                             EditorWorkspace::World, EditorWorkspace::Encounters})
    {
        const EditorWorkspaceDefinition &entry = editorWorkspaceDefinition(workspace);
        const char *recoveryAttention=workspace==EditorWorkspace::World?worldRecoveryAttentionTooltip():"";
        if (WorkspaceUi::navigationItem(editorWorkspaceLabel(workspace), entry.shortLabel,
                                        _activeWorkspace == workspace, *recoveryAttention?
                                            recoveryAttention:editorWorkspaceDescription(workspace)))
            activateWorkspace(workspace);
        if(*recoveryAttention) {
            // Draw within the existing item: its caption, ID, size and normal
            // navigation remain unchanged, including checkpoint refusal.
            const auto minimum=ImGui::GetItemRectMin(),maximum=ImGui::GetItemRectMax();
            const float radius=ImGui::GetFontSize()*.38f;
            const ImVec2 center(maximum.x-WorkspaceUi::NavigationHorizontalPadding-radius,(minimum.y+maximum.y)*.5f);
            auto *draw=ImGui::GetWindowDrawList();
            draw->AddCircleFilled(center,radius,ImGui::GetColorU32(WorkspaceUi::Amber));
            const auto text=ImGui::CalcTextSize("!");
            draw->AddText(ImVec2(center.x-text.x*.5f,center.y-text.y*.5f),ImGui::GetColorU32(WorkspaceUi::SurfaceRaised),"!");
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    ImGui::TextDisabled("%s", germanNav ? "Ressourcen" : "Resources");
    const bool audioSelected = _controller._navigation.current().editorRoute == "show:audio_manager";
    const EditorWorkspaceDefinition &compendium = editorWorkspaceDefinition(EditorWorkspace::Compendium);
    if (WorkspaceUi::navigationItem(editorWorkspaceLabel(compendium.workspace), compendium.shortLabel,
                                    _activeWorkspace == EditorWorkspace::Compendium && !audioSelected,
                                    editorWorkspaceDescription(compendium.workspace)))
        activateWorkspace(EditorWorkspace::Compendium);
    if (WorkspaceUi::navigationItem("Audio", "AU", audioSelected,
                                    germanNav ? "Musik, Atmosphäre und Effekte verwalten."
                                              : "Manage music, ambience, and effects."))
        _controller.showAudioManager();
    if (WorkspaceUi::navigationItem(germanNav ? "Spieleranzeige" : "Player display", "PD", _presentationPanelOpen,
                                    germanNav ? "Private und öffentliche Informationen vor der Anzeige prüfen."
                                              : "Review private and public information before display."))
        _controller.showPresentationPanel();
    if (_activeWorkspace != EditorWorkspace::Home && _activeWorkspace != EditorWorkspace::Live &&
        (_returnToLiveContext || !_sessionWorkspace.sessionId().empty()))
    {
        ImGui::Spacing();
        if (drawUiButton(_returnToLiveContext ? (germanNav ? "Zurück zu Live" : "Back to Live")
                                              : (germanNav ? "Zur Sitzung" : "Back to session"),
                         UiButtonRole::Secondary, ImVec2(-1, 0)))
            activateWorkspace(_returnToLiveContext ? EditorWorkspace::Live : EditorWorkspace::Home);
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    const EditorWorkspaceDefinition &settings = editorWorkspaceDefinition(EditorWorkspace::Settings);
    if (WorkspaceUi::navigationItem(editorWorkspaceLabel(settings.workspace), settings.shortLabel,
                                    _activeWorkspace == EditorWorkspace::Settings,
                                    editorWorkspaceDescription(settings.workspace)))
    {
        activateWorkspace(EditorWorkspace::Settings);
    }

    const char *saveLabel = shellUnsaved ? gm::localization::tr(gm::localization::TextId::CommonUnsaved)
                                         : gm::localization::tr(gm::localization::TextId::CommonAllChangesSaved);
    const float footerWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float footerHeight =
        ImGui::CalcTextSize(saveLabel, nullptr, false, footerWidth).y +
        (shellCampaign.ruleSet ? ImGui::CalcTextSize(shellCampaign.ruleSet->getName(), nullptr, false, footerWidth).y
                               : 0.0f) +
        (shellUnsaved ? ImGui::GetFrameHeightWithSpacing() : 0.0f) + 3.0f * ImGui::GetStyle().ItemSpacing.y;
    const float remainingHeight = ImGui::GetContentRegionAvail().y;
    if (remainingHeight > footerHeight)
        ImGui::Dummy(ImVec2(0.0f, remainingHeight - footerHeight));
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, shellUnsaved ? WorkspaceUi::Amber : WorkspaceUi::Muted);
    ImGui::TextWrapped("%s", saveLabel);
    ImGui::PopStyleColor();
    if (shellUnsaved)
    {
        if (drawSmallUiButton(gm::localization::tr(gm::localization::TextId::CommonSave), UiButtonRole::Primary,
                              "Save the current campaign."))
            _controller.saveCurrentCampaign("navigation save");
    }
    if (shellCampaign.ruleSet)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, WorkspaceUi::Muted);
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), shellCampaign.ruleSet->getName());
        ImGui::PopStyleColor();
    }

    ImGui::End();
    return;

    const Campaign &campaign = _controller.currentCampaign();
    ProjectSaveService &saveService = ProjectSaveService::getInstance();
    const ProjectSaveConfig &saveConfig = saveService.config();
    const ProjectSaveState &saveState = saveService.state();

    const std::string modeLabel = campaignModeLabel(campaign);
    const std::string selectionLabel = campaignSelectionLabel(campaign);
    const std::string nextActionLabel = campaignNextActionLabel(campaign, saveState);
    renderSidebarWorkflowHeader(campaign, modeLabel, nextActionLabel);
    ImGui::Separator();
    drawStatusActionStrip(
        modeLabel.c_str(), selectionLabel.c_str(), nextActionLabel.c_str(),
        "This strip shows the current campaign mode, what the shell is focused on, and the safest next action.");

    const bool hasPreparedEncounter = !campaign.encounters.empty();
    const bool playerDisplayArmed = campaign.localPlayerDisplay.enabled;
    const auto activateWorkflow = [this](const char *label) { _activeSidebarWorkflowLabel = label; };
    const std::vector<SidebarNavEntry> sidebarEntries = {
        {"Start", "Campaign Home", "home hub start summary health save", UiButtonRole::Primary,
         [this, activateWorkflow]() {
             activateWorkflow("Campaign Home");
             ImGui::SetWindowFocus("Campaign Home");
         }},
        {"Start", "Find Campaign", "find search entity tag reference", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Find Campaign");
             _globalSearchOpen = true;
         }},
        {"Start", "Save", "save dirty changes", saveState.dirty ? UiButtonRole::Primary : UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Save");
             _controller.saveCurrentCampaign("manual save");
         }},
        {"Create", "New Player", "create player character pc actor", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("New Player");
             _controller.showPlayerCreator();
         }},
        {"Create", "New NPC", "create npc monster creature faction deity actor", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("New NPC");
             _controller.showNPCCreator();
         }},
        {"Create", "New Map", "map creator draw terrain tile", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("New Map");
             _controller.showMapCreator();
         }},
        {"Create", "Encounter", "encounter prep combat roster initiative", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Encounter");
             _controller.showNewEncounterBuilder();
         }},
        {"Create", "Battlefield", "battlefield map combat terrain", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Battlefield");
             _controller.showBattlefieldCreator();
         }},
        {"Create", "Dungeon", "dungeon room trap secret loot exploration", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Dungeon");
             _controller.showDungeonBuilder();
         }},
        {"Prep Session", "Story Board", "story scene plot clue arc prep", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Story Board");
             _controller.showStoryWorkspace();
         }},
        {"Prep Session", "Party", "player party character hp ac prep", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Party");
             _controller.showPlayerOverview();
         }},
        {"Prep Session", "NPCs", "npc monster faction deity creature prep", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("NPCs");
             _controller.showNPCOverview();
         }},
        {"Prep Session", "Sheets", "sheet character player pc details", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Sheets");
             _controller.showCharacterSheet();
         }},
        {"Prep Session", "Maps", "map viewer travel party location", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Maps");
             _controller.showMapViewer();
         }},
        {"Run Table", "Live Dashboard", "live session table scene notes clock", UiButtonRole::Primary,
         [this, activateWorkflow]() {
             activateWorkflow("Live Dashboard");
             _controller.showLiveSessionDashboard();
         }},
        {"Run Table", "Combat", "combat initiative battle tracker attack turn", UiButtonRole::Primary,
         [this, activateWorkflow]() {
             activateWorkflow("Combat");
             _controller.showBattleTracker();
         },
         hasPreparedEncounter, "Build or load an encounter before opening Combat."},
        {"Run Table", "GM Screen", "gm screen reference dc conditions live", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("GM Screen");
             _controller.showGMScreen();
         }},
        {"Library", "Items", "item weapon armor consumable catalog", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Items");
             _controller.showItemCreator();
         }},
        {"Library", "Inventory", "inventory equipment item owner transfer loot", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Inventory");
             _controller.showInventoryManager();
         }},
        {"Library", "Shops", "shop merchant stock buy sell", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Shops");
             _controller.showShopManager();
         }},
        {"Library", "Spells", "spell magic library casting", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Spells");
             _controller.showSpellLibrary();
         }},
        {"Library", "Audio Library", "audio library import relink diagnostics advanced", UiButtonRole::Advanced,
         [this, activateWorkflow]() {
             activateWorkflow("Audio Library");
             _controller.showAudioManager();
         }},
        {"Present", "Table Audio", "audio music ambience effect sound", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Table Audio");
             _controller.showAudioQuickControls();
         }},
        {"Present", "Presentation", "presentation player display reveal handout", UiButtonRole::Primary,
         [this, activateWorkflow]() {
             activateWorkflow("Presentation");
             _controller.showPresentationPanel();
         }},
        {"Present", "Player Display", "player display screen public table", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Player Display");
             _controller.showLocalPlayerDisplay();
         },
         playerDisplayArmed, "Arm Player Display in the Presentation panel before opening the table display."},
        {"Advanced/System", "Command Palette", "command shortcut action jump", UiButtonRole::Secondary,
         [this, activateWorkflow]() {
             activateWorkflow("Command Palette");
             _commandPaletteOpen = true;
         }},
        {"Advanced/System", "Graph", "graph relationship backlink advanced system knowledge", UiButtonRole::Advanced,
         [this, activateWorkflow]() {
             activateWorkflow("Graph");
             _knowledgeGraphOpen = true;
         }},
    };

    ImGui::SetNextItemWidth(-1.0f);
    FormUi::InputTextWithHint(localizedUiLabel("##SidebarWorkflowFilter").c_str(),
                              localizedUiText("Find tool: map, audio, npc, shop, combat"), &_sidebarSearchQuery);
    showHelpTooltip("Filter labels and aliases, then open the matching tool directly from the result list.");
    renderSidebarSearchResults(_sidebarSearchQuery, sidebarEntries, _activeSidebarWorkflowLabel);

    renderSidebarRecommendedActions(campaign, saveState, sidebarEntries, _activeSidebarWorkflowLabel);
    renderSidebarGroup("Start", _sidebarHomeOpen, sidebarEntries, _activeSidebarWorkflowLabel);
    if (!_controller.shortcutStatusMessage().empty())
    {
        ImGui::Separator();
        ImGui::TextColored(kMutedText, localizedUiLabel("%s").c_str(), _controller.shortcutStatusMessage().c_str());
    }

    renderSidebarGroup("Run Table", _sidebarRunOpen, sidebarEntries, _activeSidebarWorkflowLabel);
    renderSidebarGroup("Prep Session", _sidebarPrepOpen, sidebarEntries, _activeSidebarWorkflowLabel);
    renderSidebarGroup("Create", _sidebarCreateOpen, sidebarEntries, _activeSidebarWorkflowLabel);
    renderSidebarGroup("Library", _sidebarManageOpen, sidebarEntries, _activeSidebarWorkflowLabel);
    renderSidebarGroup("Present", _sidebarPresentOpen, sidebarEntries, _activeSidebarWorkflowLabel);
