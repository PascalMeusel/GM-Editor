#pragma once

#include "Models/Map/MapModels.hpp"

inline bool isEncounterScaleMap(const MapModel &map)
{
    if (map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region ||
        map.mapKind == MapKind::Geographic)
        return false;

    return map.atlasLevel == MapLevel::Tactical ||
           map.mapKind == MapKind::Dungeon || map.mapKind == MapKind::Interior ||
           map.mapKind == MapKind::Encounter || map.mapKind == MapKind::Settlement ||
           map.scope == MapScope::Dungeon;
}

inline const char *encounterMapTypeLabel(const MapModel &map)
{
    if (map.mapKind == MapKind::Encounter || map.atlasLevel == MapLevel::Tactical)
        return "Encounter map";
    if (map.mapKind == MapKind::Settlement)
        return "City map";
    return "Dungeon map";
}
