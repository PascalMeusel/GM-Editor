#include "BattleTrackerControllerEffectSupport.hpp"

bool BattleTrackerController::spendSelectedSpellSlot(const MonsterActionModel &action, EncounterParticipantModel &participant, std::string *reason)
{
    if (!canCastSelectedSpell(reason))
        return false;

    if (_castSelectedSpellAsRitual && selectedSpellCanBeCastAsRitual())
        return true;

    if (!action.consumesSpellSlot || action.spellLevel <= 0)
        return true;

    CharacterSpellcastingModel *spellcasting = findParticipantSpellcasting(_context.campaign, participant);
    if (!spellcasting || !_context.campaign.ruleSet)
    {
        if (reason)
            *reason = combatText("This combatant has no spell slot tracker.");
        return false;
    }

    std::string ruleReason;
    if (!_context.campaign.ruleSet->spendSpellSlot(*spellcasting, std::clamp(action.spellLevel, 1, 9), &ruleReason))
    {
        if (reason)
            *reason = localizedRuleReason(ruleReason);
        return false;
    }
    if (reason)
        *reason = localizedRuleReason(ruleReason);

    touchParticipantSpellcaster(_context.campaign, participant);
    saveCampaign();
    return true;
}

bool BattleTrackerController::spendSelectedActionResource(const MonsterActionModel &action, std::string *reason)
{
    if (action.linkedResourceId.empty())
        return true;

    std::vector<ResourceTrackerModel> *resources = getSelectedSourceResources();
    if (!resources)
    {
        if (reason)
            *reason = combatText("Selected action has a resource link, but this combatant has no source resource tracker.");
        return false;
    }

    auto trackerIt = std::find_if(resources->begin(), resources->end(), [&](const ResourceTrackerModel &tracker) {
        return tracker.metadata.id == action.linkedResourceId;
    });
    if (trackerIt == resources->end())
    {
        if (reason)
            *reason = combatText("Selected action's linked resource is missing.");
        return false;
    }

    normalizeResourceTracker(*trackerIt);
    if (trackerIt->used >= trackerIt->maximum)
    {
        if (reason)
            *reason = combatFormat("{0} has no uses left.", {trackerIt->name});
        return false;
    }

    ++trackerIt->used;
    trackerIt->metadata.touch();
    if (_selectedParticipantIndex >= 0 && _selectedParticipantIndex < static_cast<int>(_draftEncounter.participants.size()))
        touchParticipantSpellcaster(_context.campaign, _draftEncounter.participants[_selectedParticipantIndex]);
    saveCampaign();
    if (reason)
        *reason = combatFormat("{0} uses left: {1}/{2}.",
                               {trackerIt->name,
                                std::to_string(std::max(0, trackerIt->maximum - trackerIt->used)),
                                std::to_string(trackerIt->maximum)});
    addCombatLogEntry("Resource",
                      combatFormat("{0} spends {1}.", {_draftEncounter.participants[_selectedParticipantIndex].displayName, trackerIt->name}),
                      _draftEncounter.participants[_selectedParticipantIndex].displayName);
    return true;
}

std::string BattleTrackerController::reactionResourceStatus(int participantIndex, int actionIndex) const
{
    if (participantIndex < 0 || participantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return combatText("Missing combatant.");

    const EncounterParticipantModel &participant = _draftEncounter.participants[static_cast<std::size_t>(participantIndex)];
    if (actionIndex < 0 || actionIndex >= static_cast<int>(participant.actions.size()))
        return combatText("Missing reaction action.");

    const MonsterActionModel &action = participant.actions[static_cast<std::size_t>(actionIndex)];
    if (action.linkedResourceId.empty())
        return combatText("No linked resource.");

    const std::vector<ResourceTrackerModel> *resources = findParticipantResources(_context.campaign, participant);
    if (!resources)
        return combatText("No source resource tracker.");

    const auto trackerIt = std::find_if(resources->begin(), resources->end(), [&](const ResourceTrackerModel &tracker) {
        return tracker.metadata.id == action.linkedResourceId;
    });
    if (trackerIt == resources->end())
        return combatText("Linked resource missing.");

    ResourceTrackerModel tracker = *trackerIt;
    normalizeResourceTracker(tracker);
    return combatFormat("{0}: {1}/{2} left",
                        {tracker.name,
                         std::to_string(std::max(0, tracker.maximum - tracker.used)),
                         std::to_string(tracker.maximum)});
}

bool BattleTrackerController::reactionResourceAvailable(int participantIndex, int actionIndex, std::string *reason) const
{
    if (participantIndex < 0 || participantIndex >= static_cast<int>(_draftEncounter.participants.size()))
    {
        if (reason)
            *reason = combatText("Choose a combatant reaction first.");
        return false;
    }

    const EncounterParticipantModel &participant = _draftEncounter.participants[static_cast<std::size_t>(participantIndex)];
    if (actionIndex < 0 || actionIndex >= static_cast<int>(participant.actions.size()))
    {
        if (reason)
            *reason = combatText("Choose a reaction action first.");
        return false;
    }

    const MonsterActionModel &action = participant.actions[static_cast<std::size_t>(actionIndex)];
    if (action.linkedResourceId.empty())
        return true;

    const std::vector<ResourceTrackerModel> *resources = findParticipantResources(_context.campaign, participant);
    if (!resources)
    {
        if (reason)
            *reason = combatText("Reaction has a resource link, but this combatant has no source resource tracker.");
        return false;
    }

    const auto trackerIt = std::find_if(resources->begin(), resources->end(), [&](const ResourceTrackerModel &tracker) {
        return tracker.metadata.id == action.linkedResourceId;
    });
    if (trackerIt == resources->end())
    {
        if (reason)
            *reason = combatText("Reaction's linked resource is missing.");
        return false;
    }

    ResourceTrackerModel tracker = *trackerIt;
    normalizeResourceTracker(tracker);
    if (tracker.used >= tracker.maximum)
    {
        if (reason)
            *reason = combatFormat("{0} has no uses left.", {tracker.name});
        return false;
    }

    return true;
}

bool BattleTrackerController::spendReactionActionResource(int participantIndex, int actionIndex, std::string *reason)
{
    if (participantIndex < 0 || participantIndex >= static_cast<int>(_draftEncounter.participants.size()))
    {
        if (reason)
            *reason = combatText("Choose a combatant reaction first.");
        return false;
    }

    EncounterParticipantModel &participant = _draftEncounter.participants[static_cast<std::size_t>(participantIndex)];
    if (actionIndex < 0 || actionIndex >= static_cast<int>(participant.actions.size()))
    {
        if (reason)
            *reason = combatText("Choose a reaction action first.");
        return false;
    }

    const MonsterActionModel &action = participant.actions[static_cast<std::size_t>(actionIndex)];
    if (action.linkedResourceId.empty())
    {
        if (reason)
            *reason = combatText("Reaction has no linked resource to spend.");
        addCombatLogEntry("Reaction",
                          combatFormat("{0} reaction reviewed: {1}.",
                                       {participant.displayName, action.name.empty() ? combatText("Reaction") : action.name}),
                          participant.displayName);
        return true;
    }

    std::vector<ResourceTrackerModel> *resources = findParticipantResources(_context.campaign, participant);
    if (!resources)
    {
        if (reason)
            *reason = combatText("Reaction has a resource link, but this combatant has no source resource tracker.");
        return false;
    }

    auto trackerIt = std::find_if(resources->begin(), resources->end(), [&](const ResourceTrackerModel &tracker) {
        return tracker.metadata.id == action.linkedResourceId;
    });
    if (trackerIt == resources->end())
    {
        if (reason)
            *reason = combatText("Reaction's linked resource is missing.");
        return false;
    }

    normalizeResourceTracker(*trackerIt);
    if (trackerIt->used >= trackerIt->maximum)
    {
        if (reason)
            *reason = combatFormat("{0} has no uses left.", {trackerIt->name});
        return false;
    }

    ++trackerIt->used;
    trackerIt->metadata.touch();
    touchParticipantSpellcaster(_context.campaign, participant);
    saveCampaign();
    if (reason)
        *reason = combatFormat("{0} uses left: {1}/{2}.",
                               {trackerIt->name,
                                std::to_string(std::max(0, trackerIt->maximum - trackerIt->used)),
                                std::to_string(trackerIt->maximum)});
    addCombatLogEntry("Reaction",
                      combatFormat("{0} spends {1} for {2}.",
                                   {participant.displayName, trackerIt->name, action.name.empty() ? combatText("Reaction") : action.name}),
                      participant.displayName);
    return true;
}
