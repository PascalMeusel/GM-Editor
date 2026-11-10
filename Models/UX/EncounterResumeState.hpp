#pragma once
#include "Models/Combat/EncounterModel.hpp"
#include <algorithm>
#include <vector>

namespace gm::ux {
inline bool encounterHasResumableState(const EncounterModel &encounter)
{
    // Wrap-up deliberately keeps combatPaused and history for inspection. Its
    // explicit completion phase outranks those older continuation heuristics.
    if (encounter.combatPhase == CombatPhase::WrapUp) return false;
    if (encounter.combatPhase != CombatPhase::Setup && encounter.combatPhase != CombatPhase::Initiative)
        return true;
    return encounter.round > 1 || encounter.activeParticipantIndex > 0 ||
           !encounter.combatLog.empty() || encounter.combatPaused;
}
inline const EncounterModel *resumableEncounter(const std::vector<EncounterModel> &encounters)
{
    const auto paused = std::find_if(encounters.begin(), encounters.end(), [](const EncounterModel &encounter) {
        return encounterHasResumableState(encounter) &&
               (encounter.combatPaused || encounter.combatPhase == CombatPhase::PausedBreak);
    });
    if (paused != encounters.end()) return &*paused;
    const auto active = std::find_if(encounters.begin(), encounters.end(), encounterHasResumableState);
    return active == encounters.end() ? nullptr : &*active;
}
}
