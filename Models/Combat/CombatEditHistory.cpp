#include "CombatEditHistory.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace
{
using namespace CombatEditHistory;
template<class T> auto &entity(T &values, const EntityId &id)
{
    const auto found = std::find_if(values.begin(), values.end(), [&](const auto &value) { return value.metadata.id == id; });
    if (found == values.end()) throw std::runtime_error("The combat record is no longer available.");
    return *found;
}
template<class C> auto &resources(C &campaign, const Snapshot &scope)
{
    if (scope.sourceKind == CombatantKind::Player) return entity(campaign.characters, scope.source).resourceTrackers;
    if (scope.sourceKind == CombatantKind::NPC) return entity(campaign.npcs, scope.source).resourceTrackers;
    throw std::runtime_error("This combatant has no linked resource tracker.");
}
bool sameContext(const Snapshot &a, const Snapshot &b)
{
    return std::tie(a.campaign, a.encounter, a.participant, a.source, a.sourceKind, a.active, a.round,
                    a.map, a.width, a.height, a.resource, a.kind) ==
           std::tie(b.campaign, b.encounter, b.participant, b.source, b.sourceKind, b.active, b.round,
                    b.map, b.width, b.height, b.resource, b.kind);
}
bool same(const Snapshot &a, const Snapshot &b)
{ return sameContext(a, b) && a.values == b.values && a.playerHealth == b.playerHealth; }
void applyParticipant(EncounterModel &encounter, const Snapshot &desired)
{
    auto &actor = entity(encounter.participants, desired.participant);
    if (desired.kind == Kind::Health)
    {
        actor.currentHitPoints = desired.values[0]; actor.temporaryHitPoints = desired.values[1];
        actor.defeated = desired.values[2] != 0; actor.maxHitPoints = desired.values[3];
    }
    if (desired.kind == Kind::Movement)
    { actor.x = desired.values[0]; actor.y = desired.values[1]; actor.movementUsedFeet = desired.values[2]; }
    actor.metadata.touch(); encounter.metadata.touch();
}
void applySource(Campaign &campaign, const Snapshot &desired)
{
    if (desired.kind == Kind::Resource)
    {
        auto &tracker = entity(resources(campaign, desired), desired.resource);
        tracker.used = desired.values[0]; tracker.metadata.touch();
    }
    if (desired.playerHealth)
    {
        auto &player = entity(campaign.characters, desired.source);
        player.currentHitPoints = (*desired.playerHealth)[0]; player.temporaryHitPoints = (*desired.playerHealth)[1];
        player.maxHitPoints = (*desired.playerHealth)[2]; player.metadata.touch();
    }
}
}

namespace CombatEditHistory
{
Snapshot capture(const Campaign &campaign, const EncounterModel &encounter, const EntityId &participantId,
                 Kind kind, const EntityId &resourceId)
{
    const auto &actor = entity(encounter.participants, participantId);
    Snapshot value; value.kind = kind; value.campaign = campaign.metadata.id; value.encounter = encounter.metadata.id;
    value.participant = participantId; value.source = actor.sourceEntityId; value.sourceKind = actor.kind;
    value.round = encounter.round; value.map = encounter.mapId; value.width = encounter.battlefieldWidth; value.height = encounter.battlefieldHeight;
    if (encounter.activeParticipantIndex >= 0 && encounter.activeParticipantIndex < static_cast<int>(encounter.participants.size()))
        value.active = encounter.participants[static_cast<std::size_t>(encounter.activeParticipantIndex)].metadata.id;
    value.resource = resourceId;
    if (kind == Kind::Health)
    {
        value.values = {actor.currentHitPoints, actor.temporaryHitPoints, actor.defeated ? 1 : 0, actor.maxHitPoints, 0};
        if (actor.kind == CombatantKind::Player)
        {
            const auto &player = entity(campaign.characters, actor.sourceEntityId);
            value.playerHealth = std::array{player.currentHitPoints, player.temporaryHitPoints, player.maxHitPoints};
        }
    }
    if (kind == Kind::Movement) value.values = {actor.x, actor.y, actor.movementUsedFeet, actor.turnStartX, actor.turnStartY};
    if (kind == Kind::Resource)
    {
        const auto &tracker = entity(resources(campaign, value), resourceId);
        value.values = {tracker.used, tracker.maximum, 0, 0, 0};
    }
    return value;
}

void recordApplied(Campaign &campaign, EncounterModel &draft, const Snapshot &before,
                   std::function<bool(Campaign &)> persist, std::function<bool()> contextCurrent, std::string label)
{
    const auto after = capture(campaign, draft, before.participant, before.kind, before.resource);
    if (!sameContext(before, after)) throw std::runtime_error("Combat context changed during this edit.");
    if (same(before, after)) return;
    auto perform = [&campaign, &draft, before, after, persist = std::move(persist), contextCurrent = std::move(contextCurrent), label](bool forward) {
        if (contextCurrent && !contextCurrent()) throw std::runtime_error("Combat selection or turn changed. This correction is no longer available.");
        const auto &expected = forward ? before : after, &desired = forward ? after : before;
        const auto &saved = entity(campaign.encounters, expected.encounter);
        if (draft.metadata.id != expected.encounter || draft.combatPhase == CombatPhase::WrapUp || saved.combatPhase == CombatPhase::WrapUp ||
            !same(capture(campaign, draft, expected.participant, expected.kind, expected.resource), expected) ||
            !same(capture(campaign, saved, expected.participant, expected.kind, expected.resource), expected))
            throw std::runtime_error("The turn or combat values changed. Newer work was preserved.");
        Campaign staged = campaign;
        EncounterModel nextDraft = draft;
        auto &nextSaved = entity(staged.encounters, expected.encounter);
        applyParticipant(nextSaved, desired); applyParticipant(nextDraft, desired); applySource(staged, desired);
        CombatLogEntryModel entry; entry.round = nextSaved.round; entry.eventType = "Correction";
        entry.actorName = entity(nextSaved.participants, expected.participant).displayName;
        entry.summary = std::string(forward ? "Redid: " : "Undid: ") + label;
        nextSaved.combatLog.push_back(entry); nextDraft.combatLog.push_back(entry);
        staged.metadata.touch(); blackoutLocalPlayerDisplay(staged.localPlayerDisplay);
        if (!persist || !persist(staged)) throw std::runtime_error("The combat correction could not be saved. Nothing was changed.");
        campaign = std::move(staged); draft = std::move(nextDraft);
    };
    ProjectCommandManager::getInstance().recordApplied(std::make_unique<LambdaProjectCommand>(
        std::move(label), [perform] { perform(true); }, [perform] { perform(false); }));
}
}
