#include "Controllers/Editor/EditorWindowControllerSupport.hpp"
#include "Models/Session/PlayerDisplayReviewConfirmation.hpp"

PlayerDisplayReviewSnapshot EditorWindowController::buildLocalPlayerDisplayReview(PlayerDisplayReview::Measurements *measurements) const
{
    return PlayerDisplayReview::build(_currentCampaign, PlayerDisplayReview::currentLocaleCode(),
        PlayerDisplayReview::BuildPurpose::Comparison, measurements, PlayerDisplayReview::Origin::Open);
}

bool EditorWindowController::localPlayerDisplayReviewIsCurrent(const PlayerDisplayReviewSnapshot &review) const
{
    return PlayerDisplayReview::isCurrent(_currentCampaign, review);
}

bool EditorWindowController::armLocalPlayerDisplayFromReview(const PlayerDisplayReviewSnapshot &review, PlayerDisplayReview::Measurements *measurements)
{
    PlayerDisplayReview::Measure total(measurements,PlayerDisplayReview::Stage::ConfirmationTotal);
    const bool originatingOwner=&review==&view->_playerReviewSession.review();
    if(originatingOwner&&(_currentCampaign.presentationModeActive||
        !view->_playerReviewSession.knownCurrent(_currentCampaign,PlayerDisplayReview::currentLocaleCode())))
    {
        _shortcuts.statusMessage=uiText("This review is not ready. Wait for preparation or refresh it before confirming.",
            "Diese Vorschau ist nicht bereit. Warte auf die Vorbereitung oder aktualisiere sie vor dem Bestätigen.");
        return false;
    }
    const auto heldMemo=originatingOwner?view->_playerReviewSession.compileMemo():nullptr;
    gm::map_render::MapRenderCompileMemo localMemo;
    auto *compileMemo=heldMemo?heldMemo.get():&localMemo;
    const auto epoch=view->_playerReviewContextEpoch, generation=view->_playerReviewSession.generation();
    const bool popupOwned=view->_playerReviewSession.prepared()&&!view->_playerReviewSession.stale();
    bool contextRevoked=false;
    const auto result = PlayerDisplayReview::armAndSave(_currentCampaign, review, [this,epoch,generation,popupOwned,&contextRevoked](Campaign &staged) {
        const bool saved = ProjectSaveService::getInstance().saveCampaign(staged, "enable reviewed local player display");
        if (saved) markSearchIndexDirty();
        contextRevoked=saved && (epoch!=view->_playerReviewContextEpoch || generation!=view->_playerReviewSession.generation() ||
            (popupOwned&&(!view->_playerReviewSession.prepared()||view->_playerReviewSession.stale())));
        return saved && !contextRevoked;
    }, measurements, compileMemo);
    if(originatingOwner&&epoch==view->_playerReviewContextEpoch&&generation==view->_playerReviewSession.generation()&&
        result.status==PlayerDisplayReview::TransactionStatus::ReviewChanged)view->_playerReviewSession.invalidate();
    _children.localPlayerDisplay->publicationCleared();
    _shortcuts.statusMessage = result.applied() ? std::string{} :
        !contextRevoked && result.status == PlayerDisplayReview::TransactionStatus::PersistenceFailed
            ? uiText("The display could not be saved. It remains black; retry after resolving the save problem.",
                     "Die Anzeige konnte nicht gespeichert werden. Sie bleibt schwarz; behebe den Speicherfehler und versuche es erneut.")
            : uiText("The staged content changed. Review it again before showing it to players.",
                     "Die bereitgestellten Inhalte haben sich geändert. Prüfe sie erneut, bevor du sie den Spielern zeigst.");
    return result.applied();
}

bool EditorWindowController::publishLocalPlayerDisplay(const PlayerDisplayReviewSnapshot &review, PlayerDisplayReview::Measurements *measurements)
{
    PlayerDisplayReview::Measure total(measurements,PlayerDisplayReview::Stage::ConfirmationTotal);
    const bool originatingOwner=&review==&view->_playerReviewSession.review();
    if(originatingOwner&&(_currentCampaign.presentationModeActive||
        !view->_playerReviewSession.knownCurrent(_currentCampaign,PlayerDisplayReview::currentLocaleCode())))
    {
        _shortcuts.statusMessage=uiText("This review is not ready. Wait for preparation or refresh it before confirming.",
            "Diese Vorschau ist nicht bereit. Warte auf die Vorbereitung oder aktualisiere sie vor dem Bestätigen.");
        return false;
    }
    // The popup may be destroyed by a save subscriber. Keep only derived
    // compile storage alive; epoch/generation checks still revoke consent.
    const auto heldMemo=originatingOwner?view->_playerReviewSession.compileMemo():nullptr;
    gm::map_render::MapRenderCompileMemo localMemo;
    auto *compileMemo=heldMemo?heldMemo.get():&localMemo;
    const auto ownedReview=PlayerDisplayReview::ownConfirmation(review,measurements);
    const auto epoch=view->_playerReviewContextEpoch, generation=view->_playerReviewSession.generation();
    const bool popupOwned=view->_playerReviewSession.prepared()&&!view->_playerReviewSession.stale();
    bool contextRevoked=false;
    const auto result = PlayerDisplayReview::publishAndSave(_currentCampaign, ownedReview, [this,epoch,generation,popupOwned,&contextRevoked](Campaign &staged) {
        const bool saved = ProjectSaveService::getInstance().saveCampaign(staged, "publish reviewed local player display");
        if (saved) markSearchIndexDirty();
        contextRevoked=saved && (epoch!=view->_playerReviewContextEpoch || generation!=view->_playerReviewSession.generation() ||
            (popupOwned&&(!view->_playerReviewSession.prepared()||view->_playerReviewSession.stale())));
        return saved && !contextRevoked;
    }, measurements, compileMemo);
    const bool published = result.applied() && _children.localPlayerDisplay->publicationCommitted(ownedReview, measurements, compileMemo);
    if(originatingOwner&&epoch==view->_playerReviewContextEpoch&&generation==view->_playerReviewSession.generation()&&
        (result.status==PlayerDisplayReview::TransactionStatus::ReviewChanged||(result.applied()&&!published)))
        view->_playerReviewSession.invalidate();
    if (!published)
        _children.localPlayerDisplay->publicationCleared();
    _shortcuts.statusMessage = published ? std::string{} :
        !contextRevoked && result.status == PlayerDisplayReview::TransactionStatus::PersistenceFailed
            ? uiText("Publication could not be saved. The display remains black; retry after resolving the save problem.",
                     "Die Veröffentlichung konnte nicht gespeichert werden. Die Anzeige bleibt schwarz; behebe den Speicherfehler und versuche es erneut.")
            : uiText("The review changed or the display is no longer armed. Review and enable the display again.",
                     "Die Vorschau hat sich geändert oder die Anzeige ist nicht mehr aktiviert. Prüfe und aktiviere die Anzeige erneut.");
    return published;
}

void EditorWindowController::disableLocalPlayerDisplay()
{
    ::disableLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    _children.localPlayerDisplay->publicationCleared();
    touchLocalPlayerDisplayAndSave("disable local player display");
}

void EditorWindowController::blackoutLocalPlayerDisplayOutput()
{
    _children.localPlayerDisplay->blackout();
    _currentCampaign.localPlayerDisplay.metadata.touch();
    _currentCampaign.metadata.touch();
}

bool EditorWindowController::localPlayerDisplayCombatStatsUpdatePending() const
{
    return _children.localPlayerDisplay->combatStatsUpdatePending();
}

bool EditorWindowController::updateLocalPlayerDisplayCombatStats()
{
    const bool updated = _children.localPlayerDisplay->updateApprovedCombatStats();
    _shortcuts.statusMessage = updated
        ? uiText("Shared health and turn updated from the already-approved roster.",
                 "Geteilte Gesundheit und Zug wurden aus der bereits geprüften Gruppe aktualisiert.")
        : uiText("The approved player content changed. The display remains black until it is reviewed again.",
                 "Die geprüften Spielerinhalte haben sich geändert. Die Anzeige bleibt schwarz, bis sie erneut geprüft wurde.");
    return updated;
}

bool EditorWindowController::pingLocalPlayerDisplay()
{
    return _children.localPlayerDisplay->pingAtPublicCamera();
}

void EditorWindowController::setLocalPlayerDisplayMapOnly(bool mapOnly)
{
    if (_currentCampaign.localPlayerDisplay.mapOnly == mapOnly)
        return;
    _currentCampaign.localPlayerDisplay.mapOnly = mapOnly;
    if (!mapOnly)
        _currentCampaign.localPlayerDisplay.fullscreenMapOnly = false;
    blackoutLocalPlayerDisplayOutput();
    _children.localPlayerDisplay->requestWindowPlacement();
    touchLocalPlayerDisplayAndSave("change local player display layout");
}

void EditorWindowController::setLocalPlayerDisplayFullscreen(bool fullscreen)
{
    LocalPlayerDisplayModel &display = _currentCampaign.localPlayerDisplay;
    if (display.fullscreenMapOnly == fullscreen && (!fullscreen || display.mapOnly))
        return;
    display.fullscreenMapOnly = fullscreen;
    if (fullscreen)
        display.mapOnly = true;
    blackoutLocalPlayerDisplayOutput();
    _children.localPlayerDisplay->requestWindowPlacement();
    touchLocalPlayerDisplayAndSave("change local player display fullscreen mode");
}

void EditorWindowController::setLocalPlayerDisplayMonitor(int monitorIndex)
{
    LocalPlayerDisplayModel &display = _currentCampaign.localPlayerDisplay;
    monitorIndex = std::max(-1, monitorIndex);
    if (display.preferredMonitorIndex == monitorIndex)
        return;
    display.preferredMonitorIndex = monitorIndex;
    blackoutLocalPlayerDisplayOutput();
    _children.localPlayerDisplay->requestWindowPlacement();
    touchLocalPlayerDisplayAndSave("change local player display monitor");
}

void EditorWindowController::setLocalPlayerDisplayCameraFollow(bool follow)
{
    LocalPlayerDisplayCameraState &camera = _currentCampaign.localPlayerDisplay.publicCamera;
    if (camera.followGmCamera == follow)
        return;
    camera.followGmCamera = follow;
    touchLocalPlayerDisplayAndSave(follow ? "link public camera to GM map" : "detach public camera from GM map");
}

void EditorWindowController::setLocalPlayerDisplayCamera(float normalizedCenterX,
                                                         float normalizedCenterY,
                                                         float zoom)
{
    LocalPlayerDisplayCameraState &camera = _currentCampaign.localPlayerDisplay.publicCamera;
    camera.normalizedCenterX = normalizedCenterX;
    camera.normalizedCenterY = normalizedCenterY;
    camera.zoom = zoom;
    camera.followGmCamera = false;
    normalizeLocalPlayerDisplayCamera(camera);
    touchLocalPlayerDisplayAndSave("change independent public camera");
}

bool EditorWindowController::revealEntirePublicMap()
{
    if (!setAllPublicDisplayMapFog(_currentCampaign, false))
        return false;
    touchLocalPlayerDisplayAndSave("reveal entire public map");
    return true;
}

bool EditorWindowController::hideEntirePublicMap()
{
    if (!setAllPublicDisplayMapFog(_currentCampaign, true))
        return false;
    touchLocalPlayerDisplayAndSave("hide entire public map");
    return true;
}

void EditorWindowController::restoreLocalPlayerDisplayState(const LocalPlayerDisplayModel &display)
{
    const EntityMetadata metadata = _currentCampaign.localPlayerDisplay.metadata;
    const bool windowWasOpen = _currentCampaign.localPlayerDisplay.enabled;
    _currentCampaign.localPlayerDisplay = display;
    _currentCampaign.localPlayerDisplay.metadata = metadata;
    _currentCampaign.localPlayerDisplay.enabled = windowWasOpen;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave("restore presentation panel state");
}

void EditorWindowController::clearLocalPlayerDisplayState()
{
    LocalPlayerDisplayModel &display = _currentCampaign.localPlayerDisplay;
    ::disableLocalPlayerDisplay(display);
    display.publicSceneId.clear();
    display.publicMapId.clear();
    display.publicEncounterId.clear();
    display.visibleNpcIds.clear();
    display.publicHandoutIds.clear();
    display.sharedArtAssetIds.clear();
    display.visibleCharacterIds.clear();
    display.showParty = false;
    display.showPartySpellSlots = false;
    display.showPartyInventory = false;
    _children.localPlayerDisplay->publicationCleared();
    touchLocalPlayerDisplayAndSave("clear presentation panel");
}

void EditorWindowController::setPublicScene(const EntityId &sceneId)
{
    _currentCampaign.localPlayerDisplay.publicSceneId = sceneId;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave("set public scene");
}

void EditorWindowController::setPublicMap(const EntityId &mapId)
{
    LocalPlayerDisplayModel &display = _currentCampaign.localPlayerDisplay;
    const bool publicMapChanged = display.publicMapId != mapId;
    display.publicMapId = mapId;
    if (publicMapChanged)
        resetLocalPlayerDisplayCameraForMapChange(display.publicCamera);
    blackoutLocalPlayerDisplayOutput();
    const auto map = std::find_if(_currentCampaign.maps.begin(), _currentCampaign.maps.end(), [&](const MapModel &candidate) { return candidate.metadata.id == mapId; });
    if (map != _currentCampaign.maps.end())
        map->visibility = MapVisibility::PlayersVisible;
    touchLocalPlayerDisplayAndSave("set public map");
}

void EditorWindowController::setPublicMapReleaseCategory(MapPlayerReleaseCategory category, bool visible)
{
    const EntityId &mapId = _currentCampaign.localPlayerDisplay.publicMapId;
    const auto map = std::find_if(_currentCampaign.maps.begin(), _currentCampaign.maps.end(),
                                  [&](const MapModel &candidate) { return candidate.metadata.id == mapId; });
    if (map == _currentCampaign.maps.end() ||
        !setMapPlayerReleaseCategory(map->playerRelease, category, visible))
        return;

    map->metadata.touch();
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(visible ? "release public map content category"
                                           : "hide public map content category");
}

void EditorWindowController::setPublicEncounter(const EntityId &encounterId)
{
    _currentCampaign.localPlayerDisplay.publicEncounterId = encounterId;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave("set public initiative");
}

void EditorWindowController::setShowPublicMap(bool show)
{
    _currentCampaign.localPlayerDisplay.showMap = show;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(show ? "show public map" : "hide public map");
}

void EditorWindowController::setShowPublicInitiative(bool show)
{
    _currentCampaign.localPlayerDisplay.showInitiative = show;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(show ? "show public initiative" : "hide public initiative");
}

void EditorWindowController::setShowPublicNpcs(bool show)
{
    _currentCampaign.localPlayerDisplay.showVisibleNpcs = show;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(show ? "show public NPCs" : "hide public NPCs");
}

void EditorWindowController::setShowPublicHandouts(bool show)
{
    _currentCampaign.localPlayerDisplay.showPublicHandouts = show;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(show ? "show public handouts" : "hide public handouts");
}

void EditorWindowController::setShowSharedArt(bool show)
{
    _currentCampaign.localPlayerDisplay.showSharedArt = show;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(show ? "show shared art" : "hide shared art");
}

void EditorWindowController::setPublicNpcVisibility(const EntityId &npcId, bool visible)
{
    std::vector<EntityId> &visibleNpcIds = _currentCampaign.localPlayerDisplay.visibleNpcIds;
    const auto existing = std::find(visibleNpcIds.begin(), visibleNpcIds.end(), npcId);
    if (visible && existing == visibleNpcIds.end())
        visibleNpcIds.push_back(npcId);
    else if (!visible && existing != visibleNpcIds.end())
        visibleNpcIds.erase(existing);
    else
        return;

    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(visible ? "reveal public NPC" : "hide public NPC");
}

void EditorWindowController::setShowPublicParty(bool show)
{
    _currentCampaign.localPlayerDisplay.showParty = show;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(show ? "show selected party members" : "hide party summary");
}

void EditorWindowController::setPublicCharacterVisibility(const EntityId &characterId, bool visible)
{
    auto &ids = _currentCampaign.localPlayerDisplay.visibleCharacterIds;
    const auto found = std::find(ids.begin(), ids.end(), characterId);
    if (visible && found == ids.end())
    {
        const auto character = std::find_if(_currentCampaign.characters.begin(), _currentCampaign.characters.end(),
            [&](const CharacterModel &candidate) { return candidate.metadata.id == characterId; });
        if (character == _currentCampaign.characters.end()) return;
        ids.push_back(characterId);
    }
    else if (!visible && found != ids.end()) ids.erase(found);
    else return;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(visible ? "share party member" : "hide party member");
}

void EditorWindowController::setShowPublicPartySpellSlots(bool show)
{
    _currentCampaign.localPlayerDisplay.showPartySpellSlots = show;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(show ? "share selected party spell slot counts" : "hide party spell slot counts");
}

void EditorWindowController::setShowPublicPartyInventory(bool show)
{
    _currentCampaign.localPlayerDisplay.showPartyInventory = show;
    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(show ? "share selected party inventory" : "hide party inventory");
}

void EditorWindowController::setPlayerTextScale(float scale)
{
    _currentCampaign.localPlayerDisplay.playerTextScale = std::isfinite(scale) ? std::clamp(scale, 1.0f, 1.8f) : 1.15f;
    touchLocalPlayerDisplayAndSave("change player text size");
}

void EditorWindowController::setPublicHandoutVisibility(const EntityId &handoutId, bool visible)
{
    std::vector<EntityId> &publicHandouts = _currentCampaign.localPlayerDisplay.publicHandoutIds;
    const auto existing = std::find(publicHandouts.begin(), publicHandouts.end(), handoutId);
    if (visible && existing == publicHandouts.end())
        publicHandouts.push_back(handoutId);
    else if (!visible && existing != publicHandouts.end())
        publicHandouts.erase(existing);
    else
        return;

    blackoutLocalPlayerDisplayOutput();
    touchLocalPlayerDisplayAndSave(visible ? "reveal public handout" : "hide public handout");
}
