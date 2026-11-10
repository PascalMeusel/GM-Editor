#pragma once

#include "Models/Rulesets/RulesetDefinitionModel.hpp"

#include <map>
#include <string>
#include <vector>

struct RulesetFormulaEvaluationResult
{
    bool valid = false;
    double value = 0.0;
    std::vector<std::string> errors;
};

RulesetFormulaEvaluationResult evaluateRulesetFormula(const std::string &formula, const std::map<std::string, double> &variables);
std::map<std::string, double> buildRulesetFormulaPreviewVariables(const RulesetDefinitionModel &definition);
RulesetFormulaEvaluationResult previewRulesetFormula(const RulesetDefinitionModel &definition, const std::string &formula);
