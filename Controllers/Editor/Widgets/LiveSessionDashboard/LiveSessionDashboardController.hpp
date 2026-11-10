#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Controllers/Editor/TabletopToolkit/TabletopToolkitController.hpp"
#include "Models/Audio/AudioPlaybackService.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/Project/EntityRegistry.hpp"
#include "Models/Project/TextDraftRecovery.hpp"
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Session/SessionReportTarget.hpp"
#include "Models/Random/RandomTableService.hpp"
#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardView.hpp"

#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

class LiveSessionDashboardController : public IWidgetController
{
public:
    LiveSessionDashboardController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService,
                                   std::shared_ptr<TabletopToolkitHistoryState> toolkitHistory = {});

    void update(std::shared_ptr<GLFWwindow> window) override;
    void setOpenStoryHandler(std::function<void(const EntityId &, const EntityId &)> handler);
    void setOpenEntityHandler(std::function<void(std::string, EntityId)> handler);
    void setOpenMapHandler(std::function<void(const EntityId &, bool)> handler);
    void setOpenEncounterHandler(std::function<void(const EntityId &)> handler);
    void setRenderCombatHandler(std::function<void(const EntityId &)> handler);
    void setOpenNpcHandler(std::function<void()> handler);
    void setOpenItemHandler(std::function<void()> handler);
    void setOpenAudioMiniHandler(std::function<void()> handler);
    void setOpenAudioLibraryHandler(std::function<void()> handler);
    void setPreparationTargetHandler(PreparationTargetSink handler);
    void setPreparationContextRevokedHandler(std::function<void()> handler);
    bool matchesPreparationTarget(const PreparationTargetRequest &request) const;
    void reportPreparationTarget(const PreparationTargetRequest &, const PreparationTargetResult &);
    void clearPreparationTarget();
    bool checkpointPreparationNavigation();
    void setPresentationModeActive(bool active);
    LiveSessionDashboardPhase selectedPhase() const;
    void selectPhase(LiveSessionDashboardPhase phase);
    void renderEmbedded();
    void requestRunSessionTab();
    void openTabletopToolkit();
    void openObjectives();
    void openClues();
    bool focusObjective(const EntityId &objectiveId);
    bool focusClue(const EntityId &clueId);
    bool hasUnsavedDrafts() const;
    bool flushDrafts();
    void onLiveCampaignCommitted();
    bool checkpointOutcomeDrafts() { return !_view || _view->checkpointOutcomeDrafts(_currentCampaign); }
    bool hasPendingOutcomeDraftWrites() const { return _view && _view->hasPendingOutcomeDraftWrites(); }
    void checkpointDraftRecovery() { checkpointTextRecovery(true); if (_view) _view->checkpointOutcomeDrafts(_currentCampaign); }
    void resetDrafts();
    // Call only after a new session's start transaction has succeeded.
    void afterSessionStarted(const EntityId &sessionId, const EntityId &sceneId = {});
    void endActiveSession();
    void startSessionBreak(bool pauseAudio = false);
    void resumeSessionFromBreak(bool resumeAudio = false);

private:
    friend class LiveSessionDashboardView;
    friend struct TabletopHistoryProbe;
    friend struct SessionTargetProbe;

    Campaign &_currentCampaign;
    std::shared_ptr<AudioPlaybackService> _playbackService;
    std::unique_ptr<LiveSessionDashboardView> _view;
    bool flushSessionDrafts();

    std::unique_ptr<TabletopToolkitController> _tabletopToolkit;
    std::mt19937 _randomGenerator;

    int _selectedAudioAssetIndex = -1;
    float _audioVolume = 1.0f;
    bool _audioLoop = false;
    std::string _audioStatusMessage;
    std::string _audioErrorMessage;
    int _selectedSceneIndex = -1;
    std::string _sceneLaunchMessage;
    EntityId _selectedPlanEntryId;
    std::string _sessionPlanMessage;
    std::string _sessionSummaryDraft;
    EntityId _sessionSummaryDraftId;
    std::string _sessionNoteDraft;
    std::string _sessionNoteSearch;
    EntityId _textRecoveryCampaign;
    bool _textRecoveryRead = false, _textRecoveryPending = false;
    double _textRecoveryAt = -10.0;
    TextDraftRecovery::Fields _textRecoveryBase, _textRecoveryFields, _textRecoveredFields;
    std::string _textRecoveryError;
    void prepareTextRecovery();
    void checkpointTextRecovery(bool force = false);
    bool restoreTextRecovery();
    void clearTextRecovery();
    EntityId _selectedNoteEntityId;
    LiveSessionDashboardPhase _selectedDashboardPhase = LiveSessionDashboardPhase::Prep;
    LiveSessionDashboardPhase _lastInferredDashboardPhase = LiveSessionDashboardPhase::Prep;
    bool _dashboardPhaseManuallySelected = false;
    std::string _clockDraftSessionId;
    std::string _clockAdvanceDraft = "10 minutes";
    std::string _clockReasonDraft;
    std::string _clockStatusMessage;
    std::string _clockConfigDraftKey;
    std::string _calendarNameDraft;
    int _clockYearDraft = 1;
    int _clockMonthDraft = 1;
    int _clockDayDraft = 1;
    int _clockHourDraft = 0;
    int _clockMinuteDraft = 0;
    int _calendarMonthCountDraft = 12;
    int _calendarWeekdayCountDraft = 7;
    std::vector<std::string> _calendarMonthNameDrafts;
    std::vector<int> _calendarMonthDayDrafts;
    std::vector<std::string> _calendarWeekdayNameDrafts;
    int _calendarHoursPerDayDraft = 24;
    int _calendarDaylightHoursDraft = 12;
    int _calendarNightHoursDraft = 12;
    int _calendarDayStartsAtHourDraft = 6;
    double _clockMultiplierDraft = 1.0;
    std::string _recapDraft;
    std::string _unresolvedThreadsDraft;
    std::string _lootAwardedDraft;
    std::string _xpMilestonesDraft;
    std::string _npcsMetDraft;
    std::string _locationsVisitedDraft;
    std::string _openHooksDraft;
    std::string _timelineTitleDraft;
    std::string _timelineDateDraft;
    std::string _timelineDescriptionDraft;
    std::string _postSessionReportDraft;
    SessionReport::Owner _sessionReview;
    int _lastD20Roll = 0;
    int _lastD100Roll = 0;
    int _selectedRandomTableIndex = 0;
    std::vector<RandomTableRollResult> _randomTableRollHistory;
    std::string _randomTableStatus;

    SessionModel *activeSession();
    const SessionModel *activeSession() const;
    std::string defaultSessionTitle() const;
    const StoryThreadModel *activeStoryThread() const;
    const StoryBeatModel *activeStoryBeat() const;
    const MapModel *activeMap() const;
    const EncounterModel *activeEncounter() const;
    const AudioAssetModel *selectedAudioAsset() const;
    AudioAssetModel *selectedAudioAsset();
    std::vector<const StoryBeatModel *> sceneOptions() const;
    LiveSessionDashboardPhase inferredDashboardPhase() const;
    LiveSessionDashboardPhase dashboardPhase() const;

    void configure();
    void selectDashboardPhase(LiveSessionDashboardPhase phase);
    void reviewSessionStart();
    bool startSession();
    bool viewCompletedRecap(const EntityId &id = {});
    void useCurrentRecap();
    bool isSessionBreakActive() const;
    std::string currentInWorldClockLabel() const;
    std::string currentClockPhaseLabel() const;
    std::string clockDraftKey() const;
    void syncClockDraftsFromCampaign();
    void resizeCalendarDrafts();
    CampaignCalendarModel draftCalendar() const;
    InWorldDateTime draftDateTime() const;
    void advanceSessionClock();
    void adjustSessionClock();
    void saveCalendarSettings();
    void toggleClockRunning();
    void applyClockMultiplier();
    void resetClockToSystemTime();
    void launchSelectedScene();
    void launchPlanEntry(const EntityId &entryId);
    void playCommittedScene(const EntityId &sceneId);
    void markPlanEntry(const EntityId &entryId, SessionEntryState state);
    SessionPlan::Persist sessionPersist(const std::string &reason);
    bool acceptSessionResult(const SessionPlan::Result &result);
    SessionPlan::WrapUpDraft currentWrapUpDraft() const;
    void openActiveEncounter();
    void openEncounterPreparation();
    void renderActiveCombatWorkspace();
    void selectAudioAsset(int index);
    void playSelectedAudio();
    void pauseSelectedAudio();
    void resumeSelectedAudio();
    void stopSelectedAudio();
    void stopAllAudio();
    void applySelectedAudioVolume();
    bool isSelectedAudioActive() const;
    bool isSelectedAudioPaused() const;
    void openAudioMini();
    void openAudioLibrary();
    void saveSessionSummary();
    std::vector<EntityRegistryEntry> noteLinkCandidates() const;
    void addSessionNote();
    void removeSessionNote(const EntityId &noteId);
    void saveSessionRecap();
    std::vector<std::string> sessionTemplateLabels() const;
    void applySessionTemplate(int templateIndex);
    void addTimelineEvent();
    void removeTimelineEvent(int eventIndex);
    bool generatePostSessionReport();
    int rollDie(int sides);
    void rollSelectedRandomTable();
    bool saveLatestRandomTableRollToNotes();
    bool createLatestRandomTableHandout();

    AudioAssetModel *findAudioAsset(const EntityId &assetId);
    const AudioAssetModel *findAudioAsset(const EntityId &assetId) const;
    bool playAudioAsset(const EntityId &assetId, float volume, bool loop, const std::string &contextLabel);
    bool ensureSelectedAudioFileAvailable(AudioAssetModel &asset);
    void saveCampaign(const std::string &reason);
    void onShow() override;

    std::function<void(const EntityId &, const EntityId &)> _openStoryHandler;
    std::function<void(std::string, EntityId)> _openEntityHandler;
    std::function<void(const EntityId &, bool)> _openMapHandler;
    std::function<void(const EntityId &)> _openEncounterHandler;
    std::function<void(const EntityId &)> _renderCombatHandler;
    std::function<void()> _openNpcHandler;
    std::function<void()> _openItemHandler;
    std::function<void()> _openAudioMiniHandler;
    std::function<void()> _openAudioLibraryHandler;
    PreparationTargetSink _preparationTargetHandler;
    std::function<void()> _preparationContextRevokedHandler;
};
