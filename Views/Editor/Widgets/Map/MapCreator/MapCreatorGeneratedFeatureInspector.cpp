#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawSelectedGeneratedFeatureInspector() {     MapModel &map = _controller._draftMap;
        MapGeneratedFeature &feature =
            map.generation.graph.features[static_cast<std::size_t>(_controller._editing.selectedGeneratedFeatureIndex)];
        const std::string typeLabel = localizedUiLabel(generatedFeatureKindLabel(feature.kind));
        ImGui::TextUnformatted(typeLabel.c_str());
        const bool objectLocked = mapGeneratedFeatureIsLocked(map, feature);
        if (objectLocked)
        {
            if (drawUiButton(uiTextForLocale("Unlock to edit", "Zum Bearbeiten entsperren"), UiButtonRole::Primary,
                             ImVec2(-1, 0)))
            {
                finishGeneratedFeatureGeometryEdit();
                _controller.setGeneratedFeatureLocked(feature.id, false);
                _controller.markDraftEdited();
            }
        }
        else
        {
            const float actionWidth =
                std::max(40.0f, (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * .5f);
            if (gm::mapui::mapObjectCanMove(feature) &&
                drawUiButton(uiTextForLocale("Move", "Verschieben"),
                             _objectEditMode == gm::mapui::MapObjectEditMode::Move ? UiButtonRole::Primary
                                                                                   : UiButtonRole::Secondary,
                             ImVec2(gm::mapui::mapObjectCanReshape(feature) ? actionWidth : -1, 0)))
            {
                resetGeneratedFeatureGeometryInteraction();
                _objectEditMode = gm::mapui::MapObjectEditMode::Move;
            }
            if (gm::mapui::mapObjectCanReshape(feature))
            {
                ImGui::SameLine();
                if (drawUiButton(uiTextForLocale("Reshape", "Umformen"),
                                 _objectEditMode == gm::mapui::MapObjectEditMode::Reshape ? UiButtonRole::Primary
                                                                                          : UiButtonRole::Secondary,
                                 ImVec2(actionWidth, 0)))
                {
                    resetGeneratedFeatureGeometryInteraction();
                    _objectEditMode = gm::mapui::MapObjectEditMode::Reshape;
                }
            }
            if (gm::mapui::mapObjectCanMove(feature))
                ImGui::TextWrapped(
                    "%s",
                    _objectEditMode == gm::mapui::MapObjectEditMode::Move
                        ? uiTextForLocale("Drag the object on the map. Its shape stays intact.",
                                          "Ziehe das Objekt auf der Karte. Seine Form bleibt erhalten.")
                        : uiTextForLocale(
                              "Drag a handle to change the route. Double-click the line to add a handle.",
                              "Ziehe einen Punkt, um den Verlauf zu ändern. Doppelklick auf die Linie fügt einen Punkt hinzu."));
        }
        // Reserve actual translated label space instead of ImGui's default 65%
        // field width, which clips these labels with enlarged application text.
        if (!_drawingCompactInspector)
        {
            float labelWidth = 0.0f;
            for (const char *label : {"Feature name", "Classification", "Display profile", "Width"})
                labelWidth = std::max(labelWidth, ImGui::CalcTextSize(localizedUiLabel(label).c_str()).x);
            ImGui::PushItemWidth(std::max(ImGui::GetFontSize() * 4.0f, ImGui::GetContentRegionAvail().x - labelWidth -
                                                                           ImGui::GetStyle().ItemInnerSpacing.x));
        }

        if (mapGeneratedFeatureIsLocked(map, feature))
        {
            drawInlineLabelValue("Feature name", feature.name.c_str());
            const std::vector<MapGeneratedFeatureAppearanceOption> lockedAppearanceOptions =
                mapGeneratedFeatureAppearanceOptions(feature);
            const int lockedAppearance = mapGeneratedFeatureAppearanceOptionIndex(feature);
            const char *lockedAppearancePreview =
                lockedAppearance >= 0 ? lockedAppearanceOptions[static_cast<std::size_t>(lockedAppearance)].label
                                      : "Current map style";
            drawInlineLabelValue("Classification", typeLabel.c_str());
            drawInlineLabelValue("Display profile", localizedUiLabel(lockedAppearancePreview).c_str());
            ImGui::TextColored(ImVec4(0.94f, 0.73f, 0.34f, 1.0f), "%s", localizedUiLabel("Locked").c_str());
            ImGui::TextWrapped(
                "%s",
                localizedUiLabel(
                    feature.lineage.sourceMapId.empty()
                        ? "This feature is locked. Unlock it before changing its geometry."
                        : "This inherited atlas feature is locked. Detach or unlock it before changing its geometry.")
                    .c_str());
        }
        else
        {
            undoableInputText("Feature name", feature.name, "Edit Generated Feature");

            const std::vector<MapGeneratedFeatureClassificationOption> classifications =
                mapGeneratedFeatureClassificationOptions(feature.kind);
            if (classifications.size() > 1u)
            {
                const int currentClassification = mapGeneratedFeatureClassificationOptionIndex(feature);
                const char *previewLabel =
                    classifications[static_cast<std::size_t>(std::clamp(currentClassification, 0,
                                                                        static_cast<int>(classifications.size()) - 1))]
                        .label;
                if (FormUi::BeginCombo(localizedUiLabel("Type").c_str(), localizedUiLabel(previewLabel).c_str()))
                {
                    for (int optionIndex = 0; optionIndex < static_cast<int>(classifications.size()); ++optionIndex)
                    {
                        const MapGeneratedFeatureClassificationOption &option =
                            classifications[static_cast<std::size_t>(optionIndex)];
                        const bool selected = option.kind == feature.kind;
                        if (ImGui::Selectable(localizedUiLabel(option.label).c_str(), selected) && !selected)
                        {
                            _controller.finishPropertyEditUndo();
                            _controller.captureUndoSnapshot("Change Generated Feature Classification");
                            if (applyGeneratedMapFeatureClassification(map, _controller._editing.selectedGeneratedFeatureIndex,
                                                                       option.kind))
                                _controller._integration.statusMessage =
                                    gm::localization::trUi("The feature classification was updated.");
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            else
            {
                drawInlineLabelValue("Classification", typeLabel.c_str());
            }

            const std::vector<MapGeneratedFeatureAppearanceOption> appearanceOptions =
                mapGeneratedFeatureAppearanceOptions(feature);
            const int currentAppearance = mapGeneratedFeatureAppearanceOptionIndex(feature);
            const char *appearancePreview = currentAppearance >= 0
                                                ? appearanceOptions[static_cast<std::size_t>(currentAppearance)].label
                                                : "Current map style";
            if (appearanceOptions.size() > 1u || currentAppearance < 0)
            {
                if (FormUi::BeginCombo(localizedUiLabel("Symbol").c_str(), localizedUiLabel(appearancePreview).c_str()))
                {
                    for (int optionIndex = 0; optionIndex < static_cast<int>(appearanceOptions.size()); ++optionIndex)
                    {
                        const MapGeneratedFeatureAppearanceOption &option =
                            appearanceOptions[static_cast<std::size_t>(optionIndex)];
                        const bool selected = optionIndex == currentAppearance;
                        if (ImGui::Selectable(localizedUiLabel(option.label).c_str(), selected) && !selected)
                        {
                            _controller.finishPropertyEditUndo();
                            _controller.captureUndoSnapshot("Change Generated Feature Profile");
                            if (applyGeneratedMapFeatureAppearance(map, _controller._editing.selectedGeneratedFeatureIndex,
                                                                   option.materialId))
                                _controller._integration.statusMessage =
                                    gm::localization::trUi("The feature display profile was updated.");
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            else
            {
                drawInlineLabelValue("Display profile", localizedUiLabel(appearancePreview).c_str());
            }

            if (feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail ||
                feature.kind == MapGeneratedFeatureKind::River)
            {
                const float maximumWidth = feature.kind == MapGeneratedFeatureKind::River ? 2.4f : 1.5f;
                undoableSliderFloat("Width", feature.width, 0.35f, maximumWidth, "%.2f", "Edit Generated Feature");
            }
            undoableCheckbox("Discovered", feature.discovered, "Edit Generated Feature");
            ImGui::SameLine();
            undoableCheckbox("GM Only", feature.gmOnly, "Edit Generated Feature");

            if (_objectEditMode == gm::mapui::MapObjectEditMode::Reshape && gm::mapui::mapObjectCanReshape(feature))
            {
                ImGui::SeparatorText(localizedUiLabel("Control points").c_str());
                ImGui::TextWrapped(
                    "%s",
                    localizedUiLabel(
                        "Click a point on the map and drag it to reshape the feature. Double-click a segment to add a point.")
                        .c_str());
                if (_selectedGeneratedFeaturePoint >= 0 &&
                    _selectedGeneratedFeaturePoint < static_cast<int>(feature.points.size()))
                {
                    ImGui::Text(
                        "%s",
                        formatLocalizedUi("Selected control point: %d", _selectedGeneratedFeaturePoint + 1).c_str());
                }
                else
                {
                    ImGui::TextDisabled("%s", localizedUiLabel("No control point selected.").c_str());
                }

                const std::size_t minimumPoints = feature.closed ? 3u : 2u;
                const bool pointSelected = _selectedGeneratedFeaturePoint >= 0 &&
                                           _selectedGeneratedFeaturePoint < static_cast<int>(feature.points.size());
                const bool canRemovePoint = pointSelected && feature.points.size() > minimumPoints;
                const char *disabledReason =
                    !pointSelected ? "Select a control point on the map first."
                                   : (feature.closed ? "A closed area needs at least three control points."
                                                     : "A line needs at least two control points.");
                const bool removeFromButton =
                    drawUiButton("Remove control point", canRemovePoint, disabledReason, UiButtonRole::Destructive,
                                 ImVec2(-1.0f, 0.0f), "Remove the selected point without deleting the feature.");
                const bool removeFromKeyboard =
                    canRemovePoint && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false);
                if (removeFromButton || removeFromKeyboard)
                    removeSelectedGeneratedFeaturePoint();
            }
            if (ImGui::TreeNode(uiTextForLocale("Protection", "Schutz")))
            {
                ImGui::TextWrapped(
                    "%s", uiTextForLocale("Lock this object to protect it from accidental edits and regeneration.",
                                          "Sperre dieses Objekt gegen versehentliche Änderungen und Neuerzeugung."));
                if (drawUiButton(uiTextForLocale("Lock object", "Objekt sperren"), UiButtonRole::Secondary,
                                 ImVec2(-1, 0)))
                {
                    finishGeneratedFeatureGeometryEdit();
                    _controller.setGeneratedFeatureLocked(feature.id, true);
                    _controller.markDraftEdited();
                }
                ImGui::TreePop();
            }
        }
        if (!_drawingCompactInspector)
            ImGui::PopItemWidth();
        if (map.atlasLevel == MapLevel::World &&
            (feature.kind == MapGeneratedFeatureKind::Settlement || feature.kind == MapGeneratedFeatureKind::Landmark))
        {
            ImGui::PushID("SelectedPlaceSharing");
            // Sharing details are important during publication, but should not
            // displace the place's common editing controls on selection.
            const bool showSharing = ImGui::TreeNode(
                uiTextForLocale("Campaign link and player visibility", "Kampagnenlink und Spielersichtbarkeit"));
            if (showSharing)
            {
                // The inspector only needs sharing. Do not compile/filter a whole
                // illustrated map on every frame merely to explain its consent.
                MapModel sharingDraft;
                sharingDraft.metadata.id = map.metadata.id;
                sharingDraft.visibility = map.visibility;
                sharingDraft.playerRelease = map.playerRelease;
                const MapModel playerMap = makeMapCreatorPlayerOutputSnapshot(
                    sharingDraft, _controller._persistence.playerSettingsBaseline, _controller._currentCampaign.maps);
                const bool campaignPlaceRevealed =
                    mapCreatorPlayerOutputFeatureIsPublic(_controller._currentCampaign, map, feature);
                const char *reason = nullptr;
                if (playerMap.visibility == MapVisibility::GameMasterOnly)
                    reason = uiTextForLocale(
                        "Hidden from players: this map is private or has not been saved for player review.",
                        "Für Spieler verborgen: Diese Karte ist privat oder noch nicht für die Spieleransicht-Prüfung gespeichert.");
                else if (feature.gmOnly || !feature.discovered)
                    reason = uiTextForLocale(
                        "Hidden from players: enable Discovered and turn off GM Only for this feature, then save the map.",
                        "Für Spieler verborgen: Aktiviere Entdeckt, deaktiviere Nur für SL für dieses Element und speichere die Karte.");
                else if (!campaignPlaceRevealed)
                    reason = uiTextForLocale(
                        "Hidden from players: this feature needs one linked campaign place with Visibility set to Discovered or Public. Places and Names alone do not reveal it.",
                        "Für Spieler verborgen: Dieses Element benötigt genau einen verknüpften Kampagnenort mit Sichtbarkeit Entdeckt oder Öffentlich. Orte und Namen allein enthüllen es nicht.");
                else if (!playerMap.playerRelease.placesAndMarkers && !playerMap.playerRelease.labels)
                    reason = uiTextForLocale(
                        "Hidden from players: enable Places for its symbol or Names for its label in the player review. Its campaign place is already revealed.",
                        "Für Spieler verborgen: Aktiviere Orte für das Symbol oder Namen für die Beschriftung in der Spieleransicht-Prüfung. Der Kampagnenort ist bereits enthüllt.");
                else if (!playerMap.playerRelease.placesAndMarkers)
                    reason = uiTextForLocale(
                        "The name is permitted; Places is off. Fog can still hide the name.",
                        "Der Name ist freigegeben; Orte ist ausgeschaltet. Nebel kann den Namen weiterhin verbergen.");
                else
                    reason =
                        playerMap.playerRelease.labels
                            ? uiTextForLocale("Place and name are permitted; fog can still hide them.",
                                              "Ort und Name sind freigegeben; Nebel kann sie weiterhin verbergen.")
                            : uiTextForLocale(
                                  "The place symbol is permitted; Names is off. Fog can still hide the symbol.",
                                  "Das Ortssymbol ist freigegeben; Namen ist ausgeschaltet. Nebel kann das Symbol weiterhin verbergen.");
                ImGui::TextWrapped("%s", reason);
                const auto eligibility =
                    WorldInspector::inspectGeneratedPlacePromotion(_controller._currentCampaign, map, feature.id);
                if (!campaignPlaceRevealed)
                {
                    if (eligibility.status != WorldInspector::GeneratedPlacePromotionStatus::AlreadyPromoted)
                    {
                        const std::string disabledReason = gm::localization::trSystemMessage(eligibility.message);
                        if (drawUiButton("Add to campaign world", eligibility.eligible(), disabledReason.c_str(),
                                         UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
                            _controller.promoteGeneratedPlace(feature.id);
                        if (!eligibility.eligible())
                            ImGui::TextWrapped("%s", disabledReason.c_str());
                    }
                    ImGui::TextWrapped(
                        "%s",
                        uiTextForLocale(
                            "Reveal a linked place in Campaign > World > Places: select it, set Visibility to Discovered or Public, then Save place.",
                            "Enthülle einen verknüpften Ort unter Kampagne > Welt > Orte: Ort wählen, Sichtbarkeit auf Entdeckt oder Öffentlich setzen, dann Ort speichern."));
                }
                drawWorldPlaceSharingHelp(map);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
}
