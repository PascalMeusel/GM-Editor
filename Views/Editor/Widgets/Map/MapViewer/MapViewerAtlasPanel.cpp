#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewInternal.hpp"

using namespace gm::map_viewer_detail;

void MapViewerView::drawAtlasSidePanel(MapModel &map)
{
    const bool creatingDetail = _controller._interactionMode == MapViewerInteractionMode::SelectDetailArea;
    if (!creatingDetail)
    {
    ImGui::SeparatorText(localizedUiLabel("Atlas").c_str());
    if (!_controller._atlasStatusMessage.empty())
        ImGui::TextWrapped("%s", _controller._atlasStatusMessage.c_str());
    const std::vector<MapAtlasHierarchyNode> hierarchy = MapAtlasService::buildHierarchy(_controller._currentCampaign.maps);
    const MapModel *world = MapAtlasService::primaryWorldMap(_controller._currentCampaign.maps);
    const EntityId worldId = world ? world->metadata.id : EntityId{};
    if (!worldId.empty() && map.metadata.id != worldId &&
        drawUiButton(uiTextForLocale("Open world map", "Weltkarte öffnen"), UiButtonRole::Secondary,
                     ImVec2(-1.0f, 0.0f)))
        _controller.selectMapById(worldId);
    ImGui::TextWrapped("%s", uiTextForLocale(
        "Maps are nested by place: world, region, site, then encounter. Select an area on a parent to create or place a linked detail map.",
        "Karten liegen ineinander: Welt, Region, Ort, dann Begegnung. Wähle einen Bereich auf der übergeordneten Karte, um eine Detailkarte zu erstellen oder dort zu platzieren."));
    if (world && !world->isMainMap)
        ImGui::TextWrapped(uiTextForLocale(
            "No main world is designated. Showing %s as the atlas root; mark it Main Map in the map editor to make this permanent.",
            "Keine Hauptweltkarte festgelegt. %s wird als Atlaswurzel angezeigt; markiere sie im Karteneditor als Hauptkarte, um dies dauerhaft festzulegen."),
            world->name.c_str());
    std::function<void(const MapAtlasHierarchyNode &)> drawNode;
    drawNode = [&](const MapAtlasHierarchyNode &node) {
        const bool selected = node.mapId == map.metadata.id;
        const auto model = std::find_if(_controller._currentCampaign.maps.begin(), _controller._currentCampaign.maps.end(),
                                        [&](const MapModel &candidate) { return candidate.metadata.id == node.mapId; });
        const bool unpositioned = model != _controller._currentCampaign.maps.end() &&
                                  !MapAtlasService::effectiveParentId(*model).empty() &&
                                  model->atlasPlacement.parentId.empty();
        std::string label = (node.name.empty() ? localizedUiLabel("Untitled map") : node.name) +
                            "  [" + localizedUiLabel(atlasLevelLabel(node.level)) + "]";
        if (unpositioned)
            label += uiTextForLocale(" · location not set", " · Position fehlt");
        ImGui::PushID(node.mapId.c_str());
        if (node.children.empty())
        {
            if (ImGui::Selectable(label.c_str(), selected))
                _controller.selectMapById(node.mapId);
        }
        else
        {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (selected)
                flags |= ImGuiTreeNodeFlags_Selected;
            const bool open = ImGui::TreeNodeEx("##AtlasNode", flags, "%s", label.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                _controller.selectMapById(node.mapId);
            if (open)
            {
                for (const MapAtlasHierarchyNode &child : node.children)
                    drawNode(child);
                ImGui::TreePop();
            }
        }
        ImGui::PopID();
    };
    const auto worldRoot = std::find_if(hierarchy.begin(), hierarchy.end(),
                                        [&](const MapAtlasHierarchyNode &root) { return root.mapId == worldId; });
    if (worldRoot != hierarchy.end())
        drawNode(*worldRoot);
    int looseCount = static_cast<int>(hierarchy.size()) - (worldRoot == hierarchy.end() ? 0 : 1);
    if (looseCount > 0 && ImGui::TreeNodeEx("##OtherAtlasMaps", ImGuiTreeNodeFlags_DefaultOpen,
                                            "%s (%d)", uiTextForLocale("Other maps (unplaced)", "Weitere Karten (ohne Ort)"), looseCount))
    {
        for (const MapAtlasHierarchyNode &root : hierarchy)
            if (worldRoot == hierarchy.end() || root.mapId != worldRoot->mapId)
                drawNode(root);
        ImGui::TreePop();
    }

    if (map.atlasLevel == MapLevel::World &&
        ImGui::TreeNode(uiTextForLocale("Expand world map / Advanced", "Weltkarte erweitern / Erweitert")))
    {
        drawWorldExpansionPanel(map);
        ImGui::TreePop();
    }

    ImGui::Spacing();
    int visibility = static_cast<int>(map.visibility);
    const char *visibilityItems = "GM only\0Players know it\0Visible to players\0";
    if (FormUi::Combo(localizedUiLabel("Visibility").c_str(), &visibility, localizedComboItems(visibilityItems).c_str()))
    {
        map.visibility = static_cast<MapVisibility>(std::clamp(visibility, 0, 2));
        _controller.saveCampaign();
    }
    }

    if (creatingDetail)
    {
        ImGui::SeparatorText(localizedUiLabel("Create detail map").c_str());
        if (!_controller._atlasStatusMessage.empty())
            ImGui::TextWrapped("%s", _controller._atlasStatusMessage.c_str());
        if (!_controller.hasDetailSelection())
        {
            ImGui::TextWrapped(localizedUiLabel("Drag an area on the map. Rivers, roads, settlements, and terrain inside it become locked anchors for the new detail map.").c_str());
        }
        else
        {
            ImGui::TextUnformatted(localizedUiLabel("Area selected").c_str());
            if (ImGui::TreeNode(uiTextForLocale("Use an existing map", "Vorhandene Karte nutzen")))
            {
                drawExistingMapPlacementPanel(map);
                ImGui::TreePop();
            }
            ImGui::TextUnformatted(uiTextForLocale("Map name", "Kartenname"));
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputText("##AtlasDetailName", &_controller._detailMapName);

            const std::string targetLabel = localizedUiLabel(atlasLevelLabel(_controller._detailTargetLevel));
            ImGui::TextWrapped(localizedUiLabel(_controller._detailAdvancedLevelOverride
                                                    ? "Advanced target level: %s"
                                                    : "Next atlas level: %s")
                                   .c_str(),
                               targetLabel.c_str());
            if (beginAdvancedTreeNode(uiTextForLocale("Change detail level", "Detailebene ändern"), "Normally a detail map moves exactly one level deeper. Open this only when an intentional level jump is needed."))
            {
                std::vector<MapLevel> allowedLevels;
                for (int depth = MapAtlasService::levelDepth(map.atlasLevel) + 1;
                     depth <= MapAtlasService::levelDepth(MapLevel::Tactical); ++depth)
                    allowedLevels.push_back(static_cast<MapLevel>(depth));
                int selectedLevel = 0;
                for (int index = 0; index < static_cast<int>(allowedLevels.size()); ++index)
                    if (allowedLevels[static_cast<std::size_t>(index)] == _controller._detailTargetLevel)
                        selectedLevel = index;
                std::vector<std::string> levelNames;
                std::vector<const char *> levelLabels;
                for (MapLevel level : allowedLevels)
                    levelNames.push_back(localizedUiLabel(atlasLevelLabel(level)));
                for (const std::string &name : levelNames)
                    levelLabels.push_back(name.c_str());
                if (!levelLabels.empty() && FormUi::Combo(localizedUiLabel("Detail level").c_str(), &selectedLevel,
                                                          levelLabels.data(), static_cast<int>(levelLabels.size())))
                {
                    const MapLevel chosenLevel = allowedLevels[static_cast<std::size_t>(selectedLevel)];
                    _controller.setDetailTargetLevel(
                        chosenLevel, !MapAtlasService::isNextLevel(chosenLevel, map.atlasLevel));
                }
                ImGui::TreePop();
            }

            if (_controller._detailGenerationRunning)
            {
                const int completed = _controller._detailGenerationProgress
                                          ? _controller._detailGenerationProgress->step.load(std::memory_order_relaxed)
                                          : 0;
                const int total = _controller._detailGenerationProgress
                                      ? std::max(1, _controller._detailGenerationProgress->totalSteps.load(std::memory_order_relaxed))
                                      : 24;
                const float fraction = std::clamp(static_cast<float>(completed) / static_cast<float>(total), 0.0f, 1.0f);
                ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f));
                ImGui::TextDisabled("%s", localizedUiLabel("Checking deterministic designs...").c_str());
                if (drawUiButton(localizedUiLabel("Cancel generation").c_str(), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                                 "Discard this background result without changing either atlas map."))
                    _controller.cancelDetailGeneration();
            }
            else if (drawUiButton(localizedUiLabel("Generate 3 Designs").c_str(), UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                                  "Evaluate 24 deterministic layouts in the background and show only valid designs."))
            {
                _controller.generateDetailCandidates();
            }

            if (!_controller._detailCandidates.empty())
            {
                if (drawUiButton(uiTextForLocale("Compare designs", "Entwürfe vergleichen"),
                                 UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
                    _openDetailDesignReview = true;
                ImGui::TextWrapped("%s", uiTextForLocale(
                    "Review all designs in the large comparison window before creating a map.",
                    "Prüfe alle Entwürfe im großen Vergleichsfenster, bevor du eine Karte erstellst."));
            }
        }
        // Creating a child can reallocate the campaign's map vector. Besides
        // keeping this panel focused, returning avoids reading the old map.
        return;
    }

    const EntityId parentId = MapAtlasService::effectiveParentId(map);
    if (!parentId.empty() && !map.atlasPlacement.parentId.empty())
    {
        ImGui::SeparatorText(localizedUiLabel("Atlas synchronization").c_str());
        const MapAtlasStaleReport stale = MapAtlasService::detectStale(map, _controller._currentCampaign.maps);
        ImGui::TextWrapped(localizedUiLabel(stale.stale ? "The parent or detail map changed. Review before synchronizing." : "This detail map is linked to the current parent revision.").c_str());
        if (stale.parentChanged && drawUiButton(
                                       localizedUiLabel("Review parent update").c_str(), UiButtonRole::Primary,
                                       ImVec2(-1.0f, 0.0f),
                                       "Compare inherited map content before changing this detail map."))
            _controller.prepareRebasePreview();
        if (_controller._hasRebasePreview)
        {
            ImGui::TextWrapped("%s", localizedUiLabel("Choose what should happen to every changed inherited map item. Nothing is applied until you confirm.").c_str());
            for (const std::string &error : _controller._rebasePreview.errors)
            {
                const std::string localizedError = gm::localization::trSystemMessage(error);
                ImGui::BulletText("%s", localizedError.c_str());
            }
            for (const std::string &warning : _controller._rebasePreview.warnings)
            {
                const std::string localizedWarning = gm::localization::trSystemMessage(warning);
                ImGui::BulletText("%s", localizedWarning.c_str());
            }
            static const char *resolutionKeys[] = {"Please choose", "Use parent", "Keep local", "Detach"};
            for (std::size_t index = 0; index < _controller._rebasePreview.items.size(); ++index)
            {
                const MapAtlasRebaseItem &item = _controller._rebasePreview.items[index];
                if (!item.requiresDecision)
                    continue;
                ImGui::PushID(static_cast<int>(index));
                const char *itemKindKey = item.kind == MapAtlasRebaseItemKind::Region
                                              ? "Region"
                                              : (item.kind == MapAtlasRebaseItemKind::Route
                                                     ? "Route"
                                                     : "Feature");
                const char *unnamedKey = item.kind == MapAtlasRebaseItemKind::Region
                                             ? "Unnamed region"
                                             : (item.kind == MapAtlasRebaseItemKind::Route
                                                    ? "Unnamed route"
                                                    : "Unnamed feature");
                std::string name;
                if (item.parentFeature && !item.parentFeature->name.empty())
                    name = item.parentFeature->name;
                else if (item.localFeature && !item.localFeature->name.empty())
                    name = item.localFeature->name;
                else if (item.parentRegion && !item.parentRegion->name.empty())
                    name = item.parentRegion->name;
                else if (item.localRegion && !item.localRegion->name.empty())
                    name = item.localRegion->name;
                else if (item.parentRoute && !item.parentRoute->name.empty())
                    name = item.parentRoute->name;
                else if (item.localRoute && !item.localRoute->name.empty())
                    name = item.localRoute->name;
                else
                    name = localizedUiLabel(unnamedKey);
                ImGui::TextWrapped("%s", name.c_str());
                const char *changeKey = item.change == MapAtlasRebaseChange::AddedByParent
                                            ? "Added on parent"
                                            : (item.change == MapAtlasRebaseChange::RemovedFromParent
                                                   ? "Removed from parent"
                                                   : "Changed on parent");
                const std::string itemKindLabel = localizedUiLabel(itemKindKey);
                const std::string changeLabel = localizedUiLabel(changeKey);
                ImGui::TextDisabled("%s · %s", itemKindLabel.c_str(), changeLabel.c_str());
                std::vector<std::string> localizedResolutions;
                std::vector<const char *> resolutionLabels;
                for (const char *key : resolutionKeys)
                    localizedResolutions.push_back(localizedUiLabel(key));
                for (const std::string &label : localizedResolutions)
                    resolutionLabels.push_back(label.c_str());
                int comboIndex = _controller._rebaseResolutionIndices[index] + 1;
                if (FormUi::Combo("##RebaseResolution", &comboIndex,
                                 resolutionLabels.data(), static_cast<int>(resolutionLabels.size())))
                    _controller._rebaseResolutionIndices[index] = comboIndex - 1;
                ImGui::Separator();
                ImGui::PopID();
            }
            const bool rebaseReady = _controller.rebasePreviewReadyToApply();
            if (drawUiButton(localizedUiLabel("Apply parent update").c_str(), rebaseReady,
                             rebaseReady
                                 ? nullptr
                                 : "Choose what should happen to every changed inherited map item. Nothing is applied until you confirm.",
                             UiButtonRole::Primary,
                             ImVec2(-1.0f, 0.0f),
                             "Apply all selected resolutions as one confirmed atlas change."))
                _controller.applyRebasePreview();
        }
        if (drawUiButton(localizedUiLabel("Promote to atlas").c_str(), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                         "Prepare a non-destructive preview of simplified detail features on the parent map."))
            _controller.preparePromotionPreview();
        if (_controller._hasPromotionPreview)
        {
            ImGui::Text(localizedUiLabel("%d features ready, %d conflicts").c_str(),
                        static_cast<int>(_controller._promotionPreview.promotedFeatures.size()),
                        static_cast<int>(_controller._promotionPreview.conflicts.size()));
            for (const std::string &error : _controller._promotionPreview.errors)
            {
                const std::string localizedError = gm::localization::trSystemMessage(error);
                ImGui::BulletText("%s", localizedError.c_str());
            }
            for (const std::string &warning : _controller._promotionPreview.warnings)
            {
                const std::string localizedWarning = gm::localization::trSystemMessage(warning);
                ImGui::BulletText("%s", localizedWarning.c_str());
            }
            for (const MapAtlasPromotionConflict &conflict : _controller._promotionPreview.conflicts)
            {
                const std::string localizedConflict = gm::localization::trSystemMessage(conflict.message);
                ImGui::BulletText("%s", localizedConflict.c_str());
            }
            if (drawUiButton(localizedUiLabel("Apply promotion").c_str(), _controller._promotionPreview.valid,
                             "Resolve conflicts before applying the promotion.", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                             "Add the previewed features to the parent and update atlas revisions."))
                _controller.applyPromotionPreview();
        }
    }
}
