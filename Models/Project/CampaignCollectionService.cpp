#include "CampaignCollectionService.hpp"

#include <algorithm>
#include <cctype>

namespace
{
std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsCaseInsensitive(const std::vector<std::string> &values, const std::string &needle)
{
    const std::string loweredNeedle = lowerCopy(needle);
    return std::any_of(values.begin(), values.end(), [&](const std::string &value) {
        return lowerCopy(value) == loweredNeedle;
    });
}

bool matchesTypeFilter(const CampaignSearchDocument &document, const CampaignSavedFilterModel &filter)
{
    return filter.entityTypes.empty() || containsCaseInsensitive(filter.entityTypes, document.type);
}

bool matchesTagFilter(const CampaignSearchDocument &document, const CampaignSavedFilterModel &filter)
{
    if (filter.requiredTags.empty())
        return true;

    const auto tagMatches = [&](const std::string &tag) {
        return containsCaseInsensitive(document.tags, tag);
    };

    if (filter.matchAllTags)
        return std::all_of(filter.requiredTags.begin(), filter.requiredTags.end(), tagMatches);
    return std::any_of(filter.requiredTags.begin(), filter.requiredTags.end(), tagMatches);
}
}

namespace CampaignCollections
{
std::vector<CampaignSearchResult> evaluate(const CampaignSearchIndex &index,
                                           const CampaignSavedFilterModel &filter,
                                           std::size_t limit)
{
    std::vector<CampaignSearchResult> candidates;
    if (filter.query.empty())
    {
        for (const CampaignSearchDocument &document : index.documents())
            candidates.push_back(CampaignSearchResult{document, 1});
    }
    else
    {
        candidates = index.search(filter.query, index.documents().size());
    }

    std::vector<CampaignSearchResult> results;
    for (const CampaignSearchResult &candidate : candidates)
    {
        if (!matchesTypeFilter(candidate.document, filter) || !matchesTagFilter(candidate.document, filter))
            continue;
        results.push_back(candidate);
        if (results.size() >= limit)
            break;
    }
    return results;
}
}
