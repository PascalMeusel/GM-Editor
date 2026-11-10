#pragma once

#include "Models/Combat/EncounterGrid.hpp"

inline int mapEdgeMaskForDelta(int dx, int dy)
{
    int mask = 0;
    if (dy < 0)
        mask |= 1;
    if (dx > 0)
        mask |= 2;
    if (dy > 0)
        mask |= 4;
    if (dx < 0)
        mask |= 8;
    return normalizeMapEdgeMask(mask);
}

inline int oppositeMapEdgeMaskForDelta(int dx, int dy)
{
    int mask = 0;
    if (dy < 0)
        mask |= 4;
    if (dx > 0)
        mask |= 8;
    if (dy > 0)
        mask |= 1;
    if (dx < 0)
        mask |= 2;
    return normalizeMapEdgeMask(mask);
}

inline bool encounterCellFullyBlocksMovement(const MapCellModel *cell)
{
    return cell && (cell->movementCost >= 99 || normalizeMapEdgeMask(cell->movementBlockedMask) == 0x0f);
}

inline bool encounterMovementStepBlocked(const EncounterModel &encounter, int fromX, int fromY, int toX, int toY)
{
    if (!isEncounterCellInside(encounter, fromX, fromY) || !isEncounterCellInside(encounter, toX, toY))
        return true;

    const int dx = std::clamp(toX - fromX, -1, 1);
    const int dy = std::clamp(toY - fromY, -1, 1);
    if (dx == 0 && dy == 0)
        return false;

    const MapCellModel *from = encounterCellAt(encounter, fromX, fromY);
    const MapCellModel *to = encounterCellAt(encounter, toX, toY);
    if (encounterCellFullyBlocksMovement(to))
        return true;

    const int outgoing = mapEdgeMaskForDelta(dx, dy);
    const int incoming = oppositeMapEdgeMaskForDelta(dx, dy);
    if (from && (normalizeMapEdgeMask(from->movementBlockedMask) & outgoing) != 0)
        return true;
    if (to && (normalizeMapEdgeMask(to->movementBlockedMask) & incoming) != 0)
        return true;
    if (from && (normalizeMapEdgeMask(from->movementOneWayBlockedMask) & outgoing) != 0)
        return true;

    if (dx != 0 && dy != 0)
    {
        if (encounterCellFullyBlocksMovement(encounterCellAt(encounter, fromX + dx, fromY)) ||
            encounterCellFullyBlocksMovement(encounterCellAt(encounter, fromX, fromY + dy)))
        {
            return true;
        }
    }

    return false;
}

inline int encounterCellEnterCostFeet(const EncounterModel &encounter, int fromX, int fromY, int toX, int toY)
{
    constexpr int unreachable = std::numeric_limits<int>::max() / 4;
    if (encounterMovementStepBlocked(encounter, fromX, fromY, toX, toY))
        return unreachable;

    const MapCellModel *from = encounterCellAt(encounter, fromX, fromY);
    const MapCellModel *to = encounterCellAt(encounter, toX, toY);
    if (encounterCellFullyBlocksMovement(to))
        return unreachable;

    const int cellFeet = std::max(1, encounter.cellSizeFeet);
    int multiplier = to ? std::max(1, to->movementCost) : 1;
    if (to && to->difficultTerrain)
        multiplier = std::max(multiplier, 2);
    if (multiplier >= 99)
        return unreachable;

    const int elevationDelta = (from && to) ? std::abs(to->elevation - from->elevation) : 0;
    return cellFeet * multiplier + elevationDelta * cellFeet;
}

inline int encounterMovementCostFeet(const EncounterModel &encounter, int fromX, int fromY, int toX, int toY)
{
    constexpr int unreachable = std::numeric_limits<int>::max() / 4;
    if (!isEncounterCellInside(encounter, fromX, fromY) || !isEncounterCellInside(encounter, toX, toY))
        return unreachable;
    if (fromX == toX && fromY == toY)
        return 0;

    const int width = std::max(0, encounter.battlefieldWidth);
    const int height = std::max(0, encounter.battlefieldHeight);
    const int cellCount = width * height;
    if (cellCount <= 0)
        return unreachable;

    std::vector<int> distances(static_cast<std::size_t>(cellCount), unreachable);
    using QueueEntry = std::pair<int, int>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> open;

    const int startIndex = encounterCellIndex(encounter, fromX, fromY);
    const int targetIndex = encounterCellIndex(encounter, toX, toY);
    distances[static_cast<std::size_t>(startIndex)] = 0;
    open.push({0, startIndex});

    constexpr int directions[8][2] = {
        {0, -1},
        {1, 0},
        {0, 1},
        {-1, 0},
        {1, -1},
        {1, 1},
        {-1, 1},
        {-1, -1}};

    while (!open.empty())
    {
        const QueueEntry current = open.top();
        open.pop();
        const int currentDistance = current.first;
        const int currentIndex = current.second;
        if (currentDistance != distances[static_cast<std::size_t>(currentIndex)])
            continue;
        if (currentIndex == targetIndex)
            return currentDistance;

        const int x = currentIndex % width;
        const int y = currentIndex / width;
        for (const auto &direction : directions)
        {
            const int nextX = x + direction[0];
            const int nextY = y + direction[1];
            if (!isEncounterCellInside(encounter, nextX, nextY))
                continue;

            const int stepCost = encounterCellEnterCostFeet(encounter, x, y, nextX, nextY);
            if (stepCost >= unreachable)
                continue;

            const int nextIndex = encounterCellIndex(encounter, nextX, nextY);
            const int nextDistance = currentDistance + stepCost;
            if (nextDistance < distances[static_cast<std::size_t>(nextIndex)])
            {
                distances[static_cast<std::size_t>(nextIndex)] = nextDistance;
                open.push({nextDistance, nextIndex});
            }
        }
    }

    return unreachable;
}

inline bool encounterLineOfSightStepBlocked(const EncounterModel &encounter, int fromX, int fromY, int toX, int toY)
{
    if (!isEncounterCellInside(encounter, fromX, fromY) || !isEncounterCellInside(encounter, toX, toY))
        return true;

    const int dx = std::clamp(toX - fromX, -1, 1);
    const int dy = std::clamp(toY - fromY, -1, 1);
    if (dx == 0 && dy == 0)
        return false;

    const MapCellModel *from = encounterCellAt(encounter, fromX, fromY);
    const MapCellModel *to = encounterCellAt(encounter, toX, toY);
    const int outgoing = mapEdgeMaskForDelta(dx, dy);
    const int incoming = oppositeMapEdgeMaskForDelta(dx, dy);
    if (from && (normalizeMapEdgeMask(from->lineOfSightBlockedMask) & outgoing) != 0)
        return true;
    if (to && (normalizeMapEdgeMask(to->lineOfSightBlockedMask) & incoming) != 0)
        return true;
    if (from && (normalizeMapEdgeMask(from->lineOfSightOneWayBlockedMask) & outgoing) != 0)
        return true;
    return false;
}

inline bool encounterLineOfSightBlocked(const EncounterModel &encounter, int fromX, int fromY, int toX, int toY)
{
    if (!isEncounterCellInside(encounter, fromX, fromY) || !isEncounterCellInside(encounter, toX, toY))
        return true;
    if (fromX == toX && fromY == toY)
        return false;

    int x = fromX;
    int y = fromY;
    const int dx = std::abs(toX - fromX);
    const int dy = std::abs(toY - fromY);
    const int sx = fromX < toX ? 1 : -1;
    const int sy = fromY < toY ? 1 : -1;
    int err = dx - dy;

    while (x != toX || y != toY)
    {
        int nextX = x;
        int nextY = y;
        const int twiceErr = 2 * err;
        if (twiceErr > -dy)
        {
            err -= dy;
            nextX += sx;
        }
        if (twiceErr < dx)
        {
            err += dx;
            nextY += sy;
        }

        if (encounterLineOfSightStepBlocked(encounter, x, y, nextX, nextY))
            return true;

        x = nextX;
        y = nextY;
        if (x == toX && y == toY)
            break;

        const MapCellModel *cell = encounterCellAt(encounter, x, y);
        if (cell && cell->blocksLineOfSight)
            return true;
    }

    return false;
}
