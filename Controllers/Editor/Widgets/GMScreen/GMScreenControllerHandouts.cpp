#include "Controllers/Editor/Widgets/GMScreen/GMScreenControllerSupport.hpp"

void GMScreenController::setNpcPublicVisibility(const EntityId &npcId, bool visible)
{
    std::vector<EntityId> &visibleNpcIds = _currentCampaign.localPlayerDisplay.visibleNpcIds;
    const auto existing = std::find(visibleNpcIds.begin(), visibleNpcIds.end(), npcId);
    if (visible && existing == visibleNpcIds.end())
        visibleNpcIds.push_back(npcId);
    else if (!visible && existing != visibleNpcIds.end())
        visibleNpcIds.erase(existing);
    else
        return;
    ::blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    touchLocalPlayerDisplay();
}

void GMScreenController::browseHandoutAttachment()
{
    if(const auto path=gm::assets::chooseImage(_sessionOperationError,true)) {
        ProjectAssetImport::AssetImportRequest request;
        request.kind=ProjectAssetImport::AssetKind::Handout;request.sourcePath=*path;
        request.assetId=createEntityId("handout_attachment");request.displayName=_handoutTitleDraft;
        const auto result=ProjectAssetImport::importAsset(_currentCampaign,request);
        if(result.success)_handoutFilePathDraft=result.projectPath;else _sessionOperationError=result.errorMessage;
    }
}

void GMScreenController::addHandout()
{
    if (_handoutTitleDraft.empty() && _handoutBodyDraft.empty() && _handoutFilePathDraft.empty())
        return;

    HandoutModel handout;
    handout.title = _handoutTitleDraft.empty() ? gm::localization::trUi("Untitled Handout") : _handoutTitleDraft;
    handout.body = _handoutBodyDraft;
    handout.filePath = _handoutFilePathDraft;
    Campaign proposed=_currentCampaign;
    proposed.handouts.push_back(handout);
    proposed.metadata.touch();
    if(!ProjectSaveService::getInstance().saveCampaign(proposed,"create hidden handout")) {
        _sessionOperationError=ProjectSaveService::getInstance().state().lastError;return;
    }
    _currentCampaign=std::move(proposed);
    _sessionOperationError.clear();
    _handoutTitleDraft.clear();
    _handoutBodyDraft.clear();
    _handoutFilePathDraft.clear();
}

void GMScreenController::removeHandout(int index)
{
    if (index < 0 || index >= static_cast<int>(_currentCampaign.handouts.size()))
        return;

    const EntityId handoutId = _currentCampaign.handouts[static_cast<std::size_t>(index)].metadata.id;
    _currentCampaign.handouts.erase(_currentCampaign.handouts.begin() + index);
    std::vector<EntityId> &publicHandouts = _currentCampaign.localPlayerDisplay.publicHandoutIds;
    publicHandouts.erase(std::remove(publicHandouts.begin(), publicHandouts.end(), handoutId), publicHandouts.end());
    touchLocalPlayerDisplay();
}

void GMScreenController::setHandoutPublic(const EntityId &handoutId, bool visible)
{
    std::vector<EntityId> &publicHandouts = _currentCampaign.localPlayerDisplay.publicHandoutIds;
    const auto existing = std::find(publicHandouts.begin(), publicHandouts.end(), handoutId);
    if (visible && existing == publicHandouts.end())
        publicHandouts.push_back(handoutId);
    else if (!visible && existing != publicHandouts.end())
        publicHandouts.erase(existing);
    else
        return;
    ::blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    touchLocalPlayerDisplay();
}
