#pragma once



#include "Models/Rulesets/DND/DND5e.hpp"



inline RuleArmorSummary DND5e::evaluateArmor(const RuleArmorInput &input) const
{
        RuleArmorSummary summary;
        const InventoryModel emptyInventory;
        const std::vector<ItemModel> emptyItems;
        const InventoryModel &inventory = input.inventory ? *input.inventory : emptyInventory;
        const std::vector<ItemModel> &items = input.items ? *input.items : emptyItems;
        const int dexterityModifier = getAbilityModifier(input.attributes, "Dexterity");
        const int constitutionModifier = getAbilityModifier(input.attributes, "Constitution");
        const int wisdomModifier = getAbilityModifier(input.attributes, "Wisdom");
        int bestBodyArmorClass = std::max(1, 10 + dexterityModifier);
        std::string bestBodyArmorSource = "Unarmored base";
        int shieldBonus = 0;
        int equippedShieldCount = 0;
        int additiveCustomBonus = 0;
        int equippedBodyArmorCount = 0;
        bool hasKnownBodyArmor = false;

        for (const InventoryEntryModel &entry : inventory.entries)
        {
            if (!entry.equipped)
                continue;

            const ItemModel *item = findItemById(items, entry.itemId);
            if (!item || !isArmorItem(*item))
                continue;

            summary.damageReduction += item->damageReduction;
            if (isShield(*item))
            {
                ++equippedShieldCount;
                shieldBonus = std::max(shieldBonus, std::max(0, item->armorClassBonus));
                continue;
            }

            ArmorProfile profile = armorProfileFor(*item);
            if (!profile.known)
            {
                additiveCustomBonus += std::max(0, item->armorClassBonus);
                continue;
            }

            ++equippedBodyArmorCount;
            hasKnownBodyArmor = true;
            int dexterityContribution = dexterityModifier;
            if (profile.maxDexterityBonus >= 0)
                dexterityContribution = std::min(dexterityContribution, profile.maxDexterityBonus);
            if (!profile.usesDexterity)
                dexterityContribution = 0;
            const int armorClass = profile.baseArmorClass + dexterityContribution;
            if (equippedBodyArmorCount == 1 || armorClass > bestBodyArmorClass)
            {
                bestBodyArmorClass = armorClass;
                bestBodyArmorSource = item->name.empty() ? "Body armor" : item->name;
            }
        }

        if (equippedBodyArmorCount > 1)
            summary.warnings.push_back("Multiple body armors are equipped; D&D uses only the best valid armor formula here.");
        if (equippedShieldCount > 1)
            summary.warnings.push_back("Multiple shields are equipped; D&D shield bonuses do not stack, so only the best shield applies.");

        if (!hasKnownBodyArmor && additiveCustomBonus == 0 && input.allowUnarmoredDefense)
        {
            const std::string className = lowerCopy(input.className);
            if (className == "barbarian")
            {
                const int armorClass = 10 + dexterityModifier + constitutionModifier;
                if (armorClass > bestBodyArmorClass)
                {
                    bestBodyArmorClass = armorClass;
                    bestBodyArmorSource = "Barbarian Unarmored Defense";
                }
            }
            else if (className == "monk")
            {
                if (equippedShieldCount > 0)
                {
                    summary.warnings.push_back("Monk Unarmored Defense is ignored while a shield is equipped.");
                }
                else
                {
                    const int armorClass = 10 + dexterityModifier + wisdomModifier;
                    if (armorClass > bestBodyArmorClass)
                    {
                        bestBodyArmorClass = armorClass;
                        bestBodyArmorSource = "Monk Unarmored Defense";
                    }
                }
            }
        }

        if (!hasKnownBodyArmor && input.naturalArmorClass > 0 && input.naturalArmorClass > bestBodyArmorClass)
        {
            bestBodyArmorClass = input.naturalArmorClass;
            bestBodyArmorSource = "Natural Armor";
        }

        summary.armorClass = bestBodyArmorClass + shieldBonus + additiveCustomBonus;
        summary.sources.push_back(bestBodyArmorSource);
        if (shieldBonus > 0)
            summary.sources.push_back("Shield");
        if (additiveCustomBonus > 0)
            summary.sources.push_back("Custom armor bonus");
        if (additiveCustomBonus > 0)
            summary.warnings.push_back("Custom armor without a D&D armor profile is treated as an additive AC bonus.");
        return summary;
    }

inline RuleWeaponProfile DND5e::getWeaponProfile(const ItemModel &weapon) const
{
        RuleWeaponProfile profile;
        profile.name = weapon.name;
        profile.primaryAbility = weapon.weaponAbility.empty() ? "Strength" : weapon.weaponAbility;
        profile.damage = weapon.weaponDamage;
        profile.damageType = weapon.weaponDamageType.empty() ? damageTypeFromDamage(profile.damage) : weapon.weaponDamageType;
        profile.versatileDamage = weapon.weaponVersatileDamage;
        profile.ammunitionType = weapon.ammunitionType;
        profile.mastery = weapon.weaponMastery;
        profile.reachFeet = std::max(0, weapon.weaponReachFeet);
        profile.normalRangeFeet = std::max(profile.reachFeet, weapon.weaponRangeFeet);
        profile.longRangeFeet = std::max(profile.normalRangeFeet, weapon.weaponLongRangeFeet);
        profile.properties = weapon.weaponProperties;
        profile.finesse = hasProperty(profile, "Finesse");
        profile.ammunition = hasProperty(profile, "Ammunition");
        profile.loading = hasProperty(profile, "Loading");
        profile.heavy = hasProperty(profile, "Heavy");
        profile.light = hasProperty(profile, "Light");
        profile.twoHanded = hasProperty(profile, "Two-Handed");
        profile.thrown = hasProperty(profile, "Thrown");
        profile.versatile = hasProperty(profile, "Versatile") || !profile.versatileDamage.empty();
        if (weapon.weaponAttackKind == "Thrown")
            profile.attackKind = RuleWeaponAttackKind::Thrown;
        else if (weapon.weaponAttackKind == "MeleeOrThrown")
            profile.attackKind = RuleWeaponAttackKind::MeleeOrThrown;
        else if (weapon.weaponAttackKind == "Ranged")
            profile.attackKind = RuleWeaponAttackKind::Ranged;
        else if (weapon.weaponAttackKind == "Melee")
            profile.attackKind = RuleWeaponAttackKind::Melee;

        const std::string normalizedName = slugify(weapon.name);
        applyWeaponProfile(normalizedName, profile);
        if (profile.finesse)
            profile.primaryAbility = "Finesse";
        if (profile.thrown && (profile.attackKind == RuleWeaponAttackKind::Melee || profile.attackKind == RuleWeaponAttackKind::Unknown))
            profile.attackKind = RuleWeaponAttackKind::MeleeOrThrown;
        if (profile.attackKind == RuleWeaponAttackKind::Unknown)
            profile.attackKind = profile.normalRangeFeet > profile.reachFeet ? RuleWeaponAttackKind::Ranged : RuleWeaponAttackKind::Melee;
        if (profile.attackKind == RuleWeaponAttackKind::Ranged && profile.normalRangeFeet <= profile.reachFeet)
            profile.normalRangeFeet = std::max(20, profile.reachFeet);
        if (profile.longRangeFeet < profile.normalRangeFeet)
            profile.longRangeFeet = profile.normalRangeFeet;
        return profile;
    }
