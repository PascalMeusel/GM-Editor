#include "Controllers/Editor/EditorWindowControllerSupport.hpp"
#include "Models/Project/ApplicationPaths.hpp"

void EditorWindowController::requestCurrentPlayerReview(PlayerReviewSource source, const EntityId &sourceId)
{
    if (!view->isVisible || _currentCampaign.presentationModeActive || sourceId.empty() ||
        view->_presentationReviewAction != EditorWindowView::PresentationReviewAction::None ||
        view->_openPresentationArmReviewNextFrame)
        return;
    EditorPlayerReviewEnvelope request;
    request.source = source;
    request.sourceId = sourceId;
    request.ownerId = source == PlayerReviewSource::Inventory ? _children.inventoryManager->selectedOwnerId() : EntityId{};
    request.campaignId = _currentCampaign.metadata.id;
    request.campaign = &_currentCampaign;
    request.root = ApplicationPaths::writableRoot();
    request.locale = PlayerDisplayReview::currentLocaleCode();
    request.editorRoute = _workspace.navigation.current().editorRoute;
    request.epoch = view->_playerReviewContextEpoch;
    request.workspace = view->_activeWorkspace;
    _workspace.playerReviewError.clear();
    _workspace.playerReviewTarget = std::move(request);
    // An overlay preserves the source selection and every authored draft.
    // In particular, do not navigate to Live: that route can flush preparation.
    view->_presentationPanelOpen = true;
}

bool EditorWindowController::consumeCurrentPlayerReview()
{
    if (!_workspace.playerReviewTarget) return false;
    const auto request = std::move(*_workspace.playerReviewTarget);
    _workspace.playerReviewTarget.reset();
    bool sourceCurrent = false;
    switch (request.source)
    {
    case PlayerReviewSource::Npc:
        sourceCurrent = _children.npcOverview->currentSelectedNpcId() == request.sourceId;
        break;
    case PlayerReviewSource::Item:
        sourceCurrent = _children.itemCreator->getModel().metadata.id == request.sourceId;
        break;
    case PlayerReviewSource::Inventory:
        sourceCurrent = _children.inventoryManager->currentSelectedItemId() == request.sourceId &&
            _children.inventoryManager->selectedOwnerId() == request.ownerId;
        break;
    }
    const bool current = sourceCurrent && !request.sourceId.empty() &&
        request.campaign == &_currentCampaign && request.campaignId == _currentCampaign.metadata.id &&
        request.root == ApplicationPaths::writableRoot() && request.locale == PlayerDisplayReview::currentLocaleCode() &&
        request.epoch == view->_playerReviewContextEpoch && request.workspace == view->_activeWorkspace &&
        request.editorRoute == _workspace.navigation.current().editorRoute && view->isVisible &&
        !_currentCampaign.presentationModeActive &&
        view->_presentationReviewAction == EditorWindowView::PresentationReviewAction::None;
    if (!current)
        _workspace.playerReviewError = uiText(
            "The selection changed. Open the player review again from the current selection.",
            "Die Auswahl wurde geändert. Öffne die Spielervorschau erneut aus der aktuellen Auswahl.");
    return current;
}
