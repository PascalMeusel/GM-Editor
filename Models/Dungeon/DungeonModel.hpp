#pragma once

#include "Models/Core/EntityMetadata.hpp"
#include <string>
#include <vector>

enum class DungeonRoomExplorationState
{
    Unknown,
    Seen,
    Explored,
    Cleared
};

struct DungeonRoomConnectionModel
{
    EntityMetadata metadata{"room_connection"};
    EntityId targetRoomId;
    std::string label;
    bool locked = false;
    bool secret = false;
    bool discovered = false;
};

struct DungeonRoomModel
{
    EntityMetadata metadata{"room"};
    std::string name;
    std::string description;
    std::string keyedDescription;
    std::string readAloudText;
    std::string secrets;
    std::string traps;
    std::string explorationNotes;
    DungeonRoomExplorationState explorationState = DungeonRoomExplorationState::Unknown;
    int mapX = 0;
    int mapY = 0;
    std::vector<EntityId> encounterIds;
    std::vector<EntityId> lootItemIds;
    std::vector<DungeonRoomConnectionModel> connections;
};

struct DungeonModel
{
    EntityMetadata metadata{"dungeon"};
    std::string name;
    std::string description;
    EntityId parentMapId;
    EntityId activeRoomId;
    std::string gmNotes;
    std::vector<DungeonRoomModel> rooms;
    std::vector<EntityId> mapIds;
};

inline const char *dungeonRoomExplorationStateLabel(DungeonRoomExplorationState state)
{
    switch (state)
    {
    case DungeonRoomExplorationState::Unknown:
        return "Unknown";
    case DungeonRoomExplorationState::Seen:
        return "Seen";
    case DungeonRoomExplorationState::Explored:
        return "Explored";
    case DungeonRoomExplorationState::Cleared:
        return "Cleared";
    }
    return "Unknown";
}
