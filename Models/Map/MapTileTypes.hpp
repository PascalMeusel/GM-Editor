#pragma once

#include "Models/Map/MapCoreTypes.hpp"

enum class GridType
{
    None,
    Square,
    Hex
};

enum class MapTileKind
{
    Empty,
    Water,
    Forest,
    Mountain,
    Hill,
    Road,
    Settlement,
    Building,
    Room,
    Wall,
    Door,
    Cave,
    Stairs,
    Hazard,
    Landmark,
    Dirt,
    Stone,
    Snow,
    Desert,
    Swamp,
    Lava,
    Ice,
    Pit,
    Rubble,
    Cover,
    Chasm,
    Bridge
};

enum class MapWayKind
{
    None,
    Trail,
    DirtRoad,
    StoneRoad,
    River,
    Canal,
    DungeonCorridor,
    CaveTunnel
};

enum class MapSymbolVariantMode
{
    Auto,
    Continuous,
    Individual
};

enum class MapFeatureKind
{
    NamedLocation,
    Door,
    SecretDoor,
    Trap,
    Hazard,
    Pin
};

enum class MapTokenKind
{
    Player,
    NPC,
    Monster,
    Vehicle,
    Prop,
    Marker,
    Party
};

enum class MapLightingMode
{
    Unknown,
    Bright,
    Dim,
    Darkness
};

enum class MapTacticalPreset
{
    CustomBlocker,
    Wall,
    Ledge,
    StairsUp,
    Window,
    LowCover,
    ClosedDoor,
    OpenDoor,
    RampUp,
    TreeTrunk,
    ClearEdge
};

struct MapCellModel
{
    MapTileKind kind = MapTileKind::Empty;
    int rotationQuarterTurns = 0;
    bool rotationLocked = false;
    MapSymbolVariantMode symbolVariantMode = MapSymbolVariantMode::Auto;
    MapWayKind wayKind = MapWayKind::None;
    int movementCost = 1;
    bool difficultTerrain = false;
    bool blocksLineOfSight = false;
    MapLightingMode lighting = MapLightingMode::Unknown;
    bool fogged = false;
    int elevation = 0;
    int movementBlockedMask = 0;
    int lineOfSightBlockedMask = 0;
    // Directional blockers are evaluated only while leaving this cell. Unlike
    // the symmetric edge masks above, they can model ledges and magical
    // barriers that permit travel or sight in the opposite direction.
    int movementOneWayBlockedMask = 0;
    int lineOfSightOneWayBlockedMask = 0;
};

struct MapLayerModel
{
    EntityMetadata metadata{"map_layer"};
    std::string name;
    bool visible = true;
    bool locked = false;
    int sortOrder = 0;
};

struct MapTokenModel
{
    EntityMetadata metadata{"token"};
    std::string name;
    MapTokenKind kind = MapTokenKind::Marker;
    std::string linkedEntityType;
    EntityId linkedEntityId;
    EntityId assetId;
    EntityId layerId;
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    float size = 1.0f;
    bool hiddenFromPlayers = false;
};

struct MapRegionModel
{
    EntityMetadata metadata{"map_region"};
    std::string name;
    std::string description;
    int x = 0;
    int y = 0;
    int width = 1;
    int height = 1;
    int movementCost = 1;
    MapLightingMode lighting = MapLightingMode::Unknown;
    bool difficultTerrain = false;
    bool blocksLineOfSight = false;
    bool gmOnly = false;
};

struct MapFeatureModel
{
    EntityMetadata metadata{"map_feature"};
    MapFeatureKind kind = MapFeatureKind::Pin;
    std::string name;
    std::string description;
    std::string gmNotes;
    EntityId linkedEntityId;
    EntityId targetMapId;
    int x = 0;
    int y = 0;
    bool discovered = false;
    bool gmOnly = false;
    int difficultyClass = 0;
};

struct MapRouteModel
{
    EntityMetadata metadata{"map_route"};
    std::string name;
    EntityId fromMapId;
    EntityId toMapId;
    int startX = 0;
    int startY = 0;
    int endX = 0;
    int endY = 0;
    float distanceMiles = 0.0f;
    std::string terrain;
    std::string notes;
    bool discovered = true;
};

