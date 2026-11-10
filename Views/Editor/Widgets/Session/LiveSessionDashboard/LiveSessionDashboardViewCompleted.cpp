#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardViewSupport.hpp"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"

void LiveSessionDashboardView::drawCompletedRecapEntry()
{
    if (_controller._currentCampaign.presentationModeActive || !SessionReport::latestCompleted(_controller._currentCampaign)) return;
    if (drawUiButton(uiTextForLocale("View completed recap", "Abgeschlossenen Rückblick ansehen"), UiButtonRole::Secondary, ImVec2(-1,0))) {
        const auto *selected = SessionReport::resolve(_controller._currentCampaign, _controller._sessionReview.completedId);
        _controller.viewCompletedRecap(selected && SessionReport::completed(*selected) ? selected->metadata.id : EntityId{});
    }
}

bool LiveSessionDashboardView::drawCompletedRecap()
{
    if (_controller._currentCampaign.presentationModeActive) return true;
    auto &owner = _controller._sessionReview;
    if (drawUiButton(uiTextForLocale("Choose completed session", "Abgeschlossene Sitzung wählen"),
        SessionReport::latestCompleted(_controller._currentCampaign) != nullptr,
        uiTextForLocale("No completed session is available.", "Keine abgeschlossene Sitzung verfügbar."), UiButtonRole::Secondary))
        ImGui::OpenPopup("CompletedSessionSelection");
    TaskUi::preparePickerWindow();
    if (ImGui::BeginPopup("CompletedSessionSelection")) {
        for (const auto &session : _controller._currentCampaign.sessions) if (SessionReport::completed(session)) {
            ImGui::PushID(session.metadata.id.c_str());
            if (drawUiButton(session.title.c_str(), UiButtonRole::Secondary, ImVec2(-1,0))) {
                _controller.viewCompletedRecap(session.metadata.id); ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    if (!owner.completedId.empty()) {
        if (drawUiButton(uiTextForLocale("Use current", "Aktuelle verwenden"), UiButtonRole::Secondary)) _controller.useCurrentRecap();
    }
    const auto *session = SessionReport::resolve(_controller._currentCampaign, owner.completedId);
    if (!session) {
        ImGui::TextWrapped("%s", uiTextForLocale("No available session selected. Choose another session or use current.", "Keine verfügbare Sitzung ausgewählt. Bitte eine andere Sitzung oder die aktuelle verwenden."));
        return true;
    }
    if (!SessionReport::completed(*session)) return false;
    ImGui::TextWrapped("%s: %s", uiTextForLocale("Completed recap · read only", "Abgeschlossener Rückblick · schreibgeschützt"), session->title.c_str());
    const auto field = [](const char *en, const char *de, const std::string &text) {
        ImGui::SeparatorText(uiTextForLocale(en,de)); ImGui::TextWrapped("%s", text.empty() ? "—" : text.c_str());
    };
    field("Summary", "Zusammenfassung", session->summary);
    field("Recap", "Rückblick", session->recap);
    field("Unresolved threads", "Offene Handlungsfäden", session->unresolvedThreads);
    field("Loot awarded", "Vergebene Beute", session->lootAwarded);
    field("XP / Milestones", "EP / Meilensteine", session->xpMilestones);
    field("NPCs met", "Getroffene NSC", session->npcsMet);
    field("Locations visited", "Besuchte Orte", session->locationsVisited);
    field("Open hooks", "Offene Aufhänger", session->openHooks);
    return true;
}

void LiveSessionDashboardView::drawPostSessionReport()
{
    if (_controller._currentCampaign.presentationModeActive) return;
    auto &owner = _controller._sessionReview;
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LivePostSessionReport));
    const auto *target = SessionReport::resolve(_controller._currentCampaign, owner.completedId);
    if (target) ImGui::TextWrapped("%s: %s", uiTextForLocale("Report session", "Berichtssitzung"), target->title.c_str());
    if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveGenerateReport), target != nullptr,
        uiTextForLocale("Select an available active or completed session.", "Eine verfügbare aktive oder abgeschlossene Sitzung wählen."), UiButtonRole::Advanced, ImVec2(-1,0))) {
        owner.report = SessionReport::review(_controller._currentCampaign, owner.completedId); owner.reportError.clear();
        ImGui::OpenPopup(gm::localization::tr(gm::localization::TextId::LivePostSessionReport));
    }
    setResponsiveAutoSizeWindow(620,400);
    if (ImGui::BeginPopupModal(gm::localization::tr(gm::localization::TextId::LivePostSessionReport), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!owner.report.valid) { ImGui::CloseCurrentPopup(); ImGui::EndPopup(); return; }
        ImGui::TextWrapped("%s", uiTextForLocale("Generate a private GM report from saved fields and notes. Unsaved drafts are not included. The timeline covers the whole campaign.",
            "Privaten SL-Bericht aus gespeicherten Feldern und Notizen erzeugen. Ungespeicherte Entwürfe sind nicht enthalten. Der Zeitverlauf umfasst die ganze Kampagne."));
        drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveSession), owner.report.title.c_str());
        drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveState), owner.report.completed ? uiTextForLocale("Completed", "Abgeschlossen") : uiTextForLocale("Active", "Aktiv"));
        if (!owner.reportError.empty()) ImGui::TextWrapped("%s", owner.reportError.c_str());
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveGenerate), UiButtonRole::Primary))
            if (_controller.generatePostSessionReport()) ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::CommonCancel), UiButtonRole::Secondary)) {
            owner.report = {}; owner.reportError.clear(); ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (!_controller._postSessionReportDraft.empty()) {
        ImGui::TextWrapped("%s: %s", uiTextForLocale("Generated report", "Erzeugter Bericht"), owner.output.title.c_str());
        FormUi::InputTextMultiline("##PostSessionReport", &_controller._postSessionReportDraft, ImVec2(-1,180), ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_ReadOnly);
    }
}
