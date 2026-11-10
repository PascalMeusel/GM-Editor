#include "Views/Editor/TabletopToolkit/TabletopToolkitView.hpp"

#include "Controllers/Editor/TabletopToolkit/TabletopToolkitController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Session/TabletopToolkit/TabletopToolkitService.hpp"
#include "Views/Editor/ApprovedUi/ApprovedToolkitChrome.hpp"
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"

#include "imgui.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
const char *ttText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
               ? german
               : english;
}

const char *objectiveStatusLabel(StoryObjectiveStatus status)
{
    switch (status)
    {
    case StoryObjectiveStatus::Planned: return ttText("Planned", "Geplant");
    case StoryObjectiveStatus::Active: return ttText("Active", "Aktiv");
    case StoryObjectiveStatus::Completed: return ttText("Completed", "Abgeschlossen");
    case StoryObjectiveStatus::Failed: return ttText("Failed", "Gescheitert");
    }
    return "-";
}

const char *clueStatusLabel(StoryClueStatus status)
{
    switch (status)
    {
    case StoryClueStatus::Hidden: return ttText("Hidden", "Verborgen");
    case StoryClueStatus::Discovered: return ttText("Discovered", "Entdeckt");
    case StoryClueStatus::Shared: return ttText("Shared", "Geteilt");
    }
    return "-";
}

struct WarningTarget
{
    EntityId id;
    std::string type;
    std::string label;
};

std::vector<WarningTarget> warningTargets(const Campaign &campaign)
{
    std::vector<WarningTarget> targets{{campaign.metadata.id, "campaign", campaign.name.empty() ? ttText("Campaign", "Kampagne") : campaign.name}};
    for (const auto &thread : campaign.storyThreads)
        for (const auto &beat : thread.beats)
            targets.push_back({beat.metadata.id, "story_beat", beat.title.empty() ? ttText("Untitled scene", "Unbenannte Szene") : beat.title});
    for (const auto &encounter : campaign.encounters)
        targets.push_back({encounter.metadata.id, "encounter", encounter.name.empty() ? ttText("Untitled encounter", "Unbenannte Begegnung") : encounter.name});
    return targets;
}
}

TabletopToolkitView::TabletopToolkitView(TabletopToolkitController &controller) : _controller(controller) {}

void TabletopToolkitView::requestObjectiveFocus(const EntityId &objectiveId)
{
    _focusedObjectiveId = objectiveId;
    _focusedClueId.clear();
    _requestObjectivesTab = true;
    _requestCluesTab = false;
    _requestStoryTab = true;
}

void TabletopToolkitView::requestClueFocus(const EntityId &clueId)
{
    _focusedClueId = clueId;
    _focusedObjectiveId.clear();
    _requestObjectivesTab = false;
    _requestCluesTab = true;
    _requestStoryTab = true;
}

void TabletopToolkitView::requestObjectivesTab()
{
    _focusedObjectiveId.clear();
    _focusedClueId.clear();
    _requestObjectivesTab = true;
    _requestCluesTab = false;
    _requestStoryTab = true;
}

void TabletopToolkitView::requestCluesTab()
{
    _focusedObjectiveId.clear();
    _focusedClueId.clear();
    _requestObjectivesTab = false;
    _requestCluesTab = true;
    _requestStoryTab = true;
}

void TabletopToolkitView::drawDiscardPendingDraft(TabletopToolkitPendingDraft draft, const char *entityLabel)
{
    const std::string popup = std::string("Discard ") + entityLabel + " draft###TabletopToolkitDiscard" +
                              std::to_string(static_cast<int>(draft));
    if (drawUiButton(ttText("Discard draft", "Entwurf verwerfen"), UiButtonRole::Secondary))
        ImGui::OpenPopup(popup.c_str());
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    setResponsiveWindowSize(ImVec2(560.0f, 260.0f), ImVec2(380.0f, 220.0f));
    if (!ImGui::BeginPopupModal(popup.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;
    ImGui::TextWrapped("%s", ttText("Discard this unfinished entry? It has not been added to the campaign. Other unfinished table-tool drafts stay available.",
                                     "Diesen unfertigen Eintrag verwerfen? Er wurde noch nicht zur Kampagne hinzugefügt. Andere unfertige Tischwerkzeug-Entwürfe bleiben erhalten."));
    const std::string confirmLabel = std::string(ttText("Discard draft", "Entwurf verwerfen")) +
                                     "##TabletopToolkitDiscardConfirm" + std::to_string(static_cast<int>(draft));
    if (drawModalActionButtons(confirmLabel.c_str(), UiButtonRole::Destructive,
                               ttText("Keep editing", "Weiter bearbeiten"), 168.0f))
        _controller.discardPendingDraft(draft);
    ImGui::EndPopup();
}

void TabletopToolkitView::drawEmbedded()
{
    ImGui::PushID("TabletopToolkit");
    gm::approved_ui::renderToolkitHeader(_controller._campaign, _controller._statusMessage);

    if (ImGui::BeginTabBar("TabletopToolkitTabs"))
    {
        if (ImGui::BeginTabItem(ttText("Dice", "Würfel"))) { drawDice(); ImGui::EndTabItem(); }
        // A focused record requests Story exactly once. Reapplying SetSelected
        // while the record is open would override the GM's next tab choice.
        const bool selectStory = _requestStoryTab;
        _requestStoryTab = false;
        if (ImGui::BeginTabItem(ttText("Objectives and clues", "Ziele und Hinweise"), nullptr,
                                selectStory ? ImGuiTabItemFlags_SetSelected : 0))
        {
            drawStory();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(ttText("Safety", "Sicherheit"))) { drawSafety(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::PopID();
}

void TabletopToolkitView::drawDice()
{
    gm::approved_ui::toolkitSection(ttText("Quick roll", "Schnellwurf"),
        ttText("Keep the result in view and decide whether it belongs in the session record.",
               "Behalte das Ergebnis im Blick und entscheide, ob es in die Sitzungsnotizen gehört."));
    FormUi::InputText(ttText("Expression", "Ausdruck"), &_controller._rollExpression);
    FormUi::InputText(ttText("Label", "Bezeichnung"), &_controller._rollLabel);
    FormUi::Checkbox(ttText("Add rolls to session notes", "Würfe zu Sitzungsnotizen hinzufügen"), &_controller._rollToSessionNotes);
    if (drawUiButton(ttText("Roll now", "Jetzt würfeln"), UiButtonRole::Primary))
        _controller.rollNow(_controller._rollExpression, _controller._rollLabel, _controller._rollToSessionNotes);
    if (_controller._lastRoll.valid)
    {
        ImGui::SameLine();
        ImGui::Text("%s: %d", ttText("Result", "Ergebnis"), _controller._lastRoll.total);
    }

    gm::approved_ui::toolkitSection(ttText("Saved macros", "Gespeicherte Makros"),
        ttText("Reusable table actions for recurring checks, damage, and improvisation.",
               "Wiederverwendbare Tischaktionen für Proben, Schaden und Improvisation."));
    int removeIndex = -1;
    {
        DraftEditSignal::Scope existingEdits([this] { _controller.markCampaignEdited(); });
        for (std::size_t index = 0; index < _controller._campaign.diceMacros.size(); ++index)
        {
            CampaignDiceMacroModel &macro = _controller._campaign.diceMacros[index];
            ImGui::PushID(macro.metadata.id.c_str());
            if (ImGui::TreeNodeEx(macro.name.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                FormUi::InputText(ttText("Name", "Name"), &macro.name);
                FormUi::InputText(ttText("Expression", "Ausdruck"), &macro.expression);
                FormUi::InputTextMultiline(ttText("Notes", "Notizen"), &macro.notes, ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
                FormUi::Checkbox(ttText("Add to session notes", "Zu Sitzungsnotizen hinzufügen"), &macro.addToSessionNotes);
                if (drawUiButton(ttText("Roll", "Würfeln"), UiButtonRole::Primary))
                    _controller.rollNow(macro.expression, macro.name, macro.addToSessionNotes);
                ImGui::SameLine();
                if (drawUiButton(ttText("Save macro", "Makro speichern"), UiButtonRole::Secondary))
                {
                    std::string error;
                    if (TabletopToolkit::validDiceMacro(_controller._campaign, macro, &error))
                    { macro.metadata.touch(); _controller.persist("update campaign dice macro"); }
                    else _controller._statusMessage = error;
                }
                ImGui::SameLine();
                if (drawUiButton(ttText("Remove", "Entfernen"), UiButtonRole::Destructive)) removeIndex = static_cast<int>(index);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    if (removeIndex >= 0)
    {
        _controller._campaign.diceMacros.erase(_controller._campaign.diceMacros.begin() + removeIndex);
        _controller.persist("remove campaign dice macro");
    }

    if (ImGui::CollapsingHeader(ttText("Add macro", "Makro hinzufügen")))
    {
        DraftEditSignal::Scope newDraftEdits([] {});
        FormUi::InputText(ttText("Name##NewMacro", "Name##NewMacro"), &_controller._newMacro.name);
        FormUi::InputText(ttText("Expression##NewMacro", "Ausdruck##NewMacro"), &_controller._newMacro.expression);
        // Keep the required Save/Discard decision alongside its required
        // fields. Optional notes can fall below the compact Live viewport.
        if (drawUiButton(ttText("Save new macro", "Neues Makro speichern"), UiButtonRole::Primary)) _controller.addMacro();
        ImGui::SameLine();
        drawDiscardPendingDraft(TabletopToolkitPendingDraft::Macro, ttText("macro", "Makro"));
        FormUi::InputTextMultiline(ttText("Notes##NewMacro", "Notizen##NewMacro"), &_controller._newMacro.notes,
                                   ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
        FormUi::Checkbox(ttText("Add to session notes##NewMacro", "Zu Sitzungsnotizen hinzufügen##NewMacro"), &_controller._newMacro.addToSessionNotes);
    }
}

void TabletopToolkitView::drawStory()
{
    gm::approved_ui::toolkitSection(ttText("Story state", "Handlungsstand"),
        ttText("Separate the GM truth from the exact information the players may see.",
               "Trenne die SL-Wahrheit klar von den Informationen, die Spieler sehen dürfen."));
    if (ImGui::BeginTabBar("StoryTrackingTabs"))
    {
        const bool selectObjectives = _requestObjectivesTab || !_focusedObjectiveId.empty();
        const bool selectClues = _requestCluesTab || !_focusedClueId.empty();
        _requestObjectivesTab = false;
        _requestCluesTab = false;
        if (ImGui::BeginTabItem(ttText("Objectives", "Ziele"), nullptr,
                                selectObjectives ? ImGuiTabItemFlags_SetSelected : 0)) { drawObjectives(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem(ttText("Clues", "Hinweise"), nullptr,
                                selectClues ? ImGuiTabItemFlags_SetSelected : 0)) { drawClues(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
}

void TabletopToolkitView::drawObjectives()
{
    int removeIndex = -1;
    DraftEditSignal::Scope existingEdits([this] { _controller.markCampaignEdited(); });
    for (std::size_t index = 0; index < _controller._campaign.storyObjectives.size(); ++index)
    {
        StoryObjectiveModel &objective = _controller._campaign.storyObjectives[index];
        const auto before = objective;
        DraftEditSignal::Scope recordEdits([this, before] { _controller.trackObjective(before); _controller.markCampaignEdited(); });
        const bool focused = objective.metadata.id == _focusedObjectiveId;
        ImGui::PushID(objective.metadata.id.c_str());
        const std::string header = objective.title + "  ·  " + objectiveStatusLabel(objective.status);
        if (focused)
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            if (focused)
                ImGui::SetScrollHereY(0.25f);
            FormUi::InputText(ttText("Title", "Titel"), &objective.title);
            FormUi::InputTextMultiline(ttText("Player summary", "Spielertext"), &objective.playerSummary,
                                       ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
            FormUi::InputTextMultiline(ttText("GM notes", "SL-Notizen"), &objective.gmNotes,
                                       ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
            if (FormUi::BeginCombo(ttText("Status", "Status"), objectiveStatusLabel(objective.status)))
            {
                for (int value = 0; value <= static_cast<int>(StoryObjectiveStatus::Failed); ++value)
                {
                    const auto status = static_cast<StoryObjectiveStatus>(value);
                    if (ImGui::Selectable(objectiveStatusLabel(status), objective.status == status))
                    { _controller.trackObjective(objective); TabletopToolkit::setObjectiveStatus(objective, status); _controller.persist("update story objective status"); }
                }
                ImGui::EndCombo();
            }
            FormUi::Checkbox(ttText("Show to players", "Spielern zeigen"), &objective.playerVisible);
            if (drawUiButton(ttText("Save objective", "Ziel speichern"), UiButtonRole::Primary))
            { objective.metadata.touch(); _controller.persist("update story objective"); }
            ImGui::SameLine();
            if (drawUiButton(ttText("Remove", "Entfernen"), UiButtonRole::Destructive)) removeIndex = static_cast<int>(index);
            ImGui::TreePop();
        }
        ImGui::PopID();
        if (focused)
            _focusedObjectiveId.clear();
    }
    if (removeIndex >= 0)
    {
        _controller.trackObjective(_controller._campaign.storyObjectives[static_cast<std::size_t>(removeIndex)]);
        _controller._campaign.storyObjectives.erase(_controller._campaign.storyObjectives.begin() + removeIndex);
        _controller.persist("remove story objective");
    }
    if (_controller._campaign.storyObjectives.empty())
        ImGui::TextDisabled("%s", ttText("No objectives yet. Add the next concrete thing the party can pursue.",
                                          "Noch keine Ziele. Füge den nächsten konkreten Ansatz der Gruppe hinzu."));

    if (ImGui::CollapsingHeader(ttText("Add objective", "Ziel hinzufügen")))
    {
        DraftEditSignal::Scope newDraftEdits([] {});
        FormUi::InputText(ttText("Title##NewObjective", "Titel##NewObjective"), &_controller._newObjective.title);
        FormUi::InputTextMultiline(ttText("Player summary##NewObjective", "Spielertext##NewObjective"), &_controller._newObjective.playerSummary,
                                   ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
        FormUi::InputTextMultiline(ttText("GM notes##NewObjective", "SL-Notizen##NewObjective"), &_controller._newObjective.gmNotes,
                                   ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
        FormUi::Checkbox(ttText("Show to players##NewObjective", "Spielern zeigen##NewObjective"), &_controller._newObjective.playerVisible);
        if (drawUiButton(ttText("Save new objective", "Neues Ziel speichern"), UiButtonRole::Primary)) _controller.addObjective();
        ImGui::SameLine();
        drawDiscardPendingDraft(TabletopToolkitPendingDraft::Objective, ttText("objective", "Ziel"));
    }
}

void TabletopToolkitView::drawClues()
{
    int removeIndex = -1;
    DraftEditSignal::Scope existingEdits([this] { _controller.markCampaignEdited(); });
    for (std::size_t index = 0; index < _controller._campaign.storyClues.size(); ++index)
    {
        StoryClueModel &clue = _controller._campaign.storyClues[index];
        const auto before = clue;
        DraftEditSignal::Scope recordEdits([this, before] { _controller.trackClue(before); _controller.markCampaignEdited(); });
        const bool focused = clue.metadata.id == _focusedClueId;
        ImGui::PushID(clue.metadata.id.c_str());
        const std::string header = clue.title + "  ·  " + clueStatusLabel(clue.status);
        if (focused)
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            if (focused)
                ImGui::SetScrollHereY(0.25f);
            FormUi::InputText(ttText("Title", "Titel"), &clue.title);
            FormUi::InputTextMultiline(ttText("Player-safe wording", "Spielersicherer Text"), &clue.playerText,
                                       ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
            FormUi::InputTextMultiline(ttText("GM truth", "SL-Wahrheit"), &clue.gmTruth,
                                       ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
            if (clue.status == StoryClueStatus::Hidden && drawUiButton(ttText("Mark discovered", "Als entdeckt markieren"), UiButtonRole::Advanced))
            { _controller.trackClue(clue); TabletopToolkit::revealClue(clue, false); _controller.persist("mark clue discovered"); }
            if (clue.status != StoryClueStatus::Shared)
            {
                if (clue.status == StoryClueStatus::Hidden) ImGui::SameLine();
                if (drawUiButton(ttText("Share with players", "Mit Spielern teilen"), UiButtonRole::Primary))
                { _controller.trackClue(clue); TabletopToolkit::revealClue(clue, true); _controller.persist("share story clue"); }
            }
            if (clue.status != StoryClueStatus::Hidden)
            { if (clue.status == StoryClueStatus::Discovered) ImGui::SameLine();
              if (drawUiButton(ttText("Hide", "Verbergen"), UiButtonRole::Secondary))
              { _controller.trackClue(clue); TabletopToolkit::hideClue(clue); _controller.persist("hide story clue"); } }
            if (drawUiButton(ttText("Save clue", "Hinweis speichern"), UiButtonRole::Secondary))
            { clue.metadata.touch(); _controller.persist("update story clue"); }
            ImGui::SameLine();
            if (drawUiButton(ttText("Remove", "Entfernen"), UiButtonRole::Destructive)) removeIndex = static_cast<int>(index);
            ImGui::TreePop();
        }
        ImGui::PopID();
        if (focused)
            _focusedClueId.clear();
    }
    if (removeIndex >= 0)
    {
        _controller.trackClue(_controller._campaign.storyClues[static_cast<std::size_t>(removeIndex)]);
        _controller._campaign.storyClues.erase(_controller._campaign.storyClues.begin() + removeIndex);
        _controller.persist("remove story clue");
    }
    if (_controller._campaign.storyClues.empty())
        ImGui::TextDisabled("%s", ttText("No clues yet. Keep the truth separate from what players may read.",
                                          "Noch keine Hinweise. Trenne die Wahrheit vom spielersicheren Text."));

    if (ImGui::CollapsingHeader(ttText("Add clue", "Hinweis hinzufügen")))
    {
        DraftEditSignal::Scope newDraftEdits([] {});
        FormUi::InputText(ttText("Title##NewClue", "Titel##NewClue"), &_controller._newClue.title);
        FormUi::InputTextMultiline(ttText("Player-safe wording##NewClue", "Spielersicherer Text##NewClue"), &_controller._newClue.playerText,
                                   ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
        FormUi::InputTextMultiline(ttText("GM truth##NewClue", "SL-Wahrheit##NewClue"), &_controller._newClue.gmTruth,
                                   ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
        if (drawUiButton(ttText("Save new clue", "Neuen Hinweis speichern"), UiButtonRole::Primary)) _controller.addClue();
        ImGui::SameLine();
        drawDiscardPendingDraft(TabletopToolkitPendingDraft::Clue, ttText("clue", "Hinweis"));
    }
}
