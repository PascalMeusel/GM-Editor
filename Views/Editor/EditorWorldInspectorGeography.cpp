#include "Views/Editor/EditorWorldInspectorSupport.hpp"

void EditorWindowView::renderWorldNationInspector(const EntityId &nationId)
{
    if(!worldRecoveryReady()){renderWorldRecoveryNotice();return;}
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto text = [german](const char *english, const char *deutsch) { return german ? deutsch : english; };
    if (_worldDraftEntityId != nationId || _worldDraftReloadRequested)
        reloadWorldInspectorDraft(nationId);
    if(_worldDraftEntityId!=nationId)return;
    if (!WorldInspector::findNation(_controller.currentCampaign(), nationId))
    {
        WorkspaceUi::emptyState(
            text("Nation unavailable", "Nation nicht verfügbar"),
            text("The selected nation no longer exists.", "Die ausgewählte Nation existiert nicht mehr."));
        return;
    }

    DraftEditSignal::Scope nationEdits([this]{_worldFormEdited=true;});
    FormUi::InputText(text("Name", "Name"), &_worldNationDraft.name);
    FormUi::InputTextMultiline(text("Description", "Beschreibung"), &_worldNationDraft.description,
                               ImVec2(-1.0f, 62.0f));
    FormUi::InputText(text("Government", "Regierung"), &_worldNationDraft.government);
    FormUi::InputText(text("Culture", "Kultur"), &_worldNationDraft.culture);
    FormUi::InputTextMultiline(text("Relationships", "Beziehungen"), &_worldNationDraft.relationships,
                               ImVec2(-1.0f, 54.0f));

    int visibility = static_cast<int>(_worldNationDraft.visibility);
    const char *visibilityItems = "GM only\0Discovered\0Public\0";
    if (FormUi::Combo(text("Visibility", "Sichtbarkeit"), &visibility, visibilityItems))
        _worldNationDraft.visibility = static_cast<WorldObjectVisibility>(std::clamp(visibility, 0, 2));

    std::vector<std::string> capitalNames{text("No capital", "Keine Hauptstadt")};
    std::vector<const char *> capitalLabels;
    int capitalIndex = 0;
    for (int index = 0; index < static_cast<int>(_controller.currentCampaign().worldPlaces.size()); ++index)
    {
        const WorldPlaceModel &place = _controller.currentCampaign().worldPlaces[static_cast<std::size_t>(index)];
        capitalNames.push_back(place.name.empty() ? text("Unnamed place", "Unbenannter Ort") : place.name);
        if (place.metadata.id == _worldNationDraft.capitalPlaceId)
            capitalIndex = index + 1;
    }
    for (const std::string &name : capitalNames)
        capitalLabels.push_back(name.c_str());
    if (FormUi::Combo(text("Capital", "Hauptstadt"), &capitalIndex, capitalLabels.data(),
                      static_cast<int>(capitalLabels.size())))
        _worldNationDraft.capitalPlaceId =
            capitalIndex == 0
                ? EntityId{}
                : _controller.currentCampaign().worldPlaces[static_cast<std::size_t>(capitalIndex - 1)].metadata.id;

    ImGui::SeparatorText(text("Territories", "Gebiete"));
    if (drawSmallUiButton(text("Add territory", "Gebiet hinzufügen"), UiButtonRole::Secondary))
    {
        const WorldInspector::MutationResult result = _controller.applyWorldInspectorMutation(
            [nationId](Campaign &campaign) {
                return WorldInspector::addNationTerritory(campaign, nationId, "Territory");
            },
            "add nation territory");
        reportWorldInspectorResult(result, "Territory added.", "Gebiet hinzugefügt.");
    }
    for (int territoryIndex = 0; territoryIndex < static_cast<int>(_worldNationDraft.territories.size());
         ++territoryIndex)
    {
        NationTerritoryModel &territory = _worldNationDraft.territories[static_cast<std::size_t>(territoryIndex)];
        ImGui::PushID(territory.metadata.id.c_str());
        const bool open =
            ImGui::TreeNodeEx("##Territory", ImGuiTreeNodeFlags_DefaultOpen, "%s",
                              territory.name.empty() ? text("Territory", "Gebiet") : territory.name.c_str());
        if (open)
        {
            FormUi::InputText(text("Territory name", "Gebietsname"), &territory.name);
            ImGui::TextDisabled("%s", text("Normalized polygon points", "Normierte Polygonpunkte"));
            for (int pointIndex = 0; pointIndex < static_cast<int>(territory.polygon.size()); ++pointIndex)
            {
                float point[2] = {static_cast<float>(territory.polygon[static_cast<std::size_t>(pointIndex)].x),
                                  static_cast<float>(territory.polygon[static_cast<std::size_t>(pointIndex)].y)};
                ImGui::PushID(pointIndex);
                if (FormUi::DragFloat2("##TerritoryPoint", point, 0.005f, 0.0f, 1.0f, "%.3f"))
                {
                    territory.polygon[static_cast<std::size_t>(pointIndex)].x = point[0];
                    territory.polygon[static_cast<std::size_t>(pointIndex)].y = point[1];
                }
                ImGui::PopID();
            }
            if (drawSmallUiButton(text("Remove territory", "Gebiet entfernen"), UiButtonRole::Destructive))
            {
                const EntityId territoryId = territory.metadata.id;
                const WorldInspector::MutationResult result = _controller.applyWorldInspectorMutation(
                    [nationId, territoryId](Campaign &campaign) {
                        return WorldInspectorDrafts::removeTerritory(campaign, nationId, territoryId);
                    },
                    "remove nation territory", {}, {WorldInspectorDrafts::Part::Territory,nationId,territoryId});
                reportWorldInspectorResult(result, "Territory removed.", "Gebiet entfernt.");
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (drawUiButton(text("Save nation", "Nation speichern"), UiButtonRole::Primary, ImVec2(160.0f, 0.0f)))
    {
        reportWorldInspectorResult(_controller.saveWorldDraft(WorldInspectorDrafts::Scope::entity(nationId), "save nation"),
                                   "Nation saved.", "Nation gespeichert.");
    }
    ImGui::SameLine();
    if (drawUiButton(text("Delete nation", "Nation löschen"), UiButtonRole::Destructive, ImVec2(160.0f, 0.0f)))
        ImGui::OpenPopup(localizedUiLabel("Delete Nation").c_str());
    if (ImGui::BeginPopupModal(localizedUiLabel("Delete Nation").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("%s", text("Delete this nation? Assigned places must be moved first.",
                                      "Diese Nation löschen? Zugeordnete Orte müssen zuvor verschoben werden."));
        if (drawModalActionButtons(text("Delete", "Löschen"), UiButtonRole::Destructive, text("Cancel", "Abbrechen"),
                                   130.0f))
            reportWorldInspectorResult(
                _controller.applyWorldInspectorMutation(
                    [nationId](Campaign &campaign) { return WorldInspector::deleteNation(campaign, nationId); },
                    "delete nation", {}, WorldInspectorDrafts::Scope::entity(nationId)),
                "Nation deleted.", "Nation gelöscht.");
        ImGui::EndPopup();
    }
    if (!_worldInspectorStatus.empty())
        ImGui::TextWrapped("%s", _worldInspectorStatus.c_str());
}

void EditorWindowView::renderWorldPlaceInspector(const EntityId &placeId)
{
    if(!worldRecoveryReady()){renderWorldRecoveryNotice();return;}
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto text = [german](const char *english, const char *deutsch) { return german ? deutsch : english; };
    if (_worldDraftEntityId != placeId || _worldDraftReloadRequested)
        reloadWorldInspectorDraft(placeId);
    if(_worldDraftEntityId!=placeId)return;
    if (!WorldInspector::findWorldPlace(_controller.currentCampaign(), placeId))
    {
        WorkspaceUi::emptyState(
            text("Place unavailable", "Ort nicht verfügbar"),
            text("The selected place no longer exists.", "Der ausgewählte Ort existiert nicht mehr."));
        return;
    }

    DraftEditSignal::Scope placeEdits([this]{_worldFormEdited=true;});
    FormUi::InputText(text("Name", "Name"), &_worldPlaceDraft.name);
    int typeIndex = static_cast<int>(_worldPlaceDraft.type);
    const char *typeItems = german ? "Hauptstadt\0Stadt\0Kleinstadt\0Dorf\0Orientierungspunkt\0Schauplatz\0"
                                   : "Capital\0City\0Town\0Village\0Landmark\0Site\0";
    if (FormUi::Combo(text("Place type", "Ortstyp"), &typeIndex, typeItems))
        _worldPlaceDraft.type = static_cast<WorldPlaceType>(std::clamp(typeIndex, 0, 5));
    FormUi::InputTextMultiline(text("Description", "Beschreibung"), &_worldPlaceDraft.description,
                               ImVec2(-1.0f, 62.0f));
    FormUi::InputTextMultiline(text("GM notes", "SL-Notizen"), &_worldPlaceDraft.notes, ImVec2(-1.0f, 54.0f));

    std::vector<std::string> nationNames{text("No nation", "Keine Nation")};
    std::vector<const char *> nationLabels;
    int nationIndex = 0;
    for (int index = 0; index < static_cast<int>(_controller.currentCampaign().nations.size()); ++index)
    {
        const NationModel &nation = _controller.currentCampaign().nations[static_cast<std::size_t>(index)];
        nationNames.push_back(nation.name.empty() ? text("Unnamed nation", "Unbenannte Nation") : nation.name);
        if (nation.metadata.id == _worldPlaceDraft.nationId)
            nationIndex = index + 1;
    }
    for (const std::string &name : nationNames)
        nationLabels.push_back(name.c_str());
    if (FormUi::Combo(text("Nation", "Nation"), &nationIndex, nationLabels.data(),
                      static_cast<int>(nationLabels.size())))
        _worldPlaceDraft.nationId =
            nationIndex == 0
                ? EntityId{}
                : _controller.currentCampaign().nations[static_cast<std::size_t>(nationIndex - 1)].metadata.id;

    std::vector<std::string> mapNames{text("No linked map", "Keine verknüpfte Karte")};
    std::vector<const char *> mapLabels;
    int mapIndex = 0;
    for (int index = 0; index < static_cast<int>(_controller.currentCampaign().maps.size()); ++index)
    {
        const MapModel &map = _controller.currentCampaign().maps[static_cast<std::size_t>(index)];
        mapNames.push_back(map.name.empty() ? text("Untitled map", "Unbenannte Karte") : map.name);
        if (map.metadata.id == _worldPlaceDraft.linkedMapId)
            mapIndex = index + 1;
    }
    for (const std::string &name : mapNames)
        mapLabels.push_back(name.c_str());
    if (FormUi::Combo(text("Linked map", "Verknüpfte Karte"), &mapIndex, mapLabels.data(),
                      static_cast<int>(mapLabels.size())))
    {
        _worldPlaceDraft.linkedMapId =
            mapIndex == 0 ? EntityId{}
                          : _controller.currentCampaign().maps[static_cast<std::size_t>(mapIndex - 1)].metadata.id;
        if (_worldPlaceDraft.linkedMapId.empty())
            _worldPlaceDraft.hasMapPosition = false;
    }

    ImGui::BeginDisabled(_worldPlaceDraft.linkedMapId.empty());
    FormUi::Checkbox(text("Use as fixed map anchor", "Als feste Kartenvorgabe verwenden"),
                     &_worldPlaceDraft.hasMapPosition);
    if (_worldPlaceDraft.hasMapPosition)
    {
        float westEast = static_cast<float>(_worldPlaceDraft.mapPosition.x);
        float northSouth = static_cast<float>(_worldPlaceDraft.mapPosition.y);
        if (FormUi::SliderFloat(text("West to east", "West nach Ost"), &westEast, 0.0f, 1.0f, "%.2f"))
            _worldPlaceDraft.mapPosition.x = westEast;
        if (FormUi::SliderFloat(text("North to south", "Nord nach Süd"), &northSouth, 0.0f, 1.0f, "%.2f"))
            _worldPlaceDraft.mapPosition.y = northSouth;
        ImGui::TextWrapped(
            "%s",
            text(
                "Map generation keeps this place and fills terrain, roads, rivers, and additional locations around it.",
                "Die Kartenerzeugung behält diesen Ort bei und ergänzt Gelände, Straßen, Flüsse und weitere Orte darum herum."));
    }
    ImGui::EndDisabled();

    const bool settlementLike =
        _worldPlaceDraft.type == WorldPlaceType::Capital || _worldPlaceDraft.type == WorldPlaceType::City ||
        _worldPlaceDraft.type == WorldPlaceType::Town || _worldPlaceDraft.type == WorldPlaceType::Village;
    if (settlementLike)
    {
        ImGui::Spacing();
        if (drawUiButton(text("Create city map from this place", "Stadtkarte aus diesem Ort erstellen"),
                         UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                         text("Open a focused settlement brief and inherit this place as the map anchor.",
                              "Öffnet einen fokussierten Siedlungsentwurf und übernimmt diesen Ort als Kartenanker.")))
        {
            const auto saved = _controller.saveWorldDraft(WorldInspectorDrafts::Scope::entity(placeId),
                "save world place before creating settlement map");
            if (saved.applied() && _controller.createMapFromWorldPlace(placeId))
                _worldInspectorStatus =
                    text("Settlement map brief opened.", "Der Entwurf für die Stadtkarte wurde geöffnet.");
            else if (!saved.applied())
                reportWorldInspectorResult(saved, "Place saved.", "Ort gespeichert.");
        }
    }

    int visibility = static_cast<int>(_worldPlaceDraft.visibility);
    const char *visibilityItems = german ? "Nur Spielleitung\0Entdeckt\0Öffentlich\0" : "GM only\0Discovered\0Public\0";
    if (FormUi::Combo(text("Visibility", "Sichtbarkeit"), &visibility, visibilityItems))
        _worldPlaceDraft.visibility = static_cast<WorldObjectVisibility>(std::clamp(visibility, 0, 2));

    if (drawUiButton(text("Save place", "Ort speichern"), UiButtonRole::Primary, ImVec2(160.0f, 0.0f)))
    {
        reportWorldInspectorResult(_controller.saveWorldDraft(WorldInspectorDrafts::Scope::entity(placeId), "save world place"),
                                   "Place saved.", "Ort gespeichert.");
    }
    ImGui::SameLine();
    if (drawUiButton(text("Delete place", "Ort löschen"), UiButtonRole::Destructive, ImVec2(160.0f, 0.0f)))
        ImGui::OpenPopup(localizedUiLabel("Delete World Place").c_str());
    if (ImGui::BeginPopupModal(localizedUiLabel("Delete World Place").c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("%s", text("Delete this place? A capital must be reassigned first.",
                                      "Diesen Ort löschen? Eine Hauptstadt muss zuvor neu zugewiesen werden."));
        if (drawModalActionButtons(text("Delete", "Löschen"), UiButtonRole::Destructive, text("Cancel", "Abbrechen"),
                                   130.0f))
            reportWorldInspectorResult(
                _controller.applyWorldInspectorMutation(
                    [placeId](Campaign &campaign) { return WorldInspector::deleteWorldPlace(campaign, placeId); },
                    "delete world place", {}, WorldInspectorDrafts::Scope::entity(placeId)),
                "Place deleted.", "Ort gelöscht.");
        ImGui::EndPopup();
    }
    if (!_worldInspectorStatus.empty())
        ImGui::TextWrapped("%s", _worldInspectorStatus.c_str());
}
