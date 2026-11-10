#include "Views/Editor/EditorNavigationSupport.hpp"

namespace
{
std::string searchResultTypeLabel(const CampaignSearchDocument &document)
{
    if (document.type == "Objective") return uiTextForLocale("Objective", "Ziel");
    if (document.type == "Clue") return uiTextForLocale("Clue", "Hinweis");
    return localizedUiLabel(document.type.c_str());
}
}

bool EditorWindowView::activateWorkspace(EditorWorkspace workspace, bool rememberReturn)
{
    if(!_controller.checkpointEditorDraftRecoveryForLeave())return false;
    if(_activeWorkspace!=workspace&&!checkpointWorldRecovery(true))return false;
    _controller.captureWorkspaceLocation();
    if (!_controller._navigation.navigate(workspace, rememberReturn, [this]() {
        return _activeWorkspace != EditorWorkspace::Home || _controller.flushPreparationDrafts("leave session preparation");
    })) return false;
    if (_activeWorkspace == workspace)
    {
        ImGui::SetWindowFocus(workspace == EditorWorkspace::Home ? "Campaign Home" : "Workspace");
        return true;
    }

    if (workspace == EditorWorkspace::Home || workspace == EditorWorkspace::Live)
    {
        _returnToLiveContext = false;
        _campaignNavigationOpen = false;
    }
    else if (_activeWorkspace == EditorWorkspace::Live)
        _returnToLiveContext = true;
    _controller.hideAllWorkspaceTools();
    _activeWorkspace = workspace;
    _controller.restoreWorkspaceLocation();
    if (workspace != EditorWorkspace::Live)
        _presentationPanelOpen = false;
    _activeSidebarWorkflowLabel = editorWorkspaceDefinition(workspace).label;
    if (workspace == EditorWorkspace::Home)
    {
        _focusCampaignHomeNextFrame = true;
        ImGui::SetWindowFocus("Campaign Home");
    }
    else
    {
        ImGui::SetWindowFocus("Workspace");
    }
    return true;
}

void EditorWindowView::renderUniversalSearch()
{
    if (!_universalSearchOpen)
        return;

    setResponsiveWindowSize(ImVec2(760.0f, 580.0f), ImVec2(480.0f, 360.0f));
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (_focusUniversalSearchNextFrame) ImGui::SetNextWindowFocus();
    if (!ImGui::Begin(localizedUiLabel("Search and Commands").c_str(), &_universalSearchOpen,
                      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        _universalSearchOpen = false;
        _focusUniversalSearchNextFrame = false;
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::IsWindowAppearing() || _focusUniversalSearchNextFrame)
        ImGui::SetKeyboardFocusHere();
    _focusUniversalSearchNextFrame = false;
    const bool activateFirst =
        FormUi::InputTextWithHint(localizedUiLabel("##UniversalSearchQuery").c_str(),
                                  localizedUiText("Search story, objectives, clues, character, map, encounter, item, spell, audio..."),
                                  &_commandPaletteQuery, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::TextDisabled(
        localizedUiLabel("%s").c_str(),
        uiTextForLocale("Enter opens the first result · Esc closes", "Enter öffnet den ersten Treffer · Esc schließt"));
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::BeginChild("UniversalSearchResults", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);

    const std::string query = lowerCopy(_commandPaletteQuery);
    const std::vector<CampaignCommandPaletteItem> commands = _controller.commandPaletteItems(_commandPaletteQuery);
    const std::vector<CampaignSearchResult> entities =
        _commandPaletteQuery.empty() ? std::vector<CampaignSearchResult>{}
                                     : _controller.searchCurrentCampaign(_commandPaletteQuery, 12);

    std::vector<const CampaignCommandPaletteItem *> actionCommands;
    for (const CampaignCommandPaletteItem &item : commands)
    {
        // Workspace routes are already presented below, and open:* entries are
        // rendered from ranked entity results. Omitting both avoids duplicates.
        if (item.id.rfind("workspace:", 0) == 0 || item.id.rfind("open:", 0) == 0)
            continue;
        actionCommands.push_back(&item);
    }

    const bool exactTableToolsCommand = !actionCommands.empty() &&
        (actionCommands.front()->id == "action:open_objectives" ||
         actionCommands.front()->id == "action:open_clues");
    bool showedResult = false;
    const auto drawActions = [&]() {
        if (actionCommands.empty())
            return;
        ImGui::SeparatorText(uiTextForLocale("Actions", "Aktionen"));
        int shownCommands = 0;
        for (const CampaignCommandPaletteItem *item : actionCommands)
        {
            if (shownCommands++ >= 8)
                break;
            const std::string label = item->label + "##command" + item->id;
            if (drawUiButton(label.c_str(), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), item->category.c_str()) ||
                (activateFirst && !showedResult))
            {
                _controller.executePaletteCommand(*item);
                _universalSearchOpen = false;
            }
            showedResult = true;
        }
    };

    // Exact Objectives/Clues commands are more specific than a workspace
    // description containing those words. Render them first so Enter follows
    // the visible first result into the requested inner toolkit tab.
    if (exactTableToolsCommand)
        drawActions();

    ImGui::SeparatorText(uiTextForLocale("Workspaces", "Arbeitsbereiche"));
    for (const EditorWorkspaceDefinition &definition : kEditorWorkspaces)
    {
        const char *workspaceLabel = editorWorkspaceLabel(definition.workspace);
        const char *workspaceDescription = editorWorkspaceDescription(definition.workspace);
        if (!query.empty() && !containsText(workspaceLabel, query) && !containsText(workspaceDescription, query))
            continue;
        const std::string label = std::string(gm::localization::tr(gm::localization::TextId::CommonOpen)) + " " +
                                  workspaceLabel + "##workspace";
        if (drawUiButton(label.c_str(), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), workspaceDescription) ||
            (activateFirst && !showedResult))
        {
            activateWorkspace(definition.workspace);
            _universalSearchOpen = false;
        }
        showedResult = true;
    }

    if (!exactTableToolsCommand)
        drawActions();

    if (!entities.empty())
        ImGui::SeparatorText(uiTextForLocale("Campaign results", "Kampagnentreffer"));
    for (const CampaignSearchResult &result : entities)
    {
        const CampaignSearchDocument &document = result.document;
        const std::string label =
            document.title + "  /  " + searchResultTypeLabel(document) + "##entity" + document.id;
        if (drawUiButton(label.c_str(), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), document.subtitle.c_str()) ||
            (activateFirst && !showedResult))
        {
            CampaignCommandPaletteItem item{"open:" + document.id, "Open " + document.title, document.type, document.id,
                                            document.type};
            _controller.executePaletteCommand(item);
            _universalSearchOpen = false;
        }
        showedResult = true;
    }

    if (!showedResult)
        WorkspaceUi::emptyState(
            "No matching result",
            "Try a broader entity name or a workspace such as Cast, World, Encounters, Compendium, or Live.");

    ImGui::EndChild();
    ImGui::End();
}
