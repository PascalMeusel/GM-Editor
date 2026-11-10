#include "BattleTrackerControllerEffectSupport.hpp"
#include "Models/Rulesets/SpellcastingValues.hpp"

BattleTrackerController::BattleTrackerController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService)
    : _currentCampaign(currentCampaign), _playbackService(std::move(playbackService))
{
    _view = std::make_unique<BattleTrackerView>(*this);
    startNewEncounter();
}

void BattleTrackerController::update(std::shared_ptr<GLFWwindow> window)
{
    if (!m_isOpen)
        return;

    synchronizeParticipantsWithSources();
    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}





void BattleTrackerController::renderEmbedded()
{
    synchronizeParticipantsWithSources();
    _view->drawEmbedded();
}

void BattleTrackerController::startNewEncounter()
{
    if (!canLeaveForPreparation()) return;
    ++_correctionGeneration; _dragCorrection.reset();
    _draftEncounter = EncounterModel{};
    _draftEncounter.name = combatText("New Encounter");
    _draftEncounter.cellSizeFeet = 5;
    _draftEncounter.battlefieldWidth = 16;
    _draftEncounter.battlefieldHeight = 12;
    _draftEncounter.round = 1;
    _draftEncounter.activeParticipantIndex = 0;
    _selectedEncounterIndex = -1;
    _selectedParticipantIndex = -1;
    _rangeTargetIndex = -1;
    _selectedTargetIndices.clear();
    _selectedActionIndex = 0;
    _selectedDiceRollPresetIndex = 0;
    _combatStep = 0;
    _freePlacementMode = false;
    _initiativeLocked = false;
    _lastAttackResolved = false;
    _actionAdjudicationReviewed = false;
    _diceRollPresetName = combatText("Quick Roll");
    _diceRollExpression = "1d20";
    _diceRollPresetNotes.clear();
    _lastDiceRollMessage.clear();
    DiceRollPresetModel attackPreset;
    attackPreset.name = combatText("Attack Check");
    attackPreset.expression = "1d20";
    attackPreset.notes = combatText("Generic d20 roll.");
    DiceRollPresetModel damagePreset;
    damagePreset.name = combatText("Small Damage");
    damagePreset.expression = "1d6";
    damagePreset.notes = combatText("Common light damage die.");
    _draftEncounter.diceRollPresets.push_back(attackPreset);
    _draftEncounter.diceRollPresets.push_back(damagePreset);
    loadSelectedDiceRollPreset();
    normalizeEncounter(_draftEncounter);
    setCombatPhase(CombatPhase::Setup);
}



void BattleTrackerController::setSelectedTemporaryHitPoints(int value)
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;
    auto &participant = _draftEncounter.participants[_selectedParticipantIndex];
    value = std::max(0, value);
    if (participant.temporaryHitPoints == value) return;
    const auto before = captureCorrection(CombatEditHistory::Kind::Health);
    participant.temporaryHitPoints = value;
    syncPlayerHealthFromParticipant(_context.campaign, participant);
    saveEncounter();
    recordCorrection(before, "Edit temporary hit points");
}

void BattleTrackerController::changeCombatPhase(CombatPhase phase)
{
    if (!canLeaveForPreparation()) return;
    if (_draftEncounter.participants.empty())
        return;
    if (combatPhaseIsPaused(_draftEncounter.combatPhase))
        return;
    if (!combatPhaseRunsActiveTurn(phase))
        return;

    setCombatPhase(phase);
    _combatStatusMessage = combatFormat("Combat phase set to {0}.", {combatText(combatPhaseDisplayLabel(phase))});
    _combatErrorMessage.clear();
    saveEncounter();
}



void BattleTrackerController::synchronizeParticipantsWithSources()
{
    if (_draftEncounter.participants.empty() || !_context.campaign.ruleSet)
        return;

    const std::vector<ItemModel> items = getAvailableItems();
    const std::vector<SpellModel> availableSpells = getAvailableSpells(_context.campaign);
    const std::vector<MonsterDefinitionModel> monsters = getMonsterCatalog();
    const IRuleSet &ruleset = *_context.campaign.ruleSet;

    for (EncounterParticipantModel &participant : _draftEncounter.participants)
    {
        if (participant.kind == CombatantKind::Player)
        {
            const auto characterIt = std::find_if(_context.campaign.characters.begin(), _context.campaign.characters.end(), [&](const CharacterModel &character) {
                return character.metadata.id == participant.sourceEntityId;
            });
            if (characterIt == _context.campaign.characters.end())
                continue;

            CharacterModel character = *characterIt;
            normalizeCharacterHitPoints(character);
            participant.displayName = std::string(character.nameBuffer);
            refreshHitPointBounds(participant, character.maxHitPoints);
            participant.currentHitPoints = std::clamp(character.currentHitPoints, 0, participant.maxHitPoints);
            participant.defeated = participant.currentHitPoints == 0;
            participant.conditions = character.conditions;
            const RuleArmorSummary armor = CharacterRules::evaluateArmor(ruleset, character, items);
            participant.armorClass = armor.armorClass;
            participant.armorDamageReduction = armor.damageReduction;
            participant.actions = ruleset.getEquippedWeaponActions(character.inventory, items, character.attributes, character.level);
            const std::vector<MonsterActionModel> featureActions = getCharacterFeatureActions(character);
            participant.actions.insert(participant.actions.end(), featureActions.begin(), featureActions.end());
            const std::vector<MonsterActionModel> itemPowerActions = getInventoryPowerActions(character.inventory, items);
            participant.actions.insert(participant.actions.end(), itemPowerActions.begin(), itemPowerActions.end());
            const std::vector<MonsterActionModel> spellActions = SpellcastingValues::preparedActions(_context.campaign, character.spellcasting, availableSpells, character.attributes, character.level);
            participant.actions.insert(participant.actions.end(), spellActions.begin(), spellActions.end());
            continue;
        }

        if (participant.kind == CombatantKind::NPC)
        {
            const auto npcIt = std::find_if(_context.campaign.npcs.begin(), _context.campaign.npcs.end(), [&](const NPCModel &npc) {
                return npc.metadata.id == participant.sourceEntityId;
            });
            if (npcIt == _context.campaign.npcs.end())
                continue;

            const NPCModel &npc = *npcIt;
            participant.displayName = npc.name.empty() ? combatText("NPC") : npc.name;
            refreshHitPointBounds(participant, 10 + std::max(0, ruleset.getAbilityModifier(npc.attributes, "Constitution")));
            const RuleArmorSummary armor = ruleset.evaluateArmor(npc.attributes, npc.inventory, items);
            participant.armorClass = armor.armorClass;
            participant.armorDamageReduction = armor.damageReduction;
            participant.conditions = npc.conditions;
            participant.actions = ruleset.getEquippedWeaponActions(npc.inventory, items, npc.attributes, 1);
            const std::vector<MonsterActionModel> itemPowerActions = getInventoryPowerActions(npc.inventory, items);
            participant.actions.insert(participant.actions.end(), itemPowerActions.begin(), itemPowerActions.end());
            const std::vector<MonsterActionModel> spellActions = SpellcastingValues::preparedActions(_context.campaign, npc.spellcasting, availableSpells, npc.attributes, 1);
            participant.actions.insert(participant.actions.end(), spellActions.begin(), spellActions.end());
            continue;
        }

        const auto monsterIt = std::find_if(monsters.begin(), monsters.end(), [&](const MonsterDefinitionModel &monster) {
            return monster.metadata.id == participant.sourceEntityId || monster.metadata.id == participant.monsterDefinitionId;
        });
        if (monsterIt == monsters.end())
            continue;

        const MonsterDefinitionModel &monster = *monsterIt;
        participant.displayName = monster.name;
        participant.monsterDefinitionId = monster.metadata.id;
        refreshHitPointBounds(participant, monster.hitPoints);
        participant.armorClass = monster.armorClass;
        participant.armorDamageReduction = monster.armorDamageReduction;
        participant.speedFeet = monster.speedFeet;
        participant.damageResistances = splitDamageTraits(monster.damageResistances);
        participant.damageVulnerabilities = splitDamageTraits(monster.damageVulnerabilities);
        participant.damageImmunities = splitDamageTraits(monster.damageImmunities);
        participant.actions = monster.actions;
    }

    normalizeEncounter(_draftEncounter);
}
