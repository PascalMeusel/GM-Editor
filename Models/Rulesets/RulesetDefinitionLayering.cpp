#include "RulesetDefinitionLayering.hpp"

#include <algorithm>

namespace
{
template <typename T>
void mergeDefinitionsById(std::vector<T> &baseValues, const std::vector<T> &overrideValues)
{
    for (const T &overrideValue : overrideValues)
    {
        auto found = std::find_if(baseValues.begin(), baseValues.end(), [&](const T &baseValue) {
            return baseValue.id == overrideValue.id;
        });

        if (found == baseValues.end())
            baseValues.push_back(overrideValue);
        else
            *found = overrideValue;
    }
}
}

RulesetDefinitionModel createLayeredRulesetDefinition(const RulesetDefinitionModel &baseDefinition, const RulesetDefinitionModel &overrideDefinition)
{
    RulesetDefinitionModel layered = baseDefinition;
    layered.metadata.id = overrideDefinition.metadata.id.empty() ? baseDefinition.metadata.id : overrideDefinition.metadata.id;
    layered.rulesetId = baseDefinition.rulesetId.empty() ? baseDefinition.metadata.id : baseDefinition.rulesetId;
    layered.name = overrideDefinition.name.empty() ? baseDefinition.name : overrideDefinition.name;
    layered.version = overrideDefinition.version.empty() ? baseDefinition.version : overrideDefinition.version;
    layered.baseRulesetId = overrideDefinition.baseRulesetId.empty() ? baseDefinition.metadata.id : overrideDefinition.baseRulesetId;
    layered.installedGlobally = false;
    if (!overrideDefinition.abilityModifierFormula.empty())
        layered.abilityModifierFormula = overrideDefinition.abilityModifierFormula;
    if (!overrideDefinition.proficiencyFormula.empty())
        layered.proficiencyFormula = overrideDefinition.proficiencyFormula;
    if (!overrideDefinition.checkDiceFormula.empty())
        layered.checkDiceFormula = overrideDefinition.checkDiceFormula;
    layered.passiveScoreBase = overrideDefinition.passiveScoreBase;
    layered.published = baseDefinition.published;
    layered.publishedAtUtc = baseDefinition.publishedAtUtc;

    mergeDefinitionsById(layered.abilityScores, overrideDefinition.abilityScores);
    mergeDefinitionsById(layered.derivedStats, overrideDefinition.derivedStats);
    mergeDefinitionsById(layered.skills, overrideDefinition.skills);
    mergeDefinitionsById(layered.defenses, overrideDefinition.defenses);
    mergeDefinitionsById(layered.resources, overrideDefinition.resources);
    mergeDefinitionsById(layered.diceFormulas, overrideDefinition.diceFormulas);
    mergeDefinitionsById(layered.currencies, overrideDefinition.currencies);
    mergeDefinitionsById(layered.sizes, overrideDefinition.sizes);
    mergeDefinitionsById(layered.speeds, overrideDefinition.speeds);
    mergeDefinitionsById(layered.conditions, overrideDefinition.conditions);
    mergeDefinitionsById(layered.classes, overrideDefinition.classes);
    mergeDefinitionsById(layered.subclasses, overrideDefinition.subclasses);
    mergeDefinitionsById(layered.ancestries, overrideDefinition.ancestries);
    mergeDefinitionsById(layered.backgrounds, overrideDefinition.backgrounds);
    mergeDefinitionsById(layered.features, overrideDefinition.features);
    mergeDefinitionsById(layered.equipmentCategories, overrideDefinition.equipmentCategories);
    mergeDefinitionsById(layered.spellTraditions, overrideDefinition.spellTraditions);
    mergeDefinitionsById(layered.monsterTemplates, overrideDefinition.monsterTemplates);

    return layered;
}
