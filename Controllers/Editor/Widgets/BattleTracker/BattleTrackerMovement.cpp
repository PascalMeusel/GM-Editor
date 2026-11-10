#include "BattleTrackerControllerEffectSupport.hpp"

bool BattleTrackerController::moveParticipant(int participantIndex, int x, int y)
{
    if (!canParticipantMoveTo(participantIndex, x, y))
        return false;

    EncounterParticipantModel &participant = _draftEncounter.participants[participantIndex];
    participant.x = x;
    participant.y = y;
    if (participantIndex == _draftEncounter.activeParticipantIndex)
    {
        const int tacticalDistance = encounterMovementCostFeet(_draftEncounter, participant.turnStartX, participant.turnStartY, participant.x, participant.y);
        participant.movementUsedFeet = tacticalDistance >= std::numeric_limits<int>::max() / 8
                                           ? encounterCellDistanceFeet(_draftEncounter, participant.turnStartX, participant.turnStartY, participant.x, participant.y)
                                           : tacticalDistance;
    }
    return true;
}

void BattleTrackerController::beginParticipantDrag(int participantIndex)
{
    if (participantIndex < 0 || participantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    _dragParticipantIndex = participantIndex;
    _dragStartX = _draftEncounter.participants[participantIndex].x;
    _dragStartY = _draftEncounter.participants[participantIndex].y;
    _selectedParticipantIndex = participantIndex;
    _dragCorrection = captureCorrection(CombatEditHistory::Kind::Movement);
}

void BattleTrackerController::endParticipantDrag()
{
    if (_dragParticipantIndex >= 0 && _dragParticipantIndex < static_cast<int>(_draftEncounter.participants.size()))
    {
        const EncounterParticipantModel &participant = _draftEncounter.participants[_dragParticipantIndex];
        if (participant.x != _dragStartX || participant.y != _dragStartY)
        {
            bool reactionPrompted = false;
            addCombatLogEntry("Movement",
                              combatFormat("{0} moved from ({1}, {2}) to ({3}, {4}).",
                                           {participant.displayName,
                                            std::to_string(_dragStartX),
                                            std::to_string(_dragStartY),
                                            std::to_string(participant.x),
                                            std::to_string(participant.y)}),
                              participant.displayName);
            for (int otherIndex = 0; otherIndex < static_cast<int>(_draftEncounter.participants.size()); ++otherIndex)
            {
                if (otherIndex == _dragParticipantIndex)
                    continue;
                const EncounterParticipantModel &threat = _draftEncounter.participants[otherIndex];
                if (threat.defeated || threat.friendly == participant.friendly)
                    continue;
                const int threatRange = participantThreatRangeFeet(threat);
                const int startDistance = encounterCellDistanceFeet(_draftEncounter, _dragStartX, _dragStartY, threat.x, threat.y);
                const int endDistance = encounterCellDistanceFeet(_draftEncounter, participant.x, participant.y, threat.x, threat.y);
                if (startDistance <= threatRange && endDistance > threatRange)
                {
                    addCombatLogEntry("Reaction",
                                      combatFormat("{0} left {1}'s reach. Consider an opportunity attack or reaction.",
                                                   {participant.displayName, threat.displayName}),
                                      threat.displayName);
                    reactionPrompted = true;
                }
            }
            if (reactionPrompted && _combatStep == 1 && !_draftEncounter.combatPaused)
                changeCombatPhase(CombatPhase::ReactionsEffects);
        }
    }
    _dragParticipantIndex = -1;
    recordCorrection(_dragCorrection, "Move combat token");
    _dragCorrection.reset();
}

bool BattleTrackerController::canParticipantMoveTo(int participantIndex, int x, int y) const
{
    if (participantIndex < 0 || participantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return false;
    if (!isEncounterCellInside(_draftEncounter, x, y))
        return false;
    if (_draftEncounter.theaterOfMind)
        return true;
    if (_freePlacementMode)
        return true;
    if (_combatStep != 1)
        return !_draftEncounter.combatPaused;
    if (participantIndex != _draftEncounter.activeParticipantIndex)
        return false;

    const EncounterParticipantModel &participant = _draftEncounter.participants[participantIndex];
    const int distance = encounterMovementCostFeet(_draftEncounter, participant.turnStartX, participant.turnStartY, x, y);
    return distance <= participant.speedFeet;
}

int BattleTrackerController::distanceBetweenParticipants(int attackerIndex, int targetIndex) const
{
    if (attackerIndex < 0 || attackerIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return 0;
    if (targetIndex < 0 || targetIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return 0;
    if (_draftEncounter.theaterOfMind)
        return 0;

    const EncounterParticipantModel &attacker = _draftEncounter.participants[attackerIndex];
    const EncounterParticipantModel &target = _draftEncounter.participants[targetIndex];
    return encounterCellDistanceFeet(_draftEncounter, attacker.x, attacker.y, target.x, target.y);
}

std::vector<ItemModel> BattleTrackerController::getAvailableItems() const
{
    std::vector<ItemModel> items;
    if (_context.campaign.ruleSet)
        items = _context.campaign.ruleSet->getItems();
    items.insert(items.end(), _context.campaign.customItems.begin(), _context.campaign.customItems.end());
    return items;
}

MapModel *BattleTrackerController::getSelectedMap()
{
    if (_context.campaign.maps.empty())
        return nullptr;
    if (_selectedMapIndex < 0 || _selectedMapIndex >= static_cast<int>(_context.campaign.maps.size()))
        _selectedMapIndex = 0;
    return &_context.campaign.maps[_selectedMapIndex];
}

const MapModel *BattleTrackerController::getSelectedMap() const
{
    if (_context.campaign.maps.empty())
        return nullptr;
    if (_selectedMapIndex < 0 || _selectedMapIndex >= static_cast<int>(_context.campaign.maps.size()))
        return &_context.campaign.maps.front();
    return &_context.campaign.maps[_selectedMapIndex];
}

ImU32 BattleTrackerController::participantColor(const EncounterParticipantModel &participant) const
{
    if (participant.defeated)
        return IM_COL32(90, 96, 104, 255);
    if (participant.kind == CombatantKind::Player)
        return IM_COL32(92, 172, 238, 255);
    if (participant.kind == CombatantKind::NPC)
        return participant.friendly ? IM_COL32(98, 204, 150, 255) : IM_COL32(220, 176, 80, 255);
    return IM_COL32(224, 92, 82, 255);
}

int BattleTrackerController::findOpenX() const
{
    const int index = static_cast<int>(_draftEncounter.participants.size());
    return std::min(_draftEncounter.battlefieldWidth - 1, 1 + index % std::max(1, _draftEncounter.battlefieldWidth - 2));
}

int BattleTrackerController::findOpenY() const
{
    const int index = static_cast<int>(_draftEncounter.participants.size());
    return std::min(_draftEncounter.battlefieldHeight - 1, 1 + index / std::max(1, _draftEncounter.battlefieldWidth - 2));
}



void BattleTrackerController::onShow()
{
    if (_selectedEncounterIndex < 0 && !_context.campaign.encounters.empty())
    {
        _selectedEncounterIndex = 0;
        loadSelectedEncounter();
    }
    _view->show();
    _view->requestDockOnNextUpdate();
}
