#include "sqlite3.h"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
void exec(sqlite3 *db, const std::string &sql)
{
    char *error = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK)
    {
        std::string message = error ? error : "unknown sqlite error";
        sqlite3_free(error);
        throw std::runtime_error(message + "\nSQL:\n" + sql);
    }
}

bool columnExists(sqlite3 *db, const std::string &tableName, const std::string &columnName)
{
    sqlite3_stmt *statement = nullptr;
    const std::string sql = "PRAGMA table_info(" + tableName + ");";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Could not inspect table: " + tableName);

    bool found = false;
    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        const unsigned char *text = sqlite3_column_text(statement, 1);
        if (text && columnName == reinterpret_cast<const char *>(text))
        {
            found = true;
            break;
        }
    }
    sqlite3_finalize(statement);
    return found;
}

void addColumnIfMissing(sqlite3 *db, const std::string &tableName, const std::string &columnName, const std::string &definition)
{
    if (!columnExists(db, tableName, columnName))
        exec(db, "ALTER TABLE " + tableName + " ADD COLUMN " + columnName + " " + definition + ";");
}

std::string quote(const std::string &value)
{
    std::string escaped = "'";
    for (const char c : value)
    {
        if (c == '\'')
            escaped += "''";
        else
            escaped += c;
    }
    escaped += "'";
    return escaped;
}

std::string attrs(int str, int dex, int con, int intl, int wis, int cha)
{
    std::ostringstream out;
    out << "Strength:" << str << ";Dexterity:" << dex << ";Constitution:" << con
        << ";Intelligence:" << intl << ";Wisdom:" << wis << ";Charisma:" << cha << ";";
    return out.str();
}

std::string inv(std::initializer_list<std::string> entries)
{
    std::string result;
    for (const auto &entry : entries)
        result += entry + ";";
    return result;
}

std::string slots(std::initializer_list<int> values)
{
    std::ostringstream out;
    int index = 0;
    for (const int value : values)
    {
        if (index++ > 0)
            out << ",";
        out << value;
    }
    return out.str();
}

std::string spellbook(std::initializer_list<std::string> spellIds)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';
    std::string result;
    for (const auto &spellId : spellIds)
    {
        result += spellId;
        result += fieldSeparator;
        result += "1";
        result += fieldSeparator;
        result += "Campaign";
        result += fieldSeparator;
        result += "Prepared for the feature tour.";
        result += entrySeparator;
    }
    return result;
}

std::string actions(std::initializer_list<std::string> entries)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char actionSeparator = '\x1e';
    std::string result;
    for (const auto &entry : entries)
    {
        result += entry;
        result += actionSeparator;
    }
    return result;
}

std::string action(const std::string &name, int reach, int range, const std::string &attack, const std::string &damage, const std::string &notes = "")
{
    constexpr char fieldSeparator = '\x1f';
    return name + fieldSeparator + std::to_string(reach) + fieldSeparator + std::to_string(range) + fieldSeparator + attack + fieldSeparator + damage + fieldSeparator + notes;
}

std::string packed(std::initializer_list<std::string> fields)
{
    constexpr char fieldSeparator = '\x1f';
    std::string result;
    bool first = true;
    for (const auto &field : fields)
    {
        if (!first)
            result += fieldSeparator;
        first = false;
        result += field;
    }
    return result;
}

std::string packedEntries(std::initializer_list<std::string> entries)
{
    constexpr char entrySeparator = '\x1e';
    std::string result;
    for (const auto &entry : entries)
    {
        result += entry;
        result += entrySeparator;
    }
    return result;
}

std::string combatLogEntry(const std::string &id, int round, const std::string &actorName, const std::string &eventType, const std::string &summary)
{
    return packed({id, std::to_string(round), actorName, eventType, summary, "datetime('now')", "datetime('now')"});
}

std::string dicePreset(const std::string &id, const std::string &name, const std::string &expression, const std::string &notes)
{
    return packed({id, name, expression, notes, "datetime('now')", "datetime('now')"});
}

std::string activeEffect(const std::string &id, int kind, const std::string &name, const std::string &sourceName, const std::string &sourceEntityId, const std::string &sourceSpellId, const std::string &notes, int remainingRounds, bool concentration)
{
    return packed({id, std::to_string(kind), name, sourceName, sourceEntityId, sourceSpellId, notes, std::to_string(remainingRounds), concentration ? "1" : "0", "1", "datetime('now')", "datetime('now')"});
}

std::string roomConnection(const std::string &id, const std::string &targetRoomId, const std::string &label, bool locked, bool secret, bool discovered)
{
    return packed({id, targetRoomId, label, locked ? "1" : "0", secret ? "1" : "0", discovered ? "1" : "0", "datetime('now')", "datetime('now')"});
}

std::string mapRegion(const std::string &id, const std::string &name, const std::string &description, int x, int y, int width, int height, int movementCost, int lighting, bool difficult, bool blocksLineOfSight, bool gmOnly)
{
    return packed({id, name, description, std::to_string(x), std::to_string(y), std::to_string(width), std::to_string(height), std::to_string(movementCost), std::to_string(lighting), difficult ? "1" : "0", blocksLineOfSight ? "1" : "0", gmOnly ? "1" : "0", "datetime('now')", "datetime('now')"});
}

std::string mapFeature(const std::string &id, int kind, const std::string &name, const std::string &description, const std::string &gmNotes, const std::string &linkedEntityId, const std::string &targetMapId, int x, int y, bool discovered, bool gmOnly, int difficultyClass)
{
    return packed({id, std::to_string(kind), name, description, gmNotes, linkedEntityId, targetMapId, std::to_string(x), std::to_string(y), discovered ? "1" : "0", gmOnly ? "1" : "0", std::to_string(difficultyClass), "datetime('now')", "datetime('now')"});
}

std::string mapRoute(const std::string &id, const std::string &name, const std::string &fromMapId, const std::string &toMapId, int startX, int startY, int endX, int endY, float distanceMiles, const std::string &terrain, const std::string &notes, bool discovered)
{
    std::ostringstream miles;
    miles << distanceMiles;
    return packed({id, name, fromMapId, toMapId, std::to_string(startX), std::to_string(startY), std::to_string(endX), std::to_string(endY), miles.str(), terrain, notes, discovered ? "1" : "0", "datetime('now')", "datetime('now')"});
}

void ensureSchema(sqlite3 *db)
{
    addColumnIfMissing(db, "campaigns", "party_vehicles", "TEXT");
    addColumnIfMissing(db, "campaigns", "party_strongholds", "TEXT");
    addColumnIfMissing(db, "campaigns", "party_renown", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "campaigns", "party_reputation", "TEXT");
    addColumnIfMissing(db, "campaigns", "last_modified", "DATETIME DEFAULT CURRENT_TIMESTAMP");
    addColumnIfMissing(db, "campaigns", "travel_route_id", "TEXT");
    addColumnIfMissing(db, "campaigns", "travel_route_name", "TEXT");
    addColumnIfMissing(db, "campaigns", "travel_pace", "TEXT");
    addColumnIfMissing(db, "campaigns", "travel_distance_miles", "REAL DEFAULT 0");
    addColumnIfMissing(db, "campaigns", "travel_miles_traveled", "REAL DEFAULT 0");
    addColumnIfMissing(db, "campaigns", "travel_watches", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "campaigns", "travel_rations_used", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "campaigns", "travel_random_encounter_checks", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "campaigns", "travel_notes", "TEXT");
    addColumnIfMissing(db, "campaigns", "presentation_mode_active", "INTEGER DEFAULT 0");

    addColumnIfMissing(db, "characters", "current_hp", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "characters", "max_hp", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "characters", "portrait_asset_id", "TEXT");
    addColumnIfMissing(db, "characters", "inventory_entries", "TEXT");
    addColumnIfMissing(db, "characters", "conditions", "TEXT");
    addColumnIfMissing(db, "characters", "spellcasting_enabled", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "characters", "spellcasting_ability", "TEXT");
    addColumnIfMissing(db, "characters", "spell_attack_bonus", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "characters", "spell_save_dc", "INTEGER DEFAULT 10");
    addColumnIfMissing(db, "characters", "spell_slots_max", "TEXT");
    addColumnIfMissing(db, "characters", "spell_slots_used", "TEXT");
    addColumnIfMissing(db, "characters", "spellbook_entries", "TEXT");
    addColumnIfMissing(db, "characters", "resource_trackers", "TEXT");

    addColumnIfMissing(db, "npcs", "category", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "npcs", "faction_id", "TEXT");
    addColumnIfMissing(db, "npcs", "deity_id", "TEXT");
    addColumnIfMissing(db, "npcs", "attitude_toward_party", "TEXT");
    addColumnIfMissing(db, "npcs", "secrets", "TEXT");
    addColumnIfMissing(db, "npcs", "goals", "TEXT");
    addColumnIfMissing(db, "npcs", "resources", "TEXT");
    addColumnIfMissing(db, "npcs", "threat_location", "TEXT");
    addColumnIfMissing(db, "npcs", "relationships", "TEXT");
    addColumnIfMissing(db, "npcs", "threat_level", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "npcs", "inventory_entries", "TEXT");
    addColumnIfMissing(db, "npcs", "spellcasting_enabled", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "npcs", "spellcasting_ability", "TEXT");
    addColumnIfMissing(db, "npcs", "spell_attack_bonus", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "npcs", "spell_save_dc", "INTEGER DEFAULT 10");
    addColumnIfMissing(db, "npcs", "spell_slots_max", "TEXT");
    addColumnIfMissing(db, "npcs", "spell_slots_used", "TEXT");
    addColumnIfMissing(db, "npcs", "spellbook_entries", "TEXT");
    addColumnIfMissing(db, "npcs", "resource_trackers", "TEXT");

    addColumnIfMissing(db, "maps", "background_image_path", "TEXT");
    addColumnIfMissing(db, "maps", "background_image_opacity", "REAL DEFAULT 0.65");
    addColumnIfMissing(db, "maps", "regions", "TEXT");
    addColumnIfMissing(db, "maps", "features", "TEXT");
    addColumnIfMissing(db, "maps", "routes", "TEXT");

    addColumnIfMissing(db, "map_tokens", "kind", "INTEGER DEFAULT 5");
    addColumnIfMissing(db, "map_tokens", "linked_entity_type", "TEXT");
    addColumnIfMissing(db, "map_tokens", "layer_id", "TEXT");
    addColumnIfMissing(db, "map_tokens", "size", "REAL DEFAULT 1");

    addColumnIfMissing(db, "encounters", "combat_log", "TEXT");
    addColumnIfMissing(db, "encounters", "dice_roll_presets", "TEXT");
    addColumnIfMissing(db, "encounters", "combat_paused", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "encounters", "theater_of_mind", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "encounters", "after_combat_summary", "TEXT");
    addColumnIfMissing(db, "encounters", "after_combat_loot_notes", "TEXT");
    addColumnIfMissing(db, "encounters", "after_combat_story_consequences", "TEXT");
    addColumnIfMissing(db, "encounters", "after_combat_session_notes", "TEXT");

    addColumnIfMissing(db, "encounter_participants", "temporary_hp", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "encounter_participants", "group_name", "TEXT");
    addColumnIfMissing(db, "encounter_participants", "wave_number", "INTEGER DEFAULT 1");
    addColumnIfMissing(db, "encounter_participants", "starts_hidden", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "encounter_participants", "reinforcement_round", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "encounter_participants", "starting_x", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "encounter_participants", "starting_y", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "encounter_participants", "damage_resistances", "TEXT");
    addColumnIfMissing(db, "encounter_participants", "damage_vulnerabilities", "TEXT");
    addColumnIfMissing(db, "encounter_participants", "damage_immunities", "TEXT");
    addColumnIfMissing(db, "encounter_participants", "active_effects", "TEXT");

    addColumnIfMissing(db, "story_beats", "cue_audio_asset_id", "TEXT");
    addColumnIfMissing(db, "story_beats", "cue_volume", "REAL DEFAULT 1.0");
    addColumnIfMissing(db, "story_beats", "cue_loop", "INTEGER DEFAULT 1");

    addColumnIfMissing(db, "dungeons", "parent_map_id", "TEXT");
    addColumnIfMissing(db, "dungeons", "active_room_id", "TEXT");
    addColumnIfMissing(db, "dungeons", "gm_notes", "TEXT");

    addColumnIfMissing(db, "dungeon_rooms", "keyed_description", "TEXT");
    addColumnIfMissing(db, "dungeon_rooms", "read_aloud_text", "TEXT");
    addColumnIfMissing(db, "dungeon_rooms", "secrets", "TEXT");
    addColumnIfMissing(db, "dungeon_rooms", "traps", "TEXT");
    addColumnIfMissing(db, "dungeon_rooms", "exploration_notes", "TEXT");
    addColumnIfMissing(db, "dungeon_rooms", "exploration_state", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "dungeon_rooms", "map_x", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "dungeon_rooms", "map_y", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "dungeon_rooms", "connections", "TEXT");

    addColumnIfMissing(db, "sessions", "started_at_utc", "TEXT");
    addColumnIfMissing(db, "sessions", "ended_at_utc", "TEXT");
    addColumnIfMissing(db, "sessions", "recap", "TEXT");
    addColumnIfMissing(db, "sessions", "unresolved_threads", "TEXT");
    addColumnIfMissing(db, "sessions", "loot_awarded", "TEXT");
    addColumnIfMissing(db, "sessions", "xp_milestones", "TEXT");
    addColumnIfMissing(db, "sessions", "npcs_met", "TEXT");
    addColumnIfMissing(db, "sessions", "locations_visited", "TEXT");
    addColumnIfMissing(db, "sessions", "open_hooks", "TEXT");

    exec(db, R"(
        CREATE TABLE IF NOT EXISTS session_notes (
            id INTEGER PRIMARY KEY,
            stable_id TEXT UNIQUE,
            session_id INTEGER NOT NULL,
            timestamp_utc TEXT,
            body TEXT NOT NULL,
            linked_entity_id TEXT,
            linked_entity_type TEXT,
            linked_entity_label TEXT,
            tags TEXT,
            created_at TEXT,
            updated_at TEXT,
            FOREIGN KEY(session_id) REFERENCES sessions(id) ON DELETE CASCADE
        );
    )");

    addColumnIfMissing(db, "homebrew_definitions", "source", "TEXT");
    addColumnIfMissing(db, "homebrew_definitions", "imported_at_utc", "TEXT");
    addColumnIfMissing(db, "homebrew_definitions", "enabled", "INTEGER DEFAULT 1");

    exec(db, R"(
        CREATE TABLE IF NOT EXISTS custom_spells (
            id INTEGER PRIMARY KEY,
            stable_id TEXT UNIQUE,
            campaign_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            spell_level INTEGER DEFAULT 0,
            school INTEGER DEFAULT 4,
            resolution INTEGER DEFAULT 0,
            casting_time TEXT,
            spell_range TEXT,
            duration TEXT,
            concentration INTEGER DEFAULT 0,
            ritual INTEGER DEFAULT 0,
            verbal INTEGER DEFAULT 1,
            somatic INTEGER DEFAULT 1,
            material INTEGER DEFAULT 0,
            material_description TEXT,
            classes TEXT,
            attack_bonus TEXT,
            saving_throw_ability TEXT,
            damage TEXT,
            healing TEXT,
            area TEXT,
            effect_summary TEXT,
            scaling TEXT,
            description TEXT,
            tags TEXT,
            created_at TEXT,
            updated_at TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )");
}

void seedDatabase(const std::filesystem::path &path)
{
    sqlite3 *db = nullptr;
    if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK)
        throw std::runtime_error("Could not open database: " + path.string());

    try
    {
        exec(db, "PRAGMA foreign_keys = ON;");
        ensureSchema(db);
        exec(db, "BEGIN TRANSACTION;");

        exec(db, "DELETE FROM campaigns WHERE name IN ('Codex Feature Tour - Full System Check', 'Codex Feature Tour - Current Systems');");

        const std::string campaignId = "campaign_current_feature_tour";
        exec(db,
             "INSERT INTO campaigns (stable_id, name, ruleset_name, description, tags, created_at, updated_at, party_map_id, party_x, party_y, party_vehicles, party_strongholds, party_renown, party_reputation, last_modified) VALUES (" +
                 quote(campaignId) + ", " +
                 quote("Codex Feature Tour - Current Systems") + ", " +
                 quote("Dungeons & Dragons 2024 (5.5e)") + ", " +
                 quote("A seeded QA campaign covering the current app: story arcs, scenes, narrative audio cues, character sheets, portraits, health, inventory, equipment, spells, NPCs, monsters, maps, encounters, and combat.") + ", " +
                 quote("feature-tour;qa;current-stage;") + ", datetime('now'), datetime('now'), " + quote("map_harbor_ward") + ", 4, 5, " +
                 quote("Schooner \"Sea Ledger\";Dockside Signal Mast;") + ", " +
                 quote("Harbor Ward Watchtower;Lighthouse Annex;") + ", 18, " +
                 quote("Known as the party that closed the gate below the harbor.") + ", datetime('now'));");

        const long long campaignRowId = sqlite3_last_insert_rowid(db);
        const std::string campaignRow = std::to_string(campaignRowId);

        exec(db,
             "UPDATE campaigns SET "
             "travel_route_id='route_harbor_to_archive', "
             "travel_route_name='Harbor Ward to Archive Vault', "
             "travel_pace='Careful', "
             "travel_distance_miles=3.5, "
             "travel_miles_traveled=1.25, "
             "travel_watches=1, "
             "travel_rations_used=4, "
             "travel_random_encounter_checks=1, "
             "travel_notes='Use this to test party travel bookkeeping before opening the archive encounter.', "
             "presentation_mode_active=0 "
             "WHERE id=" +
                 campaignRow + ";");

        exec(db,
             "INSERT INTO inventories (stable_id, campaign_id, owner_entity_id, owner_type, name, copper, silver, gold, platinum, tags, created_at, updated_at) VALUES "
             "('inv_party_current_feature_tour'," + campaignRow + ",'" + campaignId + "','party','Party Inventory',120,80,32,4,'shared;campaign;story;',datetime('now'),datetime('now'));");
        exec(db,
             std::string("INSERT INTO inventory_entries (inventory_id, stable_id, item_id, container_entry_id, quantity, equipped, slot, attuned, charges, ammunition_remaining, durability, state_notes, sort_order) ") +
                 "SELECT id, 'party_inv_entry_1', 'item_healing_tonic', '', 3, 0, 'Bandolier', 0, -1, 0, -1, 'Bulk purchased at the harbor dock.', 0 FROM inventories WHERE stable_id='inv_party_current_feature_tour' UNION ALL " +
                 "SELECT id, 'party_inv_entry_2', 'item_gate_key', '', 1, 0, 'Belt', 0, -1, 0, -1, 'Stored in a hidden pocket.', 1 FROM inventories WHERE stable_id='inv_party_current_feature_tour' UNION ALL " +
                 "SELECT id, 'party_inv_entry_3', 'item_field_journal', '', 1, 0, 'Satchel', 0, -1, 0, -1, 'Shared campaign notes.', 2 FROM inventories WHERE stable_id='inv_party_current_feature_tour';");

        exec(db,
             "INSERT INTO custom_items (stable_id, campaign_id, name, description, ruleset_key, category, item_type, rarity, weight, base_value, stackable, armor_class_bonus, damage_reduction, weapon_attack_bonus, weapon_damage, weapon_ability, weapon_reach_feet, weapon_range_feet, effects, tags, created_at, updated_at) VALUES "
             "('item_starforged_rapier'," + campaignRow + ",'Starforged Rapier','A duelist weapon used to test equipped weapon attacks.','dnd2024','Weapon','Weapon',2,2.0,12000,0,0,0,1,'1d8 piercing','Finesse',5,5,'glows near planar gates;','weapon;magic;',datetime('now'),datetime('now')),"
             "('item_moonlit_shield'," + campaignRow + ",'Moonlit Shield','A shield with an obvious equipped armor bonus.','dnd2024','Armor','Armor',1,6.0,9000,0,2,1,0,'','Strength',5,5,'sheds dim light on command;','armor;magic;',datetime('now'),datetime('now')),"
             "('item_field_journal'," + campaignRow + ",'Cassian Field Journal','Story item linked to personal arcs.','dnd2024','Story Item','Book',0,1.0,0,0,0,0,0,'','Strength',5,5,'contains coded expedition notes;','story;',datetime('now'),datetime('now')),"
             "('item_healing_tonic'," + campaignRow + ",'Crimson Healing Tonic','Consumable used to test stackable inventory.','dnd2024','Consumable','Potion',0,0.5,5000,1,0,0,0,'','Strength',5,5,'restore health manually after drinking;','consumable;',datetime('now'),datetime('now')),"
             "('item_gate_key'," + campaignRow + ",'Obsidian Gate Key','Plot key connected to a story scene.','dnd2024','Story Item','Key',3,0.2,0,0,0,0,0,'','Strength',5,5,'opens one sealed threshold below the harbor;','plot;',datetime('now'),datetime('now')),"
             "('item_charting_chalk'," + campaignRow + ",'Charting Chalk','A bundle of enchanted chalk used for floor planning.','dnd2024','Consumable','Ammunition',0,0.1,150,1,0,0,0,'','Dexterity',5,5,'marks movement and geometry clearly.','utility;merchant;',datetime('now'),datetime('now')),"
             "('item_tideglass_focus'," + campaignRow + ",'Tideglass Focus','Shard of moonstone used by cultic spellcasters.','dnd2024','Magic Focus','Wondrous',4,0.3,3500,0,0,0,0,'','Intelligence',5,5,'grants insight on planar disturbances.','magic;focus;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO custom_spells (stable_id, campaign_id, name, spell_level, school, resolution, casting_time, spell_range, duration, concentration, ritual, verbal, somatic, material, material_description, classes, attack_bonus, saving_throw_ability, damage, healing, area, effect_summary, scaling, description, tags, created_at, updated_at) VALUES "
             "('spell_ember_lance'," + campaignRow + ",'Ember Lance',1,4,1,'1 Action','90 ft','Instantaneous',0,0,1,1,0,'','Wizard, Sorcerer','','','1d10 fire','','One creature','Ranged spell attack that tests spell attack actions.','+1d10 fire per higher slot','A clean combat spell for testing attack bonus, damage, and combat action generation.','combat;fire;',datetime('now'),datetime('now')),"
             "('spell_warding_chorus'," + campaignRow + ",'Warding Chorus',2,0,2,'1 Action','30 ft','1 minute',1,0,1,1,1,'a silver tuning fork','Bard, Cleric','','Wisdom','','','15 ft aura','Creatures in the area make a save or lose momentum.','Increase area by 5 ft per higher slot','Tests concentration, saving throw metadata, and effect text.','control;concentration;',datetime('now'),datetime('now')),"
             "('spell_menders_light'," + campaignRow + ",'Mender''s Light',1,0,3,'1 Action','Touch','Instantaneous',0,0,1,1,0,'','Cleric, Druid','','','','1d8 + ability','One creature','Restores health manually and tracks healing metadata.','+1d8 healing per higher slot','Tests healing spells on character sheets.','healing;',datetime('now'),datetime('now')),"
             "('spell_echo_step'," + campaignRow + ",'Echo Step',0,1,0,'1 Bonus Action','Self','Instantaneous',0,0,0,1,0,'','Bard, Wizard','','','','','Self','Move through a nearby echo or reflection.','','Cantrip utility spell for no-slot casting.','utility;cantrip;',datetime('now'),datetime('now')),"
             "('spell_gate_sense'," + campaignRow + ",'Gate Sense',1,2,0,'10 minutes','Self','1 hour',0,1,1,0,1,'powdered obsidian','Wizard, Warlock','','','','','Self','Detects nearby planar thresholds.','Ritual use does not consume a slot.','Tests ritual metadata and story-linked magic.','ritual;story;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO characters (stable_id, campaign_id, name, level, class, race, attributes, tags, created_at, updated_at, backstory, current_hp, max_hp, portrait_asset_id, inventory_entries, spellcasting_enabled, spellcasting_ability, spell_attack_bonus, spell_save_dc, spell_slots_max, spell_slots_used, spellbook_entries) VALUES " +
                std::string("(") + quote("char_mira") + "," + campaignRow + "," + quote("Mira Vale") + ",5," + quote("Wizard") + "," + quote("Human") + "," + quote(attrs(8, 14, 13, 18, 12, 11)) + "," + quote("leader;spellcaster;") + ",datetime('now'),datetime('now')," + quote("Mira charts unstable gates and tests spellbook, portrait, health, slots, and story links.") + ",28,34," + quote("Resources/Campaigns/campaign_current_feature_tour/Assets/Portraits/char_mira.bmp") + "," + quote(inv({"item_starforged_rapier:1:1:Main Hand", "item_field_journal:1:0:Pack", "item_healing_tonic:2:0:Pouch"})) + ",1," + quote("Intelligence") + ",7,15," + quote(slots({0,4,3,2,0,0,0,0,0,0})) + "," + quote(slots({0,1,0,1,0,0,0,0,0,0})) + "," + quote(spellbook({"spell_ember_lance", "spell_echo_step", "spell_gate_sense"})) + "),"
                "(" + quote("char_thorne") + "," + campaignRow + "," + quote("Thorne Ashkeeper") + ",4," + quote("Paladin") + "," + quote("Dwarf") + "," + quote(attrs(16, 10, 15, 9, 13, 16)) + "," + quote("defender;divine;") + ",datetime('now'),datetime('now')," + quote("Thorne guards the party and tests armor, shield, healing magic, and adjustable health.") + ",38,42," + quote("Resources/Campaigns/campaign_current_feature_tour/Assets/Portraits/char_thorne.bmp") + "," + quote(inv({"item_moonlit_shield:1:1:Off Hand", "item_healing_tonic:1:0:Belt"})) + ",1," + quote("Charisma") + ",6,14," + quote(slots({0,3,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(spellbook({"spell_menders_light", "spell_warding_chorus"})) + "),"
                "(" + quote("char_lio") + "," + campaignRow + "," + quote("Lio Quickwater") + ",4," + quote("Rogue") + "," + quote("Halfling") + "," + quote(attrs(9, 18, 12, 13, 14, 11)) + "," + quote("scout;inventory;") + ",datetime('now'),datetime('now')," + quote("Lio carries plot items and tests non-caster sheet editing, inventory removal, and story connections.") + ",25,27," + quote("Resources/Campaigns/campaign_current_feature_tour/Assets/Portraits/char_lio.bmp") + "," + quote(inv({"item_gate_key:1:0:Hidden Pocket", "item_healing_tonic:3:0:Pouch"})) + ",0," + quote("Intelligence") + ",0,10," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote("") + "),"
                "(" + quote("char_seren") + "," + campaignRow + "," + quote("Seren Dusksong") + ",5," + quote("Bard") + "," + quote("Elf") + "," + quote(attrs(10, 15, 12, 13, 12, 18)) + "," + quote("face;support;") + ",datetime('now'),datetime('now')," + quote("Seren tests prepared support magic and narrative music connections.") + ",31,31," + quote("Resources/Campaigns/campaign_current_feature_tour/Assets/Portraits/char_seren.bmp") + "," + quote(inv({"item_healing_tonic:1:0:Satchel"})) + ",1," + quote("Charisma") + ",7,15," + quote(slots({0,4,3,2,0,0,0,0,0,0})) + "," + quote(slots({0,0,1,0,0,0,0,0,0,0})) + "," + quote(spellbook({"spell_warding_chorus", "spell_echo_step", "spell_menders_light"})) + ");");

        exec(db,
             "INSERT INTO npcs (stable_id, campaign_id, category, name, description, faction, role, dialogue_notes, attributes, conditions, tags, created_at, updated_at, inventory_entries, spellcasting_enabled, spellcasting_ability, spell_attack_bonus, spell_save_dc, spell_slots_max, spell_slots_used, spellbook_entries) VALUES " +
                 std::string("(") + quote("npc_captain_sera") + "," + campaignRow + ",0," + quote("Captain Sera Voss") + "," + quote("Harbor captain who can become an ally or obstacle.") + "," + quote("Harbor Watch") + "," + quote("Authority Contact") + "," + quote("Keeps answers short. Hates being surprised.") + "," + quote(attrs(12, 13, 14, 12, 15, 14)) + "," + quote("") + "," + quote("watch;ally;") + ",datetime('now'),datetime('now')," + quote(inv({"item_healing_tonic:1:0:Desk"})) + ",0," + quote("Wisdom") + ",0,10," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote("") + "),"
                 "(" + quote("npc_oracle_nyme") + "," + campaignRow + ",3," + quote("Oracle Nyme") + "," + quote("A masked diviner tied to the gate mystery.") + "," + quote("The Glass Choir") + "," + quote("Mystic Patron") + "," + quote("Speaks in trade prices and weather omens.") + "," + quote(attrs(8, 12, 11, 16, 18, 16)) + "," + quote("Concentrating;") + "," + quote("spellcaster;deity;") + ",datetime('now'),datetime('now')," + quote(inv({"item_field_journal:1:0:Lectern"})) + ",1," + quote("Wisdom") + ",8,16," + quote(slots({0,4,3,3,1,0,0,0,0,0})) + "," + quote(slots({0,0,1,0,0,0,0,0,0,0})) + "," + quote(spellbook({"spell_gate_sense", "spell_warding_chorus", "spell_menders_light"})) + "),"
                 "(" + quote("npc_broker_cass") + "," + campaignRow + ",0," + quote("Cassian Vale") + "," + quote("Mira's sibling and a charming smuggler broker.") + "," + quote("Lantern Syndicate") + "," + quote("Rival Contact") + "," + quote("Deflects fear with jokes and bad cards.") + "," + quote(attrs(10, 16, 12, 14, 11, 15)) + "," + quote("") + "," + quote("rival;story;") + ",datetime('now'),datetime('now')," + quote(inv({"item_gate_key:1:0:Coat"})) + ",0," + quote("Charisma") + ",0,10," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote("") + "),"
                 "(" + quote("npc_golem") + "," + campaignRow + ",1," + quote("Archive Golem") + "," + quote("A creature custodian used to test creature NPC category and inventory links.") + "," + quote("Old Archive") + "," + quote("Guardian") + "," + quote("Repeats old catalog numbers before attacking.") + "," + quote(attrs(18, 8, 16, 6, 10, 5)) + "," + quote("Guarding;") + "," + quote("creature;guardian;") + ",datetime('now'),datetime('now')," + quote(inv({"item_moonlit_shield:1:1:Arm Plate"})) + ",0," + quote("Intelligence") + ",0,10," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote("") + ");");

        exec(db,
             "INSERT INTO factions (stable_id, campaign_id, name, faction_type, summary, headquarters, leader_npc_id, attitude_toward_party, goals, resources, gm_notes, influence, threat_level, tags, created_at, updated_at) VALUES " +
                 std::string("(") + quote("faction_harbor_watch") + "," + campaignRow + "," + quote("Harbor Watch") + "," + quote("City Watch") + "," + quote("Lawful maritime defenders controlling harbor entries and customs.") + "," + quote("Harbor Ward Hall") + "," + quote("npc_captain_sera") + "," + quote("Protective, skeptical of outsiders") + "," + quote("Contain the gate breach and keep trade lanes open.") + "," + quote("Chain shotguns, patrol skiffs, legal authority") + "," + quote("Tracks player debts and favors closely.") + ",48,3," + quote("military;authority;coastwatch;") + ",datetime('now'),datetime('now')),"
                 "(" + quote("faction_lantern_syndicate") + "," + campaignRow + "," + quote("Lantern Syndicate") + "," + quote("Smuggling Guild") + "," + quote("A flexible cartel managing black-market river traffic.") + "," + quote("Night Market Quarter") + "," + quote("npc_broker_cass") + "," + quote("Business-first, negotiable.") + "," + quote("Move contraband past port checks without bloodshed.") + "," + quote("Safe routes, lockpicks, courier chains") + "," + quote("Often useful, occasionally volatile.") + ",22,4," + quote("merchant;rival;underground;") + ",datetime('now'),datetime('now')),"
                 "(" + quote("faction_archive_conclave") + "," + campaignRow + "," + quote("Archive Conclave") + "," + quote("Scholar Circle") + "," + quote("Custodians of hidden planar records beneath the archive.") + "," + quote("Under-Archive Stacks") + "," + quote("npc_golem") + "," + quote("Obsessive, indifferent to emotion.") + "," + quote("Secure and classify anomalous findings.") + "," + quote("Cartographic arrays, archive vaults, cipher archives") + "," + quote("Favors expeditions who share clean data.") + ",15,1," + quote("scholar;planar;library;") + ",datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO deities (stable_id, campaign_id, name, power_type, pantheon, domains, symbol, alignment, tenets, worshipers, relationships, gm_notes, influence, threat_level, tags, created_at, updated_at) VALUES " +
                 std::string("(") + quote("deity_aquora") + "," + campaignRow + "," + quote("Aquora") + "," + quote("Deity") + "," + quote("Tidebound Pantheon") + "," + quote("Water, Fate, Oathkeeping") + "," + quote("Silver wave over black stone") + "," + quote("Neutral Good") + "," + quote("Protect coastal pacts and those who keep their word.") + "," + quote("Harbor sailors, cartographers, sworn captains") + "," + quote("Maintains an uneasy arrangement with the Glass Choir over storm omens.") + "," + quote("Revealed through calm waves and stable tides.") + ",30,2," + quote("divine;sea;") + ",datetime('now'),datetime('now')),"
                 "(" + quote("deity_noctur") + "," + campaignRow + "," + quote("Noctur") + "," + quote("Ascended Spirit") + "," + quote("Glass Choir Ascendancy") + "," + quote("Knowledge, Secrets, Veils") + "," + quote("Closed eye over fractured halo") + "," + quote("Lawful Neutral") + "," + quote("Knowledge is only truth after it is tested.") + "," + quote("Diviners, archivists, spies") + "," + quote("Competes with Aquora for ritual authority.") + "," + quote("Rarely appears as a living voice; more often as reflected silence.") + ",24,3," + quote("divine;mystery;") + ",datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO npcs (stable_id, campaign_id, category, faction_id, deity_id, name, description, faction, role, attitude_toward_party, secrets, goals, resources, threat_location, relationships, threat_level, dialogue_notes, attributes, conditions, tags, created_at, updated_at, inventory_entries, spellcasting_enabled, spellcasting_ability, spell_attack_bonus, spell_save_dc, spell_slots_max, spell_slots_used, spellbook_entries) VALUES " +
                 std::string("(") + quote("npc_watcher_ell") + "," + campaignRow + ",0," + quote("faction_harbor_watch") + "," + quote("deity_aquora") + "," + quote("Elias Watcher Ell") + "," + quote("Harbor quartermaster who coordinates night watch schedules.") + "," + quote("Harbor Watch") + "," + quote("Logistics Officer") + "," + quote("Cordial but practical") + "," + quote("") + "," + quote("Ensure patrol routes remain unbroken.") + "," + quote("Sea rations, tide maps, and signal flags.") + "," + quote("Dock 3 watchtower") + "," + quote("Friendly with Sera, neutral with Cassian.") + ",2," + quote("If the gate stirs, he starts ringing the horn before dawn.") + "," + quote(attrs(11, 11, 13, 10, 12, 11)) + "," + quote("") + "," + quote("witness;watch;") + ",datetime('now'),datetime('now')," + quote(inv({"item_charting_chalk:2:0:Wrist"})) + ",0," + quote("Wisdom") + ",0,10," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote("") + ");");

        exec(db,
             "INSERT INTO custom_monsters (stable_id, campaign_id, name, source, size, creature_type, challenge_rating, armor_class, armor_damage_reduction, hit_points, speed_feet, experience, attributes, actions, tags, created_at, updated_at) VALUES " +
                 std::string("(") + quote("monster_tide_wraith") + "," + campaignRow + "," + quote("Tide Wraith") + "," + quote("Campaign") + "," + quote("Medium") + "," + quote("Undead") + "," + quote("2") + ",14,1,45,30,450," + quote(attrs(8, 16, 14, 10, 13, 15)) + "," + quote(actions({action("Drowning Touch", 5, 5, "+5", "2d8 necrotic", "Target hears the harbor bell."), action("Brackish Bolt", 5, 60, "+5", "1d10 cold")})) + "," + quote("undead;harbor;") + ",datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO maps (stable_id, campaign_id, name, description, scope, grid_type, width, height, cell_size, is_main_map, parent_map_id, cells, tags, created_at, updated_at) VALUES "
             "('map_harbor_ward'," + campaignRow + ",'Harbor Ward','Party context map with current location.',1,0,8,8,1.0,1,'','0:0,0:0,6:0,6:0,1:0,1:0,2:0,2:0,0:0,6:0,6:0,6:0,1:0,2:0,2:0,2:0,0:0,6:0,7:0,6:0,6:0,2:0,2:0,2:0,0:0,6:0,6:0,6:0,6:0,2:0,2:0,2:0,0:0,0:0,6:0,6:0,6:0,6:0,2:0,2:0,0:0,0:0,0:0,6:0,6:0,6:0,6:0,2:0,0:0,0:0,0:0,0:0,6:0,6:0,6:0,0:0,0:0,0:0,0:0,0:0,0:0,6:0,6:0,','map;harbor;',datetime('now'),datetime('now')),"
             "('map_archive_vault'," + campaignRow + ",'Archive Vault','Small encounter-ready dungeon map.',2,0,10,8,1.0,0,'','8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,9:0,9:0,9:0,10:0,9:0,9:0,9:0,9:0,8:0,8:0,9:0,14:0,9:0,9:0,9:0,14:0,9:0,9:0,8:0,8:0,9:0,9:0,9:0,13:0,9:0,9:0,9:0,9:0,8:0,8:0,9:0,9:0,9:0,9:0,9:0,9:0,14:0,9:0,8:0,8:0,9:0,9:0,10:0,9:0,9:0,9:0,9:0,9:0,8:0,8:0,9:0,9:0,9:0,9:0,14:0,9:0,9:0,9:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,','map;vault;',datetime('now'),datetime('now'));");

        exec(db,
             "UPDATE maps SET regions=" +
                 quote(packedEntries({mapRegion("region_fog_quay", "Fog Quay", "Rain-slick market approach with difficult terrain.", 0, 0, 4, 3, 2, 1, true, false, false),
                                      mapRegion("region_watch_line", "Watch Line", "GM-only patrol pressure near customs.", 4, 2, 3, 3, 1, 1, false, false, true)})) +
                 ", features=" +
                 quote(packedEntries({mapFeature("feature_gate_clue", 0, "Salt-Rune Clue", "A residue mark that points toward the archive vault.", "Reveal after a DC 13 investigation check.", "story_gate_below", "map_archive_vault", 3, 2, true, false, 13),
                                      mapFeature("feature_supply_house", 0, "Harbor Supply House", "Merchant access for consumables and chalk.", "Open the shop manager from this location.", "shop_harbor_supply", "", 6, 4, true, false, 0)})) +
                 ", routes=" +
                 quote(packedEntries({mapRoute("route_harbor_to_archive", "Smuggler Stair to Archive", "map_harbor_ward", "map_archive_vault", 5, 5, 1, 6, 3.5f, "urban tunnels", "Travel route for the session dashboard and map viewer.", true)})) +
                 " WHERE stable_id='map_harbor_ward';");

        exec(db,
             "UPDATE maps SET regions=" +
                 quote(packedEntries({mapRegion("region_sealed_stacks", "Sealed Stacks", "Line of sight is blocked by collapsed shelving.", 1, 1, 3, 5, 1, 2, false, true, false),
                                      mapRegion("region_wraith_tide", "Wraith Tide", "Cold water makes movement costly.", 5, 3, 4, 3, 2, 1, true, false, false)})) +
                 ", features=" +
                 quote(packedEntries({mapFeature("feature_obsidian_lock", 1, "Obsidian Lock", "The gate key can open this threshold.", "Start the Gate Reveal Pulse cue when opened.", "item_gate_key", "", 4, 3, false, false, 15),
                                      mapFeature("feature_hidden_ledger", 2, "Hidden Ledger Niche", "Cassian's field notes are hidden behind cracked stone.", "Links the character story to the dungeon.", "item_field_journal", "", 6, 5, false, true, 14)})) +
                 " WHERE stable_id='map_archive_vault';");

        exec(db,
             "INSERT INTO map_layers (stable_id, map_id, name, visible, locked, sort_order, tags, created_at, updated_at) "
             "SELECT 'layer_harbor_gm', id, 'GM Pins', 1, 0, 10, 'gm;notes;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_harbor_ward' UNION ALL "
             "SELECT 'layer_harbor_party', id, 'Party Tokens', 1, 0, 20, 'party;tokens;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_harbor_ward' UNION ALL "
             "SELECT 'layer_archive_tokens', id, 'Encounter Tokens', 1, 0, 10, 'combat;tokens;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_archive_vault';");

        exec(db,
             "INSERT INTO map_tokens (stable_id, map_id, name, kind, linked_entity_type, linked_entity_id, asset_id, layer_id, x, y, rotation, size, hidden_from_players, tags, created_at, updated_at) "
             "SELECT 'token_party_harbor', id, 'Party Location', 0, 'Party', 'campaign_current_feature_tour', '', 'layer_harbor_party', 4, 5, 0, 1, 0, 'party;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_harbor_ward' UNION ALL "
             "SELECT 'token_captain_sera', id, 'Captain Sera', 1, 'NPC', 'npc_captain_sera', '', 'layer_harbor_gm', 2, 1, 0, 1, 0, 'npc;ally;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_harbor_ward' UNION ALL "
             "SELECT 'token_tide_wraith', id, 'Tide Wraith Start', 2, 'Monster', 'monster_tide_wraith', '', 'layer_archive_tokens', 7, 5, 0, 1, 1, 'monster;hidden;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_archive_vault' UNION ALL "
             "SELECT 'token_oracle_archive', id, 'Oracle Nyme', 1, 'NPC', 'npc_oracle_nyme', '', 'layer_archive_tokens', 5, 4, 0, 1, 0, 'npc;caster;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_archive_vault';");

        exec(db,
             "INSERT INTO audio_assets (stable_id, campaign_id, name, original_file_name, original_source_path, file_path, asset_type, default_volume, loop, tags, created_at, updated_at) VALUES "
             "('audio_harbor_rain'," + campaignRow + ",'Harbor Rain Underscore','harbor_rain.wav','','Resources/Campaigns/campaign_current_feature_tour/Assets/Audio/harbor_rain.wav',1,0.45,1,'ambience;story;',datetime('now'),datetime('now')),"
             "('audio_gate_reveal'," + campaignRow + ",'Gate Reveal Pulse','gate_reveal.wav','','Resources/Campaigns/campaign_current_feature_tour/Assets/Audio/gate_reveal.wav',0,0.7,0,'music;cue;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO audio_playlists (stable_id, campaign_id, name, audio_asset_ids, shuffle, crossfade_seconds, tags, created_at, updated_at) VALUES "
             "('playlist_session_one'," + campaignRow + ",'Session One Cue Stack','audio_harbor_rain;audio_gate_reveal;',0,0.0,'session;audio;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO asset_metadata (stable_id, campaign_id, asset_type, display_name, source_path, project_path, content_hash, mime_type, byte_size, missing, tags, created_at, updated_at) VALUES "
             "('assetmeta_harbor_rain'," + campaignRow + ",'audio','Harbor Rain Underscore','','Resources/Campaigns/campaign_current_feature_tour/Assets/Audio/harbor_rain.wav','seeded-demo-audio-1','audio/wav',0,0,'audio;ambience;',datetime('now'),datetime('now')),"
             "('assetmeta_gate_reveal'," + campaignRow + ",'audio','Gate Reveal Pulse','','Resources/Campaigns/campaign_current_feature_tour/Assets/Audio/gate_reveal.wav','seeded-demo-audio-2','audio/wav',0,0,'audio;cue;',datetime('now'),datetime('now')),"
             "('assetmeta_mira_portrait'," + campaignRow + ",'portrait','Mira Vale Portrait','','Resources/Campaigns/campaign_current_feature_tour/Assets/Portraits/char_mira.bmp','seeded-demo-portrait-mira','image/bmp',0,0,'portrait;player;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO shops (stable_id, campaign_id, name, merchant_name, description, faction_reputation_notes, inventory_id, tags, created_at, updated_at) VALUES "
             "('shop_harbor_supply'," + campaignRow + ",'Harbor Supply House','Captain Rook','Customs-forwarding with practical equipment and ritual-safe tools.','Positive reputation with Harbor Watch improves resting discounts by 20%.','','" + "trade;ration;" + "',datetime('now'),datetime('now')),"
             "('shop_glass_market'," + campaignRow + ",'Glass Choir Bazaar','Myridian','Curated relic store with coded prayer pieces and tide-charms.','Noctur loyalists may get better terms when relationship tags are visible.','','" + "relic;mystic;" + "',datetime('now'),datetime('now'));");
        exec(db,
             std::string("INSERT INTO shop_stock (shop_id, item_id, quantity, price_multiplier, sell_price_multiplier, restocks, availability, haggle_notes, faction_modifier, reputation_modifier) ") +
                 "SELECT id, 'item_charting_chalk', 6, 0.75, 0.45, 1, 'Common', 'Ask for extra bags on first purchase.', 1.1, 1.2 FROM shops WHERE stable_id='shop_harbor_supply' UNION ALL "
                 "SELECT id, 'item_moonlit_shield', 2, 0.9, 0.5, 0, 'Limited', 'Not cheap outside the watch schedule.', 1.0, 1.0 FROM shops WHERE stable_id='shop_harbor_supply' UNION ALL "
                 "SELECT id, 'item_tideglass_focus', 1, 1.8, 0.65, 1, 'Rare', 'Requires Glass Choir token to access this item.', 1.2, 1.6 FROM shops WHERE stable_id='shop_glass_market';");

        exec(db,
             "INSERT INTO loot_parcels (stable_id, campaign_id, name, source, notes, awarded, awarded_to, copper, silver, gold, platinum, tags, created_at, updated_at) VALUES "
             "('parcel_gate_cache'," + campaignRow + ",'Gate Cache','Archive Breach','Recovered from the Tide Wraith collapse site.',0,'',60,0,3,0,'combat;reward;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO loot_parcel_items (loot_parcel_id, item_id, quantity, notes) "
             "SELECT id, 'item_tideglass_focus', 1, 'Cracks when exposed to daylight.' FROM loot_parcels WHERE stable_id='parcel_gate_cache' UNION ALL "
             "SELECT id, 'item_field_journal', 1, 'Personal observations about harmonic seals.' FROM loot_parcels WHERE stable_id='parcel_gate_cache';");

        exec(db,
             "INSERT INTO encounters (stable_id, campaign_id, name, description, map_id, map_context_name, map_context_x, map_context_y, battlefield_width, battlefield_height, cell_size_feet, use_map_cells, battlefield_cells, round, active_participant_index, tags, created_at, updated_at) VALUES "
             "('enc_archive_breach'," + campaignRow + ",'Archive Breach','Combat encounter with spellcasting PCs, a spellcasting NPC, an equipped NPC, and a custom monster.','map_archive_vault','Archive Vault',4,3,10,8,5,1,'8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,9:0,9:0,9:0,10:0,9:0,9:0,9:0,9:0,8:0,8:0,9:0,14:0,9:0,9:0,9:0,14:0,9:0,9:0,8:0,8:0,9:0,9:0,9:0,13:0,9:0,9:0,9:0,9:0,8:0,8:0,9:0,9:0,9:0,9:0,9:0,9:0,14:0,9:0,8:0,8:0,9:0,9:0,10:0,9:0,9:0,9:0,9:0,9:0,8:0,8:0,9:0,9:0,9:0,9:0,14:0,9:0,9:0,9:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,8:0,',1,0,'combat;magic;',datetime('now'),datetime('now'));");

        const long long encounterRowId = sqlite3_last_insert_rowid(db);
        const std::string encounterRow = std::to_string(encounterRowId);
        exec(db,
             "INSERT INTO encounter_participants (stable_id, encounter_id, kind, source_entity_id, monster_definition_id, display_name, initiative, current_hp, max_hp, armor_class, armor_damage_reduction, speed_feet, grid_x, grid_y, turn_start_x, turn_start_y, movement_used_feet, defeated, friendly, conditions, actions, tags, created_at, updated_at) VALUES " +
                 std::string("(") + quote("part_mira") + "," + encounterRow + ",0," + quote("char_mira") + "," + quote("") + "," + quote("Mira Vale") + ",15,28,34,12,0,30,2,2,2,2,0,0,1," + quote("") + "," + quote(actions({action("Ember Lance", 5, 90, "+7", "1d10 fire", "Spell attack from spellbook."), action("Starforged Rapier", 5, 5, "+8", "1d8 piercing", "Equipped weapon.")})) + "," + quote("pc;spell;") + ",datetime('now'),datetime('now')),"
                 "(" + quote("part_thorne") + "," + encounterRow + ",0," + quote("char_thorne") + "," + quote("") + "," + quote("Thorne Ashkeeper") + ",11,38,42,14,1,30,3,2,3,2,0,0,1," + quote("") + "," + quote(actions({action("Mender's Light", 5, 5, "+6", "1d8 + ability", "Healing spell."), action("Shield Bash", 5, 5, "+5", "1d6 bludgeoning")})) + "," + quote("pc;defender;") + ",datetime('now'),datetime('now')),"
                 "(" + quote("part_oracle") + "," + encounterRow + ",1," + quote("npc_oracle_nyme") + "," + quote("") + "," + quote("Oracle Nyme") + ",13,22,22,12,0,30,5,4,5,4,0,0,1," + quote("Concentrating;") + "," + quote(actions({action("Warding Chorus", 5, 30, "+0", "manual result", "Wisdom save DC 16.")})) + "," + quote("npc;spell;") + ",datetime('now'),datetime('now')),"
                 "(" + quote("part_wraith") + "," + encounterRow + ",2," + quote("monster_tide_wraith") + "," + quote("monster_tide_wraith") + "," + quote("Tide Wraith") + ",16,45,45,14,1,30,7,5,7,5,0,0,0," + quote("") + "," + quote(actions({action("Drowning Touch", 5, 5, "+5", "2d8 necrotic"), action("Brackish Bolt", 5, 60, "+5", "1d10 cold")})) + "," + quote("monster;") + ",datetime('now'),datetime('now'));");

        exec(db,
             "UPDATE encounters SET "
             "dice_roll_presets=" +
                 quote(packedEntries({dicePreset("preset_initiative", "Group Initiative", "1d20+2", "Fast start for the seeded encounter."),
                                      dicePreset("preset_wraith_damage", "Tide Wraith Damage", "2d8", "Use for Drowning Touch."),
                                      dicePreset("preset_gate_surge", "Gate Surge", "1d6+2", "Environmental hazard damage.")})) +
                 ", combat_log=" +
                 quote(packedEntries({combatLogEntry("log_setup", 1, "GM", "Note", "Archive Breach is prepped with party tokens, a hidden wraith, and an active music cue."),
                                      combatLogEntry("log_round_hint", 1, "Oracle Nyme", "Spell", "Warding Chorus is concentrating and should trigger concentration reminders if damaged.")})) +
                 ", after_combat_summary='If the Tide Wraith is defeated, the gate stabilizes long enough for one major reveal.', "
                 "after_combat_loot_notes='Award Gate Cache loot parcel and 450 XP or milestone progress.', "
                 "after_combat_story_consequences='Mark The Obsidian Key Turns scene complete and advance Mira and Cassian''s Debt.', "
                 "after_combat_session_notes='Good spot to end Session 1 or open Session 2 with the Glass Choir.', "
                 "combat_paused=0, theater_of_mind=0 "
                 "WHERE stable_id='enc_archive_breach';");

        exec(db,
             "UPDATE encounter_participants SET group_name='Party', wave_number=1, starting_x=grid_x, starting_y=grid_y WHERE stable_id IN ('part_mira','part_thorne');");
        exec(db,
             "UPDATE encounter_participants SET group_name='Allies', wave_number=1, starting_x=grid_x, starting_y=grid_y, active_effects=" +
                 quote(packedEntries({activeEffect("effect_oracle_chorus", 2, "Warding Chorus", "Oracle Nyme", "npc_oracle_nyme", "spell_warding_chorus", "Concentration reminder for the combat tracker.", 10, true)})) +
                 " WHERE stable_id='part_oracle';");
        exec(db,
             "UPDATE encounter_participants SET group_name='Wave 2', wave_number=2, starts_hidden=1, reinforcement_round=2, starting_x=7, starting_y=5, damage_resistances='cold;necrotic;', damage_vulnerabilities='radiant;', damage_immunities='', temporary_hp=5 WHERE stable_id='part_wraith';");

        exec(db,
             "INSERT INTO story_threads (stable_id, campaign_id, title, thread_type, status, summary, stakes, gm_notes, tags, created_at, updated_at) VALUES "
             "('story_gate_below'," + campaignRow + ",'The Gate Below the Harbor',0,3,'Main plot arc for checking scene structure, map/audio/spell/item links, and active narrative cues.','If the party fails, the harbor becomes a planar breach.','Use the gate reveal cue when the key turns.','main;magic;',datetime('now'),datetime('now')),"
             "('story_mira_cass'," + campaignRow + ",'Mira and Cassian''s Debt',1,2,'Character story involving Mira, Cassian, and the field journal.','Mira may save Cassian or expose the syndicate.','Connects character sheets to story arcs.','character;family;',datetime('now'),datetime('now')),"
             "('story_glass_choir'," + campaignRow + ",'The Glass Choir',2,1,'World/faction story for Oracle Nyme and ritual magic.','The Choir may become patron, villain, or warning system.','Spell and NPC links should be visible in overviews.','world;faction;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_gate_key', id, 'The Obsidian Key Turns', 'The party uses the key in the archive vault and the gate answers.', 'Start the reveal cue and open the archive scene.', 2, 'Night 3', 'audio_gate_reveal', 0.70, 0, 'cue;scene;', datetime('now'), datetime('now'), 0 FROM story_threads WHERE stable_id='story_gate_below';");
        exec(db,
             "INSERT INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_harbor_rain', id, 'Rain Over the Harbor Ward', 'Investigation scene introducing Sera, Cassian, and the first clue.', 'Use ambience as a low-volume loop.', 1, 'Night 1', 'audio_harbor_rain', 0.45, 1, 'ambience;scene;', datetime('now'), datetime('now'), 1 FROM story_threads WHERE stable_id='story_gate_below';");
        exec(db,
             "INSERT INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_cassian_debt', id, 'Cassian''s Debt', 'Cassian owes old debts tied to the Lantern Syndicate and asks for help.', 'Keep this beat linked to the shop and rival NPC.', 1, 'Evening 2', '', 0.0, 0, 'rival;social;', datetime('now'), datetime('now'), 0 FROM story_threads WHERE stable_id='story_mira_cass';");
        exec(db,
             "INSERT INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_glass_choir_council', id, 'Council Above the Glass', 'The party receives a formal ritual audience with Oracle Nyme and a public envoy.', 'This is the world-level reveal point for the choir thread.', 2, 'Dawn 1', '', 0.0, 0, 'faction;ritual;', datetime('now'), datetime('now'), 0 FROM story_threads WHERE stable_id='story_glass_choir';");

        exec(db,
             "INSERT INTO story_links (stable_id, campaign_id, owner_entity_id, owner_kind, link_type, label, target_entity_id, target_entity_type, tags, created_at, updated_at) VALUES "
             "('link_gate_mira'," + campaignRow + ",'story_gate_below',0,1,'primary researcher','char_mira','Player','',datetime('now'),datetime('now')),"
             "('link_gate_map'," + campaignRow + ",'story_gate_below',0,1,'battlefield location','map_archive_vault','Map','',datetime('now'),datetime('now')),"
             "('link_gate_spell'," + campaignRow + ",'story_gate_below',0,5,'ritual clue','spell_gate_sense','Spell','',datetime('now'),datetime('now')),"
             "('link_cass_lio'," + campaignRow + ",'story_mira_cass',0,1,'key carrier','char_lio','Player','',datetime('now'),datetime('now')),"
             "('link_cass_npc'," + campaignRow + ",'story_mira_cass',0,1,'rival sibling','npc_broker_cass','NPC','',datetime('now'),datetime('now')),"
             "('link_choir_oracle'," + campaignRow + ",'story_glass_choir',0,1,'voice of the choir','npc_oracle_nyme','NPC','',datetime('now'),datetime('now')),"
             "('link_beat_key_item'," + campaignRow + ",'beat_gate_key',1,5,'opens the gate','item_gate_key','Item','',datetime('now'),datetime('now')),"
             "('link_beat_key_audio'," + campaignRow + ",'beat_gate_key',1,3,'play reveal music','audio_gate_reveal','Audio','',datetime('now'),datetime('now')),"
             "('link_beat_harbor_audio'," + campaignRow + ",'beat_harbor_rain',1,3,'loop during investigation','audio_harbor_rain','Audio','',datetime('now'),datetime('now')),"
             "('link_beat_debt'," + campaignRow + ",'beat_cassian_debt',1,1,'rival contract point','npc_broker_cass','NPC','',datetime('now'),datetime('now')),"
             "('link_beat_debt_shop'," + campaignRow + ",'beat_cassian_debt',1,1,'access point for debts','shop_harbor_supply','Shop','',datetime('now'),datetime('now')),"
             "('link_beat_choir'," + campaignRow + ",'beat_glass_choir_council',1,1,'ritual audience','faction_archive_conclave','Faction','',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO story_documents (stable_id, campaign_id, title, body, tags, created_at, updated_at) VALUES "
             "('doc_session_one_runbook'," + campaignRow + ",'Session 1 Runbook','Open with Harbor Rain Underscore. Let each of the four player characters establish one clue, one resource, and one risk. Move from Harbor Ward to Archive Vault, then start Archive Breach when the Obsidian Gate Key is used.','session;prep;gm;',datetime('now'),datetime('now')),"
             "('doc_table_safety_and_tone'," + campaignRow + ",'Table Tone Notes','Mystery, coastal fantasy, occult pressure, and practical heroics. Keep threats eerie but readable; avoid gore and give players clear ways to de-escalate NPC conflicts.','session;safety;tone;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO timeline_events (stable_id, campaign_id, title, description, in_world_date, related_entity_ids, tags, created_at, updated_at) VALUES "
             "('timeline_harbor_hook'," + campaignRow + ",'Rain Over the Harbor','The party receives Sera''s warning and Cassian''s debt note while the gate storm gathers.','Night 1','beat_harbor_rain;char_mira;npc_captain_sera;npc_broker_cass','setup;social;',datetime('now'),datetime('now')),"
             "('timeline_archive_breach'," + campaignRow + ",'Archive Breach','The Obsidian Gate Key turns inside the archive and the Tide Wraith emerges.','Night 1 - Bell Three','enc_archive_breach;item_gate_key;monster_tide_wraith;audio_gate_reveal','combat;climax;',datetime('now'),datetime('now')),"
             "('timeline_choir_audience'," + campaignRow + ",'Glass Choir Audience','Oracle Nyme offers a ritual explanation and a price for closing the gate permanently.','Dawn 2','story_glass_choir;npc_oracle_nyme;spell_gate_sense','faction;ritual;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO dungeons (stable_id, campaign_id, name, description, parent_map_id, active_room_id, gm_notes, map_ids, tags, created_at, updated_at) VALUES "
             "('dungeon_archive_lower'," + campaignRow + ",'Lower Archive Vault','Three-room playable dungeon slice tied to the archive map and encounter.','map_archive_vault','room_archive_entry','Use this to test room states, read-aloud text, traps, secrets, loot links, and encounter links.','map_archive_vault;','dungeon;showcase;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO dungeon_rooms (stable_id, dungeon_id, name, description, keyed_description, read_aloud_text, secrets, traps, exploration_notes, exploration_state, map_x, map_y, encounter_ids, loot_item_ids, connections, tags, created_at, updated_at) "
             "SELECT 'room_archive_entry', id, 'Salted Entry Stacks', 'Collapsed entry with water stains and watch chalk.', 'Room A: safer staging room with the first gate clue.', 'Rainwater drips upward along the shelves, then falls again as if remembering gravity.', 'A DC 13 check identifies Cassian''s chalk mark.', '', 'Seen from the first map reveal.', 1, 1, 1, '', 'item_charting_chalk', " +
                 quote(packedEntries({roomConnection("conn_entry_lock", "room_obsidian_lock", "Obsidian-locked aisle", true, false, true)})) +
                 ", 'entry;clue;', datetime('now'), datetime('now') FROM dungeons WHERE stable_id='dungeon_archive_lower' UNION ALL "
             "SELECT 'room_obsidian_lock', id, 'Obsidian Lock Aisle', 'Gate threshold and ritual lock.', 'Room B: keyed gate scene with audio cue.', 'The lock blooms open like black glass under moonlight.', 'The Tideglass Focus hums if brought here.', 'Opening without the key triggers a cold surge.', 'Active room for the current feature tour.', 2, 4, 3, 'enc_archive_breach', 'item_gate_key;item_tideglass_focus', " +
                 quote(packedEntries({roomConnection("conn_lock_entry", "room_archive_entry", "Back to entry stacks", false, false, true),
                                      roomConnection("conn_lock_ledger", "room_ledger_niche", "Secret ledger niche", false, true, false)})) +
                 ", 'gate;combat;', datetime('now'), datetime('now') FROM dungeons WHERE stable_id='dungeon_archive_lower' UNION ALL "
             "SELECT 'room_ledger_niche', id, 'Ledger Niche', 'Hidden side chamber with Cassian''s older notes.', 'Room C: optional story reward and character-story turn.', 'A dry alcove waits behind a shelf that should have rotted away years ago.', 'The journal names a Glass Choir patron.', '', 'Mark cleared when the party recovers the ledger.', 0, 6, 5, '', 'item_field_journal', " +
                 quote(packedEntries({roomConnection("conn_ledger_lock", "room_obsidian_lock", "Hidden return panel", false, true, false)})) +
                 ", 'secret;story;', datetime('now'), datetime('now') FROM dungeons WHERE stable_id='dungeon_archive_lower';");

        exec(db,
             "INSERT INTO sessions (stable_id, campaign_id, title, scheduled_at_utc, started_at_utc, ended_at_utc, summary, recap, unresolved_threads, loot_awarded, xp_milestones, npcs_met, locations_visited, open_hooks, participant_character_ids, encounter_ids, story_document_ids, live_mode_active, tags, created_at, updated_at) VALUES "
             "('session_feature_tour_one'," + campaignRow + ",'Session 1: Rain, Debt, and the Gate',datetime('now'),'', '', 'A ready-to-run four-player session showcasing story scenes, audio cues, maps, dungeon rooms, shop/inventory, and one combat encounter.', '', 'Who funds Cassian? What does Oracle Nyme know?', 'Gate Cache pending.', 'Archive Breach milestone or 450 XP.', 'Captain Sera Voss;Cassian Vale;Oracle Nyme;', 'Harbor Ward;Lower Archive Vault;', 'Glass Choir audience;Gate origin;', 'char_mira;char_thorne;char_lio;char_seren;', 'enc_archive_breach;', 'doc_session_one_runbook;doc_table_safety_and_tone;', 0, 'session;showcase;four-player;', datetime('now'), datetime('now'));");

        exec(db,
             "INSERT INTO session_notes (stable_id, session_id, timestamp_utc, body, linked_entity_id, linked_entity_type, linked_entity_label, tags, created_at, updated_at) "
             "SELECT 'note_opening_cue', id, datetime('now'), 'Start Harbor Rain Underscore before reading the harbor scene.', 'audio_harbor_rain', 'Audio', 'Harbor Rain Underscore', 'cue;', datetime('now'), datetime('now') FROM sessions WHERE stable_id='session_feature_tour_one' UNION ALL "
             "SELECT 'note_gate_reveal', id, datetime('now'), 'When Lio uses the Obsidian Gate Key, trigger Gate Reveal Pulse and load Archive Breach.', 'enc_archive_breach', 'Encounter', 'Archive Breach', 'combat;cue;', datetime('now'), datetime('now') FROM sessions WHERE stable_id='session_feature_tour_one';");

        exec(db,
             "INSERT INTO ruleset_definitions (stable_id, campaign_id, name, version, base_ruleset_id, definition_json, installed, tags, created_at, updated_at) VALUES "
             "('ruleset_feature_tour_overlay'," + campaignRow + ",'Feature Tour Table Overlay','1.0','dnd-2024-5.5e','{\"notes\":\"Showcase-only overlay for ritual endurance, market hours, and gate instability.\"}',1,'ruleset;showcase;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO homebrew_definitions (stable_id, campaign_id, ruleset_id, content_type, name, version, author, license, source, imported_at_utc, enabled, definition_json, tags, created_at, updated_at) VALUES "
             "('homebrew_feature_tour_pack'," + campaignRow + ",'ruleset_feature_tour_overlay','content_pack','Feature Tour Homebrew Pack','1.0','Codex Seed','CC0','Seeded local demo',datetime('now'),1,'{\"items\":[\"Starforged Rapier\",\"Moonlit Shield\"],\"spells\":[\"Ember Lance\",\"Warding Chorus\"],\"session\":\"Archive Breach\"}','homebrew;showcase;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO house_rules (stable_id, campaign_id, label, description, enabled, change_history, tags, created_at, updated_at) VALUES "
             "('house_rule_spell_fatigue'," + campaignRow + ",'Ritual Endurance','Spellcasters recover one used ritual slot after a short rest if at least 30 minutes are spent in controlled meditation.',1," + quote("") + "," + quote("house-rule;spellcasting;") + ",datetime('now'),datetime('now')),"
             "('house_rule_shops_open'," + campaignRow + ",'Harbor Market Hours','Shops keep all shop stock from respawning unless explicitly restocked by marksmanship missions.',1," + quote(std::string("2026-05-22T00:00:00") + '\x1f' + "Added special market cadence after long sea voyages." + '\x1e') + "," + quote("house-rule;economy;") + ",datetime('now'),datetime('now'));");
    }
    catch (...)
    {
        exec(db, "ROLLBACK;");
        sqlite3_close(db);
        throw;
    }

    exec(db, "COMMIT;");
    sqlite3_close(db);
}
}

int main(int argc, char **argv)
{
    try
    {
        if (argc < 2)
            throw std::runtime_error("Usage: seed_current_feature_tour <db> [db...]");
        for (int i = 1; i < argc; ++i)
            seedDatabase(argv[i]);
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
