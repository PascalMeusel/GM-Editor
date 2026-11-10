#include "StoryWorkspaceViewSupport.hpp"

void StoryWorkspaceView::drawLinkEditor(bool forBeat)
{
    StoryLinkModel &draft = forBeat ? _controller._draftBeatLink : _controller._draftThreadLink;
    std::string &targetType = forBeat ? _controller._beatLinkTargetType : _controller._threadLinkTargetType;
    std::string &targetSearch = forBeat ? _controller._beatLinkTargetSearch : _controller._threadLinkTargetSearch;
    EntityId &selectedTargetId = forBeat ? _controller._beatLinkSelectedTargetId : _controller._threadLinkSelectedTargetId;
    int &targetIndex = forBeat ? _controller._beatLinkTargetIndex : _controller._threadLinkTargetIndex;

    ImGui::PushID(forBeat ? "BeatLinkEditor" : "ThreadLinkEditor");
    drawLinkTypeCombo("Connection Meaning", draft.type);
    FormUi::InputText(localizedUiLabel("Short Note").c_str(), &draft.label);
    showHelpTooltip("A plain-language note, such as 'key bearer', 'ambush site', or 'reveals the shrine'.");

    ImGui::SeparatorText(localizedUiLabel("Find Target").c_str());
    FormUi::InputText(localizedUiLabel("Search Targets").c_str(), &targetSearch);
    showHelpTooltip("Search the current target type by name or stable ID before choosing a link target.");

    const std::vector<std::string> entityTypes = _controller.getEntityTypes();
    if (FormUi::BeginCombo(localizedUiLabel("Connect To").c_str(), localizedUiText(targetType.c_str())))
    {
        for (const std::string &type : entityTypes)
        {
            const bool selected = targetType == type;
            if (ImGui::Selectable(localizedUiText(type.c_str()), selected))
            {
                targetType = type;
                selectedTargetId.clear();
                targetIndex = -1;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const std::vector<StoryEntityOption> targets = _controller.getEntityOptions(targetType);
    const StoryEntityOption recentTarget = _controller.getRecentEntityOption(targetType);
    const bool hasRecentTarget = !recentTarget.id.empty();
    const std::string localizedTargetType = localizedUiText(targetType.c_str());
    const std::string recentDisabledReason = std::string(localizedUiText("Open a")) + " " + localizedTargetType + " " +
                                             localizedUiText("from Search, Command Palette, or Favorites before using this shortcut.");
    const std::string recentTooltip = hasRecentTarget
                                          ? std::string(localizedUiText("Select recent")) + " " + localizedTargetType + ": " + recentTarget.label
                                          : localizedUiText("Select the most recently opened compatible entity.");
    if (drawUiButton("Use Recent Entity",
                     hasRecentTarget,
                     recentDisabledReason.c_str(),
                     UiButtonRole::Secondary,
                     ImVec2(-1.0f, 0.0f),
                     recentTooltip.c_str()))
    {
        selectedTargetId = recentTarget.id;
        targetSearch.clear();
        targetIndex = -1;
    }
    if (hasRecentTarget)
    {
        const std::string recentHelp = std::string(localizedUiText("Recent")) + " " + localizedTargetType + ": " + recentTarget.label;
        drawMutedHelpText(recentHelp.c_str(), "Shortcut uses the most recently opened compatible campaign entity.");
    }
    std::vector<int> filteredTargetIndices;
    filteredTargetIndices.reserve(targets.size());
    for (int i = 0; i < static_cast<int>(targets.size()); ++i)
    {
        if (storyEntityMatchesSearch(targets[static_cast<std::size_t>(i)], targetSearch))
            filteredTargetIndices.push_back(i);
    }

    const StoryEntityOption *selectedTarget = nullptr;
    for (int i = 0; i < static_cast<int>(targets.size()); ++i)
    {
        const StoryEntityOption &target = targets[static_cast<std::size_t>(i)];
        if (target.id == selectedTargetId)
        {
            selectedTarget = &target;
            targetIndex = i;
            break;
        }
    }
    if (!selectedTargetId.empty() && !selectedTarget)
    {
        selectedTargetId.clear();
        targetIndex = -1;
    }
    const bool selectedTargetVisible =
        selectedTarget &&
        std::find_if(filteredTargetIndices.begin(), filteredTargetIndices.end(), [&](int filteredIndex) {
            return targets[static_cast<std::size_t>(filteredIndex)].id == selectedTargetId;
        }) != filteredTargetIndices.end();

    const std::string selectedName = selectedTarget ? selectedTarget->label : localizedUiText("No target selected");
    const std::string resultSummary =
        std::string(localizedUiText("Type:")) + " " + localizedTargetType + " | " + localizedUiText("Results:") + " " +
        std::to_string(filteredTargetIndices.size()) + " " + localizedUiText("of") + " " + std::to_string(targets.size());
    UiStatusActionStrip pickerStrip;
    pickerStrip.mode = "Link Target";
    pickerStrip.selection = selectedName.c_str();
    pickerStrip.nextAction = resultSummary.c_str();
    pickerStrip.help = "Pick a visible target row before adding a connection. Filtering never chooses a target automatically.";
    drawStatusActionStrip(forBeat ? "BeatLinkTargetStatus" : "ThreadLinkTargetStatus", pickerStrip);

    ImGui::BeginChild("TargetResults", ImVec2(0.0f, 132.0f), true);
    if (targets.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No %s targets exist yet.").c_str(), localizedTargetType.c_str());
    }
    else if (filteredTargetIndices.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No %s targets match this search.").c_str(), localizedTargetType.c_str());
    }
    else if (ImGui::BeginTable("TargetResultTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Name").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Type").c_str(), ImGuiTableColumnFlags_WidthFixed, 82.0f);
        ImGui::TableHeadersRow();
        for (int filteredIndex : filteredTargetIndices)
        {
            const StoryEntityOption &target = targets[static_cast<std::size_t>(filteredIndex)];
            const bool selected = selectedTargetId == target.id;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(target.label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
            {
                selectedTargetId = target.id;
                targetIndex = filteredIndex;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
            ImGui::TableNextColumn();
            ImGui::TextDisabled(localizedUiLabel("%s").c_str(), localizedUiText(target.type.c_str()));
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::TextDisabled(localizedUiLabel("Selected Target").c_str());
    if (selectedTarget)
        ImGui::TextWrapped(localizedUiLabel("%s: %s").c_str(), localizedUiText(selectedTarget->type.c_str()), selectedTarget->label.c_str());
    else
        ImGui::TextWrapped(localizedUiLabel("Choose a target from the results before adding this connection.").c_str());

    const bool canAddLink = selectedTarget != nullptr && selectedTargetVisible;
    std::string disabledReason = localizedUiText("Select a target from the results first.");
    if (targets.empty())
        disabledReason = std::string(localizedUiText("Create a")) + " " + localizedTargetType + " " + localizedUiText("before linking to one.");
    else if (filteredTargetIndices.empty())
        disabledReason = localizedUiText("No targets match the current search.");
    else if (selectedTarget && !selectedTargetVisible)
        disabledReason = localizedUiText("The selected target is hidden by the current search.");

    if (drawUiButton(
            forBeat ? "Add Scene Connection" : "Add Arc Connection",
            canAddLink,
            disabledReason.c_str(),
            UiButtonRole::Primary,
            ImVec2(-1.0f, 0.0f),
            "Add the selected target as a story connection."))
    {
        if (forBeat)
            _controller.addBeatLink();
        else
            _controller.addThreadLink();
    }
    ImGui::PopID();
}

void StoryWorkspaceView::drawLinks(bool forBeat)
{
    StoryThreadModel *thread = _controller.getSelectedThread();
    StoryBeatModel *beat = _controller.getSelectedBeat();
    std::vector<StoryLinkModel> *links = forBeat ? (beat ? &beat->links : nullptr) : (thread ? &thread->links : nullptr);
    if (!links)
        return;

    if (links->empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No connections yet.").c_str());
        return;
    }

    if (ImGui::BeginTable(forBeat ? "BeatLinks" : "ThreadLinks", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Meaning").c_str(), ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Note").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Connected To").c_str());
        ImGui::TableSetupColumn("##LinkActions", ImGuiTableColumnFlags_WidthFixed,
            ImGui::CalcTextSize(localizedUiText("Remove")).x + ImGui::GetStyle().FramePadding.x * 2.0f);
        ImGui::TableHeadersRow();

        for (std::size_t i = 0; i < links->size(); ++i)
        {
            StoryLinkModel &link = (*links)[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(localizedUiText(storyLinkTypeLabel(link.type)));
            ImGui::TableNextColumn();
            tableTextWrapped(link.label);
            ImGui::TableNextColumn();
            const std::string target = std::string(localizedUiText(link.targetEntityType.c_str())) + ": " + _controller.describeEntity(link);
            tableTextWrapped(target);
            ImGui::TableNextColumn();
            if (drawSmallUiButton("Remove##StoryLink", UiButtonRole::Destructive, "Remove this story connection."))
            {
                if (forBeat)
                    _controller.deleteBeatLink(i);
                else
                    _controller.deleteThreadLink(i);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void StoryWorkspaceView::drawThreadTypeCombo(const char *label, StoryThreadType &type)
{
    const std::string localizedLabel = localizedUiLabel(label);
    if (FormUi::BeginCombo(localizedLabel.c_str(), localizedUiText(storyThreadTypeLabel(type))))
    {
        for (int i = 0; i < kThreadTypeCount; ++i)
        {
            const StoryThreadType candidate = static_cast<StoryThreadType>(i);
            const bool selected = type == candidate;
            if (ImGui::Selectable(localizedUiText(storyThreadTypeLabel(candidate)), selected))
                type = candidate;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void StoryWorkspaceView::drawStatusCombo(const char *label, StoryStatus &status)
{
    const std::string localizedLabel = localizedUiLabel(label);
    if (FormUi::BeginCombo(localizedLabel.c_str(), localizedUiText(storyStatusLabel(status))))
    {
        for (int i = 0; i < kStoryStatusCount; ++i)
        {
            const StoryStatus candidate = static_cast<StoryStatus>(i);
            const bool selected = status == candidate;
            if (ImGui::Selectable(localizedUiText(storyStatusLabel(candidate)), selected))
                status = candidate;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void StoryWorkspaceView::drawLinkTypeCombo(const char *label, StoryLinkType &type)
{
    const std::string localizedLabel = localizedUiLabel(label);
    if (FormUi::BeginCombo(localizedLabel.c_str(), localizedUiText(storyLinkTypeLabel(type))))
    {
        for (int i = 0; i < kStoryLinkTypeCount; ++i)
        {
            const StoryLinkType candidate = static_cast<StoryLinkType>(i);
            const bool selected = type == candidate;
            if (ImGui::Selectable(localizedUiText(storyLinkTypeLabel(candidate)), selected))
                type = candidate;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}
