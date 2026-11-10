#include "Models/Combat/EncounterDraftPersistence.hpp"
#include <iostream>
#include <stdexcept>

int main()
{
    int failures = 0;
    const auto check = [&](bool value, const char *message) {
        if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
    };
    Campaign campaign;
    CharacterModel first; first.metadata.id = "player-one"; first.maxHitPoints = 20; first.currentHitPoints = 20;
    CharacterModel second; second.metadata.id = "player-two"; second.maxHitPoints = 30; second.currentHitPoints = 30;
    campaign.characters = {first, second};
    EncounterModel draft; draft.metadata.id = "fight";
    EncounterParticipantModel p; p.kind = CombatantKind::Player; p.sourceEntityId = first.metadata.id;
    p.maxHitPoints = 20; p.currentHitPoints = 20; p.temporaryHitPoints = 5;
    draft.participants.push_back(p);
    p.sourceEntityId = second.metadata.id; p.maxHitPoints = 30; p.currentHitPoints = 30; p.temporaryHitPoints = 0;
    draft.participants.push_back(p);
    EncounterModel other; other.metadata.id = "other-fight";
    campaign.encounters = {other, draft};
    Campaign committed = campaign;
    int selectedIndex = 0; // Deliberately stale after library reordering.
    int saves = 0;
    auto coherent = [&](const Campaign &snapshot) {
        const auto &saved = snapshot.encounters[1];
        return saved.metadata.id == "fight" && snapshot.encounters[0].metadata.id == "other-fight" &&
            saved.participants[0].currentHitPoints == snapshot.characters[0].currentHitPoints &&
            saved.participants[1].currentHitPoints == snapshot.characters[1].currentHitPoints &&
            saved.participants[0].temporaryHitPoints == 0 && saved.combatLog.size() == 1;
    };
    // Model the existing rules result, then synchronize both targets and log
    // the action. No target sync may publish an intermediate disk snapshot.
    draft.participants[0].currentHitPoints = 17; draft.participants[0].temporaryHitPoints = 0;
    draft.participants[1].currentHitPoints = 22;
    syncEncounterPlayerHealthDraft(campaign, draft.participants[0]);
    syncEncounterPlayerHealthDraft(campaign, draft.participants[1]);
    CombatLogEntryModel entry; entry.eventType = "Damage"; entry.summary = "Both targets damaged";
    draft.combatLog.push_back(entry);
    check(saves == 0 && committed.characters[0].currentHitPoints == 20 && committed.encounters[1].combatLog.empty(),
          "per-target health synchronization must not persist partial health");
    const auto failed = [&](Campaign &snapshot) {
        ++saves; check(coherent(snapshot), "failed write received inconsistent player/encounter/log snapshot");
        return false;
    };
    check(!persistEncounterDraftSnapshot(campaign, draft, selectedIndex, failed), "injected failure must propagate");
    check(selectedIndex == 1 && campaign.encounters.size() == 2, "stale index must resolve identity without replacing another encounter");
    check(coherent(campaign), "failed save must retain coherent editable health draft");
    check(committed.characters[0].currentHitPoints == 20 && committed.encounters[1].participants[0].temporaryHitPoints == 5,
          "failed persistence changed committed snapshot");
    const auto throwing = [&](Campaign &) -> bool { ++saves; throw std::runtime_error("injected persistence exception"); };
    check(!persistEncounterDraftSnapshot(campaign, draft, selectedIndex, throwing), "thrown save failure must stay retryable");
    const auto success = [&](Campaign &snapshot) { ++saves; check(coherent(snapshot), "retry snapshot incoherent"); committed = snapshot; return true; };
    check(persistEncounterDraftSnapshot(campaign, draft, selectedIndex, success), "retry failed");
    check(saves == 3 && coherent(committed) && committed.characters[0].currentHitPoints == 17,
          "retry must save once without applying damage twice or duplicating log");
    draft.participants[0].currentHitPoints = 20;
    syncEncounterPlayerHealthDraft(campaign, draft.participants[0]);
    check(persistEncounterDraftSnapshot(campaign, draft, selectedIndex, success) && committed.characters[0].currentHitPoints == 20,
          "healing must commit matching canonical and encounter HP");
    EncounterParticipantModel npc = draft.participants[0]; npc.kind = CombatantKind::NPC; npc.currentHitPoints = 1;
    syncEncounterPlayerHealthDraft(campaign, npc);
    check(campaign.characters[0].currentHitPoints == 20, "NPC sync must not mutate a player with same source ID");
    EncounterModel newFight; newFight.metadata.id = "new-fight";
    const auto noWrite = [](Campaign &) { return false; };
    check(!persistEncounterDraftSnapshot(campaign, newFight, selectedIndex, noWrite), "new encounter failure must propagate");
    check(!persistEncounterDraftSnapshot(campaign, newFight, selectedIndex, noWrite) && campaign.encounters.size() == 3,
          "new encounter retry duplicated draft identity");
    std::cout << "Encounter persistence contracts: " << failures << " failures\n";
    return failures ? 1 : 0;
}
