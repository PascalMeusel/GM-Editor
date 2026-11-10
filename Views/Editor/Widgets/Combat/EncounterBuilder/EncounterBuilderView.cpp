#include "EncounterBuilderViewSupport.hpp"

EncounterBuilderView::EncounterBuilderView(EncounterBuilderController &controller)
    : DockableWidget("Encounter Builder", 0, ImVec2(1180.0f, 760.0f)), _controller(controller)
{
}

void EncounterBuilderView::draw()
{
    normalizeEncounter(_controller._draftEncounter);
    if (drawUiButton(uiTextForLocale("Encounters…", "Begegnungen…"), UiButtonRole::Secondary))
        ImGui::OpenPopup("EncounterLibrary");
    setResponsiveAutoSizeWindow(420.0f, 280.0f);
    if (ImGui::BeginPopup("EncounterLibrary"))
    {
        ImGui::PushItemWidth(340.0f);
        drawEncounterList();
        ImGui::PopItemWidth();
        ImGui::EndPopup();
    }
    if (!_controller._retainedEncounterDrafts.empty() &&
        ImGui::BeginCombo("##RetainedEncounterDrafts", uiTextForLocale("Resume a draft", "Entwurf fortsetzen")))
    {
        EntityId chosen;
        for (const auto &entry : _controller._retainedEncounterDrafts)
            if (entry.first != _controller._draftEncounter.metadata.id &&
                ImGui::Selectable((entry.second.encounter.name + "##" + entry.first).c_str())) chosen = entry.first;
        ImGui::EndCombo();
        if (!chosen.empty()) _controller.restoreRetainedEncounterDraft(chosen);
    }
    drawEncounterDetails();
}

void EncounterBuilderView::drawEncounterList()
{
    drawSectionTitle("Encounter Prep");
    if (drawUiButton("New Encounter", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Create a new encounter draft. Choose or generate its battle map in the preparation area on the right."))
    {
        _controller.startNewEncounter();
        ImGui::CloseCurrentPopup();
    }

    if (!_controller._currentCampaign.encounters.empty())
    {
        std::vector<std::string> labelStorage;
        std::vector<const char *> labels;
        for (const EncounterModel &encounter : _controller._currentCampaign.encounters)
            labelStorage.push_back(encounter.name.empty() ? localizedUiLabel("<Unnamed Encounter>") : encounter.name);
        for (const std::string &label : labelStorage)
            labels.push_back(label.c_str());
        if (_controller._selectedEncounterIndex < 0)
            _controller._selectedEncounterIndex = 0;
        ImGui::TextDisabled("%s", localizedUiText("Saved Encounter"));
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::Combo("##SavedEncounter", &_controller._selectedEncounterIndex, labels.data(), static_cast<int>(labels.size()));
        if (drawUiButton("Load Encounter", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), "Load the selected saved encounter into the prep editor."))
        {
            _controller.loadSelectedEncounter();
            ImGui::CloseCurrentPopup();
        }
    }
    else
    {
        ImGui::TextWrapped(localizedUiLabel("No encounters have been saved yet.").c_str());
    }

}

void EncounterBuilderView::drawEncounterDetails()
{
    DraftEditSignal::Scope draftEdits([this] { _controller.markDraftEdited(); });
    EncounterModel &encounter = _controller._draftEncounter;

    if (drawUiButton(_controller._saveFailed ? "Retry saving" : "Save Encounter Prep", UiButtonRole::Primary)) _controller.saveEncounter();
    const char *stageLabel = uiTextForLocale("Save and stage in Live", "Speichern und in Live öffnen");
    const float stageWidth = ImGui::CalcTextSize(stageLabel).x + 2.0f * ImGui::GetStyle().FramePadding.x;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + stageWidth <=
        ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x)
        TaskUi::sameLineIfFits();
    if (drawUiButton(stageLabel,
                       static_cast<bool>(_controller._stageHandler), "Live is unavailable.", UiButtonRole::Secondary))
    {
        if (_controller.saveEncounter())
        {
            if (_controller._stageHandler(encounter.metadata.id)) { hide(); return; }
            _controller._tacticalStatus = uiTextForLocale("Saved, but Live could not be updated. Check the active session and retry.", "Gespeichert, aber Live konnte nicht aktualisiert werden. Prüfe die aktive Sitzung und versuche es erneut.");
        }
    }
    if (_controller._saveFailed) ImGui::TextWrapped("%s", uiTextForLocale("Not saved. Your edits remain here. Retry saving.", "Nicht gespeichert. Deine Eingaben bleiben hier. Bitte erneut speichern."));
    else if (_controller._saveSucceeded) ImGui::TextDisabled("%s", uiTextForLocale("Last save succeeded.", "Letztes Speichern erfolgreich."));
    if (!_controller._tacticalStatus.empty())
        ImGui::TextWrapped("%s", localizedUiLabel(_controller._tacticalStatus.c_str()).c_str());
    ImGui::Spacing();
    if (!ImGui::BeginTabBar("EncounterPreparationTabs")) return;
    if (ImGui::BeginTabItem(uiTextForLocale("Encounter", "Begegnung")))
    {
    drawFieldLabel("Name");
    ImGui::SetNextItemWidth(-1.0f);
    FormUi::InputText("##EncounterName", &encounter.name);
    drawFieldLabel("Prep Notes");
    FormUi::InputTextMultiline("##EncounterPrepNotes", &encounter.description, ImVec2(-1.0f, 110.0f), ImGuiInputTextFlags_WordWrap);
    showHelpTooltip("Use this for boxed text, tactical notes, enemy plans, or reminders before combat starts.");
    FormUi::Checkbox(localizedUiLabel("Theater of the Mind").c_str(), &encounter.theaterOfMind);
    showHelpTooltip("Run this encounter without grid range enforcement. Participants still keep roster, initiative, actions, HP, and log state.");
    ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(uiTextForLocale("Participants", "Beteiligte")))
    {
        if (drawUiButton(uiTextForLocale("Add participants…", "Beteiligte hinzufügen…"), UiButtonRole::Primary))
            ImGui::OpenPopup("EncounterParticipants");
        if (ImGui::IsPopupOpen("EncounterParticipants")) TaskUi::preparePickerWindow();
        if (ImGui::BeginPopup("EncounterParticipants"))
        {
            drawParticipantTools();
            ImGui::EndPopup();
        }
        drawParticipantTable();
        if (ImGui::TreeNode(uiTextForLocale("Difficulty estimate", "Schwierigkeit einschätzen")))
        { drawDifficultyPreview(); ImGui::TreePop(); }
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(uiTextForLocale("Battlefield", "Schlachtfeld")))
    {
        drawBattlefield();
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}
