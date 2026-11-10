#pragma once

#include "Models/Campaign/Campaign.hpp"
#include <string>

namespace gm::ux
{
struct NavigationEntity
{
    std::string type;
    bool available = false;
};

// Resolve against the current campaign, including uncommitted in-memory changes.
// The secondary registry may legitimately be stale after a committed warning.
NavigationEntity resolveNavigationEntity(const Campaign &campaign, const EntityId &id);
}
