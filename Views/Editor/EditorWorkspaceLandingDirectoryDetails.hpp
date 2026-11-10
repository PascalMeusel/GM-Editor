// Internal renderWorkspaceLanding continuation: directory list, selection, and contextual details.
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_NoSavedSettings))
        {
            const float directoryWidth = std::clamp(availableWidth * 0.36f, 260.0f, 320.0f);
            ImGui::TableSetupColumn(uiText("Directory", "Verzeichnis"), ImGuiTableColumnFlags_WidthFixed,
                                    directoryWidth);
            ImGui::TableSetupColumn(uiText("Inspector", "Inspektor"), ImGuiTableColumnFlags_WidthStretch);
            if (showContextRail)
                ImGui::TableSetupColumn(uiText("Context", "Kontext"), ImGuiTableColumnFlags_WidthFixed, 260.0f);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::BeginChild("WorkspaceDirectoryList", ImVec2(0.0f, -1.0f), ImGuiChildFlags_None);
            if (visibleEntries.empty())
            {
                if (directoryEntries.empty())
                {
                    const char *emptyTitle = uiText("Nothing here yet", "Noch keine Einträge");
                    const char *emptyDescription =
                        uiText("Use the New action above to create the first entry in this workspace.",
                               "Erstelle über »Neu« den ersten Eintrag in diesem Arbeitsbereich.");
                    WorkspaceUi::emptyState(emptyTitle, emptyDescription);
                }
                else
                {
                    WorkspaceUi::emptyState(uiText("No matches", "Keine Treffer"),
                                            uiText("Try a name, entity type, or descriptive word.",
                                                   "Versuche einen Namen, Entitätstyp oder ein beschreibendes Wort."));
                }
            }
            for (const WorkspaceEntityEntry *entry : visibleEntries)
            {
                ImGui::PushID(entry->id.c_str());
                const bool selected = _workspaceSelectionId == entry->id;
                const std::string typeLine = std::string(localizedUiText(entry->type.c_str())) +
                                             (entry->archived ? uiText(" · Archived", " · Archiviert") : "");
                const std::string directoryLabel = entry->name + "\n" + typeLine;
                const float rowHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f + 8.0f;
                if (ImGui::Selectable(directoryLabel.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick,
                                      ImVec2(0.0f, rowHeight)))
                {
                    _workspaceSelectionId = entry->id;
                    _workspaceArchiveStatus.clear();
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && entry->open)
                        entry->open();
                }
                ImGui::SetItemTooltip("%s", entry->summary.c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);
            const auto selected =
                std::find_if(directoryEntries.begin(), directoryEntries.end(),
                             [&](const WorkspaceEntityEntry &entry) { return entry.id == _workspaceSelectionId; });
            if (selected == directoryEntries.end())
            {
                WorkspaceUi::emptyState(
                    uiText("Select an entry", "Eintrag auswählen"),
                    uiText("Choose something from the directory to inspect it without leaving the workspace.",
                           "Wähle einen Eintrag aus dem Verzeichnis, um ihn im Arbeitsbereich zu prüfen."));
            }
            else
            {
                ImGui::TextColored(WorkspaceUi::Amber, localizedUiLabel("%s").c_str(),
                                   localizedUiText(selected->type.c_str()));
                if (selected->archived)
                {
                    ImGui::SameLine();
                    ImGui::TextColored(WorkspaceUi::Muted, localizedUiLabel("%s").c_str(),
                                       uiText("Archived", "Archiviert"));
                }
                ImGui::SetWindowFontScale(1.22f);
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), selected->name.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, WorkspaceUi::Muted);
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), selected->summary.c_str());
                ImGui::PopStyleColor();
                ImGui::Dummy(ImVec2(0.0f, 12.0f));
                const bool embeddedWorldShop = _activeWorkspace == EditorWorkspace::World &&
                                               WorldInspector::findShop(campaign, selected->id) != nullptr;
                const bool embeddedWorldDungeon = _activeWorkspace == EditorWorkspace::World &&
                                                  WorldInspector::findDungeon(campaign, selected->id) != nullptr;
                const bool embeddedWorldNation = _activeWorkspace == EditorWorkspace::World &&
                                                 WorldInspector::findNation(campaign, selected->id) != nullptr;
                const bool embeddedWorldPlace = _activeWorkspace == EditorWorkspace::World &&
                                                WorldInspector::findWorldPlace(campaign, selected->id) != nullptr;
                const bool embeddedRandomTable = _activeWorkspace == EditorWorkspace::Compendium &&
                    std::any_of(campaign.randomTables.begin(), campaign.randomTables.end(), [&](const auto &table) {
                        return table.metadata.id == selected->id;
                    });
                renderContextLinks(selected->id);
                if (embeddedWorldShop)
                {
                    renderWorldShopInspector(selected->id);
                }
                else if (embeddedWorldDungeon)
                {
                    renderWorldDungeonInspector(selected->id);
                }
                else if (embeddedWorldNation)
                {
                    renderWorldNationInspector(selected->id);
                }
                else if (embeddedWorldPlace)
                {
                    renderWorldPlaceInspector(selected->id);
                }
                else if (embeddedRandomTable)
                {
                    renderRandomTableInspector(selected->id);
                }
                else
                {
                    const char *nextStep = uiText(
                        "Open the full editor to change this entry. Your selection stays in this workspace when you return.",
                        "Öffne den vollständigen Editor, um diesen Eintrag zu ändern. Die Auswahl bleibt bei der Rückkehr erhalten.");
                    if (_activeWorkspace == EditorWorkspace::Story)
                        nextStep = uiText("Edit the scene, then stage it for Live when it is ready for the table.",
                                          "Bearbeite die Szene und stelle sie anschließend für Live bereit.");
                    else if (_activeWorkspace == EditorWorkspace::Cast)
                        nextStep =
                            uiText("Open the sheet to edit profile, rules, inventory, magic, and story details.",
                                   "Öffne den Bogen, um Profil, Regeln, Inventar, Magie und Geschichte zu bearbeiten.");
                    else if (_activeWorkspace == EditorWorkspace::Encounters)
                        nextStep = uiText(
                            "Open preparation to review participants and readiness, then stage the encounter for Live Combat.",
                            "Öffne die Vorbereitung, prüfe Beteiligte und Bereitschaft und stelle die Begegnung dann für Live-Kampf bereit.");
                    else if (_activeWorkspace == EditorWorkspace::Compendium)
                        nextStep = uiText(
                            "Open the detail editor to review rules text, ownership, import state, or audio metadata.",
                            "Öffne den Detail-Editor für Regeltext, Besitz, Importstatus oder Audiometadaten.");

                    const float actionAreaWidth = ImGui::GetContentRegionAvail().x;
                    float usedActionWidth = 0.0f;
                    const auto actionWidth = [&](const char *label, float preferred) {
                        const float labelWidth =
                            ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f + 14.0f;
                        return std::min(actionAreaWidth, std::max(preferred, labelWidth));
                    };
                    const auto placeAction = [&](float width) {
                        const float spacing = ImGui::GetStyle().ItemSpacing.x;
                        if (usedActionWidth > 0.0f && usedActionWidth + spacing + width <= actionAreaWidth)
                        {
                            ImGui::SameLine();
                            usedActionWidth += spacing + width;
                        }
                        else
                        {
                            usedActionWidth = width;
                        }
                    };

                    const char *editLabel = uiText("Edit details", "Details bearbeiten");
                    const float editWidth = actionWidth(editLabel, 180.0f);
                    placeAction(editWidth);
                    ImGui::BeginDisabled(!selected->open);
                    if (drawUiButton(editLabel, UiButtonRole::Primary, ImVec2(editWidth, 0.0f), nextStep) &&
                        selected->open)
                        selected->open();
                    ImGui::EndDisabled();
                    const bool selectedWorldMap = _activeWorkspace == EditorWorkspace::World &&
                        std::any_of(campaign.maps.begin(), campaign.maps.end(), [&](const MapModel &map) {
                            return map.metadata.id == selected->id;
                        });
                    if (selectedWorldMap)
                    {
                        const char *atlasLabel = uiText("Open in Atlas", "Im Atlas öffnen");
                        const float atlasWidth = actionWidth(atlasLabel, 150.0f);
                        placeAction(atlasWidth);
                        if (drawUiButton(atlasLabel, UiButtonRole::Secondary, ImVec2(atlasWidth, 0.0f),
                                         uiText("View this map in its world-to-detail hierarchy.",
                                                "Zeige diese Karte in der Hierarchie von Welt bis Detail.")))
                            _controller.openWorkspaceEntity("Map", selected->id);
                    }
                    if (!selected->archiveType.empty())
                    {
                        const char *archiveLabel = selected->archived ? uiText("Restore", "Wiederherstellen")
                                                                      : uiText("Archive", "Archivieren");
                        const UiButtonRole archiveRole =
                            selected->archived ? UiButtonRole::Secondary : UiButtonRole::Advanced;
                        const float archiveWidth = actionWidth(archiveLabel, 150.0f);
                        placeAction(archiveWidth);
                        if (drawUiButton(archiveLabel, archiveRole, ImVec2(archiveWidth, 0.0f)))
                        {
                            _pendingWorkspaceArchiveId = selected->id;
                            _pendingWorkspaceArchiveType = selected->archiveType;
                            _pendingWorkspaceArchiveName = selected->name;
                            _pendingWorkspaceArchiveState = !selected->archived;
                            ImGui::OpenPopup(archivePopupTitle);
                        }
                    }
                    if (_activeWorkspace == EditorWorkspace::Story && selected->type == "Scene")
                    {
                        const char *stageLabel = uiText("Stage for Live", "Für Live bereitstellen");
                        const float stageWidth = actionWidth(stageLabel, 180.0f);
                        placeAction(stageWidth);
                        if (drawUiButton(stageLabel, UiButtonRole::Secondary, ImVec2(stageWidth, 0.0f)))
                            _controller.stageSceneForLive(selected->id);
                    }
                    else if (_activeWorkspace == EditorWorkspace::Encounters)
                    {
                        const char *stageLabel = uiText("Stage in Live Combat", "Für Live-Kampf bereitstellen");
                        const float stageWidth = actionWidth(stageLabel, 210.0f);
                        placeAction(stageWidth);
                        if (drawUiButton(stageLabel, UiButtonRole::Secondary, ImVec2(stageWidth, 0.0f)))
                            _controller.stageEncounterForLive(selected->id);
                    }
                }
                if (!_workspaceArchiveStatus.empty())
                {
                    ImGui::Dummy(ImVec2(0.0f, 8.0f));
                    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _workspaceArchiveStatus.c_str());
                }
            }

            if (ImGui::BeginPopupModal(archivePopupTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                if (_pendingWorkspaceArchiveState)
                {
                    ImGui::TextWrapped(
                        localizedUiLabel("%s").c_str(),
                        uiText(
                            "Archive this entry? It remains in the campaign and all references are preserved.",
                            "Diesen Eintrag archivieren? Er bleibt in der Kampagne und alle Verweise bleiben erhalten."));
                }
                else
                {
                    ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                                       uiText("Restore this entry to active campaign use?",
                                              "Diesen Eintrag für die aktive Kampagnennutzung wiederherstellen?"));
                }
                ImGui::Spacing();
                ImGui::TextColored(WorkspaceUi::Text, localizedUiLabel("%s").c_str(),
                                   _pendingWorkspaceArchiveName.c_str());
                const char *confirmLabel = _pendingWorkspaceArchiveState ? uiText("Archive", "Archivieren")
                                                                         : uiText("Restore", "Wiederherstellen");
                const UiButtonRole confirmRole =
                    _pendingWorkspaceArchiveState ? UiButtonRole::Advanced : UiButtonRole::Primary;
                if (drawModalActionButtons(confirmLabel, confirmRole, uiText("Cancel", "Abbrechen"), 140.0f))
                {
                    const bool changed = _controller.setWorkspaceEntityArchived(
                        _pendingWorkspaceArchiveType, _pendingWorkspaceArchiveId, _pendingWorkspaceArchiveState);
                    if (changed)
                    {
                        _workspaceArchiveStatus =
                            _pendingWorkspaceArchiveState
                                ? uiText("Entry archived. References and persisted data were preserved.",
                                         "Eintrag archiviert. Verweise und gespeicherte Daten wurden erhalten.")
                                : uiText("Entry restored to active campaign use.",
                                         "Eintrag für die aktive Kampagnennutzung wiederhergestellt.");
                    }
                    else
                    {
                        _workspaceArchiveStatus = uiText(
                            "The archive change could not be saved. No in-memory state was changed.",
                            "Die Archivänderung konnte nicht gespeichert werden. Der Zustand im Arbeitsspeicher blieb unverändert.");
                    }
                }
                ImGui::EndPopup();
            }
            if (showContextRail)
            {
                ImGui::TableSetColumnIndex(2);
                gm::approved_ui::renderWorkspaceContextRail(
                    _activeWorkspace, campaign,
                    selected == directoryEntries.end() ? std::string{} : selected->name,
                    selected == directoryEntries.end() ? std::string{} : selected->type);
            }
            ImGui::EndTable();
        }
        ImGui::End();
        return;
    }
    const auto beginCards = [&]() {
        return ImGui::BeginTable("WorkspaceActions", columnCount,
                                 ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings);
    };
    const auto nextCard = [&]() {
        ImGui::TableNextColumn();
        return ImVec2(-1.0f, 148.0f);
    };
