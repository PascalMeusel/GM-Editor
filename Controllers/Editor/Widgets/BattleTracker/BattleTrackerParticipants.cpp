#include "BattleTrackerControllerEffectSupport.hpp"
#include "Models/Rulesets/SpellcastingValues.hpp"

void BattleTrackerController::addSelectedPlayer()
{
    if (_context.campaign.characters.empty() || !_context.campaign.ruleSet)
        return;

    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_context.campaign.characters.size()))
        _selectedCharacterIndex = 0;

    CharacterModel character = _context.campaign.characters[_selectedCharacterIndex];
    normalizeCharacterHitPoints(character);
    const std::vector<ItemModel> items = getAvailableItems();
    const std::vector<SpellModel> availableSpells = getAvailableSpells(_context.campaign);
    const IRuleSet &ruleset = *_context.campaign.ruleSet;
    const RuleArmorSummary armor = CharacterRules::evaluateArmor(ruleset, character, items);
    EncounterParticipantModel participant;
    participant.kind = CombatantKind::Player;
    participant.sourceEntityId = character.metadata.id;
    participant.displayName = std::string(character.nameBuffer);
    participant.maxHitPoints = character.maxHitPoints;
    participant.currentHitPoints = std::clamp(character.currentHitPoints, 0, character.maxHitPoints);
    participant.armorClass = armor.armorClass;
    participant.armorDamageReduction = armor.damageReduction;
    participant.conditions = character.conditions;
    participant.actions = ruleset.getEquippedWeaponActions(character.inventory, items, character.attributes, character.level);
    const std::vector<MonsterActionModel> featureActions = getCharacterFeatureActions(character);
    participant.actions.insert(participant.actions.end(), featureActions.begin(), featureActions.end());
    const std::vector<MonsterActionModel> itemPowerActions = getInventoryPowerActions(character.inventory, items);
    participant.actions.insert(participant.actions.end(), itemPowerActions.begin(), itemPowerActions.end());
    const std::vector<MonsterActionModel> spellActions = SpellcastingValues::preparedActions(_context.campaign, character.spellcasting, availableSpells, character.attributes, character.level);
    participant.actions.insert(participant.actions.end(), spellActions.begin(), spellActions.end());
    participant.speedFeet = 30;
    participant.friendly = true;
    participant.x = findOpenX();
    participant.y = findOpenY();
    participant.startingX = participant.x;
    participant.startingY = participant.y;
    participant.turnStartX = participant.x;
    participant.turnStartY = participant.y;
    _draftEncounter.participants.push_back(participant);
    _selectedParticipantIndex = static_cast<int>(_draftEncounter.participants.size()) - 1;
    _initiativeLocked = false;
    setCombatPhase(CombatPhase::Setup);
}

void BattleTrackerController::addAllPlayers()
{
    for (int i = 0; i < static_cast<int>(_context.campaign.characters.size()); ++i)
    {
        _selectedCharacterIndex = i;
        addSelectedPlayer();
    }
}

void BattleTrackerController::addSelectedNPC()
{
    if (_context.campaign.npcs.empty() || !_context.campaign.ruleSet)
        return;

    if (_selectedNPCIndex < 0 || _selectedNPCIndex >= static_cast<int>(_context.campaign.npcs.size()))
        _selectedNPCIndex = 0;

    const NPCModel &npc = _context.campaign.npcs[_selectedNPCIndex];
    const std::vector<ItemModel> items = getAvailableItems();
    const std::vector<SpellModel> availableSpells = getAvailableSpells(_context.campaign);
    const IRuleSet &ruleset = *_context.campaign.ruleSet;
    const RuleArmorSummary armor = ruleset.evaluateArmor(npc.attributes, npc.inventory, items);
    EncounterParticipantModel participant;
    participant.kind = CombatantKind::NPC;
    participant.sourceEntityId = npc.metadata.id;
    participant.displayName = npc.name.empty() ? combatText("NPC") : npc.name;
    participant.maxHitPoints = 10 + std::max(0, ruleset.getAbilityModifier(npc.attributes, "Constitution"));
    participant.currentHitPoints = participant.maxHitPoints;
    participant.armorClass = armor.armorClass;
    participant.armorDamageReduction = armor.damageReduction;
    participant.actions = ruleset.getEquippedWeaponActions(npc.inventory, items, npc.attributes, 1);
    const std::vector<MonsterActionModel> itemPowerActions = getInventoryPowerActions(npc.inventory, items);
    participant.actions.insert(participant.actions.end(), itemPowerActions.begin(), itemPowerActions.end());
    const std::vector<MonsterActionModel> spellActions = SpellcastingValues::preparedActions(_context.campaign, npc.spellcasting, availableSpells, npc.attributes, 1);
    participant.actions.insert(participant.actions.end(), spellActions.begin(), spellActions.end());
    participant.speedFeet = 30;
    participant.conditions = npc.conditions;
    participant.x = findOpenX();
    participant.y = findOpenY();
    participant.startingX = participant.x;
    participant.startingY = participant.y;
    participant.turnStartX = participant.x;
    participant.turnStartY = participant.y;
    _draftEncounter.participants.push_back(participant);
    _selectedParticipantIndex = static_cast<int>(_draftEncounter.participants.size()) - 1;
    _initiativeLocked = false;
    setCombatPhase(CombatPhase::Setup);
}

void BattleTrackerController::addSelectedMonster()
{
    const std::vector<MonsterDefinitionModel> monsters = getMonsterCatalog();
    if (monsters.empty())
        return;

    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(monsters.size()))
        _selectedMonsterIndex = 0;

    const MonsterDefinitionModel &monster = monsters[_selectedMonsterIndex];
    EncounterParticipantModel participant;
    participant.kind = CombatantKind::Monster;
    participant.sourceEntityId = monster.metadata.id;
    participant.monsterDefinitionId = monster.metadata.id;
    participant.displayName = monster.name;
    participant.maxHitPoints = monster.hitPoints;
    participant.currentHitPoints = monster.hitPoints;
    participant.armorClass = monster.armorClass;
    participant.armorDamageReduction = monster.armorDamageReduction;
    participant.speedFeet = monster.speedFeet;
    participant.damageResistances = splitDamageTraits(monster.damageResistances);
    participant.damageVulnerabilities = splitDamageTraits(monster.damageVulnerabilities);
    participant.damageImmunities = splitDamageTraits(monster.damageImmunities);
    participant.actions = monster.actions;
    participant.x = findOpenX();
    participant.y = findOpenY();
    participant.startingX = participant.x;
    participant.startingY = participant.y;
    participant.turnStartX = participant.x;
    participant.turnStartY = participant.y;
    _draftEncounter.participants.push_back(participant);
    _selectedParticipantIndex = static_cast<int>(_draftEncounter.participants.size()) - 1;
    _initiativeLocked = false;
    setCombatPhase(CombatPhase::Setup);
}

void BattleTrackerController::removeSelectedParticipant()
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    _draftEncounter.participants.erase(_draftEncounter.participants.begin() + _selectedParticipantIndex);
    normalizeEncounter(_draftEncounter);
    if (_draftEncounter.participants.empty())
    {
        _selectedParticipantIndex = -1;
        _rangeTargetIndex = -1;
        setCombatPhase(CombatPhase::Setup);
        return;
    }

    _selectedParticipantIndex = std::min(_selectedParticipantIndex, static_cast<int>(_draftEncounter.participants.size()) - 1);
    if (_rangeTargetIndex >= static_cast<int>(_draftEncounter.participants.size()))
        _rangeTargetIndex = 0;
    _initiativeLocked = false;
}

void BattleTrackerController::rollInitiativeForAll()
{
    if (!canLeaveForPreparation()) return;
    if (_draftEncounter.participants.empty())
        return;

    const std::vector<MonsterDefinitionModel> monsters = getMonsterCatalog();
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<int> d20(1, 20);

    if (!_context.campaign.ruleSet)
        return;

    for (EncounterParticipantModel &participant : _draftEncounter.participants)
        participant.initiative = d20(generator) + initiativeModifierForParticipant(*_context.campaign.ruleSet, _context.campaign, participant, monsters);

    _initiativeLocked = false;
    setCombatPhase(CombatPhase::Initiative);
    _lastAttackResolved = true;
    _lastAttackHit = false;
    _lastAttackMessage = combatText("Rolled initiative for all combatants. Sort by roll when ready.");
    addCombatLogEntry("Initiative", _lastAttackMessage);
}





void BattleTrackerController::pauseCombat()
{
    if (!canLeaveForPreparation()) return;
    if (_draftEncounter.participants.empty())
        return;
    _combatErrorMessage.clear();
    _combatStatusMessage = combatText("Combat paused. Live session and audio were left unchanged.");
    setCombatPhase(CombatPhase::PausedBreak);
    _freePlacementMode = false;
    addCombatLogEntry("Combat", combatText("Combat paused. Live session and audio were left unchanged."), combatantActorName(_draftEncounter));
    saveEncounter();
}

void BattleTrackerController::resumeCombat()
{
    if (!canLeaveForPreparation()) return;
    if (_draftEncounter.participants.empty())
        return;
    _combatErrorMessage.clear();
    _combatStatusMessage = combatText("Combat resumed. Live session and audio were left unchanged.");
    setCombatPhase(CombatPhase::ActiveTurn);
    addCombatLogEntry("Combat", combatText("Combat resumed. Live session and audio were left unchanged."), combatantActorName(_draftEncounter));
    saveEncounter();
}

void BattleTrackerController::moveParticipantInOrder(int participantIndex, int direction)
{
    const int targetIndex = participantIndex + direction;
    if (participantIndex < 0 || participantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;
    if (targetIndex < 0 || targetIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    std::swap(_draftEncounter.participants[participantIndex], _draftEncounter.participants[targetIndex]);
    if (_draftEncounter.activeParticipantIndex == participantIndex)
        _draftEncounter.activeParticipantIndex = targetIndex;
    else if (_draftEncounter.activeParticipantIndex == targetIndex)
        _draftEncounter.activeParticipantIndex = participantIndex;
    _selectedParticipantIndex = targetIndex;
    _initiativeLocked = !_draftEncounter.participants.empty();
}
