#include "Models/DataBaseManager.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "SQLiteCpp/SQLiteCpp.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
bool tableExists(SQLite::Database &database, const std::string &tableName)
{
    SQLite::Statement query(database, "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1;");
    query.bind(1, tableName);
    return query.executeStep();
}

bool columnExists(SQLite::Database &database, const std::string &tableName, const std::string &columnName)
{
    SQLite::Statement query(database, "PRAGMA table_info(" + tableName + ");");
    while (query.executeStep())
    {
        if (query.getColumn(1).getString() == columnName)
            return true;
    }
    return false;
}

int scalarInt(SQLite::Database &database, const std::string &sql)
{
    SQLite::Statement query(database, sql);
    if (!query.executeStep())
        return 0;
    return query.getColumn(0).getInt();
}

bool require(bool condition, const std::string &message)
{
    if (condition)
        return true;

    std::cerr << message << std::endl;
    return false;
}
}

#include "spell_override_migration_cases.hpp"

int main()
{
    try
    {
        const auto databasePath = ApplicationPaths::databasePath();
        std::filesystem::remove(databasePath);

        DatabaseManager::getInstance();
        SQLite::Database database(databasePath.string(), SQLite::OPEN_READONLY);

        const std::vector<std::string> expectedTables = {
            "schema_migrations",
            "campaigns",
            "characters",
            "inventories",
            "inventory_entries",
            "npcs",
            "nations",
            "nation_territories",
            "world_places",
            "custom_items",
            "custom_spells",
            "maps",
            "map_layers",
            "map_tokens",
            "audio_assets",
            "audio_playlists",
            "audio_ambience_layers",
            "audio_triggers",
            "asset_metadata",
            "custom_monsters",
            "encounters",
            "encounter_participants",
            "story_threads",
            "story_beats",
            "story_links",
            "story_documents",
            "timeline_events",
            "shops",
            "shop_stock",
            "loot_parcels",
            "loot_parcel_items",
            "dungeons",
            "dungeon_rooms",
            "handouts",
            "random_tables",
            "random_table_entries",
            "local_player_display",
            "sessions",
            "session_notes",
            "ruleset_definitions",
            "homebrew_definitions",
            "house_rules"};

        bool ok = true;
        for (const std::string &table : expectedTables)
            ok &= require(tableExists(database, table), "Missing expected table: " + table);

        ok &= require(columnExists(database, "campaigns", "stable_id"), "campaigns.stable_id migration missing");
        ok &= require(columnExists(database, "campaigns", "content_language"), "campaigns.content_language migration missing");
        ok &= require(columnExists(database, "characters", "spellbook_entries"), "characters.spellbook_entries migration missing");
        ok &= require(columnExists(database, "inventories", "owner_entity_id"), "inventories.owner_entity_id migration missing");
        ok &= require(columnExists(database, "inventory_entries", "slot"), "inventory_entries.slot migration missing");
        ok &= require(columnExists(database, "inventory_entries", "container_entry_id"), "inventory_entries.container_entry_id migration missing");
        ok &= require(columnExists(database, "inventory_entries", "attuned"), "inventory_entries.attuned migration missing");
        ok &= require(columnExists(database, "inventory_entries", "charges"), "inventory_entries.charges migration missing");
        ok &= require(columnExists(database, "inventory_entries", "ammunition_remaining"), "inventory_entries.ammunition_remaining migration missing");
        ok &= require(columnExists(database, "inventory_entries", "durability"), "inventory_entries.durability migration missing");
        ok &= require(columnExists(database, "npcs", "spellbook_entries"), "npcs.spellbook_entries migration missing");
        ok &= require(columnExists(database, "nations", "capital_place_id"), "nations.capital_place_id migration missing");
        ok &= require(columnExists(database, "nations", "faction_ids"), "nations.faction_ids migration missing");
        ok &= require(columnExists(database, "nations", "visibility"), "nations.visibility migration missing");
        ok &= require(columnExists(database, "nation_territories", "polygon"), "nation_territories.polygon migration missing");
        ok &= require(columnExists(database, "world_places", "place_type"), "world_places.place_type migration missing");
        ok &= require(columnExists(database, "world_places", "linked_feature_id"), "world_places.linked_feature_id migration missing");
        ok &= require(columnExists(database, "world_places", "has_map_position"), "world_places.has_map_position migration missing");
        ok &= require(columnExists(database, "world_places", "map_position_x"), "world_places.map_position_x migration missing");
        ok &= require(columnExists(database, "world_places", "map_position_y"), "world_places.map_position_y migration missing");
        ok &= require(columnExists(database, "custom_items", "weapon_damage"), "custom_items.weapon_damage migration missing");
        ok &= require(columnExists(database, "custom_items", "weapon_long_range_feet"), "custom_items.weapon_long_range_feet migration missing");
        ok &= require(columnExists(database, "custom_items", "weapon_properties"), "custom_items.weapon_properties migration missing");
        ok &= require(columnExists(database, "custom_items", "is_container"), "custom_items.is_container migration missing");
        ok &= require(columnExists(database, "custom_items", "requires_attunement"), "custom_items.requires_attunement migration missing");
        ok &= require(columnExists(database, "custom_items", "max_charges"), "custom_items.max_charges migration missing");
        ok &= require(columnExists(database, "custom_items", "cursed"), "custom_items.cursed migration missing");
        ok &= require(columnExists(database, "custom_items", "activation"), "custom_items.activation migration missing");
        ok &= require(columnExists(database, "custom_items", "linked_spell_id"), "custom_items.linked_spell_id migration missing");
        ok &= require(columnExists(database, "custom_items", "command_words"), "custom_items.command_words migration missing");
        ok &= require(columnExists(database, "custom_items", "is_ammunition"), "custom_items.is_ammunition migration missing");
        ok &= require(columnExists(database, "custom_items", "max_durability"), "custom_items.max_durability migration missing");
        ok &= require(columnExists(database, "custom_spells", "effect_summary"), "custom_spells.effect_summary migration missing");
        ok &= require(columnExists(database, "maps", "cells"), "maps.cells migration missing");
        ok &= require(columnExists(database, "maps", "atlas_data"), "maps.atlas_data migration missing");
        ok &= require(columnExists(database, "audio_assets", "file_path"), "audio_assets.file_path migration missing");
        ok &= require(columnExists(database, "audio_assets", "duration_seconds"), "audio_assets.duration_seconds migration missing");
        ok &= require(columnExists(database, "audio_assets", "missing_file"), "audio_assets.missing_file migration missing");
        ok &= require(columnExists(database, "audio_assets", "last_played_at_utc"), "audio_assets.last_played_at_utc migration missing");
        ok &= require(columnExists(database, "custom_monsters", "actions"), "custom_monsters.actions migration missing");
        ok &= require(columnExists(database, "encounters", "combat_log"), "encounters.combat_log migration missing");
        ok &= require(columnExists(database, "encounters", "dice_roll_presets"), "encounters.dice_roll_presets migration missing");
        ok &= require(columnExists(database, "encounters", "combat_paused"), "encounters.combat_paused migration missing");
        ok &= require(columnExists(database, "encounters", "theater_of_mind"), "encounters.theater_of_mind migration missing");
        ok &= require(columnExists(database, "encounters", "after_combat_summary"), "encounters.after_combat_summary migration missing");
        ok &= require(columnExists(database, "encounters", "after_combat_loot_notes"), "encounters.after_combat_loot_notes migration missing");
        ok &= require(columnExists(database, "encounters", "after_combat_story_consequences"), "encounters.after_combat_story_consequences migration missing");
        ok &= require(columnExists(database, "encounters", "after_combat_session_notes"), "encounters.after_combat_session_notes migration missing");
        ok &= require(columnExists(database, "encounters", "source_map_id"), "encounters.source_map_id migration missing");
        ok &= require(columnExists(database, "encounters", "source_rect_x"), "encounters.source_rect_x migration missing");
        ok &= require(columnExists(database, "encounters", "source_rect_y"), "encounters.source_rect_y migration missing");
        ok &= require(columnExists(database, "encounters", "source_rect_width"), "encounters.source_rect_width migration missing");
        ok &= require(columnExists(database, "encounters", "source_rect_height"), "encounters.source_rect_height migration missing");
        ok &= require(columnExists(database, "encounters", "source_map_revision"), "encounters.source_map_revision migration missing");
        ok &= require(columnExists(database, "encounters", "tactical_map_id"), "encounters.tactical_map_id migration missing");
        ok &= require(columnExists(database, "encounter_participants", "actions"), "encounter_participants.actions migration missing");
        ok &= require(columnExists(database, "encounter_participants", "temporary_hp"), "encounter_participants.temporary_hp migration missing");
        ok &= require(columnExists(database, "encounter_participants", "group_name"), "encounter_participants.group_name migration missing");
        ok &= require(columnExists(database, "encounter_participants", "wave_number"), "encounter_participants.wave_number migration missing");
        ok &= require(columnExists(database, "encounter_participants", "starts_hidden"), "encounter_participants.starts_hidden migration missing");
        ok &= require(columnExists(database, "encounter_participants", "reinforcement_round"), "encounter_participants.reinforcement_round migration missing");
        ok &= require(columnExists(database, "encounter_participants", "starting_x"), "encounter_participants.starting_x migration missing");
        ok &= require(columnExists(database, "encounter_participants", "starting_y"), "encounter_participants.starting_y migration missing");
        ok &= require(columnExists(database, "encounter_participants", "damage_resistances"), "encounter_participants.damage_resistances migration missing");
        ok &= require(columnExists(database, "encounter_participants", "damage_vulnerabilities"), "encounter_participants.damage_vulnerabilities migration missing");
        ok &= require(columnExists(database, "encounter_participants", "damage_immunities"), "encounter_participants.damage_immunities migration missing");
        ok &= require(columnExists(database, "encounter_participants", "narrative_position"), "encounter_participants.narrative_position migration missing");
        ok &= require(columnExists(database, "encounter_participants", "narrative_range_band"), "encounter_participants.narrative_range_band migration missing");
        ok &= require(columnExists(database, "encounter_participants", "narrative_spotlight"), "encounter_participants.narrative_spotlight migration missing");
        ok &= require(columnExists(database, "story_threads", "gm_notes"), "story_threads.gm_notes migration missing");
        ok &= require(columnExists(database, "story_beats", "player_summary"), "story_beats.player_summary migration missing");
        ok &= require(columnExists(database, "story_beats", "read_aloud_text"), "story_beats.read_aloud_text migration missing");
        ok &= require(columnExists(database, "story_beats", "clues"), "story_beats.clues migration missing");
        ok &= require(columnExists(database, "story_beats", "consequences"), "story_beats.consequences migration missing");
        ok &= require(columnExists(database, "story_beats", "cue_audio_asset_id"), "story_beats.cue_audio_asset_id migration missing");
        ok &= require(columnExists(database, "story_links", "target_entity_id"), "story_links.target_entity_id migration missing");
        ok &= require(columnExists(database, "map_layers", "visible"), "map_layers.visible migration missing");
        ok &= require(columnExists(database, "maps", "features"), "maps.features migration missing");
        ok &= require(columnExists(database, "maps", "regions"), "maps.regions migration missing");
        ok &= require(columnExists(database, "maps", "routes"), "maps.routes migration missing");
        ok &= require(columnExists(database, "maps", "generation_data"), "maps.generation_data migration missing");
        ok &= require(columnExists(database, "maps", "background_image_path"), "maps.background_image_path migration missing");
        ok &= require(columnExists(database, "map_tokens", "kind"), "map_tokens.kind migration missing");
        ok &= require(columnExists(database, "map_tokens", "layer_id"), "map_tokens.layer_id migration missing");
        ok &= require(columnExists(database, "map_tokens", "linked_entity_type"), "map_tokens.linked_entity_type migration missing");
        ok &= require(columnExists(database, "map_tokens", "hidden_from_players"), "map_tokens.hidden_from_players migration missing");
        ok &= require(columnExists(database, "audio_playlists", "audio_asset_ids"), "audio_playlists.audio_asset_ids migration missing");
        ok &= require(columnExists(database, "audio_playlists", "repeat"), "audio_playlists.repeat migration missing");
        ok &= require(columnExists(database, "audio_playlists", "one_shot"), "audio_playlists.one_shot migration missing");
        ok &= require(columnExists(database, "audio_ambience_layers", "audio_asset_id"), "audio_ambience_layers.audio_asset_id migration missing");
        ok &= require(columnExists(database, "audio_ambience_layers", "sort_order"), "audio_ambience_layers.sort_order migration missing");
        ok &= require(columnExists(database, "audio_triggers", "source_type"), "audio_triggers.source_type migration missing");
        ok &= require(columnExists(database, "audio_triggers", "event_name"), "audio_triggers.event_name migration missing");
        ok &= require(columnExists(database, "asset_metadata", "content_hash"), "asset_metadata.content_hash migration missing");
        ok &= require(columnExists(database, "shops", "merchant_name"), "shops.merchant_name migration missing");
        ok &= require(columnExists(database, "shops", "faction_reputation_notes"), "shops.faction_reputation_notes migration missing");
        ok &= require(columnExists(database, "shop_stock", "sell_price_multiplier"), "shop_stock.sell_price_multiplier migration missing");
        ok &= require(columnExists(database, "shop_stock", "availability"), "shop_stock.availability migration missing");
        ok &= require(columnExists(database, "shop_stock", "haggle_notes"), "shop_stock.haggle_notes migration missing");
        ok &= require(columnExists(database, "shop_stock", "faction_modifier"), "shop_stock.faction_modifier migration missing");
        ok &= require(columnExists(database, "shop_stock", "reputation_modifier"), "shop_stock.reputation_modifier migration missing");
        ok &= require(columnExists(database, "dungeons", "map_ids"), "dungeons.map_ids migration missing");
        ok &= require(columnExists(database, "dungeons", "active_room_id"), "dungeons.active_room_id migration missing");
        ok &= require(columnExists(database, "dungeon_rooms", "read_aloud_text"), "dungeon_rooms.read_aloud_text migration missing");
        ok &= require(columnExists(database, "dungeon_rooms", "exploration_state"), "dungeon_rooms.exploration_state migration missing");
        ok &= require(columnExists(database, "dungeon_rooms", "connections"), "dungeon_rooms.connections migration missing");
        ok &= require(columnExists(database, "campaigns", "travel_route_id"), "campaigns.travel_route_id migration missing");
        ok &= require(columnExists(database, "campaigns", "travel_miles_traveled"), "campaigns.travel_miles_traveled migration missing");
        ok &= require(columnExists(database, "campaigns", "audio_master_volume"), "campaigns.audio_master_volume migration missing");
        ok &= require(columnExists(database, "campaigns", "audio_music_volume"), "campaigns.audio_music_volume migration missing");
        ok &= require(columnExists(database, "campaigns", "audio_ambience_volume"), "campaigns.audio_ambience_volume migration missing");
        ok &= require(columnExists(database, "campaigns", "audio_effects_volume"), "campaigns.audio_effects_volume migration missing");
        ok &= require(columnExists(database, "campaigns", "audio_voice_volume"), "campaigns.audio_voice_volume migration missing");
        ok &= require(columnExists(database, "campaigns", "presentation_mode_active"), "campaigns.presentation_mode_active migration missing");
        ok &= require(columnExists(database, "campaigns", "calendar_name"), "campaigns.calendar_name migration missing");
        ok &= require(columnExists(database, "campaigns", "calendar_month_names"), "campaigns.calendar_month_names migration missing");
        ok &= require(columnExists(database, "campaigns", "calendar_month_lengths"), "campaigns.calendar_month_lengths migration missing");
        ok &= require(columnExists(database, "campaigns", "calendar_weekday_names"), "campaigns.calendar_weekday_names migration missing");
        ok &= require(columnExists(database, "campaigns", "calendar_hours_per_day"), "campaigns.calendar_hours_per_day migration missing");
        ok &= require(columnExists(database, "campaigns", "calendar_daylight_hours"), "campaigns.calendar_daylight_hours migration missing");
        ok &= require(columnExists(database, "campaigns", "calendar_night_hours"), "campaigns.calendar_night_hours migration missing");
        ok &= require(columnExists(database, "campaigns", "calendar_day_starts_at_hour"), "campaigns.calendar_day_starts_at_hour migration missing");
        ok &= require(columnExists(database, "campaigns", "clock_use_system_time"), "campaigns.clock_use_system_time migration missing");
        ok &= require(columnExists(database, "campaigns", "clock_running"), "campaigns.clock_running migration missing");
        ok &= require(columnExists(database, "campaigns", "clock_multiplier"), "campaigns.clock_multiplier migration missing");
        ok &= require(columnExists(database, "campaigns", "clock_anchor_minutes"), "campaigns.clock_anchor_minutes migration missing");
        ok &= require(columnExists(database, "campaigns", "clock_anchor_utc"), "campaigns.clock_anchor_utc migration missing");
        ok &= require(columnExists(database, "campaigns", "ruleset_id"), "campaigns.ruleset_id migration missing");
        ok &= require(columnExists(database, "campaigns", "ruleset_version"), "campaigns.ruleset_version migration missing");
        ok &= require(columnExists(database, "campaigns", "ruleset_overlay_id"), "campaigns.ruleset_overlay_id migration missing");
        ok &= require(columnExists(database, "campaigns", "ruleset_overlay_version"), "campaigns.ruleset_overlay_version migration missing");
        ok &= require(columnExists(database, "characters", "rule_values"), "characters.rule_values migration missing");
        ok &= require(columnExists(database, "characters", "temporary_hp"), "characters.temporary_hp migration missing");
        ok &= require(columnExists(database, "handouts", "file_path"), "handouts.file_path migration missing");
        ok &= require(columnExists(database, "handouts", "gm_notes"), "handouts.gm_notes migration missing");
        ok &= require(columnExists(database, "local_player_display", "public_map_id"), "local_player_display.public_map_id migration missing");
        ok &= require(columnExists(database, "local_player_display", "public_handout_ids"), "local_player_display.public_handout_ids migration missing");
        ok &= require(columnExists(database, "local_player_display", "show_shared_art"), "local_player_display.show_shared_art migration missing");
        ok &= require(columnExists(database, "local_player_display", "map_only"), "local_player_display.map_only migration missing");
        ok &= require(columnExists(database, "local_player_display", "camera_center_x") &&
                          columnExists(database, "local_player_display", "camera_center_y") &&
                          columnExists(database, "local_player_display", "camera_zoom"),
                      "local_player_display public camera migration missing");
        ok &= require(columnExists(database, "local_player_display", "follow_public_camera"),
                      "local_player_display.follow_public_camera migration missing");
        ok &= require(columnExists(database, "local_player_display", "show_party") &&
                          columnExists(database, "local_player_display", "show_party_spell_slots") &&
                          columnExists(database, "local_player_display", "visible_character_ids") &&
                          columnExists(database, "local_player_display", "player_text_scale"),
                      "local_player_display party and presentation-preference migration missing");
        ok &= require(columnExists(database, "local_player_display", "preferred_monitor_index") &&
                          columnExists(database, "local_player_display", "fullscreen_map_only"),
                      "local_player_display safe monitor/fullscreen migration missing");
        ok &= require(columnExists(database, "sessions", "live_mode_active"), "sessions.live_mode_active migration missing");
        ok &= require(columnExists(database, "sessions", "started_at_utc"), "sessions.started_at_utc migration missing");
        ok &= require(columnExists(database, "sessions", "ended_at_utc"), "sessions.ended_at_utc migration missing");
        ok &= require(columnExists(database, "sessions", "recap"), "sessions.recap migration missing");
        ok &= require(columnExists(database, "sessions", "unresolved_threads"), "sessions.unresolved_threads migration missing");
        ok &= require(columnExists(database, "sessions", "loot_awarded"), "sessions.loot_awarded migration missing");
        ok &= require(columnExists(database, "sessions", "xp_milestones"), "sessions.xp_milestones migration missing");
        ok &= require(columnExists(database, "sessions", "npcs_met"), "sessions.npcs_met migration missing");
        ok &= require(columnExists(database, "sessions", "locations_visited"), "sessions.locations_visited migration missing");
        ok &= require(columnExists(database, "sessions", "open_hooks"), "sessions.open_hooks migration missing");
        ok &= require(columnExists(database, "session_notes", "timestamp_utc"), "session_notes.timestamp_utc migration missing");
        ok &= require(columnExists(database, "session_notes", "linked_entity_id"), "session_notes.linked_entity_id migration missing");
        ok &= require(columnExists(database, "session_notes", "linked_entity_type"), "session_notes.linked_entity_type migration missing");
        ok &= require(columnExists(database, "story_documents", "body"), "story_documents.body migration missing");
        ok &= require(columnExists(database, "timeline_events", "related_entity_ids"), "timeline_events.related_entity_ids migration missing");
        ok &= require(columnExists(database, "ruleset_definitions", "definition_json"), "ruleset_definitions.definition_json migration missing");
        ok &= require(columnExists(database, "homebrew_definitions", "definition_json"), "homebrew_definitions.definition_json migration missing");
        ok &= require(columnExists(database, "homebrew_definitions", "source"), "homebrew_definitions.source migration missing");
        ok &= require(columnExists(database, "homebrew_definitions", "imported_at_utc"), "homebrew_definitions.imported_at_utc migration missing");
        ok &= require(columnExists(database, "homebrew_definitions", "enabled"), "homebrew_definitions.enabled migration missing");
        ok &= require(columnExists(database, "house_rules", "change_history"), "house_rules.change_history migration missing");

        ok &= require(columnExists(database, "sessions", "rough_notes") && columnExists(database, "sessions", "plan_entries_json") && columnExists(database, "sessions", "live_state_json"), "Session-plan migration columns missing");
        ok &= require(columnExists(database, "encounter_participants", "sort_order") && columnExists(database, "story_links", "sort_order"), "Collection ordering migration missing");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 25;") == 1, "Collection migration should be recorded exactly once");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 28;") == 1, "Random-table migration should be recorded exactly once");
        ok &= require(scalarInt(database, "PRAGMA user_version;") >= 29, "PRAGMA user_version was not advanced to the party-summary schema");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 23;") == 1, "Session-plan migration should be recorded exactly once");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 24;") == 1,
                      "Entity-property migration should be recorded exactly once");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'campaign_entity_properties';") == 1,
                      "Migration must create durable contextual-link and attribution storage");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 1;") == 1,
                      "Schema migration version 1 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 2;") == 1,
                      "Schema migration version 2 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 3;") == 1,
                      "Schema migration version 3 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 4;") == 1,
                      "Schema migration version 4 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 5;") == 1,
                      "Schema migration version 5 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 6;") == 1,
                      "Schema migration version 6 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 7;") == 1,
                      "Schema migration version 7 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 8;") == 1,
                      "Schema migration version 8 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 9;") == 1,
                      "Schema migration version 9 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 10;") == 1,
                      "Schema migration version 10 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 11;") == 1,
                      "Schema migration version 11 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 12;") == 1,
                      "Schema migration version 12 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 13;") == 1,
                      "Schema migration version 13 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 14;") == 1,
                      "Schema migration version 14 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 15;") == 1,
                      "Schema migration version 15 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 16;") == 1,
                      "Schema migration version 16 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 17;") == 1,
                      "Schema migration version 17 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 18;") == 1,
                      "Schema migration version 18 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 19;") == 1,
                      "Schema migration version 19 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 20;") == 1,
                      "Schema migration version 20 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 21;") == 1,
                      "Schema migration version 21 was not recorded");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 22;") == 1,
                      "Schema migration version 22 was not recorded");

        ok &= require(columnExists(database, "characters", "class_levels"), "Character class history migration missing");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 26;") == 1, "Schema migration 26 missing");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 27;") == 1, "Schema migration 27 missing");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 28;") == 1, "Schema migration 28 missing");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 29;") == 1, "Schema migration 29 missing");
        ok &= require(scalarInt(database, "SELECT COUNT(*) FROM schema_migrations WHERE version = 30;") == 1 &&
                          scalarInt(database, "PRAGMA user_version;") >= 30, "Spell-slot opt-in migration 30 missing");
        ok &= spellOverrideMigration();
        return ok ? 0 : 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
