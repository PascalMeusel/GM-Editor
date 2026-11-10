#pragma once

#include "Models/Combat/EncounterModel.hpp"
#include "Models/Combat/EncounterMapEligibility.hpp"
#include "Models/Map/MapRenderPlan.hpp"

inline MapRenderPlanRequest encounterGameMasterArtRequest(const std::string &locale)
{
    MapRenderPlanRequest request;
    request.audience = MapRenderAudience::GameMaster;
    request.locale = locale;
    request.layers = MapRenderPlanTerrain | MapRenderPlanWater | MapRenderPlanWays |
                     MapRenderPlanSymbols | MapRenderPlanLabels;
    // Encounter tokens, grid and fog shading are drawn over this GM-only base.
    // This request must never be reused for player publication.
    request.applyFog = false;
    return request;
}

// Combat rules operate on the saved encounter grid. An atlas illustration is
// safe to reuse only when it depicts that same grid, not a dropdown selection
// or a subsequently edited map with different movement/LOS geometry.
inline bool encounterMapRenderCellsMatch(const MapCellModel &a, const MapCellModel &b)
{
    return a.kind == b.kind && a.rotationQuarterTurns == b.rotationQuarterTurns &&
        a.rotationLocked == b.rotationLocked && a.symbolVariantMode == b.symbolVariantMode &&
        a.wayKind == b.wayKind && a.movementCost == b.movementCost &&
        a.difficultTerrain == b.difficultTerrain && a.blocksLineOfSight == b.blocksLineOfSight &&
        a.lighting == b.lighting && a.fogged == b.fogged && a.elevation == b.elevation &&
        a.movementBlockedMask == b.movementBlockedMask &&
        a.lineOfSightBlockedMask == b.lineOfSightBlockedMask &&
        a.movementOneWayBlockedMask == b.movementOneWayBlockedMask &&
        a.lineOfSightOneWayBlockedMask == b.lineOfSightOneWayBlockedMask;
}

inline const MapModel *encounterMapRenderSource(const EncounterModel &encounter,
                                               const std::vector<MapModel> &maps)
{
    if (!encounter.useMapCells || encounter.theaterOfMind ||
        encounter.battlefieldWidth <= 0 || encounter.battlefieldHeight <= 0)
        return nullptr;
    const EntityId &id = encounter.tacticalMapId && !encounter.tacticalMapId->empty()
        ? *encounter.tacticalMapId : encounter.mapId;
    if (id.empty())
        return nullptr;
    for (const auto &map : maps)
    {
        if (map.metadata.id != id)
            continue;
        if (!isEncounterScaleMap(map) || map.width != encounter.battlefieldWidth ||
            map.height != encounter.battlefieldHeight ||
            map.cells.size() != static_cast<std::size_t>(map.width) * map.height ||
            map.cells.size() != encounter.battlefieldCells.size())
            return nullptr;
        for (std::size_t i = 0; i < map.cells.size(); ++i)
            if (!encounterMapRenderCellsMatch(map.cells[i], encounter.battlefieldCells[i]))
                return nullptr;
        return &map;
    }
    return nullptr;
}
