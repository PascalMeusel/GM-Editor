#include "RulesetDefinitionValidator.hpp"

#include "Models/Rulesets/RulesetFormulaEvaluator.hpp"

#include <cctype>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <utility>

namespace
{
void addDiagnostic(std::vector<RulesetValidationDiagnostic> &diagnostics,
                   const std::string &code,
                   const std::string &path,
                   const std::string &message,
                   RulesetDiagnosticMessageKey messageKey = RulesetDiagnosticMessageKey::Legacy,
                   std::vector<std::string> arguments = {})
{
    diagnostics.push_back(RulesetValidationDiagnostic{
        RulesetDiagnosticSeverity::Error, code, path, message, messageKey, std::move(arguments)});
}

template <typename T>
void collectIds(const std::vector<T> &values, std::set<std::string> &ids)
{
    for (const T &value : values)
    {
        if (!value.id.empty())
            ids.insert(value.id);
    }
}

template <typename T>
void checkDuplicateIds(const std::vector<T> &values, const std::string &path, std::vector<RulesetValidationDiagnostic> &diagnostics)
{
    std::set<std::string> seen;
    for (const T &value : values)
    {
        if (value.id.empty())
        {
            addDiagnostic(diagnostics, "missing_id", path, "Definition is missing an id.",
                          RulesetDiagnosticMessageKey::MissingId);
            continue;
        }
        if (!seen.insert(value.id).second)
            addDiagnostic(diagnostics, "duplicate_id", path + "." + value.id, "Duplicate id: " + value.id + ".",
                          RulesetDiagnosticMessageKey::DuplicateId, {value.id});
    }
}

void requireReference(const std::set<std::string> &ids, const std::string &reference, const std::string &path, const std::string &label, std::vector<RulesetValidationDiagnostic> &diagnostics)
{
    if (!reference.empty() && ids.find(reference) == ids.end())
        addDiagnostic(diagnostics, "missing_reference", path, "Missing " + label + " reference: " + reference + ".",
                      RulesetDiagnosticMessageKey::MissingReference, {label, reference});
}

std::set<std::string> extractFormulaIdentifiers(const std::string &formula)
{
    std::set<std::string> identifiers;
    for (std::size_t index = 0; index < formula.size();)
    {
        const char character = formula[index];
        if (!std::isalpha(static_cast<unsigned char>(character)) && character != '_')
        {
            ++index;
            continue;
        }

        const std::size_t start = index;
        while (index < formula.size())
        {
            const char token = formula[index];
            if (!std::isalnum(static_cast<unsigned char>(token)) && token != '_' && token != '.')
                break;
            ++index;
        }
        identifiers.insert(formula.substr(start, index - start));
    }
    return identifiers;
}

void validateFormula(const std::string &formula,
                     const std::string &path,
                     const std::set<std::string> &knownFormulaSymbols,
                     std::vector<RulesetValidationDiagnostic> &diagnostics)
{
    if (formula.empty())
        return;

    const std::set<std::string> identifiers = extractFormulaIdentifiers(formula);
    std::map<std::string, double> variables;
    for (const std::string &identifier : identifiers)
    {
        if (knownFormulaSymbols.find(identifier) == knownFormulaSymbols.end())
            addDiagnostic(diagnostics, "missing_reference", path, "Unknown formula symbol: " + identifier + ".",
                          RulesetDiagnosticMessageKey::UnknownFormulaSymbol, {identifier});
        variables[identifier] = 1.0;
    }

    const RulesetFormulaEvaluationResult result = evaluateRulesetFormula(formula, variables);
    if (!result.valid)
    {
        for (const std::string &error : result.errors)
            addDiagnostic(diagnostics, "broken_formula", path, error,
                          RulesetDiagnosticMessageKey::BrokenFormula, {error});
    }
}

bool isValidDiceFormula(const std::string &formula)
{
    static const std::regex diceRegex(R"(^\s*\d*d\d+(\s*[+-]\s*(\d+|[A-Za-z_][A-Za-z0-9_.]*))*\s*$)");
    return std::regex_match(formula, diceRegex);
}

void detectDerivedStatCycles(const RulesetDefinitionModel &definition, std::vector<RulesetValidationDiagnostic> &diagnostics)
{
    std::set<std::string> derivedIds;
    collectIds(definition.derivedStats, derivedIds);

    std::map<std::string, std::set<std::string>> graph;
    for (const RulesetDerivedStatDefinition &stat : definition.derivedStats)
    {
        for (const std::string &identifier : extractFormulaIdentifiers(stat.formula))
        {
            if (derivedIds.find(identifier) != derivedIds.end())
                graph[stat.id].insert(identifier);
        }
    }

    std::set<std::string> visiting;
    std::set<std::string> visited;
    std::function<bool(const std::string &)> visit = [&](const std::string &id) {
        if (visiting.find(id) != visiting.end())
            return true;
        if (visited.find(id) != visited.end())
            return false;

        visiting.insert(id);
        for (const std::string &next : graph[id])
        {
            if (visit(next))
                return true;
        }
        visiting.erase(id);
        visited.insert(id);
        return false;
    };

    for (const RulesetDerivedStatDefinition &stat : definition.derivedStats)
    {
        visiting.clear();
        if (visit(stat.id))
            addDiagnostic(diagnostics, "circular_dependency", "derivedStats." + stat.id, "Derived stat participates in a circular formula dependency.",
                          RulesetDiagnosticMessageKey::CircularDependency);
    }
}
}

std::vector<RulesetValidationDiagnostic> validateRulesetDefinition(const RulesetDefinitionModel &definition)
{
    std::vector<RulesetValidationDiagnostic> diagnostics;

    validateFormula(definition.abilityModifierFormula,
                    "runtime.abilityModifierFormula",
                    {"score"},
                    diagnostics);
    validateFormula(definition.proficiencyFormula,
                    "runtime.proficiencyFormula",
                    {"level"},
                    diagnostics);
    if (!definition.checkDiceFormula.empty() && !isValidDiceFormula(definition.checkDiceFormula))
        addDiagnostic(diagnostics, "invalid_dice", "runtime.checkDiceFormula", "Invalid check dice formula: " + definition.checkDiceFormula + ".",
                      RulesetDiagnosticMessageKey::InvalidCheckDiceFormula, {definition.checkDiceFormula});
    if (definition.published && (definition.rulesetId.empty() || definition.version.empty()))
        addDiagnostic(diagnostics, "missing_publish_identity", "overview", "Published versions require a ruleset ID and version.",
                      RulesetDiagnosticMessageKey::MissingPublishIdentity);

    checkDuplicateIds(definition.abilityScores, "abilityScores", diagnostics);
    checkDuplicateIds(definition.derivedStats, "derivedStats", diagnostics);
    checkDuplicateIds(definition.skills, "skills", diagnostics);
    checkDuplicateIds(definition.defenses, "defenses", diagnostics);
    checkDuplicateIds(definition.resources, "resources", diagnostics);
    checkDuplicateIds(definition.diceFormulas, "diceFormulas", diagnostics);
    checkDuplicateIds(definition.currencies, "currencies", diagnostics);
    checkDuplicateIds(definition.sizes, "sizes", diagnostics);
    checkDuplicateIds(definition.speeds, "speeds", diagnostics);
    checkDuplicateIds(definition.conditions, "conditions", diagnostics);
    checkDuplicateIds(definition.classes, "classes", diagnostics);
    checkDuplicateIds(definition.subclasses, "subclasses", diagnostics);
    checkDuplicateIds(definition.ancestries, "ancestries", diagnostics);
    checkDuplicateIds(definition.backgrounds, "backgrounds", diagnostics);
    checkDuplicateIds(definition.features, "features", diagnostics);
    checkDuplicateIds(definition.equipmentCategories, "equipmentCategories", diagnostics);
    checkDuplicateIds(definition.spellTraditions, "spellTraditions", diagnostics);
    checkDuplicateIds(definition.monsterTemplates, "monsterTemplates", diagnostics);

    std::set<std::string> abilityIds;
    std::set<std::string> skillIds;
    std::set<std::string> classIds;
    std::set<std::string> sizeIds;
    std::set<std::string> speedIds;
    std::set<std::string> knownFormulaSymbols{"level", "proficiency", "party_level", "shield", "hit_die_average"};
    collectIds(definition.abilityScores, abilityIds);
    collectIds(definition.skills, skillIds);
    collectIds(definition.classes, classIds);
    collectIds(definition.sizes, sizeIds);
    collectIds(definition.speeds, speedIds);
    collectIds(definition.derivedStats, knownFormulaSymbols);
    collectIds(definition.resources, knownFormulaSymbols);
    for (const std::string &abilityId : abilityIds)
    {
        knownFormulaSymbols.insert(abilityId);
        knownFormulaSymbols.insert(abilityId + ".mod");
    }

    for (const RulesetSkillDefinition &skill : definition.skills)
        requireReference(abilityIds, skill.abilityScoreId, "skills." + skill.id, "ability score", diagnostics);
    for (const RulesetClassDefinition &classDefinition : definition.classes)
    {
        for (const std::string &abilityId : classDefinition.primaryAbilityScoreIds)
            requireReference(abilityIds, abilityId, "classes." + classDefinition.id, "ability score", diagnostics);
    }
    for (const RulesetSubclassDefinition &subclass : definition.subclasses)
        requireReference(classIds, subclass.parentClassId, "subclasses." + subclass.id, "class", diagnostics);
    for (const RulesetAncestryDefinition &ancestry : definition.ancestries)
    {
        requireReference(sizeIds, ancestry.sizeId, "ancestries." + ancestry.id, "size", diagnostics);
        requireReference(speedIds, ancestry.speedId, "ancestries." + ancestry.id, "speed", diagnostics);
    }
    for (const RulesetBackgroundDefinition &background : definition.backgrounds)
    {
        for (const std::string &skillId : background.skillIds)
            requireReference(skillIds, skillId, "backgrounds." + background.id, "skill", diagnostics);
    }
    for (const RulesetSpellTraditionDefinition &tradition : definition.spellTraditions)
        requireReference(abilityIds, tradition.abilityScoreId, "spellTraditions." + tradition.id, "ability score", diagnostics);
    for (const RulesetMonsterTemplateDefinition &monsterTemplate : definition.monsterTemplates)
        requireReference(sizeIds, monsterTemplate.sizeId, "monsterTemplates." + monsterTemplate.id, "size", diagnostics);

    for (const RulesetDerivedStatDefinition &stat : definition.derivedStats)
        validateFormula(stat.formula, "derivedStats." + stat.id + ".formula", knownFormulaSymbols, diagnostics);
    for (const RulesetDefenseDefinition &defense : definition.defenses)
        validateFormula(defense.formula, "defenses." + defense.id + ".formula", knownFormulaSymbols, diagnostics);
    for (const RulesetResourceDefinition &resource : definition.resources)
    {
        validateFormula(resource.maximumFormula, "resources." + resource.id + ".maximumFormula", knownFormulaSymbols, diagnostics);
        validateFormula(resource.recoveryFormula, "resources." + resource.id + ".recoveryFormula", knownFormulaSymbols, diagnostics);
        if (resource.recoveryTrigger.empty())
            addDiagnostic(diagnostics, "missing_recovery_trigger", "resources." + resource.id + ".recoveryTrigger", "Choose a recovery trigger.",
                          RulesetDiagnosticMessageKey::MissingRecoveryTrigger);
    }
    for (const RulesetMonsterTemplateDefinition &monsterTemplate : definition.monsterTemplates)
        validateFormula(monsterTemplate.challengeFormula, "monsterTemplates." + monsterTemplate.id + ".challengeFormula", knownFormulaSymbols, diagnostics);
    for (const RulesetDiceFormulaDefinition &dice : definition.diceFormulas)
    {
        if (!dice.formula.empty() && !isValidDiceFormula(dice.formula))
            addDiagnostic(diagnostics, "invalid_dice", "diceFormulas." + dice.id + ".formula", "Invalid dice string: " + dice.formula + ".",
                          RulesetDiagnosticMessageKey::InvalidDiceString, {dice.formula});
    }

    detectDerivedStatCycles(definition, diagnostics);
    return diagnostics;
}
