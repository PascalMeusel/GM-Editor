#include "Controllers/Editor/TabletopToolkit/TabletopToolkitController.hpp"
#include "Models/Project/CampaignEditHistory.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include <algorithm>

namespace
{
template<class T> void retainBefore(std::vector<T> &baseline, const std::vector<T> &current, const T &before)
{
    const auto matches = [&](const auto &value) { return value.metadata.id == before.metadata.id; };
    const auto found = std::find_if(baseline.begin(), baseline.end(), matches);
    if (found != baseline.end()) *found = before;
    else
    {
        // A record added elsewhere while another edit was pending still needs
        // its actual position, not the end of the older baseline.
        const auto position = std::find_if(current.begin(), current.end(), matches) - current.begin();
        baseline.insert(baseline.begin() + std::min<std::ptrdiff_t>(baseline.size(), position), before);
    }
}
const char *historyText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}
template<class T> std::string editLabel(const std::vector<T> &before, const std::vector<T> &after,
                                      const std::set<EntityId> &ids, bool objectives)
{
    if (ids.size() == 1)
    {
        const auto matches = [&](const auto &value) { return value.metadata.id == *ids.begin(); };
        if (std::none_of(before.begin(), before.end(), matches))
            return objectives ? historyText("Add objective", "Ziel hinzufügen") : historyText("Add clue", "Hinweis hinzufügen");
        if (std::none_of(after.begin(), after.end(), matches))
            return objectives ? historyText("Remove objective", "Ziel entfernen") : historyText("Remove clue", "Hinweis entfernen");
        return objectives ? historyText("Edit objective", "Ziel bearbeiten") : historyText("Edit clue", "Hinweis bearbeiten");
    }
    return objectives ? historyText("Edit objectives", "Ziele bearbeiten") : historyText("Edit clues", "Hinweise bearbeiten");
}
}

void TabletopToolkitController::resetHistoryBaseline()
{
    _history->owner = &_campaign;
    _history->objectives = _campaign.storyObjectives;
    _history->clues = _campaign.storyClues;
    _history->pending = false;
    _history->objectiveIds.clear(); _history->clueIds.clear();
}

void TabletopToolkitController::trackObjective(const StoryObjectiveModel &before)
{
    if (_history->objectiveIds.insert(before.metadata.id).second)
        retainBefore(_history->objectives, _campaign.storyObjectives, before);
    _history->pending = true;
}
void TabletopToolkitController::trackClue(const StoryClueModel &before)
{
    if (_history->clueIds.insert(before.metadata.id).second)
        retainBefore(_history->clues, _campaign.storyClues, before);
    _history->pending = true;
}

bool TabletopToolkitController::commitPendingHistory()
{
    if (!_history->pending && _history->objectiveIds.empty() && _history->clueIds.empty()) return true;
    const auto persist = [](Campaign &staged) {
        return ProjectSaveService::getInstance().saveCampaign(staged, "apply tabletop edit history");
    };
    // Full collections retain insertion positions; only explicitly tracked IDs
    // enter the delta. Clear each baseline only after registration acknowledges
    // it, including when the command manager is busy applying another command.
    try
    {
        if (CampaignEditHistory::recordObjectives(_campaign, _history->objectives, _campaign.storyObjectives, persist,
            editLabel(_history->objectives, _campaign.storyObjectives, _history->objectiveIds, true), &_history->objectiveIds))
        { _history->objectiveIds.clear(); _history->objectives = _campaign.storyObjectives; }
        if (CampaignEditHistory::recordClues(_campaign, _history->clues, _campaign.storyClues, persist,
            editLabel(_history->clues, _campaign.storyClues, _history->clueIds, false), &_history->clueIds))
        { _history->clueIds.clear(); _history->clues = _campaign.storyClues; }
    }
    catch (const std::exception &) { /* A history failure cannot undo the completed save. */ }
    _history->pending = !_history->objectiveIds.empty() || !_history->clueIds.empty();
    if (_history->pending) _statusMessage = historyText("Saved. Undo registration is pending; save again to retry.",
        "Gespeichert. Der Rückgängig-Eintrag steht noch aus; speichere erneut, um ihn einzutragen.");
    return !_history->pending;
}
