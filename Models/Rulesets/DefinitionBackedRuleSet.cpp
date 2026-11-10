#include "DefinitionBackedRuleSet.hpp"

#include "Models/Rulesets/RulesetDefinitionLayering.hpp"
#include "Models/Rulesets/RulesetDefinitionValidator.hpp"
#include "Models/Rulesets/RulesetFormulaEvaluator.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
void appendEvaluationErrors(RulesEvaluationSnapshot &snapshot,
                            const std::string &path,
                            const RulesetFormulaEvaluationResult &result)
{
    if (result.valid)
        return;
    snapshot.valid = false;
    for (const std::string &error : result.errors)
        snapshot.errors.push_back(path + ": " + error);
}

std::string versionIdFor(const RulesetDefinitionModel &definition)
{
    if (definition.version.empty())
        return definition.metadata.id;
    const std::string id = definition.rulesetId.empty() ? definition.metadata.id : definition.rulesetId;
    if (id.empty())
        return definition.version;
    return id + "@" + definition.version;
}
}

DefinitionBackedRuleSet::DefinitionBackedRuleSet(RulesetDefinitionModel definition)
    : _definition(std::move(definition))
{
    _name = _definition.name.empty() ? "Custom Ruleset" : _definition.name;
    _id = !_definition.rulesetId.empty() ? _definition.rulesetId :
          (_definition.metadata.id.empty() ? _name : _definition.metadata.id);
    _versionId = versionIdFor(_definition);
}

DefinitionBackedRuleSet::DefinitionBackedRuleSet(const RulesetDefinitionModel &baseDefinition,
                                                 const RulesetDefinitionModel &overlayDefinition)
    : DefinitionBackedRuleSet(createLayeredRulesetDefinition(baseDefinition, overlayDefinition))
{
}

const char *DefinitionBackedRuleSet::getName() const
{
    return _name.c_str();
}

const char *DefinitionBackedRuleSet::getId() const
{
    return _id.c_str();
}

const char *DefinitionBackedRuleSet::getVersionId() const
{
    return _versionId.c_str();
}

const bool DefinitionBackedRuleSet::levelsExist() const
{
    return !_definition.classes.empty() || _definition.proficiencyFormula.find("level") != std::string::npos;
}

const std::vector<std::string> DefinitionBackedRuleSet::getAttributes() const
{
    std::vector<std::string> values;
    values.reserve(_definition.abilityScores.size());
    for (const RulesetAbilityScoreDefinition &ability : _definition.abilityScores)
        values.push_back(ability.name.empty() ? ability.id : ability.name);
    return values;
}

const std::vector<std::string> DefinitionBackedRuleSet::getClasses() const
{
    std::vector<std::string> values;
    values.reserve(_definition.classes.size());
    for (const RulesetClassDefinition &classDefinition : _definition.classes)
        values.push_back(classDefinition.name.empty() ? classDefinition.id : classDefinition.name);
    return values;
}

const std::vector<std::string> DefinitionBackedRuleSet::getRaces() const
{
    std::vector<std::string> values;
    values.reserve(_definition.ancestries.size());
    for (const RulesetAncestryDefinition &ancestry : _definition.ancestries)
        values.push_back(ancestry.name.empty() ? ancestry.id : ancestry.name);
    return values;
}

const std::vector<ItemModel> DefinitionBackedRuleSet::getItems() const
{
    return {};
}

const std::vector<RuleSkillDefinition> DefinitionBackedRuleSet::getSkills() const
{
    std::vector<RuleSkillDefinition> values;
    values.reserve(_definition.skills.size());
    for (const RulesetSkillDefinition &skill : _definition.skills)
        values.push_back({skill.id, skill.name, skill.abilityScoreId});
    return values;
}

const std::vector<RuleCurrencyDefinition> DefinitionBackedRuleSet::getCurrencies() const
{
    std::vector<RuleCurrencyDefinition> values;
    values.reserve(_definition.currencies.size());
    for (const RulesetCurrencyDefinition &currency : _definition.currencies)
        values.push_back({currency.id, currency.name, currency.code, currency.baseUnitValue});
    return values.empty() ? IRuleSet::getCurrencies() : values;
}

const std::vector<RuleConditionDefinition> DefinitionBackedRuleSet::getConditionDefinitions() const
{
    std::vector<RuleConditionDefinition> values;
    values.reserve(_definition.conditions.size());
    for (const RulesetConditionDefinition &condition : _definition.conditions)
    {
        RuleConditionDefinition value;
        value.id = condition.id;
        value.name = condition.name;
        value.summary = condition.description;
        value.mechanicalEffects = condition.mechanicalEffects;
        value.endCondition = condition.endCondition;
        value.valid = true;
        values.push_back(std::move(value));
    }
    return values;
}

int DefinitionBackedRuleSet::getAbilityModifier(int score) const
{
    const RulesetFormulaEvaluationResult result = evaluateRulesetFormula(
        _definition.abilityModifierFormula,
        {{"score", static_cast<double>(score)}});
    return result.valid ? static_cast<int>(std::floor(result.value)) : IRuleSet::getAbilityModifier(score);
}

int DefinitionBackedRuleSet::getAbilityModifier(const std::map<std::string, int> &attributes,
                                                 const std::string &ability) const
{
    auto found = attributes.find(ability);
    if (found == attributes.end())
    {
        for (const RulesetAbilityScoreDefinition &definition : _definition.abilityScores)
        {
            if (definition.id != ability && definition.name != ability)
                continue;
            found = attributes.find(definition.id);
            if (found == attributes.end())
                found = attributes.find(definition.name);
            break;
        }
    }
    return found == attributes.end() ? 0 : getAbilityModifier(found->second);
}

int DefinitionBackedRuleSet::getProficiencyBonus(int level) const
{
    const RulesetFormulaEvaluationResult result = evaluateRulesetFormula(
        _definition.proficiencyFormula,
        {{"level", static_cast<double>(std::max(1, level))}});
    return result.valid ? static_cast<int>(std::floor(result.value)) : IRuleSet::getProficiencyBonus(level);
}

RuleCheckResult DefinitionBackedRuleSet::evaluateCheck(const std::map<std::string, int> &attributes,
                                                       const RuleCheckRequest &request) const
{
    RuleCheckResult result = IRuleSet::evaluateCheck(attributes, request);
    result.passiveScore = _definition.passiveScoreBase + result.modifier;
    return result;
}

RuleRestResult DefinitionBackedRuleSet::applyRest(const RuleRestRequest &request) const
{
    RuleRestResult result = IRuleSet::applyRest(request);
    const std::string trigger = request.type == RuleRestType::LongRest ? "Long Rest" : "Short Rest";
    for (const RulesetResourceDefinition &resource : _definition.resources)
    {
        if (resource.recoveryTrigger == trigger)
            result.recoveredResources.push_back(resource.name.empty() ? resource.id : resource.name);
    }
    return result;
}

RuleDiceSummary DefinitionBackedRuleSet::summarizeDiceExpression(const std::string &expression) const
{
    // The compiled D&D package owns the mature system-neutral NdM parser today.
    // Reusing it keeps definition-backed dice behavior compatible while combat
    // action automation remains intentionally code-backed in v1.
    static const DND5e dicePrimitives;
    return dicePrimitives.summarizeDiceExpression(expression);
}

RuleDiceRollResult DefinitionBackedRuleSet::evaluateDiceExpression(const std::string &expression,
                                                                   const std::vector<int> &suppliedRolls) const
{
    static const DND5e dicePrimitives;
    return dicePrimitives.evaluateDiceExpression(expression, suppliedRolls);
}

RulesEvaluationSnapshot DefinitionBackedRuleSet::evaluate(const RulesEvaluationContext &context) const
{
    RulesEvaluationSnapshot snapshot;
    std::map<std::string, double> variables = context.additionalVariables;
    variables["level"] = static_cast<double>(std::max(1, context.level));
    snapshot.proficiency = getProficiencyBonus(context.level);
    variables["proficiency"] = static_cast<double>(snapshot.proficiency);

    for (const RulesetAbilityScoreDefinition &ability : _definition.abilityScores)
    {
        const auto found = context.values.find(ability.id);
        const int score = found == context.values.end() ? 0 : found->second;
        variables[ability.id] = static_cast<double>(score);
        const double modifier = static_cast<double>(getAbilityModifier(score));
        variables[ability.id + ".mod"] = modifier;
        snapshot.abilityModifiers[ability.id] = modifier;
    }

    // Resolve derived values in dependency order without requiring authors to
    // arrange rows manually. Validation reports cycles and unresolved symbols.
    std::vector<bool> resolved(_definition.derivedStats.size(), false);
    for (std::size_t pass = 0; pass < _definition.derivedStats.size(); ++pass)
    {
        bool progress = false;
        for (std::size_t index = 0; index < _definition.derivedStats.size(); ++index)
        {
            if (resolved[index])
                continue;
            const RulesetDerivedStatDefinition &stat = _definition.derivedStats[index];
            const RulesetFormulaEvaluationResult result = evaluateRulesetFormula(stat.formula, variables);
            if (!result.valid)
                continue;
            variables[stat.id] = result.value;
            snapshot.derivedStats[stat.id] = result.value;
            resolved[index] = true;
            progress = true;
        }
        if (!progress)
            break;
    }

    for (std::size_t index = 0; index < _definition.derivedStats.size(); ++index)
    {
        if (resolved[index])
            continue;
        const RulesetDerivedStatDefinition &stat = _definition.derivedStats[index];
        appendEvaluationErrors(snapshot, "derivedStats." + stat.id, evaluateRulesetFormula(stat.formula, variables));
    }

    for (const RulesetDefenseDefinition &defense : _definition.defenses)
    {
        const RulesetFormulaEvaluationResult result = evaluateRulesetFormula(defense.formula, variables);
        if (result.valid)
            snapshot.defenses[defense.id] = result.value;
        else
            appendEvaluationErrors(snapshot, "defenses." + defense.id, result);
    }

    for (const RulesetResourceDefinition &resource : _definition.resources)
    {
        const RulesetFormulaEvaluationResult result = evaluateRulesetFormula(resource.maximumFormula, variables);
        if (result.valid)
            snapshot.resourceMaximums[resource.id] = std::max(0.0, result.value);
        else
            appendEvaluationErrors(snapshot, "resources." + resource.id, result);
    }

    for (const RulesetValidationDiagnostic &diagnostic : validateRulesetDefinition(_definition))
    {
        if (diagnostic.severity != RulesetDiagnosticSeverity::Error)
            continue;
        snapshot.valid = false;
        snapshot.errors.push_back(diagnostic.path + ": " + diagnostic.message);
    }
    return snapshot;
}

void migrateLegacyCharacterRuleValues(CharacterModel &character,
                                      const RulesetDefinitionModel &definition)
{
    for (const RulesetAbilityScoreDefinition &ability : definition.abilityScores)
    {
        if (ability.id.empty() || character.ruleValues.find(ability.id) != character.ruleValues.end())
            continue;

        auto legacy = character.attributes.find(ability.name);
        if (legacy == character.attributes.end())
            legacy = character.attributes.find(ability.id);
        if (legacy == character.attributes.end() && !ability.abbreviation.empty())
            legacy = character.attributes.find(ability.abbreviation);
        if (legacy != character.attributes.end())
            character.ruleValues[ability.id] = legacy->second;
    }
}
