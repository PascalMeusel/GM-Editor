#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Session/TabletopToolkit/TabletopToolkitModels.hpp"
#include "Controllers/Editor/TabletopToolkit/TabletopToolkitHistoryState.hpp"

#include <memory>
#include <random>
#include <string>
#include <set>

class TabletopToolkitView;

enum class TabletopToolkitPendingDraft
{
    Macro,
    Objective,
    Clue,
    Boundary,
    Warning
};

class TabletopToolkitController
{
public:
    explicit TabletopToolkitController(Campaign &campaign, std::shared_ptr<TabletopToolkitHistoryState> history = {});
    ~TabletopToolkitController();

    void drawEmbedded();
    bool persist(const std::string &reason);
    bool focusObjective(const EntityId &objectiveId);
    bool focusClue(const EntityId &clueId);
    void showObjectives();
    void showClues();
    bool hasPendingDrafts() const;
    // A campaign-wide Save must not create a half-finished toolkit record.
    // Returning false leaves every new-form draft available to finish or
    // discard in its originating toolkit.
    bool flushDraftsForCampaignSave();
    // Only after this live campaign object has committed successfully.
    bool commitPendingHistory();
    void resetDrafts();

private:
    friend class TabletopToolkitView;
    friend struct TabletopHistoryProbe;

    Campaign &_campaign;
    std::unique_ptr<TabletopToolkitView> _view;
    std::mt19937 _generator;

    std::string _rollExpression = "1d20";
    std::string _rollLabel = "Quick roll";
    bool _rollToSessionNotes = true;
    TabletopDiceRollResult _lastRoll;
    std::string _statusMessage;

    CampaignDiceMacroModel _newMacro;
    StoryObjectiveModel _newObjective;
    StoryClueModel _newClue;
    SafetyBoundaryModel _newBoundary;
    SafetyContentWarningModel _newWarning;
    std::shared_ptr<TabletopToolkitHistoryState> _history;
    void trackObjective(const StoryObjectiveModel &before);
    void trackClue(const StoryClueModel &before);
    void resetHistoryBaseline();

    void rollNow(const std::string &expression, const std::string &label, bool addToSessionNotes);
    void addMacro();
    void addObjective();
    void addClue();
    void addBoundary();
    void addWarning();
    void markCampaignEdited();
    void discardPendingDraft(TabletopToolkitPendingDraft draft);
};
