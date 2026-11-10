#pragma once

#include "Models/Rulesets/RuleSetActionTypes.hpp"

class IRuleSet {
public:
    virtual ~IRuleSet() = default;

    // Pure virtual function to get the name of the ruleset
    virtual const char* getName() const = 0;
    virtual const char* getId() const;
    virtual const char* getVersionId() const;
    virtual const std::vector<std::string> getLegacyNames() const;
    virtual bool matchesRulesetName(const std::string& name) const;

    virtual const bool levelsExist() const = 0;
    virtual const std::vector<std::string> getAttributes() const = 0;
    virtual const std::vector<std::string> getClasses() const = 0;
    virtual RuleAdvancementProfile advancementProfile(const std::string &) const;
    virtual const std::vector<std::string> getRaces() const = 0;
    virtual const std::vector<ItemModel> getItems() const = 0;
    virtual const std::vector<MonsterDefinitionModel> getMonsters() const;
    virtual const std::vector<SpellModel> getSpells() const;
    virtual const std::vector<RuleCurrencyDefinition> getCurrencies() const;
    virtual const std::vector<RuleEquipmentSlotDefinition> getEquipmentSlots() const;
    virtual std::vector<RuleContentAttribution> getContentAttributions() const;
    virtual RuleContentGateResult validateContentAttribution(const RuleContentAttribution& attribution, bool publicBundle) const;
    virtual const std::vector<RuleSkillDefinition> getSkills() const;
    virtual const std::vector<RuleDifficultyClass> getCommonDifficultyClasses() const;
    virtual const std::vector<RuleConditionDefinition> getConditionDefinitions() const;

    virtual RuleConditionDefinition findConditionDefinition(const std::string& idOrName) const;

    virtual RuleActiveCondition createActiveCondition(const std::string& idOrName, const std::string& sourceEntityId = "", const std::string& sourceName = "", int remainingRounds = -1) const;

    virtual RuleActiveCondition advanceConditionRound(RuleActiveCondition condition) const;

    virtual std::string describeActiveCondition(const RuleActiveCondition& condition) const;

    virtual RuleActionEconomyState startTurnActionEconomy(int speedFeet, const RuleActionEconomyState& previousState = {}) const;

    virtual RuleActionEconomyState startRoundActionEconomy(const RuleActionEconomyState& previousState = {}) const;

    virtual bool canSpendActionResource(const RuleActionEconomyState& state, RuleActionResource resource, int movementFeet = 0, std::string* reason = nullptr) const;

    virtual RuleActionSpendResult spendActionResource(RuleActionEconomyState state, RuleActionResource resource, int movementFeet = 0, const std::string& concentrationSource = "") const;

    virtual RuleActionEconomyState endConcentration(RuleActionEconomyState state) const;

    virtual RuleConcentrationCheck createConcentrationCheck(int damageTaken, int casterLevel = 1, RuleProficiencyTier constitutionSaveProficiency = RuleProficiencyTier::None) const;

    virtual RuleConcentrationResult resolveConcentrationCheck(const std::map<std::string, int>& attributes, const RuleConcentrationCheck& check, int firstD20, int secondD20 = 0, RuleRollMode rollMode = RuleRollMode::Normal) const;

    virtual RuleRestResult applyRest(const RuleRestRequest& request) const;

    virtual RuleDamageResult applyDamageOrHealing(const RuleDamageApplication& request) const;

    virtual RuleDeathSaveResult resolveDeathSave(RuleDeathSaveState state, int d20Roll) const;

    virtual int getExperienceForChallengeRating(const std::string& challengeRating) const;

    virtual RuleEncounterDifficultySummary summarizeEncounterDifficulty(const std::vector<int>& partyLevels, const std::vector<std::string>& monsterChallengeRatings) const;

    virtual RuleRewardSummary summarizeRewards(int experience, bool milestoneAvailable, const std::string& milestoneLabel = "") const;

    virtual RuleTreasureParcel generateTreasureParcel(const RuleLootGenerationRequest& request) const;

    virtual int getInventoryCurrencyValue(const InventoryModel& inventory) const;

    virtual std::string formatCurrencyValue(int baseUnitValue) const;

    virtual void normalizeInventoryCurrency(InventoryModel& inventory) const;

    virtual RuleCarryingCapacitySummary evaluateCarryingCapacity(const std::map<std::string, int>& attributes, const InventoryModel& inventory, const std::vector<ItemModel>& items) const;

    virtual RuleEquipmentSlotValidation validateEquipmentSlots(const InventoryModel& inventory, const std::vector<ItemModel>& items) const;

    virtual int getAbilityModifier(const std::map<std::string, int>& attributes, const std::string& ability) const;

    virtual int getAbilityModifier(int score) const;

    virtual int getProficiencyBonus(int level) const;

    virtual int getProficiencyContribution(int level, RuleProficiencyTier proficiency) const;

    virtual RuleCheckResult evaluateCheck(const std::map<std::string, int>& attributes, const RuleCheckRequest& request) const;

    virtual int getSkillModifier(const std::map<std::string, int>& attributes, const std::string& skillName, int level, RuleProficiencyTier proficiency, int situationalModifier = 0) const;

    virtual int getPassiveScore(const std::map<std::string, int>& attributes, const std::string& skillOrAbility, int level = 1, RuleProficiencyTier proficiency = RuleProficiencyTier::None, int situationalModifier = 0) const;

    virtual RuleArmorSummary evaluateArmor(const std::map<std::string, int>& attributes, const InventoryModel& inventory, const std::vector<ItemModel>& items) const;

    virtual RuleArmorSummary evaluateArmor(const RuleArmorInput& input) const;

    virtual int getWeaponAbilityModifier(const std::map<std::string, int>& attributes, const ItemModel& weapon) const;

    virtual int getWeaponAttackBonus(const std::map<std::string, int>& attributes, const ItemModel& weapon, int level, RuleProficiencyTier proficiency = RuleProficiencyTier::Proficient) const;

    virtual std::string formatSignedModifier(int value) const;

    virtual RuleWeaponProfile getWeaponProfile(const ItemModel& weapon) const;

    virtual std::vector<MonsterActionModel> getEquippedWeaponActions(const InventoryModel& inventory, const std::vector<ItemModel>& items, const std::map<std::string, int>& attributes, int level) const;

    virtual int getSpellAttackBonus(const std::map<std::string, int>& attributes, const CharacterSpellcastingModel& spellcasting, int level) const;

    virtual int getSpellSaveDc(const std::map<std::string, int>& attributes, const CharacterSpellcastingModel& spellcasting, int level) const;

    virtual std::vector<MonsterActionModel> getPreparedSpellActions(const CharacterSpellcastingModel& spellcasting, const std::vector<SpellModel>& spells, const std::map<std::string, int>& attributes, int level) const;

    virtual RuleAreaTemplate parseAreaTemplate(const std::string& areaText) const;

    virtual RuleSpellResolutionSummary summarizeSpellResolution(const SpellModel& spell, const CharacterSpellcastingModel& spellcasting, const std::map<std::string, int>& attributes, int casterLevel, int castAtLevel) const;

    virtual bool canSpendSpellSlot(CharacterSpellcastingModel spellcasting, int spellLevel, std::string* reason = nullptr) const;

    virtual bool spendSpellSlot(CharacterSpellcastingModel& spellcasting, int spellLevel, std::string* reason = nullptr) const;

    virtual RuleDiceSummary summarizeDiceExpression(const std::string& expression) const;

    virtual RuleDiceRollResult evaluateDiceExpression(const std::string& expression, const std::vector<int>& suppliedRolls) const;

    virtual RuleCheckRollResult resolveCheckRoll(const std::map<std::string, int>& attributes, const RuleCheckRequest& request, int firstD20, int secondD20 = 0) const;
};

#include "Models/Rulesets/IRuleSetIdentityDefaults.hpp"
#include "Models/Rulesets/IRuleSetCombatDefaults.hpp"
#include "Models/Rulesets/IRuleSetInventoryDefaults.hpp"
#include "Models/Rulesets/IRuleSetCheckDefaults.hpp"
#include "Models/Rulesets/IRuleSetSpellDefaults.hpp"
