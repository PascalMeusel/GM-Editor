#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Rulesets/RulesetDefinitionModel.hpp"

#include <memory>
#include <vector>

struct RulesetLibraryEntry
{
    std::shared_ptr<IRuleSet> ruleSet;
    RulesetReference reference;
    bool builtIn = false;
};

// Builds the rulesets offered while creating a campaign. The compiled D&D
// package is always the first, safe fallback. Only immutable, globally
// installed definition versions are exposed alongside it.
std::vector<RulesetLibraryEntry> buildRulesetLibrary(
    const std::vector<RulesetDefinitionModel> &installedDefinitions);

// Returns the exact persisted pin represented by a runtime ruleset. Definition
// versions use their logical ID and plain published version, rather than the
// combined display/version ID used by IRuleSet.
RulesetReference rulesetReferenceFor(const IRuleSet &ruleSet);

// Applies an exact ruleset pin before a newly-created campaign is persisted.
void pinCampaignRuleset(Campaign &campaign);
