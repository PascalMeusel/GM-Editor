#pragma once

#include "Models/Combat/EncounterTypes.hpp"

inline CombatPhase inferCombatPhaseFromLegacyState(const EncounterModel &encounter)
{
    if (encounter.combatPaused && !encounter.afterCombatSummary.empty())
        return CombatPhase::WrapUp;
    if (encounter.combatPaused)
        return CombatPhase::PausedBreak;
    if (encounter.participants.empty())
        return CombatPhase::Setup;
    if (encounter.round > 1 || encounter.activeParticipantIndex > 0)
        return CombatPhase::ActiveTurn;
    for (const CombatLogEntryModel &entry : encounter.combatLog)
    {
        if (entry.eventType == "Turn" || entry.eventType == "Attack" || entry.eventType == "Damage" || entry.eventType == "Spell" || entry.eventType == "Movement")
            return CombatPhase::ActiveTurn;
        if (entry.eventType == "Combat" && entry.summary.find("began") != std::string::npos)
            return CombatPhase::ActiveTurn;
        if (entry.eventType == "Initiative")
            return CombatPhase::Initiative;
    }
    return CombatPhase::Initiative;
}

inline const char *combatantKindLabel(CombatantKind kind)
{
    switch (kind)
    {
    case CombatantKind::Player:
        return "Player";
    case CombatantKind::NPC:
        return "NPC";
    case CombatantKind::Monster:
        return "Monster";
    }
    return "Combatant";
}

inline int encounterCellDistanceFeet(const EncounterModel &encounter, int x1, int y1, int x2, int y2)
{
    const int dx = std::abs(x2 - x1);
    const int dy = std::abs(y2 - y1);
    return std::max(dx, dy) * std::max(1, encounter.cellSizeFeet);
}

inline bool isEncounterCellInside(const EncounterModel &encounter, int x, int y)
{
    return x >= 0 && y >= 0 && x < encounter.battlefieldWidth && y < encounter.battlefieldHeight;
}

inline constexpr int encounterMaximumGridDimension = 512;

inline bool encounterMapGridSupported(const MapModel &map)
{
    return map.width >= 4 && map.height >= 4 &&
        map.width <= encounterMaximumGridDimension && map.height <= encounterMaximumGridDimension &&
        map.cells.size() == static_cast<std::size_t>(map.width) * static_cast<std::size_t>(map.height);
}

// Explicit resizing preserves spatial coordinates. vector::resize alone would
// reinterpret every later row whenever the width changes.
inline void resizeEncounterBattlefield(EncounterModel &encounter, int width, int height)
{
    width = std::clamp(width, 4, encounterMaximumGridDimension);
    height = std::clamp(height, 4, encounterMaximumGridDimension);
    const std::size_t required = static_cast<std::size_t>(width) * height;
    if (width == encounter.battlefieldWidth && height == encounter.battlefieldHeight)
    {
        encounter.battlefieldCells.resize(required);
        return;
    }
    std::vector<MapCellModel> resized(required);
    if (encounter.battlefieldWidth > 0 && encounter.battlefieldHeight > 0)
        for (int y = 0; y < std::min(height, encounter.battlefieldHeight); ++y)
            for (int x = 0; x < std::min(width, encounter.battlefieldWidth); ++x)
            {
                const std::size_t source = static_cast<std::size_t>(y) * encounter.battlefieldWidth + x;
                if (source < encounter.battlefieldCells.size())
                    resized[static_cast<std::size_t>(y) * width + x] = encounter.battlefieldCells[source];
            }
    encounter.battlefieldWidth = width;
    encounter.battlefieldHeight = height;
    encounter.battlefieldCells = std::move(resized);
}

inline void normalizeEncounter(EncounterModel &encounter)
{
    encounter.cellSizeFeet = std::max(1, encounter.cellSizeFeet);
    if (encounter.combatPaused && !combatPhaseIsPaused(encounter.combatPhase))
        encounter.combatPhase = encounter.afterCombatSummary.empty() ? CombatPhase::PausedBreak : CombatPhase::WrapUp;
    else if (combatPhaseIsPaused(encounter.combatPhase))
        encounter.combatPaused = true;
    // Grid resolution is independent of physical scale. In particular an
    // 80x80 map must not become 65x65 merely because its cells represent 5ft.
    resizeEncounterBattlefield(encounter, encounter.battlefieldWidth, encounter.battlefieldHeight);
    if (encounter.participants.empty())
    {
        encounter.activeParticipantIndex = 0;
        return;
    }

    if (encounter.activeParticipantIndex < 0 || encounter.activeParticipantIndex >= static_cast<int>(encounter.participants.size()))
        encounter.activeParticipantIndex = 0;

    for (auto &participant : encounter.participants)
    {
        participant.waveNumber = std::max(1, participant.waveNumber);
        participant.reinforcementRound = std::max(0, participant.reinforcementRound);
        participant.startingX = std::clamp(participant.startingX, 0, encounter.battlefieldWidth - 1);
        participant.startingY = std::clamp(participant.startingY, 0, encounter.battlefieldHeight - 1);
        participant.x = std::clamp(participant.x, 0, encounter.battlefieldWidth - 1);
        participant.y = std::clamp(participant.y, 0, encounter.battlefieldHeight - 1);
        participant.turnStartX = std::clamp(participant.turnStartX, 0, encounter.battlefieldWidth - 1);
        participant.turnStartY = std::clamp(participant.turnStartY, 0, encounter.battlefieldHeight - 1);
        participant.currentHitPoints = std::clamp(participant.currentHitPoints, 0, std::max(0, participant.maxHitPoints));
        participant.temporaryHitPoints = std::max(0, participant.temporaryHitPoints);
        participant.movementUsedFeet = std::max(0, participant.movementUsedFeet);
        if (participant.narrativePosition.empty())
            participant.narrativePosition = "In the scene";
        if (participant.narrativeRangeBand.empty())
            participant.narrativeRangeBand = "Near";
    }
}

inline int encounterCellIndex(const EncounterModel &encounter, int x, int y)
{
    return y * encounter.battlefieldWidth + x;
}

inline MapCellModel *encounterCellAt(EncounterModel &encounter, int x, int y)
{
    if (!isEncounterCellInside(encounter, x, y))
        return nullptr;
    normalizeEncounter(encounter);
    return &encounter.battlefieldCells[static_cast<size_t>(encounterCellIndex(encounter, x, y))];
}

inline const MapCellModel *encounterCellAt(const EncounterModel &encounter, int x, int y)
{
    if (!isEncounterCellInside(encounter, x, y))
        return nullptr;
    const size_t index = static_cast<size_t>(encounterCellIndex(encounter, x, y));
    if (index >= encounter.battlefieldCells.size())
        return nullptr;
    return &encounter.battlefieldCells[index];
}

