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

void TabletopToolkitView::drawWarnings()
{
    gm::approved_ui::toolkitSection(ttText("Content warnings", "Inhaltswarnungen"), nullptr);
    int removeIndex = -1;
    for (std::size_t index = 0; index < _controller._campaign.safetyProfile.warnings.size(); ++index)
    {
        const SafetyContentWarningModel &warning = _controller._campaign.safetyProfile.warnings[index];
        ImGui::PushID(warning.metadata.id.c_str());
        ImGui::TextWrapped("%s: %s", warning.targetLabel.c_str(), warning.warning.c_str());
        ImGui::SameLine(); if (drawUiButton(ttText("Remove", "Entfernen"), UiButtonRole::Destructive)) removeIndex = static_cast<int>(index);
        ImGui::PopID();
    }
    if (removeIndex >= 0)
    {
        auto &values = _controller._campaign.safetyProfile.warnings;
        values.erase(values.begin() + removeIndex); _controller.persist("remove content warning");
    }

    if (ImGui::CollapsingHeader(ttText("Add content warning", "Inhaltswarnung hinzufügen")))
    {
        DraftEditSignal::Scope newDraftEdits([] {});
        const std::vector<WarningTarget> targets = warningTargets(_controller._campaign);
        int selected = 0;
        for (std::size_t index = 0; index < targets.size(); ++index)
            if (targets[index].id == _controller._newWarning.targetEntityId) selected = static_cast<int>(index);
        if (FormUi::BeginCombo(ttText("Applies to", "Gilt für"), targets[static_cast<std::size_t>(selected)].label.c_str()))
        {
            for (std::size_t index = 0; index < targets.size(); ++index)
                if (ImGui::Selectable(targets[index].label.c_str(), static_cast<int>(index) == selected))
                {
                    _controller._newWarning.targetEntityId = targets[index].id;
                    _controller._newWarning.targetEntityType = targets[index].type;
                    _controller._newWarning.targetLabel = targets[index].label;
                }
            ImGui::EndCombo();
        }
        FormUi::InputTextMultiline(ttText("Warning", "Warnung"), &_controller._newWarning.warning,
                                   ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
        FormUi::Checkbox(ttText("Share with players", "Mit Spielern teilen"), &_controller._newWarning.shareWithPlayers);
        if (drawUiButton(ttText("Save warning", "Warnung speichern"), UiButtonRole::Primary)) _controller.addWarning();
        ImGui::SameLine();
        drawDiscardPendingDraft(TabletopToolkitPendingDraft::Warning, ttText("content warning", "Inhaltswarnung"));
    }
}

void TabletopToolkitView::drawSafety()
{
    DraftEditSignal::Scope existingEdits([this] { _controller.markCampaignEdited(); });
    gm::approved_ui::toolkitSection(ttText("Table safety", "Tischsicherheit"),
        ttText("Pause and check-in controls remain prominent without recording who used them.",
               "Pause und Check-in bleiben sichtbar, ohne zu erfassen, wer sie ausgelöst hat."));
    CampaignSafetyProfileModel &safety = _controller._campaign.safetyProfile;
    if (FormUi::Checkbox(ttText("Enable table safety tools", "Tischsicherheit aktivieren"), &safety.enabled))
        _controller.persist("toggle table safety tools");
    if (safety.pauseActive)
    {
        ImGui::TextWrapped("%s", ttText("The table is paused. Resolve the check-in before continuing.",
                                         "Der Tisch pausiert. Kläre den Check-in, bevor es weitergeht."));
        if (drawUiButton(ttText("Resume when ready", "Fortsetzen, wenn alle bereit sind"), UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
        { TabletopToolkit::clearSafetyPause(_controller._campaign); _controller.persist("clear safety pause"); }
    }
    else if (safety.enabled && drawUiButton(safety.pauseLabel.empty() ? ttText("Pause", "Pause") : safety.pauseLabel.c_str(),
                                             UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f)))
    { TabletopToolkit::requestSafetyPause(_controller._campaign); _controller.persist("request safety pause"); }
    ImGui::TextDisabled("%s", ttText("The log records the pause, never who requested it.",
                                      "Das Protokoll erfasst die Pause, niemals wer sie ausgelöst hat."));

    if (safety.checkInActive)
    {
        ImGui::TextWrapped("%s", safety.checkInPrompt.c_str());
        if (drawUiButton(ttText("Finish check-in", "Check-in beenden"), UiButtonRole::Primary))
        { TabletopToolkit::clearSafetyCheckIn(_controller._campaign); _controller.persist("finish safety check-in"); }
    }
    else if (safety.enabled && drawUiButton(ttText("Start table check-in", "Tisch-Check-in starten"), UiButtonRole::Advanced))
    { TabletopToolkit::startSafetyCheckIn(_controller._campaign, safety.checkInPrompt); _controller.persist("start safety check-in"); }

    if (ImGui::CollapsingHeader(ttText("Safety protocol", "Sicherheitsprotokoll")))
    {
        FormUi::InputText(ttText("Pause label", "Pausenbezeichnung"), &safety.pauseLabel);
        FormUi::InputTextMultiline(ttText("What happens on pause", "Ablauf bei Pause"), &safety.pauseProtocol,
                                   ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 3.0f), ImGuiInputTextFlags_WordWrap);
        FormUi::InputText(ttText("Check-in prompt", "Check-in-Frage"), &safety.checkInPrompt);
        if (drawUiButton(ttText("Save protocol", "Protokoll speichern"), UiButtonRole::Primary))
            _controller.persist("update safety protocol");
    }
    drawBoundaries();
    drawWarnings();
}

void TabletopToolkitView::drawBoundaries()
{
    gm::approved_ui::toolkitSection(ttText("Lines and veils", "Lines und Veils"), nullptr);
    int removeIndex = -1;
    for (std::size_t index = 0; index < _controller._campaign.safetyProfile.boundaries.size(); ++index)
    {
        SafetyBoundaryModel &boundary = _controller._campaign.safetyProfile.boundaries[index];
        ImGui::PushID(boundary.metadata.id.c_str());
        const char *kind = boundary.kind == SafetyBoundaryKind::Line ? "Line" : "Veil";
        const std::string header = std::string(kind) + ": " + boundary.topic;
        if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            if (FormUi::BeginCombo(ttText("Boundary", "Grenze"), kind))
            {
                if (ImGui::Selectable(ttText("Line", "Grenze"), boundary.kind == SafetyBoundaryKind::Line)) { boundary.kind = SafetyBoundaryKind::Line; _controller.markCampaignEdited(); }
                if (ImGui::Selectable(ttText("Veil", "Schleier"), boundary.kind == SafetyBoundaryKind::Veil)) { boundary.kind = SafetyBoundaryKind::Veil; _controller.markCampaignEdited(); }
                ImGui::EndCombo();
            }
            FormUi::InputText(ttText("Topic", "Thema"), &boundary.topic);
            FormUi::InputTextMultiline(ttText("Guidance", "Hinweis"), &boundary.guidance,
                                       ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
            FormUi::Checkbox(ttText("Active", "Aktiv"), &boundary.active);
            FormUi::Checkbox(ttText("Share summary with players", "Zusammenfassung mit Spielern teilen"), &boundary.shareWithPlayers);
            if (drawUiButton(ttText("Save boundary", "Grenze speichern"), UiButtonRole::Primary))
            { boundary.metadata.touch(); _controller.persist("update safety boundary"); }
            ImGui::SameLine(); if (drawUiButton(ttText("Remove", "Entfernen"), UiButtonRole::Destructive)) removeIndex = static_cast<int>(index);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (removeIndex >= 0)
    {
        auto &values = _controller._campaign.safetyProfile.boundaries;
        values.erase(values.begin() + removeIndex); _controller.persist("remove safety boundary");
    }
    if (ImGui::CollapsingHeader(ttText("Add line or veil", "Line oder Veil hinzufügen")))
    {
        DraftEditSignal::Scope newDraftEdits([] {});
        const char *kind = _controller._newBoundary.kind == SafetyBoundaryKind::Line ? "Line" : "Veil";
        if (FormUi::BeginCombo(ttText("Boundary##New", "Grenze##New"), kind))
        {
        if (ImGui::Selectable(ttText("Line", "Grenze"), _controller._newBoundary.kind == SafetyBoundaryKind::Line)) _controller._newBoundary.kind = SafetyBoundaryKind::Line;
        if (ImGui::Selectable(ttText("Veil", "Schleier"), _controller._newBoundary.kind == SafetyBoundaryKind::Veil)) _controller._newBoundary.kind = SafetyBoundaryKind::Veil;
            ImGui::EndCombo();
        }
        FormUi::InputText(ttText("Topic##New", "Thema##New"), &_controller._newBoundary.topic);
        FormUi::InputTextMultiline(ttText("Guidance##New", "Hinweis##New"), &_controller._newBoundary.guidance,
                                   ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f), ImGuiInputTextFlags_WordWrap);
        FormUi::Checkbox(ttText("Share summary with players##New", "Zusammenfassung mit Spielern teilen##New"), &_controller._newBoundary.shareWithPlayers);
        if (drawUiButton(ttText("Save new boundary", "Neue Grenze speichern"), UiButtonRole::Primary)) _controller.addBoundary();
        ImGui::SameLine();
        drawDiscardPendingDraft(TabletopToolkitPendingDraft::Boundary, ttText("boundary", "Grenze"));
    }
}
