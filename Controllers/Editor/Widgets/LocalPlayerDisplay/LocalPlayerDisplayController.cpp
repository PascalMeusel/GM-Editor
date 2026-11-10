#include "LocalPlayerDisplayController.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Session/PlayerDisplayReviewConfirmation.hpp"
#include "Models/Session/PlayerSafeDataFilter.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "imgui.h"

LocalPlayerDisplayController::LocalPlayerDisplayController(Campaign &currentCampaign)
    : _currentCampaign(currentCampaign)
{
    prepareLocalPlayerDisplayForApplicationStart(_currentCampaign.localPlayerDisplay);
    _view = std::make_unique<LocalPlayerDisplayView>(*this);
}

void LocalPlayerDisplayController::onCampaignLoaded()
{
    // Restore content and placement preferences, never window visibility or publication.
    prepareLocalPlayerDisplayForApplicationStart(_currentCampaign.localPlayerDisplay);
    invalidateReviewCache();
    clearRuntimePublicationProof();
    if (!_currentCampaign.localPlayerDisplay.enabled)
    {
        m_isOpen = false;
        _view->hide();
    }
}

bool LocalPlayerDisplayController::publish(const PlayerDisplayReviewSnapshot &review)
{
    gm::map_render::MapRenderCompileMemo compileMemo;
    const auto ownedReview=PlayerDisplayReview::ownConfirmation(review,_reviewMeasurements);
    const bool published = PlayerDisplayReview::publish(_currentCampaign, ownedReview, &compileMemo);
    return published && publicationCommitted(ownedReview, nullptr, &compileMemo);
}

bool LocalPlayerDisplayController::publicationCommitted(const PlayerDisplayReviewSnapshot &review,
    PlayerDisplayReview::Measurements *measurements, gm::map_render::MapRenderCompileMemo *compileMemo)
{
    PlayerDisplayReview::Confirmation ownedReview;
    try { ownedReview=PlayerDisplayReview::ownConfirmation(review,measurements?measurements:_reviewMeasurements); }
    catch (...) { blackout(); return false; }
    return publicationCommitted(ownedReview,measurements,compileMemo);
}

bool LocalPlayerDisplayController::publicationCommitted(const PlayerDisplayReview::Confirmation &review,
    PlayerDisplayReview::Measurements *measurements, gm::map_render::MapRenderCompileMemo *compileMemo)
{
    using namespace PlayerDisplayReview;
    if(!measurements)measurements=_reviewMeasurements;
    Measure installation(measurements,Stage::Installation,Origin::Installation);
    clearRuntimePublicationProof();
    if (!localPlayerDisplayIsPublished(_currentCampaign.localPlayerDisplay) || !review.eligible)
    {
        blackout();
        return false;
    }

    { Measure capture(measurements,Stage::AssetCapture,Origin::Installation);
      if (!_publicationAssets.capture(review)) { blackout(); return false; } }
    invalidateReviewCache();
    const PlayerDisplayReviewSnapshot &current = refreshReviewCache(measurements,Origin::Installation,compileMemo);
    if (!matchesConfirmation(current,review))
    { blackout(); return false; }

    // Retain the freshly validated player-only projection. The GM comparison
    // plan is not part of publication. Keep the canonical cache already built
    // above; later combat refreshes still require the normalized source key
    // and can replace only HP/temp HP and active turn/round.
    { Measure copies(measurements,Stage::ApprovedCopy,Origin::Installation);
      _approvedReviewSnapshot = current;
      if (current.hasMapPreview && _reviewedMapCacheValid)
      { _approvedMapCache = _reviewedMapCache; _approvedMapCacheValid = true; } }
    _approvedReviewSnapshotValid = true;
    Measure keys(measurements,Stage::SourceKeys,Origin::Installation);
    _approvedSourceFingerprint = PlayerDisplayReview::sourceFingerprint(
        _currentCampaign, current.locale, false, false);
    _approvedNormalizedSourceFingerprint = PlayerDisplayReview::sourceFingerprint(
        _currentCampaign, current.locale, true, false);
    _reviewCacheValid = true;
    return true;
}

void LocalPlayerDisplayController::publicationCleared()
{
    clearRuntimePublicationProof();
    invalidateReviewCache();
}

void LocalPlayerDisplayController::blackout()
{
    blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    clearRuntimePublicationProof();
    invalidateReviewCache();
}

bool LocalPlayerDisplayController::updateApprovedCombatStats()
{
    LocalPlayerDisplayModel &display = _currentCampaign.localPlayerDisplay;
    if (!localPlayerDisplayIsPublished(display) || !_approvedReviewSnapshotValid || !_combatStatsUpdatePending)
        return false;

    const std::string locale = _approvedReviewSnapshot.locale;
    if (!_publicationAssets.sourcesCurrent(true) || PlayerDisplayReview::currentLocaleCode() != locale ||
        display.publishedPayloadFingerprint != localPlayerDisplayPayloadFingerprint(display) ||
        PlayerDisplayReview::sourceFingerprint(_currentCampaign, locale, true, false) !=
        _approvedNormalizedSourceFingerprint)
    {
        blackout();
        return false;
    }

    const PlayerDisplayReviewSnapshot current = PlayerDisplayReview::build(_currentCampaign, locale,
        PlayerDisplayReview::BuildPurpose::PlayerOnly, _reviewMeasurements, PlayerDisplayReview::Origin::Runtime);
    if (!current.eligible || current.locale != locale)
    {
        blackout();
        return false;
    }

    // Equality of the complete normalized source fingerprint proves every
    // player-visible value other than current/temporary HP and active
    // turn/round still matches the approved content. Keep the assignments
    // narrow as a second boundary: a future summary field cannot silently
    // become live merely because a caller replaced a whole vector.
    if (current.playerParty.size() != _approvedReviewSnapshot.playerParty.size() ||
        current.playerInitiative.size() != _approvedReviewSnapshot.playerInitiative.size() ||
        current.playerEncounterName != _approvedReviewSnapshot.playerEncounterName)
    {
        blackout();
        return false;
    }
    for (std::size_t index = 0; index < current.playerParty.size(); ++index)
    {
        const PlayerSafeCharacterSummary &live = current.playerParty[index];
        PlayerSafeCharacterSummary &approved = _approvedReviewSnapshot.playerParty[index];
        if (live.entityId != approved.entityId || live.name != approved.name ||
            live.characterClass != approved.characterClass || live.level != approved.level ||
            live.maxHitPoints != approved.maxHitPoints || live.speedFeet != approved.speedFeet ||
            live.conditions != approved.conditions || live.spellSlots.size() != approved.spellSlots.size())
        {
            blackout();
            return false;
        }
        for (std::size_t slot = 0; slot < live.spellSlots.size(); ++slot)
            if (live.spellSlots[slot].level != approved.spellSlots[slot].level ||
                live.spellSlots[slot].available != approved.spellSlots[slot].available ||
                live.spellSlots[slot].maximum != approved.spellSlots[slot].maximum)
            { blackout(); return false; }
        approved.currentHitPoints = live.currentHitPoints;
        approved.temporaryHitPoints = live.temporaryHitPoints;
    }
    for (std::size_t index = 0; index < current.playerInitiative.size(); ++index)
    {
        const PlayerSafeInitiativeEntry &live = current.playerInitiative[index];
        PlayerSafeInitiativeEntry &approved = _approvedReviewSnapshot.playerInitiative[index];
        if (live.displayName != approved.displayName || live.initiative != approved.initiative)
        {
            blackout();
            return false;
        }
        approved.active = live.active;
    }
    _approvedReviewSnapshot.playerEncounterRound = current.playerEncounterRound;
    _approvedReviewSnapshot.contentFingerprint = current.contentFingerprint;
    display.publishedReviewFingerprint = current.contentFingerprint;
    _reviewCache = _approvedReviewSnapshot;
    _reviewCacheValid = true;
    if (_approvedMapCacheValid)
    {
        _reviewedMapCache = _approvedMapCache;
        _reviewedMapCacheValid = true;
    }
    _combatStatsUpdatePending = false;
    _approvedSourceFingerprint = PlayerDisplayReview::sourceFingerprint(_currentCampaign, locale, false, false);
    _approvedNormalizedSourceFingerprint = PlayerDisplayReview::sourceFingerprint(_currentCampaign, locale, true, false);
    return true;
}

bool LocalPlayerDisplayController::pingAtPublicCamera()
{
    const PlayerDisplayReviewSnapshot &review = refreshReviewCache();
    if (!review.eligible ||
        review.contentFingerprint !=
            _currentCampaign.localPlayerDisplay.publishedReviewFingerprint)
        return false;
    const LocalPlayerDisplayCameraState &camera = _currentCampaign.localPlayerDisplay.publicCamera;
    return startPlayerSafeDisplayPing(_currentCampaign, camera.normalizedCenterX, camera.normalizedCenterY);
}

void LocalPlayerDisplayController::invalidateReviewCache()
{
    _reviewCache = {};
    _reviewedMapCache = {};
    _reviewSourceFingerprint = 0;
    _reviewCacheValid = false;
    _reviewedMapCacheValid = false;
}

void LocalPlayerDisplayController::clearRuntimePublicationProof()
{
    _publicationAssets.clear();
    _approvedReviewSnapshot = {};
    _approvedMapCache = {};
    _approvedSourceFingerprint = 0;
    _approvedNormalizedSourceFingerprint = 0;
    _approvedReviewSnapshotValid = false;
    _approvedMapCacheValid = false;
    _combatStatsUpdatePending = false;
}

const PlayerDisplayReviewSnapshot &LocalPlayerDisplayController::refreshReviewCache(PlayerDisplayReview::Measurements *measurements,
    PlayerDisplayReview::Origin origin, gm::map_render::MapRenderCompileMemo *compileMemo)
{
    if(!measurements)measurements=_reviewMeasurements;
    PlayerDisplayReview::Measure timing(measurements,PlayerDisplayReview::Stage::CacheRefresh,origin);
    const std::string locale = PlayerDisplayReview::currentLocaleCode();
    const std::uint64_t sourceFingerprint =
        PlayerDisplayReview::sourceFingerprint(_currentCampaign, locale, false, false);
    if (_reviewCacheValid && _reviewSourceFingerprint == sourceFingerprint &&
        _reviewCache.locale == locale)
        return _reviewCache;

    _reviewCache = PlayerDisplayReview::build(_currentCampaign, locale, PlayerDisplayReview::BuildPurpose::PlayerOnly, measurements, origin, compileMemo);
    _reviewSourceFingerprint = sourceFingerprint;
    _reviewCacheValid = true;
    _reviewedMapCacheValid = false;

    if (_reviewCache.hasMapPreview)
    {
        const MapModel *sourceMap = PlayerDisplayReview::publicMap(_currentCampaign);
        if (sourceMap != nullptr &&
            mapRenderPlanMatchesMap(_reviewCache.playerMapPlan, *sourceMap))
        {
            const MapModel playerSource =
                PlayerDisplayReview::playerSafeMapSource(*sourceMap);
            _reviewedMapCache = filterMapForRenderPlan(
                playerSource, _reviewCache.playerMapPlan);
            _reviewedMapCacheValid = mapRenderPlanMatchesMap(
                _reviewCache.playerMapPlan, _reviewedMapCache);
        }
    }
    return _reviewCache;
}

void LocalPlayerDisplayController::requestWindowPlacement()
{
    _view->requestPlacement();
}

void LocalPlayerDisplayController::update(std::shared_ptr<GLFWwindow> window)
{
    (void)window;

    LocalPlayerDisplayModel &display = _currentCampaign.localPlayerDisplay;

    if (!display.enabled)
    {
        clearRuntimePublicationProof();
        invalidateReviewCache();
        m_isOpen = false;
        _view->hide();
        return;
    }

    // Any staged channel/source change after publication fails closed. Camera
    // movement and pings are excluded from the payload fingerprint.
    if (!localPlayerDisplayIsPublished(display))
    {
        // Revoke flags as well as cached proof. Merely skipping a mismatched
        // payload could otherwise revive it when selections return later.
        blackout();
    }

    // GM-screen publication uses the same persisted review proof. Capture its
    // exact reviewed bytes before the detached view can draw its first frame.
    if (localPlayerDisplayIsPublished(display) && !_approvedReviewSnapshotValid)
    {
        gm::map_render::MapRenderCompileMemo compileMemo;
        const auto review = PlayerDisplayReview::build(_currentCampaign, PlayerDisplayReview::currentLocaleCode(),
            PlayerDisplayReview::BuildPurpose::PlayerOnly, _reviewMeasurements, PlayerDisplayReview::Origin::Installation, &compileMemo);
        if (review.contentFingerprint != display.publishedReviewFingerprint ||
            !publicationCommitted(review, nullptr, &compileMemo))
            blackout();
    }
    if (localPlayerDisplayIsPublished(display) && !_publicationAssets.pollSources()) blackout();

    // Black output resolves no content. Keep window placement/close handling
    // below, but do not rebuild a map after arm or failed source validation.
    if (localPlayerDisplayIsPublished(display))
    {
    const PlayerDisplayReviewSnapshot &review = refreshReviewCache();

    if (display.published &&
        (display.publishedPayloadFingerprint != localPlayerDisplayPayloadFingerprint(display) ||
         !review.eligible ||
         review.contentFingerprint != display.publishedReviewFingerprint ||
         (_approvedReviewSnapshotValid && _reviewSourceFingerprint != _approvedSourceFingerprint)))
    {
        const bool onlyApprovedCombatStatsChanged =
            _approvedReviewSnapshotValid && review.eligible &&
            PlayerDisplayReview::currentLocaleCode() == _approvedReviewSnapshot.locale &&
            display.publishedPayloadFingerprint == localPlayerDisplayPayloadFingerprint(display) &&
            PlayerDisplayReview::sourceFingerprint(_currentCampaign, _approvedReviewSnapshot.locale, true, false) ==
                _approvedNormalizedSourceFingerprint;
        if (onlyApprovedCombatStatsChanged)
        {
            // Show the last approved values until the GM explicitly applies
            // the bounded refresh; never stream live campaign data directly.
            _combatStatsUpdatePending = true;
            _reviewCache = _approvedReviewSnapshot;
            _reviewCacheValid = true;
            if (_approvedMapCacheValid)
            {
                _reviewedMapCache = _approvedMapCache;
                _reviewedMapCacheValid = true;
            }
        }
        else
        {
            blackoutLocalPlayerDisplay(display);
            clearRuntimePublicationProof();
            invalidateReviewCache();
        }
    }
    else if (display.published && _combatStatsUpdatePending && _approvedReviewSnapshotValid && review.eligible &&
             PlayerDisplayReview::currentLocaleCode() == _approvedReviewSnapshot.locale &&
             display.publishedPayloadFingerprint == localPlayerDisplayPayloadFingerprint(display) &&
             _reviewSourceFingerprint == _approvedSourceFingerprint &&
             review.contentFingerprint == display.publishedReviewFingerprint)
    {
        // A GM may undo damage or a turn advance before applying it. Once the
        // live source exactly returns to the approved review, no stale-update
        // affordance should remain.
        _combatStatsUpdatePending = false;
    }
    }

    advancePlayerSafeDisplayPing(display, ImGui::GetIO().DeltaTime);

    m_isOpen = true;
    _view->show();
    _view->update();
    if (!_view->isVisible())
    {
        disableLocalPlayerDisplay(display);
        clearRuntimePublicationProof();
        invalidateReviewCache();
        display.metadata.touch();
        _currentCampaign.metadata.touch();
        ProjectSaveService::getInstance().saveCampaign(_currentCampaign, "close local player display");
        m_isOpen = false;
    }
}
