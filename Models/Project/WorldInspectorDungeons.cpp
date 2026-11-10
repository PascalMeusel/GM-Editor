#include "WorldInspectorServiceInternal.hpp"

namespace WorldInspector
{
using namespace detail;
MutationResult createDungeon(Campaign &campaign, const std::string &name, const EntityId &parentMapId)
{
    if (!mapExists(campaign, parentMapId))
        return result(MutationStatus::NotFound, "The selected map no longer exists.");
    DungeonModel dungeon;
    dungeon.name = name.empty() ? "New Dungeon" : name;
    dungeon.parentMapId = parentMapId;
    if (!parentMapId.empty())
        dungeon.mapIds.push_back(parentMapId);
    const EntityId id = dungeon.metadata.id;
    campaign.dungeons.push_back(std::move(dungeon));
    campaign.metadata.touch();
    return result(MutationStatus::Applied, "Dungeon structure created.", id);
}

MutationResult updateDungeon(Campaign &campaign, const EntityId &dungeonId, const DungeonModel &replacement)
{
    DungeonModel *dungeon = findDungeon(campaign, dungeonId);
    if (!dungeon)
        return result(MutationStatus::NotFound, "The selected dungeon no longer exists.");
    if (replacement.name.empty())
        return result(MutationStatus::Invalid, "A dungeon name is required.");
    if (!mapExists(campaign, replacement.parentMapId))
        return result(MutationStatus::NotFound, "The selected map no longer exists.");
    const EntityMetadata metadata = dungeon->metadata;
    *dungeon = replacement;
    dungeon->metadata = metadata;
    if (!dungeon->parentMapId.empty() && std::find(dungeon->mapIds.begin(), dungeon->mapIds.end(), dungeon->parentMapId) == dungeon->mapIds.end())
        dungeon->mapIds.push_back(dungeon->parentMapId);
    touch(campaign, dungeon->metadata);
    return result(MutationStatus::Applied, "Dungeon structure saved.", dungeonId);
}

MutationResult deleteDungeon(Campaign &campaign, const EntityId &dungeonId)
{
    auto found = std::find_if(campaign.dungeons.begin(), campaign.dungeons.end(), [&](const DungeonModel &dungeon) { return dungeon.metadata.id == dungeonId; });
    if (found == campaign.dungeons.end())
        return result(MutationStatus::NotFound, "The selected dungeon no longer exists.");
    campaign.dungeons.erase(found);
    campaign.metadata.touch();
    return result(MutationStatus::Applied, "Dungeon structure deleted.", dungeonId);
}

MutationResult addRoom(Campaign &campaign, const EntityId &dungeonId, const std::string &name)
{
    DungeonModel *dungeon = findDungeon(campaign, dungeonId);
    if (!dungeon)
        return result(MutationStatus::NotFound, "The selected dungeon no longer exists.");
    DungeonRoomModel room;
    room.name = name.empty() ? "New Room" : name;
    const EntityId roomId = room.metadata.id;
    dungeon->rooms.push_back(std::move(room));
    if (dungeon->activeRoomId.empty())
        dungeon->activeRoomId = roomId;
    touch(campaign, dungeon->metadata);
    return result(MutationStatus::Applied, "Room structure added.", roomId);
}

MutationResult updateRoom(Campaign &campaign, const EntityId &dungeonId, const EntityId &roomId, const DungeonRoomModel &replacement)
{
    DungeonModel *dungeon = findDungeon(campaign, dungeonId);
    DungeonRoomModel *room = dungeon ? findRoom(*dungeon, roomId) : nullptr;
    if (!room)
        return result(MutationStatus::NotFound, "The selected room no longer exists.");
    if (replacement.name.empty())
        return result(MutationStatus::Invalid, "A room name is required.");
    const EntityMetadata metadata = room->metadata;
    *room = replacement;
    room->metadata = metadata;
    room->metadata.touch();
    touch(campaign, dungeon->metadata);
    return result(MutationStatus::Applied, "Room structure saved.", roomId);
}

MutationResult deleteRoom(Campaign &campaign, const EntityId &dungeonId, const EntityId &roomId)
{
    DungeonModel *dungeon = findDungeon(campaign, dungeonId);
    if (!dungeon)
        return result(MutationStatus::NotFound, "The selected dungeon no longer exists.");
    auto room = std::find_if(dungeon->rooms.begin(), dungeon->rooms.end(), [&](const DungeonRoomModel &candidate) { return candidate.metadata.id == roomId; });
    if (room == dungeon->rooms.end())
        return result(MutationStatus::NotFound, "The selected room no longer exists.");
    dungeon->rooms.erase(room);
    for (DungeonRoomModel &candidate : dungeon->rooms)
        candidate.connections.erase(std::remove_if(candidate.connections.begin(), candidate.connections.end(), [&](const DungeonRoomConnectionModel &connection) {
            return connection.targetRoomId == roomId;
        }), candidate.connections.end());
    if (dungeon->activeRoomId == roomId)
        dungeon->activeRoomId = dungeon->rooms.empty() ? EntityId{} : dungeon->rooms.front().metadata.id;
    touch(campaign, dungeon->metadata);
    return result(MutationStatus::Applied, "Room and inbound connections deleted.", roomId);
}

MutationResult addConnection(Campaign &campaign, const EntityId &dungeonId, const EntityId &sourceRoomId, const EntityId &targetRoomId)
{
    DungeonModel *dungeon = findDungeon(campaign, dungeonId);
    DungeonRoomModel *source = dungeon ? findRoom(*dungeon, sourceRoomId) : nullptr;
    const DungeonRoomModel *target = dungeon ? findRoom(*dungeon, targetRoomId) : nullptr;
    if (!source || !target)
        return result(MutationStatus::NotFound, "Both rooms must exist in the selected dungeon.");
    if (sourceRoomId == targetRoomId)
        return result(MutationStatus::Invalid, "A room cannot connect to itself.");
    if (std::any_of(source->connections.begin(), source->connections.end(), [&](const DungeonRoomConnectionModel &connection) { return connection.targetRoomId == targetRoomId; }))
        return result(MutationStatus::Conflict, "That room connection already exists.");
    DungeonRoomConnectionModel connection;
    connection.targetRoomId = targetRoomId;
    connection.label = target->name;
    const EntityId connectionId = connection.metadata.id;
    source->connections.push_back(std::move(connection));
    source->metadata.touch();
    touch(campaign, dungeon->metadata);
    return result(MutationStatus::Applied, "Room connection added.", connectionId);
}

MutationResult updateConnection(Campaign &campaign, const EntityId &dungeonId, const EntityId &sourceRoomId,
                                const EntityId &connectionId, const DungeonRoomConnectionModel &replacement)
{
    DungeonModel *dungeon = findDungeon(campaign, dungeonId);
    DungeonRoomModel *source = dungeon ? findRoom(*dungeon, sourceRoomId) : nullptr;
    if (!source)
        return result(MutationStatus::NotFound, "The source room no longer exists.");
    auto connection = std::find_if(source->connections.begin(), source->connections.end(), [&](const DungeonRoomConnectionModel &candidate) {
        return candidate.metadata.id == connectionId;
    });
    if (connection == source->connections.end())
        return result(MutationStatus::NotFound, "The room connection no longer exists.");
    if (replacement.targetRoomId == sourceRoomId || !findRoom(*dungeon, replacement.targetRoomId))
        return result(MutationStatus::Invalid, "Choose another existing room as the connection target.");
    if (std::any_of(source->connections.begin(), source->connections.end(), [&](const DungeonRoomConnectionModel &candidate) {
            return candidate.metadata.id != connectionId && candidate.targetRoomId == replacement.targetRoomId;
        }))
        return result(MutationStatus::Conflict, "That room connection already exists.");
    const EntityMetadata metadata = connection->metadata;
    *connection = replacement;
    connection->metadata = metadata;
    connection->metadata.touch();
    source->metadata.touch();
    touch(campaign, dungeon->metadata);
    return result(MutationStatus::Applied, "Room connection saved.", connectionId);
}

MutationResult deleteConnection(Campaign &campaign, const EntityId &dungeonId, const EntityId &sourceRoomId, const EntityId &connectionId)
{
    DungeonModel *dungeon = findDungeon(campaign, dungeonId);
    DungeonRoomModel *source = dungeon ? findRoom(*dungeon, sourceRoomId) : nullptr;
    if (!source)
        return result(MutationStatus::NotFound, "The source room no longer exists.");
    auto connection = std::find_if(source->connections.begin(), source->connections.end(), [&](const DungeonRoomConnectionModel &candidate) {
        return candidate.metadata.id == connectionId;
    });
    if (connection == source->connections.end())
        return result(MutationStatus::NotFound, "The room connection no longer exists.");
    source->connections.erase(connection);
    source->metadata.touch();
    touch(campaign, dungeon->metadata);
    return result(MutationStatus::Applied, "Room connection removed.", connectionId);
}
} // namespace WorldInspector
