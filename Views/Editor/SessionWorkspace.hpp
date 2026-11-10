#pragma once

#include "Models/Session/SessionPlanService.hpp"
#include "Models/Project/TextDraftRecovery.hpp"
#include "Views/Editor/SessionOutcomesPanel.hpp"
#include "Views/Editor/SessionPreparationTarget.hpp"
#include <functional>

struct ImGuiInputTextCallbackData;

// View-owned drafts survive failed saves and excursions into campaign editors.
class SessionWorkspace
{
public:
    struct Actions
    {
        SessionPlan::Persist persist;
        std::function<void()> run;
        std::function<void(const EntityId &, const EntityId &)> started;
        std::function<void(const std::string &, const EntityId &)> openEntity;
        std::function<void()> newMap, newCharacter, newNpc, newEncounter, presentation, blackout;
        PreparationTargetSink prepareTarget;
    };
    void draw(Campaign &, const Actions &);
    bool flush(Campaign &, const SessionPlan::Persist &);
    // Navigation only: never persist, discard, or substitute a different ID.
    PreparationTargetResult inspectPreparationTarget(Campaign &, const EntityId &);
    PreparationTargetResult openPreparationTarget(Campaign &, const EntityId &);
    bool matchesPreparationTarget(const PreparationTargetRequest &request) const;
    void reportPreparationTarget(const PreparationTargetRequest &, const PreparationTargetResult &);
    void clearPreparationTarget();
    bool restoreSelection(Campaign &, const EntityId &sessionId, const EntityId &entryId,
                          bool unavailable, const SessionPlan::Persist &);
    bool attachMap(Campaign &, const EntityId &mapId, const SessionPlan::Persist &);
    bool dirty() const { return _sessionDirty || _sceneDirty || _entryDirty; }
    bool hasPendingOutcomeDraftWrites() const { return _outcomes.hasPendingDraftWrites(); }
    bool checkpointOutcomeDrafts(Campaign &campaign) { return _outcomes.checkpointDrafts(campaign); }
    const EntityId &sessionId() const { return _sessionId; }
    const EntityId &planEntryId() const { return _entryId; }
    const EntityId &sceneId() const { return _sceneId; }
private:
    SessionOutcomesPanel _outcomes{SessionOutcomeDraftKeys::Surface::Preparation};
    EntityId _campaignId, _sessionId, _entryId, _sceneId;
    EntityId _handoutId;
    std::string _title, _notes, _scheduled, _selectedNotes, _entryNote;
    std::string _newSessionTitle, _newSceneTitle;
    StoryBeatModel _scene, _sceneBase;
    bool _sessionDirty = false, _sceneDirty = false, _entryDirty = false, _error = false;
    bool _draftConflict = false;
    bool _selectionUnavailable = false;
    double _lastEdit = 0.0;
    bool _recoveryRead = false, _recoveryPending = false, _recoveryReview = false;
    double _lastRecoveryAt = -10.0;
    TextDraftRecovery::Fields _recoveredFields, _lastRecoveryFields, _sessionTextBase;
    std::string _recoveryError;
    bool drawRecovery(Campaign &);
    void readRecovery(Campaign &);
    void checkpointRecovery(Campaign &, bool force = false);
    bool restoreRecovery(Campaign &);
    void refreshRecoveryBase(const Campaign &);
    std::string _linkFilter;
    SessionPlan::Status _lastStatus = SessionPlan::Status::Applied;
    const char *statusText() const;
    int _compactPane = 0, _linkCategory = 0;
    int _lastCompactPane = -1;
    bool _resetCompactScroll = true;
    bool _showClues = false, _showConsequences = false, _showPlayerSummary = false, _showEntryNote = false;
    bool _showRoughNotes = true;
    void load(Campaign &, const EntityId &, const EntityId & = {});
    bool report(const SessionPlan::Result &);
    void changed(bool &);
    void drawSequence(Campaign &, const SessionModel &, const Actions &);
    void drawScene(Campaign &, const SessionModel &, const Actions &);
    void drawRoughNotes(Campaign &, const SessionModel &, const Actions &);
    void drawLinks(Campaign &, const Actions &);
    static int captureSelection(ImGuiInputTextCallbackData *);
};
