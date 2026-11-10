#pragma once

#include "Models/Core/EntityMetadata.hpp"

#include <string>

struct RulesetContentPackModel
{
    EntityMetadata metadata{"ruleset_content_pack"};
    std::string rulesetId;
    std::string name;
    std::string version;
    std::string author;
    std::string license;
    std::string importedAtUtc;
    std::string source;
    std::string definitionJson;
    bool enabled = true;
};
