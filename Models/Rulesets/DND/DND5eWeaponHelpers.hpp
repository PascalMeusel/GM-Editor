#pragma once



#include "Models/Rulesets/DND/DND5e.hpp"



inline std::string DND5e::damageTypeFromDamage(const std::string &damage)
{
        const std::vector<std::string> types{"acid", "bludgeoning", "cold", "fire", "force", "lightning", "necrotic", "piercing", "poison", "psychic", "radiant", "slashing", "thunder"};
        const std::string lower = lowerCopy(damage);
        for (const std::string &type : types)
        {
            if (lower.find(type) != std::string::npos)
                return type;
        }
        return "";
    }

inline std::string DND5e::lowerCopy(std::string value)
{
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

inline void DND5e::addProperty(RuleWeaponProfile &profile, const std::string &property)
{
        if (std::find(profile.properties.begin(), profile.properties.end(), property) == profile.properties.end())
            profile.properties.push_back(property);
    }

inline bool DND5e::hasProperty(const RuleWeaponProfile &profile, const std::string &property)
{
        return std::find(profile.properties.begin(), profile.properties.end(), property) != profile.properties.end();
    }

inline void DND5e::setRange(RuleWeaponProfile &profile, int normalRangeFeet, int longRangeFeet)
{
        profile.normalRangeFeet = normalRangeFeet;
        profile.longRangeFeet = std::max(normalRangeFeet, longRangeFeet);
    }

inline void DND5e::applyCommonProperties(RuleWeaponProfile &profile)
{
        if (profile.finesse)
            addProperty(profile, "Finesse");
        if (profile.ammunition)
            addProperty(profile, "Ammunition");
        if (profile.loading)
            addProperty(profile, "Loading");
        if (profile.heavy)
            addProperty(profile, "Heavy");
        if (profile.light)
            addProperty(profile, "Light");
        if (profile.twoHanded)
            addProperty(profile, "Two-Handed");
        if (profile.thrown)
            addProperty(profile, "Thrown");
        if (profile.versatile)
            addProperty(profile, "Versatile");
    }

inline void DND5e::applyWeaponProfile(const std::string &id, RuleWeaponProfile &profile)
{
        static const std::set<std::string> rangedWeapons{
            "blowgun", "crossbow-hand", "crossbow-heavy", "crossbow-light", "longbow", "musket", "pistol", "shortbow", "sling"};
        if (rangedWeapons.count(id) > 0)
        {
            profile.attackKind = RuleWeaponAttackKind::Ranged;
            profile.primaryAbility = "Dexterity";
        }

        if (id == "club")
        {
            profile.light = true;
            profile.mastery = "Slow";
        }
        else if (id == "dagger")
        {
            profile.finesse = true;
            profile.light = true;
            profile.thrown = true;
            setRange(profile, 20, 60);
            profile.mastery = "Nick";
        }
        else if (id == "greatclub")
        {
            profile.twoHanded = true;
            profile.mastery = "Push";
        }
        else if (id == "handaxe")
        {
            profile.light = true;
            profile.thrown = true;
            setRange(profile, 20, 60);
            profile.mastery = "Vex";
        }
        else if (id == "javelin")
        {
            profile.thrown = true;
            setRange(profile, 30, 120);
            profile.mastery = "Slow";
        }
        else if (id == "light-hammer")
        {
            profile.light = true;
            profile.thrown = true;
            setRange(profile, 20, 60);
            profile.mastery = "Nick";
        }
        else if (id == "mace")
            profile.mastery = "Sap";
        else if (id == "quarterstaff")
        {
            profile.versatile = true;
            profile.versatileDamage = "1d8 bludgeoning";
            profile.mastery = "Topple";
        }
        else if (id == "sickle")
        {
            profile.light = true;
            profile.mastery = "Nick";
        }
        else if (id == "spear")
        {
            profile.thrown = true;
            profile.versatile = true;
            profile.versatileDamage = "1d8 piercing";
            setRange(profile, 20, 60);
            profile.mastery = "Sap";
        }
        else if (id == "crossbow-light")
        {
            profile.ammunition = true;
            profile.loading = true;
            profile.twoHanded = true;
            profile.ammunitionType = "Crossbow Bolts";
            setRange(profile, 80, 320);
            profile.mastery = "Slow";
        }
        else if (id == "dart")
        {
            profile.finesse = true;
            profile.thrown = true;
            setRange(profile, 20, 60);
            profile.mastery = "Vex";
        }
        else if (id == "shortbow")
        {
            profile.ammunition = true;
            profile.twoHanded = true;
            profile.ammunitionType = "Arrows";
            setRange(profile, 80, 320);
            profile.mastery = "Vex";
        }
        else if (id == "sling")
        {
            profile.ammunition = true;
            profile.ammunitionType = "Sling Bullets";
            setRange(profile, 30, 120);
            profile.mastery = "Slow";
        }
        else if (id == "battleaxe")
        {
            profile.versatile = true;
            profile.versatileDamage = "1d10 slashing";
            profile.mastery = "Topple";
        }
        else if (id == "flail")
            profile.mastery = "Sap";
        else if (id == "glaive")
        {
            profile.heavy = true;
            profile.reachFeet = 10;
            profile.twoHanded = true;
            profile.mastery = "Graze";
        }
        else if (id == "greataxe")
        {
            profile.heavy = true;
            profile.twoHanded = true;
            profile.mastery = "Cleave";
        }
        else if (id == "greatsword")
        {
            profile.heavy = true;
            profile.twoHanded = true;
            profile.mastery = "Graze";
        }
        else if (id == "halberd")
        {
            profile.heavy = true;
            profile.reachFeet = 10;
            profile.twoHanded = true;
            profile.mastery = "Cleave";
        }
        else if (id == "lance")
        {
            profile.reachFeet = 10;
            profile.mastery = "Topple";
        }
        else if (id == "longsword")
        {
            profile.versatile = true;
            profile.versatileDamage = "1d10 slashing";
            profile.mastery = "Sap";
        }
        else if (id == "maul")
        {
            profile.heavy = true;
            profile.twoHanded = true;
            profile.mastery = "Topple";
        }
        else if (id == "morningstar")
            profile.mastery = "Sap";
        else if (id == "pike")
        {
            profile.heavy = true;
            profile.reachFeet = 10;
            profile.twoHanded = true;
            profile.mastery = "Push";
        }
        else if (id == "rapier")
        {
            profile.finesse = true;
            profile.mastery = "Vex";
        }
        else if (id == "scimitar")
        {
            profile.finesse = true;
            profile.light = true;
            profile.mastery = "Nick";
        }
        else if (id == "shortsword")
        {
            profile.finesse = true;
            profile.light = true;
            profile.mastery = "Vex";
        }
        else if (id == "trident")
        {
            profile.thrown = true;
            profile.versatile = true;
            profile.versatileDamage = "1d8 piercing";
            setRange(profile, 20, 60);
            profile.mastery = "Topple";
        }
        else if (id == "war-pick")
            profile.mastery = "Sap";
        else if (id == "warhammer")
        {
            profile.versatile = true;
            profile.versatileDamage = "1d10 bludgeoning";
            profile.mastery = "Push";
        }
        else if (id == "whip")
        {
            profile.finesse = true;
            profile.reachFeet = 10;
            profile.mastery = "Slow";
        }
        else if (id == "blowgun")
        {
            profile.ammunition = true;
            profile.loading = true;
            profile.ammunitionType = "Blowgun Needles";
            setRange(profile, 25, 100);
            profile.mastery = "Vex";
        }
        else if (id == "crossbow-hand")
        {
            profile.ammunition = true;
            profile.light = true;
            profile.loading = true;
            profile.ammunitionType = "Crossbow Bolts";
            setRange(profile, 30, 120);
            profile.mastery = "Vex";
        }
        else if (id == "crossbow-heavy")
        {
            profile.ammunition = true;
            profile.heavy = true;
            profile.loading = true;
            profile.twoHanded = true;
            profile.ammunitionType = "Crossbow Bolts";
            setRange(profile, 100, 400);
            profile.mastery = "Push";
        }
        else if (id == "longbow")
        {
            profile.ammunition = true;
            profile.heavy = true;
            profile.twoHanded = true;
            profile.ammunitionType = "Arrows";
            setRange(profile, 150, 600);
            profile.mastery = "Slow";
        }

        applyCommonProperties(profile);
    }
