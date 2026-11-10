#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardViewSupport.hpp"

void LiveSessionDashboardView::drawOutcomesWorkspace()
{
    auto &campaign = _controller._currentCampaign;
    if (campaign.presentationModeActive)
    {
        _outcomes.tickDrafts(campaign);
        ImGui::TextWrapped("%s", uiTextForLocale("Private outcomes are hidden. Return to GM mode in Live to edit them. Drafts are retained.",
            "Private Ergebnisse sind ausgeblendet. Wechsle in Live zurück zur SL-Ansicht, um sie zu bearbeiten. Entwürfe bleiben erhalten."));
        return;
    }
    const auto *active = _controller.activeSession();
    const auto activeId = active ? active->metadata.id : EntityId{};
    _outcomeSelection.synchronize(campaign, activeId);
    if (_outcomeSelection.historical)
    {
        ImGui::TextWrapped("%s", uiTextForLocale("Different session selected. New outcomes will be recorded in that session; your other drafts are retained.",
            "Andere Sitzung ausgewählt. Neue Ergebnisse werden dort festgehalten; andere Entwürfe bleiben erhalten."));
        if (ImGui::Button(uiTextForLocale("Follow current session", "Aktueller Sitzung folgen")))
        {
            _outcomeSelection.followCurrent(); _outcomeSelection.synchronize(campaign, activeId);
        }
    }
    else ImGui::TextWrapped("%s", active ? uiTextForLocale("Following the current live session.", "Folgt der aktuell laufenden Sitzung.") :
        uiTextForLocale("No live session. Review the selected session; a newly started session will be followed automatically.",
            "Keine laufende Sitzung. Prüfe die ausgewählte Sitzung; einer neu gestarteten Sitzung wird automatisch gefolgt."));
    const auto *session = SessionPlan::findSession(campaign, _outcomeSelection.sessionId);
    ImGui::TextUnformatted(uiTextForLocale("Record and review in session", "Ergebnisse festhalten und prüfen in Sitzung"));
    if (FormUi::BeginCombo("##outcome-session", session ? session->title.c_str() : uiTextForLocale("Choose a session", "Sitzung wählen")))
    {
        for (const auto &candidate : campaign.sessions)
            if (std::find(candidate.metadata.tags.begin(), candidate.metadata.tags.end(), "archived") == candidate.metadata.tags.end())
                if (ImGui::Selectable((candidate.title + "##" + candidate.metadata.id).c_str(), candidate.metadata.id == _outcomeSelection.sessionId))
                {
                    _outcomeSelection.choose(candidate.metadata.id, activeId);
                }
        ImGui::EndCombo();
    }
    session = SessionPlan::findSession(campaign, _outcomeSelection.sessionId);
    const auto *entry = session ? SessionPlan::findEntry(*session, session->liveState.activePlanEntryId) : nullptr;
    const auto scene = entry ? entry->storyBeatId : EntityId{};
    _outcomes.draw(campaign, _outcomeSelection.sessionId, _controller.sessionPersist("session outcomes"), scene,
        _controller._preparationTargetHandler);
}
