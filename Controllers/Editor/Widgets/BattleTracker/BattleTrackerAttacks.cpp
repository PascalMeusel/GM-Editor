#include "BattleTrackerControllerEffectSupport.hpp"
#include "Models/Rulesets/SpellcastingValues.hpp"

void BattleTrackerController::resolveAttackRoll()
{
    _lastAttackResolved = false;
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;
    const std::vector<int> targets = getSelectedTargetIndices();
    if (targets.empty())
        return;

    const MonsterActionModel action = getSelectedAction();
    if (!action.sourceSpellId.empty())
    {
        SpellModel spell;
        const auto caster = getParticipantSpellcasterSnapshot(
            _context.campaign, _draftEncounter.participants[_selectedParticipantIndex]);
        const bool sourceAvailable = caster.valid &&
            findAvailableSpellById(_context.campaign, action.sourceSpellId, spell);
        if (!sourceAvailable || !SpellcastingValues::readyForSpell(
                _context.campaign, caster.attributes, caster.spellcasting, caster.level, spell))
        {
            _lastAttackResolved = true;
            _lastAttackHit = false;
            const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
            _lastAttackMessage = sourceAvailable
                ? (german ? "Dieser Zauber benötigt eine nicht verfügbare Regelberechnung. Prüfe Berechnung und Quelle oder wähle Manuell."
                          : "This spell needs an unavailable rules calculation. Review its calculation and source, or choose Manual.")
                : (german ? "Der Zauber oder seine zaubernde Figur ist nicht mehr verfügbar."
                          : "The spell or its caster is no longer available.");
            return;
        }
    }
    const int actionRange = std::max(action.reachFeet, action.rangeFeet);
    if (!_draftEncounter.theaterOfMind && actionRange > 0)
    {
        const auto outOfRangeIt = std::find_if(targets.begin(), targets.end(), [&](int targetIndex) {
            return distanceBetweenParticipants(_selectedParticipantIndex, targetIndex) > actionRange;
        });
        if (outOfRangeIt != targets.end())
        {
            _lastAttackResolved = true;
            _lastAttackHit = false;
            _lastAttackMessage = combatFormat("{0} is out of range for {1}.",
                                              {_draftEncounter.participants[*outOfRangeIt].displayName, action.name});
            return;
        }
    }

    if (!_draftEncounter.theaterOfMind)
    {
        const EncounterParticipantModel &attacker = _draftEncounter.participants[_selectedParticipantIndex];
        const auto outOfSightIt = std::find_if(targets.begin(), targets.end(), [&](int targetIndex) {
            const EncounterParticipantModel &target = _draftEncounter.participants[targetIndex];
            return encounterLineOfSightBlocked(_draftEncounter, attacker.x, attacker.y, target.x, target.y);
        });
        if (outOfSightIt != targets.end())
        {
            _lastAttackResolved = true;
            _lastAttackHit = false;
            _lastAttackMessage = combatFormat("{0} is behind blocked line of sight for {1}.",
                                              {_draftEncounter.participants[*outOfSightIt].displayName, action.name});
            return;
        }
    }

    std::string resourceReason;
    if (!spendSelectedActionResource(action, &resourceReason))
    {
        _lastAttackResolved = true;
        _lastAttackHit = false;
        _lastAttackMessage = resourceReason;
        return;
    }

    int hitCount = 0;
    std::string context;
    if (_attackHasAdvantage && !_attackHasDisadvantage)
        context += combatText(" Advantage applied.");
    if (_attackHasDisadvantage && !_attackHasAdvantage)
        context += combatText(" Disadvantage applied.");
    if (_targetHasCover)
        context += combatText(" Cover applied (+2 AC).");
    if (_targetProne)
        context += combatText(" Target prone.");
    if (_targetUnseen)
        context += combatText(" Target unseen.");
    if (_targetHidden)
        context += combatText(" Target hidden.");
    if (!_gmAdjudicationNotes.empty())
        context += combatText(" GM: ") + _gmAdjudicationNotes;
    for (int targetIndex : targets)
    {
        const EncounterParticipantModel &target = _draftEncounter.participants[targetIndex];
        const int targetArmorClass = target.armorClass + (_targetHasCover ? 2 : 0);
        if (getAttackTotal() >= targetArmorClass)
            ++hitCount;
    }
    _lastAttackHit = hitCount > 0;
    _lastAttackResolved = true;
    _lastAttackMessage = combatFormat("{0} resolves against {1} target(s) with {2}: {3} hit.",
                                      {action.name,
                                       std::to_string(static_cast<int>(targets.size())),
                                       std::to_string(getAttackTotal()),
                                       std::to_string(hitCount)});
    addCombatLogEntry("Attack", appendContext(_lastAttackMessage, context), _draftEncounter.participants[_selectedParticipantIndex].displayName);
    if (!_lastAttackHit)
        changeCombatPhase(CombatPhase::ReactionsEffects);
}

void BattleTrackerController::applyDamageRoll()
{
    if (!_lastAttackResolved || !_lastAttackHit)
        return;
    const std::vector<int> targets = getSelectedTargetIndices();
    if (targets.empty())
        return;

    const std::string actorName = _draftEncounter.participants[_selectedParticipantIndex].displayName;
    for (int targetIndex : targets)
    {
        EncounterParticipantModel &target = _draftEncounter.participants[targetIndex];
        const int targetArmorClass = target.armorClass + (_targetHasCover ? 2 : 0);
        if (getAttackTotal() < targetArmorClass)
            continue;

        const std::string damageType = _damageTypeDraft.empty() ? damageTypeFromText(getSelectedAction().damage) : lowerCopy(trimCopy(_damageTypeDraft));
        RuleDamageResult result;
        if (_context.campaign.ruleSet)
        {
            RuleDamageApplication request;
            request.currentHitPoints = target.currentHitPoints;
            request.maxHitPoints = target.maxHitPoints;
            request.temporaryHitPoints = target.temporaryHitPoints;
            request.amount = std::max(0, getDamageTotal() - target.armorDamageReduction);
            request.damageType = damageType;
            request.resistances = target.damageResistances;
            request.vulnerabilities = target.damageVulnerabilities;
            request.immunities = target.damageImmunities;
            result = _context.campaign.ruleSet->applyDamageOrHealing(request);
        }
        else
        {
            result.currentHitPoints = std::clamp(target.currentHitPoints - std::max(0, getDamageTotal() - target.armorDamageReduction), 0, std::max(0, target.maxHitPoints));
            result.temporaryHitPoints = target.temporaryHitPoints;
            result.effectiveAmount = target.currentHitPoints - result.currentHitPoints;
            result.defeated = result.currentHitPoints == 0;
        }
        target.currentHitPoints = result.currentHitPoints;
        target.temporaryHitPoints = result.temporaryHitPoints;
        target.defeated = result.defeated;
        syncPlayerHealthFromParticipant(_context.campaign, target);
        std::string notes;
        for (const std::string &note : result.notes)
            notes += " " + combatText(note.c_str());
        const std::string amount = std::to_string(result.effectiveAmount);
        const std::string localizedDamageType = damageType.empty() ? std::string{} : combatText(damageType.c_str());
        const char *damageTemplate = damageType.empty()
                                         ? (target.defeated ? "{0} takes {1} damage and is defeated." : "{0} takes {1} damage.")
                                         : (target.defeated ? "{0} takes {1} {2} damage and is defeated." : "{0} takes {1} {2} damage.");
        addCombatLogEntry("Damage",
                          combatFormat(damageTemplate, {target.displayName, amount, localizedDamageType}) + notes,
                          actorName);
        if (result.effectiveAmount > 0 && participantIsConcentrating(target) && _context.campaign.ruleSet)
        {
            const RuleConcentrationCheck concentrationCheck = _context.campaign.ruleSet->createConcentrationCheck(result.effectiveAmount);
            addCombatLogEntry("Concentration",
                              combatFormat("{0} is concentrating and should make a Constitution save DC {1}.",
                                           {target.displayName, std::to_string(concentrationCheck.dc)}),
                              target.displayName);
        }
    }
    changeCombatPhase(CombatPhase::ReactionsEffects);
}

MonsterActionModel BattleTrackerController::getSelectedAction() const
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return MonsterActionModel{};

    const EncounterParticipantModel &participant = _draftEncounter.participants[_selectedParticipantIndex];
    if (!participant.actions.empty())
    {
        const int index = std::clamp(_selectedActionIndex, 0, static_cast<int>(participant.actions.size()) - 1);
        return participant.actions[index];
    }

    MonsterActionModel manual;
    manual.name = _manualActionName.empty() ? combatText("Manual Attack") : _manualActionName;
    manual.reachFeet = _manualAttackRangeFeet;
    manual.normalRangeFeet = _manualAttackRangeFeet;
    manual.rangeFeet = _manualAttackRangeFeet;
    manual.damage = _manualDamageFormula;
    return manual;
}

bool BattleTrackerController::selectedActionIsSpell() const
{
    return !getSelectedAction().sourceSpellId.empty();
}
