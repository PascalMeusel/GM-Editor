#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <string>

struct CampaignIndexRebuildResult
{
    int searchableDocuments = 0;
    int graphNodes = 0;
    int graphEdges = 0;
    int brokenReferences = 0;
    std::string rebuiltAtUtc;
};

namespace CampaignIndexRebuild
{
CampaignIndexRebuildResult rebuild(const Campaign &campaign);
}
