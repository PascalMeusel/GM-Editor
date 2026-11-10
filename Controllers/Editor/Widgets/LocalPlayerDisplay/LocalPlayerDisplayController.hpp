#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Session/PlayerDisplayReviewConfirmation.hpp"
#include "Models/Session/PlayerDisplayPublicationAssets.hpp"
#include "Views/Editor/Widgets/Session/LocalPlayerDisplay/LocalPlayerDisplayView.hpp"

class LocalPlayerDisplayController : public IWidgetController
{
public:
    explicit LocalPlayerDisplayController(Campaign &currentCampaign);

    void update(std::shared_ptr<GLFWwindow> window) override;
    void onCampaignLoaded();
    bool publish(const PlayerDisplayReviewSnapshot &review);
    bool publicationCommitted(const PlayerDisplayReviewSnapshot &review, PlayerDisplayReview::Measurements *measurements = nullptr,
        gm::map_render::MapRenderCompileMemo *compileMemo = nullptr);
    bool publicationCommitted(const PlayerDisplayReview::Confirmation &review, PlayerDisplayReview::Measurements *measurements = nullptr,
        gm::map_render::MapRenderCompileMemo *compileMemo = nullptr);
    void setReviewMeasurements(PlayerDisplayReview::Measurements *m) { _reviewMeasurements=m; }
    void publicationCleared();
    void blackout();
    bool combatStatsUpdatePending() const
    {
        return _combatStatsUpdatePending && localPlayerDisplayIsPublished(_currentCampaign.localPlayerDisplay);
    }
    bool updateApprovedCombatStats();
    bool pingAtPublicCamera();
    void requestWindowPlacement();

private:
    friend class LocalPlayerDisplayView;
    friend struct PlayerReviewUiProbe;

    void invalidateReviewCache();
    const PlayerDisplayReviewSnapshot &refreshReviewCache(PlayerDisplayReview::Measurements *measurements = nullptr,
        PlayerDisplayReview::Origin origin = PlayerDisplayReview::Origin::Runtime,
        gm::map_render::MapRenderCompileMemo *compileMemo = nullptr);
    void clearRuntimePublicationProof();

    Campaign &_currentCampaign;
    std::unique_ptr<LocalPlayerDisplayView> _view;
    PlayerDisplayPublicationAssets _publicationAssets;
    PlayerDisplayReview::Measurements *_reviewMeasurements=nullptr;
    PlayerDisplayReviewSnapshot _reviewCache;
    MapModel _reviewedMapCache;
    std::uint64_t _reviewSourceFingerprint = 0;
    bool _reviewCacheValid = false;
    bool _reviewedMapCacheValid = false;
    // Never persisted: startup always begins black. This retained copy is the
    // only player-facing source while a narrowly approved combat refresh is
    // pending, so the detached view cannot re-read live campaign content.
    PlayerDisplayReviewSnapshot _approvedReviewSnapshot;
    MapModel _approvedMapCache;
    std::uint64_t _approvedSourceFingerprint = 0;
    std::uint64_t _approvedNormalizedSourceFingerprint = 0;
    bool _approvedReviewSnapshotValid = false;
    bool _approvedMapCacheValid = false;
    bool _combatStatsUpdatePending = false;
};
