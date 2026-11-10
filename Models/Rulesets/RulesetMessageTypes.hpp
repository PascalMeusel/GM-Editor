#pragma once

#include <string>

// Stable, presentation-neutral descriptions of changes made by a ruleset
// upgrade.  The legacy English strings remain available on
// RulesetUpgradePreview for persistence/test compatibility, but UI code should
// render these values through RulesetPresentationFormatter.
enum class RulesetDefinitionKind
{
    None,
    Ability,
    DerivedStat,
    SkillOrCheck,
    Defense,
    Resource,
    DiceFormula,
    Currency,
    Size,
    Speed,
    Condition,
    Class,
    Subclass,
    Ancestry,
    Background,
    Feature,
    EquipmentCategory,
    SpellTradition,
    MonsterTemplate
};

enum class RulesetUpgradeChangeKind
{
    Version,
    Added,
    Removed,
    Changed,
    AbilityModifierFormula,
    ProficiencyFormula,
    DefaultCheckDiceFormula,
    PassiveScoreBase
};

struct RulesetUpgradeChange
{
    RulesetUpgradeChangeKind kind = RulesetUpgradeChangeKind::Changed;
    RulesetDefinitionKind definitionKind = RulesetDefinitionKind::None;
    std::string stableId;
    std::string previousValue;
    std::string nextValue;
};
