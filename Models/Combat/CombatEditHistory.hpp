#pragma once
#include "Models/Campaign/Campaign.hpp"
#include <array>
#include <functional>
#include <optional>

namespace CombatEditHistory
{
enum class Kind { Health, Resource, Movement };
struct Snapshot
{
    Kind kind = Kind::Health;
    EntityId campaign, encounter, participant, source, active, map, resource;
    CombatantKind sourceKind = CombatantKind::Monster;
    int round = 0, width = 0, height = 0;
    std::array<int, 5> values{};
    std::optional<std::array<int, 3>> playerHealth;
};
Snapshot capture(const Campaign &, const EncounterModel &, const EntityId &participantId, Kind,
                 const EntityId &resourceId = {});
// The original operation is applied to the retained draft. If its save failed,
// contextCurrent must remain false until a successful retry. Exact-value guards
// reject later conflicting edits. Corrections commit only a scoped patch,
// retaining old log entries and appending an explicit correction event.
void recordApplied(Campaign &, EncounterModel &liveDraft, const Snapshot &before,
                   std::function<bool(Campaign &)> persist, std::function<bool()> contextCurrent,
                   std::string label);
}
