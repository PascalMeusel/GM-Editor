#pragma once
#include "Models/Core/EntityMetadata.hpp"

namespace SessionOutcomeDraftKeys
{
enum class Surface { Preparation, Live };
// Preparation retains the original keys so existing unfinished checkpoints are
// still offered for recovery. Independent Live editors must never overwrite or
// delete that checkpoint while flushing their own stale in-memory draft.
inline std::string composer(Surface surface, const EntityId &sessionId)
{ return (surface == Surface::Live ? "live-outcome-" : "outcome-") + sessionId; }
inline std::string recap(Surface surface, const EntityId &sessionId)
{ return (surface == Surface::Live ? "live-recap-" : "recap-") + sessionId; }
}
