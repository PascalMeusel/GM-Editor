#include "RulesetStudioLibrary.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <numeric>

namespace
{
std::string logicalIdOf(const RulesetDefinitionModel &definition)
{
    return definition.rulesetId.empty() ? definition.metadata.id : definition.rulesetId;
}

bool parseNumericVersion(const std::string &value, std::vector<int> &parts)
{
    parts.clear();
    std::size_t index = (!value.empty() && (value.front() == 'v' || value.front() == 'V')) ? 1 : 0;
    if (index == value.size())
        return false;

    while (index < value.size())
    {
        if (!std::isdigit(static_cast<unsigned char>(value[index])))
            return false;
        int part = 0;
        while (index < value.size() && std::isdigit(static_cast<unsigned char>(value[index])))
        {
            part = part * 10 + (value[index] - '0');
            ++index;
        }
        parts.push_back(part);
        if (index == value.size())
            break;
        if (value[index] != '.')
            return false;
        ++index;
        if (index == value.size())
            return false;
    }
    return !parts.empty();
}

int campaignUseCount(const std::string &rulesetId,
                     const std::string &version,
                     const std::vector<Campaign> &campaigns)
{
    return static_cast<int>(std::count_if(campaigns.begin(), campaigns.end(), [&](const Campaign &campaign) {
        return campaign.rulesetReference.rulesetId == rulesetId &&
               campaign.rulesetReference.version == version;
    }));
}
}

int RulesetStudioFamily::campaignUseCount() const
{
    return std::accumulate(publishedVersions.begin(), publishedVersions.end(), 0,
                           [](int total, const RulesetStudioVersionEntry &entry) {
                               return total + entry.campaignUseCount;
                           });
}

int compareRulesetVersions(const std::string &left, const std::string &right)
{
    if (left == right)
        return 0;

    std::vector<int> leftParts;
    std::vector<int> rightParts;
    if (parseNumericVersion(left, leftParts) && parseNumericVersion(right, rightParts))
    {
        const std::size_t count = std::max(leftParts.size(), rightParts.size());
        leftParts.resize(count, 0);
        rightParts.resize(count, 0);
        if (leftParts < rightParts)
            return -1;
        if (rightParts < leftParts)
            return 1;
        return 0;
    }
    return left < right ? -1 : 1;
}

std::vector<RulesetStudioFamily> buildRulesetStudioLibrary(
    const std::vector<RulesetDefinitionModel> &installedDefinitions,
    const std::vector<Campaign> &campaigns)
{
    std::map<std::string, RulesetStudioFamily> byId;
    for (const RulesetDefinitionModel &definition : installedDefinitions)
    {
        if (!definition.installedGlobally)
            continue;
        const std::string rulesetId = logicalIdOf(definition);
        if (rulesetId.empty())
            continue;

        RulesetStudioFamily &family = byId[rulesetId];
        family.rulesetId = rulesetId;
        if (family.name.empty() || (!definition.name.empty() && definition.published))
            family.name = definition.name.empty() ? rulesetId : definition.name;

        RulesetStudioVersionEntry entry;
        entry.definition = definition;
        entry.campaignUseCount = campaignUseCount(rulesetId, definition.version, campaigns);
        if (definition.published)
        {
            const auto existing = std::find_if(family.publishedVersions.begin(), family.publishedVersions.end(),
                                               [&](const RulesetStudioVersionEntry &candidate) {
                                                   return candidate.definition.version == definition.version;
                                               });
            if (existing == family.publishedVersions.end())
                family.publishedVersions.push_back(std::move(entry));
        }
        else
            family.drafts.push_back(std::move(entry));
    }

    std::vector<RulesetStudioFamily> result;
    result.reserve(byId.size());
    for (auto &[id, family] : byId)
    {
        std::stable_sort(family.publishedVersions.begin(), family.publishedVersions.end(),
                         [](const RulesetStudioVersionEntry &left, const RulesetStudioVersionEntry &right) {
                             return compareRulesetVersions(left.definition.version, right.definition.version) > 0;
                         });
        std::stable_sort(family.drafts.begin(), family.drafts.end(),
                         [](const RulesetStudioVersionEntry &left, const RulesetStudioVersionEntry &right) {
                             return left.definition.metadata.updatedAtUtc > right.definition.metadata.updatedAtUtc;
                         });
        result.push_back(std::move(family));
    }

    std::stable_sort(result.begin(), result.end(), [](const RulesetStudioFamily &left, const RulesetStudioFamily &right) {
        if (left.name != right.name)
            return left.name < right.name;
        return left.rulesetId < right.rulesetId;
    });
    return result;
}

std::string suggestNextRulesetVersion(
    const std::string &rulesetId,
    const std::vector<RulesetDefinitionModel> &installedDefinitions)
{
    const RulesetDefinitionModel *newest = nullptr;
    for (const RulesetDefinitionModel &definition : installedDefinitions)
    {
        if (!definition.installedGlobally || !definition.published || logicalIdOf(definition) != rulesetId)
            continue;
        std::vector<int> ignored;
        if (!parseNumericVersion(definition.version, ignored))
            continue;
        if (!newest || compareRulesetVersions(definition.version, newest->version) > 0)
            newest = &definition;
    }
    if (!newest)
        return "1.0.0";

    std::vector<int> parts;
    parseNumericVersion(newest->version, parts);
    parts.resize(3, 0);
    ++parts[2];
    return std::to_string(parts[0]) + "." + std::to_string(parts[1]) + "." + std::to_string(parts[2]);
}

RulesetDefinitionModel createNextRulesetVersionDraft(
    const RulesetDefinitionModel &publishedSource,
    const std::vector<RulesetDefinitionModel> &installedDefinitions)
{
    RulesetDefinitionModel draft = publishedSource;
    draft.rulesetId = logicalIdOf(publishedSource);
    draft.version = suggestNextRulesetVersion(draft.rulesetId, installedDefinitions);
    draft.published = false;
    draft.publishedAtUtc.clear();
    draft.installedGlobally = true;
    draft.metadata.id = draft.rulesetId + "@draft";
    draft.metadata.createdAtUtc = currentUtcTimestamp();
    draft.metadata.updatedAtUtc = draft.metadata.createdAtUtc;
    return draft;
}
