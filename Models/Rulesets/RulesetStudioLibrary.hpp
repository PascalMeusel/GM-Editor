#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Rulesets/RulesetDefinitionModel.hpp"

#include <string>
#include <vector>

struct RulesetStudioVersionEntry
{
    RulesetDefinitionModel definition;
    int campaignUseCount = 0;

    bool editable() const { return !definition.published; }
};

struct RulesetStudioFamily
{
    std::string rulesetId;
    std::string name;
    std::vector<RulesetStudioVersionEntry> drafts;
    // Newest published version first.
    std::vector<RulesetStudioVersionEntry> publishedVersions;

    int campaignUseCount() const;
};

// Builds the Ruleset Studio library from globally installed definitions. Drafts
// stay visible and editable while published versions form a read-only history.
std::vector<RulesetStudioFamily> buildRulesetStudioLibrary(
    const std::vector<RulesetDefinitionModel> &installedDefinitions,
    const std::vector<Campaign> &campaigns = {});

// Compares numeric dotted versions (for example 1.10.0 > 1.2.0), with a
// deterministic lexical fallback for custom version labels.
int compareRulesetVersions(const std::string &left, const std::string &right);

// Suggests the next patch version after the newest published version in the
// family. Returns 1.0.0 when no numeric published version exists.
std::string suggestNextRulesetVersion(
    const std::string &rulesetId,
    const std::vector<RulesetDefinitionModel> &installedDefinitions);

// Copies an immutable published version into the family's single editable
// draft slot. The source definition remains unchanged.
RulesetDefinitionModel createNextRulesetVersionDraft(
    const RulesetDefinitionModel &publishedSource,
    const std::vector<RulesetDefinitionModel> &installedDefinitions);
