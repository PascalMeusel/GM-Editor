#pragma once

#include "Models/Rulesets/IRuleSet.hpp"
#include "Models/Rulesets/RulesetDefinitionModel.hpp"
#include "Models/Character/CharacterModel.hpp"

#include <map>
#include <string>
#include <vector>

struct RulesEvaluationContext
{
    int level = 1;
    std::map<std::string, int> values;
    std::map<std::string, double> additionalVariables;
};

struct RulesEvaluationSnapshot
{
    bool valid = true;
    int proficiency = 0;
    std::map<std::string, double> abilityModifiers;
    std::map<std::string, double> derivedStats;
    std::map<std::string, double> defenses;
    std::map<std::string, double> resourceMaximums;
    std::vector<std::string> errors;
};

class DefinitionBackedRuleSet final : public IRuleSet
{
public:
    explicit DefinitionBackedRuleSet(RulesetDefinitionModel definition);
    DefinitionBackedRuleSet(const RulesetDefinitionModel &baseDefinition,
                            const RulesetDefinitionModel &overlayDefinition);

    const RulesetDefinitionModel &definition() const { return _definition; }
    RulesEvaluationSnapshot evaluate(const RulesEvaluationContext &context) const;

    const char *getName() const override;
    const char *getId() const override;
    const char *getVersionId() const override;
    const bool levelsExist() const override;
    const std::vector<std::string> getAttributes() const override;
    const std::vector<std::string> getClasses() const override;
    const std::vector<std::string> getRaces() const override;
    const std::vector<ItemModel> getItems() const override;
    const std::vector<RuleSkillDefinition> getSkills() const override;
    const std::vector<RuleCurrencyDefinition> getCurrencies() const override;
    const std::vector<RuleConditionDefinition> getConditionDefinitions() const override;
    int getAbilityModifier(int score) const override;
    int getAbilityModifier(const std::map<std::string, int> &attributes, const std::string &ability) const override;
    int getProficiencyBonus(int level) const override;
    RuleCheckResult evaluateCheck(const std::map<std::string, int> &attributes, const RuleCheckRequest &request) const override;
    RuleRestResult applyRest(const RuleRestRequest &request) const override;
    RuleDiceSummary summarizeDiceExpression(const std::string &expression) const override;
    RuleDiceRollResult evaluateDiceExpression(const std::string &expression, const std::vector<int> &suppliedRolls) const override;

private:
    RulesetDefinitionModel _definition;
    std::string _name;
    std::string _id;
    std::string _versionId;
};

// Copies legacy display-name keyed attributes into stable definition IDs.
// Legacy values remain untouched so older compiled rulesets keep working.
void migrateLegacyCharacterRuleValues(CharacterModel &character,
                                      const RulesetDefinitionModel &definition);
