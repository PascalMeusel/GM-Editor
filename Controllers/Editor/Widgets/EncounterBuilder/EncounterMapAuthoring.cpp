#include "Controllers/Editor/Widgets/EncounterBuilder/EncounterBuilderController.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include <algorithm>

bool EncounterBuilderController::openSharedMapAuthoring(bool createNew)
{
    if (!_mapAuthoringHandler || !saveEncounter()) return false;
    return _mapAuthoringHandler(_draftEncounter.metadata.id,
        createNew ? EntityId{} : _draftEncounter.tacticalMapId.value_or(EntityId{}));
}

bool EncounterBuilderController::assignSavedMap(const EntityId &encounterId, const EntityId &mapId)
{
    // The canvas return destination, current draft and live campaign must agree.
    // A deleted encounter must never be recreated by a late map completion.
    if (encounterId != _draftEncounter.metadata.id ||
        !gm::ux::resolveNavigationEntity(_currentCampaign, encounterId).available ||
        !gm::ux::resolveNavigationEntity(_currentCampaign, mapId).available) return false;
    const auto found = std::find_if(_currentCampaign.maps.begin(), _currentCampaign.maps.end(),
        [&](const MapModel &map) { return map.metadata.id == mapId; });
    if (found == _currentCampaign.maps.end()) return false;
    _selectedAtlasMapIndex = static_cast<int>(std::distance(_currentCampaign.maps.begin(), found));
    if (!useSelectedAtlasTacticalMap()) return false;
    return saveEncounter();
}
