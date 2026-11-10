#include "RulesetMigrationPlan.hpp"

#include <map>

namespace
{
void addStep(RulesetMigrationPlan &plan, const std::string &code, const std::string &message)
{
    plan.requiresReview = true;
    plan.steps.push_back(RulesetMigrationStep{code, message});
}
}

RulesetMigrationPlan createRulesetMigrationPlan(const std::string &currentRulesetVersion,
                                                const std::string &nextRulesetVersion,
                                                const std::vector<RulesetContentPackModel> &currentPacks,
                                                const std::vector<RulesetContentPackModel> &nextPacks)
{
    RulesetMigrationPlan plan;
    if (!currentRulesetVersion.empty() && currentRulesetVersion != nextRulesetVersion)
    {
        addStep(plan,
                "ruleset_version_changed",
                "Ruleset version changes from " + currentRulesetVersion + " to " + nextRulesetVersion + "; review derived stats, formulas, actions, and house rules.");
    }

    std::map<std::string, RulesetContentPackModel> currentById;
    std::map<std::string, RulesetContentPackModel> nextById;
    for (const RulesetContentPackModel &pack : currentPacks)
        currentById[pack.metadata.id] = pack;
    for (const RulesetContentPackModel &pack : nextPacks)
        nextById[pack.metadata.id] = pack;

    for (const auto &entry : nextById)
    {
        const auto current = currentById.find(entry.first);
        if (current == currentById.end())
        {
            if (entry.second.enabled)
                addStep(plan, "content_pack_enabled", "Enable new homebrew pack: " + entry.second.name + ".");
            continue;
        }

        if (current->second.enabled != entry.second.enabled)
        {
            addStep(plan,
                    entry.second.enabled ? "content_pack_enabled" : "content_pack_disabled",
                    std::string(entry.second.enabled ? "Enable" : "Disable") + " homebrew pack: " + entry.second.name + ".");
        }
    }

    for (const auto &entry : currentById)
    {
        if (entry.second.enabled && nextById.find(entry.first) == nextById.end())
            addStep(plan, "content_pack_disabled", "Disable removed homebrew pack: " + entry.second.name + ".");
    }

    return plan;
}
