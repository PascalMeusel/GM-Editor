#pragma once

#include "WorldInspectorService.hpp"

namespace WorldInspector::detail
{
void synchronizeGeneratedWorldVisibility(Campaign &campaign);
MutationResult result(MutationStatus status, std::string message, EntityId entityId = {});
void touch(Campaign &campaign, EntityMetadata &metadata);
bool mapExists(const Campaign &campaign, const EntityId &mapId);
} // namespace WorldInspector::detail
