#pragma once



#include "Models/Rulesets/DND/DND5e.hpp"



inline bool DND5e::isShield(const ItemModel &item)
{
        return item.name == "Shield" || item.itemType == "Shield";
    }

inline DND5e::ArmorProfile DND5e::armorProfileFor(const ItemModel &item)
{
        if (item.name == "Padded Armor" || item.name == "Leather Armor")
            return {true, 11, true, -1};
        if (item.name == "Studded Leather Armor")
            return {true, 12, true, -1};
        if (item.name == "Hide Armor")
            return {true, 12, true, 2};
        if (item.name == "Chain Shirt")
            return {true, 13, true, 2};
        if (item.name == "Scale Mail" || item.name == "Breastplate")
            return {true, 14, true, 2};
        if (item.name == "Half Plate Armor")
            return {true, 15, true, 2};
        if (item.name == "Ring Mail")
            return {true, 14, false, 0};
        if (item.name == "Chain Mail")
            return {true, 16, false, 0};
        if (item.name == "Splint Armor")
            return {true, 17, false, 0};
        if (item.name == "Plate Armor")
            return {true, 18, false, 0};
        return {};
    }

inline MonsterActionModel DND5e::makeAction(const std::string &name, int reachFeet, int rangeFeet, const std::string &attackBonus, const std::string &damage, const std::string &notes)
{
        MonsterActionModel action;
        action.name = name;
        action.reachFeet = reachFeet;
        action.rangeFeet = rangeFeet;
        action.normalRangeFeet = rangeFeet;
        action.attackBonus = attackBonus;
        action.damage = damage;
        action.notes = notes;
        return action;
    }

inline MonsterDefinitionModel DND5e::makeMonster( const std::string &name, const std::string &size, const std::string &type, const std::string &challengeRating, int armorClass, int hitPoints, int speedFeet, int experience, const std::map<std::string, int> &attributes, const std::vector<MonsterActionModel> &actions)
{
        MonsterDefinitionModel monster;
        monster.metadata.id = "dnd.srd.monster." + slugify(name);
        monster.metadata.createdAtUtc = "";
        monster.metadata.updatedAtUtc = "";
        monster.name = name;
        monster.source = "SRD reference";
        monster.size = size;
        monster.type = type;
        monster.challengeRating = challengeRating;
        monster.armorClass = armorClass;
        monster.armorDamageReduction = std::max(0, (armorClass - 10) / 4);
        monster.hitPoints = hitPoints;
        monster.speedFeet = speedFeet;
        monster.experience = experience;
        monster.attributes = attributes;
        monster.actions = actions;
        return monster;
    }

inline ItemModel DND5e::makeItem(const std::string &name, const std::string &type, int valueCopper, double weight, bool stackable, int armorClassBonus, int damageReduction)
{
        ItemModel item;
        item.metadata.id = "dnd2024.item." + slugify(name);
        item.metadata.createdAtUtc = "";
        item.metadata.updatedAtUtc = "";
        item.name = name;
        item.rulesetKey = "dnd2024";
        item.category = type;
        item.itemType = type;
        item.baseValue = valueCopper;
        item.weight = weight;
        item.stackable = stackable;
        item.armorClassBonus = armorClassBonus;
        item.damageReduction = damageReduction;
        return item;
    }

inline ItemModel DND5e::makeWeapon(const std::string &name, int valueCopper, double weight, bool stackable, const std::string &damage, const std::string &ability, int reachFeet, int rangeFeet, int attackBonus)
{
        ItemModel item = makeItem(name, "Weapon", valueCopper, weight, stackable);
        item.weaponDamage = damage;
        item.weaponAbility = ability;
        item.weaponReachFeet = reachFeet;
        item.weaponRangeFeet = rangeFeet;
        item.weaponAttackBonus = attackBonus;
        return item;
    }

inline SpellModel DND5e::makeSpell( const std::string &name, int level, SpellSchool school, SpellResolutionType resolution, const std::string &castingTime, const std::string &range, const std::string &duration, const std::string &classes, const std::string &damage, const std::string &healing, const std::string &savingThrowAbility, const std::string &area, const std::string &effectSummary, const std::string &scaling, bool concentration, bool ritual)
{
        SpellModel spell;
        spell.metadata.id = "dnd.srd.spell." + slugify(name);
        spell.metadata.createdAtUtc = "";
        spell.metadata.updatedAtUtc = "";
        spell.metadata.tags = {"srd", "ruleset"};
        spell.name = name;
        spell.level = level;
        spell.school = school;
        spell.resolution = resolution;
        spell.castingTime = castingTime;
        spell.range = range;
        spell.duration = duration;
        spell.concentration = concentration;
        spell.ritual = ritual;
        spell.classes = classes;
        spell.damage = damage;
        spell.healing = healing;
        spell.savingThrowAbility = savingThrowAbility;
        spell.area = area;
        spell.effectSummary = effectSummary;
        spell.scaling = scaling;
        spell.description = "Ruleset catalog metadata for table automation; review campaign-specific text before publishing or exporting.";
        return spell;
    }

inline std::string DND5e::slugify(std::string value)
{
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            if (c >= 'A' && c <= 'Z')
                return static_cast<char>(c - 'A' + 'a');
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                return static_cast<char>(c);
            return '-';
        });

        value.erase(std::unique(value.begin(), value.end(), [](char a, char b) { return a == '-' && b == '-'; }), value.end());
        while (!value.empty() && value.front() == '-')
            value.erase(value.begin());
        while (!value.empty() && value.back() == '-')
            value.pop_back();
        return value;
    }
