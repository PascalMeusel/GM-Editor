#pragma once
#include "RulesetResolutionState.hpp"
#include <vector>

struct Campaign;
bool noRulesetPins(const RulesetReference &reference);
bool sameRulesetPins(const RulesetReference &left, const RulesetReference &right);
const RulesetDefinitionModel *findPinnedRulesetDefinition(const std::vector<RulesetDefinitionModel> &,
    const std::string &id, const std::string &version, bool overlay, SpellcastingValues::Reason &reason);
// Owns no database access. Exact equivalent embedded/installed copies coalesce;
// contradictory or unavailable pins are recorded without rewriting the pins.
bool resolveCampaignRuleset(Campaign &campaign, const std::vector<RulesetDefinitionModel> &available);
SpellcastingValues::Reason campaignRulesetResolution(const Campaign &campaign);
