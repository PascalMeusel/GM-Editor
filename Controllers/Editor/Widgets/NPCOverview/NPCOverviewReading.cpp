#include "NPCOverviewController.hpp"
#include <algorithm>

EntityId NPCOverviewController::currentSelectedNpcId() const
{
    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_currentCampaign.npcs.size())) return {};
    const auto &id = _currentCampaign.npcs[static_cast<std::size_t>(_selectedNpcIndex)].metadata.id;
    if (id.empty() || std::count_if(_currentCampaign.npcs.begin(), _currentCampaign.npcs.end(),
        [&](const auto &npc) { return npc.metadata.id == id; }) != 1) return {};
    return id;
}

void NPCOverviewController::resetReadingContext()
{
    if (_view) _view->resetReadingContext();
}
