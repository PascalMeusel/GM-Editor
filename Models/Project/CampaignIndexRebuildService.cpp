#include "CampaignIndexRebuildService.hpp"

#include "Models/Project/CampaignGraphService.hpp"
#include "Models/Project/CampaignReferenceService.hpp"
#include "Models/Project/CampaignSearchIndex.hpp"

namespace CampaignIndexRebuild
{
CampaignIndexRebuildResult rebuild(const Campaign &campaign)
{
    CampaignSearchIndex searchIndex;
    searchIndex.rebuild(campaign);
    const CampaignKnowledgeGraph graph = CampaignGraph::buildKnowledgeGraph(campaign);
    const std::vector<CampaignBrokenReference> diagnostics = CampaignReferences::brokenReferences(campaign);

    CampaignIndexRebuildResult result;
    result.searchableDocuments = static_cast<int>(searchIndex.documents().size());
    result.graphNodes = static_cast<int>(graph.nodes.size());
    result.graphEdges = static_cast<int>(graph.edges.size());
    result.brokenReferences = static_cast<int>(diagnostics.size());
    result.rebuiltAtUtc = currentUtcTimestamp();
    return result;
}
}
