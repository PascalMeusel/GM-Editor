#include "Controllers/Editor/Widgets/MapViewer/MapViewerControllerInternals.hpp"

MapViewerController::MapViewerController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService)
    : _currentCampaign(currentCampaign), _playbackService(std::move(playbackService))
{
    _selectedCampaignId = _currentCampaign.metadata.id;
    _view = std::make_unique<MapViewerView>(*this);
}

MapViewerController::~MapViewerController()
{
    cancelFogStroke();
    if (_detailGenerationProgress)
        _detailGenerationProgress->discardResult.store(true, std::memory_order_relaxed);
    if (_detailGenerationFuture.valid())
        _detailGenerationFuture.wait();
}

void MapViewerController::openTravelWorkflow()
{
    _interactionMode = MapViewerInteractionMode::InspectSelect;
    _view->requestTravelWorkflow();
}

void MapViewerController::update(std::shared_ptr<GLFWwindow> window)
{
    pollDetailGeneration();
    if (!m_isOpen)
    {
        cancelFogStroke();
        return;
    }

    if (_focusViewRequested)
    {
        // Opening an already docked atlas must also select its tab. The
        // workspace navigation action may just have focused the overview.
        ImGui::SetNextWindowFocus();
        _focusViewRequested = false;
    }
    _view->update();
    if (!_view->isVisible())
    {
        cancelFogStroke();
        m_isOpen = false;
    }
}

bool MapViewerController::selectMapById(const EntityId &mapId)
{
    for (int i = 0; i < static_cast<int>(_currentCampaign.maps.size()); ++i)
    {
        if (_currentCampaign.maps[static_cast<std::size_t>(i)].metadata.id == mapId)
        {
            cancelFogStroke();
            cancelDetailGeneration();
            _selectedMapIndex = i;
            _selectedCampaignId = _currentCampaign.metadata.id;
            _mapSelectionExplicit = true;
            clearSelectedCell();
            clearPendingPartyMove();
            clearDetailSelection();
            _detailCandidates.clear();
            _selectedDetailCandidate = -1;
            _detailAdvancedLevelOverride = false;
            _hasPromotionPreview = false;
            _hasRebasePreview = false;
            _rebaseResolutionIndices.clear();
            _fitZoomRequested = true;
            _atlasStatusMessage.clear();
            _mapInteractionStatusMessage.clear();
            return true;
        }
    }
    return false;
}

MapModel *MapViewerController::getSelectedMap()
{
    if (_currentCampaign.maps.empty())
        return nullptr;
    if (_selectedCampaignId != _currentCampaign.metadata.id)
    {
        _selectedCampaignId = _currentCampaign.metadata.id;
        _selectedMapIndex = 0;
        _mapSelectionExplicit = false;
    }
    if (!_mapSelectionExplicit)
    {
        const MapModel *world = MapAtlasService::primaryWorldMap(_currentCampaign.maps);
        if (world)
            _selectedMapIndex = static_cast<int>(world - _currentCampaign.maps.data());
    }
    if (_selectedMapIndex < 0 || _selectedMapIndex >= static_cast<int>(_currentCampaign.maps.size()))
        _selectedMapIndex = 0;
    return &_currentCampaign.maps[_selectedMapIndex];
}

const MapModel *MapViewerController::getSelectedMap() const
{
    if (_currentCampaign.maps.empty())
        return nullptr;
    if (!_mapSelectionExplicit || _selectedCampaignId != _currentCampaign.metadata.id)
    {
        if (const MapModel *world = MapAtlasService::primaryWorldMap(_currentCampaign.maps))
            return world;
    }
    if (_selectedMapIndex < 0 || _selectedMapIndex >= static_cast<int>(_currentCampaign.maps.size()))
        return &_currentCampaign.maps.front();
    return &_currentCampaign.maps[_selectedMapIndex];
}

MapModel *MapViewerController::findMapById(const EntityId &mapId)
{
    if (mapId.empty())
        return nullptr;
    auto mapIt = std::find_if(_currentCampaign.maps.begin(), _currentCampaign.maps.end(), [&](const MapModel &map) {
        return map.metadata.id == mapId;
    });
    return mapIt == _currentCampaign.maps.end() ? nullptr : &*mapIt;
}

const MapModel *MapViewerController::findMapById(const EntityId &mapId) const
{
    if (mapId.empty())
        return nullptr;
    const auto mapIt = std::find_if(_currentCampaign.maps.begin(), _currentCampaign.maps.end(), [&](const MapModel &map) {
        return map.metadata.id == mapId;
    });
    return mapIt == _currentCampaign.maps.end() ? nullptr : &*mapIt;
}

std::vector<const MapModel *> MapViewerController::selectedMapBreadcrumbs() const
{
    std::vector<const MapModel *> reversed;
    const MapModel *current = getSelectedMap();
    std::vector<EntityId> visited;
    while (current)
    {
        if (std::find(visited.begin(), visited.end(), current->metadata.id) != visited.end())
            break;
        visited.push_back(current->metadata.id);
        reversed.push_back(current);
        current = findMapById(MapAtlasService::effectiveParentId(*current));
    }
    std::reverse(reversed.begin(), reversed.end());
    return reversed;
}

std::vector<int> MapViewerController::childMapIndices(const EntityId &parentMapId) const
{
    std::vector<int> result;
    for (int index = 0; index < static_cast<int>(_currentCampaign.maps.size()); ++index)
    {
        if (MapAtlasService::effectiveParentId(_currentCampaign.maps[static_cast<std::size_t>(index)]) == parentMapId)
            result.push_back(index);
    }
    return result;
}

bool MapViewerController::selectParentMap()
{
    const MapModel *selected = getSelectedMap();
    const EntityId parentId = selected ? MapAtlasService::effectiveParentId(*selected) : EntityId{};
    return !parentId.empty() && selectMapById(parentId);
}

bool MapViewerController::hasDetailSelection() const
{
    const MapModel *map = getSelectedMap();
    return map && isMapCellInside(*map, _detailStartX, _detailStartY) && isMapCellInside(*map, _detailEndX, _detailEndY);
}

MapNormalizedRect MapViewerController::selectedDetailRect() const
{
    const MapModel *map = getSelectedMap();
    if (!map || !hasDetailSelection())
        return {};
    const int minimumX = std::min(_detailStartX, _detailEndX);
    const int minimumY = std::min(_detailStartY, _detailEndY);
    const int maximumX = std::max(_detailStartX, _detailEndX);
    const int maximumY = std::max(_detailStartY, _detailEndY);
    return {
        static_cast<float>(minimumX) / static_cast<float>(std::max(1, map->width)),
        static_cast<float>(minimumY) / static_cast<float>(std::max(1, map->height)),
        static_cast<float>(maximumX - minimumX + 1) / static_cast<float>(std::max(1, map->width)),
        static_cast<float>(maximumY - minimumY + 1) / static_cast<float>(std::max(1, map->height))};
}

void MapViewerController::beginDetailSelection(int x, int y)
{
    const MapModel *map = getSelectedMap();
    if (!map || !isMapCellInside(*map, x, y))
        return;
    cancelDetailGeneration();
    _detailStartX = _detailEndX = x;
    _detailStartY = _detailEndY = y;
    _detailSelectionDragging = true;
    _detailCandidates.clear();
    _selectedDetailCandidate = -1;
    _atlasStatusMessage.clear();
}

void MapViewerController::updateDetailSelection(int x, int y)
{
    const MapModel *map = getSelectedMap();
    if (!_detailSelectionDragging || !map)
        return;
    _detailEndX = std::clamp(x, 0, std::max(0, map->width - 1));
    _detailEndY = std::clamp(y, 0, std::max(0, map->height - 1));
}

void MapViewerController::finishDetailSelection(int x, int y)
{
    updateDetailSelection(x, y);
    _detailSelectionDragging = false;
    const MapModel *map = getSelectedMap();
    if (!map)
        return;
    _detailTargetLevel = nextAtlasLevel(map->atlasLevel);
    _detailAdvancedLevelOverride = false;
    if (_detailMapName.empty())
    {
        const char *suffix = _detailTargetLevel == MapLevel::Region
                                 ? gm::localization::trUi("Region")
                                 : (_detailTargetLevel == MapLevel::Site ? gm::localization::trUi("Site")
                                                                        : gm::localization::trUi("Encounter"));
        _detailMapName = (map->name.empty() ? std::string(gm::localization::trUi("Atlas")) : map->name) + " - " + suffix;
    }
}

void MapViewerController::clearDetailSelection()
{
    cancelDetailGeneration();
    _detailStartX = _detailStartY = _detailEndX = _detailEndY = -1;
    _detailSelectionDragging = false;
    _detailCandidates.clear();
    _selectedDetailCandidate = -1;
}

void MapViewerController::setDetailTargetLevel(MapLevel level, bool advancedOverride)
{
    const MapModel *parent = getSelectedMap();
    if (!parent)
        return;
    const bool valid = advancedOverride
                           ? MapAtlasService::isDeeperLevel(level, parent->atlasLevel)
                           : MapAtlasService::isNextLevel(level, parent->atlasLevel);
    if (!valid || level == MapLevel::World)
        return;
    if (_detailTargetLevel == level && _detailAdvancedLevelOverride == advancedOverride)
        return;
    const auto suffixForLevel = [](MapLevel target) -> const char * {
        return target == MapLevel::Region
                   ? gm::localization::trUi("Region")
                   : (target == MapLevel::Site ? gm::localization::trUi("Site")
                                               : gm::localization::trUi("Encounter"));
    };
    const std::string baseName = parent->name.empty() ? std::string(gm::localization::trUi("Atlas")) : parent->name;
    const bool generatedName = _detailMapName.empty() ||
                               _detailMapName == baseName + " - " + suffixForLevel(_detailTargetLevel);
    cancelDetailGeneration();
    _detailTargetLevel = level;
    _detailAdvancedLevelOverride = advancedOverride && !MapAtlasService::isNextLevel(level, parent->atlasLevel);
    if (generatedName)
        _detailMapName = baseName + " - " + suffixForLevel(_detailTargetLevel);
    _detailCandidates.clear();
    _selectedDetailCandidate = -1;
}
