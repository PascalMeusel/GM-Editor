#include "Views/Editor/EditorWorkspacesSupport.hpp"

void EditorWindowView::renderCampaignHome()
{
    if (DockableWidget::defaultDockId() != 0)
        ImGui::SetNextWindowDockID(DockableWidget::defaultDockId(), ImGuiCond_Always);
    if (_focusCampaignHomeNextFrame)
    {
        ImGui::SetNextWindowFocus();
        _focusCampaignHomeNextFrame = false;
    }
    const char *title = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                            ? "Sitzung###Campaign Home"
                            : "Session###Campaign Home";
    if (ImGui::Begin(title, nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        SessionWorkspace::Actions actions;
        actions.persist = [this](Campaign &staged) {
            const bool saved = ProjectSaveService::getInstance().saveCampaign(staged, "prepare session");
            if (saved)
                _controller.markSearchIndexDirty();
            return saved;
        };
        actions.run = [this]() { _controller.showLiveSessionRunTab(); };
        actions.prepareTarget = [this](PreparationTargetRequest request) { _controller.queuePreparationTarget(std::move(request)); };
        actions.started = [this](const EntityId &sessionId, const EntityId &sceneId) {
            _controller._liveSessionDashboard->afterSessionStarted(sessionId, sceneId);
        };
        actions.openEntity = [this](const std::string &type, const EntityId &id) {
            if (type == "Map" || type == "map")
                _controller.editWorkspaceMap(id);
            else
                _controller.openWorkspaceEntity(type, id);
        };
        actions.newMap = [this]() { _controller.showMapCreator(true); };
        const CampaignSceneDraftContext creationContext{_controller._currentCampaign.metadata.id,
                                                        _sessionWorkspace.sessionId(), _sessionWorkspace.sceneId()};
        const auto createdFromScene = [this, creationContext, creationEntryId = _sessionWorkspace.planEntryId()]() {
            _controller.markSearchIndexDirty();
            if (!_sessionWorkspace.restoreSelection(_controller._currentCampaign, creationContext.sessionId,
                creationEntryId, false, [](Campaign &staged) {
                    return ProjectSaveService::getInstance().saveCampaign(staged, "return to entity creation scene");
                })) return;
            activateWorkspace(EditorWorkspace::Home);
            _focusCampaignHomeNextFrame = true;
        };
        actions.newCharacter = [this, creationContext, createdFromScene]() {
            _controller.showPlayerCreator();
            _controller._playerCreator->setSceneCreationContext(creationContext, createdFromScene);
        };
        actions.newNpc = [this, creationContext, createdFromScene]() {
            _controller.showNPCCreator();
            _controller._npcCreator->setSceneCreationContext(creationContext, createdFromScene);
        };
        actions.newEncounter = [this]() { _controller.showNewEncounterBuilder(); };
        actions.presentation = [this]() { _controller.showPresentationPanel(); };
        actions.blackout = [this]() { _controller.blackoutLocalPlayerDisplayOutput(); };
        _sessionWorkspace.draw(_controller._currentCampaign, actions);
    }
    ImGui::End();
}

void EditorWindowView::renderKnowledgeGraph()
{
    if (!_knowledgeGraphOpen)
        return;

    const CompactUiDensityScope uiDensity;
    ImGui::SetNextWindowSize(ImVec2(980.0f, 620.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin(localizedUiLabel("Knowledge Graph").c_str(), &_knowledgeGraphOpen))
    {
        ImGui::End();
        return;
    }

    const CampaignKnowledgeGraph graph = _controller.knowledgeGraph();
    ImGui::TextColored(kMutedText, localizedUiLabel("%d nodes, %d edges").c_str(), static_cast<int>(graph.nodes.size()),
                       static_cast<int>(graph.edges.size()));
    ImGui::Separator();

    if (graph.nodes.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No campaign relationships have been indexed yet.").c_str());
        ImGui::End();
        return;
    }

    if (!findGraphNode(graph, _selectedKnowledgeGraphNodeId))
        _selectedKnowledgeGraphNodeId = graph.nodes.front().id;

    const CampaignGraphNode *selectedNode = findGraphNode(graph, _selectedKnowledgeGraphNodeId);
    const CampaignGraphEdge *selectedEdge = findGraphEdge(graph, _selectedKnowledgeGraphEdgeKey);
    if (!selectedEdge || !edgeTouchesNode(*selectedEdge, _selectedKnowledgeGraphNodeId))
    {
        _selectedKnowledgeGraphEdgeKey.clear();
        selectedEdge = nullptr;
        const auto firstRelated =
            std::find_if(graph.edges.begin(), graph.edges.end(), [&](const CampaignGraphEdge &edge) {
                return edgeTouchesNode(edge, _selectedKnowledgeGraphNodeId);
            });
        if (firstRelated != graph.edges.end())
        {
            _selectedKnowledgeGraphEdgeKey = knowledgeGraphEdgeKey(*firstRelated);
            selectedEdge = &(*firstRelated);
        }
    }

    if (ImGui::BeginTable("KnowledgeGraphInspector", 3,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Entities").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
        ImGui::TableSetupColumn(localizedUiLabel("Relationships").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.34f);
        ImGui::TableSetupColumn(localizedUiLabel("Inspector").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.38f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        pushBoldFont();
        ImGui::TextUnformatted(localizedUiLabel("Entities").c_str());
        popBoldFont();
        ImGui::BeginChild("KnowledgeGraphNodes", ImVec2(0.0f, 0.0f), true);
        for (const CampaignGraphNode &node : graph.nodes)
        {
            int relatedCount = 0;
            for (const CampaignGraphEdge &edge : graph.edges)
            {
                if (edgeTouchesNode(edge, node.id))
                    ++relatedCount;
            }
            const std::string label = node.label + " [" + node.type + "]##GraphNode" + node.id;
            if (ImGui::Selectable(label.c_str(), _selectedKnowledgeGraphNodeId == node.id))
            {
                _selectedKnowledgeGraphNodeId = node.id;
                _selectedKnowledgeGraphEdgeKey.clear();
            }
            ImGui::TextColored(kMutedText, localizedUiLabel("%d relationship(s)").c_str(), relatedCount);
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        pushBoldFont();
        ImGui::TextUnformatted(localizedUiLabel("Relationships").c_str());
        popBoldFont();
        ImGui::BeginChild("KnowledgeGraphEdges", ImVec2(0.0f, 0.0f), true);
        int visibleEdgeCount = 0;
        for (const CampaignGraphEdge &edge : graph.edges)
        {
            if (!edgeTouchesNode(edge, _selectedKnowledgeGraphNodeId))
                continue;
            ++visibleEdgeCount;
            const std::string key = knowledgeGraphEdgeKey(edge);
            const bool selected = key == _selectedKnowledgeGraphEdgeKey;
            const std::string label = graphEdgeSummary(edge) + "##GraphEdge" + key;
            if (ImGui::Selectable(label.c_str(), selected))
                _selectedKnowledgeGraphEdgeKey = key;
            ImGui::TextColored(kMutedText, localizedUiLabel("%s -> %s").c_str(), edge.fromLabel.c_str(),
                               edge.toLabel.c_str());
        }
        if (visibleEdgeCount == 0)
        {
            ImGui::TextDisabled(localizedUiLabel("No relationships for the selected entity.").c_str());
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(2);
        pushBoldFont();
        ImGui::TextUnformatted(localizedUiLabel("Inspector").c_str());
        popBoldFont();
        ImGui::BeginChild("KnowledgeGraphInspectorDetail", ImVec2(0.0f, 0.0f), true);
        if (selectedNode)
        {
            ImGui::SeparatorText(localizedUiLabel("Selected Entity").c_str());
            drawInlineLabelValue("Label", selectedNode->label.c_str());
            drawInlineLabelValue("Type", selectedNode->type.c_str());
            if (drawSmallUiButton("Open Entity", UiButtonRole::Primary, "Open the widget that owns this entity."))
            {
                CampaignCommandPaletteItem item{"open:" + selectedNode->id, "Open " + selectedNode->label,
                                                selectedNode->type, selectedNode->id, selectedNode->type};
                _controller.executePaletteCommand(item);
            }
            ImGui::SetNextItemOpen(false, ImGuiCond_Once);
            if (ImGui::CollapsingHeader(localizedUiLabel("Advanced Raw Entity Data##GraphRawEntity").c_str()))
            {
                drawInlineLabelValue("ID", selectedNode->id.c_str());
                if (drawSmallUiButton("Copy Entity ID", UiButtonRole::Advanced,
                                      "Copy this entity ID for linking or diagnostics."))
                    ImGui::SetClipboardText(selectedNode->id.c_str());
            }

            const std::vector<CampaignBacklink> backlinks = _controller.backlinksToEntity(selectedNode->id);
            ImGui::SeparatorText(localizedUiLabel("Backlinks").c_str());
            if (backlinks.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("No backlinks found.").c_str());
            }
            else
            {
                ImGui::TextColored(kMutedText, localizedUiLabel("%d backlink(s)").c_str(),
                                   static_cast<int>(backlinks.size()));
                for (const CampaignBacklink &backlink : backlinks)
                    ImGui::BulletText("%s [%s] - %s", backlink.sourceTitle.c_str(), backlink.sourceType.c_str(),
                                      backlink.relationship.c_str());
            }
        }

        if (selectedEdge)
        {
            ImGui::SeparatorText(localizedUiLabel("Selected Relationship").c_str());
            drawInlineLabelValue("Summary", graphEdgeSummary(*selectedEdge).c_str());
            drawInlineLabelValue("Relationship", selectedEdge->relationship.c_str());
            drawInlineLabelValue("Source", selectedEdge->fromLabel.c_str());
            drawInlineLabelValue("Target", selectedEdge->toLabel.c_str());
            ImGui::SetNextItemOpen(false, ImGuiCond_Once);
            if (ImGui::CollapsingHeader(
                    localizedUiLabel("Advanced Raw Relationship Data##GraphRawRelationship").c_str()))
            {
                drawInlineLabelValue("Source ID", selectedEdge->fromId.c_str());
                drawInlineLabelValue("Target ID", selectedEdge->toId.c_str());
                if (drawSmallUiButton("Copy Source ID", UiButtonRole::Advanced, "Copy the source entity ID."))
                    ImGui::SetClipboardText(selectedEdge->fromId.c_str());
                ImGui::SameLine();
                if (drawSmallUiButton("Copy Target ID", UiButtonRole::Advanced, "Copy the target entity ID."))
                    ImGui::SetClipboardText(selectedEdge->toId.c_str());
            }
        }
        else
        {
            ImGui::SeparatorText(localizedUiLabel("Selected Relationship").c_str());
            ImGui::TextDisabled(
                localizedUiLabel("Select a relationship to inspect source, target, and raw IDs.").c_str());
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }

    ImGui::End();
}
