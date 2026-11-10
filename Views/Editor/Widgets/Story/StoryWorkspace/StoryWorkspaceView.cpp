#include "StoryWorkspaceViewSupport.hpp"

StoryWorkspaceView::StoryWorkspaceView(StoryWorkspaceController &controller)
    : DockableWidget("Story Workspace", 0, ImVec2(1120.0f, 760.0f)), _controller(controller)
{
}

void StoryWorkspaceView::draw()
{
    const float unit = std::max(1.0f, ImGui::GetFontSize() / 16.0f);
    const float width = std::min(ImGui::GetContentRegionAvail().x, 980.0f * unit);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (ImGui::GetContentRegionAvail().x - width) * 0.5f));
    ImGui::BeginChild("FocusedStoryWorkspace", ImVec2(width, 0));
    const StoryThreadModel *selectedThread = _controller.getSelectedThread();
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::BeginCombo("##StoryArcSelection", selectedThread ? selectedThread->title.c_str() : localizedUiText("No arc selected")))
    {
        for (int index = 0; index < static_cast<int>(_controller._currentCampaign.storyThreads.size()); ++index)
        {
            const auto &thread = _controller._currentCampaign.storyThreads[index];
            if (ImGui::Selectable(thread.title.c_str(), index == _controller._selectedThreadIndex))
            {
                _controller._selectedThreadIndex = index;
                _controller._selectedBeatIndex = thread.beats.empty() ? -1 : 0;
            }
        }
        ImGui::EndCombo();
    }
    if (drawUiButton("New Story Arc", UiButtonRole::Secondary)) ImGui::OpenPopup("StoryArcBrowser");
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(width, 420.0f * unit), 0),
        ImVec2(std::min(width, 720.0f * unit), ImGui::GetMainViewport()->WorkSize.y * 0.85f));
    if (ImGui::BeginPopup("StoryArcBrowser"))
    {
        drawThreadBrowser();
        ImGui::EndPopup();
    }
    if (!_controller._errorMessage.empty())
        ImGui::TextWrapped("%s", _controller._errorMessage.c_str());
    if (_controller._saveFailed && drawUiButton("Retry Save", UiButtonRole::Primary))
        _controller.saveCampaign();
    if (ImGui::BeginTabBar("StoryEditingTabs"))
    {
        if (ImGui::BeginTabItem(localizedUiLabel("Scenes").c_str()))
        {
            drawBeatBoard();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Arc Detail").c_str()))
        {
            drawThreadEditor();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

void StoryWorkspaceView::drawThreadBrowser()
{
    drawHelpText("New Story Arc", "Create a long-running narrative line. Add scenes and connections after creating it.");
    FormUi::InputText(localizedUiLabel("Arc Title##NewThread").c_str(), &_controller._draftThread.title);
    showHelpTooltip("A short name you will recognize in lists, such as 'The Locked Gate' or 'Cass and the Smugglers'.");
    drawThreadTypeCombo("Arc Kind##NewThread", _controller._draftThread.type);
    if(ImGui::TreeNode(uiTextForLocale("Optional state","Optionaler Status"))) {
        drawStatusCombo("Current State##NewThread", _controller._draftThread.status);ImGui::TreePop();
    }
    ImGui::TextUnformatted(localizedUiText("Pitch"));
    FormUi::InputTextMultiline("##NewThreadPitch", &_controller._draftThread.summary, ImVec2(-1.0f, 120.0f), ImGuiInputTextFlags_WordWrap);
    showHelpTooltip("One or two sentences describing what this arc is about.");
    if (_controller._saveFailed) ImGui::TextWrapped("%s", _controller._errorMessage.c_str());
    if (drawUiButton(_controller._saveFailed ? "Retry Save" : "Create Story Arc", !_controller._draftThread.title.empty(), "Name is required.", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
    {
        _controller.addThread();
        if (_controller._errorMessage.empty()) ImGui::CloseCurrentPopup();
    }
}

void StoryWorkspaceView::drawThreadEditor()
{
    StoryThreadModel *thread = _controller.getSelectedThread();
    if (!thread)
    {
        drawHelpText("Story Arc Detail", "Select an arc to edit its purpose, stakes, notes, scenes, and campaign connections.");
        ImGui::Separator();
        drawWrappedMuted("Select or create a story arc. Arcs are the long-running narrative lines of the campaign.");
        return;
    }

    drawHelpText("Story Arc Detail", "This is the selected long-running story line. Use it to keep the GM-facing shape of the arc clear.");
    ImGui::Separator();
    FormUi::InputText(localizedUiLabel("Arc Title").c_str(), &thread->title);
    drawThreadTypeCombo("Arc Kind", thread->type);
    drawStatusCombo("Current State", thread->status);

    drawHelpText("Pitch", "The short version of this arc: what it is, who it affects, and why it matters.");
    FormUi::InputTextMultiline(localizedUiLabel("##ThreadSummary").c_str(), &thread->summary, ImVec2(-1.0f, 92.0f), ImGuiInputTextFlags_WordWrap);

    drawHelpText("Stakes", "What changes if the players engage with, ignore, succeed at, or fail this arc.");
    FormUi::InputTextMultiline(localizedUiLabel("##ThreadStakes").c_str(), &thread->stakes, ImVec2(-1.0f, 88.0f), ImGuiInputTextFlags_WordWrap);

    drawHelpText("Private GM Notes", "Your prep notes, secrets, intended twists, and reminders. Treat this as behind-the-screen text.");
    FormUi::InputTextMultiline(localizedUiLabel("##ThreadNotes").c_str(), &thread->gmNotes, ImVec2(-1.0f, 110.0f), ImGuiInputTextFlags_WordWrap);

    if (ImGui::Button(localizedUiLabel("Save Story Arc").c_str()))
    {
        thread->metadata.touch();
        _controller.saveCampaign();
    }
    ImGui::SameLine();
    if (ImGui::Button(localizedUiLabel("Delete Arc").c_str()))
        ImGui::OpenPopup(localizedUiLabel("Delete Story Thread").c_str());

    if (ImGui::BeginPopupModal(localizedUiLabel("Delete Story Thread").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(localizedUiLabel("Delete this story arc and all of its scenes?").c_str());
        if (drawModalActionButtons("Delete", UiButtonRole::Destructive, "Cancel", 110.0f))
            _controller.deleteSelectedThread();
        ImGui::EndPopup();
    }

    ImGui::SeparatorText(localizedUiLabel("Arc Connections").c_str());
    showHelpTooltip("Connections tie this arc to players, NPCs, maps, items, encounters, monsters, or audio.");
    drawLinks(false);
    if (ImGui::TreeNode(localizedUiLabel("Add Arc Connection").c_str()))
    {
        drawLinkEditor(false);
        ImGui::TreePop();
    }
}
