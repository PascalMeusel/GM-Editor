#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <string>
#include <vector>

struct CampaignSearchDocument
{
    EntityId id;
    std::string type;
    std::string title;
    std::string subtitle;
    std::string body;
    std::vector<std::string> tags;
    std::string actionHint;
};

struct CampaignSearchResult
{
    CampaignSearchDocument document;
    int score = 0;
    // Query-local ranking: an exact title must beat repeated prose matches.
    int titleMatchPriority = 0;
};

class CampaignSearchIndex
{
public:
    void rebuild(const Campaign &campaign);
    std::vector<CampaignSearchResult> search(const std::string &query, std::size_t limit = 50) const;
    const std::vector<CampaignSearchDocument> &documents() const { return _documents; }

private:
    std::vector<CampaignSearchDocument> _documents;
};
