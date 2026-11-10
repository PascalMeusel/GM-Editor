#include "Controllers/Editor/EditorWindowController.hpp"
#include "Views/Editor/EditorWindowView.hpp"
#include "Views/Editor/Widgets/DockableWidget.hpp"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include <algorithm>

namespace
{
const char *navigationText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}

const char *embeddedToolLabel(const std::string &route)
{
    if (route == "show:character_sheet") return navigationText("Character sheet", "Charakterbogen");
    if (route == "show:player_creator") return navigationText("New player", "Neue Spielfigur");
    if (route == "show:player_overview") return navigationText("Player directory", "Figurenverzeichnis");
    if (route == "show:npc_creator") return navigationText("New NPC or creature", "Neuer NSC oder Kreatur");
    if (route == "show:npc_overview") return navigationText("NPC directory", "NSC-Verzeichnis");
    if (route == "show:item_creator") return navigationText("Item editor", "Gegenstandseditor");
    if (route == "show:inventory_manager") return navigationText("Inventory", "Inventar");
    if (route == "show:spell_library") return navigationText("Spell library", "Zauberbibliothek");
    if (route == "show:audio_manager") return navigationText("Audio manager", "Audioverwaltung");
    if (route == "show:map_creator") return navigationText("Map editor", "Karteneditor");
    if (route == "show:map_viewer") return navigationText("Map viewer", "Kartenansicht");
    if (route == "show:encounter_builder") return navigationText("Encounter builder", "Begegnungseditor");
    if (route == "show:story_workspace") return navigationText("Story editor", "Handlungseditor");
    return navigationText("Workspace editor", "Arbeitsbereichseditor");
}

std::string backToWorkspaceLabel(EditorWorkspace workspace)
{
    return std::string(navigationText("Back to ", "Zurück zu ")) + editorWorkspaceLabel(workspace);
}

std::string returnToWorkspaceLabel(EditorWorkspace workspace)
{
    return std::string(navigationText("Back: ", "Zurück: ")) +
           editorWorkspaceLabel(workspace) + "##PreviousTask";
}
}

void EditorWindowController::captureWorkspaceLocation()
{
    auto &location = _navigation.current();
    location.filter = view->_workspaceFilter;
    location.category = view->_activeWorkspace == EditorWorkspace::World ? view->_worldCategoryFilter :
        view->_activeWorkspace == EditorWorkspace::Compendium ? view->_compendiumCategoryFilter : 0;
    location.selectedEntityId = view->_workspaceSelectionId;
    location.selectedEntityType = gm::ux::resolveNavigationEntity(_currentCampaign, location.selectedEntityId).type;
    location.sessionId = view->_sessionWorkspace.sessionId();
    location.planEntryId = view->_sessionWorkspace.planEntryId();
    location.sceneId = view->_sessionWorkspace.sceneId();
}

void EditorWindowController::restoreWorkspaceLocation()
{
    _navigation.reconcile([this](const std::string &, const EntityId &id) {
        return gm::ux::resolveNavigationEntity(_currentCampaign, id).available;
    });
    const auto &location = _navigation.current();
    view->_activeWorkspace = location.workspace;
    view->_workspaceFilter = location.filter;
    view->_workspaceSelectionId = location.selectedEntityId;
    if (location.workspace == EditorWorkspace::World) view->_worldCategoryFilter = location.category;
    if (location.workspace == EditorWorkspace::Compendium) view->_compendiumCategoryFilter = location.category;
}

std::shared_ptr<IWidgetController> EditorWindowController::workspaceTool(const std::string &route) const
{
    if (route == "show:character_sheet") return _characterSheet;
    if (route == "show:player_creator") return _playerCreator;
    if (route == "show:player_overview") return _playerOverview;
    if (route == "show:npc_creator") return _npcCreator;
    if (route == "show:npc_overview") return _npcOverview;
    if (route == "show:item_creator") return _itemCreator;
    if (route == "show:inventory_manager") return _inventoryManager;
    if (route == "show:spell_library") return _spellLibrary;
    if (route == "show:audio_manager") return _audioManager;
    if (route == "show:map_creator") return _mapCreator;
    if (route == "show:map_viewer") return _mapViewer;
    if (route == "show:encounter_builder") return _encounterBuilder;
    if (route == "show:story_workspace") return _storyWorkspace;
    return {};
}

bool EditorWindowController::openWorkspaceTool(const std::string &route, gm::ux::WorkspaceTarget workspace)
{
    const auto tool = workspaceTool(route);
    const bool rememberReturn = !_openingWorkspaceEntity &&
        (view->_activeWorkspace != workspace || _navigation.current().editorRoute != route);
    if (!tool || !view->activateWorkspace(workspace, rememberReturn)) return false;
    hideAllWorkspaceTools();
    _navigation.current().editorRoute = route;
    _navigation.current().targetUnavailable = false;
    _navigation.current().sessionId = view->_sessionWorkspace.sessionId();
    _navigation.current().planEntryId = view->_sessionWorkspace.planEntryId();
    _navigation.current().sceneId = view->_sessionWorkspace.sceneId();
    _navigation.current().sessionUnavailable = false;
    tool->show();
    return true;
}

bool EditorWindowController::returnToPreviousWorkspace()
{
    _navigation.reconcile([this](const std::string &, const EntityId &id) {
        return gm::ux::resolveNavigationEntity(_currentCampaign, id).available;
    });
    const auto *previous = _navigation.returnDestination();
    if (!previous) return false;
    const auto destination = *previous;
    if (view->_activeWorkspace == EditorWorkspace::Home && !flushPreparationDrafts("return to previous task")) return false;
    if (!restoreWorkspaceToolSelection(destination)) return false;
    if (destination.workspace == EditorWorkspace::Home &&
        !view->_sessionWorkspace.restoreSelection(_currentCampaign, destination.sessionId, destination.planEntryId,
            destination.sessionUnavailable, [this](Campaign &proposed) {
                return ProjectSaveService::getInstance().saveCampaign(proposed, "restore session navigation");
            })) return false;
    // activateWorkspace performs the production preparation save guard first.
    if (!view->activateWorkspace(destination.workspace, false)) return false;
    if (!_navigation.returnToPrevious({})) return false;
    restoreWorkspaceLocation();
    return true;
}

bool EditorWindowController::restoreWorkspaceToolSelection(const gm::ux::WorkspaceLocation &location)
{
    const auto &id = location.selectedEntityId;
    if (id.empty() || location.editorRoute.empty()) return true;
    if (location.editorRoute == "show:character_sheet") return _characterSheet->selectCharacterById(id);
    if (location.editorRoute == "show:npc_overview") return _npcOverview->selectEntityById(id, location.selectedEntityType);
    if (location.editorRoute == "show:item_creator") return _itemCreator->selectItemById(id);
    if (location.editorRoute == "show:inventory_manager") return _inventoryManager->selectOwnerById(id);
    if (location.editorRoute == "show:map_viewer") return _mapViewer->selectMapById(id);
    if (location.editorRoute == "show:encounter_builder") return _encounterBuilder->selectEncounterById(id);
    if (location.editorRoute == "show:spell_library") return _spellLibrary->selectSpellById(id);
    if (location.editorRoute == "show:story_workspace")
    {
        if (location.selectedEntityType == "story") return _storyWorkspace->selectThreadById(id);
        for (const auto &thread : _currentCampaign.storyThreads)
            for (const auto &scene : thread.beats)
                if (scene.metadata.id == id)
                    return _storyWorkspace->selectThreadById(thread.metadata.id) && _storyWorkspace->selectBeatById(id);
        return false;
    }
    // Map Creator retains its own immutable draft identity and history. Opening
    // it again here could replace the draft that this return is meant to resume.
    return true;
}

bool EditorWindowController::renderEmbeddedWorkspaceTool()
{
    auto &location = _navigation.current();
    const std::string route = location.editorRoute;
    const auto tool = workspaceTool(route);
    if (!tool) return false;

    const ProjectSaveState &saveState = ProjectSaveService::getInstance().state();
    const bool hasPendingChanges = hasUnsavedChanges();
    const bool retainedDraft = hasPendingChanges && !saveState.dirty;
    const char *campaignState = retainedDraft
                                    ? navigationText("Draft not yet saved", "Entwurf noch nicht gespeichert")
                                    : saveState.dirty
                                    ? navigationText("Campaign changes pending", "Kampagnenänderungen ausstehend")
                                    : navigationText("Campaign saved", "Kampagne gespeichert");

    // This intentionally separates persisted campaign state from an editor
    // draft. A new player can be retained even while the campaign file is
    // already saved.
    ImGui::PushStyleColor(ImGuiCol_Text, WorkspaceUi::Text);
    ImGui::TextUnformatted(editorWorkspaceLabel(view->_activeWorkspace));
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::TextDisabled("·");
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::PushStyleColor(ImGuiCol_Text, WorkspaceUi::Amber);
    ImGui::TextWrapped("%s", embeddedToolLabel(route));
    ImGui::PopStyleColor();

    // A pointer gesture can make a retained draft pending. Reserve the same
    // status/action geometry for every state so the canvas cannot move under it.
    const char *stateLabels[] = {
        navigationText("Draft not yet saved", "Entwurf noch nicht gespeichert"),
        navigationText("Campaign changes pending", "Kampagnenänderungen ausstehend"),
        navigationText("Campaign saved", "Kampagne gespeichert")};
    float stateWidth = 0.0f;
    for (const char *label : stateLabels) stateWidth = std::max(stateWidth, ImGui::CalcTextSize(label).x);
    stateWidth += ImGui::GetStyle().ItemSpacing.x * 2.0f;
    if (ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x - ImGui::GetItemRectMax().x > stateWidth)
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
    const float stateTop = ImGui::GetCursorPosY();
    const float stateWrapWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    float stateHeight = 0.0f;
    for (const char *label : stateLabels)
        stateHeight = std::max(stateHeight, ImGui::CalcTextSize(label, nullptr, false, stateWrapWidth).y);
    ImGui::PushStyleColor(ImGuiCol_Text, hasPendingChanges ? WorkspaceUi::Amber : WorkspaceUi::Teal);
    ImGui::TextWrapped("%s", campaignState);
    ImGui::PopStyleColor();
    ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), stateTop + stateHeight + ImGui::GetStyle().ItemSpacing.y));
    if (retainedDraft)
    {
        showHelpTooltip(navigationText(
            "The campaign file is saved. A draft still needs saving. Finish new entries with Create in their editor.",
            "Die Kampagnendatei ist gespeichert. Ein Entwurf ist noch nicht gespeichert. Schließe neue Einträge mit Erstellen im jeweiligen Editor ab."));
    }

    const char *reviewLabel = navigationText("Review player display", "Spieleranzeige prüfen");
    const char *saveLabel = navigationText("Save changes", "Änderungen speichern");
    if (drawUiButton(reviewLabel, UiButtonRole::Secondary, ImVec2(0.0f, 0.0f),
                     navigationText("Review the player-safe output before publishing it to the table display.",
                                    "Prüfe die spielersichere Ausgabe, bevor du sie für die Tischanzeige veröffentlichst.")))
        showPresentationPanel();
    TaskUi::sameLineForButton(localizedUiLabel(saveLabel).c_str());
    if (drawUiButton(saveLabel, hasPendingChanges,
                     navigationText("There are no pending changes to save.", "Keine ausstehenden Änderungen zum Speichern."),
                     UiButtonRole::Primary, ImVec2(0.0f, 0.0f),
                     navigationText("Save edited records. New entries must be completed with Create in their editor.",
                                    "Speichert bearbeitete Einträge. Neue Einträge werden mit Erstellen im jeweiligen Editor abgeschlossen.")))
        saveCurrentCampaign("embedded workspace header save");

    const std::string backLabel = backToWorkspaceLabel(view->_activeWorkspace);
    const auto *previous = _navigation.returnDestination();
    if (previous && (previous->workspace != view->_activeWorkspace || !previous->editorRoute.empty()))
    {
        const std::string returnLabel = returnToWorkspaceLabel(previous->workspace);
        TaskUi::sameLineForButton(returnLabel.c_str());
        if (TaskUi::button(returnLabel.c_str()) && returnToPreviousWorkspace())
            return true;
    }
    if (location.targetUnavailable)
    {
        ImGui::TextWrapped("%s", navigationText(
            "The selected entry is unavailable. Choose another entry; retained drafts remain in their editor.",
            "Der gewählte Eintrag ist nicht mehr verfügbar. Wähle einen anderen Eintrag; Entwürfe bleiben im Editor erhalten."));
    }
    TaskUi::sameLineForButton(backLabel.c_str());
    if (TaskUi::button(backLabel.c_str()))
    {
        tool->hide();
        location.editorRoute.clear();
        return false;
    }
    ImGui::Separator();
    if (!tool->isOpen()) tool->show();
    {
        DockableWidget::EmbeddedScope embedded;
        tool->update({});
    }
    // Successful creation may close a tool or return through its existing callback.
    // Do not replace a destination chosen by that callback during this frame.
    if (_navigation.current().editorRoute == route && !tool->isOpen())
    {
        _navigation.current().editorRoute.clear();
        returnToPreviousWorkspace();
    }
    return true;
}
