#pragma once

#include "../IRuleSet.hpp"

class ClockworkBloodlineRuleset : public IRuleSet
{
public:
    const char *getName() const override
    {
        return "Clockwork Bloodline: Cinder Vow Engine";
    }

    const char *getId() const override
    {
        return "clockwork-bloodline";
    }

    const char *getVersionId() const override
    {
        return "clockwork-bloodline-cinder-vow-1.0";
    }

    const std::vector<std::string> getLegacyNames() const override
    {
        return {"Cinder Vow Engine", "Clockwork Bloodline"};
    }

    const bool levelsExist() const override { return true; }

    const std::vector<std::string> getAttributes() const override
    {
        return {"Iron", "Spark", "Veil", "Grace", "Lore", "Pulse"};
    }

    const std::vector<std::string> getClasses() const override
    {
        return {"Heartwright", "Smoke-Talker", "Debt Medium", "Engine Knight", "Gearwright", "Brass Duelist", "Union Agitator", "Relic Scholar"};
    }

    const std::vector<std::string> getRaces() const override
    {
        return {"Human", "Clockmarked", "Relic-Bound", "Smoke-Touched", "Debt-Bound"};
    }

    const std::vector<ItemModel> getItems() const override
    {
        return {};
    }

    const std::vector<RuleSkillDefinition> getSkills() const override
    {
        return {
            {"fight", "Fight", "Iron"},
            {"shoot", "Shoot", "Veil"},
            {"tinker", "Tinker", "Spark"},
            {"pilot", "Pilot", "Spark"},
            {"skulk", "Skulk", "Veil"},
            {"study", "Study", "Lore"},
            {"treat", "Treat", "Spark"},
            {"command", "Command", "Grace"},
            {"charm", "Charm", "Grace"},
            {"attune", "Attune", "Pulse"},
            {"notice", "Notice", "Veil"},
            {"survive", "Survive", "Iron"}};
    }

    const std::vector<RuleDifficultyClass> getCommonDifficultyClasses() const override
    {
        return {{"Simple", 1}, {"Risky", 2}, {"Hard", 3}, {"Severe", 4}, {"Mythic", 5}};
    }

    const std::vector<RuleCurrencyDefinition> getCurrencies() const override
    {
        return {
            {"bit", "Bit", "bit", 1},
            {"mark", "Mark", "mk", 10},
            {"crown", "Crown", "cr", 100},
            {"sovereign", "Sovereign", "sv", 1000}};
    }

    const std::vector<RuleEquipmentSlotDefinition> getEquipmentSlots() const override
    {
        return {
            {"body", "Body", 1},
            {"main-hand", "Main Hand", 1},
            {"off-hand", "Off Hand", 1},
            {"two-handed", "Two-Handed", 1},
            {"head", "Head", 1},
            {"tool", "Tool Rig", 2},
            {"relic", "Relic", 1},
            {"pack", "Pack", 99},
            {"pouch", "Pouch", 99}};
    }

    const std::vector<RuleConditionDefinition> getConditionDefinitions() const override
    {
        return {
            {"overheated", "Overheated", "Ethertech is hot, unstable, or venting pressure.", {"-1 die to delicate Spark rolls.", "Overclocking risks a malfunction on 1-2."}, "Until cooled or repaired", "Clear with a safe repair action."},
            {"oathbound", "Oathbound", "A vow or divine contract is actively constraining the character.", {"Breaking the oath marks Stain.", "Honoring it can gain Favor."}, "Until fulfilled or absolved", "Fulfill, renegotiate, or ritually sever the vow."},
            {"stained", "Stained", "Occult residue has become visible to sensitive observers.", {"Hostile occult forces can track the mark.", "Attune rolls may draw attention."}, "Until cleansed", "Cleanse through downtime, confession, or sacrifice."},
            {"wounded", "Wounded", "A minor or major injury is affecting action.", {"Apply the listed injury penalty from the campaign notes."}, "Until treated", "Treat with medicine, rest, or a miracle."}};
    }

    int getAbilityModifier(int score) const override
    {
        return std::max(0, score);
    }

    int getProficiencyBonus(int level) const override
    {
        (void)level;
        return 0;
    }

    int getProficiencyContribution(int level, RuleProficiencyTier proficiency) const override
    {
        (void)level;
        switch (proficiency)
        {
        case RuleProficiencyTier::Half:
            return 1;
        case RuleProficiencyTier::Proficient:
            return 2;
        case RuleProficiencyTier::Expertise:
            return 3;
        case RuleProficiencyTier::None:
        default:
            return 0;
        }
    }
};
