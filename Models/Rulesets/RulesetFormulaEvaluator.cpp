#include "RulesetFormulaEvaluator.hpp"

#include <cctype>
#include <cmath>
#include <sstream>

namespace
{
class FormulaParser
{
public:
    FormulaParser(const std::string &formula, const std::map<std::string, double> &variables)
        : _formula(formula), _variables(variables)
    {
    }

    RulesetFormulaEvaluationResult evaluate()
    {
        RulesetFormulaEvaluationResult result;
        const double value = parseExpression();
        skipWhitespace();
        if (_position < _formula.size())
            _errors.push_back("Unexpected token at position " + std::to_string(_position) + ".");

        result.valid = _errors.empty();
        result.value = result.valid ? value : 0.0;
        result.errors = _errors;
        return result;
    }

private:
    const std::string &_formula;
    const std::map<std::string, double> &_variables;
    std::vector<std::string> _errors;
    std::size_t _position = 0;

    void skipWhitespace()
    {
        while (_position < _formula.size() && std::isspace(static_cast<unsigned char>(_formula[_position])))
            ++_position;
    }

    bool match(char token)
    {
        skipWhitespace();
        if (_position < _formula.size() && _formula[_position] == token)
        {
            ++_position;
            return true;
        }
        return false;
    }

    char peek()
    {
        skipWhitespace();
        return _position < _formula.size() ? _formula[_position] : '\0';
    }

    double parseExpression()
    {
        double value = parseTerm();
        while (true)
        {
            if (match('+'))
                value += parseTerm();
            else if (match('-'))
                value -= parseTerm();
            else
                break;
        }
        return value;
    }

    double parseTerm()
    {
        double value = parseFactor();
        while (true)
        {
            if (match('*'))
            {
                value *= parseFactor();
            }
            else if (match('/'))
            {
                const double divisor = parseFactor();
                if (std::abs(divisor) < 0.000001)
                {
                    _errors.push_back("Division by zero.");
                    value = 0.0;
                }
                else
                {
                    value /= divisor;
                }
            }
            else
            {
                break;
            }
        }
        return value;
    }

    double parseFactor()
    {
        if (match('+'))
            return parseFactor();
        if (match('-'))
            return -parseFactor();
        if (match('('))
        {
            const double value = parseExpression();
            if (!match(')'))
                _errors.push_back("Missing closing parenthesis.");
            return value;
        }

        if (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.')
            return parseNumber();

        if (std::isalpha(static_cast<unsigned char>(peek())) || peek() == '_')
            return parseVariable();

        _errors.push_back("Expected number, variable, or parenthesized expression at position " + std::to_string(_position) + ".");
        if (_position < _formula.size())
            ++_position;
        return 0.0;
    }

    double parseNumber()
    {
        const std::size_t start = _position;
        bool sawDecimal = false;
        while (_position < _formula.size())
        {
            const char character = _formula[_position];
            if (character == '.')
            {
                if (sawDecimal)
                    break;
                sawDecimal = true;
                ++_position;
                continue;
            }
            if (!std::isdigit(static_cast<unsigned char>(character)))
                break;
            ++_position;
        }
        return std::stod(_formula.substr(start, _position - start));
    }

    double parseVariable()
    {
        const std::size_t start = _position;
        while (_position < _formula.size())
        {
            const char character = _formula[_position];
            if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_' && character != '.')
                break;
            ++_position;
        }

        const std::string name = _formula.substr(start, _position - start);
        const auto found = _variables.find(name);
        if (found == _variables.end())
        {
            _errors.push_back("Unknown formula variable: " + name + ".");
            return 0.0;
        }
        return found->second;
    }
};
}

RulesetFormulaEvaluationResult evaluateRulesetFormula(const std::string &formula, const std::map<std::string, double> &variables)
{
    if (formula.empty())
        return RulesetFormulaEvaluationResult{false, 0.0, {"Formula is empty."}};
    return FormulaParser(formula, variables).evaluate();
}

std::map<std::string, double> buildRulesetFormulaPreviewVariables(const RulesetDefinitionModel &definition)
{
    std::map<std::string, double> variables{
        {"level", 5.0},
        {"proficiency", 3.0},
        {"party_level", 5.0},
        {"shield", 2.0},
        {"hit_die_average", 5.5}};

    for (const RulesetAbilityScoreDefinition &ability : definition.abilityScores)
    {
        if (ability.id.empty())
            continue;
        variables[ability.id] = 10.0;
        variables[ability.id + ".mod"] = 0.0;
    }

    for (const RulesetDerivedStatDefinition &stat : definition.derivedStats)
    {
        if (stat.id.empty())
            continue;
        const RulesetFormulaEvaluationResult result = evaluateRulesetFormula(stat.formula, variables);
        variables[stat.id] = result.valid ? result.value : 0.0;
    }

    for (const RulesetResourceDefinition &resource : definition.resources)
    {
        if (!resource.id.empty())
            variables[resource.id] = 0.0;
    }

    return variables;
}

RulesetFormulaEvaluationResult previewRulesetFormula(const RulesetDefinitionModel &definition, const std::string &formula)
{
    return evaluateRulesetFormula(formula, buildRulesetFormulaPreviewVariables(definition));
}
