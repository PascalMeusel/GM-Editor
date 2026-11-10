// Internal renderWindowSelector continuation: bookmarks, snapshot, and advanced tools.
    const std::vector<CampaignEntityBookmarkModel> favorites = _controller.favoriteItems();
    const std::vector<CampaignEntityBookmarkModel> recent = _controller.recentItems();
    ImGui::SetNextItemOpen(false, ImGuiCond_Once);
    if (ImGui::CollapsingHeader(localizedUiLabel("Favorites / Recent##SidebarFavoritesRecent").c_str()))
    {
        if (ImGui::BeginTable("LauncherFavoritesRecent", 2,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled(localizedUiLabel("Favorites").c_str());
            renderBookmarkLauncherList(_controller, favorites, "Favorite entities from Global Search appear here.",
                                       "Favorite: ", UiButtonRole::Primary, 3);
            ImGui::TableNextColumn();
            ImGui::TextDisabled(localizedUiLabel("Recent").c_str());
            renderBookmarkLauncherList(_controller, recent,
                                       "Opened entities appear here after Search or Command Palette use.",
                                       "Recent: ", UiButtonRole::Secondary, 4);
            ImGui::EndTable();
        }
    }

    renderSidebarGroupButton("Campaign Snapshot", _sidebarSnapshotOpen, false);
    if (_sidebarSnapshotOpen)
    {
        const SidebarSubmenuFrame submenuFrame = beginSidebarSubmenuFrame("Campaign Snapshot", false);
        if (ImGui::BeginTable("CampaignEntityCounts", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            renderLabeledCount("Players", static_cast<int>(campaign.characters.size()));
            ImGui::TableNextColumn();
            renderLabeledCount("NPCs", static_cast<int>(campaign.npcs.size()));
            ImGui::TableNextColumn();
            renderLabeledCount("Custom Items", static_cast<int>(campaign.customItems.size()));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            renderLabeledCount("Spells", static_cast<int>(campaign.customSpells.size()));
            ImGui::TableNextColumn();
            renderLabeledCount("Shops", static_cast<int>(campaign.shops.size()));
            ImGui::TableNextColumn();
            renderLabeledCount("Maps", static_cast<int>(campaign.maps.size()));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            renderLabeledCount("Encounters", static_cast<int>(campaign.encounters.size()));
            ImGui::TableNextColumn();
            renderLabeledCount("Story Arcs", static_cast<int>(campaign.storyThreads.size()));
            ImGui::TableNextColumn();
            renderLabeledCount("Audio", static_cast<int>(campaign.audioAssets.size()));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            renderLabeledCount("Indexed Entities", static_cast<int>(EntityRegistry::getInstance().size()), kWarmAccent);
            ImGui::EndTable();
        }
        endSidebarSubmenuFrame(submenuFrame);
    }

    const EditorLayoutProfileStatus &layoutStatus = EditorLayoutProfileService::getInstance().status();
    const std::vector<ProjectLogEntry> problems = ProjectLogger::getInstance().recentProblems(5);
    const bool layoutNeedsAttention = !layoutStatus.lastError.empty();
    const bool saveNeedsAttention = _controller.hasUnsavedChanges() || !saveState.lastError.empty() ||
                                    !saveState.lastSaveWarnings.empty() || !saveState.lastRecoveryError.empty() ||
                                    !saveState.lastBackupError.empty();
    const bool systemNeedsAttention = layoutNeedsAttention || saveNeedsAttention || !problems.empty();
    if (systemNeedsAttention)
        _sidebarAdvancedOpen = true;
    const bool advancedContainsActive =
        groupContainsActiveWorkflow(sidebarEntries, "Advanced/System", _activeSidebarWorkflowLabel);
    renderSidebarGroupButton("Advanced/System", _sidebarAdvancedOpen, advancedContainsActive);
    if (_sidebarAdvancedOpen)
    {
        const SidebarSubmenuFrame submenuFrame =
            beginSidebarSubmenuFrame("Advanced/System", advancedContainsActive || systemNeedsAttention);
        const std::string systemMode = localizedUiLabel(systemNeedsAttention ? "Needs review" : "System quiet");
        const std::string saveStatus =
            !_controller.hasUnsavedChanges() && saveState.lastError.empty() && !saveState.lastSaveWarnings.empty()
                ? saveUiText("Saved with warnings", "Gespeichert mit Warnungen")
                : localizedUiLabel(_controller.hasUnsavedChanges() ? "Unsaved" : (!saveState.lastError.empty() ? "Error" : "Saved"));
        const std::string layoutStatusLabel =
            localizedUiLabel(layoutNeedsAttention ? "Error" : (layoutStatus.profileLoaded ? "Loaded" : "New"));
        const std::string systemSelection = localizedUiLabel("Save state") + ": " + saveStatus + " | " +
                                            localizedUiLabel("Layout") + ": " + layoutStatusLabel + " | " +
                                            localizedUiLabel("Warnings") + ": " + std::to_string(problems.size());
        const std::string systemNext = localizedUiLabel(systemNeedsAttention ? "Open the affected drawer below."
                                                                             : "Use advanced tools only when needed.");
        UiStatusActionStrip systemStrip;
        systemStrip.mode = systemMode.c_str();
        systemStrip.selection = systemSelection.c_str();
        systemStrip.nextAction = systemNext.c_str();
        systemStrip.help =
            "System / Advanced keeps layout profiles, save/recovery state, and warning history out of the normal GM task flow.";
        drawStatusActionStrip("WindowSelectorSystemStrip", systemStrip);
        ImGui::TextColored(
            kWarmAccent,
            localizedUiLabel(
                "Advanced/System tools can affect project files, layout profiles, saved campaign data, diagnostics, or audio asset repair.")
                .c_str());
        ImGui::TextWrapped(
            localizedUiLabel(
                "Review warnings and backups here before changing storage, recovery, layout, or diagnostic state.")
                .c_str());

        for (const SidebarNavEntry &entry : sidebarEntries)
        {
            if (std::string(entry.group) == "Advanced/System")
                renderSidebarEntryButton(entry, _activeSidebarWorkflowLabel);
        }

        ImGui::SetNextItemOpen(layoutNeedsAttention, ImGuiCond_Once);
        if (ImGui::CollapsingHeader(localizedUiLabel("Layout Profile##SystemLayoutProfile").c_str()))
        {
            ImGui::TextColored(kMutedText, localizedUiLabel("Mode: %s").c_str(),
                               EditorLayoutProfileService::modeLabel(layoutStatus.mode));
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                               localizedUiText(layoutStatus.profileLoaded ? "Persistent campaign layout loaded."
                                                                          : "New campaign layout profile."));
            if (!layoutStatus.profilePath.empty())
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), layoutStatus.profilePath.generic_string().c_str());
            if (!layoutStatus.lastError.empty())
                ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.38f, 1.0f), localizedUiLabel("Layout error: %s").c_str(),
                                   layoutStatus.lastError.c_str());
            else if (!layoutStatus.lastMessage.empty())
                ImGui::TextColored(kMutedText, localizedUiLabel("%s").c_str(),
                                   localizedUiText(layoutStatus.lastMessage.c_str()));
            if (drawUiButton("Save Layout Now", UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f),
                             "Persist the current dock layout for this campaign."))
                EditorLayoutProfileService::getInstance().saveCurrentProfile();
        }

        ImGui::SetNextItemOpen(saveNeedsAttention, ImGuiCond_Once);
        if (ImGui::CollapsingHeader(localizedUiLabel("Save, Backup, And Recovery##SystemSaveState").c_str()))
        {
            bool autosaveEnabled = saveConfig.autosaveEnabled;
            if (FormUi::Checkbox(localizedUiLabel("Autosave").c_str(), &autosaveEnabled))
                saveService.setAutosaveEnabled(autosaveEnabled);

            int autosaveInterval = saveConfig.autosaveIntervalSeconds;
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputInt(localizedUiLabel("Interval (seconds)").c_str(), &autosaveInterval, 10, 60))
                saveService.setAutosaveIntervalSeconds(autosaveInterval);

            if (drawUiButton("Save Now", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                             "Save the current campaign immediately."))
                _controller.saveCurrentCampaign("manual save");

            if (saveState.currentlySaving)
            {
                ImGui::TextColored(kWarmAccent, localizedUiLabel("Saving...").c_str());
            }
            else if (!saveState.lastError.empty())
            {
                ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.38f, 1.0f), localizedUiLabel("Save error: %s").c_str(),
                                   saveState.lastError.c_str());
            }
            else if (saveState.dirty)
            {
                ImGui::TextColored(kWarmAccent, localizedUiLabel("Unsaved changes").c_str());
                renderSaveChangeSummary("Changed since last save:", saveState.pendingChangeSummary);
            }
            else if (!saveState.lastSavedAtUtc.empty())
            {
                ImGui::TextColored(kMutedText, localizedUiLabel("Last saved: %s").c_str(),
                                   saveState.lastSavedAtUtc.c_str());
                if (!saveState.lastAutosavedAtUtc.empty())
                    ImGui::TextColored(kMutedText, localizedUiLabel("Last autosave: %s").c_str(),
                                       saveState.lastAutosavedAtUtc.c_str());
                renderSaveChangeSummary("Saved changes:", saveState.lastSaveChangeSummary);
            }
            else
            {
                ImGui::TextColored(kMutedText, localizedUiLabel("No save activity yet").c_str());
            }

            if (!saveState.lastSaveWarnings.empty())
            {
                ImGui::TextColored(
                    kWarmAccent, "%s",
                    saveState.lastError.empty()
                        ? saveUiText("The database save succeeded. Follow-up work needs attention:",
                                     "Die Datenbank wurde gespeichert. Nacharbeiten erfordern Aufmerksamkeit:")
                        : saveUiText("Additional save warnings:", "Zusätzliche Speicherwarnungen:"));
                for (const auto &warning : saveState.lastSaveWarnings)
                    ImGui::TextWrapped("%s", warning.c_str());
            }
            if (!saveState.lastRecoveryError.empty())
                ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.38f, 1.0f),
                                   localizedUiLabel("Recovery snapshot error: %s").c_str(),
                                   saveState.lastRecoveryError.c_str());
            else if (!saveState.lastRecoverySnapshotAtUtc.empty())
                ImGui::TextColored(kMutedText, localizedUiLabel("Recovery snapshot: %s").c_str(),
                                   saveState.lastRecoverySnapshotAtUtc.c_str());
            if (!saveState.lastBackupError.empty())
                ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.38f, 1.0f), localizedUiLabel("Backup error: %s").c_str(),
                                   saveState.lastBackupError.c_str());
            else if (!saveState.lastBackupAtUtc.empty())
                ImGui::TextColored(kMutedText, localizedUiLabel("Pre-save backup: %s").c_str(),
                                   saveState.lastBackupAtUtc.c_str());
        }

        const std::vector<ProjectEvent> &events = ProjectEventBus::getInstance().history();
        ImGui::SetNextItemOpen(!problems.empty(), ImGuiCond_Once);
        if (ImGui::CollapsingHeader(localizedUiLabel("Recent Events And Warnings##SystemEvents").c_str()))
        {
            if (!events.empty())
            {
                const ProjectEvent &latestEvent = events.back();
                ImGui::TextColored(kMutedText, localizedUiLabel("Latest event: %s").c_str(),
                                   localizedUiText(projectEventTypeLabel(latestEvent.type)));
            }
            else
            {
                ImGui::TextColored(kMutedText, localizedUiLabel("No project events yet").c_str());
            }

            if (problems.empty())
            {
                ImGui::TextColored(kMutedText, localizedUiLabel("No warnings or errors").c_str());
            }
            else
            {
                for (const ProjectLogEntry &entry : problems)
                {
                    const ImVec4 color =
                        entry.level == ProjectLogLevel::Error ? ImVec4(0.95f, 0.42f, 0.38f, 1.0f) : kWarmAccent;
                    ImGui::TextColored(color, localizedUiLabel("%s - %s").c_str(),
                                       localizedUiText(projectLogLevelLabel(entry.level)), entry.source.c_str());
                    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUiText(entry.message.c_str()));
                }
            }
        }
        endSidebarSubmenuFrame(submenuFrame);
    }

    ImGui::End();
