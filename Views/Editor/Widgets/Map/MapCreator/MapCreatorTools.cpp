#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawAuthoringToolbar()
{
    const bool editable = !_controller.generationBusy() && !_imageTraceActive;
    const bool editing = _workflowStep == 3;
    const bool undoShortcut = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z);
    const bool redoShortcut =
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y) | ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z);
    if (editing && editable && !ImGui::GetIO().WantTextInput && (undoShortcut || redoShortcut))
    {
        finishGeneratedFeatureGeometryEdit();
        _controller.endBrushStroke();
        _controller.finishPropertyEditUndo();
        if (redoShortcut)
            _controller.redoLastAction();
        else
            _controller.undoLastAction();
    }

    const bool showViewControls = _workflowStep != 4;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    float usedWidth = 0.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    // Measure each localized action, rather than making Undo as wide as the
    // longest German label. This keeps a useful canvas at 150% text scale.
    const auto placeAction = [&](const char *label, float minimumWidth = 0.0f) {
        const float width =
            std::min(availableWidth, std::max(minimumWidth, ImGui::CalcTextSize(localizedUiLabel(label).c_str()).x +
                                                                ImGui::GetStyle().FramePadding.x * 2.0f));
        if (gm::mapui::mapCreatorActionFitsRow(usedWidth, width, spacing, availableWidth))
        {
            ImGui::SameLine();
            usedWidth += spacing;
        }
        else
            usedWidth = 0.0f;
        usedWidth += width;
        return ImVec2(width, 0.0f);
    };
    ImVec2 actionSize;
    if (editing)
    {
        actionSize = placeAction("Undo");
        if (drawUiButton("Undo", editable && _controller._history.hasUndo, "Paint, erase, fog, or place something first.",
                         UiButtonRole::Secondary, actionSize, _controller._history.undoLabel.c_str()))
        {
            finishGeneratedFeatureGeometryEdit();
            _controller.endBrushStroke();
            _controller.finishPropertyEditUndo();
            _controller.undoLastAction();
        }
        showHelpTooltip("Ctrl+Z");
        actionSize = placeAction("Redo");
        if (drawUiButton("Redo", editable && _controller._history.hasRedo, "Undo a map edit before using redo.",
                         UiButtonRole::Secondary, actionSize, _controller._history.redoLabel.c_str()))
        {
            finishGeneratedFeatureGeometryEdit();
            _controller.endBrushStroke();
            _controller.finishPropertyEditUndo();
            _controller.redoLastAction();
        }
        showHelpTooltip("Ctrl+Y / Ctrl+Shift+Z");
    }
    if (editing || _controller.hasUnsavedDraft() || _controller._persistence.saveFailed)
    {
        const char *saveAction =
            _controller._persistence.saveFailed ? uiTextForLocale("Retry save", "Erneut speichern") : "Save Map";
        actionSize = placeAction(saveAction);
        if (drawUiButton(saveAction, editable, "Finish or cancel tracing before continuing.", UiButtonRole::Primary,
                         actionSize))
        {
            finishGeneratedFeatureGeometryEdit();
            _controller.endBrushStroke();
            _controller.finishPropertyEditUndo();
            _controller.saveDraftMap();
        }
    }
    if (editing)
    {
        actionSize = placeAction(uiTextForLocale("Map menu", "Kartenmenü"));
        if (drawUiButton(uiTextForLocale("Map menu", "Kartenmenü"), UiButtonRole::Secondary, actionSize))
            ImGui::OpenPopup("MapAuthoringOptions");

        const bool idle = editable && !_controller._editing.strokeInProgress && !_controller._history.propertyEditUndoActive &&
                          !hasPendingAuthoringInteraction() && !_terrainDragArmed && !_boxArmed &&
                          !_regenerationAreaDragging;
        if (_mapEditInspection.valid() &&
            _mapEditInspection.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            try
            {
                auto result = _mapEditInspection.get();
                if (!_discardInspectionResult && result.mapId == _controller._draftMap.metadata.id && result.serial == _controller._editing.authoringSerial)
                {
                    _mapEditInspectionError = result.error;
                    if (result.error.empty())
                    {
                        _mapEditConflicts = std::move(result.conflicts);
                        _mapEditConflictsMapId = result.mapId;
                        _mapEditConflictsSerial = result.serial;
                        _mapEditConflictsReady = true;
                    }
                    else _mapEditInspectionRetryAt = ImGui::GetTime() + 2;
                }
            }
            catch (const std::exception &error)
            {
                if (!_discardInspectionResult)
                {
                    _mapEditInspectionError = error.what();
                    _mapEditInspectionRetryAt = ImGui::GetTime() + 2;
                }
            }
        }
        const bool stale = !_mapEditConflictsReady || _mapEditConflictsMapId != _controller._draftMap.metadata.id ||
                           _mapEditConflictsSerial != _controller._editing.authoringSerial;
        if (idle && stale && !_mapEditInspection.valid() && ImGui::GetTime() >= _mapEditInspectionRetryAt)
        {
            const auto serial = _controller._editing.authoringSerial;
            _discardInspectionResult = false;
            try
            {
                _mapEditInspection = std::async(std::launch::async, [map = _controller._draftMap, serial] {
                    ConflictInspection result{map.metadata.id, serial};
                    try { result.conflicts = inspectMapEditConflicts(map); }
                    catch (const std::exception &error) { result.error = error.what(); }
                    catch (...) { result.error = "Map inspection failed."; }
                    return result;
                });
            }
            catch (const std::exception &error)
            {
                _mapEditInspectionError = error.what();
                _mapEditInspectionRetryAt = ImGui::GetTime() + 2;
            }
        }
        if (!_mapEditInspectionError.empty())
            ImGui::TextDisabled("%s", uiTextForLocale("Map issue check failed; retrying…", "Kartenprüfung fehlgeschlagen; neuer Versuch…"));
        if (_mapEditConflictsReady && _mapEditConflictsMapId == _controller._draftMap.metadata.id &&
            !_mapEditConflicts.empty())
        {
            const std::string issuesLabel = std::string(uiTextForLocale("Map issues", "Kartenprobleme")) + " (" +
                                            std::to_string(_mapEditConflicts.size()) + ")";
            actionSize = placeAction(issuesLabel.c_str());
            ImGui::BeginDisabled(!idle || stale);
            if (drawUiButton(issuesLabel.c_str(), UiButtonRole::Secondary, actionSize))
            {
                _mapEditConflictMessage.clear();
                ImGui::OpenPopup("MapEditConflicts");
            }
            ImGui::EndDisabled();
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(260, 0),
                                            ImVec2(std::max(260.f, ImGui::GetIO().DisplaySize.x - 32.f),
                                                   std::max(180.f, ImGui::GetIO().DisplaySize.y - 32.f)));
        if (ImGui::BeginPopup("MapEditConflicts"))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::min(480.f, ImGui::GetIO().DisplaySize.x - 64.f));
            ImGui::TextWrapped(
                "%s",
                uiTextForLocale(
                    "Review places or roads in open water and small isolated water patches. Small patches may be intentional ponds; remove only unwanted ones. Each repair can be undone.",
                    "Prüfe Orte oder Wege im offenen Wasser sowie kleine isolierte Wasserflächen. Kleine Flächen können beabsichtigte Teiche sein; entferne nur unerwünschte. Jede Reparatur lässt sich rückgängig machen."));
            ImGui::PopTextWrapPos();
            ImGui::Separator();
            ImGui::BeginChild("MapConflictList",
                              ImVec2(std::min(480.f, ImGui::GetIO().DisplaySize.x - 64.f),
                                     std::min(300.f, ImGui::GetIO().DisplaySize.y * .45f)),
                              false);
            bool closeIssues = false;
            for (const auto &issue : _mapEditConflicts)
            {
                ImGui::PushID(issue.featureId.c_str());
                const std::string name = issue.name.empty() ? uiTextForLocale("Unnamed object", "Unbenanntes Objekt")
                                                            : localizedGeneratedMapName(issue.name);
                if (issue.waterFragment)
                    ImGui::TextWrapped("%s · x=%d, y=%d", uiTextForLocale("Small water patch", "Kleine Wasserfläche"),
                                       static_cast<int>(std::floor(issue.point.x)),
                                       static_cast<int>(std::floor(issue.point.y)));
                else
                    ImGui::TextWrapped("%s · %s", name.c_str(),
                                       issue.place
                                           ? uiTextForLocale("Place in water", "Ort im Wasser")
                                           : uiTextForLocale("Road crosses open water", "Weg durch offenes Wasser"));
                if (issue.locked)
                    ImGui::TextDisabled("%s", uiTextForLocale("Locked · unlock before repairing",
                                                              "Gesperrt · vor der Reparatur entsperren"));
                ImGui::BeginDisabled(!idle || stale);
                if (ImGui::SmallButton(uiTextForLocale("Select", "Auswählen")))
                {
                    if (issue.waterFragment)
                    {
                        resetGeneratedFeatureGeometryInteraction();
                        _controller.clearObjectSelection();
                        _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
                        _controller._integration.statusMessage =
                            std::string(uiTextForLocale("Small water patch", "Kleine Wasserfläche")) +
                            " · x=" + std::to_string(static_cast<int>(std::floor(issue.point.x))) +
                            ", y=" + std::to_string(static_cast<int>(std::floor(issue.point.y))) + ". " +
                            uiTextForLocale("Keep it if this pond is intentional.",
                                            "Behalte sie, wenn dieser Teich beabsichtigt ist.");
                        closeIssues = true;
                    }
                    else
                    {
                        const auto &features = _controller._draftMap.generation.graph.features;
                        const auto found = std::find_if(features.begin(), features.end(), [&](const auto &feature) {
                            return feature.id == issue.featureId;
                        });
                        if (found != features.end())
                        {
                            resetGeneratedFeatureGeometryInteraction();
                            _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
                            _controller.selectGeneratedFeatureObject(static_cast<int>(found - features.begin()));
                            _objectEditMode = gm::mapui::MapObjectEditMode::Move;
                            _showCompactInspector = true;
                            closeIssues = true;
                        }
                    }
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(issue.locked);
                if (ImGui::SmallButton(issue.waterFragment ? uiTextForLocale("Remove patch", "Wasserfläche entfernen")
                                       : issue.place       ? uiTextForLocale("Move to land", "An Land verschieben")
                                                           : uiTextForLocale("Reroute on land", "Über Land umleiten")))
                {
                    _controller.finishPropertyEditUndo();
                    MapModel repaired = _controller._draftMap;
                    if (repairMapEditConflict(repaired, issue.featureId))
                    {
                        _controller.captureUndoSnapshot(issue.waterFragment ? "Remove Small Water Patch"
                                                        : issue.place       ? "Move Submerged Place to Land"
                                                                            : "Reroute Road on Land");
                        _controller._draftMap = std::move(repaired);
                        _controller.markDraftEdited();
                        resetGeneratedFeatureGeometryInteraction();
                        _controller.clearObjectSelection();
                        _controller._integration.statusMessage = uiTextForLocale(
                            "Map issue repaired. Use Undo to restore the previous state.",
                            "Kartenproblem behoben. Rückgängig stellt den vorherigen Zustand wieder her.");
                        closeIssues = true;
                    }
                    else
                    {
                        _mapEditConflictMessage =
                            issue.waterFragment
                                ? uiTextForLocale(
                                      "This water patch could not be removed; edit its terrain manually.",
                                      "Diese Wasserfläche konnte nicht entfernt werden; bearbeite ihr Gelände manuell.")
                            : issue.place
                                ? uiTextForLocale("No suitable land found; move the place manually.",
                                                  "Kein geeignetes Land gefunden; verschiebe den Ort manuell.")
                                : uiTextForLocale("No land route found; move endpoints first.",
                                                  "Kein Landweg gefunden; verschiebe zuerst die Endpunkte.");
                        _controller._integration.statusMessage = _mapEditConflictMessage;
                    }
                }
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                ImGui::Separator();
                ImGui::PopID();
                if (closeIssues)
                    break;
            }
            ImGui::EndChild();
            if (!_mapEditConflictMessage.empty())
                ImGui::TextWrapped("%s", _mapEditConflictMessage.c_str());
            if (closeIssues)
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    if (!showViewControls || editing)
        return;
    actionSize = placeAction("Fit map");
    if (drawUiButton("Fit map", UiButtonRole::Secondary, actionSize))
    {
        _fitCanvasOnNextDraw = true;
        _fitCanvasDelayFrames = 0;
        _playerPreviewZoom = 1.0f;
        _resetWorkflowScroll = true;
    }
    actionSize = placeAction("Zoom", std::max(120.0f, ImGui::GetFrameHeight() * 4.5f));
    ImGui::SetNextItemWidth(actionSize.x);
    if (_workflowStep == 5)
        FormUi::SliderFloat("##PlayerPreviewZoom", &_playerPreviewZoom, 1.0f, 4.0f, "%.1fx");
    else
        FormUi::SliderFloat("##MapAuthoringZoom", &_controller._editing.zoom,
                            mapCreatorMinimumZoom(_controller._draftMap.atlasLevel), 3.0f, "%.1fx");
    showHelpTooltip(
        uiTextForLocale("Mouse wheel: zoom · Middle button: pan · Fit map: show everything",
                        "Mausrad: zoomen · Mittlere Maustaste: verschieben · Karte einpassen: alles anzeigen"));
}
