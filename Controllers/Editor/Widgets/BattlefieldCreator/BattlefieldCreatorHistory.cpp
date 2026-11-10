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

void BattlefieldCreatorController::refreshUndoState()
{
    _hasUndo = !_undoHistory.empty();
    _undoLabel = _hasUndo ? _undoHistory.back().label : std::string{};
}

void BattlefieldCreatorController::clearUndoHistory()
{
    _undoHistory.clear();
    _hasUndo = false;
    _strokeInProgress = false;
    _propertyEditUndoActive = false;
    _undoLabel.clear();
}

void BattlefieldCreatorController::undoLastAction()
{
    if (!_hasUndo)
        return;

    const UndoSnapshot snapshot = _undoHistory.back();
    _undoHistory.pop_back();
    _draftBattlefield = snapshot.battlefield;
    normalizeEncounter(_draftBattlefield);
    _strokeInProgress = false;
    _propertyEditUndoActive = false;
    refreshUndoState();
}

MapTileKind BattlefieldCreatorController::selectedTerrain() const
{
    const std::vector<MapTileKind> palette = getTerrainPalette();
    if (palette.empty())
        return MapTileKind::Empty;
    if (_selectedTerrainIndex < 0 || _selectedTerrainIndex >= static_cast<int>(palette.size()))
        return palette.front();
    return palette[_selectedTerrainIndex];
}

void BattlefieldCreatorController::cancelTerrainTool()
{
    _terrainTool = BattlefieldTerrainTool::Inspect;
}

MapModel *BattlefieldCreatorController::getSelectedMap()
{
    if (_currentCampaign.maps.empty())
        return nullptr;
    if (_selectedMapIndex < 0 || _selectedMapIndex >= static_cast<int>(_currentCampaign.maps.size()))
        _selectedMapIndex = 0;
    return &_currentCampaign.maps[_selectedMapIndex];
}

const MapModel *BattlefieldCreatorController::getSelectedMap() const
{
    if (_currentCampaign.maps.empty())
        return nullptr;
    if (_selectedMapIndex < 0 || _selectedMapIndex >= static_cast<int>(_currentCampaign.maps.size()))
        return &_currentCampaign.maps.front();
    return &_currentCampaign.maps[_selectedMapIndex];
}

std::vector<MapTileKind> BattlefieldCreatorController::getTerrainPalette() const
{
    return {MapTileKind::Empty, MapTileKind::Dirt, MapTileKind::Stone, MapTileKind::Forest, MapTileKind::Hill, MapTileKind::Mountain, MapTileKind::Water, MapTileKind::Ice, MapTileKind::Snow, MapTileKind::Desert, MapTileKind::Swamp, MapTileKind::Road, MapTileKind::Bridge, MapTileKind::Building, MapTileKind::Room, MapTileKind::Wall, MapTileKind::Door, MapTileKind::Cave, MapTileKind::Stairs, MapTileKind::Hazard, MapTileKind::Lava, MapTileKind::Pit, MapTileKind::Chasm, MapTileKind::Rubble, MapTileKind::Cover, MapTileKind::Landmark};
}

bool BattlefieldCreatorController::saveCampaign()
{
    _currentCampaign.metadata.touch();
    auto &service = ProjectSaveService::getInstance();
    _saveFailed = !service.saveCampaign(_currentCampaign, "battlefield edits");
    if (_saveFailed)
    {
        _saveErrorMessage = uiText("Not saved. Your battlefield remains editable here. Retry Save before loading another battlefield. ",
                                   "Nicht gespeichert. Dein Schlachtfeld bleibt hier bearbeitbar. Vor dem Laden eines anderen Schlachtfelds erneut speichern. ") + service.state().lastError;
        return false;
    }
    _saveErrorMessage.clear();
    return true;
}

void BattlefieldCreatorController::onShow()
{
    _view->show();
    _view->requestDockOnNextUpdate();
}
