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
    const std::string sql = "PRAGMA table_info(" + tableName + ")";
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
        escaped += (c == '\'') ? "''" : std::string(1, c);
    escaped += "'";
    return escaped;
}

std::string attrs(int str, int dex, int con, int intel, int wis, int cha)
{
    std::ostringstream out;
    out << "Strength:" << str << ";Dexterity:" << dex << ";Constitution:" << con
        << ";Intelligence:" << intel << ";Wisdom:" << wis << ";Charisma:" << cha << ";";
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
        if (index++)
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
    for (const auto &id : spellIds)
    {
        result += id;
        result += fieldSeparator;
        result += "1";
        result += fieldSeparator;
        result += "Campaign";
        result += fieldSeparator;
        result += "Prepared for resonance arc.";
        result += entrySeparator;
    }
    return result;
}

std::string action(const std::string &name, int reach, int range, const std::string &attack, const std::string &damage, const std::string &notes = "")
{
    constexpr char sep = '\x1f';
    return name + sep + std::to_string(reach) + sep + std::to_string(range) + sep + attack + sep + damage + sep + notes;
}

std::string actions(std::initializer_list<std::string> entries)
{
    constexpr char sep = '\x1e';
    std::string result;
    for (const auto &entry : entries)
        result += entry + sep;
    return result;
}

std::string mapCells(int width, int height, int edgeTerrain, int laneTerrain, int fieldTerrain)
{
    std::ostringstream out;
    bool first = true;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int terrain = fieldTerrain;
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
                terrain = edgeTerrain;
            else if ((x == 5 || x == width - 6) && y < height - 2)
                terrain = laneTerrain;

            if (!first)
                out << ",";
            first = false;
            out << terrain << ":0";
        }
    }
    return out.str();
}

void ensureSchema(sqlite3 *db)
{
    addColumnIfMissing(db, "campaigns", "party_vehicles", "TEXT");
    addColumnIfMissing(db, "campaigns", "party_strongholds", "TEXT");
    addColumnIfMissing(db, "campaigns", "party_renown", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "campaigns", "party_reputation", "TEXT");
    addColumnIfMissing(db, "campaigns", "last_modified", "DATETIME DEFAULT CURRENT_TIMESTAMP");

    addColumnIfMissing(db, "characters", "current_hp", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "characters", "max_hp", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "characters", "portrait_asset_id", "TEXT");
    addColumnIfMissing(db, "characters", "inventory_entries", "TEXT");
    addColumnIfMissing(db, "characters", "spellcasting_enabled", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "characters", "spellcasting_ability", "TEXT");
    addColumnIfMissing(db, "characters", "spell_attack_bonus", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "characters", "spell_save_dc", "INTEGER DEFAULT 10");
    addColumnIfMissing(db, "characters", "spell_slots_max", "TEXT");
    addColumnIfMissing(db, "characters", "spell_slots_used", "TEXT");
    addColumnIfMissing(db, "characters", "spellbook_entries", "TEXT");

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

    addColumnIfMissing(db, "story_beats", "cue_audio_asset_id", "TEXT");
    addColumnIfMissing(db, "story_beats", "cue_volume", "REAL DEFAULT 1.0");
    addColumnIfMissing(db, "story_beats", "cue_loop", "INTEGER DEFAULT 1");

    exec(db, "CREATE TABLE IF NOT EXISTS custom_spells ("
         "id INTEGER PRIMARY KEY,"
         "stable_id TEXT UNIQUE,"
         "campaign_id INTEGER NOT NULL,"
         "name TEXT NOT NULL,"
         "spell_level INTEGER DEFAULT 0,"
         "school INTEGER DEFAULT 4,"
         "resolution INTEGER DEFAULT 0,"
         "casting_time TEXT,"
         "spell_range TEXT,"
         "duration TEXT,"
         "concentration INTEGER DEFAULT 0,"
         "ritual INTEGER DEFAULT 0,"
         "verbal INTEGER DEFAULT 1,"
         "somatic INTEGER DEFAULT 1,"
         "material INTEGER DEFAULT 0,"
         "material_description TEXT,"
         "classes TEXT,"
         "attack_bonus TEXT,"
         "saving_throw_ability TEXT,"
         "damage TEXT,"
         "healing TEXT,"
         "area TEXT,"
         "effect_summary TEXT,"
         "scaling TEXT,"
         "description TEXT,"
         "tags TEXT,"
         "created_at TEXT,"
         "updated_at TEXT,"
         "FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE"
         ");");

    exec(db, "CREATE TABLE IF NOT EXISTS ruleset_definitions ("
         "id INTEGER PRIMARY KEY,"
         "stable_id TEXT UNIQUE,"
         "campaign_id INTEGER,"
         "name TEXT NOT NULL,"
         "version TEXT,"
         "base_ruleset_id TEXT,"
         "definition_json TEXT,"
         "installed INTEGER DEFAULT 0,"
         "tags TEXT,"
         "created_at TEXT,"
         "updated_at TEXT,"
         "FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE"
         ");");

    exec(db, "CREATE TABLE IF NOT EXISTS homebrew_definitions ("
         "id INTEGER PRIMARY KEY,"
         "stable_id TEXT UNIQUE,"
         "campaign_id INTEGER,"
         "ruleset_id TEXT,"
         "content_type TEXT NOT NULL,"
         "name TEXT NOT NULL,"
         "version TEXT,"
         "author TEXT,"
         "license TEXT,"
         "source TEXT,"
         "imported_at_utc TEXT,"
         "enabled INTEGER DEFAULT 1,"
         "definition_json TEXT,"
         "tags TEXT,"
         "created_at TEXT,"
         "updated_at TEXT,"
         "FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE"
         ");");
    addColumnIfMissing(db, "homebrew_definitions", "source", "TEXT");
    addColumnIfMissing(db, "homebrew_definitions", "imported_at_utc", "TEXT");
    addColumnIfMissing(db, "homebrew_definitions", "enabled", "INTEGER DEFAULT 1");

    exec(db, "CREATE TABLE IF NOT EXISTS story_documents ("
         "id INTEGER PRIMARY KEY,"
         "stable_id TEXT UNIQUE,"
         "campaign_id INTEGER NOT NULL,"
         "title TEXT NOT NULL,"
         "body TEXT,"
         "tags TEXT,"
         "created_at TEXT,"
         "updated_at TEXT,"
         "FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE"
         ");");

    exec(db, "CREATE TABLE IF NOT EXISTS timeline_events ("
         "id INTEGER PRIMARY KEY,"
         "stable_id TEXT UNIQUE,"
         "campaign_id INTEGER NOT NULL,"
         "title TEXT NOT NULL,"
         "description TEXT,"
         "in_world_date TEXT,"
         "related_entity_ids TEXT,"
         "tags TEXT,"
         "created_at TEXT,"
         "updated_at TEXT,"
         "FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE"
         ");");

    exec(db, "CREATE TABLE IF NOT EXISTS sessions ("
         "id INTEGER PRIMARY KEY,"
         "stable_id TEXT UNIQUE,"
         "campaign_id INTEGER NOT NULL,"
         "title TEXT NOT NULL,"
         "scheduled_at_utc TEXT,"
         "summary TEXT,"
         "participant_character_ids TEXT,"
         "encounter_ids TEXT,"
         "story_document_ids TEXT,"
         "live_mode_active INTEGER DEFAULT 0,"
         "tags TEXT,"
         "created_at TEXT,"
         "updated_at TEXT,"
         "FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE"
         ");");

    exec(db, "CREATE TABLE IF NOT EXISTS house_rules ("
         "id INTEGER PRIMARY KEY,"
         "stable_id TEXT UNIQUE,"
         "campaign_id INTEGER NOT NULL,"
         "label TEXT NOT NULL,"
         "description TEXT,"
         "enabled INTEGER DEFAULT 1,"
         "change_history TEXT,"
         "tags TEXT,"
         "created_at TEXT,"
         "updated_at TEXT,"
         "FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE"
         ");");
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

        const std::string campaignStableId = "campaign_neon_resonance_3_sessions";
        exec(db, "DELETE FROM campaigns WHERE stable_id = " + quote(campaignStableId) + ";");

        exec(db,
             "INSERT OR REPLACE INTO campaigns (stable_id, name, ruleset_name, description, tags, created_at, updated_at, party_map_id, party_x, party_y, party_vehicles, party_strongholds, party_renown, party_reputation, last_modified) VALUES (" +
                 quote(campaignStableId) + ", " +
                 quote("Neon Resonance: Three Signal Sessions") + ", " +
                 quote("Dungeons & Dragons 2024 (5.5e)") + ", " +
                 quote("Three session cyberpunk campaign with custom gods, resonant magic, and debt economy.") + ", " +
                 quote("cyberpunk;homebrew;3-session;resonance;money;") + ", " +
                 "datetime('now'), " +
                 "datetime('now'), " +
                 quote("map_skylane_surface") + ", " +
                 "4, 4, " +
                 quote("Rig: Echo-6;Carrier: Glassline;") + ", " +
                 quote("Neon Annex;Choir Annex;") + ", " +
                 "8, " +
                 quote("Skyline Watch:+1;Choir of Static:+2;") + ", " +
                 "datetime('now'));");

        const long long campaignRowId = sqlite3_last_insert_rowid(db);
        const std::string campaignRow = std::to_string(campaignRowId);

        exec(db, "INSERT OR REPLACE INTO inventories (stable_id, campaign_id, owner_entity_id, owner_type, name, copper, silver, gold, platinum, tags, created_at, updated_at) VALUES "
                    "('inv_party', " + campaignRow + ", " + quote(campaignStableId) + ", 'party', 'Party Treasury', 2200, 70, 4, 1, 'currency;treasury;', datetime('now'), datetime('now'));" );

        const std::string mapSurfaceCells = mapCells(18, 10, 5, 2, 0);
        const std::string mapSkirmishCells = mapCells(10, 6, 5, 2, 0);
        const std::string mapChoirChamberCells = mapCells(12, 10, 7, 4, 1);
        const std::string mapLiftCells = mapCells(14, 8, 6, 3, 0);

        const std::string rulesetJson = R"JSON(
{
  "system": "Neon Resonance",
  "version": "1.0",
  "abilities": ["Signal","Circuit","Grit","Memory","Presence","Wit"],
  "currency": {"kry":1,"mn":25,"hz":150},
  "focus": "When Resonance spellcasting fails, add Debt Marker (max 2 per caster).",
  "debtMechanic": "Each Debt Marker can be cleared by spending 1 MN during a social scene, or 1 HZ during a ritual climax."
}
)JSON";

        const std::string packJson = R"JSON(
{
  "packName": "Neon Resonance Campaign Pack",
  "version": "1.0",
  "features": ["three-currency economy", "debt markers", "choir deity rituals", "lift city arc"],
  "debtResolution": "One honest proof reduces one Debt Marker.",
  "warning": "HZ can only be spent in ritual scenes or unstable combat."
}
)JSON";

        exec(db,
             "INSERT OR REPLACE INTO ruleset_definitions (stable_id, campaign_id, name, version, base_ruleset_id, definition_json, installed, tags, created_at, updated_at) VALUES (" +
                 quote("ruleset_neon_resonance") + ", " + campaignRow + ", " + quote("Neon Resonance Overlay") + ", " + quote("1.0") + ", " + quote("dnd2024") + ", " + quote(rulesetJson) + ", 1, "
                 + quote("homebrew;resonance;") + ", datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO homebrew_definitions (stable_id, campaign_id, ruleset_id, content_type, name, version, author, license, source, imported_at_utc, enabled, definition_json, tags, created_at, updated_at) VALUES (" +
                 quote("homebrew_pack_neon_resonance") + ", " + campaignRow + ", " + quote("ruleset_neon_resonance") + ", "
                 + quote("content_pack") + ", " + quote("Neon Resonance Campaign Pack") + ", " + quote("1.0") + ", "
                 + quote("Editor Forge") + ", " + quote("CC BY 4.0") + ", " + quote("Seed content") + ", datetime('now'), 1, " + quote(packJson) + ", "
                 + quote("campaign-pack;economy;debt;resonance;") + ", datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO custom_items (stable_id, campaign_id, name, description, ruleset_key, category, item_type, rarity, weight, base_value, stackable, armor_class_bonus, damage_reduction, weapon_attack_bonus, weapon_damage, weapon_ability, weapon_reach_feet, weapon_range_feet, effects, tags, created_at, updated_at) VALUES "
             "('item_kry_credit', " + campaignRow + ", 'Kry Credit', 'Core money unit.', 'dnd2024', 'Money', 'Currency', 0, 0, 1, 1, 0, 0, 0, '', 'Neutral', 5, 5, 'Base currency.', 'money;core;', datetime('now'), datetime('now')) ,"
             "('item_mn_shard', " + campaignRow + ", 'Mnemonic Shard', 'Collateral shard worth 25 Kry.', 'dnd2024', 'Money', 'Currency', 1, 0.2, 25, 1, 0, 0, 0, '', 'Neutral', 5, 5, 'Collateral and ritual component.', 'money;collateral;', datetime('now'), datetime('now')) ,"
             "('item_haze_token', " + campaignRow + ", 'Haze Token', 'Volatile debt-credit (150 Kry) only for rituals.', 'dnd2024', 'Money', 'Currency', 2, 0.1, 150, 1, 0, 0, 0, '', 'Neutral', 5, 5, 'Unstable and risky.', 'money;volatile;', datetime('now'), datetime('now')) ,"
             "('item_signal_saber', " + campaignRow + ", 'Signal Saber', 'A humming one-handed resonant weapon.', 'dnd2024', 'Weapon', 'Weapon', 2, 2.4, 420, 0, 0, 0, 1, '1d8 radiant', 'Strength', 5, 5, 'Marks spectral entities for Resonance follow-up.', 'weapon;combat;', datetime('now'), datetime('now')) ,"
             "('item_neural_jack', " + campaignRow + ", 'Neural Jack', 'Field interface for hardline operations.', 'dnd2024', 'Gadget', 'Tool', 1, 0.6, 500, 0, 0, 0, 0, '', 'Intelligence', 5, 5, 'Increases Signal checks.', 'gear;', datetime('now'), datetime('now')) ,"
             "('item_resonant_vest', " + campaignRow + ", 'Resonant Vest', 'Protective resonant weave armor.', 'dnd2024', 'Armor', 'Armor', 3, 5.0, 820, 0, 2, 1, 0, '', 'Constitution', 5, 5, 'Slightly dampens ritual backlash.', 'armor;defense;', datetime('now'), datetime('now')) ,"
             "('item_choir_baton', " + campaignRow + ", 'Choir Baton', 'Ritual focus used by Choir rites.', 'dnd2024', 'Focus', 'Relic', 3, 1.2, 900, 0, 0, 0, 0, '', 'Wisdom', 5, 5, 'Required for session 3 ritual checks.', 'focus;relic;', datetime('now'), datetime('now')) ,"
             "('item_trace_chip', " + campaignRow + ", 'Trace Chip', 'Tracks last route through one relay node.', 'dnd2024', 'Tool', 'Tool', 1, 0.1, 80, 1, 0, 0, 0, '', 'Intelligence', 5, 5, 'Used for investigation and routing.', 'tool;investigate;', datetime('now'), datetime('now')) ,"
             "('item_echo_mask', " + campaignRow + ", 'Echo Mask', 'Dampens noise and improves social outcomes in ritual rooms.', 'dnd2024', 'Armor', 'Wondrous', 1, 0.9, 280, 0, 1, 0, 0, '', 'Constitution', 5, 5, 'Presence and ritual checks improve.', 'gear;social;', datetime('now'), datetime('now')) ,"
             "('item_ether_stim', " + campaignRow + ", 'Ether Stim', 'Single-use healing stim.', 'dnd2024', 'Consumable', 'Potion', 1, 0.2, 120, 1, 0, 0, 0, '', 'Constitution', 5, 5, 'Recover burst health.', 'consumable;', datetime('now'), datetime('now'))"
        );

        exec(db,
             "INSERT OR REPLACE INTO inventory_entries (inventory_id, stable_id, item_id, container_entry_id, quantity, equipped, slot, attuned, charges, ammunition_remaining, durability, state_notes, sort_order) "
             "SELECT id, 'party_money', 'item_kry_credit', '', 45, 0, 'Vault', 0, -1, 0, -1, 'Party base funds.', 0 FROM inventories WHERE stable_id='inv_party' UNION ALL "
             "SELECT id, 'party_collateral', 'item_mn_shard', '', 3, 0, 'Vault', 0, -1, 0, -1, 'Collateral reserve for rituals.', 1 FROM inventories WHERE stable_id='inv_party' UNION ALL "
             "SELECT id, 'party_haze', 'item_haze_token', '', 1, 0, 'Vault', 0, -1, 0, -1, 'Volatile emergency token.', 2 FROM inventories WHERE stable_id='inv_party';"
        );

        exec(db,
             "INSERT OR REPLACE INTO custom_spells (stable_id, campaign_id, name, spell_level, school, resolution, casting_time, spell_range, duration, concentration, ritual, verbal, somatic, material, material_description, classes, attack_bonus, saving_throw_ability, damage, healing, area, effect_summary, scaling, description, tags, created_at, updated_at) VALUES "
             "('spell_signal_lock', " + campaignRow + ", 'Signal Lock', 1, 0, 0, '1 Action', '120 ft', '1 minute', 0, 0, 1, 1, 0, '', 'Signal Ritualist,Fighter', '6', 'INT', '1d8 psychic', '', 'Cone 20 ft', 'Pins one target to a route.', 'N/A', 'Anchor target and force one response route.', 'combat;resonance;', datetime('now'), datetime('now')) ,"
             "('spell_debt_bind', " + campaignRow + ", 'Debt Bind', 2, 2, 1, '1 Action', '60 ft', '1 minute', 1, 0, 1, 1, 1, '1 Mn Shard', 'Signal Ritualist,Cleric', '6', 'WIS', '1d8 necrotic', '', 'Single target', 'Adds debt marker and control pressure.', 'Each round increases by 1.', 'One target has reduced options until they settle debt.', 'control;social;', datetime('now'), datetime('now')) ,"
             "('spell_echo_step', " + campaignRow + ", 'Echo Step', 0, 1, 0, 'Bonus Action', 'Self', 'Instant', 0, 0, 0, 1, 0, '', 'Rogue,Signal Ritualist', '3', 'DEX', '', '', '5 ft', 'Phase-shift movement.', 'No scaling.', 'Move through a residual signal edge.', 'mobility;resonance;', datetime('now'), datetime('now')) ,"
             "('spell_static_ward', " + campaignRow + ", 'Static Ward', 2, 3, 0, '1 Action', 'Self', '10 minutes', 1, 0, 1, 1, 0, '', 'Neural Architect,Paladin', '5', 'WIS', '', '', '15 ft aura', 'Partial protection from disruption.', 'Increase by one radius level each slot.', 'Resonant field protects social and control checks.', 'defense;buff;', datetime('now'), datetime('now')) ,"
             "('spell_echo_trace', " + campaignRow + ", 'Echo Trace', 1, 4, 0, '10 minutes', 'Self', '8 hours', 0, 1, 1, 0, 1, '1 Mn Shard', 'Cleric,Ranger', '0', 'WIS', '', '', 'Object target', 'Track recent handlers of one object.', 'One extra scene if used in ritual.', 'Learn last 3 route contacts connected to item.', 'investigation;ritual;', datetime('now'), datetime('now')) ,"
             "('spell_ghostline_surge', " + campaignRow + ", 'Ghostline Surge', 3, 2, 0, '1 Action', '30 ft', 'Concentration 1 minute', 1, 0, 1, 1, 0, '', 'Bard,Signal Ritualist', '7', 'CON', '4d8 lightning', '', '20 ft burst', 'Cloud damage against incorporeal/constructs.', 'Sustainment decreases by 1 each round.', 'Forces a re-roll for one resistance each wave.', 'offense;climax;', datetime('now'), datetime('now')) ,"
             "('spell_ritual_repair', " + campaignRow + ", 'Ritual Repair', 2, 5, 0, '10 minutes', 'Touch', '1 hour', 1, 1, 1, 1, 1, '1 Haze Token', 'Cleric,Neural Architect', '0', 'CON', '0', 'Restore 1d8+5 structure or object health', '', 'Repairs one object each aid check cycle.', 'No scaling.', 'Useful on drones, doors, and glyph components.', 'restoration;ritual;', datetime('now'), datetime('now'))"
        );

        exec(db,
             "INSERT OR REPLACE INTO characters (stable_id, campaign_id, name, level, class, race, background, attributes, tags, created_at, updated_at, backstory, current_hp, max_hp, portrait_asset_id, inventory_entries, spellcasting_enabled, spellcasting_ability, spell_attack_bonus, spell_save_dc, spell_slots_max, spell_slots_used, spellbook_entries) VALUES "
             "('pc_kyro', " + campaignRow + ", 'Kyro Vale', 4, 'Signal Ritualist', 'Human', 'Former relay inspector', " + quote(attrs(12,16,14,16,11,13)) + ", 'caster;core;', datetime('now'), datetime('now'), 'Kyro keeps people alive through math and mercy.', 32, 34, 'Resources/Campaigns/campaign_current_feature_tour/Assets/Portraits/char_lio.bmp', " + quote(inv({"item_signal_saber:1:1:Main Hand", "item_neural_jack:1:0:Backpack", "item_kry_credit:20:0:Pouch", "item_mn_shard:1:0:Pouch"})) + ", 1, 'INT', 6, 14, " + quote(slots({4,4,3,3,0,0,0,0,0,0})) + ", " + quote(slots({0,0,1,0,0,0,0,0,0,0})) + ", " + quote(spellbook({"spell_signal_lock","spell_echo_step","spell_debt_bind"})) + "),"
             "('pc_mara', " + campaignRow + ", 'Mara Kest', 3, 'Neural Architect', 'Elf', 'Street medic', " + quote(attrs(8,14,13,15,14,12)) + ", 'support;healer;', datetime('now'), datetime('now'), 'Mara knows exactly what can be fixed and what cannot.', 24, 25, 'Resources/Campaigns/campaign_current_feature_tour/Assets/Portraits/char_seren.bmp', " + quote(inv({"item_ritual_candle:1:0:Pouch", "item_ether_stim:2:0:Pouch", "item_trace_chip:1:0:Case", "item_kry_credit:30:0:Pouch"})) + ", 1, 'MEM', 5, 13, " + quote(slots({4,4,3,2,0,0,0,0,0,0})) + ", " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", " + quote(spellbook({"spell_echo_trace","spell_static_ward","spell_ritual_repair"})) + "),"
             "('pc_raen', " + campaignRow + ", 'Raen Iron', 4, 'Fighter', 'Half-Orc', 'Security ex-enforcer', " + quote(attrs(17,12,16,9,12,8)) + ", 'frontliner;tank;', datetime('now'), datetime('now'), 'Raen protects everyone and asks few questions.', 44, 46, 'Resources/Campaigns/campaign_current_feature_tour/Assets/Portraits/char_thorne.bmp', " + quote(inv({"item_resonant_vest:1:1:Body", "item_signal_saber:1:0:Backup", "item_kry_credit:40:0:Pouch"})) + ", 0, 'STR', 0, 10, " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", ''),"
             "('pc_ixa', " + campaignRow + ", 'Ixa Flux', 3, 'Rogue', 'Tiefling', 'Courier', " + quote(attrs(11,18,12,14,13,11)) + ", 'deals;stealth;', datetime('now'), datetime('now'), 'Ixa trades favors and exits before anyone notices.', 28, 29, 'Resources/Campaigns/campaign_current_feature_tour/Assets/Portraits/char_mira.bmp', " + quote(inv({"item_trace_chip:2:1:Utility", "item_kry_credit:60:0:Pouch", "item_echo_mask:1:0:Face"})) + ", 0, 'DEX', 0, 10, " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", ''),"
             "('pc_tan', " + campaignRow + ", 'Tan Milo', 4, 'Bard', 'Half-Elf', 'Interpreter', " + quote(attrs(10,13,12,14,16,15)) + ", 'social;ritual;', datetime('now'), datetime('now'), 'Tan Milo is the best mediator and worst liar in the city.', 30, 31, 'Resources/Campaigns/campaign_current_feature_tour/Assets/Portraits/char_seren.bmp', " + quote(inv({"item_choir_baton:1:1:Hand", "item_kry_credit:35:0:Pouch", "item_mn_shard:1:0:Pouch"})) + ", 1, 'CHA', 4, 12, " + quote(slots({4,4,2,2,0,0,0,0,0,0})) + ", " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", " + quote(spellbook({"spell_echo_step","spell_static_ward","spell_debt_bind"})) + ")"
        );

        exec(db,
             "INSERT OR REPLACE INTO maps (stable_id, campaign_id, name, description, scope, grid_type, width, height, cell_size, is_main_map, parent_map_id, cells, tags, created_at, updated_at) VALUES "
             "('map_skylane_surface', " + campaignRow + ", 'Skylane Surface', 'Overhead map of tower run corridors and signal alleys where the party starts.', 1, 0, 18, 10, 1.0, 1, '', " + quote(mapSurfaceCells) + ", 'surface;district;start;', datetime('now'), datetime('now')),"
             "('map_choir_chamber', " + campaignRow + ", 'Glass Choir Chamber', 'Main ritual hall beneath the city spine.', 2, 0, 12, 10, 1.0, 0, 'map_skylane_surface', " + quote(mapChoirChamberCells) + ", 'dungeon;ritual;', datetime('now'), datetime('now')),"
             "('map_lift_core', " + campaignRow + ", 'Lift Core', 'Restricted core deck where the final resonance choice is made.', 3, 0, 14, 8, 1.0, 0, 'map_skylane_surface', " + quote(mapLiftCells) + ", 'boss;core;final;', datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO factions (stable_id, campaign_id, name, faction_type, summary, headquarters, leader_npc_id, attitude_toward_party, goals, resources, gm_notes, influence, threat_level, tags, created_at, updated_at) VALUES "
             "('faction_bloodline_coil', " + campaignRow + ", 'Bloodline Coil', 'Corp', 'A megacorp that controls most public towers and debt ledgers tied to the party.', 'Upper Lift Administration', '', 'Wary and transactional', 'Monetize every failure and keep people dependent.', 'Debt records, tower permits, drone patrol access', 'The faction can switch from ally to antagonist if public stability is endangered.', 12, 3, 'corporation;control;', datetime('now'), datetime('now')),"
             "('faction_choir_of_static', " + campaignRow + ", 'Choir of Static', 'Cult', 'A secretive ritual order that treats signal storms as scripture.', 'Subbasement Choir Hall', 'npc_oracle_static', 'Cryptic and selective', 'Protect the old resonance spine and punish exploitation.', 'Ritual circles, encrypted prayer loops, relics', 'A faction whose favor can make debt vanish in public scenes.', 20, 2, 'cult;ritual;mystery;', datetime('now'), datetime('now')),"
             "('faction_street_singers', " + campaignRow + ", 'Street Singers', 'Collective', 'Information network of couriers and hackers linked to smuggling routes.', 'Nightline Arcade', 'npc_ixa_broker', 'Pragmatic and hungry for leverage', 'Move data and debt ledgers for a price.', 'False IDs, route maps, safehouses', 'A volatile ally network; reputation can open logistics support.', 14, 2, 'underground;broker;', datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO deities (stable_id, campaign_id, name, power_type, pantheon, domains, symbol, alignment, tenets, worshipers, relationships, gm_notes, influence, threat_level, tags, created_at, updated_at) VALUES "
             "('deity_static_mother', " + campaignRow + ", 'Static Mother', 'Deity', 'Signal Pantheon', 'Truth, Debt, Interference', 'Fractured ear ring', 'Neutral', 'Balance debt records with mercy, then rewrite the city.', 'Signal priests, debt clerks, ritualists', 'Frequent conflict with corporate authority structures.', 'She prefers quiet bargains made at midnight alarms.', 28, 4, 'deity;resonance;', datetime('now'), datetime('now')),"
             "('deity_circuit_hound', " + campaignRow + ", 'Circuit Hound', 'Ascended Spirit', 'Street Pantheon', 'Data, Pursuit, Loyalty', 'One eye in a black lens', 'Neutral Evil', 'Never trust a clean ledger, never trust an empty room.', 'Scanners, auditors, city watchers', 'Opposed by Static Mother during unstable ritual events.', 'Appears through repeated system failures and dead-zone hums.', 18, 3, 'deity;spirit;', datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO npcs (stable_id, campaign_id, category, faction_id, deity_id, name, description, faction, role, attitude_toward_party, secrets, goals, resources, threat_location, relationships, threat_level, dialogue_notes, attributes, conditions, tags, created_at, updated_at, inventory_entries, spellcasting_enabled, spellcasting_ability, spell_attack_bonus, spell_save_dc, spell_slots_max, spell_slots_used, spellbook_entries) VALUES "
             "('npc_captain_hall', " + campaignRow + ", 0, 'faction_bloodline_coil', '', 'Captain Veyre Hall', 'Security lieutenant who can force access or block it.', 'Bloodline Coil', 'Checkpoint Authority', 'Demanding but not cruel', 'Captain Hall has a backup keycode tied to debt registry approvals.', 'Delay bloodline inspection until the public square is secure.', 'Drone patrol routes, legal warrants', 'Skylane Surface Gate A', 'Works through a network of inspectors.', 3, 'I can clear your paperwork in ten minutes, or I can make ten inspectors look at you for a week.', " + quote(attrs(13, 12, 14, 11, 14, 9)) + ", '', 'authority;law;', datetime('now'), datetime('now'), " + quote(inv({"item_trace_chip:1:0:Pocket"})) + ", 0, '', 0, 10, " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", ''),"
             "('npc_oracle_static', " + campaignRow + ", 0, 'faction_choir_of_static', 'deity_static_mother', 'Oracle Vex', 'Oracle tied to the Choir. They appear calm until debt spikes.', 'Choir of Static', 'Ritual Advisor', 'Elliptical and urgent', 'Oracle has never paid debt in coin, only in service.', 'Keep the resonance channels open and stop the Hound run.', 'Choir prayer loops, storm maps', 'Glass Choir Chamber', 'Trusted by Captain Hall from old debt ties.', 2, 'You came chasing a signal. You leave with a question that should not be answered.', " + quote(attrs(9, 13, 12, 17, 16, 11)) + ", 'Concentrating;', 'prophet;ally;ritual;', datetime('now'), datetime('now'), " + quote(inv({"item_choir_baton:1:0:Table"})) + ", 1, 'WIS', 7, 13, " + quote(slots({0,3,0,1,0,0,0,0,0,0})) + ", " + quote(slots({0,1,0,0,0,0,0,0,0,0})) + ", " + quote(spellbook({"spell_static_ward", "spell_echo_trace"})) + "),"
             "('npc_ixa_broker', " + campaignRow + ", 0, 'faction_street_singers', '', 'Ixa Broker', 'Courier intermediary from the Street Singers, moving favors between factions.', 'Street Singers', 'Information Broker', 'Helpful if your payment is clean.', 'Keeps a ledger copied from three towers and two docks.', 'Secure a debt line through Choir archives.', 'False identities, route maps, safehouses', 'Nightline Arcade', 'Might betray to protect their own route network.', 1, 'You want a favor, I want your silence and your timing.', " + quote(attrs(13, 17, 11, 14, 13, 10)) + ", '', 'broker;rival;', datetime('now'), datetime('now'), " + quote(inv({"item_trace_chip:3:0:Satchel"})) + ", 0, '', 0, 10, " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", ''),"
             "('npc_hound_scout', " + campaignRow + ", 1, 'faction_bloodline_coil', 'deity_circuit_hound', 'Circuit Hound Scout', 'A modified skulk scout sent by the Circuit Hound.', 'Bloodline Coil', 'Pursuit Unit', 'Clinical and hostile', 'Tracking script is linked to the campaign debt registry.', 'Identify who carried the Resonance Baton.', 'Drone command chain, pursuit software', 'Lower Lift Spine', 'No direct chain of command documented in public logs.', 5, 'Target identity is secondary; asset disruption is mandatory.', " + quote(attrs(14, 15, 14, 11, 12, 11)) + ", 'Poisoned;', 'beast;pursuit;', datetime('now'), datetime('now'), '', 0, '', 0, 10, " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", " + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ", '');"
        );

        exec(db,
             "INSERT OR REPLACE INTO custom_monsters (stable_id, campaign_id, name, source, size, creature_type, challenge_rating, armor_class, armor_damage_reduction, hit_points, speed_feet, experience, attributes, actions, tags, created_at, updated_at) VALUES "
             "('monster_signal_stalker', " + campaignRow + ", 'Signal Stalker', 'Campaign', 'Small', 'Construct', '1', 13, 1, 17, 25, 35, " + quote(attrs(12, 18, 13, 7, 14, 8)) + ", " + quote(actions({action("Pulse Maul", 5, 5, "+5", "1d6 thunder"), action("Phase Sprint", 5, 0, "+6", "Movement", "Ignores difficult terrain for one round")})) + ", 'construct;tracker;', datetime('now'), datetime('now')),"
             "('monster_circuit_warren', " + campaignRow + ", 'Circuit Warren', 'Campaign', 'Medium', 'Aberration', '2', 14, 2, 35, 30, 80, " + quote(attrs(16, 12, 14, 10, 13, 9)) + ", " + quote(actions({action("Interference Lance", 5, 45, "+6", "2d6 force"), action("Warden Pulse", 5, 5, "+5", "1d4 psychic", "Targets one enemy and lowers next roll"), action("Static Lash", 5, 5, "+4", "1d8 lightning")})) + ", 'aberration;elite;', datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO encounters (stable_id, campaign_id, name, description, map_id, map_context_name, map_context_x, map_context_y, battlefield_width, battlefield_height, cell_size_feet, use_map_cells, battlefield_cells, round, active_participant_index, tags, created_at, updated_at) VALUES "
             "('enc_skylane_probe', " + campaignRow + ", 'Neon Probe Raid', 'Early-session skirmish that introduces tracker discipline and debt escalation.', 'map_skylane_surface', 'Skylane Surface', 3, 5, 10, 6, 5, 1, " + quote(mapSkirmishCells) + ", 1, 0, 'combat;scouting;', datetime('now'), datetime('now')),"
             "('enc_choir_undertow', " + campaignRow + ", 'Choir Undertow', 'Mid-session ritual room encounter with Choir guardians and signal traps.', 'map_choir_chamber', 'Glass Choir Chamber', 2, 2, 12, 10, 5, 1, " + quote(mapChoirChamberCells) + ", 1, 0, 'ritual;combat;ward;', datetime('now'), datetime('now')),"
             "('enc_lift_core', " + campaignRow + ", 'Lift Core Overture', 'Final session climax with a boss threat and debt reckoning.', 'map_lift_core', 'Lift Core', 5, 3, 14, 8, 5, 1, " + quote(mapLiftCells) + ", 1, 0, 'boss;ritual;social-climax;', datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO encounter_participants (stable_id, encounter_id, kind, source_entity_id, monster_definition_id, display_name, initiative, current_hp, max_hp, armor_class, armor_damage_reduction, speed_feet, grid_x, grid_y, turn_start_x, turn_start_y, movement_used_feet, defeated, friendly, conditions, actions, tags, created_at, updated_at) SELECT 'probe_kyro', id, 0, 'pc_kyro', '', 'Kyro Vale', 17, 32, 34, 15, 0, 30, 2, 2, 2, 2, 0, 0, 1, '', " + quote(actions({action("Signal Lock", 5, 120, "+6", "1d8 psychic"), action("Echo Step", 5, 5, "+8", "2d4 radiant")})) + ", 'pc;caster;' , datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_skylane_probe' UNION ALL "
             "SELECT 'probe_ixa', id, 0, 'pc_ixa', '', 'Ixa Flux', 16, 28, 29, 16, 0, 30, 4, 1, 4, 1, 0, 0, 1, '', " + quote(actions({action("Tracer Volley", 5, 40, "+8", "1d6+4", "Once per round if a map edge is clear.")})) + ", 'pc;striker;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_skylane_probe' UNION ALL "
             "SELECT 'probe_stalker', id, 2, '', 'monster_signal_stalker', 'Signal Stalker', 12, 17, 17, 13, 1, 30, 8, 1, 8, 1, 0, 0, 0, '', " + quote(actions({action("Phase Tether", 5, 0, "+4", "Restrain", "Pulls one hostile by resonance"), action("Pulse Maul", 5, 5, "+5", "1d6 thunder")})) + ", 'monster;scout;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_skylane_probe';"
        );

        exec(db,
             "INSERT OR REPLACE INTO encounter_participants (stable_id, encounter_id, kind, source_entity_id, monster_definition_id, display_name, initiative, current_hp, max_hp, armor_class, armor_damage_reduction, speed_feet, grid_x, grid_y, turn_start_x, turn_start_y, movement_used_feet, defeated, friendly, conditions, actions, tags, created_at, updated_at) SELECT 'choir_oracle', id, 1, 'npc_oracle_static', '', 'Oracle Vex', 15, 24, 24, 12, 0, 30, 3, 3, 3, 3, 0, 0, 1, '', " + quote(actions({action("Ward Chorus", 5, 30, "+4", "Manual", "Grant resistance when debt is paid."), action("Static Bind", 5, 20, "+4", "1d8 psychic")})) + ", 'npc;caster;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_choir_undertow' UNION ALL "
             "SELECT 'choir_warren', id, 2, '', 'monster_circuit_warren', 'Circuit Warren', 12, 35, 35, 14, 2, 30, 6, 4, 6, 4, 0, 0, 0, '', " + quote(actions({action("Interference Lance", 5, 45, "+6", "2d6 force", "Target one foe"), action("Warden Pulse", 5, 5, "+5", "1d4 psychic")})) + ", 'monster;guardian;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_choir_undertow' UNION ALL "
             "SELECT 'choir_stalker', id, 2, '', 'monster_signal_stalker', 'Signal Stalker', 13, 17, 17, 13, 1, 30, 7, 6, 7, 6, 0, 0, 0, '', " + quote(actions({action("Phase Tether", 5, 0, "+4", "Restrain", "Pulls one hostile by resonance"), action("Pulse Maul", 5, 5, "+5", "1d6 thunder")})) + ", 'monster;scout;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_choir_undertow';"
        );

        exec(db,
             "INSERT OR REPLACE INTO encounter_participants (stable_id, encounter_id, kind, source_entity_id, monster_definition_id, display_name, initiative, current_hp, max_hp, armor_class, armor_damage_reduction, speed_feet, grid_x, grid_y, turn_start_x, turn_start_y, movement_used_feet, defeated, friendly, conditions, actions, tags, created_at, updated_at) SELECT 'core_mara', id, 0, 'pc_mara', '', 'Mara Kest', 12, 24, 25, 13, 0, 30, 4, 2, 4, 2, 0, 0, 1, '', " + quote(actions({action("Static Ward", 5, 5, "+5", "N/A", "Protect the ritual perimeter for one round"), action("Ritual Repair", 5, 5, "0", "0")})) + ", 'pc;healer;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_lift_core' UNION ALL "
             "SELECT 'core_raen', id, 0, 'pc_raen', '', 'Raen Iron', 14, 44, 46, 16, 1, 30, 5, 2, 5, 2, 0, 0, 1, '', " + quote(actions({action("Resonant Smash", 5, 5, "+7", "1d8+4 bludgeoning")})) + ", 'pc;tank;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_lift_core' UNION ALL "
             "SELECT 'core_tan', id, 0, 'pc_tan', '', 'Tan Milo', 11, 30, 31, 13, 0, 30, 5, 3, 5, 3, 0, 0, 1, '', " + quote(actions({action("Resonant Speech", 0, 0, "+4", "Social Edge", "Gain tactical advantage on one ally."), action("Echo Step", 5, 5, "+4", "N/A")})) + ", 'pc;support;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_lift_core' UNION ALL "
             "SELECT 'core_warren', id, 2, '', 'monster_circuit_warren', 'Circuit Warren', 13, 35, 35, 14, 2, 30, 7, 6, 7, 6, 0, 0, 0, '', " + quote(actions({action("Interference Lance", 5, 45, "+6", "2d6 force", "Each round, targets one action"), action("Static Flood", 5, 5, "+5", "3d8 psychic")})) + ", 'boss;ritual;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_lift_core';"
         );

        exec(db,
             "INSERT OR REPLACE INTO story_documents (stable_id, campaign_id, title, body, tags, created_at, updated_at) VALUES "
             "('doc_session1_prep', " + campaignRow + ", 'Session 1 Notes', 'The party lands in Skylane District and investigates a suspicious signal ping at dawn. Use scene pacing: one social scene, one investigation roll, then a skirmish at the probe tower.', 'session;prep;outline;', datetime('now'), datetime('now')),"
             "('doc_session2_prep', " + campaignRow + ", 'Session 2 Notes', 'The group is routed toward the Glass Choir. Run this as a ritual-heavy scene before combat; the Choir is open but not fully welcoming.', 'session;ritual;prep;', datetime('now'), datetime('now')),"
             "('doc_session3_prep', " + campaignRow + ", 'Session 3 Notes', 'Final deck sequence. Debts rise quickly, and a debt payment changes social consequences.', 'session;climax;debt;', datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO timeline_events (stable_id, campaign_id, title, description, in_world_date, related_entity_ids, tags, created_at, updated_at) VALUES "
             "('timeline_ignition', " + campaignRow + ", 'Liftline Ignition', 'The party accepts first official access to Sky Tower and learns of missing relay data.', 'Night 1', 'pc_kyro;npc_captain_hall;map_skylane_surface', 'investigation;setup;', datetime('now'), datetime('now')),"
             "('timeline_choir_gate', " + campaignRow + ", 'Choir Gate Opens', 'A hidden rite reveals a debt-linked ritual chamber under the tower spine.', 'Night 2', 'faction_choir_of_static;npc_oracle_static;map_choir_chamber', 'ritual;world;', datetime('now'), datetime('now')),"
             "('timeline_lift_core', " + campaignRow + ", 'Core Resonance Fracture', 'A final debt reckoning triggers combat at the lift core where power, faith, and debt collide.', 'Night 3', 'enc_lift_core;deity_static_mother;deity_circuit_hound', 'climax;combat;', datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO story_threads (stable_id, campaign_id, title, thread_type, status, summary, stakes, gm_notes, tags, created_at, updated_at) VALUES "
             "('thread_main_resonance', " + campaignRow + ", 'A Debt-Wired Liftline', 0, 2, 'Trace the stolen relay and decide whether to pay, negotiate, or weaponize debt.', 'If the party fails, the Resonance Spine destabilizes and public trust breaks.', 'Track debt, ritual pressure, and faction responses.', 'main;campaign;', datetime('now'), datetime('now')),"
             "('thread_choir', " + campaignRow + ", 'Choir of Static', 3, 2, 'Expose who in the Choir is guiding the old order.', 'Static Mother may close the ritual loop if debt is settled badly.', 'Watch for NPC oracle loyalties and ritual clues.', 'faction;ritual;', datetime('now'), datetime('now')),"
             "('thread_ixa_broker', " + campaignRow + ", 'Broker Lines', 1, 1, 'Ixa Broker has two debts to pay and one of them is the party.', 'If trust is broken with Street Singers, transport options close.', 'Use this as a pressure thread for negotiation scenes.', 'character;street;', datetime('now'), datetime('now')),"
             "('thread_session1', " + campaignRow + ", 'Session 1: Probe and Pursuit', 7, 1, 'Introduce the setting and establish debt mechanics.', 'Party should feel pressure from the first debt marker.', 'Use this thread to pace social and combat beats.', 'session;1;', datetime('now'), datetime('now')),"
             "('thread_session2', " + campaignRow + ", 'Session 2: Choir Exposure', 7, 1, 'Bring the team into the Choir and a hidden ritual.', 'Failing here should force an unstable debt marker.', 'Use this thread for one social twist before combat.', 'session;2;', datetime('now'), datetime('now')),"
             "('thread_session3', " + campaignRow + ", 'Session 3: Lift Core Bargain', 7, 1, 'Resolve debt, faction standing, and core resonance outcome.', 'Failure gives immediate long-term economic consequences.', 'Keep session choices visible in post-session summaries.', 'session;3;', datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_probe_tension', id, 'A Tower Without Trust', 'Captain Hall opens access but warns debt marks will be logged from every failed negotiation.', 'Cue social pressure and begin first debt marker pacing.', 1, 'Night 1', '', 0.0, 0, 'social;debt;', datetime('now'), datetime('now'), 0 FROM story_threads WHERE stable_id='thread_session1';"
        );
        exec(db,
             "INSERT OR REPLACE INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_choir_disclosure', id, 'The Choir Speaks in Runes', 'Oracle Vex reveals the debt ledger is encrypted in rituals at the choir chamber.', 'Shift trust: choose ally or leverage.', 1, 'Night 2', '', 0.0, 0, 'ritual;revelation;', datetime('now'), datetime('now'), 0 FROM story_threads WHERE stable_id='thread_session2';"
        );
        exec(db,
             "INSERT OR REPLACE INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_core_balance', id, 'Choose a Resonance Cost', 'The lift core opens and every side demands payment or compliance.', 'Climax point: a debt payment can turn one monster action into an ally action.', 1, 'Night 3', '', 0.0, 0, 'climax;choice;', datetime('now'), datetime('now'), 0 FROM story_threads WHERE stable_id='thread_session3';"
        );

        exec(db,
             "INSERT OR REPLACE INTO story_links (stable_id, campaign_id, owner_entity_id, owner_kind, link_type, label, target_entity_id, target_entity_type, tags, created_at, updated_at) VALUES "
             "('link_probe_map', " + campaignRow + ", 'thread_session1', 0, 1, 'links scene to map', 'map_skylane_surface', 'map', 'guide;', datetime('now'), datetime('now')),"
             "('link_choir_oracle', " + campaignRow + ", 'thread_choir', 0, 1, 'oracle guidance', 'npc_oracle_static', 'npc', 'guide;', datetime('now'), datetime('now')),"
             "('link_lift_core_enc', " + campaignRow + ", 'thread_session3', 0, 3, 'resolves with encounter', 'enc_lift_core', 'encounter', 'climax;', datetime('now'), datetime('now')),"
             "('link_debt_mechanic', " + campaignRow + ", 'thread_main_resonance', 0, 0, 'debt thread', 'campaign_neon_resonance_3_sessions', 'campaign', 'rule;', datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO sessions (stable_id, campaign_id, title, scheduled_at_utc, summary, participant_character_ids, encounter_ids, story_document_ids, live_mode_active, tags, created_at, updated_at) VALUES "
             "('session_one_nexus', " + campaignRow + ", 'Session 1: Neon Signal and Pressure', datetime('now'), 'Skylane pursuit, first contact with Bloodline Coil, debt marker introduction.', 'pc_kyro;pc_mara;pc_raen;pc_ixa;pc_tan', 'enc_skylane_probe', 'doc_session1_prep', 0, 'session;1;', datetime('now'), datetime('now')),"
             "('session_two_choir', " + campaignRow + ", 'Session 2: Choir Under Glass', datetime('now', '+1 day'), 'Ritual clues in the Choir chamber and a hard truth from the Oracle.', 'pc_kyro;pc_mara;pc_raen;pc_ixa;pc_tan', 'enc_choir_undertow', 'doc_session2_prep', 0, 'session;2;', datetime('now'), datetime('now')),"
             "('session_three_core', " + campaignRow + ", 'Session 3: Liftline Reckoning', datetime('now', '+2 days'), 'Final core confrontation at the lift core with debt payoff options.', 'pc_kyro;pc_mara;pc_raen;pc_ixa;pc_tan', 'enc_lift_core', 'doc_session3_prep', 0, 'session;3;', datetime('now'), datetime('now'));"
        );

        exec(db,
             "INSERT OR REPLACE INTO house_rules (stable_id, campaign_id, label, description, enabled, change_history, tags, created_at, updated_at) VALUES "
             "('house_rule_static_mark', " + campaignRow + ", 'Static Debt Mark', 'Any failed social or investigation roll in public district play adds one Debt Marker unless someone spends 1 MN to smooth reputation first.', 1, 'Added to make debt visible at scene transitions.', 'house-rule;social;', datetime('now'), datetime('now')),"
             "('house_rule_core_resonance', " + campaignRow + ", 'Core Resonance Escalation', 'At the Lift Core, if a creature fails a resistance vs Resonance magic twice, one additional environmental hazard is activated. Hazards are removed by successful ritual payment (1 HZ).', 1, 'Core challenge tuning for campaign finale.', 'house-rule;combat;ritual;', datetime('now'), datetime('now'));"
        );

        exec(db, "COMMIT;");
        sqlite3_close(db);
    }
    catch (...)
    {
        exec(db, "ROLLBACK;");
        sqlite3_close(db);
        throw;
    }
}
}

int main(int argc, char **argv)
{
    try
    {
        if (argc < 2)
            throw std::runtime_error("Usage: seed_cyberpunk_campaign <db> [db...]");
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
