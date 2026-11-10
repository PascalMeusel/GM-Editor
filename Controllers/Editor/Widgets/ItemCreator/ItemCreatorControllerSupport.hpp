#pragma once

#include "ItemCreatorController.hpp"

#include <algorithm>
#include <cctype>

namespace
{
std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsText(std::string value, std::string needle)
{
    if (needle.empty())
        return true;
    return toLowerCopy(value).find(toLowerCopy(needle)) != std::string::npos;
}

void setArchivedTag(ItemModel &item, bool archived)
{
    auto &tags = item.metadata.tags;
    const auto found = std::find(tags.begin(), tags.end(), "archived");
    if (archived && found == tags.end())
        tags.push_back("archived");
    else if (!archived && found != tags.end())
        tags.erase(found);
}
}
