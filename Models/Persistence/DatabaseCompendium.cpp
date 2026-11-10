#include "Models/DataBaseManager.hpp"
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/ProjectLogger.hpp"
#include "Models/Rulesets/RulesetDefinitionSerialization.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "Models/Rulesets/Clockwork/ClockworkBloodline.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "Models/Persistence/DatabaseRows.hpp"

using namespace PersistenceSerialization;

void DatabaseManager::saveCampaignSpellsAndMonsters(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("custom_spells", campaignId, spellIds(campaign.customSpells));

    SQLite::Statement query_insert_spell(*m_db, R"(
            INSERT INTO custom_spells
            (stable_id, campaign_id, name, spell_level, school, resolution, casting_time, spell_range, duration, concentration, ritual, verbal, somatic, material, material_description, classes, attack_bonus, saving_throw_ability, damage, healing, area, effect_summary, scaling, description, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                spell_level = excluded.spell_level,
                school = excluded.school,
                resolution = excluded.resolution,
                casting_time = excluded.casting_time,
                spell_range = excluded.spell_range,
                duration = excluded.duration,
                concentration = excluded.concentration,
                ritual = excluded.ritual,
                verbal = excluded.verbal,
                somatic = excluded.somatic,
                material = excluded.material,
                material_description = excluded.material_description,
                classes = excluded.classes,
                attack_bonus = excluded.attack_bonus,
                saving_throw_ability = excluded.saving_throw_ability,
                damage = excluded.damage,
                healing = excluded.healing,
                area = excluded.area,
                effect_summary = excluded.effect_summary,
                scaling = excluded.scaling,
                description = excluded.description,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const auto &spell : campaign.customSpells)
    {
        query_insert_spell.bind(1, spell.metadata.id);
        query_insert_spell.bind(2, campaignId);
        query_insert_spell.bind(3, spell.name);
        query_insert_spell.bind(4, std::clamp(spell.level, 0, 9));
        query_insert_spell.bind(5, static_cast<int>(spell.school));
        query_insert_spell.bind(6, static_cast<int>(spell.resolution));
        query_insert_spell.bind(7, spell.castingTime);
        query_insert_spell.bind(8, spell.range);
        query_insert_spell.bind(9, spell.duration);
        query_insert_spell.bind(10, spell.concentration ? 1 : 0);
        query_insert_spell.bind(11, spell.ritual ? 1 : 0);
        query_insert_spell.bind(12, spell.verbal ? 1 : 0);
        query_insert_spell.bind(13, spell.somatic ? 1 : 0);
        query_insert_spell.bind(14, spell.material ? 1 : 0);
        query_insert_spell.bind(15, spell.materialDescription);
        query_insert_spell.bind(16, spell.classes);
        query_insert_spell.bind(17, spell.attackBonus);
        query_insert_spell.bind(18, spell.savingThrowAbility);
        query_insert_spell.bind(19, spell.damage);
        query_insert_spell.bind(20, spell.healing);
        query_insert_spell.bind(21, spell.area);
        query_insert_spell.bind(22, spell.effectSummary);
        query_insert_spell.bind(23, spell.scaling);
        query_insert_spell.bind(24, spell.description);
        query_insert_spell.bind(25, serializeStringList(spell.metadata.tags));
        query_insert_spell.bind(26, spell.metadata.createdAtUtc);
        query_insert_spell.bind(27, currentUtcTimestamp());
        query_insert_spell.exec();
        query_insert_spell.reset();
    }

    deleteRowsMissingStableIds("custom_monsters", campaignId, monsterIds(campaign.customMonsters));

    SQLite::Statement query_insert_monster(*m_db, R"(
            INSERT INTO custom_monsters
            (stable_id, campaign_id, catalog_source_id, base_catalog_monster_id, name, source, source_attribution, size, creature_type, challenge_rating, armor_class, armor_damage_reduction, hit_points, speed_feet, experience, traits, reactions, legendary_actions, lair_actions, recharge_actions, senses, languages, saving_throws, skills, damage_resistances, damage_immunities, damage_vulnerabilities, condition_immunities, encounter_role_tags, attributes, actions, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                catalog_source_id = excluded.catalog_source_id,
                base_catalog_monster_id = excluded.base_catalog_monster_id,
                name = excluded.name,
                source = excluded.source,
                source_attribution = excluded.source_attribution,
                size = excluded.size,
                creature_type = excluded.creature_type,
                challenge_rating = excluded.challenge_rating,
                armor_class = excluded.armor_class,
                armor_damage_reduction = excluded.armor_damage_reduction,
                hit_points = excluded.hit_points,
                speed_feet = excluded.speed_feet,
                experience = excluded.experience,
                traits = excluded.traits,
                reactions = excluded.reactions,
                legendary_actions = excluded.legendary_actions,
                lair_actions = excluded.lair_actions,
                recharge_actions = excluded.recharge_actions,
                senses = excluded.senses,
                languages = excluded.languages,
                saving_throws = excluded.saving_throws,
                skills = excluded.skills,
                damage_resistances = excluded.damage_resistances,
                damage_immunities = excluded.damage_immunities,
                damage_vulnerabilities = excluded.damage_vulnerabilities,
                condition_immunities = excluded.condition_immunities,
                encounter_role_tags = excluded.encounter_role_tags,
                attributes = excluded.attributes,
                actions = excluded.actions,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const auto &monster : campaign.customMonsters)
    {
        query_insert_monster.bind(1, monster.metadata.id);
        query_insert_monster.bind(2, campaignId);
        query_insert_monster.bind(3, monster.catalogSourceId);
        query_insert_monster.bind(4, monster.baseCatalogMonsterId);
        query_insert_monster.bind(5, monster.name);
        query_insert_monster.bind(6, monster.source);
        query_insert_monster.bind(7, monster.sourceAttribution);
        query_insert_monster.bind(8, monster.size);
        query_insert_monster.bind(9, monster.type);
        query_insert_monster.bind(10, monster.challengeRating);
        query_insert_monster.bind(11, monster.armorClass);
        query_insert_monster.bind(12, monster.armorDamageReduction);
        query_insert_monster.bind(13, monster.hitPoints);
        query_insert_monster.bind(14, monster.speedFeet);
        query_insert_monster.bind(15, monster.experience);
        query_insert_monster.bind(16, monster.traits);
        query_insert_monster.bind(17, monster.reactions);
        query_insert_monster.bind(18, monster.legendaryActions);
        query_insert_monster.bind(19, monster.lairActions);
        query_insert_monster.bind(20, monster.rechargeActions);
        query_insert_monster.bind(21, monster.senses);
        query_insert_monster.bind(22, monster.languages);
        query_insert_monster.bind(23, monster.savingThrows);
        query_insert_monster.bind(24, monster.skills);
        query_insert_monster.bind(25, monster.damageResistances);
        query_insert_monster.bind(26, monster.damageImmunities);
        query_insert_monster.bind(27, monster.damageVulnerabilities);
        query_insert_monster.bind(28, monster.conditionImmunities);
        query_insert_monster.bind(29, serializeStringList(monster.encounterRoleTags));
        query_insert_monster.bind(30, serializeAttributes(monster.attributes));
        query_insert_monster.bind(31, serializeMonsterActions(monster.actions));
        query_insert_monster.bind(32, serializeStringList(monster.metadata.tags));
        query_insert_monster.bind(33, monster.metadata.createdAtUtc);
        query_insert_monster.bind(34, currentUtcTimestamp());

        query_insert_monster.exec();
        query_insert_monster.reset();
    }
}

void DatabaseManager::loadCampaignSpellsAndMonsters(Campaign &campaign)
{
    SQLite::Statement query_spells(*m_db, R"(
                    SELECT stable_id, name, spell_level, school, resolution, casting_time, spell_range, duration, concentration, ritual, verbal, somatic, material, material_description, classes, attack_bonus, saving_throw_ability, damage, healing, area, effect_summary, scaling, description, tags, created_at, updated_at
                    FROM custom_spells
                    WHERE campaign_id = ?
                    ORDER BY spell_level ASC, name ASC;
                )");
    query_spells.bind(1, static_cast<int64_t>(campaign.id));

    while (query_spells.executeStep())
    {
        SpellModel spell;
        if (!query_spells.getColumn(0).isNull())
            spell.metadata.id = query_spells.getColumn(0).getString();
        spell.name = query_spells.getColumn(1).getString();
        if (!query_spells.getColumn(2).isNull())
            spell.level = std::clamp(query_spells.getColumn(2).getInt(), 0, 9);
        if (!query_spells.getColumn(3).isNull())
            spell.school = static_cast<SpellSchool>(query_spells.getColumn(3).getInt());
        if (!query_spells.getColumn(4).isNull())
            spell.resolution = static_cast<SpellResolutionType>(query_spells.getColumn(4).getInt());
        if (!query_spells.getColumn(5).isNull())
            spell.castingTime = query_spells.getColumn(5).getString();
        if (!query_spells.getColumn(6).isNull())
            spell.range = query_spells.getColumn(6).getString();
        if (!query_spells.getColumn(7).isNull())
            spell.duration = query_spells.getColumn(7).getString();
        if (!query_spells.getColumn(8).isNull())
            spell.concentration = query_spells.getColumn(8).getInt() != 0;
        if (!query_spells.getColumn(9).isNull())
            spell.ritual = query_spells.getColumn(9).getInt() != 0;
        if (!query_spells.getColumn(10).isNull())
            spell.verbal = query_spells.getColumn(10).getInt() != 0;
        if (!query_spells.getColumn(11).isNull())
            spell.somatic = query_spells.getColumn(11).getInt() != 0;
        if (!query_spells.getColumn(12).isNull())
            spell.material = query_spells.getColumn(12).getInt() != 0;
        if (!query_spells.getColumn(13).isNull())
            spell.materialDescription = query_spells.getColumn(13).getString();
        if (!query_spells.getColumn(14).isNull())
            spell.classes = query_spells.getColumn(14).getString();
        if (!query_spells.getColumn(15).isNull())
            spell.attackBonus = query_spells.getColumn(15).getString();
        if (!query_spells.getColumn(16).isNull())
            spell.savingThrowAbility = query_spells.getColumn(16).getString();
        if (!query_spells.getColumn(17).isNull())
            spell.damage = query_spells.getColumn(17).getString();
        if (!query_spells.getColumn(18).isNull())
            spell.healing = query_spells.getColumn(18).getString();
        if (!query_spells.getColumn(19).isNull())
            spell.area = query_spells.getColumn(19).getString();
        if (!query_spells.getColumn(20).isNull())
            spell.effectSummary = query_spells.getColumn(20).getString();
        if (!query_spells.getColumn(21).isNull())
            spell.scaling = query_spells.getColumn(21).getString();
        if (!query_spells.getColumn(22).isNull())
            spell.description = query_spells.getColumn(22).getString();
        if (!query_spells.getColumn(23).isNull())
            spell.metadata.tags = deserializeStringList(query_spells.getColumn(23).getString());
        if (!query_spells.getColumn(24).isNull())
            spell.metadata.createdAtUtc = query_spells.getColumn(24).getString();
        if (!query_spells.getColumn(25).isNull())
            spell.metadata.updatedAtUtc = query_spells.getColumn(25).getString();
        campaign.customSpells.push_back(spell);
    }

    SQLite::Statement query_monsters(*m_db, R"(
                    SELECT stable_id, catalog_source_id, base_catalog_monster_id, name, source, source_attribution, size, creature_type, challenge_rating, armor_class, armor_damage_reduction, hit_points, speed_feet, experience, traits, reactions, legendary_actions, lair_actions, recharge_actions, senses, languages, saving_throws, skills, damage_resistances, damage_immunities, damage_vulnerabilities, condition_immunities, encounter_role_tags, attributes, actions, tags, created_at, updated_at
                    FROM custom_monsters
                    WHERE campaign_id = ?
                    ORDER BY name ASC;
                )");
    query_monsters.bind(1, static_cast<int64_t>(campaign.id));

    while (query_monsters.executeStep())
    {
        MonsterDefinitionModel monster;
        if (!query_monsters.getColumn(0).isNull())
            monster.metadata.id = query_monsters.getColumn(0).getString();
        if (!query_monsters.getColumn(1).isNull())
            monster.catalogSourceId = query_monsters.getColumn(1).getString();
        if (!query_monsters.getColumn(2).isNull())
            monster.baseCatalogMonsterId = query_monsters.getColumn(2).getString();
        monster.name = query_monsters.getColumn(3).getString();
        if (!query_monsters.getColumn(4).isNull())
            monster.source = query_monsters.getColumn(4).getString();
        if (!query_monsters.getColumn(5).isNull())
            monster.sourceAttribution = query_monsters.getColumn(5).getString();
        if (!query_monsters.getColumn(6).isNull())
            monster.size = query_monsters.getColumn(6).getString();
        if (!query_monsters.getColumn(7).isNull())
            monster.type = query_monsters.getColumn(7).getString();
        if (!query_monsters.getColumn(8).isNull())
            monster.challengeRating = query_monsters.getColumn(8).getString();
        if (!query_monsters.getColumn(9).isNull())
            monster.armorClass = query_monsters.getColumn(9).getInt();
        if (!query_monsters.getColumn(10).isNull())
            monster.armorDamageReduction = query_monsters.getColumn(10).getInt();
        if (!query_monsters.getColumn(11).isNull())
            monster.hitPoints = query_monsters.getColumn(11).getInt();
        if (!query_monsters.getColumn(12).isNull())
            monster.speedFeet = query_monsters.getColumn(12).getInt();
        if (!query_monsters.getColumn(13).isNull())
            monster.experience = query_monsters.getColumn(13).getInt();
        if (!query_monsters.getColumn(14).isNull())
            monster.traits = query_monsters.getColumn(14).getString();
        if (!query_monsters.getColumn(15).isNull())
            monster.reactions = query_monsters.getColumn(15).getString();
        if (!query_monsters.getColumn(16).isNull())
            monster.legendaryActions = query_monsters.getColumn(16).getString();
        if (!query_monsters.getColumn(17).isNull())
            monster.lairActions = query_monsters.getColumn(17).getString();
        if (!query_monsters.getColumn(18).isNull())
            monster.rechargeActions = query_monsters.getColumn(18).getString();
        if (!query_monsters.getColumn(19).isNull())
            monster.senses = query_monsters.getColumn(19).getString();
        if (!query_monsters.getColumn(20).isNull())
            monster.languages = query_monsters.getColumn(20).getString();
        if (!query_monsters.getColumn(21).isNull())
            monster.savingThrows = query_monsters.getColumn(21).getString();
        if (!query_monsters.getColumn(22).isNull())
            monster.skills = query_monsters.getColumn(22).getString();
        if (!query_monsters.getColumn(23).isNull())
            monster.damageResistances = query_monsters.getColumn(23).getString();
        if (!query_monsters.getColumn(24).isNull())
            monster.damageImmunities = query_monsters.getColumn(24).getString();
        if (!query_monsters.getColumn(25).isNull())
            monster.damageVulnerabilities = query_monsters.getColumn(25).getString();
        if (!query_monsters.getColumn(26).isNull())
            monster.conditionImmunities = query_monsters.getColumn(26).getString();
        if (!query_monsters.getColumn(27).isNull())
            monster.encounterRoleTags = deserializeStringList(query_monsters.getColumn(27).getString());
        if (!query_monsters.getColumn(28).isNull())
            monster.attributes = deserializeAttributes(query_monsters.getColumn(28).getString());
        if (!query_monsters.getColumn(29).isNull())
            monster.actions = deserializeMonsterActions(query_monsters.getColumn(29).getString());
        if (!query_monsters.getColumn(30).isNull())
            monster.metadata.tags = deserializeStringList(query_monsters.getColumn(30).getString());
        if (!query_monsters.getColumn(31).isNull())
            monster.metadata.createdAtUtc = query_monsters.getColumn(31).getString();
        if (!query_monsters.getColumn(32).isNull())
            monster.metadata.updatedAtUtc = query_monsters.getColumn(32).getString();

        campaign.customMonsters.push_back(monster);
    }
}
