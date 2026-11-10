#include "RulesetDefinitionSerialization.hpp"

#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>

namespace
{
struct JsonValue
{
    enum class Type
    {
        Null,
        String,
        Number,
        Array,
        Object
    };

    Type type = Type::Null;
    std::string stringValue;
    int numberValue = 0;
    std::vector<JsonValue> arrayValue;
    std::map<std::string, JsonValue> objectValue;
};

class JsonParser
{
public:
    explicit JsonParser(const std::string &text) : _text(text) {}

    JsonValue parse()
    {
        skipWhitespace();
        JsonValue value = parseValue();
        skipWhitespace();
        return value;
    }

private:
    const std::string &_text;
    std::size_t _position = 0;

    void skipWhitespace()
    {
        while (_position < _text.size() && std::isspace(static_cast<unsigned char>(_text[_position])))
            ++_position;
    }

    char peek() const
    {
        return _position < _text.size() ? _text[_position] : '\0';
    }

    char consume()
    {
        if (_position >= _text.size())
            throw std::runtime_error("Unexpected end of JSON.");
        return _text[_position++];
    }

    void expect(char expected)
    {
        if (consume() != expected)
            throw std::runtime_error("Unexpected JSON token.");
    }

    JsonValue parseValue()
    {
        skipWhitespace();
        const char token = peek();
        if (token == '{')
            return parseObject();
        if (token == '[')
            return parseArray();
        if (token == '"')
            return JsonValue{JsonValue::Type::String, parseString()};
        if (token == '-' || std::isdigit(static_cast<unsigned char>(token)))
            return parseNumber();
        if (_text.compare(_position, 4, "null") == 0)
        {
            _position += 4;
            return JsonValue{};
        }
        throw std::runtime_error("Unsupported JSON value.");
    }

    JsonValue parseObject()
    {
        JsonValue value;
        value.type = JsonValue::Type::Object;
        expect('{');
        skipWhitespace();
        if (peek() == '}')
        {
            consume();
            return value;
        }

        while (true)
        {
            skipWhitespace();
            const std::string key = parseString();
            skipWhitespace();
            expect(':');
            value.objectValue[key] = parseValue();
            skipWhitespace();
            const char separator = consume();
            if (separator == '}')
                break;
            if (separator != ',')
                throw std::runtime_error("Expected JSON object separator.");
        }
        return value;
    }

    JsonValue parseArray()
    {
        JsonValue value;
        value.type = JsonValue::Type::Array;
        expect('[');
        skipWhitespace();
        if (peek() == ']')
        {
            consume();
            return value;
        }

        while (true)
        {
            value.arrayValue.push_back(parseValue());
            skipWhitespace();
            const char separator = consume();
            if (separator == ']')
                break;
            if (separator != ',')
                throw std::runtime_error("Expected JSON array separator.");
        }
        return value;
    }

    JsonValue parseNumber()
    {
        const std::size_t start = _position;
        if (peek() == '-')
            consume();
        while (std::isdigit(static_cast<unsigned char>(peek())))
            consume();

        JsonValue value;
        value.type = JsonValue::Type::Number;
        value.numberValue = std::stoi(_text.substr(start, _position - start));
        return value;
    }

    std::string parseString()
    {
        expect('"');
        std::string value;
        while (true)
        {
            const char character = consume();
            if (character == '"')
                break;
            if (character != '\\')
            {
                value.push_back(character);
                continue;
            }

            const char escaped = consume();
            switch (escaped)
            {
            case '"':
            case '\\':
            case '/':
                value.push_back(escaped);
                break;
            case 'n':
                value.push_back('\n');
                break;
            case 'r':
                value.push_back('\r');
                break;
            case 't':
                value.push_back('\t');
                break;
            default:
                throw std::runtime_error("Unsupported JSON string escape.");
            }
        }
        return value;
    }
};

const JsonValue *findObjectValue(const JsonValue &object, const std::string &key)
{
    if (object.type != JsonValue::Type::Object)
        return nullptr;
    const auto found = object.objectValue.find(key);
    return found == object.objectValue.end() ? nullptr : &found->second;
}

std::string stringMember(const JsonValue &object, const std::string &key)
{
    const JsonValue *value = findObjectValue(object, key);
    return value && value->type == JsonValue::Type::String ? value->stringValue : "";
}

int intMember(const JsonValue &object, const std::string &key, int fallback)
{
    const JsonValue *value = findObjectValue(object, key);
    return value && value->type == JsonValue::Type::Number ? value->numberValue : fallback;
}

std::vector<std::string> stringArrayMember(const JsonValue &object, const std::string &key)
{
    std::vector<std::string> values;
    const JsonValue *array = findObjectValue(object, key);
    if (!array || array->type != JsonValue::Type::Array)
        return values;

    for (const JsonValue &value : array->arrayValue)
    {
        if (value.type == JsonValue::Type::String)
            values.push_back(value.stringValue);
    }
    return values;
}

template <typename T, typename Reader>
void readObjectArray(const JsonValue &root, const std::string &key, std::vector<T> &target, Reader reader)
{
    const JsonValue *array = findObjectValue(root, key);
    if (!array || array->type != JsonValue::Type::Array)
        return;

    target.clear();
    for (const JsonValue &value : array->arrayValue)
    {
        if (value.type == JsonValue::Type::Object)
            target.push_back(reader(value));
    }
}
}

namespace RulesetDefinitionSerialization
{
void hydrateDefinitionContent(RulesetDefinitionModel &definition)
{
    if (definition.definitionJson.empty())
        return;

    try
    {
        const JsonValue root = JsonParser(definition.definitionJson).parse();
        definition.schemaVersion = intMember(root, "schemaVersion", definition.schemaVersion);
        definition.rulesetId = stringMember(root, "rulesetId");
        if (definition.rulesetId.empty())
            definition.rulesetId = definition.metadata.id;
        const std::string abilityModifierFormula = stringMember(root, "abilityModifierFormula");
        const std::string proficiencyFormula = stringMember(root, "proficiencyFormula");
        const std::string checkDiceFormula = stringMember(root, "checkDiceFormula");
        if (!abilityModifierFormula.empty())
            definition.abilityModifierFormula = abilityModifierFormula;
        if (!proficiencyFormula.empty())
            definition.proficiencyFormula = proficiencyFormula;
        if (!checkDiceFormula.empty())
            definition.checkDiceFormula = checkDiceFormula;
        definition.passiveScoreBase = intMember(root, "passiveScoreBase", definition.passiveScoreBase);
        definition.published = intMember(root, "published", definition.published ? 1 : 0) != 0;
        definition.publishedAtUtc = stringMember(root, "publishedAtUtc");
        readObjectArray(root, "abilityScores", definition.abilityScores, [](const JsonValue &value) {
            return RulesetAbilityScoreDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "abbreviation"), stringMember(value, "description")};
        });
        readObjectArray(root, "derivedStats", definition.derivedStats, [](const JsonValue &value) {
            return RulesetDerivedStatDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "formula"), stringMember(value, "description")};
        });
        readObjectArray(root, "skills", definition.skills, [](const JsonValue &value) {
            return RulesetSkillDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "abilityScoreId"), stringMember(value, "description")};
        });
        readObjectArray(root, "defenses", definition.defenses, [](const JsonValue &value) {
            return RulesetDefenseDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "formula"), stringMember(value, "description")};
        });
        readObjectArray(root, "resources", definition.resources, [](const JsonValue &value) {
            RulesetResourceDefinition resource;
            resource.id = stringMember(value, "id");
            resource.name = stringMember(value, "name");
            resource.maximumFormula = stringMember(value, "maximumFormula");
            resource.recoveryFormula = stringMember(value, "recoveryFormula");
            resource.recoveryTrigger = stringMember(value, "recoveryTrigger");
            if (resource.recoveryTrigger.empty())
                resource.recoveryTrigger = "Long Rest";
            resource.description = stringMember(value, "description");
            return resource;
        });
        readObjectArray(root, "diceFormulas", definition.diceFormulas, [](const JsonValue &value) {
            return RulesetDiceFormulaDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "formula"), stringMember(value, "description")};
        });
        readObjectArray(root, "currencies", definition.currencies, [](const JsonValue &value) {
            return RulesetCurrencyDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "code"), intMember(value, "baseUnitValue", 1), stringMember(value, "description")};
        });
        readObjectArray(root, "sizes", definition.sizes, [](const JsonValue &value) {
            return RulesetSizeDefinition{stringMember(value, "id"), stringMember(value, "name"), intMember(value, "spaceFeet", 5), stringMember(value, "description")};
        });
        readObjectArray(root, "speeds", definition.speeds, [](const JsonValue &value) {
            RulesetSpeedDefinition speed{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "unit"), stringMember(value, "description")};
            if (speed.unit.empty())
                speed.unit = "feet";
            return speed;
        });
        readObjectArray(root, "conditions", definition.conditions, [](const JsonValue &value) {
            RulesetConditionDefinition condition;
            condition.id = stringMember(value, "id");
            condition.name = stringMember(value, "name");
            condition.description = stringMember(value, "description");
            condition.mechanicalEffects = stringArrayMember(value, "mechanicalEffects");
            condition.endCondition = stringMember(value, "endCondition");
            return condition;
        });
        readObjectArray(root, "classes", definition.classes, [](const JsonValue &value) {
            RulesetClassDefinition classDefinition;
            classDefinition.id = stringMember(value, "id");
            classDefinition.name = stringMember(value, "name");
            classDefinition.hitDie = stringMember(value, "hitDie");
            classDefinition.primaryAbilityScoreIds = stringArrayMember(value, "primaryAbilityScoreIds");
            classDefinition.description = stringMember(value, "description");
            return classDefinition;
        });
        readObjectArray(root, "subclasses", definition.subclasses, [](const JsonValue &value) {
            return RulesetSubclassDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "parentClassId"), stringMember(value, "description")};
        });
        readObjectArray(root, "ancestries", definition.ancestries, [](const JsonValue &value) {
            return RulesetAncestryDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "sizeId"), stringMember(value, "speedId"), stringMember(value, "description")};
        });
        readObjectArray(root, "backgrounds", definition.backgrounds, [](const JsonValue &value) {
            RulesetBackgroundDefinition background;
            background.id = stringMember(value, "id");
            background.name = stringMember(value, "name");
            background.skillIds = stringArrayMember(value, "skillIds");
            background.description = stringMember(value, "description");
            return background;
        });
        readObjectArray(root, "features", definition.features, [](const JsonValue &value) {
            return RulesetFeatureDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "featureType"), intMember(value, "level", 1), stringMember(value, "description")};
        });
        readObjectArray(root, "equipmentCategories", definition.equipmentCategories, [](const JsonValue &value) {
            return RulesetEquipmentCategoryDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "slotType"), stringMember(value, "description")};
        });
        readObjectArray(root, "spellTraditions", definition.spellTraditions, [](const JsonValue &value) {
            return RulesetSpellTraditionDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "abilityScoreId"), stringMember(value, "description")};
        });
        readObjectArray(root, "monsterTemplates", definition.monsterTemplates, [](const JsonValue &value) {
            return RulesetMonsterTemplateDefinition{stringMember(value, "id"), stringMember(value, "name"), stringMember(value, "sizeId"), stringMember(value, "challengeFormula"), stringMember(value, "description")};
        });
    }
    catch (const std::exception &)
    {
        // Keep raw definitionJson for imports authored outside this app.
    }
}
}

