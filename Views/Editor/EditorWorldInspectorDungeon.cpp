#include "Views/Editor/EditorWorldInspectorSupport.hpp"

void EditorWindowView::renderWorldDungeonInspector(const EntityId &dungeonId)
{
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto text = [german](const char *english, const char *deutsch) { return german ? deutsch : english; };
    if (_worldDraftEntityId != dungeonId)
        reloadWorldInspectorDraft(dungeonId);
    if(_worldDraftEntityId!=dungeonId){renderWorldRecoveryNotice();return;}
    if (!WorldInspector::findDungeon(_controller.currentCampaign(), dungeonId))
    {
        WorkspaceUi::emptyState(text("Dungeon unavailable", "Dungeon nicht verfügbar"),
                                text("Select another World entry.", "Wähle einen anderen Welteintrag."));
        return;
    }

    DraftEditSignal::Scope dungeonEdits([this] { _worldDungeonDirty = true; });
    ImGui::SeparatorText(text("Dungeon map structure", "Dungeonkartenstruktur"));
    FormUi::InputText(text("Name", "Name"), &_worldDungeonDraft.name);
    FormUi::InputTextMultiline(text("Description", "Beschreibung"), &_worldDungeonDraft.description,
                               ImVec2(-1.0f, 52.0f));
    if (ImGui::TreeNode(text("Private dungeon notes", "Private Dungeon-Notizen")))
    {
        FormUi::InputTextMultiline(text("GM notes", "SL-Notizen"), &_worldDungeonDraft.gmNotes, ImVec2(-1.0f, 46.0f));
        ImGui::TreePop();
    }
    const Campaign &campaign = _controller.currentCampaign();
    std::vector<std::string> mapNames{text("No parent map", "Keine übergeordnete Karte")};
    int selectedMap = 0;
    for (int index = 0; index < static_cast<int>(campaign.maps.size()); ++index)
    {
        const MapModel &map = campaign.maps[static_cast<std::size_t>(index)];
        mapNames.push_back(map.name.empty() ? text("Untitled map", "Unbenannte Karte") : map.name);
        if (map.metadata.id == _worldDungeonDraft.parentMapId)
            selectedMap = index + 1;
    }
    std::vector<const char *> mapLabels;
    for (const std::string &name : mapNames)
        mapLabels.push_back(name.c_str());
    if (FormUi::Combo(text("Parent map", "Übergeordnete Karte"), &selectedMap, mapLabels.data(),
                      static_cast<int>(mapLabels.size())))
        _worldDungeonDraft.parentMapId =
            selectedMap == 0 ? EntityId{} : campaign.maps[static_cast<std::size_t>(selectedMap - 1)].metadata.id;
    if (drawUiButton(text("Save structure", "Struktur speichern"), UiButtonRole::Primary, ImVec2(150.0f, 0.0f)))
    {
        const DungeonModel replacement = _worldDungeonDraft;
        reportWorldInspectorResult(_controller.applyWorldInspectorMutation(
                                       [dungeonId, replacement](Campaign &value) {
                                           return WorldInspector::updateDungeon(value, dungeonId, replacement);
                                       },
                                       "edit World dungeon structure"),
                                   "Dungeon structure saved.", "Dungeonstruktur gespeichert.");
    }
    ImGui::SameLine();
    if (drawUiButton(text("Delete dungeon", "Dungeon löschen"), UiButtonRole::Destructive, ImVec2(150.0f, 0.0f)))
        ImGui::OpenPopup(localizedUiLabel("Delete World Dungeon").c_str());
    if (ImGui::BeginPopupModal(localizedUiLabel("Delete World Dungeon").c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            localizedUiLabel("%s").c_str(),
            text("Delete this dungeon, all keyed rooms, and their connections? Linked maps remain intact.",
                 "Diesen Dungeon, alle Räume und ihre Verbindungen löschen? Verknüpfte Karten bleiben erhalten."));
        if (drawModalActionButtons(text("Delete dungeon", "Dungeon löschen"), UiButtonRole::Destructive,
                                   text("Cancel", "Abbrechen"), 160.0f))
        {
            const WorldInspector::MutationResult result = _controller.applyWorldInspectorMutation(
                [dungeonId](Campaign &value) { return WorldInspector::deleteDungeon(value, dungeonId); },
                "delete World dungeon");
            if (result.applied())
            {
                _workspaceSelectionId.clear();
                _worldDraftEntityId.clear();
            }
            _worldInspectorStatus = result.applied() ? text("Dungeon deleted.", "Dungeon gelöscht.") : result.message;
        }
        ImGui::EndPopup();
    }

    ImGui::SeparatorText(text("Rooms as map structures", "Räume als Kartenstrukturen"));
    if (drawSmallUiButton(text("Add room", "Raum hinzufügen"), UiButtonRole::Secondary))
    {
        const std::string defaultRoomName = text("New Room", "Neuer Raum");
        const WorldInspector::MutationResult result = _controller.applyWorldInspectorMutation(
            [dungeonId, defaultRoomName](Campaign &value) {
                return WorldInspector::addRoom(value, dungeonId, defaultRoomName);
            },
            "add World dungeon room");
        if (result.applied())
            _worldSelectedRoomId = result.entityId;
        reportWorldInspectorResult(result, "Room added.", "Raum hinzugefügt.");
        if (result.applied())
            _worldSelectedRoomId = result.entityId;
    }
    if (!_worldDungeonDraft.rooms.empty()) {
        if(RoomDiagramUi::draw(_worldDungeonDraft,campaign,_worldSelectedRoomId,_worldPlaceRoom))
        {
            _worldDungeonDirty = true;
            _worldInspectorStatus=text("Layout changed. Save structure to keep it.", "Grundriss geändert. Zum Behalten Struktur speichern.");
        }
    }
    const bool roomColumns=TaskUi::hasRoomForColumns(42.f);
    if (_worldDungeonDraft.rooms.empty())
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), text("No keyed rooms yet.", "Noch keine Räume angelegt."));
    else if (ImGui::BeginTable("WorldDungeonRooms", roomColumns?2:1, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableSetupColumn(text("Rooms", "Räume"), roomColumns ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_WidthStretch, roomColumns ? 190.0f : 1.0f);
        if(roomColumns) ImGui::TableSetupColumn(text("Room inspector", "Rauminspektor"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        for (const DungeonRoomModel &room : _worldDungeonDraft.rooms)
        {
            const std::string label = (room.name.empty() ? text("Unnamed room", "Unbenannter Raum") : room.name) + "##" + room.metadata.id;
            if (ImGui::Selectable(label.c_str(), room.metadata.id == _worldSelectedRoomId))
                _worldSelectedRoomId = room.metadata.id;
        }
        ImGui::TableNextColumn();
        DungeonRoomModel *room = WorldInspector::findRoom(_worldDungeonDraft, _worldSelectedRoomId);
        if (room)
        {
            FormUi::InputText(text("Room name", "Raumname"), &room->name);
            int exploration = static_cast<int>(room->explorationState);
            const char *statesEnglish[] = {"Unknown", "Seen", "Explored", "Cleared"};
            const char *statesGerman[] = {"Unbekannt", "Gesehen", "Erkundet", "Gesichert"};
            if (FormUi::Combo(text("Exploration", "Erkundung"), &exploration, german ? statesGerman : statesEnglish, 4))
                room->explorationState = static_cast<DungeonRoomExplorationState>(exploration);
            if(ImGui::TreeNode(text("Exact map coordinates", "Genaue Kartenkoordinaten"))) {
                FormUi::InputInt(text("Map X", "Karte X"), &room->mapX);
                FormUi::InputInt(text("Map Y", "Karte Y"), &room->mapY);
                ImGui::TreePop();
            }
            FormUi::InputTextMultiline(text("Keyed description", "Schlüsselbeschreibung"), &room->keyedDescription,
                                       ImVec2(-1.0f, 52.0f));
            FormUi::InputTextMultiline(text("Read aloud", "Vorlesetext"), &room->readAloudText, ImVec2(-1.0f, 44.0f));
            if(ImGui::TreeNode(text("GM secrets and traps", "SL-Geheimnisse und Fallen"))) {
            FormUi::InputTextMultiline(text("Secrets", "Geheimnisse"), &room->secrets, ImVec2(-1.0f, 40.0f));
            FormUi::InputTextMultiline(text("Traps", "Fallen"), &room->traps, ImVec2(-1.0f, 40.0f));
            FormUi::InputTextMultiline(text("Exploration notes", "Erkundungsnotizen"), &room->explorationNotes,
                                       ImVec2(-1.0f, 40.0f));
            ImGui::TreePop();
            }
            if (drawUiButton(text("Save room", "Raum speichern"), UiButtonRole::Primary, ImVec2(130.0f, 0.0f)))
            {
                const EntityId roomId = room->metadata.id;
                const DungeonRoomModel replacement = *room;
                reportWorldInspectorResult(_controller.applyWorldInspectorMutation(
                                               [dungeonId, roomId, replacement](Campaign &value) {
                                                   return WorldInspector::updateRoom(value, dungeonId, roomId,
                                                                                     replacement);
                                               },
                                               "edit World dungeon room"),
                                           "Room saved.", "Raum gespeichert.");
                _worldSelectedRoomId = roomId;
            }
            ImGui::SameLine();
            if (drawUiButton(text("Delete room", "Raum löschen"), UiButtonRole::Destructive, ImVec2(130.0f, 0.0f)))
                ImGui::OpenPopup(localizedUiLabel("Delete World Room").c_str());
            if (ImGui::BeginPopupModal(localizedUiLabel("Delete World Room").c_str(), nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                                   text("Delete this room and every connection that points to it?",
                                        "Diesen Raum und alle auf ihn zeigenden Verbindungen löschen?"));
                if (drawModalActionButtons(text("Delete room", "Raum löschen"), UiButtonRole::Destructive,
                                           text("Cancel", "Abbrechen"), 140.0f))
                {
                    const EntityId roomId = room->metadata.id;
                    reportWorldInspectorResult(_controller.applyWorldInspectorMutation(
                                                   [dungeonId, roomId](Campaign &value) {
                                                       return WorldInspector::deleteRoom(value, dungeonId, roomId);
                                                   },
                                                   "delete World dungeon room"),
                                               "Room and inbound connections deleted.",
                                               "Raum und eingehende Verbindungen gelöscht.");
                }
                ImGui::EndPopup();
            }

            room = WorldInspector::findRoom(_worldDungeonDraft, _worldSelectedRoomId);
            if (room)
            {
                ImGui::SeparatorText(text("Room connections", "Raumverbindungen"));
                std::vector<const DungeonRoomModel *> targets;
                std::vector<std::string> targetNames;
                for (const DungeonRoomModel &candidate : _worldDungeonDraft.rooms)
                {
                    if (candidate.metadata.id == room->metadata.id)
                        continue;
                    targets.push_back(&candidate);
                    targetNames.push_back(candidate.name.empty() ? text("Unnamed room", "Unbenannter Raum")
                                                                 : candidate.name);
                }
                if (!targets.empty())
                {
                    _worldConnectionTargetIndex =
                        std::clamp(_worldConnectionTargetIndex, 0, static_cast<int>(targets.size()) - 1);
                    std::vector<const char *> targetLabels;
                    for (const std::string &name : targetNames)
                        targetLabels.push_back(name.c_str());
                    ImGui::SetNextItemWidth(std::min(260.0f, ImGui::GetContentRegionAvail().x - 130.0f));
                    FormUi::Combo(localizedUiLabel("##WorldRoomTarget").c_str(), &_worldConnectionTargetIndex,
                                  targetLabels.data(), static_cast<int>(targetLabels.size()));
                    ImGui::SameLine();
                    if (drawSmallUiButton(text("Connect", "Verbinden"), UiButtonRole::Secondary))
                    {
                        const EntityId sourceId = room->metadata.id;
                        const EntityId targetId =
                            targets[static_cast<std::size_t>(_worldConnectionTargetIndex)]->metadata.id;
                        reportWorldInspectorResult(_controller.applyWorldInspectorMutation(
                                                       [dungeonId, sourceId, targetId](Campaign &value) {
                                                           return WorldInspector::addConnection(value, dungeonId,
                                                                                                sourceId, targetId);
                                                       },
                                                       "add World dungeon connection"),
                                                   "Room connection added.", "Raumverbindung hinzugefügt.");
                        _worldSelectedRoomId = sourceId;
                    }
                }
                else
                    ImGui::TextDisabled(localizedUiLabel("%s").c_str(),
                                        text("Add another room to create connections.",
                                             "Füge einen weiteren Raum hinzu, um Verbindungen anzulegen."));

                room = WorldInspector::findRoom(_worldDungeonDraft, _worldSelectedRoomId);
                if (room)
                {
                    for (std::size_t index = 0; index < room->connections.size(); ++index)
                    {
                        DungeonRoomConnectionModel &connection = room->connections[index];
                        ImGui::PushID(connection.metadata.id.c_str());
                        const DungeonRoomModel *target =
                            WorldInspector::findRoom(_worldDungeonDraft, connection.targetRoomId);
                        ImGui::Text(localizedUiLabel("%s: %s").c_str(), text("To", "Nach"),
                                    target ? target->name.c_str() : text("Missing room", "Fehlender Raum"));
                        FormUi::InputText(text("Label", "Bezeichnung"), &connection.label);
                        FormUi::Checkbox(text("Locked", "Verriegelt"), &connection.locked);
                        ImGui::SameLine();
                        FormUi::Checkbox(text("Secret", "Geheim"), &connection.secret);
                        ImGui::SameLine();
                        FormUi::Checkbox(text("Discovered", "Entdeckt"), &connection.discovered);
                        if (drawSmallUiButton(text("Save connection", "Verbindung speichern"), UiButtonRole::Secondary))
                        {
                            const EntityId sourceId = room->metadata.id;
                            const EntityId connectionId = connection.metadata.id;
                            const DungeonRoomConnectionModel replacement = connection;
                            reportWorldInspectorResult(
                                _controller.applyWorldInspectorMutation(
                                    [dungeonId, sourceId, connectionId, replacement](Campaign &value) {
                                        return WorldInspector::updateConnection(value, dungeonId, sourceId,
                                                                                connectionId, replacement);
                                    },
                                    "edit World dungeon connection"),
                                "Connection saved.", "Verbindung gespeichert.");
                            _worldSelectedRoomId = sourceId;
                        }
                        ImGui::SameLine();
                        if (drawSmallUiButton(text("Remove connection", "Verbindung entfernen"),
                                              UiButtonRole::Destructive))
                            ImGui::OpenPopup(localizedUiLabel("Remove World Connection").c_str());
                        if (ImGui::BeginPopupModal(localizedUiLabel("Remove World Connection").c_str(), nullptr,
                                                   ImGuiWindowFlags_AlwaysAutoResize))
                        {
                            ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                                               text("Remove this one-way room connection?",
                                                    "Diese einseitige Raumverbindung entfernen?"));
                            if (drawModalActionButtons(text("Remove", "Entfernen"), UiButtonRole::Destructive,
                                                       text("Cancel", "Abbrechen"), 130.0f))
                            {
                                const EntityId sourceId = room->metadata.id;
                                const EntityId connectionId = connection.metadata.id;
                                reportWorldInspectorResult(_controller.applyWorldInspectorMutation(
                                                               [dungeonId, sourceId, connectionId](Campaign &value) {
                                                                   return WorldInspector::deleteConnection(
                                                                       value, dungeonId, sourceId, connectionId);
                                                               },
                                                               "remove World dungeon connection"),
                                                           "Connection removed.", "Verbindung entfernt.");
                                _worldSelectedRoomId = sourceId;
                            }
                            ImGui::EndPopup();
                        }
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                }
            }
        }
        ImGui::EndTable();
    }
    if (!_worldInspectorStatus.empty())
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _worldInspectorStatus.c_str());
}
