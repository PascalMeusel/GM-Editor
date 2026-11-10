#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Controllers/Editor/TabletopToolkit/TabletopToolkitController.hpp"
#include "Models/Audio/AudioPlaybackService.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Random/RandomTableService.hpp"
#include "Models/Session/SessionStartReview.hpp"
#include "Views/Editor/Widgets/Session/GMScreen/GMScreenView.hpp"

#include <memory>
#include <random>
#include <string>
#include <vector>

class GMScreenController : public IWidgetController
{
public:
    GMScreenController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService = nullptr,
                       std::shared_ptr<TabletopToolkitHistoryState> toolkitHistory = {});

    void update(std::shared_ptr<GLFWwindow> window) override;
    bool hasUnsavedDrafts() const;
    bool flushDrafts();
    void onLiveCampaignCommitted();
    void resetDrafts();
    void resetPlayerDisplayReview(bool clearRestore=true);
    void hide() { resetPlayerDisplayReview(false); IWidgetController::hide(); }

private:
    friend class GMScreenView;
    friend struct TabletopHistoryProbe;
    friend struct SessionTargetProbe;
    friend struct PlayerReviewUiProbe;

    Campaign &_currentCampaign;
    std::unique_ptr<GMScreenView> _view;
    std::unique_ptr<TabletopToolkitController> _tabletopToolkit;
    std::shared_ptr<AudioPlaybackService> _playbackService;
    std::mt19937 _randomGenerator;
    int _lastD20Roll = 0;
    int _lastD100Roll = 0;
    int _selectedRandomTableIndex = 0;
    std::vector<RandomTableRollResult> _randomTableRollHistory;
    std::string _randomTableStatus;
    std::string _sessionTitleDraft;
    std::string _sessionOperationError;
    SessionPlan::StartReview _sessionStartReview;
    std::string _rulesBookmarkDraft;
    std::string _handoutTitleDraft;
    std::string _handoutBodyDraft;
    std::string _handoutFilePathDraft;
    std::vector<std::string> _rulesBookmarks;

    SessionModel *activeSession();
    const SessionModel *activeSession() const;
    std::string defaultSessionTitle() const;
    void reviewSessionStart();
    bool startSession();
    void endActiveSession();
    bool isSessionBreakActive() const;
    void startSessionBreak(bool pauseAudio = false);
    void resumeSessionFromBreak(bool resumeAudio = false);
    int rollDie(int sides);
    void rollSelectedRandomTable();
    bool saveLatestRandomTableRollToNotes();
    bool createLatestRandomTableHandout();
    void addRulesBookmark();
    void removeRulesBookmark(int index);
    std::vector<ItemModel> getAvailableItems() const;
    void touchLocalPlayerDisplay();
    const MapModel *reviewedPublicMap() const;
    bool armLocalPlayerDisplayFromReview(const PlayerDisplayReviewSnapshot &review);
    bool publishLocalPlayerDisplay(const PlayerDisplayReviewSnapshot &review);
    void blackoutLocalPlayerDisplayOutput();
    bool pingLocalPlayerDisplay();
    void setPublicScene(const EntityId &sceneId);
    void setPublicMap(const EntityId &mapId);
    void setPublicMapReleaseCategory(MapPlayerReleaseCategory category, bool visible);
    void setPublicEncounter(const EntityId &encounterId);
    void clearLocalPlayerDisplay();
    void setNpcPublicVisibility(const EntityId &npcId, bool visible);
    void addHandout();
    void browseHandoutAttachment();
    void removeHandout(int index);
    void setHandoutPublic(const EntityId &handoutId, bool visible);
    void setPlayerSpellSlotMaximum(int characterIndex, int spellLevel, int value);
    void setPlayerSpellSlotUsed(int characterIndex, int spellLevel, int value);
    void resetPlayerSpellSlots(int characterIndex);
    void setNpcSpellSlotMaximum(int npcIndex, int spellLevel, int value);
    void setNpcSpellSlotUsed(int npcIndex, int spellLevel, int value);
    void resetNpcSpellSlots(int npcIndex);
    void onShow() override;
};
