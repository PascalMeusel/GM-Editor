#include "Controllers/Editor/Widgets/MapViewer/MapViewerControllerInternals.hpp"
#include "Models/Map/MapWorldExpansion.hpp"

bool MapViewerController::placeExistingMap(const EntityId &childId)
{
    const MapModel *parent = getSelectedMap();
    const MapModel *child = findMapById(childId);
    if (!parent || !child || child->isMainMap || !child->atlasPlacement.parentId.empty() ||
        !MapAtlasService::isDeeperLevel(child->atlasLevel, parent->atlasLevel) || !hasDetailSelection())
        return false;
    const MapNormalizedRect area = selectedDetailRect();
    if (!MapAtlasService::isNormalizedRectValid(area))
        return false;
    const EntityId parentId = parent->metadata.id;
    const std::uint64_t parentRevision = parent->revision;
    const MapModel originalChild = *child;
    const EntityMetadata originalCampaignMetadata = _currentCampaign.metadata;
    MapModel *placed = findMapById(childId);
    placed->parentMapId = parentId;
    placed->atlasPlacement.parentId = parentId;
    placed->atlasPlacement.parentRect = area;
    placed->atlasPlacement.parentRevision = parentRevision;
    placed->atlasPlacement.sourceRevision = placed->revision;
    placed->atlasStatus = MapAtlasStatus::Linked;
    placed->metadata.touch();
    if (!saveCampaign())
    {
        *placed = originalChild;
        _currentCampaign.metadata = originalCampaignMetadata;
        ++_mapRenderContentVersion;
        return false;
    }
    clearDetailSelection();
    _atlasStatusMessage = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
        ? "Bestehende Karte im Atlas platziert. Ihr eigener Karteninhalt wurde nicht verändert."
        : "Existing map placed in the atlas. Its own map content was not changed.";
    return true;
}

bool MapViewerController::expandWorldMap(int eastCells, int southCells)
{
    const MapModel *selected = getSelectedMap();
    if (!selected || !MapWorldExpansion::available(*selected, eastCells, southCells))
        return false;
    cancelDetailGeneration();
    clearDetailSelection();
    const EntityId worldId = selected->metadata.id;
    Campaign before = _currentCampaign;
    if (!MapWorldExpansion::apply(_currentCampaign, worldId, eastCells, southCells))
        return false;
    blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    if (!saveCampaign())
    {
        // Keep MapModel objects at their original vector addresses: the
        // current ImGui frame still holds references to the selected map.
        for (std::size_t index = 0; index < _currentCampaign.maps.size(); ++index)
            _currentCampaign.maps[index] = std::move(before.maps[index]);
        _currentCampaign.worldPlaces = std::move(before.worldPlaces);
        _currentCampaign.nations = std::move(before.nations);
        _currentCampaign.localPlayerDisplay = std::move(before.localPlayerDisplay);
        _currentCampaign.metadata = std::move(before.metadata);
        ++_mapRenderContentVersion;
        return false;
    }
    _fitZoomRequested = true;
    _hasPromotionPreview = false;
    _hasRebasePreview = false;
    _atlasStatusMessage = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
        ? "Weltkarte erweitert. Orte und Detailkarten bleiben an ihrer Position. Neue Felder sind verborgen; veröffentliche die Spieleranzeige bei Bedarf erneut."
        : "World map expanded. Places and detail maps kept their positions. New cells are hidden; republish the player display when ready.";
    return true;
}

void MapViewerController::preparePromotionPreview()
{
    const MapModel *source = getSelectedMap();
    const MapModel *parent = source ? findMapById(MapAtlasService::effectiveParentId(*source)) : nullptr;
    _promotionPreview = source && parent ? MapAtlasService::previewPromotion(*source, *parent) : MapAtlasPromotionPreview{};
    _hasPromotionPreview = source && parent;
}

bool MapViewerController::applyPromotionPreview()
{
    if (!_hasPromotionPreview || !_promotionPreview.valid)
        return false;
    MapModel *source = findMapById(_promotionPreview.sourceMapId);
    MapModel *parent = findMapById(_promotionPreview.parentMapId);
    if (!source || !parent ||
        !MapAtlasService::promotionPreviewIsCurrent(*source, *parent, _promotionPreview))
    {
        _atlasStatusMessage = gm::localization::trUi("The atlas changed after the preview. Review the promotion again.");
        return false;
    }
    const MapModel originalSource = *source;
    const MapModel originalParent = *parent;
    parent->generation.graph.features.insert(parent->generation.graph.features.end(), _promotionPreview.promotedFeatures.begin(), _promotionPreview.promotedFeatures.end());
    ++parent->revision;
    source->atlasPlacement.parentRevision = parent->revision;
    source->atlasPlacement.sourceRevision = source->revision;
    source->atlasStatus = MapAtlasStatus::Linked;
    const bool saved = saveCampaign();
    if (!saved)
    {
        *source = originalSource;
        *parent = originalParent;
        ++_mapRenderContentVersion;
        return false;
    }
    _hasPromotionPreview = false;
    _atlasStatusMessage = gm::localization::trUi("The selected details were promoted to the parent atlas map.");
    return true;
}

void MapViewerController::prepareRebasePreview()
{
    const MapModel *child = getSelectedMap();
    const MapModel *parent = child ? findMapById(MapAtlasService::effectiveParentId(*child)) : nullptr;
    _rebasePreview = child && parent ? MapAtlasService::previewRebase(*child, *parent) : MapAtlasRebasePreview{};
    _hasRebasePreview = child && parent;
    // Changed inherited content must never silently inherit the first enum
    // value (UseParent). -1 is an explicit, fail-closed "not decided" state.
    _rebaseResolutionIndices.assign(_rebasePreview.items.size(), -1);
    if (!_rebasePreview.valid)
        _atlasStatusMessage = gm::localization::trUi("The parent update could not be prepared.");
}

bool MapViewerController::rebasePreviewReadyToApply() const
{
    if (!_hasRebasePreview || !_rebasePreview.valid ||
        _rebaseResolutionIndices.size() != _rebasePreview.items.size())
        return false;
    for (std::size_t index = 0; index < _rebasePreview.items.size(); ++index)
    {
        if (!_rebasePreview.items[index].requiresDecision)
            continue;
        if (_rebaseResolutionIndices[index] < 0 ||
            _rebaseResolutionIndices[index] > static_cast<int>(MapAtlasRebaseResolution::Detach))
            return false;
    }
    return true;
}

bool MapViewerController::applyRebasePreview()
{
    if (!rebasePreviewReadyToApply())
    {
        _atlasStatusMessage = gm::localization::trUi(
            "Choose what should happen to every changed inherited map item. Nothing is applied until you confirm.");
        return false;
    }
    MapModel *child = findMapById(_rebasePreview.childMapId);
    MapModel *parent = findMapById(_rebasePreview.parentMapId);
    if (!child || !parent)
        return false;
    std::vector<MapAtlasRebaseDecision> decisions;
    decisions.reserve(_rebasePreview.items.size());
    for (std::size_t index = 0; index < _rebasePreview.items.size(); ++index)
    {
        const MapAtlasRebaseItem &item = _rebasePreview.items[index];
        if (!item.requiresDecision)
            continue;
        const int selected = _rebaseResolutionIndices[index];
        decisions.push_back({item.id, static_cast<MapAtlasRebaseResolution>(selected)});
    }
    const MapModel originalChild = *child;
    const MapAtlasRebaseResult rebased =
        MapAtlasService::applyRebaseAtomically(*child, *parent, _rebasePreview, decisions);
    if (!rebased.applied)
    {
        _atlasStatusMessage = rebased.errors.empty()
                                  ? gm::localization::trUi("The parent update could not be applied.")
                                  : gm::localization::trSystemMessage(rebased.errors.front());
        return false;
    }
    if (!saveCampaign())
    {
        // The service transaction protects against an invalid rebase. The
        // second snapshot protects the controller boundary as well: a failed
        // database write must not leave the in-memory child ahead of disk.
        *child = originalChild;
        ++_mapRenderContentVersion;
        return false;
    }

    _hasRebasePreview = false;
    _rebaseResolutionIndices.clear();
    _atlasStatusMessage = gm::localization::trUi("The detail map was synchronized with its parent.");
    return true;
}
