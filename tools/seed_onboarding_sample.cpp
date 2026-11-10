#include "sqlite3.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr const char *kCampaignId = "campaign_onboarding_lantern_fair";
constexpr const char *kCampaignName = "Onboarding Sample - The Lantern Fair";

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

std::string inventory(std::initializer_list<std::string> entries)
{
    std::string result;
    for (const auto &entry : entries)
        result += entry + ";";
    return result;
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
        result += entry + entrySeparator;
    return result;
}

std::string spellbook(std::initializer_list<std::string> spellIds)
{
    std::vector<std::string> entries;
    for (const auto &spellId : spellIds)
        entries.push_back(packed({spellId, "1", "Onboarding Sample", "Prepared for tonight's fair scenes."}));
    std::string result;
    constexpr char entrySeparator = '\x1e';
    for (const auto &entry : entries)
        result += entry + entrySeparator;
    return result;
}

std::string action(const std::string &name, int reach, int range, const std::string &attack, const std::string &damage, const std::string &notes = "")
{
    return packed({name, std::to_string(reach), std::to_string(range), attack, damage, notes});
}

std::string mapCells(int width, int height, int variant)
{
    std::ostringstream out;
    bool first = true;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int terrain = variant == 0 ? 4 : 2;
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
                terrain = variant == 0 ? 6 : 3;
            else if (variant == 0 && (x == 6 || y == 4))
                terrain = 5;
            else if (variant == 1 && (x == 3 || x == 8 || y == 5))
                terrain = 15;
            else if ((x + y + variant) % 11 == 0)
                terrain = 14;

            if (!first)
                out << ",";
            first = false;
            out << terrain << ":0";
        }
    }
    return out.str();
}

std::filesystem::path findProjectRoot()
{
    std::filesystem::path current = std::filesystem::current_path();
    while (!current.empty())
    {
        if (std::filesystem::exists(current / "Resources"))
            return current;
        if (current == current.parent_path())
            break;
        current = current.parent_path();
    }
    return std::filesystem::current_path();
}

std::string projectPath(const std::filesystem::path &path)
{
    try
    {
        return std::filesystem::relative(path, findProjectRoot()).generic_string();
    }
    catch (const std::exception &)
    {
        return path.generic_string();
    }
}

void writeU16(std::ofstream &out, std::uint16_t value)
{
    out.put(static_cast<char>(value & 0xff));
    out.put(static_cast<char>((value >> 8) & 0xff));
}

void writeU32(std::ofstream &out, std::uint32_t value)
{
    writeU16(out, static_cast<std::uint16_t>(value & 0xffff));
    writeU16(out, static_cast<std::uint16_t>((value >> 16) & 0xffff));
}

void writeBmp(const std::filesystem::path &path, int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, int motif)
{
    std::filesystem::create_directories(path.parent_path());
    const int rowStride = ((width * 3 + 3) / 4) * 4;
    const std::uint32_t pixelBytes = static_cast<std::uint32_t>(rowStride * height);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("Could not write BMP: " + path.string());

    out.put('B');
    out.put('M');
    writeU32(out, 54 + pixelBytes);
    writeU16(out, 0);
    writeU16(out, 0);
    writeU32(out, 54);
    writeU32(out, 40);
    writeU32(out, static_cast<std::uint32_t>(width));
    writeU32(out, static_cast<std::uint32_t>(height));
    writeU16(out, 1);
    writeU16(out, 24);
    writeU32(out, 0);
    writeU32(out, pixelBytes);
    writeU32(out, 2835);
    writeU32(out, 2835);
    writeU32(out, 0);
    writeU32(out, 0);

    std::vector<unsigned char> row(static_cast<std::size_t>(rowStride), 0);
    for (int y = height - 1; y >= 0; --y)
    {
        for (int x = 0; x < width; ++x)
        {
            const bool frame = x < 8 || y < 8 || x >= width - 8 || y >= height - 8;
            const bool diagonal = ((x + y + motif * 17) % 37) < 4;
            const bool center = std::abs(x - width / 2) + std::abs(y - height / 2) < width / 5;
            std::uint8_t rr = static_cast<std::uint8_t>(std::min(255, r + (frame ? 45 : 0) + (center ? 35 : 0)));
            std::uint8_t gg = static_cast<std::uint8_t>(std::min(255, g + (diagonal ? 40 : 0) + (center ? 25 : 0)));
            std::uint8_t bb = static_cast<std::uint8_t>(std::min(255, b + (frame ? 25 : 0) + (diagonal ? 35 : 0)));
            row[static_cast<std::size_t>(x * 3 + 0)] = bb;
            row[static_cast<std::size_t>(x * 3 + 1)] = gg;
            row[static_cast<std::size_t>(x * 3 + 2)] = rr;
        }
        out.write(reinterpret_cast<const char *>(row.data()), row.size());
    }
}

void writeWav(const std::filesystem::path &path, double baseFrequency, double seconds)
{
    std::filesystem::create_directories(path.parent_path());
    constexpr int sampleRate = 22050;
    const int sampleCount = static_cast<int>(sampleRate * seconds);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("Could not write WAV: " + path.string());

    out.write("RIFF", 4);
    writeU32(out, 36 + static_cast<std::uint32_t>(sampleCount * 2));
    out.write("WAVEfmt ", 8);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, 1);
    writeU32(out, sampleRate);
    writeU32(out, sampleRate * 2);
    writeU16(out, 2);
    writeU16(out, 16);
    out.write("data", 4);
    writeU32(out, static_cast<std::uint32_t>(sampleCount * 2));

    for (int i = 0; i < sampleCount; ++i)
    {
        const double t = static_cast<double>(i) / sampleRate;
        const double envelope = std::min(1.0, std::min(t * 3.0, (seconds - t) * 2.5));
        const double wave = 0.45 * std::sin(6.283185307179586 * baseFrequency * t) +
                            0.20 * std::sin(6.283185307179586 * (baseFrequency * 1.5) * t) +
                            0.08 * std::sin(6.283185307179586 * (baseFrequency * 0.5) * t);
        const auto sample = static_cast<std::int16_t>(std::clamp(wave * envelope, -1.0, 1.0) * 26000);
        writeU16(out, static_cast<std::uint16_t>(sample));
    }
}

void writeTextFile(const std::filesystem::path &path, const std::string &text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    if (!out)
        throw std::runtime_error("Could not write text asset: " + path.string());
    out << text;
}

struct SampleAssets
{
    std::filesystem::path pipPortrait;
    std::filesystem::path maraPortrait;
    std::filesystem::path torenPortrait;
    std::filesystem::path niaPortrait;
    std::filesystem::path ionaPortrait;
    std::filesystem::path mothPortrait;
    std::filesystem::path fairMap;
    std::filesystem::path hedgeMap;
    std::filesystem::path fairAmbience;
    std::filesystem::path ceremonyCue;
    std::filesystem::path mothFlutter;
    std::filesystem::path handoutNotice;
};

SampleAssets createSampleAssets()
{
    const std::filesystem::path root = findProjectRoot();
    const std::filesystem::path campaignRoot = root / "Resources" / "Campaigns" / kCampaignId;
    SampleAssets assets;
    assets.pipPortrait = campaignRoot / "Assets" / "Portraits" / "char_pip_arden.bmp";
    assets.maraPortrait = campaignRoot / "Assets" / "Portraits" / "char_mara_velle.bmp";
    assets.torenPortrait = campaignRoot / "Assets" / "Portraits" / "char_toren_mill.bmp";
    assets.niaPortrait = campaignRoot / "Assets" / "Portraits" / "char_nia_roseglass.bmp";
    assets.ionaPortrait = campaignRoot / "Assets" / "Portraits" / "npc_iona_bellweather.bmp";
    assets.mothPortrait = campaignRoot / "Assets" / "Portraits" / "npc_lantern_moth.bmp";
    assets.fairMap = campaignRoot / "Assets" / "Maps" / "bellweather_green_map.bmp";
    assets.hedgeMap = campaignRoot / "Assets" / "Maps" / "moonlit_hedge_path.bmp";
    assets.fairAmbience = campaignRoot / "Assets" / "Audio" / "fair_day_ambience.wav";
    assets.ceremonyCue = campaignRoot / "Assets" / "Audio" / "lantern_ceremony_cue.wav";
    assets.mothFlutter = campaignRoot / "Assets" / "Audio" / "moth_flutter.wav";
    assets.handoutNotice = campaignRoot / "Assets" / "Handouts" / "bellweather_fair_notice.txt";

    writeBmp(assets.pipPortrait, 192, 192, 86, 132, 188, 1);
    writeBmp(assets.maraPortrait, 192, 192, 166, 86, 180, 2);
    writeBmp(assets.torenPortrait, 192, 192, 178, 118, 72, 3);
    writeBmp(assets.niaPortrait, 192, 192, 92, 158, 112, 4);
    writeBmp(assets.ionaPortrait, 192, 192, 198, 156, 80, 5);
    writeBmp(assets.mothPortrait, 192, 192, 88, 72, 136, 6);
    writeBmp(assets.fairMap, 640, 420, 74, 132, 82, 7);
    writeBmp(assets.hedgeMap, 640, 420, 42, 96, 72, 8);
    writeWav(assets.fairAmbience, 196.0, 5.0);
    writeWav(assets.ceremonyCue, 392.0, 3.0);
    writeWav(assets.mothFlutter, 620.0, 1.75);
    writeTextFile(assets.handoutNotice,
                  "Bellweather Green Lantern Fair\n"
                  "\n"
                  "Tonight at moonrise, every household lights one painted lantern.\n"
                  "Visitors are invited to help with ribbon stalls, candle safety, and the final bell.\n"
                  "\n"
                  "Rumor for the table: someone has been taking the candle-flames without touching the wicks.\n");
    return assets;
}

void pruneKnownOutdatedResources()
{
    const std::filesystem::path campaigns = findProjectRoot() / "Resources" / "Campaigns";
    for (const std::string &folder : {"campaign_current_feature_tour", "campaign_4fcc6092e498_5", "campaign_neon_resonance_3_sessions", "test_campaign_package_import"})
    {
        const std::filesystem::path target = campaigns / folder;
        if (std::filesystem::exists(target))
            std::filesystem::remove_all(target);
    }
}

void ensureSchema(sqlite3 *db)
{
    exec(db,
         "CREATE TABLE IF NOT EXISTS audio_ambience_layers ("
         "id INTEGER PRIMARY KEY,"
         "stable_id TEXT UNIQUE,"
         "campaign_id INTEGER NOT NULL,"
         "name TEXT NOT NULL,"
         "audio_asset_id TEXT,"
         "volume REAL DEFAULT 0.35,"
         "loop INTEGER DEFAULT 1,"
         "active INTEGER DEFAULT 0,"
         "sort_order INTEGER DEFAULT 0,"
         "tags TEXT,"
         "created_at TEXT,"
         "updated_at TEXT,"
         "FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE"
         ");");
    exec(db,
         "CREATE TABLE IF NOT EXISTS story_documents ("
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
    exec(db,
         "CREATE TABLE IF NOT EXISTS timeline_events ("
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
    exec(db,
         "CREATE TABLE IF NOT EXISTS sessions ("
         "id INTEGER PRIMARY KEY,"
         "stable_id TEXT UNIQUE,"
         "campaign_id INTEGER NOT NULL,"
         "title TEXT NOT NULL,"
         "scheduled_at_utc TEXT,"
         "started_at_utc TEXT,"
         "ended_at_utc TEXT,"
         "summary TEXT,"
         "recap TEXT,"
         "unresolved_threads TEXT,"
         "loot_awarded TEXT,"
         "xp_milestones TEXT,"
         "npcs_met TEXT,"
         "locations_visited TEXT,"
         "open_hooks TEXT,"
         "participant_character_ids TEXT,"
         "encounter_ids TEXT,"
         "story_document_ids TEXT,"
         "live_mode_active INTEGER DEFAULT 0,"
         "tags TEXT,"
         "created_at TEXT,"
         "updated_at TEXT,"
         "FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE"
         ");");
    exec(db,
         "CREATE TABLE IF NOT EXISTS session_notes ("
         "id INTEGER PRIMARY KEY,"
         "stable_id TEXT UNIQUE,"
         "session_id INTEGER NOT NULL,"
         "timestamp_utc TEXT,"
         "body TEXT NOT NULL,"
         "linked_entity_id TEXT,"
         "linked_entity_type TEXT,"
         "linked_entity_label TEXT,"
         "tags TEXT,"
         "created_at TEXT,"
         "updated_at TEXT,"
         "FOREIGN KEY(session_id) REFERENCES sessions(id) ON DELETE CASCADE"
         ");");
    exec(db,
         "CREATE TABLE IF NOT EXISTS handouts ("
         "id INTEGER PRIMARY KEY,"
         "stable_id TEXT UNIQUE,"
         "campaign_id INTEGER NOT NULL,"
         "title TEXT NOT NULL,"
         "body TEXT,"
         "file_path TEXT,"
         "gm_notes TEXT,"
         "tags TEXT,"
         "created_at TEXT,"
         "updated_at TEXT,"
         "FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE"
         ");");
    addColumnIfMissing(db, "campaigns", "party_map_id", "TEXT");
    addColumnIfMissing(db, "campaigns", "party_x", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "campaigns", "party_y", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "campaigns", "party_reputation", "TEXT");
    addColumnIfMissing(db, "campaigns", "last_modified", "DATETIME DEFAULT CURRENT_TIMESTAMP");
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
    addColumnIfMissing(db, "npcs", "category", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "npcs", "inventory_entries", "TEXT");
    addColumnIfMissing(db, "maps", "background_image_path", "TEXT");
    addColumnIfMissing(db, "maps", "background_image_opacity", "REAL DEFAULT 0.65");
    addColumnIfMissing(db, "maps", "regions", "TEXT");
    addColumnIfMissing(db, "maps", "features", "TEXT");
    addColumnIfMissing(db, "maps", "routes", "TEXT");
    addColumnIfMissing(db, "story_beats", "cue_audio_asset_id", "TEXT");
    addColumnIfMissing(db, "story_beats", "cue_volume", "REAL DEFAULT 1.0");
    addColumnIfMissing(db, "story_beats", "cue_loop", "INTEGER DEFAULT 1");
    addColumnIfMissing(db, "audio_assets", "original_file_name", "TEXT");
    addColumnIfMissing(db, "audio_assets", "original_source_path", "TEXT");
    addColumnIfMissing(db, "audio_assets", "duration_seconds", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "audio_assets", "missing_file", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "audio_assets", "last_played_at_utc", "TEXT");
    addColumnIfMissing(db, "audio_playlists", "repeat", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "audio_playlists", "one_shot", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "audio_playlists", "crossfade_seconds", "REAL DEFAULT 0");
    addColumnIfMissing(db, "audio_ambience_layers", "active", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "audio_ambience_layers", "sort_order", "INTEGER DEFAULT 0");
    addColumnIfMissing(db, "sessions", "started_at_utc", "TEXT");
    addColumnIfMissing(db, "sessions", "ended_at_utc", "TEXT");
    addColumnIfMissing(db, "sessions", "recap", "TEXT");
    addColumnIfMissing(db, "sessions", "unresolved_threads", "TEXT");
    addColumnIfMissing(db, "sessions", "loot_awarded", "TEXT");
    addColumnIfMissing(db, "sessions", "xp_milestones", "TEXT");
    addColumnIfMissing(db, "sessions", "npcs_met", "TEXT");
    addColumnIfMissing(db, "sessions", "locations_visited", "TEXT");
    addColumnIfMissing(db, "sessions", "open_hooks", "TEXT");
}

void deleteOutdatedCampaignRows(sqlite3 *db)
{
    exec(db,
         "DELETE FROM campaigns WHERE "
         "stable_id IN ('campaign_current_feature_tour','campaign_neon_resonance_3_sessions','campaign_4fcc6092e498_5','campaign_2de4a960f70_20') "
         "OR stable_id LIKE 'test_campaign_%' "
         "OR name IN ("
         "'Codex Feature Tour - Full System Check',"
         "'Codex Feature Tour - Current Systems',"
         "'Neon Resonance: Three Signal Sessions',"
         "'Foundation Test Campaign roundtrip',"
         "'Foundation Test Campaign save_workflow',"
         "'Foundation Test Campaign package_export',"
         "'Foundation Test Campaign package_import',"
         "'gdfg',"
         "'32',"
         "'32423'"
         ");");
    exec(db, "DELETE FROM campaigns WHERE stable_id=" + quote(kCampaignId) + " OR name=" + quote(kCampaignName) + ";");
}

void insertAssetMetadata(sqlite3 *db, const std::string &campaignRow, const std::string &id, const std::string &kind, const std::string &name, const std::filesystem::path &path, const std::string &mime, const std::string &tags)
{
    const auto size = std::filesystem::exists(path) ? static_cast<long long>(std::filesystem::file_size(path)) : 0LL;
    exec(db,
         "INSERT INTO asset_metadata (stable_id, campaign_id, asset_type, display_name, source_path, project_path, content_hash, mime_type, byte_size, missing, tags, created_at, updated_at) VALUES (" +
             quote(id) + "," + campaignRow + "," + quote(kind) + "," + quote(name) + ",''," + quote(projectPath(path)) + "," +
             quote("onboarding-" + id) + "," + quote(mime) + "," + std::to_string(size) + ",0," + quote(tags) + ",datetime('now'),datetime('now'));");
}

void seedDatabase(const std::filesystem::path &path)
{
    sqlite3 *db = nullptr;
    if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK)
        throw std::runtime_error("Could not open database: " + path.string());

    const SampleAssets assets = createSampleAssets();

    try
    {
        exec(db, "PRAGMA foreign_keys = ON;");
        ensureSchema(db);
        exec(db, "BEGIN TRANSACTION;");
        deleteOutdatedCampaignRows(db);

        exec(db,
             "INSERT INTO campaigns (stable_id, name, ruleset_name, description, tags, created_at, updated_at, party_map_id, party_x, party_y, party_reputation, last_modified) VALUES (" +
                 quote(kCampaignId) + "," + quote(kCampaignName) + "," + quote("Dungeons & Dragons 2024 (5.5e)") + "," +
                 quote("A ready-to-run level 2 starter adventure. The party protects Bellweather Green's Lantern Fair, discovers why candle-flames are vanishing, negotiates with a tiny fey thief, and follows the trail into the moonlit hedge before the ceremony bell rings.") + "," +
                 quote("onboarding;sample;playable;original-content;") + ",datetime('now'),datetime('now'),'map_lantern_fair',6,4," +
                 quote("Trusted fair wardens if they save the moonrise ceremony.") + ",datetime('now'));");
        const std::string campaignRow = std::to_string(sqlite3_last_insert_rowid(db));

        exec(db,
             "INSERT INTO custom_items (stable_id, campaign_id, name, description, ruleset_key, category, item_type, rarity, weight, base_value, stackable, armor_class_bonus, damage_reduction, weapon_attack_bonus, weapon_damage, weapon_damage_type, weapon_ability, weapon_reach_feet, weapon_range_feet, effects, tags, created_at, updated_at) VALUES "
             "('item_fair_badge'," + campaignRow + ",'Fair Warden Badge','Marks a volunteer helper. NPCs at the fair start friendly unless frightened.','dnd2024','Story Item','Badge',0,0.1,0,0,0,0,0,'','','Charisma',5,5,'Use as a social permission token.','tutorial;social;',datetime('now'),datetime('now')),"
             "('item_glimmer_tonic'," + campaignRow + ",'Glimmer Tonic','A sparkling restorative for teaching inventory quantities and healing notes.','dnd2024','Consumable','Potion',0,0.5,5000,1,0,0,0,'','','Strength',5,5,'Restore 2d4+2 HP manually.','tutorial;consumable;',datetime('now'),datetime('now')),"
             "('item_lantern_hook'," + campaignRow + ",'Silver Lantern Hook','A hooked pole for lowering lanterns, snagging ribbons, and reaching hedge charms.','dnd2024','Tool','Pole',1,2.0,1500,0,0,0,0,'1d4','bludgeoning','Dexterity',10,10,'Can reach switches or hanging props.','tutorial;tool;',datetime('now'),datetime('now')),"
             "('item_moonseed_honey'," + campaignRow + ",'Moonseed Honey','The moth will trade secrets for this sweet, silver-flecked honey.','dnd2024','Trade Good','Jar',0,0.3,250,1,0,0,0,'','','Wisdom',5,5,'Useful in the nonviolent solution.','clue;fey;',datetime('now'),datetime('now')),"
             "('item_bell_clapper_key'," + campaignRow + ",'Bell Clapper Key','A small iron clapper that unlocks the hedge gate when rung three times.','dnd2024','Story Item','Key',2,0.2,0,0,0,0,0,'','','Charisma',5,5,'Opens the moonlit hedge gate.','plot;key;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO custom_spells (stable_id, campaign_id, name, spell_level, school, resolution, casting_time, spell_range, duration, concentration, ritual, verbal, somatic, material, material_description, classes, attack_bonus, saving_throw_ability, damage, healing, area, effect_summary, scaling, description, tags, created_at, updated_at) VALUES "
             "('spell_lantern_spark'," + campaignRow + ",'Lantern Spark',0,4,1,'1 Action','60 ft','Instantaneous',0,0,1,1,0,'','Wizard, Bard','','','1d6 fire','','One small object or creature','Relights or startles a candle-flame.','','A safe cantrip for testing spell attacks and scene utility.','tutorial;cantrip;',datetime('now'),datetime('now')),"
             "('spell_kind_word'," + campaignRow + ",'Kind Word',1,0,3,'1 Action','30 ft','Instantaneous',0,0,1,0,0,'','Bard, Cleric','','','','1d8 + ability','One ally','Steadies an ally or frightened NPC.','Healing increases by 1d8 per higher slot.','Use after the moth frightens the crowd.','tutorial;healing;',datetime('now'),datetime('now')),"
             "('spell_hearthward'," + campaignRow + ",'Hearthward',1,0,2,'1 Action','Self','10 minutes',1,1,1,1,1,'a twist of ribbon','Cleric, Druid','','Wisdom','','','10 ft aura','Keeps small fires calm and controlled.','','A ritual clue spell for protecting the final ceremony.','ritual;utility;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO characters (stable_id, campaign_id, name, level, class, race, attributes, tags, created_at, updated_at, backstory, current_hp, max_hp, portrait_asset_id, inventory_entries, spellcasting_enabled, spellcasting_ability, spell_attack_bonus, spell_save_dc, spell_slots_max, spell_slots_used, spellbook_entries) VALUES " +
                 std::string("(") + quote("char_pip_arden") + "," + campaignRow + "," + quote("Pip Arden") + ",2,'Rogue','Halfling'," + quote(attrs(8, 16, 12, 13, 12, 14)) + ",'scout;face;',datetime('now'),datetime('now')," + quote("Pip knows fair games, shortcuts, and how to talk a suspicious child into telling the truth.") + ",15,17," + quote(projectPath(assets.pipPortrait)) + "," + quote(inventory({"item_fair_badge:1:1:Jacket", "item_glimmer_tonic:2:0:Pouch"})) + ",0,'Dexterity',0,10," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ",'')" + ","
                 "('char_mara_velle'," + campaignRow + ",'Mara Velle',2,'Bard','Human'," + quote(attrs(10, 14, 12, 12, 11, 16)) + ",'caster;support;',datetime('now'),datetime('now')," + quote("Mara is playing the dusk stage and can turn a tense scene back toward kindness.") + ",14,14," + quote(projectPath(assets.maraPortrait)) + "," + quote(inventory({"item_lantern_hook:1:0:Pack"})) + ",1,'Charisma',5,13," + quote(slots({0,3,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(spellbook({"spell_lantern_spark", "spell_kind_word"})) + "),"
                 "('char_toren_mill'," + campaignRow + ",'Toren Mill',2,'Fighter','Dwarf'," + quote(attrs(16, 11, 15, 9, 12, 10)) + ",'defender;local;',datetime('now'),datetime('now')," + quote("Toren's family built the fair bell. He cares more about keeping people safe than winning fights.") + ",22,22," + quote(projectPath(assets.torenPortrait)) + "," + quote(inventory({"item_fair_badge:1:1:Coat", "item_bell_clapper_key:1:0:Tool Roll"})) + ",0,'Strength',0,10," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ",'')" + ","
                 "('char_nia_roseglass'," + campaignRow + ",'Nia Roseglass',2,'Cleric','Elf'," + quote(attrs(9, 12, 13, 11, 16, 12)) + ",'healer;ritual;',datetime('now'),datetime('now')," + quote("Nia studies old hearth blessings and can recognize when the hedge magic is more lonely than cruel.") + ",16,16," + quote(projectPath(assets.niaPortrait)) + "," + quote(inventory({"item_moonseed_honey:1:0:Satchel", "item_glimmer_tonic:1:0:Belt"})) + ",1,'Wisdom',5,13," + quote(slots({0,3,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(spellbook({"spell_kind_word", "spell_hearthward"})) + ");");

        exec(db,
             "INSERT INTO npcs (stable_id, campaign_id, category, name, description, faction, role, attitude_toward_party, secrets, goals, resources, threat_location, relationships, threat_level, dialogue_notes, attributes, conditions, tags, created_at, updated_at, inventory_entries, spellcasting_enabled, spellcasting_ability, spell_attack_bonus, spell_save_dc, spell_slots_max, spell_slots_used, spellbook_entries) VALUES "
             "('npc_iona_bellweather'," + campaignRow + ",0,'Iona Bellweather','Fair organizer trying to keep the ceremony joyful.','Bellweather Green','Quest giver','Trusting','She promised the hedge court a lantern tribute years ago and forgot.','Keep everyone calm until moonrise.','Fair badges, stall keys, village trust.','Main stage','Knows Toren and trusts Nia.',1,'Fast, warm, and practical.'," + quote(attrs(10,11,12,14,15,13)) + ",'','ally;portrait;',datetime('now'),datetime('now')," + quote(inventory({"item_fair_badge:4:0:Satchel"})) + ",0,'Wisdom',0,10," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ",'')" + ","
             "('npc_bram_candlewick'," + campaignRow + ",0,'Bram Candlewick','Beekeeper and candle-maker whose stall is missing its best honey.','Bellweather Green','Clue witness','Worried','He saw the moth but is embarrassed he screamed.','Recover the moonseed honey.','Wax, honey, fair gossip.','Candle stall','Owes Pip a favor from a card game.',0,'Mutters candle measurements when nervous.'," + quote(attrs(11,10,13,12,14,9)) + ",'','clue;merchant;',datetime('now'),datetime('now')," + quote(inventory({"item_moonseed_honey:2:0:Crate"})) + ",0,'Wisdom',0,10," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ",'')" + ","
             "('npc_lantern_moth'," + campaignRow + ",1,'Lantern Moth Envoy','Tiny fey with ember-bright wings, stealing flames as overdue tribute.','Hedge Court','Negotiable troublemaker','Curious','It wants an apology and honey more than a fight.','Bring back a tribute before moonrise.','Stolen candle-flames, secret path.','Hedge gate','Will follow Mara if treated kindly.',2,'Rhymes badly, lies poorly, loves honey.'," + quote(attrs(4,17,10,9,13,15)) + ",'Hidden;','fey;portrait;encounter;',datetime('now'),datetime('now'),'',0,'Charisma',0,10," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ",'')" + ","
             "('npc_brambleshade'," + campaignRow + ",1,'Brambleshade Snare','A hedge guardian that tests whether the party uses light, kindness, or blades.','Hedge Court','Obstacle','Territorial','It calms if the bell clapper is rung.','Keep trespassers away from the old lantern.','Thorns and grasping roots.','Moonlit hedge','Bound to the forgotten tribute.',3,'Speaks as leaves scratching glass.'," + quote(attrs(14,10,14,5,12,6)) + ",'Guarding;','guardian;encounter;',datetime('now'),datetime('now'),'',0,'Wisdom',0,10," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + "," + quote(slots({0,0,0,0,0,0,0,0,0,0})) + ",'');");

        exec(db,
             "INSERT INTO maps (stable_id, campaign_id, name, description, scope, grid_type, width, height, cell_size, is_main_map, parent_map_id, background_image_path, background_image_opacity, cells, tags, created_at, updated_at) VALUES "
             "('map_lantern_fair'," + campaignRow + ",'Bellweather Green','Fairground, stalls, stage, candle booth, hedge gate, and bell tower.',1,0,12,8,5,1,''," + quote(projectPath(assets.fairMap)) + ",0.72," + quote(mapCells(12,8,0)) + ",'fair;village;playable;',datetime('now'),datetime('now')),"
             "('map_moonlit_hedge'," + campaignRow + ",'Moonlit Hedge Path','A short second scene for tracking the moth trail and resolving the guardian.',2,0,12,9,5,0,'map_lantern_fair'," + quote(projectPath(assets.hedgeMap)) + ",0.72," + quote(mapCells(12,9,1)) + ",'hedge;fey;playable;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO map_layers (stable_id, map_id, name, visible, locked, sort_order, tags, created_at, updated_at) "
             "SELECT 'layer_fair_party', id, 'Party and NPCs', 1, 0, 10, 'tokens;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_lantern_fair' UNION ALL "
             "SELECT 'layer_fair_clues', id, 'Clues', 1, 0, 20, 'clues;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_lantern_fair' UNION ALL "
             "SELECT 'layer_hedge_tokens', id, 'Hedge Encounter', 1, 0, 10, 'tokens;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_moonlit_hedge';");
        exec(db,
             "INSERT INTO map_tokens (stable_id, map_id, name, kind, linked_entity_type, linked_entity_id, asset_id, layer_id, x, y, rotation, size, hidden_from_players, tags, created_at, updated_at) "
             "SELECT 'token_party_fair', id, 'Party Start', 6, 'Party', '" + std::string(kCampaignId) + "', '', 'layer_fair_party', 6, 4, 0, 1, 0, 'party;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_lantern_fair' UNION ALL "
             "SELECT 'token_iona_stage', id, 'Iona Bellweather', 1, 'NPC', 'npc_iona_bellweather', '', 'layer_fair_party', 5, 2, 0, 1, 0, 'npc;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_lantern_fair' UNION ALL "
             "SELECT 'token_moth_hidden', id, 'Hidden Moth', 2, 'NPC', 'npc_lantern_moth', '', 'layer_fair_clues', 9, 3, 0, 1, 1, 'hidden;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_lantern_fair' UNION ALL "
             "SELECT 'token_snare_hedge', id, 'Brambleshade Snare', 2, 'NPC', 'npc_brambleshade', '', 'layer_hedge_tokens', 8, 5, 0, 2, 0, 'guardian;', datetime('now'), datetime('now') FROM maps WHERE stable_id='map_moonlit_hedge';");

        exec(db,
             "INSERT INTO audio_assets (stable_id, campaign_id, name, original_file_name, original_source_path, file_path, asset_type, default_volume, loop, duration_seconds, missing_file, tags, created_at, updated_at) VALUES "
             "('audio_fair_day'," + campaignRow + ",'Fair Day Ambience','fair_day_ambience.wav',''," + quote(projectPath(assets.fairAmbience)) + ",1,0.35,1,5,0,'ambience;fair;',datetime('now'),datetime('now')),"
             "('audio_lantern_ceremony'," + campaignRow + ",'Lantern Ceremony Cue','lantern_ceremony_cue.wav',''," + quote(projectPath(assets.ceremonyCue)) + ",0,0.65,0,3,0,'music;ceremony;',datetime('now'),datetime('now')),"
             "('audio_moth_flutter'," + campaignRow + ",'Moth Flutter','moth_flutter.wav',''," + quote(projectPath(assets.mothFlutter)) + ",2,0.70,0,2,0,'effect;fey;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO audio_playlists (stable_id, campaign_id, name, audio_asset_ids, shuffle, repeat, one_shot, crossfade_seconds, tags, created_at, updated_at) VALUES "
             "('playlist_lantern_session'," + campaignRow + ",'Lantern Fair Session','audio_fair_day;audio_lantern_ceremony;audio_moth_flutter;',0,0,0,0.5,'session;starter;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO audio_ambience_layers (stable_id, campaign_id, name, audio_asset_id, volume, loop, active, sort_order, tags, created_at, updated_at) VALUES "
             "('ambience_layer_fair'," + campaignRow + ",'Fair Crowd Bed','audio_fair_day',0.35,1,0,0,'fair;',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO shops (stable_id, campaign_id, name, merchant_name, description, faction_reputation_notes, inventory_id, tags, created_at, updated_at) VALUES "
             "('shop_tallow_thread'," + campaignRow + ",'Tallow and Thread','Bram Candlewick','Candles, ribbons, honey, and practical fair gear.','Helping Bram lowers prices and reveals the moth clue.','','shop;fair;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO shop_stock (shop_id, item_id, quantity, price_multiplier, sell_price_multiplier, restocks, availability, haggle_notes, faction_modifier, reputation_modifier) "
             "SELECT id, 'item_glimmer_tonic', 5, 0.8, 0.5, 1, 'Common', 'Bram offers one free after the clue is found.', 1.0, 1.2 FROM shops WHERE stable_id='shop_tallow_thread' UNION ALL "
             "SELECT id, 'item_moonseed_honey', 3, 1.0, 0.5, 0, 'Limited', 'The moth wants this more than coins.', 1.0, 1.4 FROM shops WHERE stable_id='shop_tallow_thread';");
        exec(db,
             "INSERT INTO loot_parcels (stable_id, campaign_id, name, source, notes, awarded, awarded_to, copper, silver, gold, platinum, tags, created_at, updated_at) VALUES "
             "('parcel_saved_ceremony'," + campaignRow + ",'Saved Ceremony Thanks','Iona Bellweather','Award after the final lantern is lit.',0,'',20,30,12,0,'reward;fair;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO loot_parcel_items (loot_parcel_id, item_id, quantity, notes) "
             "SELECT id, 'item_glimmer_tonic', 2, 'Iona keeps these for exhausted volunteers.' FROM loot_parcels WHERE stable_id='parcel_saved_ceremony' UNION ALL "
             "SELECT id, 'item_fair_badge', 4, 'Kept as a village favor token.' FROM loot_parcels WHERE stable_id='parcel_saved_ceremony';");

        exec(db,
             "INSERT INTO encounters (stable_id, campaign_id, name, description, map_id, map_context_name, map_context_x, map_context_y, battlefield_width, battlefield_height, cell_size_feet, use_map_cells, theater_of_mind, combat_paused, battlefield_cells, round, active_participant_index, tags, created_at, updated_at) VALUES "
             "('enc_moth_mischief'," + campaignRow + ",'Scene 2: Moth Mischief','The moth steals a candle-flame. Play as chase, negotiation, or light nonlethal combat.','map_lantern_fair','Bellweather Green',9,3,12,8,5,1,0,0," + quote(mapCells(12,8,0)) + ",1,0,'starter;social-or-combat;',datetime('now'),datetime('now')),"
             "('enc_hedge_gate'," + campaignRow + ",'Scene 4: The Hedge Gate','The party reaches the old lantern and the bramble guardian. Ring the clapper, offer honey, or fight through.','map_moonlit_hedge','Moonlit Hedge Path',8,5,12,9,5,1,0,0," + quote(mapCells(12,9,1)) + ",1,0,'starter;guardian;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO encounter_participants (stable_id, encounter_id, kind, source_entity_id, monster_definition_id, display_name, initiative, current_hp, max_hp, armor_class, armor_damage_reduction, speed_feet, group_name, wave_number, starts_hidden, reinforcement_round, starting_x, starting_y, grid_x, grid_y, turn_start_x, turn_start_y, movement_used_feet, defeated, friendly, conditions, actions, tags, created_at, updated_at) "
             "SELECT 'part_moth_pip', id, 0, 'char_pip_arden', '', 'Pip Arden', 15, 15, 17, 14, 0, 25, 'Party', 1, 0, 0, 5, 4, 5, 4, 5, 4, 0, 0, 1, '', " + quote(packedEntries({action("Practice Strike", 5, 5, "+5", "1d6+3 piercing", "Use only if the scene becomes a fight.")})) + ", 'pc;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_moth_mischief' UNION ALL "
             "SELECT 'part_moth_mara', id, 0, 'char_mara_velle', '', 'Mara Velle', 12, 14, 14, 13, 0, 30, 'Party', 1, 0, 0, 6, 4, 6, 4, 6, 4, 0, 0, 1, '', " + quote(packedEntries({action("Lantern Spark", 5, 60, "+5", "1d6 fire"), action("Kind Word", 5, 30, "+0", "1d8 + ability")})) + ", 'pc;caster;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_moth_mischief' UNION ALL "
             "SELECT 'part_moth_envoy', id, 1, 'npc_lantern_moth', '', 'Lantern Moth Envoy', 16, 9, 9, 13, 0, 10, 'Mischief', 1, 1, 0, 9, 3, 9, 3, 9, 3, 0, 0, 0, 'Hidden;', " + quote(packedEntries({action("Snuff Wick", 5, 15, "+4", "0", "A lantern goes dark until relit."), action("Flutter Away", 5, 30, "+0", "0", "Moves without provoking.")})) + ", 'npc;nonlethal;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_moth_mischief' UNION ALL "
             "SELECT 'part_hedge_toren', id, 0, 'char_toren_mill', '', 'Toren Mill', 11, 22, 22, 16, 0, 25, 'Party', 1, 0, 0, 4, 5, 4, 5, 4, 5, 0, 0, 1, '', " + quote(packedEntries({action("Hammer Haft", 5, 5, "+5", "1d8+3 bludgeoning"), action("Ring Clapper Key", 5, 10, "+0", "0", "Alternative solution: calms the hedge.")})) + ", 'pc;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_hedge_gate' UNION ALL "
             "SELECT 'part_hedge_nia', id, 0, 'char_nia_roseglass', '', 'Nia Roseglass', 13, 16, 16, 14, 0, 30, 'Party', 1, 0, 0, 5, 5, 5, 5, 5, 5, 0, 0, 1, '', " + quote(packedEntries({action("Kind Word", 5, 30, "+0", "1d8 + ability"), action("Hearthward", 5, 10, "+0", "0", "Ritual solution to stabilize the lantern.")})) + ", 'pc;caster;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_hedge_gate' UNION ALL "
             "SELECT 'part_hedge_snare', id, 1, 'npc_brambleshade', '', 'Brambleshade Snare', 9, 24, 24, 13, 1, 10, 'Guardian', 1, 0, 0, 8, 5, 8, 5, 8, 5, 0, 0, 0, 'Guarding;', " + quote(packedEntries({action("Grasping Vine", 10, 10, "+4", "1d8 bludgeoning", "Restrains on a failed save if desired."), action("Thorn Wall", 5, 15, "+0", "0", "Creates difficult terrain.")})) + ", 'guardian;nonlethal;', datetime('now'), datetime('now') FROM encounters WHERE stable_id='enc_hedge_gate';");

        exec(db,
             "INSERT INTO story_threads (stable_id, campaign_id, title, thread_type, status, summary, stakes, gm_notes, tags, created_at, updated_at) VALUES "
             "('story_lights_before_moonrise'," + campaignRow + ",'Lights Before Moonrise',0,1,'A complete starter adventure in five scenes: accept Iona''s request, find the missing honey, face the moth envoy, enter the hedge, and light the ceremony lantern.','If the party fails, nobody dies; the fair ends in embarrassment and the hedge court claims the central lantern for a year.','Playable in one short session. Push toward negotiation first, but combat tracker is prepped if talks fail.','main;playable;',datetime('now'),datetime('now')),"
             "('story_pips_lucky_badge'," + campaignRow + ",'Pip''s Lucky Badge',1,1,'Pip can turn the fair warden badge into a promise from Iona or a useful village contact.','A small personal win helps model character story links.','Ask Pip how they know Bram.','character;',datetime('now'),datetime('now')),"
             "('story_hedge_court_debt'," + campaignRow + ",'The Hedge Court Debt',2,1,'The village forgot an old promise: honey and candlelight for safe harvest weather.','Resolving this creates a recurring faction hook.','The moth is an envoy, not a villain.','fey;faction;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_iona_request', id, 'Scene 1: Iona''s Request', 'Iona gives the fair badges and asks the party to watch the candle stalls.', 'Open the handout, then ask each player what they are doing at the fair.', 1, 'Fair Day - Late Afternoon', 'audio_fair_day', 0.35, 1, 'scene;opening;', datetime('now'), datetime('now'), 0 FROM story_threads WHERE stable_id='story_lights_before_moonrise';");
        exec(db,
             "INSERT INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_missing_honey', id, 'Scene 2: Missing Honey', 'Bram reports that moonseed honey and candle-flames vanished together.', 'Clues: wax dust near hedge, tiny wing scales, child heard rhyming.', 0, 'Fair Day - Golden Hour', 'audio_moth_flutter', 0.6, 0, 'scene;clue;', datetime('now'), datetime('now'), 1 FROM story_threads WHERE stable_id='story_lights_before_moonrise';");
        exec(db,
             "INSERT INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_moth_parley', id, 'Scene 3: Moth Mischief', 'The moth appears. It wants honey, apology, and the old clapper key rung at the hedge.', 'Use encounter if chase or combat starts. Honey grants advantage on parley.', 0, 'Fair Day - Sunset', 'audio_moth_flutter', 0.7, 0, 'scene;encounter;', datetime('now'), datetime('now'), 2 FROM story_threads WHERE stable_id='story_lights_before_moonrise';");
        exec(db,
             "INSERT INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_hedge_gate', id, 'Scene 4: Moonlit Hedge Gate', 'The bramble guardian blocks the old lantern. Ring, negotiate, ritualize, or fight.', 'Run Hedge Gate. Nonviolent solutions end combat immediately.', 0, 'Fair Day - Moonrise', '', 0, 0, 'scene;guardian;', datetime('now'), datetime('now'), 3 FROM story_threads WHERE stable_id='story_lights_before_moonrise';");
        exec(db,
             "INSERT INTO story_beats (stable_id, thread_id, title, summary, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order) "
             "SELECT 'beat_final_lantern', id, 'Scene 5: The Bell Rings', 'The central lantern is lit and the hedge court accepts renewed tribute.', 'Play ceremony cue. Award Saved Ceremony Thanks.', 0, 'Fair Day - Moonrise', 'audio_lantern_ceremony', 0.65, 0, 'scene;finale;', datetime('now'), datetime('now'), 4 FROM story_threads WHERE stable_id='story_lights_before_moonrise';");

        exec(db,
             "INSERT INTO story_links (stable_id, campaign_id, owner_entity_id, owner_kind, link_type, label, target_entity_id, target_entity_type, tags, created_at, updated_at) VALUES "
             "('link_main_map'," + campaignRow + ",'story_lights_before_moonrise',0,1,'main fair map','map_lantern_fair','Map','',datetime('now'),datetime('now')),"
             "('link_main_hedge'," + campaignRow + ",'story_lights_before_moonrise',0,1,'second scene map','map_moonlit_hedge','Map','',datetime('now'),datetime('now')),"
             "('link_main_iona'," + campaignRow + ",'story_lights_before_moonrise',0,1,'quest giver','npc_iona_bellweather','NPC','',datetime('now'),datetime('now')),"
             "('link_main_moth'," + campaignRow + ",'story_lights_before_moonrise',0,1,'envoy and complication','npc_lantern_moth','NPC','',datetime('now'),datetime('now')),"
             "('link_scene_moth_enc'," + campaignRow + ",'beat_moth_parley',1,1,'run if chase starts','enc_moth_mischief','Encounter','',datetime('now'),datetime('now')),"
             "('link_scene_hedge_enc'," + campaignRow + ",'beat_hedge_gate',1,1,'guardian scene','enc_hedge_gate','Encounter','',datetime('now'),datetime('now')),"
             "('link_scene_final_audio'," + campaignRow + ",'beat_final_lantern',1,3,'play at finale','audio_lantern_ceremony','Audio','',datetime('now'),datetime('now')),"
             "('link_honey_item'," + campaignRow + ",'beat_moth_parley',1,5,'peace offering','item_moonseed_honey','Item','',datetime('now'),datetime('now')),"
             "('link_pip_badge'," + campaignRow + ",'story_pips_lucky_badge',0,1,'personal token','char_pip_arden','Player','',datetime('now'),datetime('now')),"
             "('link_court_debt'," + campaignRow + ",'story_hedge_court_debt',0,1,'fey envoy','npc_lantern_moth','NPC','',datetime('now'),datetime('now'));");

        exec(db,
             "INSERT INTO story_documents (stable_id, campaign_id, title, body, tags, created_at, updated_at) VALUES "
             "('doc_lantern_fair_runbook'," + campaignRow + ",'GM Runbook - Lights Before Moonrise'," + quote("Tone: cozy mystery, low danger, useful tools. Start with Iona at the main stage. Let the players inspect stalls, talk to Bram, and discover the moth. The adventure is designed for one 60-90 minute session. Resolve scenes with checks, roleplay, or the prebuilt encounters. Finale: ring the bell clapper, offer honey, or cast Hearthward to settle the hedge court debt.") + ",'gm;runbook;',datetime('now'),datetime('now')),"
             "('doc_scene_dc_cards'," + campaignRow + ",'Scene DCs and Outcomes'," + quote("DC 10 notice wax dust. DC 12 calm frightened children. DC 13 track the moth to the hedge. DC 14 persuade the moth without honey. DC 12 use the bell clapper key correctly. Failed checks complicate the scene, not the campaign: lose time, split the crowd, or trigger an encounter.") + ",'gm;dc;',datetime('now'),datetime('now')),"
             "('doc_player_notice'," + campaignRow + ",'Player Handout - Fair Notice'," + quote("Bellweather Green welcomes travelers to the Lantern Fair. Volunteers receive a brass badge, one supper ticket, and the gratitude of Iona Bellweather. Keep candles upright, children out of the bell tower, and all honey jars accounted for.") + ",'handout;player;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO handouts (stable_id, campaign_id, title, body, file_path, gm_notes, tags, created_at, updated_at) VALUES "
             "('handout_fair_notice'," + campaignRow + ",'Bellweather Fair Notice','A public notice for the players before the first scene.'," + quote(projectPath(assets.handoutNotice)) + ",'Show this before Iona asks for help.','player;fair;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO timeline_events (stable_id, campaign_id, title, description, in_world_date, related_entity_ids, tags, created_at, updated_at) VALUES "
             "('timeline_badges_given'," + campaignRow + ",'Fair Badges Given','Iona deputizes the party as fair wardens.','Late Afternoon','npc_iona_bellweather;item_fair_badge;beat_iona_request','opening;',datetime('now'),datetime('now')),"
             "('timeline_moth_revealed'," + campaignRow + ",'The Moth Speaks','The party learns the thefts are overdue fey tribute.','Sunset','npc_lantern_moth;item_moonseed_honey;beat_moth_parley','reveal;',datetime('now'),datetime('now')),"
             "('timeline_bell_rings'," + campaignRow + ",'The Bell Rings','The old promise is renewed and the central lantern lights.','Moonrise','audio_lantern_ceremony;parcel_saved_ceremony;beat_final_lantern','finale;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO sessions (stable_id, campaign_id, title, scheduled_at_utc, started_at_utc, ended_at_utc, summary, recap, unresolved_threads, loot_awarded, xp_milestones, npcs_met, locations_visited, open_hooks, participant_character_ids, encounter_ids, story_document_ids, live_mode_active, tags, created_at, updated_at) VALUES "
             "('session_lantern_fair_one'," + campaignRow + ",'Session 1: Lights Before Moonrise','','','','Run the complete Lantern Fair starter adventure.','Not started.','Will the party renew the hedge court promise?','Saved Ceremony Thanks parcel after finale.','Level 2 one-shot; milestone complete when the final lantern is lit.','Iona Bellweather;Bram Candlewick;Lantern Moth Envoy','Bellweather Green;Moonlit Hedge Path','The Hedge Court may return next harvest.','char_pip_arden;char_mara_velle;char_toren_mill;char_nia_roseglass','enc_moth_mischief;enc_hedge_gate','doc_lantern_fair_runbook;doc_scene_dc_cards;doc_player_notice',0,'playable;starter;',datetime('now'),datetime('now'));");
        exec(db,
             "INSERT INTO session_notes (stable_id, session_id, timestamp_utc, body, linked_entity_id, linked_entity_type, linked_entity_label, tags, created_at, updated_at) "
             "SELECT 'note_start_with_audio', id, datetime('now'), 'Start Fair Day Ambience, then open the player handout and ask each PC where they are at the fair.', 'audio_fair_day', 'Audio', 'Fair Day Ambience', 'gm;', datetime('now'), datetime('now') FROM sessions WHERE stable_id='session_lantern_fair_one' UNION ALL "
             "SELECT 'note_finale_reward', id, datetime('now'), 'When the bell rings, play Lantern Ceremony Cue and award Saved Ceremony Thanks.', 'parcel_saved_ceremony', 'Loot', 'Saved Ceremony Thanks', 'gm;', datetime('now'), datetime('now') FROM sessions WHERE stable_id='session_lantern_fair_one';");

        insertAssetMetadata(db, campaignRow, "asset_pip_portrait", "portrait", "Pip Arden Portrait", assets.pipPortrait, "image/bmp", "portrait;player;");
        insertAssetMetadata(db, campaignRow, "asset_mara_portrait", "portrait", "Mara Velle Portrait", assets.maraPortrait, "image/bmp", "portrait;player;");
        insertAssetMetadata(db, campaignRow, "asset_toren_portrait", "portrait", "Toren Mill Portrait", assets.torenPortrait, "image/bmp", "portrait;player;");
        insertAssetMetadata(db, campaignRow, "asset_nia_portrait", "portrait", "Nia Roseglass Portrait", assets.niaPortrait, "image/bmp", "portrait;player;");
        insertAssetMetadata(db, campaignRow, "asset_iona_portrait", "portrait", "Iona Bellweather Portrait", assets.ionaPortrait, "image/bmp", "portrait;npc;");
        insertAssetMetadata(db, campaignRow, "asset_moth_portrait", "portrait", "Lantern Moth Portrait", assets.mothPortrait, "image/bmp", "portrait;npc;");
        insertAssetMetadata(db, campaignRow, "asset_fair_map", "map", "Bellweather Green Map", assets.fairMap, "image/bmp", "map;fair;");
        insertAssetMetadata(db, campaignRow, "asset_hedge_map", "map", "Moonlit Hedge Path Map", assets.hedgeMap, "image/bmp", "map;hedge;");
        insertAssetMetadata(db, campaignRow, "asset_fair_audio", "audio", "Fair Day Ambience", assets.fairAmbience, "audio/wav", "audio;ambience;");
        insertAssetMetadata(db, campaignRow, "asset_ceremony_audio", "audio", "Lantern Ceremony Cue", assets.ceremonyCue, "audio/wav", "audio;music;");
        insertAssetMetadata(db, campaignRow, "asset_moth_audio", "audio", "Moth Flutter", assets.mothFlutter, "audio/wav", "audio;effect;");
        insertAssetMetadata(db, campaignRow, "asset_fair_notice", "handout", "Bellweather Fair Notice", assets.handoutNotice, "text/plain", "handout;player;");

        exec(db, "COMMIT;");
        exec(db, "VACUUM;");
        sqlite3_close(db);
    }
    catch (...)
    {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
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
            throw std::runtime_error("Usage: seed_onboarding_sample <db> [db...]");
        pruneKnownOutdatedResources();
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
