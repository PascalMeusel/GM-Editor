#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <string>
#include <vector>

struct CampaignGraphNode
{
    EntityId id;
    std::string type;
    std::string label;
};

struct CampaignGraphEdge
{
    EntityId fromId;
    EntityId toId;
    std::string relationship;
    std::string fromLabel;
    std::string toLabel;
    std::string summary;
};

struct CampaignKnowledgeGraph
{
    std::vector<CampaignGraphNode> nodes;
    std::vector<CampaignGraphEdge> edges;
};

namespace CampaignGraph
{
CampaignKnowledgeGraph buildKnowledgeGraph(const Campaign &campaign);
}
