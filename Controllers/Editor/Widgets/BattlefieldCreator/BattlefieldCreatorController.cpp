#include "BattlefieldCreatorController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <random>
#include <utility>
#include <vector>

namespace
{
std::string uiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}

int randomBetween(std::mt19937 &rng, int minimum, int maximum)
{
    if (maximum < minimum)
        return minimum;
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(rng);
}

bool rollPercent(std::mt19937 &rng, int percent)
{
    return randomBetween(rng, 1, 100) <= percent;
}

void setBattleCell(EncounterModel &battlefield, int x, int y, MapTileKind kind)
{
    MapCellModel *cell = encounterCellAt(battlefield, x, y);
    if (!cell)
        return;

    applyMapCellTerrainDefaults(*cell, kind);
}

bool battleNearKind(const EncounterModel &battlefield, int x, int y, MapTileKind kind, int radius)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const MapCellModel *cell = encounterCellAt(battlefield, x + dx, y + dy);
            if (cell && cell->kind == kind)
                return true;
        }
    }
    return false;
}

void fillBattleDisc(EncounterModel &battlefield, int centerX, int centerY, int radius, MapTileKind kind, std::mt19937 &rng, int raggedPercent)
{
    const int radiusSquared = radius * radius;
    for (int y = centerY - radius; y <= centerY + radius; ++y)
    {
        for (int x = centerX - radius; x <= centerX + radius; ++x)
        {
            const int dx = x - centerX;
            const int dy = y - centerY;
            if (dx * dx + dy * dy <= radiusSquared && !rollPercent(rng, raggedPercent))
                setBattleCell(battlefield, x, y, kind);
        }
    }
}

void scatterBattleClumps(EncounterModel &battlefield, MapTileKind kind, int count, int minRadius, int maxRadius, std::mt19937 &rng, int raggedPercent)
{
    for (int i = 0; i < count; ++i)
    {
        fillBattleDisc(
            battlefield,
            randomBetween(rng, 1, std::max(1, battlefield.battlefieldWidth - 2)),
            randomBetween(rng, 1, std::max(1, battlefield.battlefieldHeight - 2)),
            randomBetween(rng, minRadius, maxRadius),
            kind,
            rng,
            raggedPercent);
    }
}

void addBattleBorder(EncounterModel &battlefield, MapTileKind anchorKind, MapTileKind borderKind, MapTileKind replaceKind, int radius, int percent, std::mt19937 &rng)
{
    std::vector<std::pair<int, int>> targets;
    for (int y = 0; y < battlefield.battlefieldHeight; ++y)
    {
        for (int x = 0; x < battlefield.battlefieldWidth; ++x)
        {
            const MapCellModel *cell = encounterCellAt(battlefield, x, y);
            if (cell && cell->kind == replaceKind && battleNearKind(battlefield, x, y, anchorKind, radius) && rollPercent(rng, percent))
                targets.push_back({x, y});
        }
    }

    for (const auto &[x, y] : targets)
        setBattleCell(battlefield, x, y, borderKind);
}

int stepToward(int value, int target)
{
    if (value < target)
        return 1;
    if (value > target)
        return -1;
    return 0;
}

void drawBattlePath(EncounterModel &battlefield, int x1, int y1, int x2, int y2, MapTileKind kind, std::mt19937 &rng)
{
    int x = std::clamp(x1, 0, std::max(0, battlefield.battlefieldWidth - 1));
    int y = std::clamp(y1, 0, std::max(0, battlefield.battlefieldHeight - 1));
    const int targetX = std::clamp(x2, 0, std::max(0, battlefield.battlefieldWidth - 1));
    const int targetY = std::clamp(y2, 0, std::max(0, battlefield.battlefieldHeight - 1));
    const int safetyLimit = std::max(16, (battlefield.battlefieldWidth + battlefield.battlefieldHeight) * 8);

    for (int step = 0; step < safetyLimit; ++step)
    {
        const MapCellModel *existing = encounterCellAt(battlefield, x, y);
        setBattleCell(battlefield, x, y, kind == MapTileKind::Road && existing && existing->kind == MapTileKind::Water ? MapTileKind::Bridge : kind);
        if (x == targetX && y == targetY)
            break;

        const int dx = targetX - x;
        const int dy = targetY - y;
        const bool preferX = std::abs(dx) >= std::abs(dy);
        if ((preferX && !rollPercent(rng, 20)) || dy == 0)
            x += stepToward(x, targetX);
        else if ((!preferX && !rollPercent(rng, 20)) || dx == 0)
            y += stepToward(y, targetY);
        else if (rollPercent(rng, 50))
            x = std::clamp(x + randomBetween(rng, -1, 1), 0, std::max(0, battlefield.battlefieldWidth - 1));
        else
            y = std::clamp(y + randomBetween(rng, -1, 1), 0, std::max(0, battlefield.battlefieldHeight - 1));

        x = std::clamp(x, 0, std::max(0, battlefield.battlefieldWidth - 1));
        y = std::clamp(y, 0, std::max(0, battlefield.battlefieldHeight - 1));
    }
}
}

BattlefieldCreatorController::BattlefieldCreatorController(Campaign &currentCampaign) : _currentCampaign(currentCampaign)
{
    _view = std::make_unique<BattlefieldCreatorView>(*this);
    startNewBattlefield();
}

void BattlefieldCreatorController::update(std::shared_ptr<GLFWwindow> window)
{
    if (!m_isOpen)
        return;

    if (_draftCampaignId != _currentCampaign.metadata.id)
    {
        _saveFailed = false;
        _saveErrorMessage.clear();
        startNewBattlefield();
    }
    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}

void BattlefieldCreatorController::startNewBattlefield()
{
    if (_saveFailed)
        return;
    _draftCampaignId = _currentCampaign.metadata.id;
    _draftBattlefield = EncounterModel{};
    _draftBattlefield.name = gm::localization::trUi("New Battlefield");
    _draftBattlefield.battlefieldWidth = 20;
    _draftBattlefield.battlefieldHeight = 20;
    _draftBattlefield.cellSizeFeet = 5;
    _draftBattlefield.useMapCells = false;
    _selectedBattlefieldIndex = -1;
    _terrainTool = BattlefieldTerrainTool::Paint;
    normalizeEncounter(_draftBattlefield);
    fillTerrainInternal(MapTileKind::Empty);
    clearUndoHistory();
}

void BattlefieldCreatorController::loadSelectedBattlefield()
{
    if (_saveFailed)
        return;
    if (_selectedBattlefieldIndex < 0 || _selectedBattlefieldIndex >= static_cast<int>(_currentCampaign.encounters.size()))
        return;

    _draftBattlefield = _currentCampaign.encounters[_selectedBattlefieldIndex];
    normalizeEncounter(_draftBattlefield);
    for (int i = 0; i < static_cast<int>(_currentCampaign.maps.size()); ++i)
    {
        if (_currentCampaign.maps[i].metadata.id == _draftBattlefield.mapId)
        {
            _selectedMapIndex = i;
            break;
        }
    }
    clearUndoHistory();
}

void BattlefieldCreatorController::saveBattlefield()
{
    if (_draftBattlefield.name.empty())
        _draftBattlefield.name = gm::localization::trUi("Untitled Battlefield");

    normalizeEncounter(_draftBattlefield);
    _draftBattlefield.metadata.touch();
    // The chooser is not the draft's identity: selecting another row must not
    // overwrite it when saving an existing or newly created battlefield.
    const auto existing = std::find_if(_currentCampaign.encounters.begin(), _currentCampaign.encounters.end(), [&](const EncounterModel &candidate) {
        return candidate.metadata.id == _draftBattlefield.metadata.id;
    });
    if (existing != _currentCampaign.encounters.end())
    {
        _draftBattlefield.participants = existing->participants;
        _draftBattlefield.activeParticipantIndex = existing->activeParticipantIndex;
        _draftBattlefield.round = existing->round;
        _selectedBattlefieldIndex = static_cast<int>(std::distance(_currentCampaign.encounters.begin(), existing));
        *existing = _draftBattlefield;
    }
    else
    {
        _currentCampaign.encounters.push_back(_draftBattlefield);
        _selectedBattlefieldIndex = static_cast<int>(_currentCampaign.encounters.size()) - 1;
    }
    saveCampaign();
}

void BattlefieldCreatorController::generateFromPartyLocation()
{
    MapModel *map = nullptr;
    if (!_currentCampaign.partyLocation.mapId.empty())
    {
        const auto it = std::find_if(_currentCampaign.maps.begin(), _currentCampaign.maps.end(), [&](const MapModel &candidate) {
            return candidate.metadata.id == _currentCampaign.partyLocation.mapId;
        });
        if (it != _currentCampaign.maps.end())
            map = &(*it);
    }
    if (!map)
        map = getSelectedMap();
    if (!map)
        return;

    captureUndoSnapshot("Generate From Party Position");
    _draftBattlefield.mapId = map->metadata.id;
    _draftBattlefield.mapContextName = map->name;
    _draftBattlefield.mapContextX = std::clamp(_currentCampaign.partyLocation.x, 0, std::max(0, map->width - 1));
    _draftBattlefield.mapContextY = std::clamp(_currentCampaign.partyLocation.y, 0, std::max(0, map->height - 1));
    _draftBattlefield.useMapCells = false;
    _draftBattlefield.battlefieldWidth = 20;
    _draftBattlefield.battlefieldHeight = 20;
    _draftBattlefield.cellSizeFeet = 5;

    const MapCellModel *contextCell = mapCellAt(*map, _draftBattlefield.mapContextX, _draftBattlefield.mapContextY);
    const MapTileKind sourceTerrain = contextCell ? contextCell->kind : MapTileKind::Empty;
    _draftBattlefield.name = map->name + " - " + mapTileKindLabel(sourceTerrain) + " Battlefield";
    generateTerrainInternal(sourceTerrain);
}

void BattlefieldCreatorController::useSelectedMapAsBattlefield()
{
    MapModel *map = getSelectedMap();
    if (!map || !selectedMapCanBeBattlefield())
        return;

    captureUndoSnapshot("Use Map As Battlefield");
    _draftBattlefield.mapId = map->metadata.id;
    _draftBattlefield.tacticalMapId = map->atlasLevel == MapLevel::Tactical
        ? std::optional<EntityId>(map->metadata.id) : std::nullopt;
    _draftBattlefield.sourceMapId = MapAtlasService::effectiveParentId(*map);
    _draftBattlefield.sourceMapRect = map->atlasPlacement.parentRect;
    _draftBattlefield.sourceMapRevision = map->atlasPlacement.parentRevision;
    _draftBattlefield.mapContextName = map->name;
    _draftBattlefield.mapContextX = _currentCampaign.partyLocation.mapId == map->metadata.id ? std::clamp(_currentCampaign.partyLocation.x, 0, map->width - 1) : 0;
    _draftBattlefield.mapContextY = _currentCampaign.partyLocation.mapId == map->metadata.id ? std::clamp(_currentCampaign.partyLocation.y, 0, map->height - 1) : 0;
    _draftBattlefield.useMapCells = true;
    _draftBattlefield.battlefieldWidth = map->width;
    _draftBattlefield.battlefieldHeight = map->height;
    _draftBattlefield.cellSizeFeet = std::max(1, static_cast<int>(map->cellSize));
    _draftBattlefield.battlefieldCells = map->cells;
    normalizeEncounter(_draftBattlefield);
}

bool BattlefieldCreatorController::selectedMapCanBeBattlefield() const
{
    const MapModel *map = getSelectedMap();
    if (!map)
        return false;
    return encounterMapGridSupported(*map);
}
