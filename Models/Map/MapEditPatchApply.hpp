#pragma once

#include "Models/Map/MapEditPatchBuilder.hpp"

inline bool canApplyMapGenerationPatchAtomically(const MapModel &target,
                                                 const MapGenerationPatch &patch)
{
    if (!patch.valid || !patch.preservationVerified ||
        patch.targetMapId != target.metadata.id || patch.sourceRevision != target.revision ||
        patch.after.metadata.id != patch.targetMapId ||
        patch.after.revision != patch.sourceRevision + 1 ||
        !mapGenerationMapsEqual(target, patch.before) ||
        !mapGenerationPatchPreservesContract(patch))
        return false;
    return true;
}

inline bool applyMapGenerationPatchAtomically(MapModel &target,
                                              const MapGenerationPatch &patch)
{
    if (!canApplyMapGenerationPatchAtomically(target, patch))
        return false;
    // Allocate/copy first. If that fails, the target remains untouched; the
    // final move swap consists only of standard-library value types.
    MapModel committed = patch.after;
    using std::swap;
    swap(target, committed);
    return true;
}
