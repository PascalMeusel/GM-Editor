#include "Models/Project/ContextLinkMetadata.hpp"
#include "EncounterBuilderController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include <algorithm>
#include "Models/UX/CampaignNavigationResolver.hpp"
#include <iterator>

bool EncounterBuilderController::saveEncounter()
{
    markDraftEdited();
    if (_beforeSaveHandler && !_beforeSaveHandler())
    {
        _saveFailed = true;
        _saveSucceeded = false;
        _saveBlockedByCombat = true;
        _tacticalStatus = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
            ? "Speichere zuerst die ausstehenden Kampfänderungen. Die Vorbereitung bleibt hier."
            : "Save pending combat changes first. Your encounter preparation remains here.";
        return false;
    }
    if (_saveBlockedByCombat)
    {
        _saveBlockedByCombat = false;
        _tacticalStatus.clear();
    }
    if (!_persistedDraftId.empty() &&
        !gm::ux::resolveNavigationEntity(_currentCampaign, _persistedDraftId).available)
    {
        _saveFailed = true;
        _saveSucceeded = false;
        _tacticalStatus = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
            ? "Diese Begegnung ist nicht mehr verfügbar. Der Entwurf bleibt erhalten."
            : "This encounter is no longer available. The draft is retained.";
        return false;
    }
    if (_draftEncounter.name.empty())
        _draftEncounter.name = gm::localization::trUi("Untitled Encounter");

    synchronizeParticipantsWithSources();
    normalizeEncounter(_draftEncounter);
    _draftEncounter.metadata.touch();

    Campaign proposed = _currentCampaign;
    const auto existing = std::find_if(proposed.encounters.begin(), proposed.encounters.end(),
        [&](const EncounterModel &encounter) { return encounter.metadata.id == _draftEncounter.metadata.id; });
    if (existing != proposed.encounters.end())
    {
        // Preparation owns map/roster/setup fields, not a running fight's
        // health, effects, turn position or log. An already-open prep draft
        // must not roll those values back after combat advances elsewhere.
        const bool running = existing->round > 1 || !existing->combatLog.empty() ||
            (existing->combatPhase != CombatPhase::Setup && existing->combatPhase != CombatPhase::Initiative);
        if (running)
        {
            EntityId activeId;
            if (existing->activeParticipantIndex >= 0 && existing->activeParticipantIndex < static_cast<int>(existing->participants.size()))
                activeId = existing->participants[existing->activeParticipantIndex].metadata.id;
            _draftEncounter.round = existing->round;
            _draftEncounter.combatPhase = existing->combatPhase;
            _draftEncounter.combatPaused = existing->combatPaused;
            _draftEncounter.combatLog = existing->combatLog;
            _draftEncounter.diceRollPresets = existing->diceRollPresets;
            _draftEncounter.afterCombatSummary = existing->afterCombatSummary;
            _draftEncounter.afterCombatLootNotes = existing->afterCombatLootNotes;
            _draftEncounter.afterCombatStoryConsequences = existing->afterCombatStoryConsequences;
            _draftEncounter.afterCombatSessionNotes = existing->afterCombatSessionNotes;
            for (int index = 0; index < static_cast<int>(_draftEncounter.participants.size()); ++index)
            {
                auto &participant = _draftEncounter.participants[index];
                const auto current = std::find_if(existing->participants.begin(), existing->participants.end(),
                    [&](const EncounterParticipantModel &candidate) { return candidate.metadata.id == participant.metadata.id; });
                if (current == existing->participants.end()) continue;
                participant.currentHitPoints = current->currentHitPoints;
                participant.maxHitPoints = current->maxHitPoints;
                participant.temporaryHitPoints = current->temporaryHitPoints;
                participant.conditions = current->conditions;
                participant.activeEffects = current->activeEffects;
                participant.defeated = current->defeated;
                participant.x = current->x;
                participant.y = current->y;
                participant.turnStartX = current->turnStartX;
                participant.turnStartY = current->turnStartY;
                participant.movementUsedFeet = current->movementUsedFeet;
                participant.narrativePosition = current->narrativePosition;
                participant.narrativeRangeBand = current->narrativeRangeBand;
                participant.narrativeSpotlight = current->narrativeSpotlight;
                if (participant.metadata.id == activeId) _draftEncounter.activeParticipantIndex = index;
            }
            normalizeEncounter(_draftEncounter);
        }
        _selectedEncounterIndex = static_cast<int>(std::distance(proposed.encounters.begin(), existing));
        preserveCurrentContextLinks(_draftEncounter.metadata, existing->metadata);
        *existing = _draftEncounter;
    }
    else
    {
        proposed.encounters.push_back(_draftEncounter);
        _selectedEncounterIndex = static_cast<int>(proposed.encounters.size()) - 1;
    }

    proposed.metadata.touch();
    _saveFailed = !ProjectSaveService::getInstance().saveCampaign(proposed, "save encounter builder prep");
    if (!_saveFailed)
    {
        _currentCampaign = std::move(proposed);
        _persistedDraftId = _draftEncounter.metadata.id;
        _draftEdited = false;
        _retainedEncounterDrafts.erase(_draftEncounter.metadata.id);
    }
    _saveSucceeded = !_saveFailed;
    if (_saveSucceeded && _savedHandler)
        _savedHandler(_draftEncounter.metadata.id);
    return !_saveFailed;
}

bool EncounterBuilderController::saveCampaign()
{
    _currentCampaign.metadata.touch();
    return ProjectSaveService::getInstance().saveCampaign(_currentCampaign, "save encounter builder prep");
}
