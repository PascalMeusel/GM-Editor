#include "CampaignSearchIndex.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace
{
std::string lowerCopy(const std::string &value)
{
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered;
}

std::vector<std::string> splitTerms(const std::string &query)
{
    std::vector<std::string> terms;
    std::istringstream stream(lowerCopy(query));
    std::string term;
    while (stream >> term)
        terms.push_back(term);
    return terms;
}

std::string normalizedPhrase(const std::string &value)
{
    std::istringstream stream(lowerCopy(value));
    std::string word;
    std::string normalized;
    while (stream >> word)
    {
        if (!normalized.empty())
            normalized += ' ';
        normalized += word;
    }
    return normalized;
}

int titleMatchPriority(const std::string &title, const std::string &normalizedQuery,
                       const std::vector<std::string> &terms)
{
    const std::string normalizedTitle = normalizedPhrase(title);
    if (normalizedTitle == normalizedQuery)
        return 3;
    if (normalizedTitle.rfind(normalizedQuery, 0) == 0)
        return 2;
    if (normalizedTitle.find(normalizedQuery) != std::string::npos ||
        std::all_of(terms.begin(), terms.end(), [&](const std::string &term) {
            return normalizedTitle.find(term) != std::string::npos;
        }))
        return 1;
    return 0;
}

std::string joinList(const std::vector<std::string> &values)
{
    std::string result;
    for (const std::string &value : values)
    {
        if (!value.empty())
        {
            if (!result.empty()) result += " ";
            result += value;
        }
    }
    return result;
}

int countOccurrences(const std::string &haystack, const std::string &needle)
{
    if (needle.empty()) return 0;
    int count = 0;
    for (std::size_t position = haystack.find(needle); position != std::string::npos;
         position = haystack.find(needle, position + needle.size()))
        ++count;
    return count;
}

int scoreField(const std::string &field, const std::vector<std::string> &terms, int weight)
{
    const std::string lowered = lowerCopy(field);
    int score = 0;
    for (const std::string &term : terms)
        score += countOccurrences(lowered, term) * weight;
    return score;
}
} // namespace
std::vector<CampaignSearchResult> CampaignSearchIndex::search(const std::string &query, std::size_t limit) const
{
    const std::vector<std::string> terms = splitTerms(query);
    if (terms.empty() || limit == 0)
        return {};
    const std::string normalizedQuery = normalizedPhrase(query);

    std::vector<CampaignSearchResult> results;
    for (const CampaignSearchDocument &document : _documents)
    {
        int score = 0;
        score += scoreField(document.title, terms, 12);
        score += scoreField(document.type, terms, 8);
        score += scoreField(document.subtitle, terms, 5);
        score += scoreField(joinList(document.tags), terms, 6);
        score += scoreField(document.body, terms, 2);
        score += scoreField(document.id, terms, 1);
        if (score > 0)
            results.push_back(CampaignSearchResult{document, score,
                titleMatchPriority(document.title, normalizedQuery, terms)});
    }

    std::sort(results.begin(), results.end(), [](const CampaignSearchResult &left, const CampaignSearchResult &right) {
        if (left.titleMatchPriority != right.titleMatchPriority)
            return left.titleMatchPriority > right.titleMatchPriority;
        if (left.score != right.score)
            return left.score > right.score;
        if (left.document.title != right.document.title)
            return left.document.title < right.document.title;
        if (left.document.type != right.document.type)
            return left.document.type < right.document.type;
        return left.document.id < right.document.id;
    });

    if (results.size() > limit)
        results.resize(limit);
    return results;
}
