#pragma once

#include "Models/Core/EntityMetadata.hpp"

#include <string>
#include <vector>

struct HouseRuleChangeModel
{
    std::string changedAtUtc;
    std::string summary;
};

struct HouseRuleModel
{
    EntityMetadata metadata{"house_rule"};
    std::string label;
    std::string description;
    bool enabled = true;
    std::vector<HouseRuleChangeModel> changeHistory;
};
