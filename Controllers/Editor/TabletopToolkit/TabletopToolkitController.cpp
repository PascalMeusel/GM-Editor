#include "Controllers/Editor/TabletopToolkit/TabletopToolkitController.hpp"

#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Session/TabletopToolkit/TabletopToolkitService.hpp"
#include "Views/Editor/TabletopToolkit/TabletopToolkitView.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
namespace
{
const char *toolkitText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
               ? german
               : english;
}
}

TabletopToolkitController::TabletopToolkitController(Campaign &campaign, std::shared_ptr<TabletopToolkitHistoryState> history)
    : _campaign(campaign), _generator(std::random_device{}()), _history(history ? std::move(history) : std::make_shared<TabletopToolkitHistoryState>())
{
    _rollLabel = toolkitText("Quick roll", "Schnellwurf");
    _newMacro.name.clear();
    _newObjective.title.clear();
    _newClue.title.clear();
    _newBoundary.topic.clear();
    _newWarning.warning.clear();
    _view = std::make_unique<TabletopToolkitView>(*this);
    if (!_history->owner) resetHistoryBaseline();
    else if (_history->owner != &_campaign) throw std::invalid_argument("Toolkit history belongs to another live campaign.");
}

TabletopToolkitController::~TabletopToolkitController() = default;
void TabletopToolkitController::drawEmbedded()
{
    if (!_history->pending) resetHistoryBaseline();
    _view->drawEmbedded();
}

bool TabletopToolkitController::persist(const std::string &reason)
{
    _campaign.metadata.touch();
    const bool saved = ProjectSaveService::getInstance().saveCampaign(_campaign, reason);
    _statusMessage = saved ? "Saved" : "Could not save changes";
    if (saved) commitPendingHistory();
    else _history->pending = true;
    return saved;
}

bool TabletopToolkitController::focusObjective(const EntityId &objectiveId)
{
    const auto found = std::find_if(_campaign.storyObjectives.begin(), _campaign.storyObjectives.end(),
                                    [&](const StoryObjectiveModel &objective) { return objective.metadata.id == objectiveId; });
    if (found == _campaign.storyObjectives.end())
        return false;
    _view->requestObjectiveFocus(objectiveId);
    return true;
}

bool TabletopToolkitController::focusClue(const EntityId &clueId)
{
    const auto found = std::find_if(_campaign.storyClues.begin(), _campaign.storyClues.end(),
                                    [&](const StoryClueModel &clue) { return clue.metadata.id == clueId; });
    if (found == _campaign.storyClues.end())
        return false;
    _view->requestClueFocus(clueId);
    return true;
}

void TabletopToolkitController::showObjectives()
{
    _view->requestObjectivesTab();
}

void TabletopToolkitController::showClues()
{
    _view->requestCluesTab();
}

bool TabletopToolkitController::hasPendingDrafts() const
{
    const bool macro = !_newMacro.name.empty() || !_newMacro.notes.empty() ||
                       _newMacro.expression != "1d20" || !_newMacro.addToSessionNotes;
    const bool objective = !_newObjective.title.empty() || !_newObjective.playerSummary.empty() ||
                           !_newObjective.gmNotes.empty() || _newObjective.status != StoryObjectiveStatus::Planned ||
                           _newObjective.playerVisible || !_newObjective.linkedStoryThreadId.empty() ||
                           !_newObjective.linkedStoryBeatId.empty() || !_newObjective.assigneeCharacterIds.empty() ||
                           !_newObjective.prerequisiteObjectiveIds.empty();
    const bool clue = !_newClue.title.empty() || !_newClue.playerText.empty() || !_newClue.gmTruth.empty() ||
                      _newClue.status != StoryClueStatus::Hidden || _newClue.playerVisible ||
                      !_newClue.sourceEntityId.empty() || !_newClue.sourceEntityType.empty() ||
                      !_newClue.discoveredAtUtc.empty();
    const bool boundary = !_newBoundary.topic.empty() || !_newBoundary.guidance.empty() ||
                          _newBoundary.kind != SafetyBoundaryKind::Line || _newBoundary.shareWithPlayers ||
                          !_newBoundary.active;
    const bool warning = !_newWarning.warning.empty() || !_newWarning.targetEntityId.empty() ||
                         !_newWarning.targetEntityType.empty() || !_newWarning.targetLabel.empty() ||
                         !_newWarning.shareWithPlayers;
    return macro || objective || clue || boundary || warning;
}

bool TabletopToolkitController::flushDraftsForCampaignSave()
{
    if (!hasPendingDrafts())
        return true;
    _statusMessage = toolkitText("Finish Save new… or Discard for each unfinished table-tool draft before saving or leaving.",
                                 "Schließe jeden unfertigen Tischwerkzeug-Entwurf mit Neu speichern… ab oder verwerfe ihn, bevor du speicherst oder wechselst.");
    return false;
}

void TabletopToolkitController::resetDrafts()
{
    resetHistoryBaseline();
    _newMacro = CampaignDiceMacroModel{};
    _newMacro.name.clear();
    _newObjective = StoryObjectiveModel{};
    _newObjective.title.clear();
    _newClue = StoryClueModel{};
    _newClue.title.clear();
    _newBoundary = SafetyBoundaryModel{};
    _newBoundary.topic.clear();
    _newWarning = SafetyContentWarningModel{};
    _newWarning.warning.clear();
}

void TabletopToolkitController::markCampaignEdited()
{
    _history->pending = true;
    ProjectSaveService::getInstance().markDirty("edit tabletop toolkit");
}

void TabletopToolkitController::discardPendingDraft(TabletopToolkitPendingDraft draft)
{
    switch (draft)
    {
    case TabletopToolkitPendingDraft::Macro:
        _newMacro = CampaignDiceMacroModel{};
        _newMacro.name.clear();
        break;
    case TabletopToolkitPendingDraft::Objective:
        _newObjective = StoryObjectiveModel{};
        _newObjective.title.clear();
        break;
    case TabletopToolkitPendingDraft::Clue:
        _newClue = StoryClueModel{};
        _newClue.title.clear();
        break;
    case TabletopToolkitPendingDraft::Boundary:
        _newBoundary = SafetyBoundaryModel{};
        _newBoundary.topic.clear();
        break;
    case TabletopToolkitPendingDraft::Warning:
        _newWarning = SafetyContentWarningModel{};
        _newWarning.warning.clear();
        break;
    }
    if (!hasPendingDrafts())
        _statusMessage.clear();
}

void TabletopToolkitController::rollNow(const std::string &expression, const std::string &label,
                                        bool addToSessionNotes)
{
    _lastRoll = TabletopToolkit::roll(_campaign, expression, label, addToSessionNotes, _generator);
    _statusMessage = _lastRoll.valid ? _lastRoll.message : _lastRoll.error;
    if (_lastRoll.valid && addToSessionNotes)
        persist("record tabletop dice roll");
}

void TabletopToolkitController::addMacro()
{
    std::string error;
    if (!TabletopToolkit::validDiceMacro(_campaign, _newMacro, &error))
    {
        _statusMessage = error;
        return;
    }
    _newMacro.metadata.touch();
    _campaign.diceMacros.push_back(_newMacro);
    _newMacro = CampaignDiceMacroModel{};
    _newMacro.name.clear();
    persist("add campaign dice macro");
}

void TabletopToolkitController::addObjective()
{
    if (_newObjective.title.empty())
    {
        _statusMessage = toolkitText("Give the objective a title.", "Gib dem Ziel einen Titel.");
        return;
    }
    _newObjective.metadata.touch();
    _history->objectiveIds.insert(_newObjective.metadata.id);
    _campaign.storyObjectives.push_back(_newObjective);
    _newObjective = StoryObjectiveModel{};
    _newObjective.title.clear();
    persist("add story objective");
}

void TabletopToolkitController::addClue()
{
    if (_newClue.title.empty() || _newClue.playerText.empty())
    {
        _statusMessage = toolkitText("Add a clue title and player-safe wording.",
                                     "Füge einen Hinweistitel und eine spielergeeignete Formulierung hinzu.");
        return;
    }
    _newClue.metadata.touch();
    _history->clueIds.insert(_newClue.metadata.id);
    _campaign.storyClues.push_back(_newClue);
    _newClue = StoryClueModel{};
    _newClue.title.clear();
    persist("add story clue");
}

void TabletopToolkitController::addBoundary()
{
    if (_newBoundary.topic.empty())
    {
        _statusMessage = toolkitText("Name the boundary topic.", "Benenne das Thema der Grenze.");
        return;
    }
    _newBoundary.metadata.touch();
    _campaign.safetyProfile.boundaries.push_back(_newBoundary);
    _newBoundary = SafetyBoundaryModel{};
    _newBoundary.topic.clear();
    persist("add safety boundary");
}

void TabletopToolkitController::addWarning()
{
    if (_newWarning.warning.empty())
    {
        _statusMessage = toolkitText("Describe the content warning.", "Beschreibe die Inhaltswarnung.");
        return;
    }
    if (_newWarning.targetEntityType.empty())
    {
        _newWarning.targetEntityId = _campaign.metadata.id;
        _newWarning.targetEntityType = "campaign";
        _newWarning.targetLabel = _campaign.name;
    }
    _newWarning.metadata.touch();
    _campaign.safetyProfile.warnings.push_back(_newWarning);
    _newWarning = SafetyContentWarningModel{};
    _newWarning.warning.clear();
    persist("add content warning");
}
