#include "EncounterBuilderController.hpp"
#include "Models/Rulesets/SpellcastingValues.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include "Models/Map/TravelSceneGenerator.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/CharacterRulesService.hpp"
#include "Models/Combat/EncounterMapEligibility.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <optional>
#include <random>
#include <sstream>

namespace
{
void refreshHitPointBounds(EncounterParticipantModel &participant, int newMaxHitPoints)
{
    const bool hadNoStoredMaximum = participant.maxHitPoints <= 0;
    participant.maxHitPoints = std::max(1, newMaxHitPoints);
    if (hadNoStoredMaximum)
        participant.currentHitPoints = participant.maxHitPoints;
    else
        participant.currentHitPoints = std::clamp(participant.currentHitPoints, 0, participant.maxHitPoints);
    participant.defeated = participant.currentHitPoints == 0;
}

std::vector<MonsterActionModel> getInventoryPowerActions(const InventoryModel &inventory, const std::vector<ItemModel> &items)
{
    std::vector<MonsterActionModel> actions;
    for (const InventoryEntryModel &entry : inventory.entries)
    {
        if (!entry.equipped && !entry.attuned)
            continue;
        const ItemModel *item = findItemById(items, entry.itemId);
        if (!item)
            continue;
        for (MonsterActionModel action : item->powers)
        {
            if (action.name.empty())
                action.name = item->name.empty() ? "Item Power" : item->name;
            if (action.actionKind.empty() || action.actionKind == "Attack")
                action.actionKind = "Item Power";
            action.sourceLabel = item->name;
            action.sourceEntityId = item->metadata.id;
            actions.push_back(action);
        }
    }
    return actions;
}

std::vector<MonsterActionModel> getCharacterFeatureActions(const CharacterModel &character)
{
    std::vector<MonsterActionModel> actions = character.featureActions;
    for (MonsterActionModel &action : actions)
    {
        if (action.actionKind.empty() || action.actionKind == "Attack")
            action.actionKind = "Feature";
        action.sourceLabel = std::string(character.nameBuffer);
        action.sourceEntityId = character.metadata.id;
    }
    return actions;
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trimCopy(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
        ++start;
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
        --end;
    return value.substr(start, end - start);
}

std::vector<std::string> splitDamageTraits(const std::string &traits)
{
    std::vector<std::string> values;
    std::stringstream stream(traits);
    std::string value;
    while (std::getline(stream, value, ','))
    {
        value = lowerCopy(trimCopy(value));
        if (!value.empty())
            values.push_back(value);
    }
    return values;
}
}

void EncounterBuilderController::synchronizeParticipantsWithSources()
{
    if (_draftEncounter.participants.empty() || !_currentCampaign.ruleSet)
        return;

    const std::vector<ItemModel> items = getAvailableItems();
    const std::vector<SpellModel> availableSpells = getAvailableSpells(_currentCampaign);
    const std::vector<MonsterDefinitionModel> monsters = getMonsterCatalog();
    const IRuleSet &ruleset = *_currentCampaign.ruleSet;

    for (EncounterParticipantModel &participant : _draftEncounter.participants)
    {
        if (participant.kind == CombatantKind::Player)
        {
            const auto characterIt = std::find_if(_currentCampaign.characters.begin(), _currentCampaign.characters.end(), [&](const CharacterModel &character) {
                return character.metadata.id == participant.sourceEntityId;
            });
            if (characterIt == _currentCampaign.characters.end())
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
            const std::vector<MonsterActionModel> spellActions = SpellcastingValues::preparedActions(_currentCampaign, character.spellcasting, availableSpells, character.attributes, character.level);
            participant.actions.insert(participant.actions.end(), spellActions.begin(), spellActions.end());
            continue;
        }

        if (participant.kind == CombatantKind::NPC)
        {
            const auto npcIt = std::find_if(_currentCampaign.npcs.begin(), _currentCampaign.npcs.end(), [&](const NPCModel &npc) {
                return npc.metadata.id == participant.sourceEntityId;
            });
            if (npcIt == _currentCampaign.npcs.end())
                continue;

            const NPCModel &npc = *npcIt;
            participant.displayName = npc.name.empty() ? "NPC" : npc.name;
            const RuleArmorSummary armor = ruleset.evaluateArmor(npc.attributes, npc.inventory, items);
            participant.armorClass = armor.armorClass;
            participant.armorDamageReduction = armor.damageReduction;
            participant.conditions = npc.conditions;
            participant.actions = ruleset.getEquippedWeaponActions(npc.inventory, items, npc.attributes, 1);
            const std::vector<MonsterActionModel> itemPowerActions = getInventoryPowerActions(npc.inventory, items);
            participant.actions.insert(participant.actions.end(), itemPowerActions.begin(), itemPowerActions.end());
            const std::vector<MonsterActionModel> spellActions = SpellcastingValues::preparedActions(_currentCampaign, npc.spellcasting, availableSpells, npc.attributes, 1);
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

void EncounterBuilderController::addSelectedPlayer()
{
    if (_currentCampaign.characters.empty() || !_currentCampaign.ruleSet)
        return;

    if (_selectedCharacterIndex < 0 || _selectedCharacterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        _selectedCharacterIndex = 0;

    CharacterModel character = _currentCampaign.characters[_selectedCharacterIndex];
    normalizeCharacterHitPoints(character);
    const std::vector<ItemModel> items = getAvailableItems();
    const std::vector<SpellModel> availableSpells = getAvailableSpells(_currentCampaign);
    const IRuleSet &ruleset = *_currentCampaign.ruleSet;
    const RuleArmorSummary armor = CharacterRules::evaluateArmor(ruleset, character, items);
    EncounterParticipantModel participant;
    participant.kind = CombatantKind::Player;
    participant.sourceEntityId = character.metadata.id;
    participant.displayName = std::string(character.nameBuffer);
    participant.maxHitPoints = character.maxHitPoints;
    participant.currentHitPoints = std::clamp(character.currentHitPoints, 0, character.maxHitPoints);
    participant.temporaryHitPoints = character.temporaryHitPoints;
    participant.armorClass = armor.armorClass;
    participant.armorDamageReduction = armor.damageReduction;
    participant.conditions = character.conditions;
    participant.actions = ruleset.getEquippedWeaponActions(character.inventory, items, character.attributes, character.level);
    const std::vector<MonsterActionModel> featureActions = getCharacterFeatureActions(character);
    participant.actions.insert(participant.actions.end(), featureActions.begin(), featureActions.end());
    const std::vector<MonsterActionModel> itemPowerActions = getInventoryPowerActions(character.inventory, items);
    participant.actions.insert(participant.actions.end(), itemPowerActions.begin(), itemPowerActions.end());
    const std::vector<MonsterActionModel> spellActions = SpellcastingValues::preparedActions(_currentCampaign, character.spellcasting, availableSpells, character.attributes, character.level);
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
    markDraftEdited();
    _selectedParticipantIndex = static_cast<int>(_draftEncounter.participants.size()) - 1;
}

void EncounterBuilderController::addAllPlayers()
{
    for (int i = 0; i < static_cast<int>(_currentCampaign.characters.size()); ++i)
    {
        _selectedCharacterIndex = i;
        addSelectedPlayer();
    }
}

void EncounterBuilderController::addSelectedNPC()
{
    if (_currentCampaign.npcs.empty() || !_currentCampaign.ruleSet)
        return;

    if (_selectedNPCIndex < 0 || _selectedNPCIndex >= static_cast<int>(_currentCampaign.npcs.size()))
        _selectedNPCIndex = 0;

    const NPCModel &npc = _currentCampaign.npcs[_selectedNPCIndex];
    const std::vector<ItemModel> items = getAvailableItems();
    const std::vector<SpellModel> availableSpells = getAvailableSpells(_currentCampaign);
    const IRuleSet &ruleset = *_currentCampaign.ruleSet;
    const RuleArmorSummary armor = ruleset.evaluateArmor(npc.attributes, npc.inventory, items);
    EncounterParticipantModel participant;
    participant.kind = CombatantKind::NPC;
    participant.sourceEntityId = npc.metadata.id;
    participant.displayName = npc.name.empty() ? "NPC" : npc.name;
    participant.maxHitPoints = 10 + std::max(0, ruleset.getAbilityModifier(npc.attributes, "Constitution"));
    participant.currentHitPoints = participant.maxHitPoints;
    participant.armorClass = armor.armorClass;
    participant.armorDamageReduction = armor.damageReduction;
    participant.actions = ruleset.getEquippedWeaponActions(npc.inventory, items, npc.attributes, 1);
    const std::vector<MonsterActionModel> itemPowerActions = getInventoryPowerActions(npc.inventory, items);
    participant.actions.insert(participant.actions.end(), itemPowerActions.begin(), itemPowerActions.end());
    const std::vector<MonsterActionModel> spellActions = SpellcastingValues::preparedActions(_currentCampaign, npc.spellcasting, availableSpells, npc.attributes, 1);
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
    markDraftEdited();
    _selectedParticipantIndex = static_cast<int>(_draftEncounter.participants.size()) - 1;
}

void EncounterBuilderController::addSelectedMonster()
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
    markDraftEdited();
    _selectedParticipantIndex = static_cast<int>(_draftEncounter.participants.size()) - 1;
}

void EncounterBuilderController::removeSelectedParticipant()
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    _draftEncounter.participants.erase(_draftEncounter.participants.begin() + _selectedParticipantIndex);
    markDraftEdited();
    normalizeEncounter(_draftEncounter);
    _selectedParticipantIndex = _draftEncounter.participants.empty() ? -1 : std::min(_selectedParticipantIndex, static_cast<int>(_draftEncounter.participants.size()) - 1);
}

std::vector<MonsterDefinitionModel> EncounterBuilderController::getMonsterCatalog() const
{
    std::vector<MonsterDefinitionModel> monsters = _currentCampaign.customMonsters;
    if (_currentCampaign.ruleSet)
    {
        const std::vector<MonsterDefinitionModel> rulesetMonsters = _currentCampaign.ruleSet->getMonsters();
        monsters.insert(monsters.end(), rulesetMonsters.begin(), rulesetMonsters.end());
    }
    return monsters;
}

std::vector<ItemModel> EncounterBuilderController::getAvailableItems() const
{
    std::vector<ItemModel> items;
    if (_currentCampaign.ruleSet)
        items = _currentCampaign.ruleSet->getItems();
    items.insert(items.end(), _currentCampaign.customItems.begin(), _currentCampaign.customItems.end());
    return items;
}

int EncounterBuilderController::findOpenX() const
{
    const int index = static_cast<int>(_draftEncounter.participants.size());
    return std::min(_draftEncounter.battlefieldWidth - 1, 1 + index % std::max(1, _draftEncounter.battlefieldWidth - 2));
}

int EncounterBuilderController::findOpenY() const
{
    const int index = static_cast<int>(_draftEncounter.participants.size());
    return std::min(_draftEncounter.battlefieldHeight - 1, 1 + index / std::max(1, _draftEncounter.battlefieldWidth - 2));
}
