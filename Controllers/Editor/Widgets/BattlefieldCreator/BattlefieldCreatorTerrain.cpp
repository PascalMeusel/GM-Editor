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

void BattlefieldCreatorController::generateTerrain(MapTileKind sourceTerrain)
{
    captureUndoSnapshot("Generate Terrain");
    generateTerrainInternal(sourceTerrain);
}

void BattlefieldCreatorController::generateTerrainInternal(MapTileKind sourceTerrain)
{
    normalizeEncounter(_draftBattlefield);
    const auto now = static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count());
    std::random_device device;
    std::seed_seq seed{
        static_cast<unsigned int>(_draftBattlefield.battlefieldWidth),
        static_cast<unsigned int>(_draftBattlefield.battlefieldHeight),
        static_cast<unsigned int>(sourceTerrain),
        now,
        device()};
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> chance(1, 100);

    for (int y = 0; y < _draftBattlefield.battlefieldHeight; ++y)
    {
        for (int x = 0; x < _draftBattlefield.battlefieldWidth; ++x)
        {
            MapCellModel *cell = encounterCellAt(_draftBattlefield, x, y);
            if (!cell)
                continue;

            cell->kind = MapTileKind::Empty;
            if (sourceTerrain == MapTileKind::Forest)
                cell->kind = chance(rng) <= 38 ? MapTileKind::Forest : MapTileKind::Empty;
            else if (sourceTerrain == MapTileKind::Water)
                cell->kind = (x < 3 || chance(rng) <= 8) ? MapTileKind::Water : MapTileKind::Empty;
            else if (sourceTerrain == MapTileKind::Mountain)
                cell->kind = chance(rng) <= 24 ? MapTileKind::Mountain : (chance(rng) <= 20 ? MapTileKind::Hill : MapTileKind::Empty);
            else if (sourceTerrain == MapTileKind::Hill)
                cell->kind = chance(rng) <= 32 ? MapTileKind::Hill : MapTileKind::Empty;
            else if (sourceTerrain == MapTileKind::Dirt || sourceTerrain == MapTileKind::Stone || sourceTerrain == MapTileKind::Snow || sourceTerrain == MapTileKind::Desert || sourceTerrain == MapTileKind::Swamp || sourceTerrain == MapTileKind::Ice)
                cell->kind = chance(rng) <= 76 ? sourceTerrain : (chance(rng) <= 35 ? MapTileKind::Rubble : MapTileKind::Empty);
            else if (sourceTerrain == MapTileKind::Lava || sourceTerrain == MapTileKind::Pit || sourceTerrain == MapTileKind::Chasm || sourceTerrain == MapTileKind::Hazard)
                cell->kind = chance(rng) <= 28 ? sourceTerrain : (chance(rng) <= 15 ? MapTileKind::Stone : MapTileKind::Room);
            else if (sourceTerrain == MapTileKind::Road)
                cell->kind = y == _draftBattlefield.battlefieldHeight / 2 ? MapTileKind::Road : MapTileKind::Empty;
            else if (sourceTerrain == MapTileKind::Settlement || sourceTerrain == MapTileKind::Building)
                cell->kind = chance(rng) <= 16 ? MapTileKind::Building : (x == _draftBattlefield.battlefieldWidth / 2 || y == _draftBattlefield.battlefieldHeight / 2 ? MapTileKind::Road : MapTileKind::Empty);
            else if (sourceTerrain == MapTileKind::Room || sourceTerrain == MapTileKind::Wall || sourceTerrain == MapTileKind::Door || sourceTerrain == MapTileKind::Cave || sourceTerrain == MapTileKind::Stairs)
                cell->kind = sourceTerrain == MapTileKind::Wall ? MapTileKind::Room : sourceTerrain;
            else if (chance(rng) <= 8)
                cell->kind = MapTileKind::Landmark;
            applyMapCellTerrainDefaults(*cell, cell->kind);
        }
    }

    const int area = std::max(1, _draftBattlefield.battlefieldWidth * _draftBattlefield.battlefieldHeight);
    if (sourceTerrain == MapTileKind::Forest)
    {
        scatterBattleClumps(_draftBattlefield, MapTileKind::Cover, std::max(3, area / 120), 1, 2, rng, 45);
        drawBattlePath(_draftBattlefield, 0, randomBetween(rng, 2, _draftBattlefield.battlefieldHeight - 3), _draftBattlefield.battlefieldWidth - 1, randomBetween(rng, 2, _draftBattlefield.battlefieldHeight - 3), MapTileKind::Road, rng);
    }
    else if (sourceTerrain == MapTileKind::Water)
    {
        addBattleBorder(_draftBattlefield, MapTileKind::Water, MapTileKind::Dirt, MapTileKind::Empty, 1, 82, rng);
        drawBattlePath(_draftBattlefield, 0, _draftBattlefield.battlefieldHeight / 2, _draftBattlefield.battlefieldWidth - 1, _draftBattlefield.battlefieldHeight / 2 + randomBetween(rng, -2, 2), MapTileKind::Road, rng);
    }
    else if (sourceTerrain == MapTileKind::Mountain || sourceTerrain == MapTileKind::Hill)
    {
        scatterBattleClumps(_draftBattlefield, MapTileKind::Rubble, std::max(3, area / 100), 1, 2, rng, 38);
        if (rollPercent(rng, 45))
            fillBattleDisc(_draftBattlefield, randomBetween(rng, 3, _draftBattlefield.battlefieldWidth - 4), randomBetween(rng, 3, _draftBattlefield.battlefieldHeight - 4), randomBetween(rng, 1, 3), MapTileKind::Chasm, rng, 32);
        drawBattlePath(_draftBattlefield, 0, _draftBattlefield.battlefieldHeight - 2, _draftBattlefield.battlefieldWidth - 1, randomBetween(rng, 1, _draftBattlefield.battlefieldHeight - 2), MapTileKind::Road, rng);
    }
    else if (sourceTerrain == MapTileKind::Settlement || sourceTerrain == MapTileKind::Building || sourceTerrain == MapTileKind::Road)
    {
        drawBattlePath(_draftBattlefield, 0, _draftBattlefield.battlefieldHeight / 2, _draftBattlefield.battlefieldWidth - 1, _draftBattlefield.battlefieldHeight / 2 + randomBetween(rng, -1, 1), MapTileKind::Road, rng);
        drawBattlePath(_draftBattlefield, _draftBattlefield.battlefieldWidth / 2, 0, _draftBattlefield.battlefieldWidth / 2 + randomBetween(rng, -1, 1), _draftBattlefield.battlefieldHeight - 1, MapTileKind::Road, rng);
        scatterBattleClumps(_draftBattlefield, MapTileKind::Building, std::max(2, area / 150), 1, 2, rng, 25);
        scatterBattleClumps(_draftBattlefield, MapTileKind::Cover, std::max(3, area / 120), 1, 1, rng, 35);
    }
    else if (sourceTerrain == MapTileKind::Room || sourceTerrain == MapTileKind::Wall || sourceTerrain == MapTileKind::Door || sourceTerrain == MapTileKind::Cave || sourceTerrain == MapTileKind::Stairs)
    {
        for (int x = 0; x < _draftBattlefield.battlefieldWidth; ++x)
        {
            setBattleCell(_draftBattlefield, x, 0, MapTileKind::Wall);
            setBattleCell(_draftBattlefield, x, _draftBattlefield.battlefieldHeight - 1, MapTileKind::Wall);
        }
        for (int y = 0; y < _draftBattlefield.battlefieldHeight; ++y)
        {
            setBattleCell(_draftBattlefield, 0, y, MapTileKind::Wall);
            setBattleCell(_draftBattlefield, _draftBattlefield.battlefieldWidth - 1, y, MapTileKind::Wall);
        }
        scatterBattleClumps(_draftBattlefield, MapTileKind::Cover, std::max(2, area / 180), 1, 1, rng, 45);
        if (_draftBattlefield.battlefieldWidth > 8 && _draftBattlefield.battlefieldHeight > 8)
            drawBattlePath(_draftBattlefield, 1, _draftBattlefield.battlefieldHeight / 2, _draftBattlefield.battlefieldWidth - 2, _draftBattlefield.battlefieldHeight / 2 + randomBetween(rng, -2, 2), MapTileKind::Room, rng);
    }
}

void BattlefieldCreatorController::fillTerrain(MapTileKind terrain)
{
    captureUndoSnapshot("Fill Terrain");
    fillTerrainInternal(terrain);
}

void BattlefieldCreatorController::fillTerrainInternal(MapTileKind terrain)
{
    normalizeEncounter(_draftBattlefield);
    for (auto &cell : _draftBattlefield.battlefieldCells)
    {
        applyMapCellTerrainDefaults(cell, terrain);
    }
}

void BattlefieldCreatorController::paintCell(int x, int y)
{
    if (_terrainTool == BattlefieldTerrainTool::Inspect)
        return;

    const std::vector<MapTileKind> palette = getTerrainPalette();
    if (_selectedTerrainIndex < 0 || _selectedTerrainIndex >= static_cast<int>(palette.size()))
        _selectedTerrainIndex = 0;

    const MapTileKind terrain = _terrainTool == BattlefieldTerrainTool::Erase ? MapTileKind::Empty : palette[_selectedTerrainIndex];
    _brushSize = std::clamp(_brushSize, 1, 8);
    const std::vector<MapCellCoordinate> footprint = battlefieldBrushFootprint(_draftBattlefield, x, y, _brushSize);
    if (!_strokeInProgress)
    {
        captureUndoSnapshot(_terrainTool == BattlefieldTerrainTool::Erase ? "Erase Terrain" : "Paint Terrain");
        _strokeInProgress = true;
    }
    applyBattlefieldTerrainBrush(_draftBattlefield, footprint, terrain);
}

void BattlefieldCreatorController::endTerrainStroke()
{
    _strokeInProgress = false;
}

void BattlefieldCreatorController::captureUndoSnapshot(const std::string &label)
{
    _undoHistory.push_back(UndoSnapshot{_draftBattlefield, label});
    while (_undoHistory.size() > maxUndoHistory)
        _undoHistory.erase(_undoHistory.begin());
    refreshUndoState();
}

void BattlefieldCreatorController::beginPropertyEditUndo(const std::string &label)
{
    if (_propertyEditUndoActive)
        return;

    captureUndoSnapshot(label);
    _propertyEditUndoActive = true;
}

void BattlefieldCreatorController::finishPropertyEditUndo()
{
    _propertyEditUndoActive = false;
}
