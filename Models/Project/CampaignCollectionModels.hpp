#pragma once

#include "Models/Core/EntityMetadata.hpp"

#include <string>
#include <vector>

struct CampaignSavedFilterModel
{
    EntityMetadata metadata{"saved_filter"};
    std::string name = "New Collection";
    std::string query;
    std::vector<std::string> entityTypes;
    std::vector<std::string> requiredTags;
    bool matchAllTags = false;
    bool smartCollection = true;
};

struct CampaignEntityBookmarkModel
{
    EntityMetadata metadata{"entity_bookmark"};
    EntityId entityId;
    std::string entityType;
    std::string label;
    bool favorite = false;
    std::string lastOpenedAtUtc;
    int openCount = 0;
};
