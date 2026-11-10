#include "Views/Editor/SessionOutcomesPanelSupport.hpp"
#include "Models/Project/ApplicationPaths.hpp"

bool SessionOutcomesPanel::matchesPreparationTarget(const PreparationTargetRequest &request) const
{
    return _preparationTarget && _campaignId == request.campaignId && _drawnSession == request.sourceSessionId &&
        _preparationRoot == ApplicationPaths::writableRoot() &&
        _preparationTarget->campaignId == request.campaignId &&
        _preparationTarget->sourceSessionId == request.sourceSessionId &&
        _preparationTarget->targetSessionId == request.targetSessionId &&
        _preparationTarget->carryRequestId == request.carryRequestId &&
        _preparationTarget->sourceSurface == request.sourceSurface;
}

void SessionOutcomesPanel::reportPreparationTarget(const PreparationTargetRequest &request,
                                                  const PreparationTargetResult &result)
{
    if (matchesPreparationTarget(request))
    {
        _message = result.message;
        _preparationNavigationError = result.opened() ? std::string{} : result.message;
    }
}

void SessionOutcomesPanel::clearPreparationTarget()
{
    _preparationTarget.reset(); _preparationTargetTitle.clear(); _preparationRoot.clear();
    _preparationNavigationError.clear(); ++_preparationEpoch;
    _preparationOpenEmitted = false;
}

void SessionOutcomesPanel::drawPreparationTarget(Campaign &campaign, const PreparationTargetSink &sink)
{
    if (!_preparationTarget || !matchesPreparationTarget(*_preparationTarget) || campaign.presentationModeActive) return;
    ImGui::Separator();
    ImGui::TextWrapped("%s: %s", ot("Last saved destination", "Zuletzt gespeicherte Zielsitzung"), _preparationTargetTitle.c_str());
    if (!sink) return;
    if (ob("Prepare this session", "Diese Sitzung vorbereiten", true))
    {
        if (!checkpointDrafts(campaign))
        {
            _message = ot("The outcome draft could not be checkpointed. Retry before opening preparation.",
                "Der Ergebnisentwurf konnte nicht gesichert werden. Versuche es vor dem Öffnen der Vorbereitung erneut.");
            _preparationNavigationError = _message;
            return;
        }
        const auto request = *_preparationTarget;
        sink(request);
        _preparationOpenEmitted = matchesPreparationTarget(request);
    }
    if (!_preparationNavigationError.empty()) ImGui::TextWrapped("%s", _preparationNavigationError.c_str());
}
