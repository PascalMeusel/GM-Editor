#pragma once

#include "Models/Campaign/Campaign.hpp"
#include <functional>

// Synchronize the editable campaign draft without performing an intermediate
// disk write. The encounter, its log and canonical player health must reach
// the repository in the same save snapshot.
inline void syncEncounterPlayerHealthDraft(Campaign &campaign, const EncounterParticipantModel &participant)
{
    if (participant.kind != CombatantKind::Player)
        return;
    const auto found = std::find_if(campaign.characters.begin(), campaign.characters.end(),
        [&](const CharacterModel &character) { return character.metadata.id == participant.sourceEntityId; });
    if (found == campaign.characters.end())
        return;
    found->maxHitPoints = std::max(1, participant.maxHitPoints);
    found->currentHitPoints = std::clamp(participant.currentHitPoints, 0, found->maxHitPoints);
    found->temporaryHitPoints = std::max(0, participant.temporaryHitPoints);
    found->metadata.touch();
    campaign.metadata.touch();
}

inline bool persistEncounterDraftSnapshot(Campaign &campaign, const EncounterModel &draft,
    int &selectedIndex, const std::function<bool(Campaign &)> &persist)
{
    // Resolve identity again: another editor may have reordered the library.
    const auto found = std::find_if(campaign.encounters.begin(), campaign.encounters.end(),
        [&](const EncounterModel &encounter) { return encounter.metadata.id == draft.metadata.id; });
    if (found != campaign.encounters.end())
    {
        selectedIndex = static_cast<int>(std::distance(campaign.encounters.begin(), found));
        *found = draft;
    }
    else
    {
        campaign.encounters.push_back(draft);
        selectedIndex = static_cast<int>(campaign.encounters.size()) - 1;
    }
    campaign.metadata.touch();
    // A failed save retains one coherent editable draft for Retry. The
    // repository's transaction retains the previous committed disk snapshot.
    try { return persist && persist(campaign); }
    catch (...) { return false; }
}
