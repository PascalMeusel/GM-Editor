#include "CampaignGraphService.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace
{
std::string fallback(const std::string &value, const std::string &fallbackValue)
{
    return value.empty() ? fallbackValue : value;
}

std::string characterName(const CharacterModel &character)
{
    return fallback(std::string(character.nameBuffer), "Unnamed character");
}

void addNode(CampaignKnowledgeGraph &graph, std::unordered_set<std::string> &seen, const EntityId &id, const std::string &type, const std::string &label)
{
    if (id.empty() || !seen.insert(id).second)
        return;
    graph.nodes.push_back(CampaignGraphNode{id, type, fallback(label, type)});
}

void addEdge(CampaignKnowledgeGraph &graph, const EntityId &fromId, const EntityId &toId, const std::string &relationship)
{
    if (fromId.empty() || toId.empty() || fromId == toId)
        return;
    graph.edges.push_back(CampaignGraphEdge{fromId, toId, relationship});
}

std::string nodeDisplay(const CampaignGraphNode &node)
{
    return node.label.empty() ? node.id : node.label + " [" + node.type + "]";
}

void resolveEdgeSummaries(CampaignKnowledgeGraph &graph)
{
    std::unordered_map<EntityId, CampaignGraphNode> nodesById;
    for (const CampaignGraphNode &node : graph.nodes)
        nodesById[node.id] = node;

    for (CampaignGraphEdge &edge : graph.edges)
    {
        const auto from = nodesById.find(edge.fromId);
        const auto to = nodesById.find(edge.toId);
        edge.fromLabel = from == nodesById.end() ? edge.fromId : nodeDisplay(from->second);
        edge.toLabel = to == nodesById.end() ? edge.toId : nodeDisplay(to->second);
        edge.summary = edge.fromLabel + " " + fallback(edge.relationship, "relates to") + " " + edge.toLabel;
    }
}
}

namespace CampaignGraph
{
CampaignKnowledgeGraph buildKnowledgeGraph(const Campaign &campaign)
{
    CampaignKnowledgeGraph graph;
    std::unordered_set<std::string> seenNodes;

    for (const CharacterModel &character : campaign.characters)
        addNode(graph, seenNodes, character.metadata.id, "Player", characterName(character));

    for (const NPCModel &npc : campaign.npcs)
    {
        addNode(graph, seenNodes, npc.metadata.id, "NPC", fallback(npc.name, "Unnamed NPC"));
        addEdge(graph, npc.metadata.id, npc.factionId, "member of");
        addEdge(graph, npc.metadata.id, npc.deityId, "serves");
    }

    for (const FactionModel &faction : campaign.factions)
    {
        addNode(graph, seenNodes, faction.metadata.id, "Faction", fallback(faction.name, "Unnamed faction"));
        addEdge(graph, faction.metadata.id, faction.leaderNpcId, "led by");
    }

    for (const DeityModel &deity : campaign.deities)
        addNode(graph, seenNodes, deity.metadata.id, "Deity", fallback(deity.name, "Unnamed deity"));

    for (const MapModel &map : campaign.maps)
    {
        addNode(graph, seenNodes, map.metadata.id, "Location", fallback(map.name, "Unnamed map"));
        addEdge(graph, map.metadata.id, map.parentMapId, "inside");
        for (const MapTokenModel &token : map.tokens)
            addEdge(graph, map.metadata.id, token.linkedEntityId, "contains token");
        for (const MapFeatureModel &feature : map.features)
        {
            addNode(graph, seenNodes, feature.metadata.id, "Clue", fallback(feature.name, "Map feature"));
            addEdge(graph, map.metadata.id, feature.metadata.id, "has feature");
            addEdge(graph, feature.metadata.id, feature.linkedEntityId, "points to");
            addEdge(graph, feature.metadata.id, feature.targetMapId, "leads to");
        }
        for (const MapRouteModel &route : map.routes)
            addEdge(graph, route.fromMapId, route.toMapId, fallback(route.name, "route"));
    }

    for (const DungeonModel &dungeon : campaign.dungeons)
    {
        addNode(graph, seenNodes, dungeon.metadata.id, "Location", fallback(dungeon.name, "Unnamed dungeon"));
        addEdge(graph, dungeon.metadata.id, dungeon.parentMapId, "beneath");
        for (const EntityId &mapId : dungeon.mapIds)
            addEdge(graph, dungeon.metadata.id, mapId, "uses map");
        for (const DungeonRoomModel &room : dungeon.rooms)
        {
            addNode(graph, seenNodes, room.metadata.id, "Location", fallback(room.name, "Dungeon room"));
            addEdge(graph, dungeon.metadata.id, room.metadata.id, "contains room");
            for (const DungeonRoomConnectionModel &connection : room.connections)
                addEdge(graph, room.metadata.id, connection.targetRoomId, fallback(connection.label, "connects to"));
        }
    }

    for (const StoryThreadModel &thread : campaign.storyThreads)
    {
        addNode(graph, seenNodes, thread.metadata.id, "Story", fallback(thread.title, "Untitled story"));
        for (const StoryLinkModel &link : thread.links)
        {
            addNode(graph, seenNodes, link.metadata.id, "Clue", fallback(link.label, "Story link"));
            addEdge(graph, thread.metadata.id, link.metadata.id, storyLinkTypeLabel(link.type));
            addEdge(graph, link.metadata.id, link.targetEntityId, "targets");
        }
        for (const StoryBeatModel &beat : thread.beats)
        {
            addNode(graph, seenNodes, beat.metadata.id, "Story Beat", fallback(beat.title, "Untitled beat"));
            addEdge(graph, thread.metadata.id, beat.metadata.id, "contains beat");
            addEdge(graph, beat.metadata.id, beat.cueAudioAssetId, "audio cue");
            for (const StoryLinkModel &link : beat.links)
            {
                addNode(graph, seenNodes, link.metadata.id, "Clue", fallback(link.label, "Story link"));
                addEdge(graph, beat.metadata.id, link.metadata.id, storyLinkTypeLabel(link.type));
                addEdge(graph, link.metadata.id, link.targetEntityId, "targets");
            }
        }
    }

    for (const TimelineEventModel &event : campaign.timelineEvents)
    {
        addNode(graph, seenNodes, event.metadata.id, "Story", fallback(event.title, "Timeline event"));
        for (const EntityId &relatedId : event.relatedEntityIds)
            addEdge(graph, event.metadata.id, relatedId, "related to");
    }

    for (const RandomTableModel &table : campaign.randomTables)
    {
        addNode(graph, seenNodes, table.metadata.id, "Random Table", fallback(table.name, "Unnamed random table"));
        for (const RandomTableEntryModel &entry : table.entries)
        {
            addNode(graph, seenNodes, entry.metadata.id, "Table Result", fallback(entry.result, "Unnamed result"));
            addEdge(graph, table.metadata.id, entry.metadata.id, "contains result");
            addEdge(graph, entry.metadata.id, entry.linkedEntityId, "links to");
        }
    }

    std::sort(graph.nodes.begin(), graph.nodes.end(), [](const CampaignGraphNode &left, const CampaignGraphNode &right) {
        if (left.type == right.type)
            return left.label < right.label;
        return left.type < right.type;
    });
    resolveEdgeSummaries(graph);
    std::sort(graph.edges.begin(), graph.edges.end(), [](const CampaignGraphEdge &left, const CampaignGraphEdge &right) {
        if (left.fromLabel == right.fromLabel)
            return left.relationship < right.relationship;
        return left.fromLabel < right.fromLabel;
    });
    return graph;
}
}
