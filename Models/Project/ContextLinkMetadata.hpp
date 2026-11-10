#pragma once
#include "Models/Core/EntityMetadata.hpp"

// Context links are edited by the workspace inspector. Retained specialist
// drafts must keep that current state, including explicit link removal.
inline void preserveCurrentContextLinks(EntityMetadata &draft, const EntityMetadata &current)
{
    for (const auto *key : {"gm.context.place", "gm.context.faction", "gm.context.shop"})
    {
        const auto found = current.customProperties.find(key);
        if (found == current.customProperties.end()) draft.customProperties.erase(key);
        else draft.customProperties[key] = found->second;
    }
}
