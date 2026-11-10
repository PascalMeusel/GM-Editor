#pragma once

#include "Models/Map/MapCreationPolicy.hpp"

inline const char *mapCreationIntentLabel(MapCreationIntent intent)
{
    switch (intent)
    {
    case MapCreationIntent::WorldOverview: return "World";
    case MapCreationIntent::RegionExploration: return "Region";
    case MapCreationIntent::Settlement: return "Settlement";
    case MapCreationIntent::TravelScene: return "Travel scene";
    case MapCreationIntent::Dungeon: return "Dungeon";
    case MapCreationIntent::Cave: return "Cave";
    case MapCreationIntent::TacticalEncounter: return "Tactical map";
    case MapCreationIntent::ImportedReference: return "Imported map";
    }
    return "Map";
}

inline const char *mapScopeLabel(MapScope scope)
{
    switch (scope)
    {
    case MapScope::World:
        return "World Map";
    case MapScope::Local:
        return "Local Map";
    case MapScope::Dungeon:
        return "Dungeon";
    }
    return "Map";
}

inline const char *mapFeatureKindLabel(MapFeatureKind kind)
{
    switch (kind)
    {
    case MapFeatureKind::NamedLocation:
        return "Named Location";
    case MapFeatureKind::Door:
        return "Door";
    case MapFeatureKind::SecretDoor:
        return "Secret Door";
    case MapFeatureKind::Trap:
        return "Trap";
    case MapFeatureKind::Hazard:
        return "Hazard";
    case MapFeatureKind::Pin:
        return "Map Pin";
    }
    return "Map Feature";
}

inline const char *mapTokenKindLabel(MapTokenKind kind)
{
    switch (kind)
    {
    case MapTokenKind::Player:
        return "Player";
    case MapTokenKind::NPC:
        return "NPC";
    case MapTokenKind::Monster:
        return "Monster";
    case MapTokenKind::Vehicle:
        return "Vehicle";
    case MapTokenKind::Prop:
        return "Prop";
    case MapTokenKind::Marker:
        return "Marker";
    case MapTokenKind::Party:
        return "Party";
    }
    return "Token";
}

inline const char *mapLightingModeLabel(MapLightingMode mode)
{
    switch (mode)
    {
    case MapLightingMode::Unknown:
        return "Unspecified";
    case MapLightingMode::Bright:
        return "Bright";
    case MapLightingMode::Dim:
        return "Dim";
    case MapLightingMode::Darkness:
        return "Darkness";
    }
    return "Unspecified";
}

inline const char *mapTileKindLabel(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty:
        return "Empty";
    case MapTileKind::Water:
        return "Water";
    case MapTileKind::Forest:
        return "Forest";
    case MapTileKind::Mountain:
        return "Mountain";
    case MapTileKind::Hill:
        return "Hill";
    case MapTileKind::Road:
        return "Road";
    case MapTileKind::Settlement:
        return "Settlement";
    case MapTileKind::Building:
        return "Building";
    case MapTileKind::Room:
        return "Room";
    case MapTileKind::Wall:
        return "Wall";
    case MapTileKind::Door:
        return "Door";
    case MapTileKind::Cave:
        return "Cave";
    case MapTileKind::Stairs:
        return "Stairs";
    case MapTileKind::Hazard:
        return "Hazard";
    case MapTileKind::Landmark:
        return "Landmark";
    case MapTileKind::Dirt:
        return "Dirt";
    case MapTileKind::Stone:
        return "Stone";
    case MapTileKind::Snow:
        return "Snow";
    case MapTileKind::Desert:
        return "Desert";
    case MapTileKind::Swamp:
        return "Swamp";
    case MapTileKind::Lava:
        return "Lava";
    case MapTileKind::Ice:
        return "Ice";
    case MapTileKind::Pit:
        return "Pit";
    case MapTileKind::Rubble:
        return "Rubble";
    case MapTileKind::Cover:
        return "Cover";
    case MapTileKind::Chasm:
        return "Chasm";
    case MapTileKind::Bridge:
        return "Bridge";
    }
    return "Unknown";
}

inline const char *mapWayKindLabel(MapWayKind kind)
{
    switch (kind)
    {
    case MapWayKind::None:
        return "None";
    case MapWayKind::Trail:
        return "Trail";
    case MapWayKind::DirtRoad:
        return "Dirt Road";
    case MapWayKind::StoneRoad:
        return "Stone Road";
    case MapWayKind::River:
        return "River";
    case MapWayKind::Canal:
        return "Canal";
    case MapWayKind::DungeonCorridor:
        return "Dungeon Corridor";
    case MapWayKind::CaveTunnel:
        return "Cave Tunnel";
    }
    return "None";
}

inline const char *mapWayKindPurpose(MapWayKind kind)
{
    switch (kind)
    {
    case MapWayKind::None:
        return "No route-like way overlay.";
    case MapWayKind::Trail:
        return "Narrow wilderness path or foot trail.";
    case MapWayKind::DirtRoad:
        return "Roads, wagon tracks, and regional paths.";
    case MapWayKind::StoneRoad:
        return "Paved roads, streets, and formal causeways.";
    case MapWayKind::River:
        return "Streams and rivers that connect across cells.";
    case MapWayKind::Canal:
        return "Straightened waterways, channels, or city canals.";
    case MapWayKind::DungeonCorridor:
        return "Dungeon corridors and connected halls.";
    case MapWayKind::CaveTunnel:
        return "Natural cave tunnels and rough passages.";
    }
    return "";
}

inline const char *mapSymbolVariantModeLabel(MapSymbolVariantMode mode)
{
    switch (mode)
    {
    case MapSymbolVariantMode::Auto:
        return "Auto";
    case MapSymbolVariantMode::Continuous:
        return "Continuous";
    case MapSymbolVariantMode::Individual:
        return "Individual";
    }
    return "Auto";
}

inline const char *mapSymbolVariantModePurpose(MapSymbolVariantMode mode)
{
    switch (mode)
    {
    case MapSymbolVariantMode::Auto:
        return "Use the style's normal context rule.";
    case MapSymbolVariantMode::Continuous:
        return "Connected symbols read as one feature, like a mountain range or forest mass.";
    case MapSymbolVariantMode::Individual:
        return "Connected symbols keep separate silhouettes, like separate peaks with valleys between them.";
    }
    return "";
}

inline const char *mapTileKindCode(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty:
        return "";
    case MapTileKind::Water:
        return "WA";
    case MapTileKind::Forest:
        return "FO";
    case MapTileKind::Mountain:
        return "MT";
    case MapTileKind::Hill:
        return "HI";
    case MapTileKind::Road:
        return "RD";
    case MapTileKind::Settlement:
        return "ST";
    case MapTileKind::Building:
        return "BD";
    case MapTileKind::Room:
        return "RM";
    case MapTileKind::Wall:
        return "WL";
    case MapTileKind::Door:
        return "DR";
    case MapTileKind::Cave:
        return "CV";
    case MapTileKind::Stairs:
        return "SR";
    case MapTileKind::Hazard:
        return "HZ";
    case MapTileKind::Landmark:
        return "LM";
    case MapTileKind::Dirt:
        return "DI";
    case MapTileKind::Stone:
        return "STN";
    case MapTileKind::Snow:
        return "SN";
    case MapTileKind::Desert:
        return "DS";
    case MapTileKind::Swamp:
        return "SW";
    case MapTileKind::Lava:
        return "LAVA";
    case MapTileKind::Ice:
        return "ICE";
    case MapTileKind::Pit:
        return "PIT";
    case MapTileKind::Rubble:
        return "RUB";
    case MapTileKind::Cover:
        return "COV";
    case MapTileKind::Chasm:
        return "CH";
    case MapTileKind::Bridge:
        return "BR";
    }
    return "?";
}

inline const char *mapTileKindPurpose(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty:
        return "Open terrain, floor, or unmarked space.";
    case MapTileKind::Water:
        return "Rivers, lakes, coasts, pools, or flooded rooms.";
    case MapTileKind::Forest:
        return "Woods, jungle, brush, or heavy vegetation.";
    case MapTileKind::Mountain:
        return "Peaks, cliffs, ridges, or impassable high ground.";
    case MapTileKind::Hill:
        return "Rolling elevation, slopes, or rough raised ground.";
    case MapTileKind::Road:
        return "Roads, streets, trails, corridors, or paths.";
    case MapTileKind::Settlement:
        return "Towns, villages, camps, districts, or map hubs.";
    case MapTileKind::Building:
        return "Houses, shops, keeps, towers, or local structures.";
    case MapTileKind::Room:
        return "Dungeon rooms, chambers, halls, or combat spaces.";
    case MapTileKind::Wall:
        return "Dungeon walls, blocked cells, cliffs, or barriers.";
    case MapTileKind::Door:
        return "Doors, gates, secret doors, or transition points.";
    case MapTileKind::Cave:
        return "Caves, tunnels, caverns, mines, or natural interiors.";
    case MapTileKind::Stairs:
        return "Stairs, ladders, portals, level exits, or connectors.";
    case MapTileKind::Hazard:
        return "Traps, lava, pits, thorns, hazards, or danger zones.";
    case MapTileKind::Landmark:
        return "Named locations, ruins, monuments, or points of interest.";
    case MapTileKind::Dirt:
        return "Bare ground, trails, arenas, farms, or excavated earth.";
    case MapTileKind::Stone:
        return "Stone ground, ruins, plazas, caves, or constructed floors.";
    case MapTileKind::Snow:
        return "Snow fields, frozen mountains, winter roads, or icy battlefields.";
    case MapTileKind::Desert:
        return "Sand, dunes, dry wastes, beaches, or dust-covered arenas.";
    case MapTileKind::Swamp:
        return "Marsh, bog, mud, reeds, wetland, or poisonous ground.";
    case MapTileKind::Lava:
        return "Lava, fire, burning ground, magma flows, or intense heat hazards.";
    case MapTileKind::Ice:
        return "Ice, slippery ground, frozen water, or magical frost.";
    case MapTileKind::Pit:
        return "Pits, holes, shafts, sinkholes, or drop hazards.";
    case MapTileKind::Rubble:
        return "Broken terrain, collapsed stone, debris, or rough cover.";
    case MapTileKind::Cover:
        return "Crates, barricades, columns, low walls, or battlefield cover.";
    case MapTileKind::Chasm:
        return "Deep ravines, voids, cliffs, or impassable gaps.";
    case MapTileKind::Bridge:
        return "Bridges, planks, crossings, or elevated passages.";
    }
    return "";
}

