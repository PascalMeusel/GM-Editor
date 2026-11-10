#pragma once

#include "Models/Rulesets/RulesetContentPackModel.hpp"

#include <string>
#include <vector>

struct RulesetMigrationStep
{
    std::string code;
    std::string message;
};

struct RulesetMigrationPlan
{
    bool requiresReview = false;
    std::vector<RulesetMigrationStep> steps;
};

RulesetMigrationPlan createRulesetMigrationPlan(const std::string &currentRulesetVersion,
                                                const std::string &nextRulesetVersion,
                                                const std::vector<RulesetContentPackModel> &currentPacks,
                                                const std::vector<RulesetContentPackModel> &nextPacks);
