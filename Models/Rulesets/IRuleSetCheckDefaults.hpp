#pragma once



#include "Models/Rulesets/IRuleSet.hpp"



inline int IRuleSet::getAbilityModifier(const std::map<std::string, int>& attributes, const std::string& ability) const
    {
        const auto it = attributes.find(ability);
        if (it == attributes.end())
            return 0;
        return getAbilityModifier(it->second);
    }

inline int IRuleSet::getAbilityModifier(int score) const
    {
        return score >= 10 ? (score - 10) / 2 : (score - 11) / 2;
    }

inline int IRuleSet::getProficiencyBonus(int level) const
    {
        (void)level;
        return 0;
    }

inline int IRuleSet::getProficiencyContribution(int level, RuleProficiencyTier proficiency) const
    {
        const int proficiencyBonus = getProficiencyBonus(level);
        switch (proficiency)
        {
        case RuleProficiencyTier::Half:
            return proficiencyBonus / 2;
        case RuleProficiencyTier::Proficient:
            return proficiencyBonus;
        case RuleProficiencyTier::Expertise:
            return proficiencyBonus * 2;
        case RuleProficiencyTier::None:
        default:
            return 0;
        }
    }

inline RuleCheckResult IRuleSet::evaluateCheck(const std::map<std::string, int>& attributes, const RuleCheckRequest& request) const
    {
        RuleCheckResult result;
        result.modifier = getAbilityModifier(attributes, request.ability) +
                          getProficiencyContribution(request.level, request.proficiency) +
                          request.situationalModifier;
        result.passiveScore = 10 + result.modifier;
        result.dc = request.dc;
        result.rollMode = request.rollMode;
        return result;
    }

inline int IRuleSet::getSkillModifier(const std::map<std::string, int>& attributes, const std::string& skillName, int level, RuleProficiencyTier proficiency, int situationalModifier) const
    {
        std::string ability = skillName;
        for (const RuleSkillDefinition& skill : getSkills())
        {
            if (skill.name == skillName || skill.id == skillName)
            {
                ability = skill.ability;
                break;
            }
        }
        RuleCheckRequest request;
        request.ability = ability;
        request.level = level;
        request.proficiency = proficiency;
        request.situationalModifier = situationalModifier;
        return evaluateCheck(attributes, request).modifier;
    }

inline int IRuleSet::getPassiveScore(const std::map<std::string, int>& attributes, const std::string& skillOrAbility, int level, RuleProficiencyTier proficiency, int situationalModifier) const
    {
        return 10 + getSkillModifier(attributes, skillOrAbility, level, proficiency, situationalModifier);
    }

inline RuleArmorSummary IRuleSet::evaluateArmor(const std::map<std::string, int>& attributes, const InventoryModel& inventory, const std::vector<ItemModel>& items) const
    {
        RuleArmorInput input;
        input.attributes = attributes;
        input.inventory = &inventory;
        input.items = &items;
        return evaluateArmor(input);
    }

inline RuleArmorSummary IRuleSet::evaluateArmor(const RuleArmorInput& input) const
    {
        RuleArmorSummary summary;
        const InventoryModel emptyInventory;
        const std::vector<ItemModel> emptyItems;
        const InventoryModel& inventory = input.inventory ? *input.inventory : emptyInventory;
        const std::vector<ItemModel>& items = input.items ? *input.items : emptyItems;
        summary.armorClass = std::max(1, 10 + getAbilityModifier(input.attributes, "Dexterity") + calculateEquippedArmorClassBonus(inventory, items));
        summary.damageReduction = calculateEquippedDamageReduction(inventory, items);
        if (input.naturalArmorClass > 0)
            summary.armorClass = std::max(summary.armorClass, input.naturalArmorClass);
        return summary;
    }

inline int IRuleSet::getWeaponAbilityModifier(const std::map<std::string, int>& attributes, const ItemModel& weapon) const
    {
        const RuleWeaponProfile profile = getWeaponProfile(weapon);
        return getAbilityModifier(attributes, profile.primaryAbility.empty() ? "Strength" : profile.primaryAbility);
    }

inline int IRuleSet::getWeaponAttackBonus(const std::map<std::string, int>& attributes, const ItemModel& weapon, int level, RuleProficiencyTier proficiency) const
    {
        return getWeaponAbilityModifier(attributes, weapon) + getProficiencyContribution(level, proficiency) + weapon.weaponAttackBonus;
    }

inline std::string IRuleSet::formatSignedModifier(int value) const
    {
        std::ostringstream stream;
        stream << (value >= 0 ? "+" : "") << value;
        return stream.str();
    }

inline RuleWeaponProfile IRuleSet::getWeaponProfile(const ItemModel& weapon) const
    {
        RuleWeaponProfile profile;
        profile.name = weapon.name;
        profile.primaryAbility = weapon.weaponAbility.empty() ? "Strength" : weapon.weaponAbility;
        profile.damage = weapon.weaponDamage;
        profile.damageType = weapon.weaponDamageType;
        profile.versatileDamage = weapon.weaponVersatileDamage;
        profile.ammunitionType = weapon.ammunitionType;
        profile.mastery = weapon.weaponMastery;
        profile.reachFeet = std::max(0, weapon.weaponReachFeet);
        profile.normalRangeFeet = std::max(profile.reachFeet, weapon.weaponRangeFeet);
        profile.longRangeFeet = std::max(profile.normalRangeFeet, weapon.weaponLongRangeFeet);
        profile.properties = weapon.weaponProperties;
        profile.finesse = std::find(profile.properties.begin(), profile.properties.end(), "Finesse") != profile.properties.end();
        profile.ammunition = std::find(profile.properties.begin(), profile.properties.end(), "Ammunition") != profile.properties.end();
        profile.loading = std::find(profile.properties.begin(), profile.properties.end(), "Loading") != profile.properties.end();
        profile.heavy = std::find(profile.properties.begin(), profile.properties.end(), "Heavy") != profile.properties.end();
        profile.light = std::find(profile.properties.begin(), profile.properties.end(), "Light") != profile.properties.end();
        profile.twoHanded = std::find(profile.properties.begin(), profile.properties.end(), "Two-Handed") != profile.properties.end();
        profile.thrown = std::find(profile.properties.begin(), profile.properties.end(), "Thrown") != profile.properties.end();
        profile.versatile = std::find(profile.properties.begin(), profile.properties.end(), "Versatile") != profile.properties.end() || !profile.versatileDamage.empty();
        if (weapon.weaponAttackKind == "Thrown")
            profile.attackKind = RuleWeaponAttackKind::Thrown;
        else if (weapon.weaponAttackKind == "MeleeOrThrown")
            profile.attackKind = RuleWeaponAttackKind::MeleeOrThrown;
        else if (weapon.weaponAttackKind == "Ranged")
            profile.attackKind = RuleWeaponAttackKind::Ranged;
        else if (weapon.weaponAttackKind == "Melee")
            profile.attackKind = RuleWeaponAttackKind::Melee;
        else
            profile.attackKind = profile.normalRangeFeet > profile.reachFeet ? RuleWeaponAttackKind::Ranged : RuleWeaponAttackKind::Melee;
        return profile;
    }

inline std::vector<MonsterActionModel> IRuleSet::getEquippedWeaponActions(const InventoryModel& inventory, const std::vector<ItemModel>& items, const std::map<std::string, int>& attributes, int level) const
    {
        std::vector<MonsterActionModel> actions;
        for (const InventoryEntryModel& entry : inventory.entries)
        {
            if (!entry.equipped)
                continue;

            const ItemModel* item = findItemById(items, entry.itemId);
            if (!item || !isWeaponItem(*item))
                continue;

            const RuleWeaponProfile profile = getWeaponProfile(*item);
            const int abilityModifier = getWeaponAbilityModifier(attributes, *item);
            MonsterActionModel action;
            action.name = item->name.empty() ? "Weapon Attack" : item->name;
            action.reachFeet = profile.reachFeet;
            action.normalRangeFeet = profile.normalRangeFeet;
            action.rangeFeet = std::max(profile.normalRangeFeet, profile.longRangeFeet);
            action.attackBonus = formatSignedModifier(getWeaponAttackBonus(attributes, *item, level));
            action.damage = profile.damage.empty() ? (item->weaponDamage.empty() ? "manual result" : item->weaponDamage) : profile.damage;
            if (abilityModifier != 0 && action.damage != "manual result")
                action.damage += std::string(abilityModifier > 0 ? " +" : " ") + std::to_string(abilityModifier);
            action.notes = "Equipped weapon using " + profile.primaryAbility + ".";
            if (!profile.properties.empty())
            {
                action.notes += " Properties: ";
                for (std::size_t i = 0; i < profile.properties.size(); ++i)
                {
                    if (i > 0)
                        action.notes += ", ";
                    action.notes += profile.properties[i];
                }
                action.notes += ".";
            }
            if (!profile.mastery.empty())
                action.notes += " Mastery: " + profile.mastery + ".";
            if (!profile.versatileDamage.empty())
                action.notes += " Versatile: " + profile.versatileDamage + ".";
            if (profile.longRangeFeet > profile.normalRangeFeet)
                action.notes += " Range: " + std::to_string(profile.normalRangeFeet) + "/" + std::to_string(profile.longRangeFeet) + " ft.";
            actions.push_back(action);
        }
        return actions;
    }
