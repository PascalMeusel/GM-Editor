#include "RulesetDefinitionSerialization.hpp"

#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>

namespace
{
std::string escapeJson(const std::string &value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (char character : value)
    {
        switch (character)
        {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    return escaped;
}

void appendStringField(std::ostringstream &stream, const std::string &name, const std::string &value, bool &firstField)
{
    if (!firstField)
        stream << ",";
    firstField = false;
    stream << "\"" << name << "\":\"" << escapeJson(value) << "\"";
}

void appendIntField(std::ostringstream &stream, const std::string &name, int value, bool &firstField)
{
    if (!firstField)
        stream << ",";
    firstField = false;
    stream << "\"" << name << "\":" << value;
}

void appendStringArrayField(std::ostringstream &stream, const std::string &name, const std::vector<std::string> &values, bool &firstField)
{
    if (!firstField)
        stream << ",";
    firstField = false;
    stream << "\"" << name << "\":[";
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index > 0)
            stream << ",";
        stream << "\"" << escapeJson(values[index]) << "\"";
    }
    stream << "]";
}

template <typename T, typename Writer>
void appendObjectArray(std::ostringstream &stream, const std::string &name, const std::vector<T> &values, Writer writer, bool &firstField)
{
    if (!firstField)
        stream << ",";
    firstField = false;
    stream << "\"" << name << "\":[";
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index > 0)
            stream << ",";
        stream << "{";
        bool firstObjectField = true;
        writer(stream, values[index], firstObjectField);
        stream << "}";
    }
    stream << "]";
}
}
namespace RulesetDefinitionSerialization
{
bool hasStructuredDefinitions(const RulesetDefinitionModel &definition)
{
    return !definition.rulesetId.empty() || definition.published ||
           !definition.abilityScores.empty() ||
           !definition.derivedStats.empty() ||
           !definition.skills.empty() ||
           !definition.defenses.empty() ||
           !definition.resources.empty() ||
           !definition.diceFormulas.empty() ||
           !definition.currencies.empty() ||
           !definition.sizes.empty() ||
           !definition.speeds.empty() ||
           !definition.conditions.empty() ||
           !definition.classes.empty() ||
           !definition.subclasses.empty() ||
           !definition.ancestries.empty() ||
           !definition.backgrounds.empty() ||
           !definition.features.empty() ||
           !definition.equipmentCategories.empty() ||
           !definition.spellTraditions.empty() ||
           !definition.monsterTemplates.empty();
}

std::string serializeDefinitionContent(const RulesetDefinitionModel &definition)
{
    if (!hasStructuredDefinitions(definition))
        return definition.definitionJson;

    std::ostringstream stream;
    stream << "{";
    bool firstField = true;
    appendIntField(stream, "schemaVersion", definition.schemaVersion, firstField);
    appendStringField(stream, "rulesetId", definition.rulesetId, firstField);
    appendStringField(stream, "abilityModifierFormula", definition.abilityModifierFormula, firstField);
    appendStringField(stream, "proficiencyFormula", definition.proficiencyFormula, firstField);
    appendStringField(stream, "checkDiceFormula", definition.checkDiceFormula, firstField);
    appendIntField(stream, "passiveScoreBase", definition.passiveScoreBase, firstField);
    appendIntField(stream, "published", definition.published ? 1 : 0, firstField);
    appendStringField(stream, "publishedAtUtc", definition.publishedAtUtc, firstField);
    appendObjectArray(stream, "abilityScores", definition.abilityScores, [](std::ostringstream &object, const RulesetAbilityScoreDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "abbreviation", value.abbreviation, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "derivedStats", definition.derivedStats, [](std::ostringstream &object, const RulesetDerivedStatDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "formula", value.formula, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "skills", definition.skills, [](std::ostringstream &object, const RulesetSkillDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "abilityScoreId", value.abilityScoreId, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "defenses", definition.defenses, [](std::ostringstream &object, const RulesetDefenseDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "formula", value.formula, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "resources", definition.resources, [](std::ostringstream &object, const RulesetResourceDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "maximumFormula", value.maximumFormula, firstObjectField);
        appendStringField(object, "recoveryFormula", value.recoveryFormula, firstObjectField);
        appendStringField(object, "recoveryTrigger", value.recoveryTrigger, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "diceFormulas", definition.diceFormulas, [](std::ostringstream &object, const RulesetDiceFormulaDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "formula", value.formula, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "currencies", definition.currencies, [](std::ostringstream &object, const RulesetCurrencyDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "code", value.code, firstObjectField);
        appendIntField(object, "baseUnitValue", value.baseUnitValue, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "sizes", definition.sizes, [](std::ostringstream &object, const RulesetSizeDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendIntField(object, "spaceFeet", value.spaceFeet, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "speeds", definition.speeds, [](std::ostringstream &object, const RulesetSpeedDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "unit", value.unit, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "conditions", definition.conditions, [](std::ostringstream &object, const RulesetConditionDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
        appendStringArrayField(object, "mechanicalEffects", value.mechanicalEffects, firstObjectField);
        appendStringField(object, "endCondition", value.endCondition, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "classes", definition.classes, [](std::ostringstream &object, const RulesetClassDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "hitDie", value.hitDie, firstObjectField);
        appendStringArrayField(object, "primaryAbilityScoreIds", value.primaryAbilityScoreIds, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "subclasses", definition.subclasses, [](std::ostringstream &object, const RulesetSubclassDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "parentClassId", value.parentClassId, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "ancestries", definition.ancestries, [](std::ostringstream &object, const RulesetAncestryDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "sizeId", value.sizeId, firstObjectField);
        appendStringField(object, "speedId", value.speedId, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "backgrounds", definition.backgrounds, [](std::ostringstream &object, const RulesetBackgroundDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringArrayField(object, "skillIds", value.skillIds, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "features", definition.features, [](std::ostringstream &object, const RulesetFeatureDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "featureType", value.featureType, firstObjectField);
        appendIntField(object, "level", value.level, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "equipmentCategories", definition.equipmentCategories, [](std::ostringstream &object, const RulesetEquipmentCategoryDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "slotType", value.slotType, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "spellTraditions", definition.spellTraditions, [](std::ostringstream &object, const RulesetSpellTraditionDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "abilityScoreId", value.abilityScoreId, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    appendObjectArray(stream, "monsterTemplates", definition.monsterTemplates, [](std::ostringstream &object, const RulesetMonsterTemplateDefinition &value, bool &firstObjectField) {
        appendStringField(object, "id", value.id, firstObjectField);
        appendStringField(object, "name", value.name, firstObjectField);
        appendStringField(object, "sizeId", value.sizeId, firstObjectField);
        appendStringField(object, "challengeFormula", value.challengeFormula, firstObjectField);
        appendStringField(object, "description", value.description, firstObjectField);
    }, firstField);
    stream << "}";
    return stream.str();
}
}
