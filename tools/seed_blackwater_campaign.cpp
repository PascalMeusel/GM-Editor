#include "Models/Campaign/Campaign.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "sqlite3.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr const char *kCampaignId = "campaign_bell_beneath_blackwater";
constexpr const char *kCampaignName = "The Bell Beneath Blackwater";

using Tags = std::initializer_list<std::string>;

std::vector<std::string> tagList(Tags tags)
{
    return std::vector<std::string>(tags.begin(), tags.end());
}

void setMetadata(EntityMetadata &metadata, const std::string &id, Tags tags = {})
{
    metadata.id = id;
    metadata.tags = tagList(tags);
    metadata.createdAtUtc = currentUtcTimestamp();
    metadata.updatedAtUtc = metadata.createdAtUtc;
}

std::filesystem::path findProjectRoot()
{
    std::filesystem::path current = std::filesystem::current_path();
    while (!current.empty())
    {
        if (std::filesystem::exists(current / "CMakeLists.txt") && std::filesystem::exists(current / "Resources"))
            return std::filesystem::absolute(current).lexically_normal();
        const std::filesystem::path parent = current.parent_path();
        if (parent == current)
            break;
        current = parent;
    }
    return std::filesystem::absolute(std::filesystem::current_path()).lexically_normal();
}

bool isInsideRoot(const std::filesystem::path &root, const std::filesystem::path &target)
{
    std::wstring rootText = std::filesystem::absolute(root).lexically_normal().wstring();
    std::wstring targetText = std::filesystem::absolute(target).lexically_normal().wstring();
    if (rootText.empty())
        return false;
    if (rootText.back() != L'\\' && rootText.back() != L'/')
        rootText.push_back(std::filesystem::path::preferred_separator);
    return targetText.rfind(rootText, 0) == 0;
}

void removePathInsideRoot(const std::filesystem::path &root, const std::filesystem::path &target)
{
    if (!std::filesystem::exists(target))
        return;
    if (!isInsideRoot(root, target))
        throw std::runtime_error("Refusing to remove path outside workspace: " + target.string());
    for (int attempt = 0; attempt < 6; ++attempt)
    {
        try
        {
            std::filesystem::remove_all(target);
            return;
        }
        catch (const std::filesystem::filesystem_error &)
        {
            if (attempt == 5)
                throw;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
}

void cleanSlate(const std::filesystem::path &root)
{
    removePathInsideRoot(root, root / "TTRPG.db");
    removePathInsideRoot(root, root / "TTRPG.db-wal");
    removePathInsideRoot(root, root / "TTRPG.db-shm");
    removePathInsideRoot(root, root / "Resources" / "Campaigns");
    removePathInsideRoot(root, root / "Resources" / "Backups");
    removePathInsideRoot(root, root / "Resources" / "Recovery");
    std::filesystem::create_directories(root / "Resources" / "Campaigns");
    std::filesystem::create_directories(root / "Resources" / "Backups");
    std::filesystem::create_directories(root / "Resources" / "Recovery");
}

std::filesystem::path packageRoot(const std::filesystem::path &root)
{
    return root / "Resources" / "Campaigns" / kCampaignId;
}

std::vector<std::string> blackwaterPortraitFiles()
{
    return {"pc_mira_voss.png", "pc_kael_thornevein.png", "pc_tamsin_reed.png", "pc_elyon_starling.png", "pc_nyra_moonbrook.png"};
}

std::filesystem::path portraitRoot(const std::filesystem::path &root)
{
    return packageRoot(root) / "Assets" / "Portraits";
}

std::filesystem::path cacheExistingPortraits(const std::filesystem::path &root)
{
    const std::filesystem::path sourceRoot = portraitRoot(root);
    const std::filesystem::path cacheRoot = root / "build" / "seed_blackwater_portrait_cache";
    bool copiedAny = false;

    removePathInsideRoot(root, cacheRoot);
    for (const std::string &file : blackwaterPortraitFiles())
    {
        const std::filesystem::path source = sourceRoot / file;
        if (!std::filesystem::exists(source))
            continue;
        std::filesystem::create_directories(cacheRoot);
        std::filesystem::copy_file(source, cacheRoot / file, std::filesystem::copy_options::overwrite_existing);
        copiedAny = true;
    }

    return copiedAny ? cacheRoot : std::filesystem::path{};
}

void restoreCachedPortraits(const std::filesystem::path &root, const std::filesystem::path &cacheRoot)
{
    if (cacheRoot.empty())
        return;

    for (const std::string &file : blackwaterPortraitFiles())
    {
        const std::filesystem::path source = cacheRoot / file;
        if (!std::filesystem::exists(source))
            continue;
        std::filesystem::create_directories(portraitRoot(root));
        std::filesystem::copy_file(source, portraitRoot(root) / file, std::filesystem::copy_options::overwrite_existing);
    }
    removePathInsideRoot(root, cacheRoot);
}

void requirePortraits(const std::filesystem::path &root)
{
    for (const std::string &file : blackwaterPortraitFiles())
    {
        const std::filesystem::path path = portraitRoot(root) / file;
        if (!std::filesystem::exists(path))
            throw std::runtime_error("Missing Blackwater generated portrait: " + path.string());
    }
}

std::string projectPath(const std::filesystem::path &root, const std::filesystem::path &path)
{
    return std::filesystem::relative(path, root).generic_string();
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
    // Preserve curated AI-painted showcase maps; this fallback only fills missing assets.
    if (std::filesystem::exists(path))
        return;
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
            const bool frame = x < 10 || y < 10 || x >= width - 10 || y >= height - 10;
            const bool pathLine = ((x + motif * 41) % 173) < 7 || ((y + motif * 29) % 139) < 6;
            const bool district = ((x / 80 + y / 72 + motif) % 5) == 0;
            const bool water = motif % 2 == 0 && y > height * 2 / 3 && ((x + y) % 17) < 8;
            std::uint8_t rr = static_cast<std::uint8_t>(std::clamp<int>(r + (frame ? 34 : 0) + (district ? 22 : 0), 0, 255));
            std::uint8_t gg = static_cast<std::uint8_t>(std::clamp<int>(g + (pathLine ? 42 : 0) + (water ? 20 : 0), 0, 255));
            std::uint8_t bb = static_cast<std::uint8_t>(std::clamp<int>(b + (water ? 62 : 0) + (district ? 16 : 0), 0, 255));
            row[static_cast<std::size_t>(x * 3 + 0)] = bb;
            row[static_cast<std::size_t>(x * 3 + 1)] = gg;
            row[static_cast<std::size_t>(x * 3 + 2)] = rr;
        }
        out.write(reinterpret_cast<const char *>(row.data()), row.size());
    }
}

void writeTextFile(const std::filesystem::path &path, const std::string &text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("Could not write text file: " + path.string());
    out << text;
}

struct AudioSeed
{
    std::string id;
    std::string name;
    std::string sourcePath;
    AudioAssetType type = AudioAssetType::Other;
    float volume = 1.0f;
    bool loop = false;
    int duration = 0;
    std::vector<std::string> tags;
};

std::vector<AudioSeed> audioSeeds()
{
    return {
        {"audio_town_theme", "Town Theme", "Resources/Audio/Music/town_theme_rpg.mp3", AudioAssetType::Music, 0.55f, true, 91, {"music", "town", "safe"}},
        {"audio_old_tower_inn", "Old Tower Inn", "Resources/Audio/Music/old_tower_inn.mp3", AudioAssetType::Music, 0.52f, true, 102, {"music", "tavern", "social"}},
        {"audio_mystery_exploration", "Mystery Exploration", "Resources/Audio/Music/mystery_exploration.mp3", AudioAssetType::Music, 0.48f, true, 104, {"music", "mystery", "clues"}},
        {"audio_field_of_dreams", "Field of Dreams", "Resources/Audio/Music/field_of_dreams.mp3", AudioAssetType::Music, 0.46f, true, 107, {"music", "mystic", "memory"}},
        {"audio_cave_explorer", "Cave Explorer", "Resources/Audio/Music/cave_explorer.mp3", AudioAssetType::Music, 0.45f, true, 95, {"music", "dungeon", "mine"}},
        {"audio_ancient_power", "Ancient Power", "Resources/Audio/Music/ancient_power_of_serpents.mp3", AudioAssetType::Music, 0.60f, false, 117, {"music", "ritual", "ancient"}},
        {"audio_medieval_battle", "Medieval Battle", "Resources/Audio/Music/medieval_battle.mp3", AudioAssetType::Music, 0.64f, false, 95, {"music", "combat"}},
        {"audio_orchestral_battle", "Orchestral Battle", "Resources/Audio/Music/orchestral_battle_theme.mp3", AudioAssetType::Music, 0.66f, false, 124, {"music", "combat", "elite"}},
        {"audio_boss_battle", "Boss Battle", "Resources/Audio/Music/fighting_is_not_an_option.mp3", AudioAssetType::Music, 0.68f, true, 108, {"music", "boss", "finale"}},
        {"audio_victory", "Victory Fanfare", "Resources/Audio/Music/medieval_victory.mp3", AudioAssetType::Music, 0.70f, false, 46, {"music", "victory"}},
        {"audio_rain_gutter", "Rain in the Gutter", "Resources/Audio/Ambience/rain_gutter_loop.mp3", AudioAssetType::Ambience, 0.30f, true, 83, {"ambience", "rain", "town"}},
        {"audio_forgotten_tomb", "Forgotten Tomb", "Resources/Audio/Ambience/forgotten_tomb_ambience.mp3", AudioAssetType::Ambience, 0.25f, true, 91, {"ambience", "dungeon", "tomb"}},
        {"audio_deep_rumble", "Deep Rumble", "Resources/Audio/Ambience/deep_rumble.mp3", AudioAssetType::Ambience, 0.28f, true, 92, {"ambience", "ritual", "tension"}},
        {"audio_deep_sea", "Deep Sea", "Resources/Audio/Ambience/deep_sea.mp3", AudioAssetType::Ambience, 0.28f, true, 90, {"ambience", "lake", "submerged"}},
        {"audio_arcane_pulse_bed", "Arcane Pulse Bed", "Resources/Audio/Ambience/scifi_pulse.mp3", AudioAssetType::Ambience, 0.24f, true, 90, {"ambience", "arcane", "machine"}},
        {"audio_danger_stinger", "Danger Stinger", "Resources/Audio/Effects/misc_short_alert.wav", AudioAssetType::Effect, 0.70f, false, 2, {"effect", "danger", "stinger"}},
        {"audio_low_impact", "Low Impact", "Resources/Audio/Effects/misc_low_impact.wav", AudioAssetType::Effect, 0.68f, false, 2, {"effect", "impact", "reveal"}},
        {"audio_creaky_door", "Creaky Door", "Resources/Audio/Effects/world_creaky_door.wav", AudioAssetType::Effect, 0.72f, false, 3, {"effect", "door", "dungeon"}},
        {"audio_coin_pouch", "Coin Pouch", "Resources/Audio/Effects/inventory_coin_pouch.wav", AudioAssetType::Effect, 0.72f, false, 2, {"effect", "loot", "shop"}},
        {"audio_bottle_clink", "Bottle Clink", "Resources/Audio/Effects/inventory_bottle_clink.wav", AudioAssetType::Effect, 0.70f, false, 2, {"effect", "potion", "tavern"}},
        {"audio_metal_ring", "Ringing Metal", "Resources/Audio/Effects/inventory_metal_ring.wav", AudioAssetType::Effect, 0.68f, false, 2, {"effect", "key", "bell"}},
        {"audio_unsheathe", "Sharp Unsheathe", "Resources/Audio/Effects/weapon_unsheathe_sharp.wav", AudioAssetType::Effect, 0.78f, false, 2, {"effect", "combat", "initiative"}},
        {"audio_weapon_light", "Light Weapon Swing", "Resources/Audio/Effects/weapon_swing_light.wav", AudioAssetType::Effect, 0.76f, false, 2, {"effect", "combat", "attack"}},
        {"audio_spell_arcane_pulse", "Arcane Pulse", "Resources/Audio/Effects/spell_arcane_pulse.wav", AudioAssetType::Effect, 0.72f, false, 3, {"effect", "magic", "ward"}},
        {"audio_spell_deep_cast", "Deep Spell Cast", "Resources/Audio/Effects/spell_deep_cast.wav", AudioAssetType::Effect, 0.74f, false, 4, {"effect", "magic", "ritual"}},
        {"audio_shadow_whisper", "Shadow Whisper", "Resources/Audio/Effects/npc_shadow_whisper.wav", AudioAssetType::Effect, 0.66f, false, 4, {"effect", "ghost", "whisper"}},
        {"audio_shade_call", "Shade Call", "Resources/Audio/Effects/npc_shade_long_1.wav", AudioAssetType::Effect, 0.66f, false, 4, {"effect", "shade", "undead"}}};
}

std::filesystem::path audioTargetPath(const std::filesystem::path &root, const AudioSeed &seed)
{
    const std::filesystem::path source(seed.sourcePath);
    const std::string folder = source.parent_path().filename().string();
    return packageRoot(root) / "Assets" / "Audio" / folder / source.filename();
}

void createProjectAssets(const std::filesystem::path &root)
{
    const std::filesystem::path campaignRoot = packageRoot(root);
    for (const std::string &folder : {"Images", "Portraits", "Tokens", "Maps", "Audio", "Handouts", "Files"})
        std::filesystem::create_directories(campaignRoot / "Assets" / folder);

    writeBmp(campaignRoot / "Assets" / "Maps" / "silted_crown_common_room.bmp", 900, 620, 54, 62, 76, 1);
    writeBmp(campaignRoot / "Assets" / "Maps" / "blackwater_town_ward.bmp", 1100, 760, 56, 74, 92, 2);
    writeBmp(campaignRoot / "Assets" / "Maps" / "old_boathouse_ambush.bmp", 960, 704, 74, 62, 50, 3);
    writeBmp(campaignRoot / "Assets" / "Maps" / "reedfen_marsh_causeway.bmp", 980, 720, 45, 84, 78, 4);
    writeBmp(campaignRoot / "Assets" / "Maps" / "abandoned_bellfoundry_cellars.bmp", 980, 720, 83, 72, 64, 5);
    writeBmp(campaignRoot / "Assets" / "Maps" / "lake_chapel_ruins.bmp", 1000, 760, 66, 80, 96, 6);
    writeBmp(campaignRoot / "Assets" / "Maps" / "mnemosyne_bell_chamber.bmp", 980, 720, 82, 72, 106, 7);

    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h01_cracked_inn_ledger.txt",
                  "Cracked Inn Ledger\n\n"
                  "Several names are crossed out in their own handwriting. A final line reads: "
                  "\"I checked in tomorrow. I left forty years ago.\"\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h02_old_mine_map.txt",
                  "Old Mine Map\n\n"
                  "Blackwater Mine before the flood. A hidden lower chamber is marked only by a bell symbol and the phrase: "
                  "\"No ringing below third shaft.\"\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h03_childs_drawing.txt",
                  "Nessa's Charcoal Drawing\n\n"
                  "A giant bell hangs over black water. Twelve faceless people stand around it, each holding a candle in their mouth.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h04_council_disaster_notice.txt",
                  "Council Disaster Notice\n\n"
                  "Official notice: thirty-two miners died in a gas explosion. Marginal notes show the victims were locked below before floodgates opened.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h05_ash_choir_hymn.txt",
                  "Ash Choir Hymn Sheet\n\n"
                  "The devotional text is gentle at first glance. The first letter of each line spells: WAKE THE REGENT.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h06_lantern_company_invoice.txt",
                  "Lantern Company Invoice\n\n"
                  "Coded shipping order: bronze acoustic relic fragments, mnemonic residue, and submersible containment rigging.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h07_waterlogged_locket.txt",
                  "Waterlogged Locket\n\n"
                  "Two portraits remain dry inside: one drowned miner, one living councilor. The dates are forty years apart.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h08_final_toll_diagram.txt",
                  "Final Toll Diagram\n\n"
                  "Four interventions surround the Bell: Break, Bind, Mute, or Crown. Each asks for a different sacrifice.\n");

    writeTextFile(campaignRoot / "README.md",
                  "# The Bell Beneath Blackwater\n\n"
                  "Clean-slate comprehensive Phase 3 campaign package. Includes five generated-portrait PCs, major NPCs, factions, maps, encounters, audio playlists, custom items, spells, shops, loot, handouts, story beats, session notes, public display state, filters, bookmarks, and table-ready timeline.\n\n"
                  "Primary smoke path: load campaign, open Campaign Home, launch `The Toll at The Silted Crown`, inspect player-safe launch text, start Rainy Town Mystery audio, open Blackwater Town Ward, run Old Boathouse Cutthroats, review handouts, then jump to Before the Eclipse.\n");

    for (const AudioSeed &seed : audioSeeds())
    {
        const std::filesystem::path source = root / seed.sourcePath;
        const std::filesystem::path target = audioTargetPath(root, seed);
        std::filesystem::create_directories(target.parent_path());
        if (std::filesystem::exists(source))
            std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing);
    }
}

ItemModel makeItem(const std::string &id,
                   const std::string &name,
                   const std::string &category,
                   const std::string &itemType,
                   ItemRarity rarity,
                   double weight,
                   int goldValue,
                   const std::string &description,
                   Tags tags)
{
    ItemModel item;
    setMetadata(item.metadata, id, tags);
    item.name = name;
    item.category = category;
    item.itemType = itemType;
    item.rarity = rarity;
    item.weight = weight;
    item.baseValue = goldValue * 100;
    item.description = description;
    item.rulesetKey = "dnd2024";
    return item;
}

InventoryEntryModel makeEntry(const std::string &id, const std::string &itemId, int quantity, bool equipped, const std::string &slot, const std::string &notes = "")
{
    InventoryEntryModel entry;
    setMetadata(entry.metadata, id);
    entry.itemId = itemId;
    entry.quantity = quantity;
    entry.equipped = equipped;
    entry.slot = slot;
    entry.stateNotes = notes;
    return entry;
}

std::map<std::string, int> attrs(int str, int dex, int con, int intel, int wis, int cha)
{
    return {{"Strength", str}, {"Dexterity", dex}, {"Constitution", con}, {"Intelligence", intel}, {"Wisdom", wis}, {"Charisma", cha}};
}

CharacterSpellEntryModel spellEntry(const std::string &spellId, const std::string &source = "Campaign", bool prepared = true, const std::string &notes = "")
{
    CharacterSpellEntryModel entry;
    entry.spellId = spellId;
    entry.source = source;
    entry.prepared = prepared;
    entry.notes = notes;
    return entry;
}

CharacterRelationshipModel relationship(const std::string &id,
                                        const std::string &targetType,
                                        const std::string &targetId,
                                        const std::string &label,
                                        const std::string &notes)
{
    CharacterRelationshipModel link;
    setMetadata(link.metadata, id);
    link.targetEntityType = targetType;
    link.targetEntityId = targetId;
    link.label = label;
    link.notes = notes;
    return link;
}

MonsterActionModel action(const std::string &name,
                          const std::string &kind,
                          int reach,
                          int range,
                          const std::string &attack,
                          const std::string &damage,
                          const std::string &notes)
{
    MonsterActionModel model;
    model.name = name;
    model.actionKind = kind;
    model.reachFeet = reach;
    model.normalRangeFeet = range;
    model.rangeFeet = range;
    model.attackBonus = attack;
    model.damage = damage;
    model.notes = notes;
    return model;
}

ResourceTrackerModel resource(const std::string &id, const std::string &name, int maximum, int used, const std::string &recharge, const std::string &notes)
{
    ResourceTrackerModel tracker;
    setMetadata(tracker.metadata, id);
    tracker.name = name;
    tracker.maximum = maximum;
    tracker.used = used;
    tracker.rechargeRule = recharge;
    tracker.notes = notes;
    return tracker;
}

CharacterModel makeCharacter(const std::string &id,
                             const std::string &name,
                             const std::string &race,
                             const std::string &klass,
                             const std::string &background,
                             int level,
                             const std::map<std::string, int> &attributes,
                             int currentHp,
                             int maxHp,
                             const std::string &portraitPath,
                             Tags tags)
{
    CharacterModel character;
    setMetadata(character.metadata, id, tags);
    std::snprintf(character.nameBuffer, sizeof(character.nameBuffer), "%s", name.c_str());
    character.selectedRace = race;
    character.selectedClass = klass;
    character.background = background;
    character.level = level;
    character.attributes = attributes;
    character.currentHitPoints = currentHp;
    character.maxHitPoints = maxHp;
    character.portraitAssetId = portraitPath;
    character.inventory.name = name + " Inventory";
    character.inventory.ownerEntityId = id;
    return character;
}

SpellModel makeSpell(const std::string &id,
                     const std::string &name,
                     int level,
                     SpellSchool school,
                     SpellResolutionType resolution,
                     const std::string &range,
                     const std::string &duration,
                     const std::string &effect,
                     const std::string &description,
                     Tags tags)
{
    SpellModel spell;
    setMetadata(spell.metadata, id, tags);
    spell.name = name;
    spell.level = level;
    spell.school = school;
    spell.resolution = resolution;
    spell.range = range;
    spell.duration = duration;
    spell.effectSummary = effect;
    spell.description = description;
    return spell;
}

NPCModel makeNpc(const std::string &id,
                 const std::string &name,
                 NPCCategory category,
                 const std::string &faction,
                 const std::string &role,
                 const std::string &description,
                 const std::string &attitude,
                 const std::string &secrets,
                 const std::string &goals,
                 int threat,
                 Tags tags)
{
    NPCModel npc;
    setMetadata(npc.metadata, id, tags);
    npc.name = name;
    npc.category = category;
    npc.faction = faction;
    npc.role = role;
    npc.description = description;
    npc.attitudeTowardParty = attitude;
    npc.secrets = secrets;
    npc.goals = goals;
    npc.threatLevel = threat;
    npc.attributes = attrs(10, 10, 10, 12, 12, 12);
    return npc;
}

FactionModel makeFaction(const std::string &id,
                         const std::string &name,
                         const std::string &type,
                         const std::string &summary,
                         const std::string &leader,
                         const std::string &goals,
                         int influence,
                         int threat,
                         Tags tags)
{
    FactionModel faction;
    setMetadata(faction.metadata, id, tags);
    faction.name = name;
    faction.type = type;
    faction.summary = summary;
    faction.leaderNpcId = leader;
    faction.goals = goals;
    faction.influence = influence;
    faction.threatLevel = threat;
    return faction;
}

MapCellModel styledCell(MapTileKind kind)
{
    MapCellModel cell;
    applyMapCellTerrainDefaults(cell, kind);
    return cell;
}

void setCell(std::vector<MapCellModel> &result, int width, int height, int x, int y, MapTileKind kind)
{
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;
    result[static_cast<std::size_t>(y * width + x)] = styledCell(kind);
}

void fillRect(std::vector<MapCellModel> &result, int width, int height, int x, int y, int rectWidth, int rectHeight, MapTileKind kind)
{
    for (int yy = y; yy < y + rectHeight; ++yy)
        for (int xx = x; xx < x + rectWidth; ++xx)
            setCell(result, width, height, xx, yy, kind);
}

void fillDisc(std::vector<MapCellModel> &result, int width, int height, int centerX, int centerY, int radius, MapTileKind kind)
{
    for (int yy = centerY - radius; yy <= centerY + radius; ++yy)
    {
        for (int xx = centerX - radius; xx <= centerX + radius; ++xx)
        {
            const int dx = xx - centerX;
            const int dy = yy - centerY;
            if (dx * dx + dy * dy <= radius * radius)
                setCell(result, width, height, xx, yy, kind);
        }
    }
}

void paintLine(std::vector<MapCellModel> &result, int width, int height, int x0, int y0, int x1, int y1, MapTileKind kind, int brush = 0)
{
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    while (true)
    {
        for (int oy = -brush; oy <= brush; ++oy)
            for (int ox = -brush; ox <= brush; ++ox)
                if (ox * ox + oy * oy <= brush * brush)
                    setCell(result, width, height, x0 + ox, y0 + oy, kind);

        if (x0 == x1 && y0 == y1)
            break;

        const int twiceError = 2 * error;
        if (twiceError >= dy)
        {
            error += dy;
            x0 += sx;
        }
        if (twiceError <= dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}

void outlineRect(std::vector<MapCellModel> &result, int width, int height, int x, int y, int rectWidth, int rectHeight, MapTileKind kind)
{
    for (int xx = x; xx < x + rectWidth; ++xx)
    {
        setCell(result, width, height, xx, y, kind);
        setCell(result, width, height, xx, y + rectHeight - 1, kind);
    }
    for (int yy = y; yy < y + rectHeight; ++yy)
    {
        setCell(result, width, height, x, yy, kind);
        setCell(result, width, height, x + rectWidth - 1, yy, kind);
    }
}

std::vector<MapCellModel> cells(int width, int height, MapScope scope, int variant)
{
    const MapTileKind baseKind = scope == MapScope::Dungeon ? MapTileKind::Wall : MapTileKind::Empty;
    std::vector<MapCellModel> result(static_cast<std::size_t>(width * height), styledCell(baseKind));

    switch (variant)
    {
    case 1: // The Silted Crown Inn
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Dirt);
        paintLine(result, width, height, 0, height - 2, width - 1, height - 2, MapTileKind::Road, 0);
        fillRect(result, width, height, 2, 2, 14, 8, MapTileKind::Room);
        outlineRect(result, width, height, 2, 2, 14, 8, MapTileKind::Wall);
        setCell(result, width, height, 8, 9, MapTileKind::Door);
        setCell(result, width, height, 9, 9, MapTileKind::Door);
        fillRect(result, width, height, 4, 5, 3, 2, MapTileKind::Cover);
        fillRect(result, width, height, 9, 4, 3, 2, MapTileKind::Cover);
        setCell(result, width, height, 3, 4, MapTileKind::Landmark);
        setCell(result, width, height, 14, 2, MapTileKind::Hazard);
        fillRect(result, width, height, 13, 0, 5, 2, MapTileKind::Water);
        break;

    case 2: // Blackwater Town Ward
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Empty);
        fillRect(result, width, height, 0, height - 7, width, 7, MapTileKind::Water);
        fillDisc(result, width, height, width - 2, height - 10, 10, MapTileKind::Water);
        fillRect(result, width, height, 5, 8, 29, 17, MapTileKind::Dirt);
        paintLine(result, width, height, 2, 16, 39, 16, MapTileKind::Road, 0);
        paintLine(result, width, height, 22, 7, 22, 29, MapTileKind::Road, 0);
        paintLine(result, width, height, 8, 24, 32, 10, MapTileKind::Road, 0);
        paintLine(result, width, height, 36, 22, 45, 28, MapTileKind::Bridge, 0);
        fillRect(result, width, height, 10, 8, 5, 4, MapTileKind::Building);
        fillRect(result, width, height, 25, 9, 6, 5, MapTileKind::Building);
        fillRect(result, width, height, 17, 14, 11, 5, MapTileKind::Settlement);
        setCell(result, width, height, 22, 16, MapTileKind::Landmark);
        setCell(result, width, height, 12, 10, MapTileKind::Landmark);
        setCell(result, width, height, 31, 11, MapTileKind::Landmark);
        fillRect(result, width, height, 34, 20, 5, 3, MapTileKind::Bridge);
        fillRect(result, width, height, 0, 0, 8, 6, MapTileKind::Forest);
        fillRect(result, width, height, 38, 3, 8, 6, MapTileKind::Forest);
        fillDisc(result, width, height, 9, 30, 5, MapTileKind::Swamp);
        fillDisc(result, width, height, 42, 25, 4, MapTileKind::Swamp);
        break;

    case 3: // Old Boathouse Ambush
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Swamp);
        fillRect(result, width, height, 0, 0, 7, height, MapTileKind::Water);
        fillRect(result, width, height, 22, 0, 8, height, MapTileKind::Water);
        fillRect(result, width, height, 8, 5, 14, 12, MapTileKind::Dirt);
        fillRect(result, width, height, 15, 4, 9, 8, MapTileKind::Building);
        paintLine(result, width, height, 5, 11, 24, 11, MapTileKind::Bridge, 0);
        paintLine(result, width, height, 10, 18, 23, 15, MapTileKind::Road, 0);
        fillRect(result, width, height, 12, 8, 5, 3, MapTileKind::Cover);
        setCell(result, width, height, 15, 9, MapTileKind::Hazard);
        setCell(result, width, height, 22, 15, MapTileKind::Landmark);
        setCell(result, width, height, 7, 18, MapTileKind::Cover);
        break;

    case 4: // Reedfen Marsh Causeway
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Swamp);
        fillDisc(result, width, height, 12, 24, 6, MapTileKind::Water);
        fillDisc(result, width, height, 23, 18, 5, MapTileKind::Water);
        fillDisc(result, width, height, 35, 10, 6, MapTileKind::Water);
        paintLine(result, width, height, 0, 19, 15, 18, MapTileKind::Road, 0);
        paintLine(result, width, height, 15, 18, 29, 14, MapTileKind::Bridge, 0);
        paintLine(result, width, height, 29, 14, width - 1, 12, MapTileKind::Road, 0);
        setCell(result, width, height, 18, 21, MapTileKind::Pit);
        fillRect(result, width, height, 30, 6, 5, 5, MapTileKind::Stone);
        setCell(result, width, height, 32, 8, MapTileKind::Landmark);
        setCell(result, width, height, 11, 25, MapTileKind::Landmark);
        for (int x = 2; x < width; x += 7)
            setCell(result, width, height, x, 4 + (x % 5), MapTileKind::Forest);
        break;

    case 5: // Abandoned Bellfoundry Cellars
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Wall);
        fillRect(result, width, height, 3, 3, 14, 9, MapTileKind::Room);
        fillRect(result, width, height, 18, 4, 14, 10, MapTileKind::Room);
        fillRect(result, width, height, 8, 15, 24, 9, MapTileKind::Room);
        paintLine(result, width, height, 15, 8, 20, 8, MapTileKind::Road, 0);
        paintLine(result, width, height, 14, 12, 14, 18, MapTileKind::Road, 0);
        fillRect(result, width, height, 4, 5, 5, 4, MapTileKind::Lava);
        fillRect(result, width, height, 10, 5, 4, 3, MapTileKind::Hazard);
        paintLine(result, width, height, 2, 20, 34, 20, MapTileKind::Water, 0);
        fillRect(result, width, height, 18, 16, 5, 3, MapTileKind::Cover);
        setCell(result, width, height, 24, 12, MapTileKind::Door);
        setCell(result, width, height, 14, 7, MapTileKind::Rubble);
        setCell(result, width, height, 30, 20, MapTileKind::Landmark);
        break;

    case 6: // Lake Chapel Ruins
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Water);
        fillDisc(result, width, height, 21, 17, 14, MapTileKind::Empty);
        fillDisc(result, width, height, 21, 17, 10, MapTileKind::Stone);
        fillRect(result, width, height, 12, 6, 18, 16, MapTileKind::Room);
        outlineRect(result, width, height, 12, 6, 18, 16, MapTileKind::Wall);
        fillRect(result, width, height, 14, 7, 7, 3, MapTileKind::Rubble);
        setCell(result, width, height, 21, 12, MapTileKind::Landmark);
        setCell(result, width, height, 21, 13, MapTileKind::Hazard);
        setCell(result, width, height, 28, 24, MapTileKind::Stairs);
        setCell(result, width, height, 13, 6, MapTileKind::Landmark);
        paintLine(result, width, height, 2, 30, 15, 24, MapTileKind::Bridge, 0);
        break;

    case 7: // Mnemosyne Bell Chamber
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Chasm);
        fillDisc(result, width, height, 18, 14, 11, MapTileKind::Water);
        fillRect(result, width, height, 12, 8, 13, 12, MapTileKind::Room);
        outlineRect(result, width, height, 12, 8, 13, 12, MapTileKind::Wall);
        paintLine(result, width, height, 18, 14, 8, 14, MapTileKind::Bridge, 0);
        paintLine(result, width, height, 18, 14, 14, 8, MapTileKind::Bridge, 0);
        paintLine(result, width, height, 18, 14, 22, 8, MapTileKind::Bridge, 0);
        paintLine(result, width, height, 18, 14, 28, 14, MapTileKind::Bridge, 0);
        setCell(result, width, height, 18, 14, MapTileKind::Landmark);
        setCell(result, width, height, 8, 14, MapTileKind::Landmark);
        setCell(result, width, height, 14, 8, MapTileKind::Landmark);
        setCell(result, width, height, 22, 8, MapTileKind::Landmark);
        setCell(result, width, height, 28, 14, MapTileKind::Landmark);
        break;

    default:
        fillRect(result, width, height, 0, 0, width, height, scope == MapScope::Dungeon ? MapTileKind::Room : MapTileKind::Empty);
        break;
    }

    if (scope == MapScope::Dungeon)
    {
        for (int x = 0; x < width; ++x)
        {
            setCell(result, width, height, x, 0, MapTileKind::Wall);
            setCell(result, width, height, x, height - 1, MapTileKind::Wall);
        }
        for (int y = 0; y < height; ++y)
        {
            setCell(result, width, height, 0, y, MapTileKind::Wall);
            setCell(result, width, height, width - 1, y, MapTileKind::Wall);
        }
    }

    return result;
}

MapFeatureModel mapFeature(const std::string &id,
                           MapFeatureKind kind,
                           const std::string &name,
                           const std::string &description,
                           const std::string &gmNotes,
                           int x,
                           int y,
                           bool discovered,
                           bool gmOnly,
                           int dc = 0)
{
    MapFeatureModel feature;
    setMetadata(feature.metadata, id);
    feature.kind = kind;
    feature.name = name;
    feature.description = description;
    feature.gmNotes = gmNotes;
    feature.x = x;
    feature.y = y;
    feature.discovered = discovered;
    feature.gmOnly = gmOnly;
    feature.difficultyClass = dc;
    return feature;
}

MapRegionModel region(const std::string &id,
                      const std::string &name,
                      const std::string &description,
                      int x,
                      int y,
                      int width,
                      int height,
                      bool difficult,
                      bool gmOnly = false)
{
    MapRegionModel mapRegion;
    setMetadata(mapRegion.metadata, id);
    mapRegion.name = name;
    mapRegion.description = description;
    mapRegion.x = x;
    mapRegion.y = y;
    mapRegion.width = width;
    mapRegion.height = height;
    mapRegion.difficultTerrain = difficult;
    mapRegion.gmOnly = gmOnly;
    return mapRegion;
}

MapTokenModel token(const std::string &id, const std::string &name, MapTokenKind kind, const std::string &type, const std::string &targetId, float x, float y, bool hidden = false)
{
    MapTokenModel mapToken;
    setMetadata(mapToken.metadata, id);
    mapToken.name = name;
    mapToken.kind = kind;
    mapToken.linkedEntityType = type;
    mapToken.linkedEntityId = targetId;
    mapToken.x = x;
    mapToken.y = y;
    mapToken.hiddenFromPlayers = hidden;
    return mapToken;
}

MapModel makeMap(const std::string &id,
                 const std::string &name,
                 const std::string &description,
                 MapScope scope,
                 int width,
                 int height,
                 const std::string &background,
                 bool mainMap,
                 Tags tags,
                 int variant)
{
    MapModel map;
    setMetadata(map.metadata, id, tags);
    map.name = name;
    map.description = description;
    map.scope = scope;
    map.gridType = GridType::Square;
    map.width = width;
    map.height = height;
    map.cellSize = 5.0f;
    map.isMainMap = mainMap;
    map.backgroundImagePath = background;
    map.backgroundImageOpacity = 0.72f;
    map.cells = cells(width, height, scope, variant);
    MapLayerModel base;
    setMetadata(base.metadata, id + "_layer_base", {"base"});
    base.name = "Base";
    base.visible = true;
    base.sortOrder = 0;
    map.layers.push_back(base);
    MapLayerModel gm;
    setMetadata(gm.metadata, id + "_layer_gm", {"gm", "hidden"});
    gm.name = "GM Notes";
    gm.visible = true;
    gm.sortOrder = 20;
    map.layers.push_back(gm);
    return map;
}

MonsterDefinitionModel makeMonster(const std::string &id,
                                   const std::string &name,
                                   const std::string &cr,
                                   int ac,
                                   int hp,
                                   const std::string &traits,
                                   Tags tags)
{
    MonsterDefinitionModel monster;
    setMetadata(monster.metadata, id, tags);
    monster.name = name;
    monster.source = kCampaignName;
    monster.size = "Medium";
    monster.type = "Spirit";
    monster.challengeRating = cr;
    monster.armorClass = ac;
    monster.hitPoints = hp;
    monster.speedFeet = 30;
    monster.traits = traits;
    monster.attributes = attrs(12, 14, 12, 10, 14, 12);
    return monster;
}

EncounterParticipantModel participant(const std::string &id,
                                      CombatantKind kind,
                                      const std::string &sourceId,
                                      const std::string &monsterId,
                                      const std::string &name,
                                      int initiative,
                                      int hp,
                                      int ac,
                                      bool friendly,
                                      int x,
                                      int y,
                                      Tags tags)
{
    EncounterParticipantModel model;
    setMetadata(model.metadata, id, tags);
    model.kind = kind;
    model.sourceEntityId = sourceId;
    model.monsterDefinitionId = monsterId;
    model.displayName = name;
    model.initiative = initiative;
    model.currentHitPoints = hp;
    model.maxHitPoints = hp;
    model.armorClass = ac;
    model.speedFeet = 30;
    model.friendly = friendly;
    model.startingX = x;
    model.startingY = y;
    model.x = x;
    model.y = y;
    model.turnStartX = x;
    model.turnStartY = y;
    model.groupName = friendly ? "Party" : "Opposition";
    return model;
}

DiceRollPresetModel dicePreset(const std::string &id, const std::string &name, const std::string &expression, const std::string &notes)
{
    DiceRollPresetModel preset;
    setMetadata(preset.metadata, id);
    preset.name = name;
    preset.expression = expression;
    preset.notes = notes;
    return preset;
}

CombatLogEntryModel combatLog(const std::string &id, int round, const std::string &actor, const std::string &event, const std::string &summary)
{
    CombatLogEntryModel entry;
    setMetadata(entry.metadata, id);
    entry.round = round;
    entry.actorName = actor;
    entry.eventType = event;
    entry.summary = summary;
    return entry;
}

EncounterModel makeEncounter(const std::string &id,
                             const std::string &name,
                             const std::string &description,
                             const std::string &mapId,
                             int width,
                             int height,
                             Tags tags,
                             int variant)
{
    EncounterModel encounter;
    setMetadata(encounter.metadata, id, tags);
    encounter.name = name;
    encounter.description = description;
    encounter.mapId = mapId;
    encounter.mapContextName = name;
    encounter.battlefieldWidth = width;
    encounter.battlefieldHeight = height;
    encounter.cellSizeFeet = 5;
    encounter.useMapCells = true;
    encounter.battlefieldCells = cells(width, height, MapScope::Local, variant);
    encounter.round = 1;
    return encounter;
}

StoryLinkModel storyLink(const std::string &id, StoryLinkType type, const std::string &targetId, const std::string &targetType, const std::string &label)
{
    StoryLinkModel link;
    setMetadata(link.metadata, id);
    link.type = type;
    link.targetEntityId = targetId;
    link.targetEntityType = targetType;
    link.label = label;
    return link;
}

StoryBeatModel beat(const std::string &id,
                    const std::string &title,
                    const std::string &summary,
                    const std::string &playerSummary,
                    const std::string &readAloud,
                    const std::string &clues,
                    const std::string &consequences,
                    const std::string &gmNotes,
                    StoryStatus status,
                    const std::string &date,
                    const std::string &audioId,
                    Tags tags,
                    std::vector<StoryLinkModel> links = {})
{
    StoryBeatModel model;
    setMetadata(model.metadata, id, tags);
    model.title = title;
    model.summary = summary;
    model.playerSummary = playerSummary;
    model.readAloudText = readAloud;
    model.clues = clues;
    model.consequences = consequences;
    model.gmNotes = gmNotes;
    model.status = status;
    model.inWorldDate = date;
    model.cueAudioAssetId = audioId;
    model.cueVolume = audioId.empty() ? 0.0f : 0.65f;
    model.cueLoop = audioId.find("ambience") != std::string::npos || audioId.find("rain") != std::string::npos;
    model.links = std::move(links);
    return model;
}

HandoutModel handout(const std::string &id, const std::string &title, const std::string &body, const std::string &filePath, const std::string &gmNotes, Tags tags)
{
    HandoutModel model;
    setMetadata(model.metadata, id, tags);
    model.title = title;
    model.body = body;
    model.filePath = filePath;
    model.gmNotes = gmNotes;
    return model;
}

ShopModel shop(const std::string &id, const std::string &name, const std::string &merchant, const std::string &description, Tags tags)
{
    ShopModel model;
    setMetadata(model.metadata, id, tags);
    model.name = name;
    model.merchantName = merchant;
    model.description = description;
    model.factionReputationNotes = "Helpful investigators receive practical discounts; exposed corruption changes availability.";
    return model;
}

ShopStockEntryModel stock(const std::string &itemId, int quantity, double price, const std::string &availability, bool restocks)
{
    ShopStockEntryModel entry;
    entry.itemId = itemId;
    entry.quantity = quantity;
    entry.priceMultiplier = price;
    entry.sellPriceMultiplier = 0.5;
    entry.availability = availability;
    entry.restocks = restocks;
    entry.haggleNotes = "Use reputation and faction pressure to adjust this row during QA.";
    return entry;
}

LootParcelModel loot(const std::string &id, const std::string &name, const std::string &source, const std::string &notes, Tags tags)
{
    LootParcelModel model;
    setMetadata(model.metadata, id, tags);
    model.name = name;
    model.source = source;
    model.notes = notes;
    return model;
}

LootParcelItemModel lootItem(const std::string &itemId, int quantity, const std::string &notes)
{
    LootParcelItemModel item;
    item.itemId = itemId;
    item.quantity = quantity;
    item.notes = notes;
    return item;
}

DungeonRoomConnectionModel roomConnection(const std::string &id, const std::string &targetId, const std::string &label, bool locked = false, bool secret = false, bool discovered = true)
{
    DungeonRoomConnectionModel connection;
    setMetadata(connection.metadata, id);
    connection.targetRoomId = targetId;
    connection.label = label;
    connection.locked = locked;
    connection.secret = secret;
    connection.discovered = discovered;
    return connection;
}

DungeonRoomModel room(const std::string &id,
                      const std::string &name,
                      const std::string &description,
                      const std::string &readAloud,
                      const std::string &secrets,
                      int x,
                      int y,
                      DungeonRoomExplorationState state,
                      Tags tags)
{
    DungeonRoomModel model;
    setMetadata(model.metadata, id, tags);
    model.name = name;
    model.description = description;
    model.keyedDescription = description;
    model.readAloudText = readAloud;
    model.secrets = secrets;
    model.mapX = x;
    model.mapY = y;
    model.explorationState = state;
    return model;
}

std::string sqlQuote(const std::string &value)
{
    std::string result = "'";
    for (const char c : value)
        result += c == '\'' ? "''" : std::string(1, c);
    result += "'";
    return result;
}

void execSql(sqlite3 *db, const std::string &sql)
{
    char *error = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK)
    {
        const std::string message = error ? error : "unknown sqlite error";
        sqlite3_free(error);
        throw std::runtime_error(message + "\nSQL:\n" + sql);
    }
}

long long campaignRowId(sqlite3 *db)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT id FROM campaigns WHERE stable_id = ? LIMIT 1;", -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("Could not prepare campaign row lookup.");
    sqlite3_bind_text(stmt, 1, kCampaignId, -1, SQLITE_TRANSIENT);
    long long result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    if (result < 0)
        throw std::runtime_error("Seeded campaign row not found.");
    return result;
}

void insertAssetMetadata(sqlite3 *db,
                         long long campaignId,
                         const std::string &id,
                         const std::string &type,
                         const std::string &name,
                         const std::filesystem::path &root,
                         const std::filesystem::path &path,
                         const std::string &mime,
                         const std::string &tags)
{
    const bool exists = std::filesystem::exists(path);
    const long long size = exists ? static_cast<long long>(std::filesystem::file_size(path)) : 0LL;
    execSql(db,
            "INSERT OR REPLACE INTO asset_metadata "
            "(stable_id, campaign_id, asset_type, display_name, source_path, project_path, content_hash, mime_type, byte_size, missing, tags, created_at, updated_at) VALUES (" +
                sqlQuote(id) + "," + std::to_string(campaignId) + "," + sqlQuote(type) + "," + sqlQuote(name) + ",''," +
                sqlQuote(projectPath(root, path)) + "," + sqlQuote("blackwater-seed-" + id) + "," + sqlQuote(mime) + "," +
                std::to_string(size) + "," + (exists ? "0" : "1") + "," + sqlQuote(tags) + ",datetime('now'),datetime('now'));");
}

void insertStoryDocument(sqlite3 *db, long long campaignId, const std::string &id, const std::string &title, const std::string &body, const std::string &tags)
{
    execSql(db,
            "INSERT OR REPLACE INTO story_documents "
            "(stable_id, campaign_id, title, body, tags, created_at, updated_at) VALUES (" +
                sqlQuote(id) + "," + std::to_string(campaignId) + "," + sqlQuote(title) + "," + sqlQuote(body) + "," +
                sqlQuote(tags) + ",datetime('now'),datetime('now'));");
}

void addPostSaveMetadata(const std::filesystem::path &root)
{
    sqlite3 *db = nullptr;
    const std::filesystem::path dbPath = root / "TTRPG.db";
    if (sqlite3_open(dbPath.string().c_str(), &db) != SQLITE_OK)
        throw std::runtime_error("Could not reopen seeded database: " + dbPath.string());

    try
    {
        const long long rowId = campaignRowId(db);
        execSql(db, "BEGIN TRANSACTION;");
        const std::filesystem::path campaignRoot = packageRoot(root);
        for (const std::string &portrait : blackwaterPortraitFiles())
            insertAssetMetadata(db, rowId, "asset_" + portrait.substr(0, portrait.find('.')), "portrait", portrait, root, campaignRoot / "Assets" / "Portraits" / portrait, "image/png", "portrait;player;");
        for (const std::string &map : {"silted_crown_common_room.bmp", "blackwater_town_ward.bmp", "old_boathouse_ambush.bmp", "reedfen_marsh_causeway.bmp", "abandoned_bellfoundry_cellars.bmp", "lake_chapel_ruins.bmp", "mnemosyne_bell_chamber.bmp"})
            insertAssetMetadata(db, rowId, "asset_" + map.substr(0, map.find('.')), "map", map, root, campaignRoot / "Assets" / "Maps" / map, "image/bmp", "map;background;");
        for (const AudioSeed &seed : audioSeeds())
            insertAssetMetadata(db, rowId, "asset_" + seed.id, "audio", seed.name, root, audioTargetPath(root, seed), seed.sourcePath.find(".mp3") != std::string::npos ? "audio/mpeg" : "audio/wav", "audio;seeded;");
        for (int i = 1; i <= 8; ++i)
        {
            std::ostringstream id;
            id << "asset_h0" << i;
            std::ostringstream file;
            file << "h0" << i << "_";
            const std::filesystem::path handoutDir = campaignRoot / "Assets" / "Handouts";
            for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(handoutDir))
            {
                if (entry.path().filename().string().rfind(file.str(), 0) == 0)
                    insertAssetMetadata(db, rowId, id.str(), "handout", entry.path().stem().string(), root, entry.path(), "text/plain", "handout;player;");
            }
        }
        insertAssetMetadata(db, rowId, "asset_campaign_readme", "file", "Campaign README", root, campaignRoot / "README.md", "text/markdown", "overview;qa;");

        insertStoryDocument(db, rowId, "doc_blackwater_runbook", "GM Runbook - The Bell Beneath Blackwater",
                            "Open at 2:12 AM in The Silted Crown. Launch Rainy Town Mystery audio, reveal H01, then move from witness questions to the lake footprints. The campaign supports social, investigation, dungeon, combat, and finale routes. Keep the Bell choices visible: Break, Bind, Mute, Crown.",
                            "gm;runbook;phase3;");
        insertStoryDocument(db, rowId, "doc_blackwater_clue_chain", "Clue Chain and Fail-Forward",
                            "Primary clues: the 2:13 toll, old miner footprints, council flood records, Ash Choir acrostic, Lantern Company invoice, and final toll diagram. Missed clues return through hauntings, public rites, dropped evidence, or memory-echo NPCs.",
                            "gm;clues;mystery;");
        insertStoryDocument(db, rowId, "doc_blackwater_finale_matrix", "Finale Matrix",
                            "Break destroys the archive and saves the town. Bind preserves knowledge through an anchor. Mute delays catastrophe but leaves fragments. Crown accepts the Regent's bargain and changes the world.",
                            "gm;finale;branches;");
        execSql(db, "COMMIT;");
    }
    catch (...)
    {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        throw;
    }
    sqlite3_close(db);
}

std::vector<ItemModel> buildItems()
{
    std::vector<ItemModel> items;
    auto add = [&](ItemModel item) {
        items.push_back(std::move(item));
        return items.back().metadata.id;
    };

    ItemModel longsword = makeItem("item_star_notched_longsword", "Star-notched Longsword", "Weapon", "Longsword", ItemRarity::Uncommon, 3.0, 120, "Officer's blade with a pale metal chip in the pommel.", {"weapon", "pc", "mira"});
    longsword.weaponDamage = "1d8 slashing";
    longsword.weaponAbility = "Strength";
    longsword.weaponProperties = {"versatile"};
    add(longsword);
    ItemModel shield = makeItem("item_shield_cracked_sun", "Cracked Sun Shield", "Armor", "Shield", ItemRarity::Common, 6.0, 15, "A battered shield bearing Veyr's cracked sun.", {"armor", "shield"});
    shield.armorClassBonus = 2;
    add(shield);
    ItemModel chain = makeItem("item_chain_mail_hauberk", "Chain Mail Hauberk", "Armor", "Chain Mail", ItemRarity::Common, 55.0, 75, "Practical military chain.", {"armor", "mira"});
    chain.armorClassBonus = 6;
    add(chain);

    ItemModel hammer = makeItem("item_warhammer_bellowsong", "Bellowsong Warhammer", "Weapon", "Warhammer", ItemRarity::Uncommon, 2.0, 85, "Runed forge hammer that rings softly near false memories.", {"weapon", "kael", "bell"});
    hammer.weaponDamage = "1d8 bludgeoning";
    hammer.weaponAbility = "Strength";
    add(hammer);
    ItemModel forgeShield = makeItem("item_steel_shield_forge_prayers", "Steel Shield of Forge Prayers", "Armor", "Shield", ItemRarity::Common, 6.0, 20, "Shield etched with heat-darkened prayers.", {"armor", "kael"});
    forgeShield.armorClassBonus = 2;
    add(forgeShield);
    ItemModel scale = makeItem("item_scale_mail_brass_rivets", "Brass-riveted Scale Mail", "Armor", "Scale Mail", ItemRarity::Common, 45.0, 50, "Scale mail with a forge-clergy pattern.", {"armor", "kael"});
    scale.armorClassBonus = 4;
    add(scale);

    ItemModel daggers = makeItem("item_paired_daggers", "Paired Daggers", "Weapon", "Dagger", ItemRarity::Common, 2.0, 4, "Twin blades hidden in a reporter's coat.", {"weapon", "tamsin"});
    daggers.weaponDamage = "1d4 piercing";
    daggers.weaponAbility = "Dexterity";
    daggers.weaponProperties = {"finesse", "light", "thrown"};
    add(daggers);
    ItemModel shortbow = makeItem("item_shortbow_reporter", "Reporter Shortbow", "Weapon", "Shortbow", ItemRarity::Common, 2.0, 25, "A compact shortbow with a wrapped grip.", {"weapon", "ranged"});
    shortbow.weaponDamage = "1d6 piercing";
    shortbow.weaponAbility = "Dexterity";
    shortbow.weaponRangeFeet = 80;
    shortbow.weaponLongRangeFeet = 320;
    add(shortbow);
    ItemModel leather = makeItem("item_leather_hidden_pockets", "Leather Armor with Hidden Pockets", "Armor", "Leather", ItemRarity::Common, 10.0, 20, "Leather armor tailored for quick hands and concealed notes.", {"armor", "tamsin"});
    leather.armorClassBonus = 1;
    add(leather);

    ItemModel staff = makeItem("item_quarterstaff_silver_rings", "Quarterstaff with Silver Rings", "Weapon", "Quarterstaff", ItemRarity::Common, 4.0, 3, "Scholar's staff that clicks like an astrolabe.", {"weapon", "elyon"});
    staff.weaponDamage = "1d6 bludgeoning";
    add(staff);
    add(makeItem("item_spellbook_blue_meridian", "Spellbook: The Blue Meridian", "Magic Focus", "Book", ItemRarity::Rare, 3.0, 300, "A spellbook lined with moving star charts.", {"spellbook", "elyon"}));
    add(makeItem("item_cracked_crystal_astrolabe", "Cracked Crystal Astrolabe", "Magic Focus", "Focus", ItemRarity::Rare, 1.0, 250, "An arcane focus that anticipates the Bell's resonance.", {"focus", "prophecy", "elyon"}));

    ItemModel longbow = makeItem("item_longbow_pale_yew", "Longbow of Pale Yew", "Weapon", "Longbow", ItemRarity::Uncommon, 2.0, 90, "A pale yew bow with a moonvine grip.", {"weapon", "nyra", "fey"});
    longbow.weaponDamage = "1d8 piercing";
    longbow.weaponAbility = "Dexterity";
    longbow.weaponRangeFeet = 150;
    longbow.weaponLongRangeFeet = 600;
    add(longbow);
    ItemModel studded = makeItem("item_studded_leather_leaf", "Leaf-embossed Studded Leather", "Armor", "Studded Leather", ItemRarity::Common, 13.0, 45, "Quiet armor with fey leaf embossing.", {"armor", "nyra"});
    studded.armorClassBonus = 2;
    add(studded);
    add(makeItem("item_moonvine_charm", "Moonvine Charm", "Magic Focus", "Charm", ItemRarity::Uncommon, 0.1, 70, "Braided moonvine and antler focus.", {"focus", "fey"}));

    add(makeItem("item_field_command_map_case", "Field Command Map Case", "Tool", "Map Case", ItemRarity::Common, 1.0, 10, "Weatherproof case for battle maps.", {"tool", "travel"}));
    add(makeItem("item_signal_whistle", "Signal Whistle", "Tool", "Whistle", ItemRarity::Common, 0.1, 1, "Carries above rain and panic.", {"tool", "signal"}));
    ItemModel healing = makeItem("item_healing_draught", "Healing Draught", "Consumable", "Potion", ItemRarity::Common, 0.5, 50, "Restore health manually after drinking.", {"consumable", "healing"});
    healing.stackable = true;
    healing.consumable = true;
    add(healing);
    ItemModel marsh = makeItem("item_marsh_breath_potion", "Potion of Marsh-Breath", "Consumable", "Potion", ItemRarity::Uncommon, 0.5, 60, "For 1 hour, ignore nonmagical marsh fumes and breathe waterlogged air.", {"consumable", "marsh"});
    marsh.stackable = true;
    marsh.consumable = true;
    add(marsh);
    add(makeItem("item_smith_tools", "Smith's Tools", "Tool", "Tools", ItemRarity::Common, 8.0, 20, "Forge tools for repairing wards and gear.", {"tool", "craft"}));
    add(makeItem("item_thieves_tools", "Thieves' Tools", "Tool", "Tools", ItemRarity::Common, 1.0, 25, "Lockwork picks and probes.", {"tool", "rogue"}));
    add(makeItem("item_reporter_satchel", "Reporter's Satchel", "Container", "Satchel", ItemRarity::Common, 2.0, 5, "Ink, paper, false credentials, and careful pockets.", {"container", "tamsin"}));
    add(makeItem("item_forgery_kit", "Forgery Kit", "Tool", "Kit", ItemRarity::Common, 5.0, 15, "For urgent documents and bad ideas.", {"tool", "social"}));
    add(makeItem("item_silvered_lockpick", "Silvered Lockpick", "Tool", "Lockpick", ItemRarity::Uncommon, 0.1, 35, "A lockpick that glows near memory-warded doors.", {"tool", "silvered"}));
    add(makeItem("item_scroll_dispel_magic", "Scroll of Dispel Magic", "Scroll", "Scroll", ItemRarity::Uncommon, 0.1, 150, "One-use spell scroll.", {"scroll", "magic"}));
    add(makeItem("item_explorer_pack", "Explorer's Pack", "Container", "Pack", ItemRarity::Common, 10.0, 10, "Rope, rations, torches, and field supplies.", {"container", "travel"}));
    add(makeItem("item_starfall_shard_moss", "Starfall Shard Wrapped in Moss", "Story Item", "Relic", ItemRarity::Rare, 0.2, 0, "Cold shard that points toward impossible histories.", {"plot", "memory", "fey"}));

    add(makeItem("item_blue_glass_lantern", "Blue-Glass Lantern", "Tool", "Lantern", ItemRarity::Uncommon, 2.0, 45, "Hidden crescent marks glow within ten feet.", {"light", "investigation", "occult"}));
    ItemModel knife = makeItem("item_silvered_canal_knife", "Silvered Canal Knife", "Weapon", "Dagger", ItemRarity::Common, 1.0, 35, "A practical knife with a silvered edge and tar-stained grip.", {"weapon", "silvered", "clue"});
    knife.weaponDamage = "1d4 piercing";
    knife.weaponAbility = "Dexterity";
    add(knife);
    add(makeItem("item_moon_glass_shard", "Moon-Glass Shard", "Magic Focus", "Focus", ItemRarity::Rare, 0.2, 180, "Reveals invisible writing or magically hidden blood traces once per day.", {"magic", "ritual", "clue"}));
    add(makeItem("item_watch_writ", "Temporary Watch Writ", "Document", "Writ", ItemRarity::Custom, 0.0, 0, "Limited authority to investigate Blackwater crime scenes.", {"authority", "social"}));
    add(makeItem("item_acolyte_charm", "Ashbound Acolyte Charm", "Trinket", "Charm", ItemRarity::Uncommon, 0.1, 50, "Cracks to help resist fear or charm from bell, ash, or shade magic.", {"cult", "ward"}));
    add(makeItem("item_foundry_key", "Black Iron Foundry Key", "Key", "Key", ItemRarity::Custom, 0.1, 0, "Opens the cracked kiln door and several cellar gates.", {"dungeon", "access"}));
    add(makeItem("item_coded_ledger_scrap", "Coded Ledger Scrap", "Clue", "Ledger", ItemRarity::Custom, 0.0, 0, "Lists payments under bell-themed aliases.", {"handout", "smuggling", "mystery"}));
    add(makeItem("item_bell_clapper_token", "Bell-Clapper Token", "Currency", "Token", ItemRarity::Common, 0.1, 10, "Recognized at the Blackvein Night Market.", {"market", "social"}));
    add(makeItem("item_saint_orwyn_medal", "Medal of Saint Orwyn", "Wondrous", "Amulet", ItemRarity::Uncommon, 0.1, 90, "Add 1d4 to one save against necrotic or possession effects once per day.", {"divine", "protection"}));
    ItemModel chalk = makeItem("item_miststep_chalk", "Miststep Chalk", "Consumable", "Chalk", ItemRarity::Uncommon, 0.1, 75, "Draw a short passage on fogged glass or wet stone.", {"ritual", "escape"});
    chalk.stackable = true;
    chalk.consumable = true;
    add(chalk);
    add(makeItem("item_moon_glass_reliquary", "Moon-Glass Reliquary", "Major Reward", "Reliquary", ItemRarity::Rare, 1.5, 400, "Can contain a shade, spirit echo, or cursed memory after a completed ritual.", {"finale", "magic", "plot"}));
    add(makeItem("item_miner_oath_token", "Miner Oath Token", "Story Item", "Token", ItemRarity::Custom, 0.1, 0, "A drowned miner's oath scratched into black iron.", {"ghost", "remnant", "truth"}));
    add(makeItem("item_rusk_cane_fragment", "Rusk's Bell Fragment Cane", "Relic", "Cane", ItemRarity::VeryRare, 2.0, 600, "Protects its holder from one minor memory effect, then whispers a false name.", {"company", "bell", "relic"}));
    return items;
}

std::vector<SpellModel> buildSpells()
{
    return {
        makeSpell("spell_bless", "Bless", 1, SpellSchool::Enchantment, SpellResolutionType::Utility, "30 ft", "1 minute", "Bolster allies against fear and confusion.", "Prepared for town defenses and hard choices.", {"divine", "support"}),
        makeSpell("spell_cure_wounds", "Cure Wounds", 1, SpellSchool::Abjuration, SpellResolutionType::Healing, "Touch", "Instantaneous", "Restore health manually.", "Standard healing entry for sheet and live-slot checks.", {"healing"}),
        makeSpell("spell_detect_magic", "Detect Magic", 1, SpellSchool::Divination, SpellResolutionType::Utility, "Self", "10 minutes", "Sense Bell residue and hidden wards.", "Useful in nearly every investigation scene.", {"ritual", "investigation"}),
        makeSpell("spell_guiding_bolt", "Guiding Bolt", 1, SpellSchool::Evocation, SpellResolutionType::SpellAttack, "120 ft", "1 round", "Radiant attack that marks a target.", "Good combat spell against shades.", {"combat", "radiant"}),
        makeSpell("spell_healing_word", "Healing Word", 1, SpellSchool::Abjuration, SpellResolutionType::Healing, "60 ft", "Instantaneous", "Restore a downed ally at range.", "Fast support spell.", {"healing", "support"}),
        makeSpell("spell_spiritual_weapon", "Spiritual Weapon", 2, SpellSchool::Evocation, SpellResolutionType::SpellAttack, "60 ft", "1 minute", "Summon a ringing forge hammer.", "Kael's signature combat summon.", {"combat", "divine"}),
        makeSpell("spell_heat_metal", "Heat Metal", 2, SpellSchool::Transmutation, SpellResolutionType::SavingThrow, "60 ft", "1 minute", "Punish armored foes and ritual tools.", "Strong against foundry enemies.", {"combat", "fire"}),
        makeSpell("spell_revivify", "Revivify", 3, SpellSchool::Necromancy, SpellResolutionType::Healing, "Touch", "Instantaneous", "Return the recently dead.", "High-stakes finale support.", {"healing", "rare"}),
        makeSpell("spell_spirit_guardians", "Spirit Guardians", 3, SpellSchool::Conjuration, SpellResolutionType::SavingThrow, "Self", "10 minutes", "Ancestral forge spirits guard an area.", "Tests concentration and aura text.", {"combat", "concentration"}),
        makeSpell("spell_shield", "Shield", 1, SpellSchool::Abjuration, SpellResolutionType::Utility, "Self", "1 round", "Reactive protection.", "Elyon's emergency defense.", {"defense"}),
        makeSpell("spell_magic_missile", "Magic Missile", 1, SpellSchool::Evocation, SpellResolutionType::SpellAttack, "120 ft", "Instantaneous", "Reliable force darts.", "Simple spell attack entry.", {"combat", "force"}),
        makeSpell("spell_find_familiar", "Find Familiar", 1, SpellSchool::Conjuration, SpellResolutionType::Utility, "10 ft", "Instantaneous", "Summon a small scouting spirit.", "Exploration helper.", {"ritual", "utility"}),
        makeSpell("spell_misty_step", "Misty Step", 2, SpellSchool::Conjuration, SpellResolutionType::Utility, "Self", "Instantaneous", "Teleport through lake mist.", "Mobility in flooded spaces.", {"mobility"}),
        makeSpell("spell_counterspell", "Counterspell", 3, SpellSchool::Abjuration, SpellResolutionType::Utility, "60 ft", "Instantaneous", "Interrupt hostile casting.", "Finale ritual counterplay.", {"reaction", "defense"}),
        makeSpell("spell_fireball", "Fireball", 3, SpellSchool::Evocation, SpellResolutionType::SavingThrow, "150 ft", "Instantaneous", "Explosive fire in a wide area.", "Useful but risky near townsfolk and gas.", {"combat", "fire"}),
        makeSpell("spell_hunters_mark", "Hunter's Mark", 1, SpellSchool::Divination, SpellResolutionType::Utility, "90 ft", "1 hour", "Mark a quarry's trail.", "Nyra's tracking signature.", {"tracking", "concentration"}),
        makeSpell("spell_goodberry", "Goodberry", 1, SpellSchool::Transmutation, SpellResolutionType::Healing, "Touch", "24 hours", "Create restorative berries.", "Safe long-route sustain.", {"healing", "wilderness"}),
        makeSpell("spell_pass_without_trace", "Pass without Trace", 2, SpellSchool::Abjuration, SpellResolutionType::Utility, "Self", "1 hour", "Hide the party's passage.", "Great for warehouse and mine approaches.", {"stealth", "wilderness"}),
        makeSpell("spell_read_residue", "Read Residue", 1, SpellSchool::Divination, SpellResolutionType::Utility, "30 ft", "Instantaneous", "Reveals emotional trace of violence or magic.", "A campaign ritual for mystery scenes.", {"ritual", "investigation", "campaign"}),
        makeSpell("spell_bellward", "Bellward", 2, SpellSchool::Abjuration, SpellResolutionType::Utility, "15 ft", "1 minute", "Dampens sonic, fear, and compulsion effects.", "Protects the party during tolls.", {"defense", "bell"}),
        makeSpell("spell_mist_script", "Mist Script", 0, SpellSchool::Illusion, SpellResolutionType::Utility, "30 ft", "1 minute", "Write a hidden message in fog or breath.", "Cantrip utility for clues and player display.", {"cantrip", "utility"}),
        makeSpell("spell_silver_nail_binding", "Silver Nail Binding", 2, SpellSchool::Abjuration, SpellResolutionType::SummonOrZone, "Touch", "Until removed", "Pins a minor spirit or shade to a location.", "Objective spell for foundry and finale.", {"ritual", "occult"}),
        makeSpell("spell_lantern_true_tracks", "Lantern of True Tracks", 2, SpellSchool::Divination, SpellResolutionType::Utility, "Self", "1 hour", "Reveal one creature's recent path as blue sparks.", "Pairs with the blue-glass lantern.", {"ritual", "tracking"}),
        makeSpell("spell_moon_glass_confession", "Moon-Glass Confession", 4, SpellSchool::Necromancy, SpellResolutionType::Utility, "30 ft", "1 hour", "Force a spirit echo to speak one buried truth at a price.", "Forbidden ritual tied to the finale.", {"forbidden", "finale"})
    };
}

void configureCharacters(Campaign &campaign, const std::filesystem::path &root)
{
    const std::filesystem::path portraits = packageRoot(root) / "Assets" / "Portraits";
    auto portrait = [&](const std::string &file) { return projectPath(root, portraits / file); };

    CharacterModel mira = makeCharacter("pc_mira_voss", "Mira Voss", "Human", "Fighter", "Disgraced officer", 5, attrs(17, 12, 15, 13, 10, 14), 44, 48, portrait("pc_mira_voss.png"), {"frontline", "leader", "tactician"});
    mira.backstory = "A disgraced battle captain trying to prove her last order saved more lives than it cost.";
    mira.playerNotes = "Protect people first. Ask hard questions second.";
    mira.gmNotes = "Marshal Vale altered Mira's report; Blackwater's council records can expose that pattern.";
    mira.inventory.entries = {
        makeEntry("inv_mira_sword", "item_star_notched_longsword", 1, true, "Main Hand"),
        makeEntry("inv_mira_shield", "item_shield_cracked_sun", 1, true, "Off Hand"),
        makeEntry("inv_mira_chain", "item_chain_mail_hauberk", 1, true, "Armor"),
        makeEntry("inv_mira_case", "item_field_command_map_case", 1, false, "Pack"),
        makeEntry("inv_mira_whistle", "item_signal_whistle", 1, false, "Belt"),
        makeEntry("inv_mira_healing", "item_healing_draught", 2, false, "Pack")};
    mira.relationships = {
        relationship("rel_mira_tamsin", "Character", "pc_tamsin_reed", "Protected witness", "Mira once protected Tamsin from a tribunal; Tamsin now wants the sealed report."),
        relationship("rel_mira_magistrate", "NPC", "npc_magistrate_elowen_vale", "Authority mirror", "Elowen's cover-up echoes the old battlefield lie.")};
    mira.featureActions = {action("Commanding Rally", "Feature", 30, 30, "+0", "1d8 morale", "Spend Rally Dice to help an ally recover momentum.")};
    mira.resourceTrackers = {resource("res_mira_rally", "Rally Dice", 4, 1, "Short Rest", "Leadership resource for table-state testing.")};

    CharacterModel kael = makeCharacter("pc_kael_thornevein", "Kael Thornevein", "Dwarf", "Cleric", "Forge priest", 5, attrs(14, 9, 16, 11, 17, 12), 39, 43, portrait("pc_kael_thornevein.png"), {"healer", "support", "forge"});
    kael.backstory = "A forge-priest who believes the Bell can sanctify truth or melt every oath ever sworn.";
    kael.spellcasting.enabled = true;
    kael.spellcasting.ability = "Wisdom";
    kael.spellcasting.spellAttackBonus = 6;
    kael.spellcasting.spellSaveDc = 14;
    kael.spellcasting.slotMaximums = {0, 4, 3, 2, 0, 0, 0, 0, 0, 0};
    kael.spellcasting.slotUsed = {0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
    kael.spellcasting.spells = {spellEntry("spell_bless"), spellEntry("spell_cure_wounds"), spellEntry("spell_detect_magic"), spellEntry("spell_guiding_bolt"), spellEntry("spell_healing_word"), spellEntry("spell_spiritual_weapon"), spellEntry("spell_heat_metal"), spellEntry("spell_revivify"), spellEntry("spell_spirit_guardians"), spellEntry("spell_bellward")};
    kael.inventory.entries = {
        makeEntry("inv_kael_hammer", "item_warhammer_bellowsong", 1, true, "Main Hand"),
        makeEntry("inv_kael_shield", "item_steel_shield_forge_prayers", 1, true, "Off Hand"),
        makeEntry("inv_kael_scale", "item_scale_mail_brass_rivets", 1, true, "Armor"),
        makeEntry("inv_kael_tools", "item_smith_tools", 1, false, "Pack"),
        makeEntry("inv_kael_healing", "item_healing_draught", 1, false, "Belt")};
    kael.relationships = {
        relationship("rel_kael_halven", "NPC", "npc_brother_halven", "Religious contact", "Halven asks Kael whether the chapel confessions are holy or haunted."),
        relationship("rel_kael_nyra", "Character", "pc_nyra_moonbrook", "Cautious ally", "Kael distrusts fey bargains, but Nyra once saved his forge from a curse.")};

    CharacterModel tamsin = makeCharacter("pc_tamsin_reed", "Tamsin Reed", "Halfling", "Rogue", "Field journalist", 5, attrs(8, 18, 12, 15, 14, 13), 31, 34, portrait("pc_tamsin_reed.png"), {"scout", "investigation", "social"});
    tamsin.backstory = "A cheerful field journalist with three false names and one very real bounty.";
    tamsin.inventory.entries = {
        makeEntry("inv_tamsin_daggers", "item_paired_daggers", 2, true, "Belt"),
        makeEntry("inv_tamsin_shortbow", "item_shortbow_reporter", 1, true, "Back"),
        makeEntry("inv_tamsin_leather", "item_leather_hidden_pockets", 1, true, "Armor"),
        makeEntry("inv_tamsin_tools", "item_thieves_tools", 1, false, "Inner Coat"),
        makeEntry("inv_tamsin_satchel", "item_reporter_satchel", 1, false, "Shoulder"),
        makeEntry("inv_tamsin_forgery", "item_forgery_kit", 1, false, "Pack"),
        makeEntry("inv_tamsin_pick", "item_silvered_lockpick", 1, false, "Boot")};
    tamsin.relationships = {
        relationship("rel_tamsin_silke", "NPC", "npc_silke_marr", "Identity trouble", "Silke sold Tamsin's false identity twice."),
        relationship("rel_tamsin_mira", "Character", "pc_mira_voss", "Owes freedom", "Tamsin wants the truth in Mira's sealed report.")};
    tamsin.resourceTrackers = {resource("res_tamsin_focus", "Investigative Focus", 3, 0, "Short Rest", "Use for clue follow-up and selected-row QA.")};

    CharacterModel elyon = makeCharacter("pc_elyon_starling", "Elyon Starling", "Elf", "Wizard", "Exiled observatory scholar", 5, attrs(7, 14, 11, 18, 13, 10), 23, 27, portrait("pc_elyon_starling.png"), {"arcane", "lore", "rituals"});
    elyon.backstory = "An exiled observatory scholar whose prophecies are accurate only when they ruin his life.";
    elyon.spellcasting.enabled = true;
    elyon.spellcasting.ability = "Intelligence";
    elyon.spellcasting.spellAttackBonus = 7;
    elyon.spellcasting.spellSaveDc = 15;
    elyon.spellcasting.slotMaximums = {0, 4, 3, 2, 0, 0, 0, 0, 0, 0};
    elyon.spellcasting.slotUsed = {0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
    elyon.spellcasting.spells = {spellEntry("spell_shield"), spellEntry("spell_magic_missile"), spellEntry("spell_detect_magic"), spellEntry("spell_find_familiar"), spellEntry("spell_misty_step"), spellEntry("spell_counterspell"), spellEntry("spell_fireball"), spellEntry("spell_read_residue"), spellEntry("spell_lantern_true_tracks"), spellEntry("spell_moon_glass_confession", "Forbidden", false, "Known from marginalia; unsafe.")};
    elyon.inventory.entries = {
        makeEntry("inv_elyon_staff", "item_quarterstaff_silver_rings", 1, true, "Main Hand"),
        makeEntry("inv_elyon_book", "item_spellbook_blue_meridian", 1, false, "Satchel"),
        makeEntry("inv_elyon_astrolabe", "item_cracked_crystal_astrolabe", 1, true, "Arcane Focus"),
        makeEntry("inv_elyon_scroll", "item_scroll_dispel_magic", 1, false, "Scroll Case")};
    elyon.relationships = {
        relationship("rel_elyon_rusk", "NPC", "npc_director_cassian_rusk", "Patron threat", "Rusk wants Elyon's observatory math for extraction."),
        relationship("rel_elyon_nyra", "Character", "pc_nyra_moonbrook", "Impossible map", "His star charts show Nyra's lost village in the lake's false city.")};

    CharacterModel nyra = makeCharacter("pc_nyra_moonbrook", "Nyra Moonbrook", "Half-Elf", "Ranger", "Borderland guide", 5, attrs(10, 17, 13, 12, 16, 14), 37, 41, portrait("pc_nyra_moonbrook.png"), {"ranged", "wilderness", "tracking"});
    nyra.backstory = "A borderland guide who remembers paths through forests that no longer exist.";
    nyra.spellcasting.enabled = true;
    nyra.spellcasting.ability = "Wisdom";
    nyra.spellcasting.spellAttackBonus = 6;
    nyra.spellcasting.spellSaveDc = 14;
    nyra.spellcasting.slotMaximums = {0, 4, 2, 0, 0, 0, 0, 0, 0, 0};
    nyra.spellcasting.slotUsed = {0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
    nyra.spellcasting.spells = {spellEntry("spell_hunters_mark"), spellEntry("spell_goodberry"), spellEntry("spell_cure_wounds"), spellEntry("spell_pass_without_trace"), spellEntry("spell_lantern_true_tracks"), spellEntry("spell_mist_script")};
    nyra.inventory.entries = {
        makeEntry("inv_nyra_longbow", "item_longbow_pale_yew", 1, true, "Hands"),
        makeEntry("inv_nyra_leather", "item_studded_leather_leaf", 1, true, "Armor"),
        makeEntry("inv_nyra_pack", "item_explorer_pack", 1, false, "Back"),
        makeEntry("inv_nyra_charm", "item_moonvine_charm", 1, true, "Neck"),
        makeEntry("inv_nyra_shard", "item_starfall_shard_moss", 1, false, "Belt Pouch")};
    nyra.relationships = {
        relationship("rel_nyra_veyra", "NPC", "npc_veyra_thornwake", "Fey debt", "Veyra knows why Nyra's village is remembered under the lake."),
        relationship("rel_nyra_elyon", "Character", "pc_elyon_starling", "Shared impossibility", "Elyon's charts prove Nyra's memories are not merely fey dreams.")};

    campaign.characters = {mira, kael, tamsin, elyon, nyra};
}

void configureNpcsAndFactions(Campaign &campaign)
{
    campaign.factions = {
        makeFaction("faction_blackwater_council", "Blackwater Town Council", "Civic Authority", "Keeps order while hiding the mine massacre.", "npc_magistrate_elowen_vale", "Control evidence and prevent panic.", 72, 3, {"civic", "secrecy"}),
        makeFaction("faction_gilded_lantern", "Gilded Lantern Company", "Industrial Company", "Wants to extract the Bell as a memory weapon.", "npc_director_cassian_rusk", "Recover relics and own the disaster response.", 68, 4, {"company", "exploitation"}),
        makeFaction("faction_ash_choir", "The Ash Choir", "Cult", "Comforts the afflicted while waking the Regent-in-Echo.", "npc_mother_sable", "Complete the Procession of Names.", 54, 5, {"cult", "ritual"}),
        makeFaction("faction_drowned_remnant", "The Drowned Remnant", "Ghosts", "Dead miners demanding truthful remembrance.", "npc_tomas_reed", "Force confession and remembrance, even violently.", 42, 4, {"ghosts", "vengeance"}),
        makeFaction("faction_thornwake_court", "Thornwake Court", "Fey Court", "Remembers paths that the Bell tried to overwrite.", "npc_veyra_thornwake", "Recover a stolen shadow from the false city.", 35, 2, {"fey", "memory"})};

    DeityModel bell;
    setMetadata(bell.metadata, "power_mnemosyne_bell", {"archive", "bell", "memory"});
    bell.name = "The Mnemosyne Bell";
    bell.powerType = "Archive Engine";
    bell.pantheon = "Pre-flood Blackwater";
    bell.domains = "Memory, Forbidden History, Echoes";
    bell.symbol = "A bell over black water";
    bell.alignment = "Unaligned";
    bell.tenets = "All buried truth must one day ring.";
    bell.worshipers = "Ash Choir, scholars, desperate mourners";
    bell.relationships = "The Regent-in-Echo feeds on the Bell's stolen names.";
    bell.influence = 80;
    bell.threatLevel = 5;
    campaign.deities = {bell};

    campaign.npcs = {
        makeNpc("npc_magistrate_elowen_vale", "Magistrate Elowen Vale", NPCCategory::Person, "Blackwater Town Council", "Council leader", "Stern council leader trying to keep the town intact.", "Protective but controlling.", "Her grandfather ordered the mine flooded.", "Suppress panic and survive the scandal.", 3, {"council", "authority", "secret"}),
        makeNpc("npc_director_cassian_rusk", "Director Cassian Rusk", NPCCategory::Person, "Gilded Lantern Company", "Industrial antagonist", "Polished company director with a silver-headed cane.", "Civil, patient, transactional.", "His cane contains a Bell fragment granting partial immunity.", "Extract the Bell before the eclipse.", 4, {"company", "villain", "relic"}),
        makeNpc("npc_mother_sable", "Mother Sable", NPCCategory::Person, "The Ash Choir", "Cult leader", "Gentle grief counselor and hidden vessel of the Regent-in-Echo.", "Soft-spoken and comforting.", "She is partially possessed by the Regent.", "Wake the Regent through the Procession of Names.", 5, {"cult", "ritual", "boss"}),
        makeNpc("npc_tomas_reed", "Tomas Reed", NPCCategory::Creature, "The Drowned Remnant", "Ghost foreman", "Dead shift foreman whose truth is not clean either.", "Bitter, direct, exhausted.", "He locked some miners below to stop the Bell.", "Be remembered truthfully.", 4, {"ghost", "miner", "truth"}),
        makeNpc("npc_nessa_vale", "Nessa Vale", NPCCategory::Person, "Blackwater Hollow", "Memory-afflicted child", "Draws Bell chambers before seeing them.", "Frightened, honest, observant.", "Her bloodline lets the Bell map itself through her dreams.", "Stay herself through the next toll.", 1, {"clue", "child", "vision"}),
        makeNpc("npc_mara_quill", "Mara Quill", NPCCategory::Person, "The Silted Crown", "Innkeeper", "Warm innkeeper preserving forgotten objects in a cedar box.", "Helpful and tired.", "She remembers guests everyone else forgot.", "Keep the inn a sanctuary.", 1, {"inn", "ally", "rumors"}),
        makeNpc("npc_brother_halven", "Brother Halven", NPCCategory::Person, "Lake Chapel", "Chapel keeper", "Nervous keeper of a bell that rings without a clapper.", "Devout and helpful.", "The dead confess through his chapel bell.", "Learn whether the voices are holy.", 2, {"chapel", "confession"}),
        makeNpc("npc_joric_pike", "Joric Pike", NPCCategory::Person, "Blackwater Fishers", "Witness", "Blunt fisherman who saw a living hand rise from his net.", "Suspicious but brave when needed.", "He cut the hand loose and lied about it.", "Protect his family from lake omens.", 1, {"lake", "witness"}),
        makeNpc("npc_lady_corvenna_ash", "Lady Corvenna Ash", NPCCategory::Person, "House Ash", "Relic collector", "Elegant noble patron with a mirror full of bad advice.", "Gracious and predatory.", "Funds rival expeditions into memory sites.", "Own a Bell fragment before Rusk does.", 3, {"noble", "patron", "schemer"}),
        makeNpc("npc_silke_marr", "Silke Marr", NPCCategory::Person, "Velvet Ledger", "Identity merchant", "Broker with a glass eye and too many true names.", "Amused and transactional.", "Sold Tamsin's false identity twice.", "Profit from every secret without being remembered.", 2, {"criminal", "social", "secrets"}),
        makeNpc("npc_veyra_thornwake", "Veyra of the Thornwake", NPCCategory::Deity, "Thornwake Court", "Fey sovereign", "Dangerously playful fey who remembers erased paths.", "Affectionate in an alien way.", "Knows who erased Nyra's village but cannot say the name.", "Ransom a stolen shadow from the false city.", 4, {"fey", "forest", "bargain"}),
        makeNpc("npc_ser_jessa_marrow", "Ser Jessa Marrow", NPCCategory::Creature, "Oathbound Dead", "Undead knight", "Spectral knight seeking release from a failed oath.", "Honorable and mournful.", "She failed by choice, not weakness.", "See the true oath spoken at the final chamber.", 4, {"undead", "knight", "boss-or-ally"}),
        makeNpc("npc_bellcaster_ilyr", "Bell-Caster Ilyr", NPCCategory::Person, "The Ash Choir", "Foundry ritualist", "Masked caster striking a bell with no clapper.", "Zealous and frightened.", "Believes Vane and Rusk both serve justice.", "Locate the lake chapel crypt.", 4, {"cult", "foundry", "encounter"}),
        makeNpc("npc_nix_lantern_boy", "Nix, the Lantern Boy", NPCCategory::Person, "Blackwater Streets", "Frightened runner", "Hooded youth paid to leave blue lantern signs.", "Scared, not evil.", "Can identify the boathouse buyer.", "Get paid and leave alive.", 1, {"chase", "clue", "noncombat"})};
}

void configureMaps(Campaign &campaign, const std::filesystem::path &root)
{
    auto mapPath = [&](const std::string &file) { return projectPath(root, packageRoot(root) / "Assets" / "Maps" / file); };

    MapModel silted = makeMap("map_silted_crown", "The Silted Crown Inn", "Opening common room, inn sanctuary, and first witness scene.", MapScope::Local, 18, 12, mapPath("silted_crown_common_room.bmp"), false, {"inn", "opening", "player-visible"}, 1);
    silted.features = {
        mapFeature("pin_ledger_box", MapFeatureKind::Pin, "Cedar Box of Forgotten Things", "Mara's box of abandoned objects.", "Contains H07 and one optional PC memory hook.", 4, 7, true, false, 0),
        mapFeature("pin_cracked_window", MapFeatureKind::Hazard, "Cracked Lake Window", "Window cracks during the first toll.", "Use Danger Stinger when it breaks.", 14, 2, true, false, 0)};
    silted.tokens = {token("tok_silted_mara", "Mara Quill", MapTokenKind::NPC, "NPC", "npc_mara_quill", 5, 6), token("tok_silted_nessa", "Nessa Vale", MapTokenKind::NPC, "NPC", "npc_nessa_vale", 10, 4)};

    MapModel town = makeMap("map_blackwater_town", "Blackwater Town Ward", "Town investigation map: council hall, market, chapel road, docks, and lake edge.", MapScope::World, 48, 36, mapPath("blackwater_town_ward.bmp"), true, {"town", "investigation", "main-map"}, 2);
    town.features = {
        mapFeature("pin_saint_orwyn_fountain", MapFeatureKind::NamedLocation, "Saint Orwyn's Fountain", "First murder site and public panic point.", "Blood diluted by fountain overflow.", 22, 16, true, false, 0),
        mapFeature("pin_council_hall", MapFeatureKind::NamedLocation, "Council Hall", "Curfew announcements and archive access.", "Hidden archive stairs below magistrate dais.", 12, 10, true, false, 13),
        mapFeature("pin_lake_chapel", MapFeatureKind::NamedLocation, "Dry Bell Chapel", "Bell rings without a clapper.", "Tomas can speak through it.", 34, 28, true, false, 0),
        mapFeature("pin_smuggler_route", MapFeatureKind::SecretDoor, "East Bridge Smuggler Route", "A route into the warehouse district.", "Revealed if Nix escapes.", 38, 18, false, true, 14)};
    town.regions = {region("reg_market_square", "Market Square", "Crowded public gathering space.", 16, 13, 12, 8, true), region("reg_lake_mist", "Lake Mist", "Visibility drops near the water.", 30, 24, 14, 10, true)};
    town.tokens = {token("tok_party_start", "Party", MapTokenKind::Party, "Party", kCampaignId, 8, 8), token("tok_rusk_dock", "Director Rusk", MapTokenKind::NPC, "NPC", "npc_director_cassian_rusk", 40, 24, true)};
    town.routes = {MapRouteModel{}};
    setMetadata(town.routes[0].metadata, "route_town_to_mine", {"travel", "mine"});
    town.routes[0].name = "Town Ward to Sealed Third Shaft";
    town.routes[0].fromMapId = "map_blackwater_town";
    town.routes[0].toMapId = "map_bellfoundry_cellars";
    town.routes[0].startX = 12;
    town.routes[0].startY = 10;
    town.routes[0].endX = 4;
    town.routes[0].endY = 5;
    town.routes[0].distanceMiles = 3.5f;
    town.routes[0].terrain = "Rainy town roads, lake mud, old mine track";
    town.routes[0].notes = "Use for travel workflow and watch/ration counters.";

    MapModel boathouse = makeMap("map_old_boathouse", "Old Boathouse Ambush", "Rotting pier, foggy canal, crates, nets, and a ledger fire timer.", MapScope::Local, 30, 22, mapPath("old_boathouse_ambush.bmp"), false, {"battlefield", "smugglers"}, 3);
    boathouse.features = {
        mapFeature("pin_net_trap", MapFeatureKind::Trap, "Loose Cargo Net", "Restrains creatures in a ten-foot square.", "DC 14 Perception to spot.", 15, 9, false, true, 14),
        mapFeature("pin_blooded_oar", MapFeatureKind::Pin, "Blooded Oar", "Matches wounds on a second victim.", "Connects smugglers to the murder route.", 22, 15, true, false, 0),
        mapFeature("pin_false_barrel", MapFeatureKind::Pin, "False Barrel", "Contains a silvered knife and coded ledger scrap.", "Reward if searched before fire spreads.", 7, 18, false, true, 13)};
    boathouse.regions = {region("reg_slippery_planks", "Slippery Planks", "First dash each round risks falling prone.", 8, 6, 14, 10, true)};

    MapModel marsh = makeMap("map_reedfen_marsh", "Reedfen Marsh Causeway", "Moonlit marsh, heavy mist, sunken shrine stones, and rescue route.", MapScope::Local, 44, 32, mapPath("reedfen_marsh_causeway.bmp"), false, {"marsh", "wilderness", "rescue"}, 4);
    marsh.features = {
        mapFeature("pin_sinking_pool", MapFeatureKind::Hazard, "Sinking Pool", "Restrains creatures that fail to escape mud.", "Use low impact cue.", 18, 21, true, false, 12),
        mapFeature("pin_shrine_stones", MapFeatureKind::NamedLocation, "Broken Shrine Stones", "Protective ward inverted inward.", "Gives H03 rubbing clue.", 32, 8, true, false, 0),
        mapFeature("pin_buried_satchel", MapFeatureKind::Pin, "Buried Satchel", "Contains wet ledger page.", "Handout H02 if recovered.", 11, 25, false, true, 13)};
    marsh.regions = {region("reg_heavy_mist", "Heavy Mist", "Sight-based checks past 40 feet are impaired.", 4, 4, 36, 24, false)};

    MapModel foundry = makeMap("map_bellfoundry_cellars", "Abandoned Bellfoundry Cellars", "Dungeon cellars with furnaces, chains, cooling channels, and ritual chalk.", MapScope::Dungeon, 38, 28, mapPath("abandoned_bellfoundry_cellars.bmp"), false, {"dungeon", "foundry", "ritual"}, 5);
    foundry.features = {
        mapFeature("pin_secret_kiln", MapFeatureKind::SecretDoor, "Secret Kiln Door", "Door behind a cracked kiln.", "Opens with Black Iron Foundry Key.", 24, 12, false, true, 15),
        mapFeature("pin_unstable_ceiling", MapFeatureKind::Hazard, "Unstable Ceiling", "Thunder or heavy impact causes falling rubble.", "Use for combat repositioning.", 14, 7, true, false, 12),
        mapFeature("pin_casting_circle", MapFeatureKind::Hazard, "Casting Circle", "Three silver nails empower Ilyr.", "Remove nails to weaken boss.", 30, 20, true, false, 0)};

    MapModel chapel = makeMap("map_lake_chapel_ruins", "Lake Chapel Ruins", "Finale battlefield on a lake island, with moon-glass altar and crypt stairs.", MapScope::Local, 42, 34, mapPath("lake_chapel_ruins.bmp"), false, {"finale", "chapel", "ritual"}, 6);
    chapel.features = {
        mapFeature("pin_moon_glass_altar", MapFeatureKind::Hazard, "Moon-Glass Altar", "Ritual focus. AC 15, HP 25.", "Pulses until disrupted.", 21, 12, true, false, 0),
        mapFeature("pin_flooded_crypt_stairs", MapFeatureKind::NamedLocation, "Flooded Crypt Stairs", "Revealed when the bell tolls.", "Leads toward Bell chamber.", 28, 24, false, true, 0),
        mapFeature("pin_broken_saint_window", MapFeatureKind::Pin, "Broken Saint Window", "Colored moonlight weakens summoned shades.", "Safe zone against Moon-Glass Shade.", 13, 6, true, false, 0)};

    MapModel bell = makeMap("map_mnemosyne_bell", "Mnemosyne Bell Chamber", "Reality-warping final chamber with the Bell above black water.", MapScope::Dungeon, 36, 28, mapPath("mnemosyne_bell_chamber.bmp"), false, {"finale", "memory", "boss"}, 7);
    bell.features = {
        mapFeature("pin_break_intervention", MapFeatureKind::Pin, "Break", "Destroy the archive to save the town.", "Erases stored ancient truths.", 8, 14, true, false, 0),
        mapFeature("pin_bind_intervention", MapFeatureKind::Pin, "Bind", "Anchor the Bell to a willing guardian.", "Requires a living, ghostly, or faction anchor.", 14, 8, true, false, 0),
        mapFeature("pin_mute_intervention", MapFeatureKind::Pin, "Mute", "Delay catastrophe and leave fragments behind.", "Sequel hook.", 22, 8, true, false, 0),
        mapFeature("pin_crown_intervention", MapFeatureKind::Pin, "Crown", "Accept the Regent's bargain.", "Transforms the world.", 28, 14, true, false, 0)};

    campaign.maps = {silted, town, boathouse, marsh, foundry, chapel, bell};
    campaign.partyLocation.mapId = "map_blackwater_town";
    campaign.partyLocation.x = 8;
    campaign.partyLocation.y = 8;
    campaign.partyTravel.currentMapId = "map_blackwater_town";
    campaign.partyTravel.routeId = "route_town_to_mine";
    campaign.partyTravel.routeName = "Town Ward to Sealed Third Shaft";
    campaign.partyTravel.travelPace = "Careful";
    campaign.partyTravel.distanceMiles = 3.5f;
    campaign.partyTravel.milesTraveled = 0.75f;
    campaign.partyTravel.watches = 1;
    campaign.partyTravel.rationsUsed = 5;
    campaign.partyTravel.randomEncounterChecks = 1;
    campaign.partyTravel.notes = "Travel clock starts after the council archive scene; use watches before entering the flooded mine.";
}

void configureEncounters(Campaign &campaign)
{
    MonsterDefinitionModel miner = makeMonster("monster_drowned_miner_echo", "Drowned Miner Echo", "1", 13, 22, "Can be calmed by truthful remembrance.", {"ghost", "memory"});
    miner.actions = {action("Waterlogged Pick", "Attack", 5, 5, "+4", "1d8+2 bludgeoning", "Deals no killing blow if appeased."), action("Memory Drag", "Hazard", 5, 30, "DC 13 Wis", "confusion", "Target sees the floodgate closing.")};
    MonsterDefinitionModel acolyte = makeMonster("monster_ash_choir_acolyte", "Ash Choir Acolyte", "2", 12, 28, "Chants cause fear and positioning pressure.", {"cult", "caster"});
    acolyte.actions = {action("Ash Hymn", "Spell", 5, 30, "DC 13 Wis", "frightened", "Failed save imposes fear until end of next turn."), action("Candle Knife", "Attack", 5, 5, "+4", "1d6+2 piercing", "Blade smokes with gray wax.")};
    MonsterDefinitionModel shade = makeMonster("monster_moon_glass_shade", "Moon-Glass Shade", "5", 14, 58, "Weak in colored moonlight; feeds on hidden guilt.", {"shade", "undead", "boss"});
    shade.actions = {action("Reflected Grasp", "Attack", 10, 10, "+6", "2d8 necrotic", "Pulls a target toward a mirror-bright tile."), action("Borrowed Voice", "Spell", 5, 60, "DC 15 Wis", "charmed", "Speaks in a loved one's voice.")};
    MonsterDefinitionModel regent = makeMonster("monster_regent_in_echo", "Regent-in-Echo", "8", 17, 124, "Boss presence made from stolen memories and false histories.", {"finale", "memory", "boss"});
    regent.actions = {action("Rewrite Wound", "Attack", 30, 60, "+8", "3d8 psychic", "Target briefly remembers being struck last year."), action("Offer Restoration", "Legendary", 5, 60, "DC 16 Wis", "temptation", "A personal bargain that can also solve a PC hook.")};
    MonsterDefinitionModel smuggler = makeMonster("monster_canalside_cutthroat", "Canalside Cutthroat", "1/2", 13, 16, "Works for coin; morale breaks if ledger is saved.", {"smuggler", "humanoid"});
    smuggler.type = "Humanoid";
    smuggler.actions = {action("Silvered Knife", "Attack", 5, 20, "+4", "1d4+2 piercing", "Knife is recoverable as loot."), action("Kick Barrel", "Hazard", 5, 10, "+0", "difficult terrain", "Creates cover or blocks a route.")};
    campaign.customMonsters = {miner, acolyte, shade, regent, smuggler};

    EncounterModel opening = makeEncounter("enc_silted_crown_opening", "The Toll at The Silted Crown", "Social hazard and panic-management opening scene.", "map_silted_crown", 18, 12, {"opening", "social", "memory"}, 1);
    opening.theaterOfMind = false;
    opening.participants = {
        participant("part_opening_mara", CombatantKind::NPC, "npc_mara_quill", "", "Mara Quill", 8, 18, 11, true, 5, 6, {"npc", "ally"}),
        participant("part_opening_nessa", CombatantKind::NPC, "npc_nessa_vale", "", "Nessa Vale", 12, 6, 10, true, 10, 4, {"npc", "rescue"}),
        participant("part_opening_echo", CombatantKind::Monster, "", "monster_drowned_miner_echo", "Memory-Sick Townsfolk", 6, 18, 10, false, 14, 3, {"hazard", "nonlethal"})};
    opening.diceRollPresets = {dicePreset("roll_opening_calm", "Calm Panic", "1d20+3", "Use Persuasion, Performance, Medicine, or Religion."), dicePreset("roll_memory_save", "Memory Save", "1d20", "Wisdom save vs minor memory displacement.")};
    opening.afterCombatSummary = "If the room is stabilized, Mara opens the cedar box and gives the cracked inn ledger.";

    EncounterModel boat = makeEncounter("enc_boathouse_cutthroats", "Old Boathouse Cutthroats", "Combat around crates, nets, wet planks, and a burning ledger scrap.", "map_old_boathouse", 30, 22, {"combat", "smugglers", "ledger"}, 3);
    boat.participants = {
        participant("part_boat_mira", CombatantKind::Player, "pc_mira_voss", "", "Mira Voss", 14, 44, 18, true, 4, 12, {"pc"}),
        participant("part_boat_tamsin", CombatantKind::Player, "pc_tamsin_reed", "", "Tamsin Reed", 17, 31, 15, true, 5, 13, {"pc"}),
        participant("part_boat_cut1", CombatantKind::Monster, "", "monster_canalside_cutthroat", "Smuggler Knife A", 13, 16, 13, false, 17, 9, {"monster"}),
        participant("part_boat_cut2", CombatantKind::Monster, "", "monster_canalside_cutthroat", "Smuggler Knife B", 11, 16, 13, false, 20, 11, {"monster"}),
        participant("part_boat_bruiser", CombatantKind::Monster, "", "monster_canalside_cutthroat", "Canal Bruiser", 8, 34, 14, false, 22, 15, {"monster", "bruiser"})};
    boat.afterCombatLootNotes = "Award silvered canal knife, coded ledger scrap, and 35 gp mixed coin.";
    boat.afterCombatStoryConsequences = "Recovered ledger points to the Bellfoundry and Company invoice.";
    boat.diceRollPresets = {dicePreset("roll_net_trap", "Cargo Net Trap", "1d20+2", "Opposed by DC 14 Perception or Dexterity save."), dicePreset("roll_ledger_fire", "Ledger Fire Timer", "1d4", "Rounds before a lookout burns the ledger.")};

    EncounterModel marsh = makeEncounter("enc_reedfen_wisps", "Lights in the Reedfen", "Rescue encounter in heavy mist and sinking pools.", "map_reedfen_marsh", 24, 18, {"marsh", "rescue", "hazard"}, 4);
    marsh.participants = {
        participant("part_marsh_nyra", CombatantKind::Player, "pc_nyra_moonbrook", "", "Nyra Moonbrook", 16, 37, 15, true, 3, 10, {"pc"}),
        participant("part_marsh_elyon", CombatantKind::Player, "pc_elyon_starling", "", "Elyon Starling", 12, 23, 12, true, 4, 10, {"pc"}),
        participant("part_marsh_wisp1", CombatantKind::Monster, "", "monster_drowned_miner_echo", "Marsh Wisp A", 18, 22, 15, false, 14, 5, {"monster", "wisp"}),
        participant("part_marsh_husk1", CombatantKind::Monster, "", "monster_drowned_miner_echo", "Drowned Husk", 6, 15, 11, false, 16, 12, {"monster", "grappler"})};
    marsh.afterCombatSummary = "Saving the scribe grants archive access and a rubbing of the inverted ward.";
    marsh.afterCombatLootNotes = "Potion of Marsh-Breath and H03 if the satchel is recovered.";

    EncounterModel sentries = makeEncounter("enc_bellfoundry_sentries", "The Chain Hall Sentries", "Dungeon fight to prevent the alarm bell from ringing.", "map_bellfoundry_cellars", 24, 18, {"dungeon", "cult", "chains"}, 5);
    sentries.participants = {
        participant("part_foundry_kael", CombatantKind::Player, "pc_kael_thornevein", "", "Kael Thornevein", 9, 39, 18, true, 4, 8, {"pc"}),
        participant("part_foundry_mira", CombatantKind::Player, "pc_mira_voss", "", "Mira Voss", 14, 44, 18, true, 5, 8, {"pc"}),
        participant("part_foundry_acolyte1", CombatantKind::Monster, "", "monster_ash_choir_acolyte", "Ashbound Acolyte", 12, 28, 12, false, 16, 8, {"monster", "caster"}),
        participant("part_foundry_sentry1", CombatantKind::Monster, "", "monster_canalside_cutthroat", "Bell Cult Sentry", 10, 24, 14, false, 18, 10, {"monster", "guard"})};
    sentries.afterCombatLootNotes = "Foundry key, acolyte charm, cult route map.";
    sentries.afterCombatSessionNotes = "If the alarm rings, start Midnight Casting with prepared defenders.";

    EncounterModel casting = makeEncounter("enc_midnight_casting", "Bell-Caster Ilyr's Midnight Casting", "Objective boss fight around the casting circle and silver nails.", "map_bellfoundry_cellars", 28, 20, {"boss", "ritual", "objective"}, 6);
    casting.participants = {
        participant("part_casting_ilyr", CombatantKind::NPC, "npc_bellcaster_ilyr", "monster_ash_choir_acolyte", "Bell-Caster Ilyr", 14, 72, 15, false, 20, 14, {"boss"}),
        participant("part_casting_chain1", CombatantKind::Monster, "", "monster_drowned_miner_echo", "Animated Chain", 8, 20, 16, false, 15, 10, {"hazard"}),
        participant("part_casting_acolyte1", CombatantKind::Monster, "", "monster_ash_choir_acolyte", "Ashbound Acolyte A", 11, 28, 12, false, 22, 12, {"caster"})};
    casting.afterCombatSummary = "Ilyr reveals the lake chapel and names Vane only if captured or magically pressed.";
    casting.afterCombatLootNotes = "Moon-glass shard, ritual notes, Black Iron Foundry Key.";
    casting.combatLog = {combatLog("log_casting_setup", 1, "GM", "Setup", "Remove three silver nails to drop Ilyr's ritual advantage.")};

    EncounterModel finale = makeEncounter("enc_lake_chapel_finale", "Ritual at the Lake Chapel", "Finale combat and confession reveal before descent into the Bell chamber.", "map_lake_chapel_ruins", 30, 22, {"finale", "ritual", "shade"}, 7);
    finale.participants = {
        participant("part_finale_vane", CombatantKind::NPC, "npc_magistrate_elowen_vale", "", "Magistrate Vane's Proxy", 15, 68, 16, false, 20, 10, {"commander"}),
        participant("part_finale_shade", CombatantKind::Monster, "", "monster_moon_glass_shade", "Moon-Glass Shade", 18, 58, 14, false, 22, 13, {"shade", "boss"}),
        participant("part_finale_devotee1", CombatantKind::Monster, "", "monster_ash_choir_acolyte", "Cult Devotee A", 9, 18, 13, false, 14, 15, {"minion"}),
        participant("part_finale_devotee2", CombatantKind::Monster, "", "monster_ash_choir_acolyte", "Cult Devotee B", 7, 18, 13, false, 17, 16, {"minion"})};
    finale.afterCombatSummary = "Expose Vane, save a possessed witness, and choose whether to pursue Break, Bind, Mute, or Crown.";
    finale.afterCombatStoryConsequences = "Saved witness reduces final DC; failed ritual adds one Regent phase.";

    EncounterModel bell = makeEncounter("enc_mnemosyne_bell", "The Mnemosyne Bell", "Reality-warping boss and moral-choice climax.", "map_mnemosyne_bell", 28, 20, {"finale", "memory", "boss"}, 8);
    bell.participants = {
        participant("part_bell_regent", CombatantKind::Monster, "", "monster_regent_in_echo", "Regent-in-Echo", 19, 124, 17, false, 14, 10, {"boss", "memory"}),
        participant("part_bell_sable", CombatantKind::NPC, "npc_mother_sable", "", "Mother Sable", 13, 42, 13, false, 16, 12, {"cult", "vessel"}),
        participant("part_bell_tomas", CombatantKind::NPC, "npc_tomas_reed", "", "Tomas Reed", 10, 38, 14, true, 9, 12, {"ghost", "possible-ally"})};
    bell.diceRollPresets = {dicePreset("roll_break", "Break the Bell", "1d20+5", "Strength, spell attack, or ritual precision."), dicePreset("roll_bind", "Bind the Bell", "1d20+5", "Religion, Arcana, or willing sacrifice."), dicePreset("roll_mute", "Mute the Bell", "1d20+5", "Thieves' tools, Arcana, or company rigging."), dicePreset("roll_crown", "Crown the Regent", "1d20", "Only if the table chooses the dark bargain.")};
    bell.afterCombatSummary = "Resolve with one of four ending tracks: Break, Bind, Mute, or Crown.";

    campaign.encounters = {opening, boat, marsh, sentries, casting, finale, bell};
}

void configureAudio(Campaign &campaign, const std::filesystem::path &root)
{
    for (const AudioSeed &seed : audioSeeds())
    {
        AudioAssetModel asset;
        setMetadata(asset.metadata, seed.id, Tags{});
        asset.metadata.tags = seed.tags;
        asset.name = seed.name;
        asset.originalFileName = std::filesystem::path(seed.sourcePath).filename().string();
        asset.originalSourcePath = seed.sourcePath;
        asset.filePath = projectPath(root, audioTargetPath(root, seed));
        asset.type = seed.type;
        asset.defaultVolume = seed.volume;
        asset.loop = seed.loop;
        asset.durationSeconds = seed.duration;
        asset.missingFile = !std::filesystem::exists(audioTargetPath(root, seed));
        campaign.audioAssets.push_back(asset);
    }

    auto playlist = [](const std::string &id, const std::string &name, std::vector<EntityId> ids, bool repeat, bool oneShot, float fade, Tags tags) {
        AudioPlaylistModel model;
        setMetadata(model.metadata, id, tags);
        model.name = name;
        model.audioAssetIds = std::move(ids);
        model.repeat = repeat;
        model.oneShot = oneShot;
        model.crossfadeSeconds = fade;
        return model;
    };
    campaign.audioPlaylists = {
        playlist("playlist_rainy_town_mystery", "Rainy Town Mystery", {"audio_mystery_exploration", "audio_rain_gutter", "audio_shadow_whisper"}, true, false, 1.5f, {"quick-start", "mystery"}),
        playlist("playlist_town_day_market", "Town Day / Market", {"audio_town_theme", "audio_old_tower_inn", "audio_coin_pouch", "audio_bottle_clink"}, true, false, 1.0f, {"quick-start", "town"}),
        playlist("playlist_drowned_mine_crawl", "Drowned Mine Crawl", {"audio_cave_explorer", "audio_forgotten_tomb", "audio_deep_sea", "audio_creaky_door", "audio_metal_ring"}, true, false, 2.0f, {"quick-start", "dungeon"}),
        playlist("playlist_combat", "Combat", {"audio_medieval_battle", "audio_orchestral_battle", "audio_unsheathe", "audio_weapon_light", "audio_victory"}, false, false, 0.5f, {"quick-start", "combat"}),
        playlist("playlist_ritual_boss", "Ritual Boss", {"audio_boss_battle", "audio_deep_rumble", "audio_arcane_pulse_bed", "audio_ancient_power", "audio_spell_deep_cast", "audio_low_impact"}, true, false, 2.5f, {"quick-start", "boss"}),
        playlist("playlist_break_recap", "Break / Recap", {"audio_old_tower_inn", "audio_field_of_dreams"}, true, false, 2.0f, {"break", "recap"})};

    auto layer = [](const std::string &id, const std::string &name, const std::string &asset, float volume, int order, bool active, Tags tags) {
        AudioAmbienceLayerModel model;
        setMetadata(model.metadata, id, tags);
        model.name = name;
        model.audioAssetId = asset;
        model.volume = volume;
        model.active = active;
        model.loop = true;
        model.sortOrder = order;
        return model;
    };
    campaign.audioAmbienceLayers = {
        layer("ambience_layer_rain", "Rain Bed", "audio_rain_gutter", 0.30f, 0, true, {"town", "active"}),
        layer("ambience_layer_tomb", "Drowned Stone", "audio_forgotten_tomb", 0.25f, 10, false, {"dungeon"}),
        layer("ambience_layer_bell_rumble", "Bell Rumble", "audio_deep_rumble", 0.28f, 20, false, {"finale"})};

    auto trigger = [](const std::string &id, const std::string &name, const std::string &sourceType, const std::string &sourceId, const std::string &asset, float volume, bool loop, Tags tags) {
        AudioTriggerModel model;
        setMetadata(model.metadata, id, tags);
        model.name = name;
        model.sourceType = sourceType;
        model.sourceEntityId = sourceId;
        model.eventName = "Scene Started";
        model.audioAssetId = asset;
        model.volume = volume;
        model.loop = loop;
        model.enabled = true;
        return model;
    };
    campaign.audioTriggers = {
        trigger("trigger_toll_rain", "Opening Rain", "StoryBeat", "beat_toll_silted_crown", "audio_rain_gutter", 0.30f, true, {"opening"}),
        trigger("trigger_memory_whisper", "Memory Whisper", "StoryBeat", "beat_witnesses_remember", "audio_shadow_whisper", 0.66f, false, {"clue"}),
        trigger("trigger_boathouse_danger", "Boathouse Ambush", "Encounter", "enc_boathouse_cutthroats", "audio_danger_stinger", 0.70f, false, {"combat"}),
        trigger("trigger_foundry_ritual", "Foundry Ritual Bed", "Encounter", "enc_midnight_casting", "audio_deep_rumble", 0.28f, true, {"ritual"}),
        trigger("trigger_bell_finale", "The Final Toll", "Encounter", "enc_mnemosyne_bell", "audio_boss_battle", 0.68f, true, {"finale"})};

    campaign.audioVolumeGroups.master = 0.9f;
    campaign.audioVolumeGroups.music = 0.78f;
    campaign.audioVolumeGroups.ambience = 0.68f;
    campaign.audioVolumeGroups.effects = 0.85f;
    campaign.audioVolumeGroups.voice = 1.0f;
}

void configureStory(Campaign &campaign)
{
    StoryThreadModel main;
    setMetadata(main.metadata, "story_bell_beneath_blackwater", {"main", "mystery", "phase3"});
    main.title = "The Bell Beneath Blackwater";
    main.type = StoryThreadType::MainStory;
    main.status = StoryStatus::Active;
    main.summary = "A drowned mining town hears an impossible bell at 2:13 AM, and every toll rewrites memory.";
    main.stakes = "If the final toll completes, Blackwater becomes a living memory overwrite zone and the Regent-in-Echo builds a body.";
    main.gmNotes = "Act I investigates the Hollow, Act II pressures factions, Act III enters the drowned mine and chooses Break, Bind, Mute, or Crown.";
    main.links = {
        storyLink("link_main_town", StoryLinkType::Contains, "map_blackwater_town", "Map", "main investigation map"),
        storyLink("link_main_session", StoryLinkType::Contains, "session_blackwater_one", "Session", "ready-to-run opening session"),
        storyLink("link_main_bell", StoryLinkType::Foreshadows, "power_mnemosyne_bell", "Power", "source of memory effects")};
    main.beats = {
        beat("beat_toll_silted_crown", "The Toll at The Silted Crown",
             "The first Bell toll cracks every glass in the inn and pulls the lake back from shore.",
             "Rain hammers the inn roof as midnight talk fades. At 2:13 AM, a bell tolls from beneath the lake.",
             "Every candle bends toward the water. Outside, the shoreline pulls away and leaves fresh footprints in the mud.",
             "H01 ledger; footprints are dead miners; Nessa draws the Bell chamber.",
             "Calming the room earns Mara's trust; panic makes the council clamp down sooner.",
             "Anyone failing the memory save loses a harmless recent memory for one hour and sees a white city.",
             StoryStatus::Active, "Day 1, 2:13 AM", "audio_rain_gutter", {"opening", "live"},
             {storyLink("link_beat_toll_map", StoryLinkType::Contains, "map_silted_crown", "Map", "opening map"),
              storyLink("link_beat_toll_handout", StoryLinkType::Reveals, "handout_h01_ledger", "Handout", "cracked inn ledger"),
              storyLink("link_beat_toll_next", StoryLinkType::BranchesTo, "beat_footprints_lake", "StoryBeat", "follow the footprints")}),
        beat("beat_footprints_lake", "Footprints into Blackwater Lake",
             "Impossible miner footprints and one modern diver trail lead toward the flooded mine.",
             "The exposed lakebed gleams under rain and lantern light.",
             "Dozens of bootprints cross the mud in a straight line into black water; some fill with blood instead of rain.",
             "Boots match forty-year-old miners; one fresh print belongs to a Gilded Lantern diver.",
             "Missed tracks return as a fisherman's impossible catch.",
             "Tomas Reed appears only in reflection unless called through the chapel bell.",
             StoryStatus::Seeded, "Day 1, early morning", "audio_deep_sea", {"lake", "clue"},
             {storyLink("link_beat_footprints_map", StoryLinkType::Contains, "map_blackwater_town", "Map", "lake edge"),
              storyLink("link_beat_footprints_npc", StoryLinkType::Foreshadows, "npc_tomas_reed", "NPC", "ghost foreman"),
              storyLink("link_beat_footprints_next", StoryLinkType::BranchesTo, "beat_lanterns_dock", "StoryBeat", "track the diver")}),
        beat("beat_witnesses_remember", "Witnesses Who Remember Tomorrow",
             "Inn patrons carry false memories of a white city and a sealed mine.",
             "The common room is crowded with shaken guests clutching broken cups.",
             "One man insists he has a wife he never married. Another weeps over a battlefield no one has heard of.",
             "Three visions: white city, Bell under water, councilors sealing a gate.",
             "One witness joins the Ash Choir if ignored.",
             "Let each PC volunteer a memory the Bell almost touches.",
             StoryStatus::Planned, "Day 1, morning", "audio_shadow_whisper", {"witness", "memory"},
             {storyLink("link_beat_witness_h03", StoryLinkType::Reveals, "handout_h03_drawing", "Handout", "Nessa's drawing"),
              storyLink("link_beat_witness_mara", StoryLinkType::Contains, "npc_mara_quill", "NPC", "innkeeper")}),
        beat("beat_council_curfew", "Curfew at Council Hall",
             "Magistrate Vale controls the town narrative and restricts evidence access.",
             "Council Hall glows with lamplight despite the hour.",
             "Constables block the doors while Magistrate Vale warns against reckless superstition.",
             "Vale knows the disaster notice is false; pressure grants archive access.",
             "Antagonizing Vale adds constables as watchers but reveals what she fears.",
             "Elowen is not the final villain; she is a cover-up vector.",
             StoryStatus::Planned, "Day 1, noon", "audio_town_theme", {"council", "faction"},
             {storyLink("link_beat_council_npc", StoryLinkType::Contains, "npc_magistrate_elowen_vale", "NPC", "council leader"),
              storyLink("link_beat_council_h04", StoryLinkType::Reveals, "handout_h04_disaster_notice", "Handout", "hidden archive proof")}),
        beat("beat_lanterns_dock", "Lanterns on the Dock",
             "Gilded Lantern salvage gear hints that the Company already knows what is under the lake.",
             "Brass diving helmets, sealed crates, and armed guards crowd the dock.",
             "A polished man with a silver-headed cane watches workers lower chained containers into a warehouse.",
             "Invoice H06; Rusk's cane fragment protects him; submersible rigging is real.",
             "If angered, company guards trail the party but expose a smuggling route.",
             "Rusk is useful, dangerous, and absolutely not neutral.",
             StoryStatus::Seeded, "Day 1, afternoon", "audio_mystery_exploration", {"company", "dock"},
             {storyLink("link_beat_dock_rusk", StoryLinkType::Contains, "npc_director_cassian_rusk", "NPC", "company director"),
              storyLink("link_beat_dock_invoice", StoryLinkType::Reveals, "handout_h06_invoice", "Handout", "coded shipping order"),
              storyLink("link_beat_dock_boat", StoryLinkType::BranchesTo, "enc_boathouse_cutthroats", "Encounter", "boathouse ambush")}),
        beat("beat_dry_bell_chapel", "The Dry Bell Chapel",
             "A chapel bell rings without a clapper and carries confessions from the dead.",
             "The old chapel leans over the lake on rotting beams.",
             "Its bell tower is empty of birds, wind, and clapper. Still, the bell gives one soft note as you enter.",
             "Tomas names the sealed third shaft; Brother Halven has recorded backward confessions.",
             "Refusing the ghosts makes them test the party in the mine.",
             "Truthful remembrance can turn the Remnant into allies.",
             StoryStatus::Planned, "Day 1, dusk", "audio_metal_ring", {"chapel", "ghost"},
             {storyLink("link_beat_chapel_halven", StoryLinkType::Contains, "npc_brother_halven", "NPC", "chapel keeper"),
              storyLink("link_beat_chapel_tomas", StoryLinkType::Reveals, "npc_tomas_reed", "NPC", "ghost foreman")}),
        beat("beat_ash_choir_rite", "The Ash Choir Comfort Rite",
             "Mother Sable comforts the memory-afflicted while preparing the Regent's awakening.",
             "Townsfolk gather beneath dripping awnings, each holding a gray candle.",
             "Mother Sable leads a hymn so soft it feels less sung than remembered.",
             "H05 acrostic spells WAKE THE REGENT; marked abductees can be protected.",
             "Interrupting the rite sends a cultist running to the bathhouse shrine.",
             "Mother Sable should feel useful before she becomes frightening.",
             StoryStatus::Seeded, "Day 1, night", "audio_field_of_dreams", {"cult", "public"},
             {storyLink("link_beat_choir_sable", StoryLinkType::Contains, "npc_mother_sable", "NPC", "cult leader"),
              storyLink("link_beat_choir_h05", StoryLinkType::Reveals, "handout_h05_hymn", "Handout", "hidden acrostic")}),
        beat("beat_archives_beneath_hall", "Archives Beneath Council Hall",
             "Records expose the mine massacre and the Bell chamber below the lake.",
             "The archive below Council Hall is colder than the street above.",
             "Shelves sag with ledgers, sealed testimonies, and maps stained by old water despite the dry stone floor.",
             "H02 mine map and H04 notice; disaster was deliberate flooding.",
             "If archive access fails, the Remnant projects records onto wet walls later.",
             "This is the cleanest midpoint reveal.",
             StoryStatus::Planned, "Day 2, morning", "audio_mystery_exploration", {"archive", "truth"},
             {storyLink("link_beat_archive_map", StoryLinkType::Reveals, "handout_h02_mine_map", "Handout", "old mine map"),
              storyLink("link_beat_archive_notice", StoryLinkType::Reveals, "handout_h04_disaster_notice", "Handout", "massacre proof")}),
        beat("beat_night_marked_doors", "Night of Marked Doors",
             "The party protects townsfolk marked for abduction by the Ash Choir.",
             "Curfew leaves Blackwater shuttered and breathless.",
             "Ash symbols darken on doors in the rain. From the alleys comes the sound of people singing with their mouths closed.",
             "Lost NPCs return as memory vessels; saved NPCs aid final preparations.",
             "The party cannot be everywhere; each choice changes the finale.",
             "Strong break point after this scene.",
             StoryStatus::Planned, "Day 2, night", "audio_rain_gutter", {"defense", "branching"},
             {storyLink("link_beat_marked_sable", StoryLinkType::Causes, "beat_before_eclipse", "StoryBeat", "sets final preparation state")}),
        beat("beat_flooded_mine_descent", "Descent Through the Flooded Mine",
             "The party enters the mine and navigates water, ghosts, company sabotage, and cult scouts.",
             "The mine descends beneath the lake in broken switchbacks.",
             "Lantern light scatters through flooded tunnels. Sometimes your reflections move a few seconds before you do.",
             "Alliance choices change the route: Company rig, Remnant guide, or independent ritual.",
             "If the party avoided the mine, sinkholes bring mine hazards to town.",
             "Run as dungeon rooms plus encounter inserts.",
             StoryStatus::Planned, "Day 3", "audio_cave_explorer", {"mine", "dungeon"},
             {storyLink("link_beat_mine_map", StoryLinkType::Contains, "map_bellfoundry_cellars", "Map", "dungeon route"),
              storyLink("link_beat_mine_enc", StoryLinkType::Contains, "enc_bellfoundry_sentries", "Encounter", "chain hall sentries")}),
        beat("beat_before_eclipse", "Before the Eclipse",
             "The party chooses allies, intervention, and session-end preparation.",
             "The eclipse will begin tonight.",
             "Across Blackwater Hollow, people write names on paper, skin, and doorframes. The lake is perfectly still.",
             "Break, Bind, Mute, Crown become explicit table options.",
             "Preparation lowers final DCs or changes boss allies.",
             "Best recap/resume point.",
             StoryStatus::Planned, "Day 4, dusk", "audio_field_of_dreams", {"prep", "session-break"},
             {storyLink("link_beat_eclipse_h08", StoryLinkType::Reveals, "handout_h08_final_toll", "Handout", "final intervention diagram"),
              storyLink("link_beat_eclipse_next", StoryLinkType::BranchesTo, "beat_antechamber_names", "StoryBeat", "enter final dungeon")}),
        beat("beat_antechamber_names", "Antechamber of Names",
             "Stolen names orbit the final door and each character anchors a defining memory.",
             "The tunnel opens into a circular chamber carved with thousands of names.",
             "Some names are ancient. Some belong to people in Blackwater. Some belong to you.",
             "Each PC anchors with item, ally, confession, or sacrifice.",
             "Failed anchors add a personal temptation to the Regent's opening move.",
             "Make PC portraits visible on the player display before this scene.",
             StoryStatus::Planned, "Day 4, eclipse", "audio_deep_rumble", {"pre-finale", "memory"},
             {storyLink("link_beat_names_finale", StoryLinkType::BranchesTo, "beat_mnemosyne_bell", "StoryBeat", "open final chamber")}),
        beat("beat_mnemosyne_bell", "The Mnemosyne Bell",
             "The party confronts the Regent-in-Echo and decides the Bell's fate.",
             "The final chamber is larger than the mine, larger than the lake, perhaps larger than memory.",
             "A bronze bell hangs over black water, its surface moving with faces. When it speaks, it uses voices you love.",
             "Resolve as combat, ritual challenge, negotiation, or hybrid.",
             "Break saves the town; Bind preserves knowledge; Mute delays; Crown transforms reality.",
             "Do not hide the four endings; the pressure is choosing, not guessing.",
             StoryStatus::Planned, "Day 4, final toll", "audio_boss_battle", {"finale", "boss"},
             {storyLink("link_beat_bell_enc", StoryLinkType::Contains, "enc_mnemosyne_bell", "Encounter", "final encounter"),
              storyLink("link_beat_bell_map", StoryLinkType::Contains, "map_mnemosyne_bell", "Map", "final map")})};

    StoryThreadModel factions;
    setMetadata(factions.metadata, "story_war_for_memory", {"faction", "branching"});
    factions.title = "The War for Memory";
    factions.type = StoryThreadType::FactionStory;
    factions.status = StoryStatus::Seeded;
    factions.summary = "Council secrecy, company exploitation, cult awakening, and ghost vengeance all compete for the same truth.";
    factions.stakes = "The party's allies decide which finale tools are reliable.";
    factions.links = {
        storyLink("link_faction_council", StoryLinkType::Contains, "faction_blackwater_council", "Faction", "suppression"),
        storyLink("link_faction_company", StoryLinkType::Contains, "faction_gilded_lantern", "Faction", "extraction"),
        storyLink("link_faction_choir", StoryLinkType::Contains, "faction_ash_choir", "Faction", "awakening"),
        storyLink("link_faction_remnant", StoryLinkType::Contains, "faction_drowned_remnant", "Faction", "remembrance")};

    StoryThreadModel personal;
    setMetadata(personal.metadata, "story_party_memory_hooks", {"character", "pc-hooks"});
    personal.title = "The Party's Stolen Edges";
    personal.type = StoryThreadType::CharacterStory;
    personal.status = StoryStatus::Seeded;
    personal.summary = "Every PC has a past that the Bell can tempt: Mira's report, Kael's oaths, Tamsin's identity, Elyon's prophecy, Nyra's erased village.";
    personal.stakes = "The Regent offers each character one restoration or rewrite.";
    personal.links = {
        storyLink("link_personal_mira", StoryLinkType::Contains, "pc_mira_voss", "Character", "sealed battlefield report"),
        storyLink("link_personal_kael", StoryLinkType::Contains, "pc_kael_thornevein", "Character", "forge oath"),
        storyLink("link_personal_tamsin", StoryLinkType::Contains, "pc_tamsin_reed", "Character", "stolen identity"),
        storyLink("link_personal_elyon", StoryLinkType::Contains, "pc_elyon_starling", "Character", "observatory exile"),
        storyLink("link_personal_nyra", StoryLinkType::Contains, "pc_nyra_moonbrook", "Character", "erased village")};

    campaign.storyThreads = {main, factions, personal};

    campaign.timelineEvents = {};
    auto event = [](const std::string &id, const std::string &title, const std::string &desc, const std::string &date, std::vector<EntityId> ids, Tags tags) {
        TimelineEventModel model;
        setMetadata(model.metadata, id, tags);
        model.title = title;
        model.description = desc;
        model.inWorldDate = date;
        model.relatedEntityIds = std::move(ids);
        return model;
    };
    campaign.timelineEvents = {
        event("timeline_first_toll", "First Toll", "Bell rings at 2:13 AM; lake recedes and memories fracture.", "Day 1, 2:13 AM", {"beat_toll_silted_crown", "handout_h01_ledger"}, {"opening"}),
        event("timeline_curfew", "Emergency Curfew", "Council attempts to control evidence and testimony.", "Day 1, noon", {"npc_magistrate_elowen_vale", "beat_council_curfew"}, {"council"}),
        event("timeline_choir_rite", "Ash Choir Comfort Rite", "Mother Sable marks future memory vessels.", "Day 1, night", {"npc_mother_sable", "handout_h05_hymn"}, {"cult"}),
        event("timeline_archive_truth", "The Flood Was Deliberate", "Archive records expose the massacre.", "Day 2, morning", {"handout_h04_disaster_notice", "story_war_for_memory"}, {"truth"}),
        event("timeline_marked_doors", "Night of Marked Doors", "The Choir abducts marked townsfolk unless stopped.", "Day 2, night", {"beat_night_marked_doors"}, {"branching"}),
        event("timeline_eclipse", "Final Toll Sequence", "The eclipse opens the drowned mine and final intervention path.", "Day 4, eclipse", {"beat_before_eclipse", "handout_h08_final_toll"}, {"finale"})};
}

void configureHandoutsShopsLootDungeonSessions(Campaign &campaign, const std::filesystem::path &root)
{
    auto hPath = [&](const std::string &file) { return projectPath(root, packageRoot(root) / "Assets" / "Handouts" / file); };
    campaign.handouts = {
        handout("handout_h01_ledger", "H01 - Cracked Inn Ledger", "Names are crossed out in their own handwriting. One impossible line remains.", hPath("h01_cracked_inn_ledger.txt"), "Give during the opening if the party stabilizes the inn.", {"player", "opening"}),
        handout("handout_h02_mine_map", "H02 - Old Mine Map", "A brittle map shows a hidden lower chamber marked by a bell symbol.", hPath("h02_old_mine_map.txt"), "Connects archive, chapel, and mine route.", {"player", "map"}),
        handout("handout_h03_drawing", "H03 - Child's Drawing", "A giant bell hangs above black water surrounded by faceless candle-bearers.", hPath("h03_childs_drawing.txt"), "Nessa's drawing grants advantage in the antechamber.", {"player", "vision"}),
        handout("handout_h04_disaster_notice", "H04 - Council Disaster Notice", "Official notice claims gas explosion; marginalia shows locked floodgates.", hPath("h04_council_disaster_notice.txt"), "Core truth of the massacre.", {"player", "truth"}),
        handout("handout_h05_hymn", "H05 - Ash Choir Hymn Sheet", "The first letter of each line spells WAKE THE REGENT.", hPath("h05_ash_choir_hymn.txt"), "Do not reveal acrostic unless examined.", {"player", "cult"}),
        handout("handout_h06_invoice", "H06 - Lantern Company Invoice", "Coded shipping order for Bell fragments and submersible rigging.", hPath("h06_lantern_company_invoice.txt"), "Proof Rusk is exploiting the disaster.", {"player", "company"}),
        handout("handout_h07_locket", "H07 - Waterlogged Locket", "Two portraits: one drowned miner and one living councilor, forty years apart.", hPath("h07_waterlogged_locket.txt"), "Personalizes the council bloodline.", {"player", "ghost"}),
        handout("handout_h08_final_toll", "H08 - Final Toll Diagram", "Four interventions: Break, Bind, Mute, or Crown.", hPath("h08_final_toll_diagram.txt"), "Reveal before the eclipse prep hub.", {"player", "finale"})};

    ShopModel wicks = shop("shop_wicks_wonders", "Wicks & Wonders", "Marda Vell", "Candles, lantern oil, rainproof cases, and hedge-mage services.", {"shop", "town"});
    wicks.stock = {stock("item_blue_glass_lantern", 3, 1.0, "Limited", false), stock("item_marsh_breath_potion", 4, 0.95, "Common", true), stock("item_miststep_chalk", 2, 1.15, "Rare", false), stock("item_healing_draught", 8, 0.9, "Common", true)};
    ShopModel morrow = shop("shop_morrows_relics", "Morrow's Relics", "Ser Morrow", "Night-market relic fence with one real clue and one convincing fake.", {"shop", "night-market", "occult"});
    morrow.stock = {stock("item_saint_orwyn_medal", 1, 1.3, "Rare", false), stock("item_acolyte_charm", 3, 1.0, "Limited", false), stock("item_bell_clapper_token", 6, 1.0, "Token", true), stock("item_rusk_cane_fragment", 1, 2.0, "Story Locked", false)};
    campaign.shops = {wicks, morrow};

    LootParcelModel boat = loot("loot_boathouse_cache", "Boathouse Cache", "Old Boathouse Cutthroats", "Reward for saving the ledger before it burns.", {"loot", "combat"});
    boat.gold = 35;
    boat.items = {lootItem("item_silvered_canal_knife", 1, "Recovered from a smuggler."), lootItem("item_coded_ledger_scrap", 1, "Dry before reading initials.")};
    LootParcelModel foundry = loot("loot_foundry_ritual", "Foundry Ritual Cache", "Bellfoundry Cellars", "Award after disrupting Ilyr's circle.", {"loot", "ritual"});
    foundry.gold = 80;
    foundry.items = {lootItem("item_foundry_key", 1, "Opens kiln and cellar gates."), lootItem("item_moon_glass_shard", 1, "Reveals hidden blood traces."), lootItem("item_acolyte_charm", 2, "Cult charms from sentries.")};
    LootParcelModel finale = loot("loot_final_bell_choice", "Final Bell Consequence", "The Mnemosyne Bell", "Use the parcel only if the Bell is contained, muted, or bound.", {"loot", "finale"});
    finale.platinum = 2;
    finale.gold = 400;
    finale.items = {lootItem("item_moon_glass_reliquary", 1, "Contains one shade or cursed memory."), lootItem("item_miner_oath_token", 1, "Proof of remembrance pact.")};
    campaign.lootParcels = {boat, foundry, finale};

    DungeonModel dungeon;
    setMetadata(dungeon.metadata, "dungeon_flooded_mine", {"dungeon", "mine", "bell"});
    dungeon.name = "Flooded Mine and Bellfoundry";
    dungeon.description = "Room-by-room route from the old mine road into foundry cellars and the Bell antechamber.";
    dungeon.parentMapId = "map_bellfoundry_cellars";
    dungeon.mapIds = {"map_bellfoundry_cellars", "map_mnemosyne_bell"};
    dungeon.activeRoomId = "room_coal_ramp";
    dungeon.gmNotes = "Use room states to model player progress; each alliance changes one room hazard.";
    DungeonRoomModel coal = room("room_coal_ramp", "Coal Ramp", "Collapsed delivery chute and black dust tracks.", "Cold air breathes up the ramp with the smell of wet ash.", "Fresh bootprints belong to Company scouts.", 2, 3, DungeonRoomExplorationState::Seen, {"entry", "clue"});
    DungeonRoomModel channel = room("room_cooling_channel", "Cooling Channel", "Knee-deep water, iron grates, and echoing bells.", "Your reflections move a few seconds before you do.", "Foundry key under grate, DC 13 Investigation.", 8, 6, DungeonRoomExplorationState::Unknown, {"water", "key"});
    DungeonRoomModel chain = room("room_chain_hall", "Chain Hall", "Hanging chains and distorted sound.", "The chains sway though no wind reaches this far.", "Alarm bell brings prepared defenders.", 15, 7, DungeonRoomExplorationState::Unknown, {"encounter"});
    chain.encounterIds = {"enc_bellfoundry_sentries"};
    DungeonRoomModel kiln = room("room_cracked_kiln", "Cracked Kiln", "Secret kiln door and warm ash.", "A line of heat glows in the soot like an opening eye.", "Secret door to ritual notes.", 23, 11, DungeonRoomExplorationState::Unknown, {"secret"});
    DungeonRoomModel pit = room("room_casting_pit", "Casting Pit", "Molten residue and a bell with no clapper.", "A masked caster raises a hammer and the whole room answers.", "Three silver nails sustain Ilyr.", 30, 20, DungeonRoomExplorationState::Unknown, {"boss"});
    pit.encounterIds = {"enc_midnight_casting"};
    coal.connections = {roomConnection("conn_coal_channel", "room_cooling_channel", "Flooded ramp")};
    channel.connections = {roomConnection("conn_channel_chain", "room_chain_hall", "Iron grate route", true), roomConnection("conn_channel_kiln", "room_cracked_kiln", "Hidden warm current", false, true, false)};
    chain.connections = {roomConnection("conn_chain_pit", "room_casting_pit", "Ringing stair")};
    kiln.connections = {roomConnection("conn_kiln_pit", "room_casting_pit", "Secret kiln passage", false, true, false)};
    dungeon.rooms = {coal, channel, chain, kiln, pit};
    campaign.dungeons = {dungeon};

    SessionModel session;
    setMetadata(session.metadata, "session_blackwater_one", {"session", "ready", "live"});
    session.title = "Session 1: The Hollow Rings";
    session.summary = "Ready-to-run opening session: Silted Crown toll, witness investigation, lake footprints, council pressure, and first faction leads.";
    session.recap = "Not started. Begin at 2:12 AM in the Silted Crown.";
    session.unresolvedThreads = "Who rang the Bell? Why did the council flood the mine? What does Rusk already know?";
    session.lootAwarded = "None yet. Boathouse Cache is the first likely parcel.";
    session.xpMilestones = "Milestone after archive truth or Bellfoundry disruption.";
    session.npcsMet = "Mara Quill;Nessa Vale;Joric Pike;Magistrate Elowen Vale;";
    session.locationsVisited = "The Silted Crown;Blackwater Lake edge;Council Hall;";
    session.openHooks = "Ash Choir rite;Gilded Lantern docks;Dry Bell Chapel;";
    session.participantCharacterIds = {"pc_mira_voss", "pc_kael_thornevein", "pc_tamsin_reed", "pc_elyon_starling", "pc_nyra_moonbrook"};
    session.encounterIds = {"enc_silted_crown_opening", "enc_boathouse_cutthroats"};
    session.storyDocumentIds = {"doc_blackwater_runbook", "doc_blackwater_clue_chain"};
    SessionNoteModel note1;
    setMetadata(note1.metadata, "note_start_rain", {"cue"});
    note1.timestampUtc = currentUtcTimestamp();
    note1.body = "Start Rainy Town Mystery playlist, then read the opening scene at 2:12 AM.";
    note1.linkedEntityId = "playlist_rainy_town_mystery";
    note1.linkedEntityType = "AudioPlaylist";
    note1.linkedEntityLabel = "Rainy Town Mystery";
    SessionNoteModel note2;
    setMetadata(note2.metadata, "note_player_safe_text", {"player-safe"});
    note2.timestampUtc = currentUtcTimestamp();
    note2.body = "Launch The Toll at The Silted Crown to stage public scene text without revealing the massacre secret.";
    note2.linkedEntityId = "beat_toll_silted_crown";
    note2.linkedEntityType = "StoryBeat";
    note2.linkedEntityLabel = "The Toll at The Silted Crown";
    SessionNoteModel note3;
    setMetadata(note3.metadata, "note_break_resume", {"break", "recap"});
    note3.timestampUtc = currentUtcTimestamp();
    note3.body = "Best break: after Council Hall. Resume with: 'Last time, the Bell rang beneath Blackwater Lake and the council tried to close ranks.'";
    note3.linkedEntityId = "beat_council_curfew";
    note3.linkedEntityType = "StoryBeat";
    note3.linkedEntityLabel = "Curfew at Council Hall";
    session.notes = {note1, note2, note3};
    campaign.sessions = {session};
}

void configureCampaignExtras(Campaign &campaign)
{
    campaign.partyInventory.name = "Party Evidence and Supplies";
    campaign.partyInventory.ownerEntityId = kCampaignId;
    campaign.partyInventory.gold = 72;
    campaign.partyInventory.silver = 40;
    campaign.partyInventory.entries = {
        makeEntry("party_inv_writ", "item_watch_writ", 1, false, "Evidence Folder"),
        makeEntry("party_inv_lantern", "item_blue_glass_lantern", 1, false, "Shared Gear"),
        makeEntry("party_inv_draughts", "item_healing_draught", 5, false, "Medical Pouch"),
        makeEntry("party_inv_mist_chalk", "item_miststep_chalk", 2, false, "Ritual Kit")};
    campaign.partyVehicles = {"Borrowed lake skiff", "Gilded Lantern diving bell (conditional)"};
    campaign.partyStrongholds = {"The Silted Crown back room", "Lake Chapel safe room"};
    campaign.partyRenown = 12;
    campaign.partyReputation = "Known as capable outsiders who stayed calm when the first impossible bell rang.";

    RulesetDefinitionModel overlay;
    setMetadata(overlay.metadata, "ruleset_blackwater_memory_overlay", {"ruleset", "derived-stats", "campaign"});
    overlay.name = "Blackwater Memory Overlay";
    overlay.version = "1.0";
    overlay.baseRulesetId = "dnd-2024-5.5e";
    overlay.installedGlobally = true;
    overlay.definitionJson = "{\"derivedStats\":[{\"id\":\"memory_anchor\",\"name\":\"Memory Anchor\",\"formula\":\"Wisdom + proficiency\"},{\"id\":\"bell_pressure\",\"name\":\"Bell Pressure\",\"formula\":\"round + failed_saves\"}],\"notes\":\"Campaign QA overlay for derived stat preview and validation.\"}";
    campaign.rulesetDefinitions = {overlay};

    RulesetContentPackModel pack;
    setMetadata(pack.metadata, "pack_blackwater_relics", {"homebrew", "content-pack"});
    pack.rulesetId = "ruleset_blackwater_memory_overlay";
    pack.name = "Blackwater Relics and Rituals";
    pack.version = "1.0";
    pack.author = "Codex Phase 3 Seed";
    pack.license = "Project-local original content";
    pack.importedAtUtc = currentUtcTimestamp();
    pack.source = "tools/seed_blackwater_campaign.cpp";
    pack.definitionJson = "{\"items\":12,\"spells\":6,\"factions\":5,\"purpose\":\"comprehensive Phase 3 smoke campaign\"}";
    campaign.rulesetContentPacks = {pack};

    HouseRuleModel memory;
    setMetadata(memory.metadata, "house_rule_memory_anchors", {"memory", "finale"});
    memory.label = "Memory Anchors";
    memory.description = "Before entering the final chamber, each character anchors one defining memory with an item, ally, confession, or sacrifice. Unanchored characters face a harder Regent temptation.";
    memory.changeHistory = {{"2026-05-24T00:00:00Z", "Added for Blackwater finale branch testing."}};
    HouseRuleModel clues;
    setMetadata(clues.metadata, "house_rule_fail_forward_clues", {"mystery", "qa"});
    clues.label = "Fail-Forward Clues";
    clues.description = "A missed core clue reappears through a haunting, dropped evidence, public rite, or memory-echo NPC rather than stopping the investigation.";
    campaign.houseRules = {memory, clues};

    CampaignSavedFilterModel unresolved;
    setMetadata(unresolved.metadata, "filter_unresolved_clues", {"search", "smart"});
    unresolved.name = "Unresolved Clues";
    unresolved.query = "clue OR handout OR secret";
    unresolved.entityTypes = {"Story", "Handout", "Item", "NPC"};
    unresolved.requiredTags = {"clue"};
    unresolved.matchAllTags = false;
    CampaignSavedFilterModel live;
    setMetadata(live.metadata, "filter_live_ready", {"search", "smart"});
    live.name = "Live-Ready Scenes";
    live.query = "player-safe OR live OR session";
    live.entityTypes = {"Story", "Encounter", "Map", "Audio"};
    live.requiredTags = {"live"};
    live.matchAllTags = false;
    campaign.savedFilters = {unresolved, live};

    auto bookmark = [](const std::string &id, const std::string &entityId, const std::string &type, const std::string &label, bool favorite, int count) {
        CampaignEntityBookmarkModel model;
        setMetadata(model.metadata, id, {"bookmark"});
        model.entityId = entityId;
        model.entityType = type;
        model.label = label;
        model.favorite = favorite;
        model.lastOpenedAtUtc = currentUtcTimestamp();
        model.openCount = count;
        return model;
    };
    campaign.entityBookmarks = {
        bookmark("bookmark_home_scene", "beat_toll_silted_crown", "StoryBeat", "The Toll at The Silted Crown", true, 7),
        bookmark("bookmark_main_map", "map_blackwater_town", "Map", "Blackwater Town Ward", true, 5),
        bookmark("bookmark_finale", "beat_before_eclipse", "StoryBeat", "Before the Eclipse", true, 4),
        bookmark("bookmark_audio_mystery", "playlist_rainy_town_mystery", "AudioPlaylist", "Rainy Town Mystery", false, 3)};

    campaign.localPlayerDisplay.enabled = true;
    setMetadata(campaign.localPlayerDisplay.metadata, "display_blackwater_public", {"public", "player-safe"});
    campaign.localPlayerDisplay.publicSceneId = "beat_toll_silted_crown";
    campaign.localPlayerDisplay.publicMapId = "map_blackwater_town";
    campaign.localPlayerDisplay.publicEncounterId = "enc_silted_crown_opening";
    campaign.localPlayerDisplay.visibleNpcIds = {"npc_mara_quill", "npc_nessa_vale", "npc_joric_pike"};
    campaign.localPlayerDisplay.publicHandoutIds = {"handout_h01_ledger", "handout_h03_drawing"};
    campaign.localPlayerDisplay.sharedArtAssetIds = {"pc_mira_voss", "pc_kael_thornevein", "pc_tamsin_reed", "pc_elyon_starling", "pc_nyra_moonbrook"};
    campaign.presentationModeActive = false;
}

Campaign buildCampaign(const std::filesystem::path &root)
{
    Campaign campaign;
    setMetadata(campaign.metadata, kCampaignId, {"phase3", "comprehensive", "clean-slate", "mystery", "audio", "maps", "portraits"});
    campaign.name = kCampaignName;
    campaign.description = "A comprehensive Phase 3 QA campaign: five generated-portrait PCs investigate an impossible submerged bell, faction pressure, memory magic, maps, encounters, audio, handouts, shops, loot, dungeon rooms, story branching, timeline, sessions, public display state, filters, and bookmarks.";
    campaign.ruleSet = std::make_shared<DND5e>();
    campaign.customItems = buildItems();
    campaign.customSpells = buildSpells();
    configureCharacters(campaign, root);
    configureNpcsAndFactions(campaign);
    configureMaps(campaign, root);
    configureEncounters(campaign);
    configureAudio(campaign, root);
    configureStory(campaign);
    configureHandoutsShopsLootDungeonSessions(campaign, root);
    configureCampaignExtras(campaign);
    return campaign;
}
}

int main(int argc, char **argv)
{
    try
    {
        bool clean = false;
        for (int index = 1; index < argc; ++index)
        {
            const std::string arg = argv[index];
            if (arg == "--clean")
                clean = true;
            else
                throw std::runtime_error("Usage: seed_blackwater_campaign [--clean]");
        }

        const std::filesystem::path root = findProjectRoot();
        const std::filesystem::path portraitCache = clean ? cacheExistingPortraits(root) : std::filesystem::path{};
        if (clean)
            cleanSlate(root);

        createProjectAssets(root);
        restoreCachedPortraits(root, portraitCache);
        requirePortraits(root);
        Campaign campaign = buildCampaign(root);

        const std::string databasePath = (root / "TTRPG.db").string();
#if defined(_WIN32)
        _putenv_s("TTRPG_DB_PATH", databasePath.c_str());
#else
        setenv("TTRPG_DB_PATH", databasePath.c_str(), 1);
#endif

        ProjectSaveService &saveService = ProjectSaveService::getInstance();
        saveService.setBackupSnapshotsEnabled(false);
        saveService.setRecoverySnapshotsEnabled(false);
        if (!saveService.saveCampaign(campaign, "seed Blackwater clean-slate campaign"))
            throw std::runtime_error("Campaign save failed: " + saveService.state().lastError);

        addPostSaveMetadata(root);
        std::cout << "Seeded " << kCampaignName << " into " << (root / "TTRPG.db").string() << "\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
