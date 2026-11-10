#include "Models/Persistence/DatabaseSchemaBuilder.hpp"

#include <string>

void DatabaseSchemaBuilder::createContentTables()
{
const std::string createItemsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS custom_items (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            campaign_id INTEGER NOT NULL,
            name        TEXT NOT NULL,
            description TEXT,
            ruleset_key TEXT,
            category    TEXT,
            item_type   TEXT,
            rarity      INTEGER,
            weight      REAL,
            base_value  INTEGER,
            stackable   INTEGER,
            armor_class_bonus INTEGER DEFAULT 0,
            damage_reduction INTEGER DEFAULT 0,
            weapon_attack_bonus INTEGER DEFAULT 0,
            weapon_damage TEXT,
            weapon_damage_type TEXT,
            weapon_ability TEXT,
            weapon_reach_feet INTEGER DEFAULT 5,
            weapon_range_feet INTEGER DEFAULT 5,
            weapon_long_range_feet INTEGER DEFAULT 5,
            weapon_attack_kind TEXT,
            weapon_mastery TEXT,
            weapon_versatile_damage TEXT,
            ammunition_type TEXT,
            weapon_properties TEXT,
            is_container INTEGER DEFAULT 0,
            container_capacity_weight REAL DEFAULT 0,
            container_capacity_items INTEGER DEFAULT 0,
            requires_attunement INTEGER DEFAULT 0,
            max_charges INTEGER DEFAULT 0,
            recharge_rule TEXT,
            cursed INTEGER DEFAULT 0,
            activation TEXT,
            linked_spell_id TEXT,
            command_words TEXT,
            consumable INTEGER DEFAULT 0,
            is_ammunition INTEGER DEFAULT 0,
            max_durability INTEGER DEFAULT 0,
            effects     TEXT,
            item_powers TEXT,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createItemsTableSQL);
    addColumnIfMissing("custom_items", "category", "TEXT");
    addColumnIfMissing("custom_items", "armor_class_bonus", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_items", "damage_reduction", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_items", "weapon_attack_bonus", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_items", "weapon_damage", "TEXT");
    addColumnIfMissing("custom_items", "weapon_damage_type", "TEXT");
    addColumnIfMissing("custom_items", "weapon_ability", "TEXT");
    addColumnIfMissing("custom_items", "weapon_reach_feet", "INTEGER DEFAULT 5");
    addColumnIfMissing("custom_items", "weapon_range_feet", "INTEGER DEFAULT 5");
    addColumnIfMissing("custom_items", "weapon_long_range_feet", "INTEGER DEFAULT 5");
    addColumnIfMissing("custom_items", "weapon_attack_kind", "TEXT");
    addColumnIfMissing("custom_items", "weapon_mastery", "TEXT");
    addColumnIfMissing("custom_items", "weapon_versatile_damage", "TEXT");
    addColumnIfMissing("custom_items", "ammunition_type", "TEXT");
    addColumnIfMissing("custom_items", "weapon_properties", "TEXT");
    addColumnIfMissing("custom_items", "is_container", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_items", "container_capacity_weight", "REAL DEFAULT 0");
    addColumnIfMissing("custom_items", "container_capacity_items", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_items", "requires_attunement", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_items", "max_charges", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_items", "recharge_rule", "TEXT");
    addColumnIfMissing("custom_items", "cursed", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_items", "activation", "TEXT");
    addColumnIfMissing("custom_items", "linked_spell_id", "TEXT");
    addColumnIfMissing("custom_items", "command_words", "TEXT");
    addColumnIfMissing("custom_items", "consumable", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_items", "is_ammunition", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_items", "max_durability", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_items", "effects", "TEXT");
    addColumnIfMissing("custom_items", "item_powers", "TEXT");

    const std::string createSpellsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS custom_spells (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            campaign_id INTEGER NOT NULL,
            name        TEXT NOT NULL,
            spell_level INTEGER DEFAULT 0,
            school      INTEGER DEFAULT 4,
            resolution  INTEGER DEFAULT 0,
            casting_time TEXT,
            spell_range TEXT,
            duration    TEXT,
            concentration INTEGER DEFAULT 0,
            ritual      INTEGER DEFAULT 0,
            verbal      INTEGER DEFAULT 1,
            somatic     INTEGER DEFAULT 1,
            material    INTEGER DEFAULT 0,
            material_description TEXT,
            classes     TEXT,
            attack_bonus TEXT,
            saving_throw_ability TEXT,
            damage      TEXT,
            healing     TEXT,
            area        TEXT,
            effect_summary TEXT,
            scaling     TEXT,
            description TEXT,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createSpellsTableSQL);
}
