#pragma once

#include "Models/Rulesets/RulesetContentPackModel.hpp"

#include <string>

struct RulesetContentPackImportResult
{
    bool success = false;
    RulesetContentPackModel pack;
    std::string error;
};

std::string exportRulesetContentPackToJson(const RulesetContentPackModel &pack);
RulesetContentPackImportResult importRulesetContentPackFromJson(const std::string &json);
