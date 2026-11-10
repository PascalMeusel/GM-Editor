#pragma once

#include "../IRuleSet.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <set>

class DND5e : public IRuleSet
{
    struct ArmorProfile
    {
        bool known = false;
        int baseArmorClass = 10;
        bool usesDexterity = true;
        int maxDexterityBonus = -1;
    };

public:
    using IRuleSet::evaluateArmor;

    DND5e() = default;
    ~DND5e() override = default;

    // Override the getName method to return the ruleset name
    const char *getName() const override;

    const char *getId() const override;

    const char *getVersionId() const override;

    const std::vector<std::string> getLegacyNames() const override;

    // Override methods to return attributes

    const bool levelsExist() const override;

    const std::vector<std::string> getAttributes() const override;
    const std::vector<std::string> getClasses() const override;
    RuleAdvancementProfile advancementProfile(const std::string &className) const override;

    const std::vector<std::string> getRaces() const override;

    std::vector<RuleContentAttribution> getContentAttributions() const override;

    RuleContentGateResult validateContentAttribution(const RuleContentAttribution &attribution, bool publicBundle) const override;

    const std::vector<RuleSkillDefinition> getSkills() const override;

    const std::vector<RuleDifficultyClass> getCommonDifficultyClasses() const override;

    const std::vector<RuleCurrencyDefinition> getCurrencies() const override;

    RuleCarryingCapacitySummary evaluateCarryingCapacity(const std::map<std::string, int> &attributes, const InventoryModel &inventory, const std::vector<ItemModel> &items) const override;

    const std::vector<RuleEquipmentSlotDefinition> getEquipmentSlots() const override;

    RuleEquipmentSlotValidation validateEquipmentSlots(const InventoryModel &inventory, const std::vector<ItemModel> &items) const override;

    const std::vector<RuleConditionDefinition> getConditionDefinitions() const override;

    int getProficiencyBonus(int level) const override;

    int getWeaponAbilityModifier(const std::map<std::string, int> &attributes, const ItemModel &weapon) const override;

    RuleArmorSummary evaluateArmor(const RuleArmorInput &input) const override;

    RuleWeaponProfile getWeaponProfile(const ItemModel &weapon) const override;

    RuleDiceSummary summarizeDiceExpression(const std::string &expression) const override;

    RuleDiceRollResult evaluateDiceExpression(const std::string &expression, const std::vector<int> &suppliedRolls) const override;

    RuleAreaTemplate parseAreaTemplate(const std::string &areaText) const override;

    RuleSpellResolutionSummary summarizeSpellResolution(const SpellModel &spell,
                                                        const CharacterSpellcastingModel &spellcasting,
                                                        const std::map<std::string, int> &attributes,
                                                        int casterLevel,
                                                        int castAtLevel) const override;

    int getExperienceForChallengeRating(const std::string &challengeRating) const override;

    RuleEncounterDifficultySummary summarizeEncounterDifficulty(const std::vector<int> &partyLevels, const std::vector<std::string> &monsterChallengeRatings) const override;

    RuleTreasureParcel generateTreasureParcel(const RuleLootGenerationRequest &request) const override;

    const std::vector<ItemModel> getItems() const override;

    const std::vector<MonsterDefinitionModel> getMonsters() const override;

    const std::vector<SpellModel> getSpells() const override;
    // Additional methods specific to the 2024 rules can be added here

private:
    static std::string damageTypeFromDamage(const std::string &damage);

    static std::string lowerCopy(std::string value);

    static void addProperty(RuleWeaponProfile &profile, const std::string &property);

    static bool hasProperty(const RuleWeaponProfile &profile, const std::string &property);

    static void setRange(RuleWeaponProfile &profile, int normalRangeFeet, int longRangeFeet);

    static void applyCommonProperties(RuleWeaponProfile &profile);

    static void applyWeaponProfile(const std::string &id, RuleWeaponProfile &profile);

    static bool isShield(const ItemModel &item);

    static ArmorProfile armorProfileFor(const ItemModel &item);

    static MonsterActionModel makeAction(const std::string &name, int reachFeet, int rangeFeet, const std::string &attackBonus, const std::string &damage, const std::string &notes = "");

    static MonsterDefinitionModel makeMonster(
        const std::string &name,
        const std::string &size,
        const std::string &type,
        const std::string &challengeRating,
        int armorClass,
        int hitPoints,
        int speedFeet,
        int experience,
        const std::map<std::string, int> &attributes,
        const std::vector<MonsterActionModel> &actions);

    static ItemModel makeItem(const std::string &name, const std::string &type, int valueCopper, double weight, bool stackable, int armorClassBonus = 0, int damageReduction = 0);

    static ItemModel makeWeapon(const std::string &name, int valueCopper, double weight, bool stackable, const std::string &damage, const std::string &ability = "Strength", int reachFeet = 5, int rangeFeet = 5, int attackBonus = 0);

    static SpellModel makeSpell(
        const std::string &name,
        int level,
        SpellSchool school,
        SpellResolutionType resolution,
        const std::string &castingTime,
        const std::string &range,
        const std::string &duration,
        const std::string &classes,
        const std::string &damage = "",
        const std::string &healing = "",
        const std::string &savingThrowAbility = "",
        const std::string &area = "",
        const std::string &effectSummary = "",
        const std::string &scaling = "",
        bool concentration = false,
        bool ritual = false);

    static std::string slugify(std::string value);
};

#include "Models/Rulesets/DND/DND5eIdentityDefaults.hpp"
#include "Models/Rulesets/DND/DND5eEquipmentDefaults.hpp"
#include "Models/Rulesets/DND/DND5eDiceDefaults.hpp"
#include "Models/Rulesets/DND/DND5eSpellResolutionDefaults.hpp"
#include "Models/Rulesets/DND/DND5eEncounterDefaults.hpp"
#include "Models/Rulesets/DND/DND5eContentDefaults.hpp"
#include "Models/Rulesets/DND/DND5eWeaponHelpers.hpp"
#include "Models/Rulesets/DND/DND5eCatalogHelpers.hpp"
