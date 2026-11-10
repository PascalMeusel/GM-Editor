#include "RulesetLibrary.hpp"

#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Models/Rulesets/RulesetResolution.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

namespace
{
std::string logicalIdOf(const RulesetDefinitionModel &definition)
{
    return definition.rulesetId.empty() ? definition.metadata.id : definition.rulesetId;
}

std::string pinKey(const std::string &rulesetId, const std::string &version)
{
    return rulesetId + "\n" + version;
}
}

std::vector<RulesetLibraryEntry> buildRulesetLibrary(
    const std::vector<RulesetDefinitionModel> &installedDefinitions)
{
    std::vector<RulesetLibraryEntry> entries;
    auto dnd = std::make_shared<DND5e>();
    entries.push_back({dnd, rulesetReferenceFor(*dnd), true});

    std::vector<RulesetDefinitionModel> publishedDefinitions;
    publishedDefinitions.reserve(installedDefinitions.size());
    for (const RulesetDefinitionModel &definition : installedDefinitions)
    {
        if (!definition.installedGlobally || !definition.published)
            continue;
        if (logicalIdOf(definition).empty() || definition.version.empty())
            continue;
        publishedDefinitions.push_back(definition);
    }

    std::stable_sort(publishedDefinitions.begin(), publishedDefinitions.end(),
                     [](const RulesetDefinitionModel &left, const RulesetDefinitionModel &right) {
                         if (left.name != right.name)
                             return left.name < right.name;
                         if (left.version != right.version)
                             return left.version < right.version;
                         return logicalIdOf(left) < logicalIdOf(right);
                     });

    std::set<std::string> seenPins;
    seenPins.insert(pinKey(entries.front().reference.rulesetId, entries.front().reference.version));
    for (const RulesetDefinitionModel &definition : publishedDefinitions)
    {
        RulesetReference reference;
        reference.rulesetId = logicalIdOf(definition);
        reference.version = definition.version;
        if (!seenPins.insert(pinKey(reference.rulesetId, reference.version)).second)
            continue;

        entries.push_back({std::make_shared<DefinitionBackedRuleSet>(definition),
                           std::move(reference),
                           false});
    }
    return entries;
}

RulesetReference rulesetReferenceFor(const IRuleSet &ruleSet)
{
    RulesetReference reference;
    if (const auto *definitionRules = dynamic_cast<const DefinitionBackedRuleSet *>(&ruleSet))
    {
        const RulesetDefinitionModel &definition = definitionRules->definition();
        reference.rulesetId = logicalIdOf(definition);
        reference.version = definition.version;
        return reference;
    }

    reference.rulesetId = ruleSet.getId();
    reference.version = ruleSet.getVersionId();
    return reference;
}

void pinCampaignRuleset(Campaign &campaign)
{
    if (!campaign.ruleSet)
    {
        campaign.rulesetReference = {};
        campaign.rulesetResolution = {};
        return;
    }
    campaign.rulesetReference = rulesetReferenceFor(*campaign.ruleSet);
    campaign.rulesetResolution = {true, campaign.rulesetReference, campaign.ruleSet, SpellcastingValues::Reason::None};
}
