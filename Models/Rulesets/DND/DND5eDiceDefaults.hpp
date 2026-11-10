#pragma once



#include "Models/Rulesets/DND/DND5e.hpp"



inline RuleDiceSummary DND5e::summarizeDiceExpression(const std::string &expression) const
{
        RuleDiceSummary summary;
        summary.expression = expression;
        int sign = 1;
        std::size_t cursor = 0;
        bool sawTerm = false;

        while (cursor < expression.size())
        {
            while (cursor < expression.size() && std::isspace(static_cast<unsigned char>(expression[cursor])))
                ++cursor;
            if (cursor >= expression.size())
                break;

            if (expression[cursor] == '+')
            {
                sign = 1;
                ++cursor;
                continue;
            }
            if (expression[cursor] == '-')
            {
                sign = -1;
                ++cursor;
                continue;
            }

            const std::size_t numberStart = cursor;
            while (cursor < expression.size() && std::isdigit(static_cast<unsigned char>(expression[cursor])))
                ++cursor;

            int leadingNumber = 0;
            if (numberStart != cursor)
                leadingNumber = std::stoi(expression.substr(numberStart, cursor - numberStart));

            if (cursor < expression.size() && (expression[cursor] == 'd' || expression[cursor] == 'D'))
            {
                ++cursor;
                const int diceCount = numberStart == cursor - 1 ? 1 : leadingNumber;
                const std::size_t sidesStart = cursor;
                while (cursor < expression.size() && std::isdigit(static_cast<unsigned char>(expression[cursor])))
                    ++cursor;
                if (sidesStart == cursor)
                    return summary;

                const int sides = std::stoi(expression.substr(sidesStart, cursor - sidesStart));
                if (diceCount <= 0 || sides <= 0)
                    return summary;

                sawTerm = true;
                if (sign > 0)
                {
                    summary.minimum += diceCount;
                    summary.maximum += diceCount * sides;
                    summary.average += static_cast<double>(diceCount) * (1.0 + static_cast<double>(sides)) / 2.0;
                }
                else
                {
                    summary.minimum -= diceCount * sides;
                    summary.maximum -= diceCount;
                    summary.average -= static_cast<double>(diceCount) * (1.0 + static_cast<double>(sides)) / 2.0;
                }
                sign = 1;
                continue;
            }

            if (numberStart == cursor)
            {
                while (cursor < expression.size() && !std::isspace(static_cast<unsigned char>(expression[cursor])) && expression[cursor] != '+' && expression[cursor] != '-')
                    ++cursor;
                continue;
            }

            sawTerm = true;
            summary.minimum += sign * leadingNumber;
            summary.maximum += sign * leadingNumber;
            summary.average += static_cast<double>(sign * leadingNumber);
            summary.flatModifier += sign * leadingNumber;
            sign = 1;
        }

        summary.valid = sawTerm;
        if (summary.minimum > summary.maximum)
            std::swap(summary.minimum, summary.maximum);
        return summary;
    }

inline RuleDiceRollResult DND5e::evaluateDiceExpression(const std::string &expression, const std::vector<int> &suppliedRolls) const
{
        RuleDiceRollResult result;
        result.summary.expression = expression;
        result.rolls = suppliedRolls;
        int sign = 1;
        std::size_t cursor = 0;
        std::size_t rollIndex = 0;
        bool sawTerm = false;
        int total = 0;

        while (cursor < expression.size())
        {
            while (cursor < expression.size() && std::isspace(static_cast<unsigned char>(expression[cursor])))
                ++cursor;
            if (cursor >= expression.size())
                break;

            if (expression[cursor] == '+')
            {
                sign = 1;
                ++cursor;
                continue;
            }
            if (expression[cursor] == '-')
            {
                sign = -1;
                ++cursor;
                continue;
            }

            const std::size_t numberStart = cursor;
            while (cursor < expression.size() && std::isdigit(static_cast<unsigned char>(expression[cursor])))
                ++cursor;

            int leadingNumber = 0;
            if (numberStart != cursor)
                leadingNumber = std::stoi(expression.substr(numberStart, cursor - numberStart));

            if (cursor < expression.size() && (expression[cursor] == 'd' || expression[cursor] == 'D'))
            {
                ++cursor;
                const int diceCount = numberStart == cursor - 1 ? 1 : leadingNumber;
                const std::size_t sidesStart = cursor;
                while (cursor < expression.size() && std::isdigit(static_cast<unsigned char>(expression[cursor])))
                    ++cursor;
                if (sidesStart == cursor || diceCount <= 0)
                {
                    result.error = "Invalid dice term.";
                    return result;
                }

                const int sides = std::stoi(expression.substr(sidesStart, cursor - sidesStart));
                for (int i = 0; i < diceCount; ++i)
                {
                    if (rollIndex >= suppliedRolls.size())
                    {
                        result.error = "Not enough supplied dice rolls.";
                        return result;
                    }
                    const int roll = suppliedRolls[rollIndex++];
                    if (roll < 1 || roll > sides)
                    {
                        result.error = "Supplied die roll is outside the die size.";
                        return result;
                    }
                    total += sign * roll;
                }
                sawTerm = true;
                sign = 1;
                continue;
            }

            if (numberStart == cursor)
            {
                while (cursor < expression.size() && !std::isspace(static_cast<unsigned char>(expression[cursor])) && expression[cursor] != '+' && expression[cursor] != '-')
                    ++cursor;
                continue;
            }

            total += sign * leadingNumber;
            result.summary.flatModifier += sign * leadingNumber;
            sawTerm = true;
            sign = 1;
        }

        result.summary = summarizeDiceExpression(expression);
        result.valid = sawTerm && result.summary.valid;
        result.total = total;
        if (!result.valid && result.error.empty())
            result.error = "Invalid dice expression.";
        return result;
    }

inline RuleAreaTemplate DND5e::parseAreaTemplate(const std::string &areaText) const
{
        RuleAreaTemplate area;
        area.rawText = areaText;
        const std::string lower = lowerCopy(areaText);
        const std::vector<std::string> shapes{"cone", "cube", "cylinder", "emanation", "line", "sphere", "wall", "custom"};
        for (const std::string &shape : shapes)
        {
            if (lower.find(shape) != std::string::npos)
            {
                area.shape = shape;
                break;
            }
        }

        for (std::size_t i = 0; i < lower.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(lower[i])))
                continue;
            std::size_t cursor = i;
            while (cursor < lower.size() && std::isdigit(static_cast<unsigned char>(lower[cursor])))
                ++cursor;
            area.sizeFeet = std::stoi(lower.substr(i, cursor - i));
            break;
        }

        if (area.shape.empty() && area.sizeFeet > 0)
            area.shape = "area";
        return area;
    }
