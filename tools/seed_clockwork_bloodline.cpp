#include "Models/Campaign/Campaign.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/Clockwork/ClockworkBloodline.hpp"
#include "Models/Rulesets/RulesetDefinitionValidator.hpp"
#include "sqlite3.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
constexpr const char *kCampaignId = "campaign_clockwork_bloodline";
constexpr const char *kCampaignName = "Clockwork Bloodline";
constexpr const char *kRulesetId = "clockwork-bloodline-cinder-vow-1.0";
constexpr const char *kRulesetStudioId = "clockwork-bloodline";
constexpr const char *kRulesetStudioDraftId = "clockwork-bloodline@draft";

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

void setMetadata(EntityMetadata &metadata, const std::string &id, const std::vector<std::string> &tags)
{
    metadata.id = id;
    metadata.tags = tags;
    metadata.createdAtUtc = currentUtcTimestamp();
    metadata.updatedAtUtc = metadata.createdAtUtc;
}

RulesetDefinitionModel buildRulesetStudioDraft()
{
    RulesetDefinitionModel definition;
    setMetadata(definition.metadata, kRulesetStudioDraftId, {"ruleset", "homebrew", "clockwork", "showcase"});
    definition.rulesetId = kRulesetStudioId;
    definition.name = "Clockwork Bloodline: Cinder Vow Engine";
    definition.version = "1.0.0";
    definition.baseRulesetId = "original";
    definition.abilityModifierFormula = "score";
    definition.proficiencyFormula = "0";
    definition.checkDiceFormula = "1d6";
    definition.passiveScoreBase = 0;
    definition.installedGlobally = true;

    definition.abilityScores = {
        {"iron", "Iron", "IRN", "Force, endurance, and physical resolve."},
        {"spark", "Spark", "SPK", "Engineering, invention, and precise repair."},
        {"veil", "Veil", "VEL", "Stealth, perception, and careful aim."},
        {"grace", "Grace", "GRC", "Charm, command, and social poise."},
        {"lore", "Lore", "LOR", "Research, history, and occult theory."},
        {"pulse", "Pulse", "PLS", "Empathy, intuition, and aetheric attunement."}};
    definition.skills = {
        {"fight", "Fight", "iron", "Close combat and direct force."},
        {"shoot", "Shoot", "veil", "Ranged attacks and careful shots."},
        {"tinker", "Tinker", "spark", "Build, repair, or sabotage machinery."},
        {"pilot", "Pilot", "spark", "Control vehicles and complex mechanisms."},
        {"skulk", "Skulk", "veil", "Move unseen and bypass attention."},
        {"study", "Study", "lore", "Research evidence and recall specialist knowledge."},
        {"treat", "Treat", "spark", "Stabilize injuries and provide medical care."},
        {"command", "Command", "grace", "Lead, organize, and compel."},
        {"charm", "Charm", "grace", "Persuade and build rapport."},
        {"attune", "Attune", "pulse", "Sense and shape occult resonance."},
        {"notice", "Notice", "veil", "Spot danger, clues, and concealed details."},
        {"survive", "Survive", "iron", "Endure hazards and navigate hostile ground."}};
    definition.derivedStats = {
        {"guard", "Guard", "6 + iron", "Resistance to immediate physical danger."},
        {"resolve", "Resolve", "6 + pulse", "Resistance to fear, pressure, and occult influence."}};
    definition.resources = {
        {"momentum", "Momentum", "6", "6", "Session Start", "Shared leverage earned through decisive action."},
        {"stress", "Stress", "6 + iron", "0", "Downtime", "Accumulated pressure and exhaustion."},
        {"injury", "Injury", "4", "0", "Treatment", "Lasting physical harm."},
        {"favor", "Favor", "5", "0", "Milestone", "Divine or factional goodwill."},
        {"stain", "Stain", "5", "0", "Cleansing", "Occult corruption and visible residue."},
        {"ethertech_charges", "Ethertech Charges", "3 + spark", "3 + spark", "Downtime", "Power available to ethertech devices."}};
    definition.diceFormulas = {
        {"action_die", "Action die", "1d6", "Each die showing 5 or 6 is a Hit; each 6 is also a Spark."},
        {"fortune_die", "Fortune die", "1d6", "An extra die granted by a strong advantage or useful asset."}};
    definition.currencies = {
        {"bit", "Bit", "bit", 1, "Common street coin."},
        {"mark", "Mark", "mk", 10, "Everyday trade denomination."},
        {"crown", "Crown", "cr", 100, "High-value civic currency."},
        {"sovereign", "Sovereign", "sv", 1000, "Rare treasury denomination."}};
    definition.sizes = {
        {"small", "Small", 5, "Compact people and creatures."},
        {"medium", "Medium", 5, "Human-scale people and creatures."},
        {"large", "Large", 10, "Large constructs and creatures."}};
    definition.speeds = {
        {"ground", "Ground", "feet", "Ordinary movement on foot."}};
    definition.conditions = {
        {"overheated", "Overheated", "Ethertech is hot, unstable, or venting pressure.", {"-1 die to delicate Spark rolls.", "Overclocking risks a malfunction on 1-2."}, "Clear with a safe repair action."},
        {"oathbound", "Oathbound", "A vow or divine contract constrains the character.", {"Breaking the oath marks Stain.", "Honoring it can gain Favor."}, "Fulfill, renegotiate, or ritually sever the vow."},
        {"stained", "Stained", "Occult residue is visible to sensitive observers.", {"Hostile occult forces can track the mark.", "Attune rolls may draw attention."}, "Cleanse through downtime, confession, or sacrifice."},
        {"wounded", "Wounded", "An injury is affecting action.", {"Apply the injury penalty recorded on the character."}, "Treat with medicine, rest, or a miracle."}};
    definition.classes = {
        {"heartwright", "Heartwright", "d8", {"pulse", "spark"}, "A healer who combines medicine and resonant craft."},
        {"smoke_talker", "Smoke-Talker", "d6", {"veil", "pulse"}, "An occult messenger who reads secrets in smoke."},
        {"debt_medium", "Debt Medium", "d6", {"lore", "pulse"}, "A negotiator with spirits, vows, and inherited debts."},
        {"engine_knight", "Engine Knight", "d10", {"iron", "spark"}, "An armored guardian bonded to dangerous machinery."},
        {"gearwright", "Gearwright", "d8", {"spark", "lore"}, "An inventor who turns tools and assets into leverage."},
        {"brass_duelist", "Brass Duelist", "d8", {"veil", "grace"}, "A precise fighter who controls tempo and position."},
        {"union_agitator", "Union Agitator", "d8", {"grace", "iron"}, "An organizer who converts solidarity into action."},
        {"relic_scholar", "Relic Scholar", "d6", {"lore", "veil"}, "An expert in forbidden history and aetheric artifacts."}};
    definition.ancestries = {
        {"human", "Human", "medium", "ground", "Ordinary people shaped by community and circumstance."},
        {"clockmarked", "Clockmarked", "medium", "ground", "People bearing visible traces of civic clockwork."},
        {"relic_bound", "Relic-Bound", "medium", "ground", "People permanently linked to an old device or oath."},
        {"smoke_touched", "Smoke-Touched", "medium", "ground", "People changed by aetheric smoke and its voices."},
        {"debt_bound", "Debt-Bound", "medium", "ground", "People carrying inherited supernatural obligations."}};
    definition.backgrounds = {
        {"guild_apprentice", "Guild Apprentice", {"tinker", "study"}, "Trained in a regulated craft and its politics."},
        {"canal_runner", "Canal Runner", {"pilot", "notice"}, "Knows the waterways, markets, and hidden routes."},
        {"ward_survivor", "Ward Survivor", {"survive", "skulk"}, "Learned to endure a dangerous industrial district."}};
    definition.equipmentCategories = {
        {"body", "Body", "body", "Armor and protective clothing."},
        {"main_hand", "Main Hand", "main-hand", "Weapons and primary tools."},
        {"off_hand", "Off Hand", "off-hand", "Shields, sidearms, and secondary tools."},
        {"tool_rig", "Tool Rig", "tool", "Specialized engineering or medical equipment."},
        {"relic", "Relic", "relic", "Aetheric and occult artifacts."},
        {"pack", "Pack", "pack", "Carried supplies and evidence."}};
    definition.spellTraditions = {
        {"aethercraft", "Aethercraft", "spark", "Technical manipulation of charged aether."},
        {"smoke_rites", "Smoke Rites", "pulse", "Intuitive communion through smoke and memory."},
        {"debt_lore", "Debt Lore", "lore", "Ritual practice built on names, contracts, and obligation."}};
    return definition;
}

void installAndVerifyRulesetStudioDraft()
{
    const RulesetDefinitionModel draft = buildRulesetStudioDraft();
    const std::vector<RulesetValidationDiagnostic> diagnostics = validateRulesetDefinition(draft);
    if (!diagnostics.empty())
        throw std::runtime_error("Clockwork Ruleset Studio fixture is invalid: " + diagnostics.front().path + " - " + diagnostics.front().message);

    CampaignRepository::saveInstalledRulesetDefinition(draft);
    const std::vector<RulesetDefinitionModel> installed = CampaignRepository::loadInstalledRulesetDefinitions();
    const auto found = std::find_if(installed.begin(), installed.end(), [](const RulesetDefinitionModel &definition) {
        return definition.metadata.id == kRulesetStudioDraftId;
    });
    if (found == installed.end() || !found->installedGlobally || found->published ||
        found->rulesetId != kRulesetStudioId || found->abilityScores.size() != 6 || found->skills.size() != 12)
    {
        throw std::runtime_error("Clockwork Ruleset Studio draft was not persisted as an installed global draft.");
    }
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

std::filesystem::path packageRoot(const std::filesystem::path &root)
{
    return root / "Resources" / "Campaigns" / kCampaignId;
}

std::filesystem::path portraitRoot(const std::filesystem::path &root)
{
    return packageRoot(root) / "Assets" / "Portraits";
}

std::vector<std::string> portraitFiles()
{
    return {"pc_iona_vale.png", "pc_corvin_rake.png", "pc_mirelle_voss.png", "pc_tomas_arkwright.png", "clockwork_portrait_contact_sheet.png"};
}

std::filesystem::path cacheExistingPortraits(const std::filesystem::path &root)
{
    const std::filesystem::path sourceRoot = portraitRoot(root);
    const std::filesystem::path cacheRoot = root / "build" / "seed_clockwork_portrait_cache";
    bool copiedAny = false;

    removePathInsideRoot(root, cacheRoot);
    for (const std::string &file : portraitFiles())
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

    for (const std::string &file : portraitFiles())
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
    for (const std::string &file : {"pc_iona_vale.png", "pc_corvin_rake.png", "pc_mirelle_voss.png", "pc_tomas_arkwright.png"})
    {
        const std::filesystem::path path = portraitRoot(root) / file;
        if (!std::filesystem::exists(path))
            throw std::runtime_error("Missing generated portrait: " + path.string());
    }
}

void cleanSlate(const std::filesystem::path &root)
{
    removePathInsideRoot(root, root / "TTRPG.db");
    removePathInsideRoot(root, root / "TTRPG.db-wal");
    removePathInsideRoot(root, root / "TTRPG.db-shm");
    removePathInsideRoot(root, packageRoot(root) / "Assets");
    removePathInsideRoot(root, packageRoot(root) / "Layouts");
    removePathInsideRoot(root, packageRoot(root) / "README.md");
    removePathInsideRoot(root, packageRoot(root) / "project.json");
    removePathInsideRoot(root, packageRoot(root) / "campaign.db");
    removePathInsideRoot(root, root / "Resources" / "Backups" / kCampaignId);
    removePathInsideRoot(root, root / "Resources" / "Recovery" / kCampaignId);
    std::filesystem::create_directories(packageRoot(root));
    std::filesystem::create_directories(root / "Resources" / "Backups");
    std::filesystem::create_directories(root / "Resources" / "Recovery");
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
            const bool rail = ((x + motif * 31) % 150) < 6 || ((y + motif * 17) % 118) < 5;
            const bool brass = ((x / 70 + y / 58 + motif) % 4) == 0;
            const bool water = motif % 3 == 0 && y > height * 2 / 3 && ((x + y) % 19) < 8;
            const bool hazard = ((x * 5 + y * 3 + motif) % 211) < 5;
            std::uint8_t rr = static_cast<std::uint8_t>(std::clamp<int>(r + (frame ? 34 : 0) + (brass ? 26 : 0) + (hazard ? 70 : 0), 0, 255));
            std::uint8_t gg = static_cast<std::uint8_t>(std::clamp<int>(g + (rail ? 44 : 0) + (water ? 18 : 0), 0, 255));
            std::uint8_t bb = static_cast<std::uint8_t>(std::clamp<int>(b + (water ? 64 : 0) + (brass ? 10 : 0), 0, 255));
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
        {"audio_market_day", "Gearmarket Day", "Resources/Audio/Music/market_day_loop.mp3", AudioAssetType::Music, 0.48f, true, 96, {"music", "market", "city"}},
        {"audio_old_tower_inn", "Porcelain Salon", "Resources/Audio/Music/old_tower_inn.mp3", AudioAssetType::Music, 0.44f, true, 102, {"music", "salon", "social"}},
        {"audio_mystery", "Patent Office Mystery", "Resources/Audio/Music/mystery_exploration.mp3", AudioAssetType::Music, 0.50f, true, 104, {"music", "mystery", "investigation"}},
        {"audio_scifi_urgent", "Pressure Alarm Pulse", "Resources/Audio/Music/scifi_urgent.mp3", AudioAssetType::Music, 0.56f, true, 91, {"music", "chase", "machinery"}},
        {"audio_medieval_battle", "Foundry Violence", "Resources/Audio/Music/medieval_battle.mp3", AudioAssetType::Music, 0.66f, false, 95, {"music", "combat"}},
        {"audio_orchestral_battle", "Clockmaker Duel", "Resources/Audio/Music/orchestral_battle_theme.mp3", AudioAssetType::Music, 0.68f, false, 104, {"music", "duel"}},
        {"audio_fighting_not_option", "Genealogical Machine Finale", "Resources/Audio/Music/fighting_is_not_an_option.mp3", AudioAssetType::Music, 0.70f, true, 115, {"music", "finale", "boss"}},
        {"audio_cave_explorer", "Submerged Chapel", "Resources/Audio/Music/cave_explorer.mp3", AudioAssetType::Music, 0.46f, true, 95, {"music", "chapel", "underground"}},
        {"audio_ancient_power", "Sovereign Engine Awakens", "Resources/Audio/Music/ancient_power_of_serpents.mp3", AudioAssetType::Music, 0.60f, false, 117, {"music", "ritual"}},
        {"audio_victory", "Hard-Won Dawn", "Resources/Audio/Music/medieval_victory.mp3", AudioAssetType::Music, 0.72f, false, 45, {"music", "ending"}},
        {"audio_rain", "Rain In The Gutter", "Resources/Audio/Ambience/rain_gutter_loop.mp3", AudioAssetType::Ambience, 0.34f, true, 80, {"ambience", "rain"}},
        {"audio_deep_rumble", "Municipal Deep Rumble", "Resources/Audio/Ambience/deep_rumble.mp3", AudioAssetType::Ambience, 0.30f, true, 84, {"ambience", "engine"}},
        {"audio_underwater_engine", "Underwater Engine", "Resources/Audio/Ambience/underwater_engine.mp3", AudioAssetType::Ambience, 0.28f, true, 86, {"ambience", "engine", "pressure"}},
        {"audio_deep_sea", "Canal Depths", "Resources/Audio/Ambience/deep_sea.mp3", AudioAssetType::Ambience, 0.32f, true, 90, {"ambience", "canal"}},
        {"audio_forgotten_tomb", "Cold Ward", "Resources/Audio/Ambience/forgotten_tomb_ambience.mp3", AudioAssetType::Ambience, 0.24f, true, 82, {"ambience", "clinic"}},
        {"audio_scifi_pulse", "Pneumatic Pulse", "Resources/Audio/Ambience/scifi_pulse.mp3", AudioAssetType::Ambience, 0.30f, true, 85, {"ambience", "machine"}},
        {"audio_short_alert", "Customs Whistle", "Resources/Audio/Effects/misc_short_alert.wav", AudioAssetType::Effect, 0.72f, false, 2, {"effect", "alert"}},
        {"audio_low_impact", "Cable Strike", "Resources/Audio/Effects/misc_low_impact.wav", AudioAssetType::Effect, 0.70f, false, 2, {"effect", "impact"}},
        {"audio_metal_ring", "Ringing Metal", "Resources/Audio/Effects/inventory_metal_ring.wav", AudioAssetType::Effect, 0.70f, false, 2, {"effect", "metal"}},
        {"audio_bottle_clink", "Clinic Bottles", "Resources/Audio/Effects/inventory_bottle_clink.wav", AudioAssetType::Effect, 0.70f, false, 2, {"effect", "clinic"}},
        {"audio_coin_pouch", "Debt Coins", "Resources/Audio/Effects/inventory_coin_pouch.wav", AudioAssetType::Effect, 0.70f, false, 2, {"effect", "social"}},
        {"audio_shadow_whisper", "Ledger Whisper", "Resources/Audio/Effects/npc_shadow_whisper.wav", AudioAssetType::Effect, 0.70f, false, 3, {"effect", "occult"}},
        {"audio_deep_cast", "Deep Petition", "Resources/Audio/Effects/spell_deep_cast.wav", AudioAssetType::Effect, 0.75f, false, 3, {"effect", "divine"}},
        {"audio_arcane_pulse", "Arcane Pulse", "Resources/Audio/Effects/spell_arcane_pulse.wav", AudioAssetType::Effect, 0.76f, false, 3, {"effect", "engine"}},
        {"audio_weapon_unsheathe", "Steady Unsheathe", "Resources/Audio/Effects/weapon_unsheathe_steady.wav", AudioAssetType::Effect, 0.76f, false, 2, {"effect", "weapon"}},
        {"audio_weapon_swing", "Fast Weapon Swing", "Resources/Audio/Effects/weapon_swing_fast.wav", AudioAssetType::Effect, 0.78f, false, 2, {"effect", "weapon"}},
        {"audio_deep_warning", "Station Address Feedback", "Resources/Audio/Effects/npc_deep_warning.wav", AudioAssetType::Effect, 0.70f, false, 3, {"effect", "warning"}}};
}

std::filesystem::path audioTargetPath(const std::filesystem::path &root, const AudioSeed &seed)
{
    std::string folder = "Other";
    switch (seed.type)
    {
    case AudioAssetType::Music:
        folder = "Music";
        break;
    case AudioAssetType::Ambience:
        folder = "Ambience";
        break;
    case AudioAssetType::Effect:
        folder = "Effects";
        break;
    case AudioAssetType::Voice:
        folder = "Voice";
        break;
    case AudioAssetType::Other:
        break;
    }
    return packageRoot(root) / "Assets" / "Audio" / folder / std::filesystem::path(seed.sourcePath).filename();
}

void createProjectAssets(const std::filesystem::path &root)
{
    const std::filesystem::path campaignRoot = packageRoot(root);
    std::filesystem::create_directories(campaignRoot / "Assets" / "Maps");
    std::filesystem::create_directories(campaignRoot / "Assets" / "Handouts");
    std::filesystem::create_directories(campaignRoot / "Assets" / "Files");
    std::filesystem::create_directories(campaignRoot / "Layouts");

    writeBmp(campaignRoot / "Assets" / "Maps" / "tallowgate_rail_terminus.bmp", 960, 640, 70, 64, 54, 1);
    writeBmp(campaignRoot / "Assets" / "Maps" / "holtz_vane_foundry_floor.bmp", 960, 640, 82, 54, 44, 2);
    writeBmp(campaignRoot / "Assets" / "Maps" / "royal_patent_office.bmp", 900, 640, 60, 70, 72, 3);
    writeBmp(campaignRoot / "Assets" / "Maps" / "sky_tram_switch_tower.bmp", 900, 640, 66, 75, 88, 4);
    writeBmp(campaignRoot / "Assets" / "Maps" / "porcelain_countess_salon.bmp", 900, 640, 78, 62, 74, 5);
    writeBmp(campaignRoot / "Assets" / "Maps" / "boiler_choir_underworks.bmp", 960, 700, 55, 62, 58, 6);
    writeBmp(campaignRoot / "Assets" / "Maps" / "glasshouse_canal_market.bmp", 900, 640, 58, 74, 70, 7);
    writeBmp(campaignRoot / "Assets" / "Maps" / "meridian_station_engine_core.bmp", 1024, 720, 74, 58, 62, 8);

    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h01_tallowgate_crate_manifest.txt",
                  "TALLOWGATE CUSTOMS MANIFEST\nCrate 7B: surgical tubing, pressure ampoules, four sealed vials marked HEIR RESERVE. Routed from Vellum Charity Clinic to Aurelian Works under council emergency exemption.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h02_union_punchcard.txt",
                  "BOILER LOCAL 13 PUNCHCARD\nNames punched through: Nari Rake, Osric Bell, Lysa Fen, Cora Hobb. Marginal code: Furnace Key candidates transferred before dawn.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h03_patent_plate_excerpt.txt",
                  "SELF-CORRECTING HYDRAULIC INHERITANCE APPARATUS\nPlate B shows an intake manifold shaped for living veins, not fuel lines. The inventor name is hidden under a Brass Council seal.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h04_countess_guest_cards.txt",
                  "PORCELAIN COUNTESS GUEST CARDS\nMaker marks on the masks match Crown, Furnace, Mirror, and Mercy genealogies. Three guests are using borrowed names.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h05_vellum_clinic_consent.txt",
                  "VELLUM CHARITY CLINIC CONSENT FORM\nThe patient agrees to temporary humoral custody, hereditary sampling, and civic stabilization procedures. Signature line: witnessed under wage relief pressure.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h06_meridian_routing_telegram.txt",
                  "MERIDIAN STATION TELEGRAM\nMove governor core by private rail at Jubilee minus six hours. Use platform three, pressure switchboard override, and sympathetic constabulary.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h07_sump_saints_genealogy.txt",
                  "SUMP SAINTS ORAL GENEALOGY\nThe Mercy Key did not vanish. It was hidden among nameless wards, debt children, and surgeons who worked without temple license.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h08_engine_covenant_fragment.txt",
                  "ENGINE COVENANT FRAGMENT\nNo command is lawful unless the city consents. No repair is holy unless the wounded may refuse. No crown may speak through all clocks forever.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h09_inheritance_valve_diagram.txt",
                  "INHERITANCE VALVE DIAGRAM\nA pressure valve has been refit with a blood-reactive governor. It can stabilize a district, locate a key, or burst a boiler choir apart.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h10_red_tinker_broadsheet.txt",
                  "THE RED TINKER STRIKES AGAIN\nA lurid broadsheet blames a lone criminal surgeon. The printed murder times match Aurelian Works shift changes too neatly.\n");
    writeTextFile(campaignRoot / "Assets" / "Handouts" / "h11_finale_choice_matrix.txt",
                  "FINAL ENGINE CHOICES\nSever, Rewrite, Destroy, Crown, or Heal. Each requires one key, one public truth, one sacrifice, and one answer to the question: who may command a city?\n");

    writeTextFile(campaignRoot / "Assets" / "Files" / "cinder_vow_engine_quickstart.md",
                  "# Cinder Vow Engine Quickstart\n\n"
                  "Roll Attribute + Skill + Asset dice - Hindrance dice. Each d6 showing 5 or 6 is a Hit; each 6 is also a Spark. Difficulty is 1 Simple, 2 Risky, 3 Hard, 4 Severe, or 5 Mythic. A success by 2+ creates a strong success and gains Momentum. One Hit short offers success with a cost.\n\n"
                  "The crew shares Momentum, maximum 6. PCs have 6 Stress boxes, or 7 with Ironclad. Overflow becomes Minor, Major, or Grievous Injury. Ethertech has Function, Frame, Power, Charges, advantage tags, and flaw tags. Divine play uses Favor; occult shortcuts create Stain.\n");

    writeTextFile(campaignRoot / "README.md",
                  "# Clockwork Bloodline\n\n"
                  "A clean-slate steampunk campaign package for four generated-portrait PCs using the original Cinder Vow Engine. Includes a 9-session conspiracy arc, linked character stories, factions, deities, maps, encounters, audio cues, handouts, shops, loot, dungeons, sessions, public display state, and GM runbook documents.\n\n"
                  "Start with Session 1: Thirteen Bells at Tallowgate. Open the rail terminus map, play Rain In The Gutter with Gearmarket Day, reveal H01/H02, and let the nameless child with the ticking heart pull every personal story into the same machine.\n");

    writeTextFile(campaignRoot / "Layouts" / "editor-prep.ini", "[Window][Clockwork Prep]\nPos=60,60\nSize=1280,720\n");
    writeTextFile(campaignRoot / "Layouts" / "editor-live.ini", "[Window][Clockwork Live]\nPos=30,30\nSize=1280,720\n");

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
                   int crownValue,
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
    item.baseValue = crownValue * 100;
    item.description = description;
    item.rulesetKey = kRulesetId;
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

std::map<std::string, int> attrs(int iron, int spark, int veil, int grace, int lore, int pulse)
{
    return {{"Iron", iron}, {"Spark", spark}, {"Veil", veil}, {"Grace", grace}, {"Lore", lore}, {"Pulse", pulse}};
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
                             const std::string &archetype,
                             const std::string &background,
                             const std::map<std::string, int> &attributes,
                             int stress,
                             const std::string &portraitPath,
                             Tags tags)
{
    CharacterModel character;
    setMetadata(character.metadata, id, tags);
    std::snprintf(character.nameBuffer, sizeof(character.nameBuffer), "%s", name.c_str());
    character.selectedRace = "Human";
    character.selectedClass = archetype;
    character.background = background;
    character.level = 1;
    character.attributes = attributes;
    character.currentHitPoints = stress;
    character.maxHitPoints = stress;
    character.portraitAssetId = portraitPath;
    character.inventory.name = name + " Kit";
    character.inventory.ownerEntityId = id;
    character.speedFeet = 30;
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
    spell.classes = "Cinder Vow Engine";
    return spell;
}

CharacterSpellEntryModel spellEntry(const std::string &spellId, const std::string &source = "Cinder Vow Engine", bool prepared = true, const std::string &notes = "")
{
    CharacterSpellEntryModel entry;
    entry.spellId = spellId;
    entry.source = source;
    entry.prepared = prepared;
    entry.notes = notes;
    return entry;
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
    npc.attributes = attrs(2, 2, 2, 2, 2, 2);
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
    case 1: // Tallowgate Rail Terminus and Fog Yard
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Stone);
        fillRect(result, width, height, 2, 4, 32, 16, MapTileKind::Dirt);
        paintLine(result, width, height, 0, 15, width - 1, 15, MapTileKind::Road, 0);
        paintLine(result, width, height, 0, 18, width - 1, 18, MapTileKind::Road, 0);
        fillRect(result, width, height, 8, 8, 8, 4, MapTileKind::Building);
        fillRect(result, width, height, 23, 5, 5, 7, MapTileKind::Building);
        fillRect(result, width, height, 12, 13, 7, 3, MapTileKind::Cover);
        setCell(result, width, height, 13, 12, MapTileKind::Landmark);
        setCell(result, width, height, 28, 5, MapTileKind::Landmark);
        fillRect(result, width, height, 0, 0, width, 4, MapTileKind::Swamp);
        break;

    case 2: // Holtz & Vane Foundry Floor
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Wall);
        fillRect(result, width, height, 2, 2, 30, 20, MapTileKind::Stone);
        fillRect(result, width, height, 3, 3, 7, 17, MapTileKind::Lava);
        fillRect(result, width, height, 11, 5, 8, 6, MapTileKind::Room);
        fillRect(result, width, height, 21, 6, 8, 13, MapTileKind::Room);
        paintLine(result, width, height, 10, 12, 27, 12, MapTileKind::Road, 0);
        paintLine(result, width, height, 5, 20, 30, 20, MapTileKind::Water, 0);
        setCell(result, width, height, 17, 12, MapTileKind::Hazard);
        setCell(result, width, height, 29, 18, MapTileKind::Landmark);
        fillRect(result, width, height, 14, 13, 5, 3, MapTileKind::Cover);
        break;

    case 3: // Royal Patent Office
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Wall);
        fillRect(result, width, height, 2, 2, 26, 20, MapTileKind::Room);
        outlineRect(result, width, height, 2, 2, 26, 20, MapTileKind::Wall);
        paintLine(result, width, height, 4, 11, 25, 11, MapTileKind::Road, 0);
        paintLine(result, width, height, 15, 3, 15, 20, MapTileKind::Road, 0);
        for (int x = 5; x < 24; x += 4)
            fillRect(result, width, height, x, 4, 2, 5, MapTileKind::Cover);
        for (int x = 5; x < 24; x += 4)
            fillRect(result, width, height, x, 14, 2, 5, MapTileKind::Cover);
        setCell(result, width, height, 15, 11, MapTileKind::Landmark);
        setCell(result, width, height, 22, 6, MapTileKind::Hazard);
        break;

    case 4: // Sky Tram and Switch Tower
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Chasm);
        paintLine(result, width, height, 1, 10, 30, 10, MapTileKind::Bridge, 0);
        paintLine(result, width, height, 1, 12, 30, 12, MapTileKind::Bridge, 0);
        fillRect(result, width, height, 7, 7, 8, 7, MapTileKind::Building);
        fillRect(result, width, height, 23, 4, 6, 10, MapTileKind::Stone);
        outlineRect(result, width, height, 23, 4, 6, 10, MapTileKind::Wall);
        setCell(result, width, height, 10, 10, MapTileKind::Landmark);
        setCell(result, width, height, 18, 5, MapTileKind::Hazard);
        paintLine(result, width, height, 12, 7, 24, 4, MapTileKind::Road, 0);
        break;

    case 5: // Porcelain Countess Salon and Midnight Ward
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Dirt);
        fillRect(result, width, height, 3, 3, 25, 18, MapTileKind::Room);
        outlineRect(result, width, height, 3, 3, 25, 18, MapTileKind::Wall);
        fillRect(result, width, height, 5, 5, 10, 7, MapTileKind::Stone);
        fillRect(result, width, height, 17, 5, 8, 7, MapTileKind::Room);
        fillRect(result, width, height, 19, 14, 7, 5, MapTileKind::Room);
        fillDisc(result, width, height, 8, 17, 4, MapTileKind::Forest);
        setCell(result, width, height, 12, 8, MapTileKind::Landmark);
        setCell(result, width, height, 25, 18, MapTileKind::Door);
        break;

    case 6: // Boiler Choir Underworks
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Wall);
        fillRect(result, width, height, 2, 2, 32, 22, MapTileKind::Room);
        outlineRect(result, width, height, 2, 2, 32, 22, MapTileKind::Wall);
        paintLine(result, width, height, 4, 7, 32, 7, MapTileKind::Water, 0);
        paintLine(result, width, height, 10, 2, 10, 23, MapTileKind::Water, 0);
        paintLine(result, width, height, 4, 13, 31, 13, MapTileKind::Road, 0);
        fillRect(result, width, height, 17, 10, 5, 6, MapTileKind::Hazard);
        fillRect(result, width, height, 27, 17, 6, 5, MapTileKind::Stone);
        setCell(result, width, height, 19, 13, MapTileKind::Landmark);
        setCell(result, width, height, 30, 20, MapTileKind::Landmark);
        break;

    case 7: // Glasshouse Canal Market
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Dirt);
        fillRect(result, width, height, 0, 8, width, 5, MapTileKind::Water);
        fillRect(result, width, height, 14, 0, 5, height, MapTileKind::Water);
        paintLine(result, width, height, 0, 7, width - 1, 7, MapTileKind::Road, 0);
        paintLine(result, width, height, 0, 13, width - 1, 13, MapTileKind::Road, 0);
        paintLine(result, width, height, 16, 2, 16, 20, MapTileKind::Bridge, 0);
        fillRect(result, width, height, 4, 14, 7, 5, MapTileKind::Settlement);
        fillRect(result, width, height, 21, 3, 8, 4, MapTileKind::Settlement);
        setCell(result, width, height, 18, 11, MapTileKind::Landmark);
        setCell(result, width, height, 7, 15, MapTileKind::Landmark);
        break;

    case 8: // Meridian Station and Sanguine Governor Core
        fillRect(result, width, height, 0, 0, width, height, MapTileKind::Stone);
        fillRect(result, width, height, 2, 5, 34, 16, MapTileKind::Room);
        outlineRect(result, width, height, 2, 5, 34, 16, MapTileKind::Wall);
        paintLine(result, width, height, 0, 9, width - 1, 9, MapTileKind::Road, 0);
        paintLine(result, width, height, 0, 17, width - 1, 17, MapTileKind::Road, 0);
        fillRect(result, width, height, 5, 4, 6, 5, MapTileKind::Building);
        fillRect(result, width, height, 18, 9, 5, 7, MapTileKind::Hazard);
        fillRect(result, width, height, 28, 16, 7, 6, MapTileKind::Room);
        setCell(result, width, height, 7, 6, MapTileKind::Landmark);
        setCell(result, width, height, 20, 12, MapTileKind::Landmark);
        setCell(result, width, height, 31, 20, MapTileKind::Landmark);
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
                                   const std::string &threat,
                                   int defense,
                                   int stress,
                                   const std::string &traits,
                                   Tags tags)
{
    MonsterDefinitionModel monster;
    setMetadata(monster.metadata, id, tags);
    monster.name = name;
    monster.source = kCampaignName;
    monster.size = "Medium";
    monster.type = "Clockwork Bloodline Threat";
    monster.challengeRating = threat;
    monster.armorClass = defense;
    monster.hitPoints = stress;
    monster.speedFeet = 30;
    monster.traits = traits;
    monster.attributes = attrs(2, 2, 2, 2, 2, 2);
    return monster;
}

EncounterParticipantModel participant(const std::string &id,
                                      CombatantKind kind,
                                      const std::string &sourceId,
                                      const std::string &monsterId,
                                      const std::string &name,
                                      int initiative,
                                      int stress,
                                      int defense,
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
    model.currentHitPoints = stress;
    model.maxHitPoints = stress;
    model.armorClass = defense;
    model.speedFeet = 30;
    model.friendly = friendly;
    model.startingX = x;
    model.startingY = y;
    model.x = x;
    model.y = y;
    model.turnStartX = x;
    model.turnStartY = y;
    model.groupName = friendly ? "Crew" : "Opposition";
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
    encounter.diceRollPresets = {
        dicePreset(id + "_pool_simple", "Simple Pool", "4d6", "Count 5-6 as Hits; 6s are Sparks."),
        dicePreset(id + "_pool_hard", "Hard Pool", "7d6", "Use for an advantaged Attribute + Skill + Asset roll.")};
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
    model.cueLoop = audioId.find("ambience") != std::string::npos || audioId.find("rain") != std::string::npos || audioId.find("rumble") != std::string::npos;
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
    model.factionReputationNotes = "Prices and access shift with union trust, Synod scrutiny, and noble scandal.";
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
    entry.haggleNotes = "Use Grace + Charm or faction leverage; a strong success adds a useful rumor.";
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
                sqlQuote(projectPath(root, path)) + "," + sqlQuote("clockwork-seed-" + id) + "," + sqlQuote(mime) + "," +
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
        const std::vector<std::string> portraits = {"pc_iona_vale.png", "pc_corvin_rake.png", "pc_mirelle_voss.png", "pc_tomas_arkwright.png", "clockwork_portrait_contact_sheet.png"};
        for (const std::string &portrait : portraits)
            insertAssetMetadata(db, rowId, "asset_" + portrait.substr(0, portrait.find('.')), "portrait", portrait, root, campaignRoot / "Assets" / "Portraits" / portrait, "image/png", "portrait;player;steampunk;");
        for (const std::string &map : {"tallowgate_rail_terminus.bmp", "holtz_vane_foundry_floor.bmp", "royal_patent_office.bmp", "sky_tram_switch_tower.bmp", "porcelain_countess_salon.bmp", "boiler_choir_underworks.bmp", "glasshouse_canal_market.bmp", "meridian_station_engine_core.bmp"})
            insertAssetMetadata(db, rowId, "asset_" + map.substr(0, map.find('.')), "map", map, root, campaignRoot / "Assets" / "Maps" / map, "image/bmp", "map;background;steampunk;");
        for (const AudioSeed &seed : audioSeeds())
            insertAssetMetadata(db, rowId, "asset_" + seed.id, "audio", seed.name, root, audioTargetPath(root, seed), seed.sourcePath.find(".mp3") != std::string::npos ? "audio/mpeg" : "audio/wav", "audio;cc0;seeded;");
        for (int i = 1; i <= 11; ++i)
        {
            std::ostringstream prefix;
            prefix << "h" << (i < 10 ? "0" : "") << i << "_";
            const std::filesystem::path handoutDir = campaignRoot / "Assets" / "Handouts";
            for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(handoutDir))
            {
                if (entry.path().filename().string().rfind(prefix.str(), 0) == 0)
                {
                    const std::string handoutAssetId = std::string("asset_h") + (i < 10 ? "0" : "") + std::to_string(i);
                    insertAssetMetadata(db, rowId, handoutAssetId, "handout", entry.path().stem().string(), root, entry.path(), "text/plain", "handout;player;");
                }
            }
        }
        insertAssetMetadata(db, rowId, "asset_cinder_vow_quickstart", "file", "Cinder Vow Engine Quickstart", root, campaignRoot / "Assets" / "Files" / "cinder_vow_engine_quickstart.md", "text/markdown", "rules;quickstart;");
        insertAssetMetadata(db, rowId, "asset_campaign_readme", "file", "Campaign README", root, campaignRoot / "README.md", "text/markdown", "overview;gm;");

        insertStoryDocument(db, rowId, "doc_clockwork_runbook", "GM Runbook - Clockwork Bloodline",
                            "Open at Tallowgate during rain and customs pressure. The nameless child, the clinic crate, and the union punchcard should point every PC toward the same conspiracy. Keep the core question visible: is the divine machinery saving Veyrbridge, harvesting it, or both?",
                            "gm;runbook;clockwork;");
        insertStoryDocument(db, rowId, "doc_cinder_vow_engine", "Cinder Vow Engine - Table Rules",
                            "Roll Attribute + Skill + Asset dice minus Hindrance dice. Each d6 showing 5 or 6 is a Hit; each 6 is a Spark. Difficulty is Simple 1, Risky 2, Hard 3, Severe 4, Mythic 5. PCs have Stress and Injuries, crew Momentum, ethertech Charges, divine Favor, and occult Stain.",
                            "rules;homebrew;player-safe;");
        insertStoryDocument(db, rowId, "doc_clockwork_clue_web", "Clue Web and Fail-Forward",
                            "Core clues: Tallowgate crate, union punchcard, patent plates, clinic consent fraud, Sump genealogy, inheritance valve, Red Tinker broadsheet, and final Engine covenant. Missed clues return through smoke messages, debt ledgers, patient symptoms, public clocks, and frightened witnesses.",
                            "gm;clues;mystery;");
        insertStoryDocument(db, rowId, "doc_clockwork_finale_matrix", "Finale Matrix",
                            "Sever the feed to free the bloodline but risk civic pressure collapse. Rewrite the registry to restore legal personhood but invite noble retaliation. Destroy the Governor for moral clarity if infrastructure was secured. Crown or heal the Engine only if the table wants a stranger political ending.",
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

    ItemModel cardiacKey = makeItem("item_clockwork_cardiac_key", "Clockwork Cardiac Key", "Tool", "Medical Tool", ItemRarity::Rare, 0.5, 80, "A delicate brass key that can tune artificial hearts, relic-cores, and bloodline valves.", {"pc", "iona", "ethertech"});
    cardiacKey.effects = {"Spend a charge to add +1 die to Treat or Tinker rolls involving hearts, relics, or living engines."};
    cardiacKey.maxCharges = 3;
    cardiacKey.rechargeRule = "Downtime in a clinic or workshop";
    add(cardiacKey);

    ItemModel injectorCane = makeItem("item_hidden_injector_cane", "Hidden Injector Cane", "Weapon", "Cane", ItemRarity::Uncommon, 2.0, 25, "A surgeon's cane with a concealed Mercy No. 7 injector.", {"weapon", "iona"});
    injectorCane.weaponDamage = "2 Stress";
    injectorCane.weaponAbility = "Iron";
    injectorCane.weaponAttackKind = "Melee";
    add(injectorCane);

    ItemModel smokeEgg = makeItem("item_smoke_egg", "Smoke Egg", "Consumable", "Grenade", ItemRarity::Common, 0.2, 6, "A palm-sized shell that bursts into choking gray smoke.", {"consumable", "corvin", "stealth"});
    smokeEgg.stackable = true;
    smokeEgg.consumable = true;
    smokeEgg.effects = {"Create a smoke zone; +1 die to Skulk if used before moving."};
    add(smokeEgg);

    ItemModel grapnel = makeItem("item_rooftop_grapnel", "Rooftop Grapnel", "Tool", "Climbing Gear", ItemRarity::Uncommon, 4.0, 35, "A compact grapnel built from rail coupling parts.", {"tool", "corvin"});
    grapnel.effects = {"Ignore one height or gap Hindrance during rooftop movement."};
    add(grapnel);

    ItemModel ledger = makeItem("item_spirit_debt_ledger", "Spirit Debt Ledger", "Relic", "Ledger", ItemRarity::Rare, 1.0, 120, "A ledger of debts owed by the living and the dead.", {"relic", "mirelle", "occult"});
    ledger.requiresAttunement = true;
    ledger.effects = {"Spend 1 Favor or accept 1 Stain to ask what is owed in a scene."};
    add(ledger);

    ItemModel pistol = makeItem("item_ivory_dueling_pistol", "Ivory-Handled Dueling Pistol", "Weapon", "Pistol", ItemRarity::Uncommon, 2.5, 60, "A precise pistol with two silvered rounds.", {"weapon", "mirelle"});
    pistol.weaponDamage = "2 Stress";
    pistol.weaponAbility = "Veil";
    pistol.weaponAttackKind = "Ranged";
    pistol.weaponRangeFeet = 30;
    pistol.weaponLongRangeFeet = 90;
    add(pistol);

    ItemModel maul = makeItem("item_sanctified_rivet_maul", "Sanctified Rivet Maul", "Weapon", "Maul", ItemRarity::Rare, 8.0, 95, "A heavy rivet maul with a sanctified pressure chamber.", {"weapon", "tomas", "relic"});
    maul.weaponDamage = "3 Stress";
    maul.weaponAbility = "Iron";
    maul.weaponAttackKind = "Melee";
    maul.effects = {"On a Spark, push a machine, construct, or armored foe one zone."};
    add(maul);

    ItemModel greatcoat = makeItem("item_temple_engine_greatcoat", "Temple Engine Greatcoat", "Armor", "Greatcoat", ItemRarity::Uncommon, 12.0, 70, "Battered navy temple coat lined with flexible armor plates.", {"armor", "tomas"});
    greatcoat.armorClassBonus = 1;
    greatcoat.damageReduction = 1;
    add(greatcoat);

    ItemModel respirator = makeItem("item_brass_respirator", "Brass Respirator", "Tool", "Mask", ItemRarity::Common, 1.0, 18, "A brass-and-leather respirator for smoke, gas, and boiler dust.", {"tool", "smoke"});
    respirator.effects = {"Reduce the first smoke or gas Hindrance in a scene."};
    add(respirator);

    ItemModel vowSeal = makeItem("item_caldris_vow_seal", "Caldris Vow Seal", "Relic", "Seal", ItemRarity::Rare, 0.3, 100, "A crown-gear seal that makes spoken promises visible as red ink.", {"relic", "deity", "crown"});
    vowSeal.requiresAttunement = true;
    vowSeal.effects = {"When a sworn statement is false, the seal warms and bleeds ink."};
    add(vowSeal);

    ItemModel valveDiagram = makeItem("item_inheritance_valve_diagram", "Inheritance Valve Diagram", "Document", "Blueprint", ItemRarity::Uncommon, 0.1, 30, "A technical diagram that proves the valve is keyed to bloodline resonance.", {"clue", "engine"});
    add(valveDiagram);

    ItemModel mercyAmpoule = makeItem("item_mercy_no_7", "Mercy No. 7 Ampoule", "Consumable", "Medicine", ItemRarity::Uncommon, 0.1, 12, "A red-gold stimulant that buys a patient one impossible minute.", {"medicine", "consumable"});
    mercyAmpoule.stackable = true;
    mercyAmpoule.consumable = true;
    mercyAmpoule.effects = {"Clear 1 Stress immediately; mark 1 Stain if used to deny a major consequence."};
    add(mercyAmpoule);

    ItemModel aetherBattery = makeItem("item_condensed_aether_battery", "Condensed Aether Battery", "Consumable", "Fuel Cell", ItemRarity::Common, 0.5, 10, "A humming blue fuel cell for ethertech devices.", {"fuel", "ethertech"});
    aetherBattery.stackable = true;
    aetherBattery.consumable = true;
    add(aetherBattery);

    ItemModel pressBadge = makeItem("item_false_press_badge", "False Gearmarket Press Badge", "Document", "Credential", ItemRarity::Common, 0.1, 8, "A convincing press badge for moving through crowds and records offices.", {"document", "social"});
    add(pressBadge);

    ItemModel engineShard = makeItem("item_sovereign_engine_shard", "Sovereign Engine Shard", "Relic", "Shard", ItemRarity::Artifact, 0.2, 500, "A warm fragment that beats in time with public clocks.", {"relic", "finale", "dangerous"});
    engineShard.cursed = true;
    engineShard.effects = {"Can stabilize or corrupt the final Engine choice."};
    add(engineShard);

    return items;
}

std::vector<SpellModel> buildSpells()
{
    return {
        makeSpell("spell_smoke_message", "Smoke Message", 0, SpellSchool::Divination, SpellResolutionType::Utility, "Near", "One question", "Read one message carried by smoke, steam, or furnace draft.", "A clue cantrip for Corvin and other smoke-touched characters.", {"smoke", "clue"}),
        makeSpell("spell_mercy_surge", "Mercy Surge", 1, SpellSchool::Abjuration, SpellResolutionType::Healing, "Touch", "Instantaneous", "Clear 2 Stress or downgrade one immediate harm with a visible cost.", "A Heartwright emergency procedure.", {"healing", "mercy"}),
        makeSpell("spell_vow_of_caldris", "Vow of Caldris", 1, SpellSchool::Enchantment, SpellResolutionType::Utility, "Near", "Scene", "Bind a promise so breaking it marks Stain or exposes the liar.", "Used in negotiations and court scenes.", {"deity", "crown"}),
        makeSpell("spell_mirror_name", "Mirror Name", 1, SpellSchool::Illusion, SpellResolutionType::Utility, "Self", "Scene", "Wear a social identity convincingly until contradicted by hard proof.", "A Velis-style occult mask.", {"mirror", "identity"}),
        makeSpell("spell_pressure_ward", "Pressure Ward", 2, SpellSchool::Abjuration, SpellResolutionType::SummonOrZone, "Near", "3 rounds", "Create a zone that reduces machine, fire, or steam harm by 1.", "Useful in foundries, tram scenes, and the Underworks.", {"engine", "defense"}),
        makeSpell("spell_aurex_hammerfall", "Aurex Hammerfall", 2, SpellSchool::Evocation, SpellResolutionType::SavingThrow, "Near", "Instantaneous", "Stagger a machine, construct, or oathbreaker; on a Spark, disable one component.", "A labor-patron miracle.", {"deity", "combat"}),
        makeSpell("spell_auditor_account", "Auditor's Account", 2, SpellSchool::Divination, SpellResolutionType::Utility, "Near", "One scene", "Learn the largest unpaid debt in the scene.", "A dangerous favor from the Sable Auditor.", {"debt", "occult"}),
        makeSpell("spell_sovereign_pulse", "Sovereign Pulse", 3, SpellSchool::Necromancy, SpellResolutionType::SummonOrZone, "Far", "Scene", "Reveal active bloodline keys, engine conduits, and people under civic command.", "A late-campaign Engine rite.", {"bloodline", "finale"})};
}

void configureCharacters(Campaign &campaign, const std::filesystem::path &root)
{
    const std::filesystem::path portraits = packageRoot(root) / "Assets" / "Portraits";
    auto portrait = [&](const std::string &file) { return projectPath(root, portraits / file); };

    CharacterModel iona = makeCharacter("pc_iona_vale", "Iona Vale", "Heartwright", "Disgraced battlefield surgeon and illegal heartwright", attrs(2, 4, 2, 2, 3, 3), 6, portrait("pc_iona_vale.png"), {"pc", "healer", "investigator"});
    iona.backstory = "Iona saved a dying prince with a prototype heart made from stolen temple relics. The patient lived, but now broadcasts faint prayers only she can hear.";
    iona.playerNotes = "Ask who benefits from calling care illegal. Prove hereditary clockwork heart failure can be cured without sacrifice.";
    iona.gmNotes = "Press Iona with triage, church auditors, and grateful patients whose symptoms reveal the Engine.";
    iona.skillProficiencies = {"Tinker", "Treat", "Study", "Attune"};
    iona.inventory.entries = {
        makeEntry("inv_iona_key", "item_clockwork_cardiac_key", 1, true, "Tool Rig"),
        makeEntry("inv_iona_cane", "item_hidden_injector_cane", 1, true, "Main Hand"),
        makeEntry("inv_iona_ampoules", "item_mercy_no_7", 3, false, "Pouch"),
        makeEntry("inv_iona_respirator", "item_brass_respirator", 1, false, "Pack")};
    iona.relationships = {
        relationship("rel_iona_corvin", "Character", "pc_corvin_rake", "Former patient", "Iona treated Corvin after a rail-yard explosion and used a temple-derived pulse graft without telling them."),
        relationship("rel_iona_mirelle", "Character", "pc_mirelle_voss", "Secret patron", "Mirelle funds Iona's free clinic through false charities."),
        relationship("rel_iona_sabine", "NPC", "npc_dr_sabine_kreel", "Former mentor", "Kreel wants Iona arrested before the College's experiments are exposed.")};
    iona.resourceTrackers = {
        resource("res_iona_charges", "Cardiac Key Charges", 3, 1, "Downtime", "Spend for +1 die on living-engine medicine."),
        resource("res_iona_stain", "Stain", 6, 1, "Cleansing", "Occult residue from illegal relic surgery.")};
    iona.featureActions = {action("Mercy Surge", "Petition", 5, 5, "+0", "Clear 2 Stress", "A quick surgery, stimulant, or prayer to the Brass Saint.")};

    CharacterModel corvin = makeCharacter("pc_corvin_rake", "Corvin Rake", "Smoke-Talker", "Union saboteur, rooftop runner, and smoke-talker", attrs(2, 3, 4, 3, 1, 3), 6, portrait("pc_corvin_rake.png"), {"pc", "scout", "union"});
    corvin.backstory = "Corvin hears messages in smoke after a failed sabotage killed three friends. Some messages know things the dead never could.";
    corvin.playerNotes = "Find the workers taken in the voluntary blood tithe and bring the surviving names home.";
    corvin.gmNotes = "Use smoke-speech as a clue engine that helps but never becomes fully trustworthy.";
    corvin.skillProficiencies = {"Skulk", "Tinker", "Command", "Notice"};
    corvin.inventory.entries = {
        makeEntry("inv_corvin_grapnel", "item_rooftop_grapnel", 1, true, "Tool Rig"),
        makeEntry("inv_corvin_smoke", "item_smoke_egg", 2, false, "Pouch"),
        makeEntry("inv_corvin_badge", "item_false_press_badge", 1, false, "Pocket"),
        makeEntry("inv_corvin_respirator", "item_brass_respirator", 1, true, "Head")};
    corvin.relationships = {
        relationship("rel_corvin_iona", "Character", "pc_iona_vale", "Unknowing graft", "Corvin does not know their hearing was saved with a temple relic graft."),
        relationship("rel_corvin_mirelle", "Character", "pc_mirelle_voss", "Distrusted aristocrat", "Mirelle quietly bought Corvin's sister's debt file and has not admitted what was inside."),
        relationship("rel_corvin_jessa", "NPC", "npc_ma_jessa_pike", "Union family", "Ma Jessa loves Corvin like family but will spend lives if the strike needs martyrs.")};
    corvin.resourceTrackers = {resource("res_corvin_smoke", "Smoke Messages", 3, 0, "Scene", "Ask the GM for one hint carried by smoke, steam, or furnace draft.")};
    corvin.featureActions = {action("Smoke Egg", "Tool", 5, 30, "+0", "Create smoke zone", "Use before Skulk, escape, or crowd movement.")};

    CharacterModel mirelle = makeCharacter("pc_mirelle_voss", "Mirelle Voss", "Debt Medium", "Disgraced noble medium and debt broker", attrs(1, 2, 3, 4, 3, 3), 6, portrait("pc_mirelle_voss.png"), {"pc", "face", "occult"});
    mirelle.backstory = "Mirelle's family bound ancestor-ghosts into counting machines that predict markets, births, deaths, and inheritances.";
    mirelle.playerNotes = "Restore the Voss name without returning to its old crimes, then abolish hereditary debt contracts.";
    mirelle.gmNotes = "Let paperwork become adventure fuel: wills, debts, marriage contracts, missing names, and inheritance trials.";
    mirelle.skillProficiencies = {"Charm", "Study", "Attune", "Notice"};
    mirelle.spellcasting.enabled = true;
    mirelle.spellcasting.ability = "Pulse";
    mirelle.spellcasting.spells = {spellEntry("spell_auditor_account"), spellEntry("spell_mirror_name"), spellEntry("spell_vow_of_caldris")};
    mirelle.inventory.entries = {
        makeEntry("inv_mirelle_ledger", "item_spirit_debt_ledger", 1, true, "Relic"),
        makeEntry("inv_mirelle_pistol", "item_ivory_dueling_pistol", 1, true, "Off Hand"),
        makeEntry("inv_mirelle_vow", "item_caldris_vow_seal", 1, false, "Pouch")};
    mirelle.relationships = {
        relationship("rel_mirelle_iona", "Character", "pc_iona_vale", "Secret funding", "Mirelle's false charities keep Iona's clinic open."),
        relationship("rel_mirelle_tomas", "Character", "pc_tomas_arkwright", "Legal shield", "Mirelle can navigate the noble courts hunting Tomas."),
        relationship("rel_mirelle_ysabet", "NPC", "npc_aunt_ysabet_voss", "Family predator", "Aunt Ysabet wants Mirelle back as heir or corpse.")};
    mirelle.resourceTrackers = {resource("res_mirelle_favor", "Auditor Favor", 3, 1, "Vows", "Spend to erase or reveal a debt; every favor makes compassion feel expensive.")};
    mirelle.featureActions = {action("Ledger Whisper", "Occult", 5, 30, "+0", "Reveal a debt", "Ask what is owed in a scene; accept Stain for more detail.")};

    CharacterModel tomas = makeCharacter("pc_tomas_arkwright", "Tomas Arkwright", "Engine Knight", "Ex-temple engine knight and runaway relic bearer", attrs(4, 2, 1, 3, 2, 3), 7, portrait("pc_tomas_arkwright.png"), {"pc", "defender", "relic"});
    tomas.backstory = "A cracked relic-core climbed into Tomas's chest during a temple fire and begged him to run.";
    tomas.playerNotes = "Discover why cathedral engines require living blood, then redeem the order or bring it down.";
    tomas.gmNotes = "His relic-core creates visions, protective surges, or public disasters when exposed to bloodline magic.";
    tomas.skillProficiencies = {"Fight", "Command", "Attune", "Survive"};
    tomas.inventory.entries = {
        makeEntry("inv_tomas_maul", "item_sanctified_rivet_maul", 1, true, "Two-Handed"),
        makeEntry("inv_tomas_greatcoat", "item_temple_engine_greatcoat", 1, true, "Body"),
        makeEntry("inv_tomas_battery", "item_condensed_aether_battery", 2, false, "Pack")};
    tomas.relationships = {
        relationship("rel_tomas_iona", "Character", "pc_iona_vale", "Clinic protector", "Tomas protects Iona's clinic from church patrols; his core recognizes her tools."),
        relationship("rel_tomas_corvin", "Character", "pc_corvin_rake", "Mercy at a riot", "Tomas once let Corvin escape arrest after a food riot."),
        relationship("rel_tomas_malvek", "NPC", "npc_prior_malvek_orr", "Former commander", "Malvek believes Tomas is possessed and wants the relic returned.")};
    tomas.resourceTrackers = {
        resource("res_tomas_core_heat", "Relic-Core Heat", 6, 2, "Cooling", "Rises near bloodline magic; can power protection or disaster."),
        resource("res_tomas_favor", "Argent Favor", 3, 1, "Discipline", "Spend to hold a line or bind a machine.")};
    tomas.featureActions = {action("Sanctified Rivet Maul", "Attack", 5, 5, "+0", "3 Stress", "On a Spark, push a machine, armored foe, or oathbreaker one zone.")};

    campaign.characters = {iona, corvin, mirelle, tomas};
}

void configureNpcsAndFactions(Campaign &campaign)
{
    campaign.factions = {
        makeFaction("faction_brass_council", "Brass Council", "Civic Government", "Technocratic city government trying to preserve order while hiding what it thinks is only a power plant.", "npc_lord_mechanist_severin_cadrane", "Prevent panic and preserve institutional control.", 80, 4, {"government", "law"}),
        makeFaction("faction_house_vaust", "House Vaust", "Old Nobility", "Debt-ridden royal line with a lost Crown Key branch and dangerous cultural legitimacy.", "npc_princess_elowen_vaust", "Recover legitimacy before the Jubilee collapses the city.", 62, 3, {"noble", "crown"}),
        makeFaction("faction_gearwright_union", "Gearwright Union", "Labor Federation", "Factory, rail, and boiler workers protecting families marked by Furnace resonance.", "npc_ma_jessa_pike", "Stop disappearances and win public proof without sacrificing the poor wards.", 70, 3, {"union", "labor"}),
        makeFaction("faction_ashglass_synod", "Ashglass Synod", "Temple Alliance", "Oath, funeral, and engine-temple clergy who edited doctrine to hide the Engine.", "npc_prior_malvek_orr", "Keep divine infrastructure from becoming public scandal.", 68, 4, {"temple", "deity"}),
        makeFaction("faction_lacquer_court", "Lacquer Court", "Information Brokers", "Masked socialites, counterfeiters, duelists, and theater owners preserving hidden Mirror lineages.", "npc_porcelain_countess", "Control which identities become true.", 55, 3, {"secrets", "social"}),
        makeFaction("faction_aurelian_works", "Aurelian Works", "Industrial Consortium", "Cadrane's consortium building the citywide obedience network under emergency safety contracts.", "npc_lord_mechanist_severin_cadrane", "Complete the Harmonious Succession.", 84, 5, {"company", "villain"}),
        makeFaction("faction_sump_saints", "Sump Saints", "Undercity Mutual Aid", "Smugglers, caretakers, and oral historians protecting Mercy lineages in forgotten districts.", "npc_ros_parel", "Evacuate vulnerable people and keep the Mercy Key free.", 44, 2, {"undercity", "mercy"})};

    DeityModel aurex;
    setMetadata(aurex.metadata, "deity_aurex_deep_anvil", {"deity", "labor", "craft"});
    aurex.name = "Aurex the Deep Anvil";
    aurex.powerType = "Patron";
    aurex.pantheon = "The Gear-Saints";
    aurex.domains = "Craft, endurance, labor, bodies changed by work";
    aurex.symbol = "Hammer over a furnace door";
    aurex.alignment = "Protective";
    aurex.tenets = "No tool is holy while the hand that uses it is disposable.";
    aurex.worshipers = "Machinists, miners, boiler choirs, injured workers";
    aurex.relationships = "Regrets forging the Engine frame and may demand it be broken.";
    aurex.influence = 70;
    aurex.threatLevel = 3;

    DeityModel velis;
    setMetadata(velis.metadata, "deity_velis_turning_face", {"deity", "mirror", "identity"});
    velis.name = "Velis of the Turning Face";
    velis.powerType = "Patron";
    velis.pantheon = "The Gear-Saints";
    velis.domains = "Memory, masks, names, mirrors, performance";
    velis.symbol = "Half-polished mirror";
    velis.alignment = "Mercifully deceptive";
    velis.tenets = "Truth without shelter can be another blade.";
    velis.worshipers = "Actors, spies, hidden heirs, debt refugees";
    velis.relationships = "Hid bloodline descendants so well that no one knows who has the right to choose.";
    velis.influence = 58;
    velis.threatLevel = 3;

    DeityModel caldris;
    setMetadata(caldris.metadata, "deity_caldris_measured_crown", {"deity", "law", "crown"});
    caldris.name = "Saint Caldris of the Measured Crown";
    caldris.powerType = "Patron";
    caldris.pantheon = "The Gear-Saints";
    caldris.domains = "Oath, civic law, command, succession";
    caldris.symbol = "Crown gear and sealed warrant";
    caldris.alignment = "Lawful";
    caldris.tenets = "No command is rightful unless the commanded may answer.";
    caldris.worshipers = "Judges, officers, clerks, heirs, reformers";
    caldris.relationships = "Tempting to authoritarians because order can sound like mercy under pressure.";
    caldris.influence = 76;
    caldris.threatLevel = 4;

    DeityModel morrow;
    setMetadata(morrow.metadata, "deity_mother_morrow_steam", {"deity", "mercy", "breath"});
    morrow.name = "Mother Morrow of the Steam";
    morrow.powerType = "Patron";
    morrow.pantheon = "The Gear-Saints";
    morrow.domains = "Labor, mercy, breath, endurance";
    morrow.symbol = "Open palm in steam";
    morrow.alignment = "Compassionate";
    morrow.tenets = "The exhausted must breathe before they can be brave.";
    morrow.worshipers = "Nurses, boiler monks, rail workers, refugees";
    morrow.relationships = "Protects the Mercy Key and opposes any cure without consent.";
    morrow.influence = 65;
    morrow.threatLevel = 2;

    DeityModel auditor;
    setMetadata(auditor.metadata, "deity_sable_auditor", {"deity", "debt", "death"});
    auditor.name = "The Sable Auditor";
    auditor.powerType = "Patron";
    auditor.pantheon = "The Gear-Saints";
    auditor.domains = "Death, debts, records, secrets";
    auditor.symbol = "Black ledger and silver needle";
    auditor.alignment = "Transactional";
    auditor.tenets = "All accounts close; choose who reads them aloud.";
    auditor.worshipers = "Executors, mediums, bankers, debt breakers";
    auditor.relationships = "Can erase debts, reveal bloodlines, or turn compassion into arithmetic.";
    auditor.influence = 60;
    auditor.threatLevel = 4;

    DeityModel argent;
    setMetadata(argent.metadata, "deity_argent_engine", {"deity", "infrastructure", "prison"});
    argent.name = "The Argent Engine";
    argent.powerType = "Divine Prison";
    argent.pantheon = "The Gear-Saints";
    argent.domains = "Order, infrastructure, sacred machinery, discipline";
    argent.symbol = "Silver piston with prayer teeth";
    argent.alignment = "Constrained";
    argent.tenets = "Hold the line until the bridge is safe.";
    argent.worshipers = "Engine knights, civic engineers, temple guards";
    argent.relationships = "May not be a god, but a prison built around one.";
    argent.influence = 82;
    argent.threatLevel = 5;

    campaign.deities = {aurex, velis, caldris, morrow, auditor, argent};

    campaign.npcs = {
        makeNpc("npc_lord_mechanist_severin_cadrane", "Lord-Mechanist Severin Cadrane", NPCCategory::Person, "Aurelian Works", "Primary antagonist", "A civic hero engineer planning the Harmonious Succession.", "Patient, paternal, certain.", "He needs consent, deception, or loving representation from four keys; force alone cannot awaken the Engine.", "Install himself as immortal regent of a perfected machine-state.", 5, {"villain", "engineer"}),
        makeNpc("npc_dr_sabine_kreel", "Dr. Sabine Kreel", NPCCategory::Person, "Royal College of Physic Engines", "Medical rival", "Iona's former mentor, now a polished college enforcer.", "Clinical and disappointed.", "Her research created the graft technique Iona stole.", "Arrest Iona before the College's crimes become evidence.", 3, {"doctor", "iona"}),
        makeNpc("npc_lord_alaric_penn", "Lord Alaric Penn", NPCCategory::Person, "House Vaust", "Hidden patient", "The prince Iona saved with a stolen relic heart.", "Grateful and terrified.", "His dreams broadcast prayers through the Engine.", "Stay alive without becoming a Crown Key puppet.", 2, {"noble", "patient"}),
        makeNpc("npc_ma_jessa_pike", "Ma Jessa Pike", NPCCategory::Person, "Gearwright Union", "Union organizer", "A beloved organizer who treats every worker name as sacred.", "Warm until the strike is threatened.", "She hides Furnace families and has authorized sabotage.", "Win proof and protection, even at a cost.", 3, {"union", "corvin"}),
        makeNpc("npc_inspector_willem_strake", "Inspector Willem Strake", NPCCategory::Person, "Railway Police", "Ambiguous pursuer", "A railway police inspector who has spared Corvin twice.", "Dry, observant, almost kind.", "He knows where Nari Rake was transferred.", "Keep the rails running and decide whether the law deserves help.", 3, {"police", "corvin"}),
        makeNpc("npc_nari_rake", "Nari Rake", NPCCategory::Person, "Aurelian Works", "Missing sister", "Corvin's sister, possibly alive inside the automaton program.", "Faint, fragmented, not fully herself.", "Her body is being used to test obedience relays.", "Be found before her name is overwritten.", 2, {"missing", "furnace"}),
        makeNpc("npc_aunt_ysabet_voss", "Aunt Ysabet Voss", NPCCategory::Person, "House Voss", "Noble predator", "Elegant, cruel, and perfectly fluent in inheritance law.", "Affection as leverage.", "The Voss ancestor-machine predicted the PCs by name.", "Bring Mirelle home as heir or corpse.", 4, {"noble", "mirelle"}),
        makeNpc("npc_tallow_jack", "Tallow Jack", NPCCategory::Person, "Sump Saints", "Debt collector ally", "Dockside collector loyal to Mirelle for freeing his son's ghost-contract.", "Gruff and practical.", "Knows the canal route to the pressure chapel.", "Keep debt children out of noble ledgers.", 2, {"undercity", "ally"}),
        makeNpc("npc_archivist_ren_sol", "Archivist Ren Sol", NPCCategory::Person, "Ashglass Synod", "Forbidden genealogist", "Temple record keeper trading genealogies for favors and affection.", "Brilliant, lonely, careful.", "Has H08 hidden in a reliquary index.", "Survive disclosure and keep one person safe.", 2, {"archive", "deity"}),
        makeNpc("npc_prior_malvek_orr", "Prior Malvek Orr", NPCCategory::Person, "Ashglass Synod", "Engine knight commander", "Tomas's former commander, convinced the relic-core is possession.", "Stern and wounded.", "He knows the Argent Engine is a prison but calls that mercy.", "Return the relic-core before the Jubilee.", 4, {"temple", "tomas"}),
        makeNpc("npc_sister_ada_flint", "Sister Ada Flint", NPCCategory::Person, "Ashglass Synod", "Temple mechanic", "Secretly sends Tomas maintenance advice and coded warnings.", "Nervous, loyal, quietly brave.", "She has access to Meridian Station service tunnels.", "Prevent Malvek from killing Tomas.", 2, {"temple", "ally"}),
        makeNpc("npc_captain_roza_pell", "Captain Roza Pell", NPCCategory::Person, "City Watch", "Public-law pressure", "A watch captain who respects Tomas but will arrest him if pressure rises.", "Fair and exhausted.", "Her warrant book already contains impossible future signatures.", "Prevent riots and keep witnesses alive.", 2, {"watch", "law"}),
        makeNpc("npc_porcelain_countess", "The Porcelain Countess", NPCCategory::Person, "Lacquer Court", "Salon broker", "A masked patron whose parlor turns secrets into currency.", "Playful, predatory, useful.", "She knows one PC's legal identity was edited.", "Make Cadrane bleed without destroying noble privilege.", 4, {"social", "secrets"}),
        makeNpc("npc_elian_ashworth_lex", "Elian Ashworth-Lex", NPCCategory::Person, "House Vaust", "Engine heir", "A young heir kept alive by the Sanguine Governor.", "Gentle, sick, angry at being an object.", "Can consent to stabilize or reject the machine.", "Find out whether owing life to a machine removes freedom.", 2, {"heir", "mercy"}),
        makeNpc("npc_ros_parel", "Ros Parel", NPCCategory::Person, "Sump Saints", "Canal captain", "Smuggler-caretaker with evacuation routes and old Mercy records.", "Suspicious and protective.", "Knows the Mercy Key line never died.", "Move people before institutions move ledgers.", 3, {"smuggler", "mercy"}),
        makeNpc("npc_inspector_mara_quill", "Inspector Mara Quill", NPCCategory::Person, "Brass Council", "Rival investigator", "A wet-coated investigator watching who protects evidence and who protects status.", "Curious, sharp, tired.", "Her badge is keyed to a Mirror lineage she does not know she has.", "Solve the Red Tinker murders without becoming Cadrane's pawn.", 2, {"investigator", "opening"})};
}

void configureMaps(Campaign &campaign, const std::filesystem::path &root)
{
    auto mapPath = [&](const std::string &file) { return projectPath(root, packageRoot(root) / "Assets" / "Maps" / file); };

    MapModel tallowgate = makeMap("map_tallowgate_rail_terminus", "Tallowgate Rail Terminus and Fog Yard", "Opening investigation map with customs booth, moving freight cars, crane arm, and rain-slick platforms.", MapScope::Local, 36, 24, mapPath("tallowgate_rail_terminus.bmp"), true, {"opening", "rail", "player-visible"}, 1);
    tallowgate.features = {
        mapFeature("pin_tallowgate_crate", MapFeatureKind::Pin, "Clinic Crate 7B", "Sealed clinic cargo with blood vials and false routing.", "Contains H01 if protected.", 13, 12, true, false, 0),
        mapFeature("pin_signal_tower", MapFeatureKind::NamedLocation, "Signal Tower", "High vantage point and switch control.", "Can move freight cars or block a pursuit.", 28, 5, true, false, 0),
        mapFeature("pin_customs_booth", MapFeatureKind::NamedLocation, "Customs Booth", "Constable records and pension threats.", "Good social pressure point.", 6, 8, true, false, 0)};
    tallowgate.regions = {region("reg_tallowgate_fog", "Fog Bank", "Edges are heavily obscured by rain and steam.", 0, 0, 36, 5, true)};
    tallowgate.tokens = {token("tok_tallowgate_party", "Crew", MapTokenKind::Party, "Party", kCampaignId, 5, 18), token("tok_tallowgate_mara", "Inspector Mara Quill", MapTokenKind::NPC, "NPC", "npc_inspector_mara_quill", 24, 9)};

    MapModel foundry = makeMap("map_holtz_vane_foundry", "Holtz & Vane Foundry Floor", "Riot and rescue map with furnaces, catwalks, cooling trough, and a jammed blood-reactive hauler.", MapScope::Local, 34, 24, mapPath("holtz_vane_foundry_floor.bmp"), false, {"foundry", "riot"}, 2);
    foundry.features = {mapFeature("pin_hauler", MapFeatureKind::Hazard, "Jammed Clockwork Hauler", "Overheating prototype governor.", "Cooling it earns union trust and H09.", 17, 12, true, false, 0), mapFeature("pin_shutdown_wheel", MapFeatureKind::Pin, "Emergency Shutoff Wheel", "One of two wheels required for safe shutdown.", "Hard Spark + Tinker under heat.", 29, 18, true, false, 3)};
    foundry.regions = {region("reg_furnace_heat", "Furnace Heat", "Standing here adds Stress or a Hindrance without protection.", 2, 3, 9, 18, true)};

    MapModel patent = makeMap("map_royal_patent_office", "Royal Patent Office", "Vertical archive with rotating file carousels, pneumatic tubes, ink alarms, and dueling-hall annex.", MapScope::Local, 30, 24, mapPath("royal_patent_office.bmp"), false, {"archive", "stealth"}, 3);
    patent.features = {mapFeature("pin_plate_carousel", MapFeatureKind::SecretDoor, "Patent Plate Carousel", "Rotating archive containing three plates.", "Reveal H03 and point to Meridian.", 15, 11, false, true, 3), mapFeature("pin_ink_alarm", MapFeatureKind::Trap, "Ink Alarm", "Sprays blue-black proof on thieves.", "Failure tags identities for the Countess.", 22, 6, false, true, 2)};

    MapModel tram = makeMap("map_sky_tram_switch", "Sky Tram and Switch Tower", "Suspended tram, cabin, roof, service ladder, cable gantry, and external switching tower.", MapScope::Local, 32, 20, mapPath("sky_tram_switch_tower.bmp"), false, {"tram", "chase"}, 4);
    tram.features = {mapFeature("pin_brake_cabin", MapFeatureKind::Pin, "Cabin Brake", "Can stabilize the tram if paired with roof work.", "Failure swings the tram.", 10, 10, true, false, 0), mapFeature("pin_roof_cable", MapFeatureKind::Hazard, "Fraying Roof Cable", "Sabotage point above the canal.", "Assassin tries to reach this.", 18, 5, true, false, 0)};

    MapModel salon = makeMap("map_porcelain_salon", "Porcelain Countess Salon and Midnight Ward", "Masked parlor, conservatory, servant corridor, clinic ward, and machine-viewing gallery.", MapScope::Local, 32, 24, mapPath("porcelain_countess_salon.bmp"), false, {"social", "clinic", "gala"}, 5);
    salon.features = {mapFeature("pin_songbird_tray", MapFeatureKind::Pin, "Recording Songbirds", "Mechanical birds recording blackmail.", "One contains a Cadrane buyer cylinder.", 12, 8, true, false, 0), mapFeature("pin_cold_room", MapFeatureKind::Door, "Clinic Cold Room", "Hidden blood storage behind salon service wall.", "Contains H05 if saved from fire.", 25, 18, false, true, 3)};

    MapModel underworks = makeMap("map_boiler_choir_underworks", "Boiler Choir Underworks", "Steam locks, municipal pressure galleries, inheritance valve, and submerged pressure chapel.", MapScope::Dungeon, 36, 26, mapPath("boiler_choir_underworks.bmp"), false, {"dungeon", "pressure"}, 6);
    underworks.features = {mapFeature("pin_inheritance_valve", MapFeatureKind::Hazard, "Inheritance Valve", "Blood-reactive pressure valve pulsing off tempo.", "H09 explains how to stabilize or weaponize it.", 19, 13, true, false, 0), mapFeature("pin_pressure_chapel", MapFeatureKind::NamedLocation, "Submerged Engine Chapel", "First failed Sanguine Governor under canal water.", "Reveals H07 and Mercy history.", 30, 20, false, true, 0)};

    MapModel canal = makeMap("map_glasshouse_canal_market", "Glasshouse Canal Market", "Smuggler parley site with narrow boat decks, glass roof, wet walkways, and lock levers.", MapScope::Local, 34, 22, mapPath("glasshouse_canal_market.bmp"), false, {"canal", "parley"}, 7);
    canal.features = {mapFeature("pin_lock_lever", MapFeatureKind::Pin, "Canal Lock Lever", "Raise, flood, or split the market routes.", "Useful for parley and combat.", 18, 11, true, false, 0), mapFeature("pin_saints_records", MapFeatureKind::Pin, "Sump Saints Records", "Oral genealogy tokens and debtor names.", "Ros trades H07 for protection.", 7, 15, true, false, 0)};

    MapModel meridian = makeMap("map_meridian_station_core", "Meridian Station and Sanguine Governor Core", "Rail station, telegraph room, pressure switchboard, armored platform, and final genealogical machine.", MapScope::Dungeon, 40, 28, mapPath("meridian_station_engine_core.bmp"), false, {"finale", "engine"}, 8);
    meridian.features = {mapFeature("pin_telegraph_room", MapFeatureKind::NamedLocation, "Telegraph Room", "Broadcast proof or false order citywide.", "One of three objective nodes.", 7, 6, true, false, 0), mapFeature("pin_pressure_switch", MapFeatureKind::Hazard, "Pressure Switchboard", "Can prevent disaster if secured before the final choice.", "Failure risks infrastructure collapse.", 20, 12, true, false, 0), mapFeature("pin_genealogical_machine", MapFeatureKind::NamedLocation, "Genealogical Machine", "Brass drums of names, contracts, medical tags, and civic charters.", "Final choices happen here.", 31, 20, true, false, 0)};

    campaign.maps = {tallowgate, foundry, patent, tram, salon, underworks, canal, meridian};
    campaign.partyLocation.mapId = "map_tallowgate_rail_terminus";
    campaign.partyLocation.x = 5;
    campaign.partyLocation.y = 18;
    campaign.partyTravel.currentMapId = "map_tallowgate_rail_terminus";
    campaign.partyTravel.routeId = "route_tallowgate_to_clinic";
    campaign.partyTravel.routeName = "Tallowgate to Iona's Clinic";
    campaign.partyTravel.travelPace = "Careful";
    campaign.partyTravel.distanceMiles = 1.8f;
    campaign.partyTravel.milesTraveled = 0.3f;
    campaign.partyTravel.watches = 0;
    campaign.partyTravel.randomEncounterChecks = 0;
    campaign.partyTravel.notes = "Use this route after the opening crate scene; smoke messages can redirect the party to Foundry Row.";
}

void configureEncounters(Campaign &campaign)
{
    MonsterDefinitionModel constable = makeMonster("monster_brass_constable", "Brass Constable", "Trained", 2, 3, "Instinct: contain public disorder. Moves: whistle for backup, form baton line, seize evidence.", {"law", "human"});
    constable.actions = {action("Baton Line", "Attack", 5, 5, "+0", "2 Stress", "On a Spark, separate a target from an objective.")};
    MonsterDefinitionModel guard = makeMonster("monster_aurelian_guard", "Aurelian Gear Guard", "Veteran", 2, 6, "Instinct: protect company property. Moves: suppress, drag, lock down machinery.", {"company", "guard"});
    guard.actions = {action("Pressure Carbine", "Attack", 5, 60, "+0", "3 Stress", "Loud, traceable, and terrifying in crowds.")};
    MonsterDefinitionModel hauler = makeMonster("monster_blood_reactive_hauler", "Blood-Reactive Hauler", "Engine", 3, 10, "Instinct: complete its route. Moves: overheat, crush, vent, follow blood resonance.", {"engine", "hazard"});
    hauler.actions = {action("Crushing Governor Lurch", "Attack", 5, 5, "+0", "4 Stress", "Telegraphed; can be disabled by objective play.")};
    MonsterDefinitionModel surgeon = makeMonster("monster_red_tinker_surgeon", "Red Tinker Surgeon", "Elite", 3, 8, "Instinct: harvest clean samples. Moves: gas, suture, vanish through service routes.", {"doctor", "villain-agent"});
    surgeon.actions = {action("Suture Knife", "Attack", 5, 5, "+0", "3 Stress", "On a Spark, mark a bloodline sample clock.")};
    MonsterDefinitionModel second = makeMonster("monster_clockwork_second", "Clockwork Second", "Veteran", 2, 6, "Instinct: mirror a duelist. Moves: delayed strike, rotating cover, parry by prediction.", {"duel", "construct"});
    second.actions = {action("Delayed Sabre", "Attack", 5, 5, "+0", "3 Stress", "Acts after a PC commits movement.")};
    MonsterDefinitionModel regulator = makeMonster("monster_regulator_rig", "Regulator Rig", "Horror", 4, 12, "Instinct: stabilize at any cost. Moves: clamp, vent, command connected prosthetics.", {"engine", "finale"});
    regulator.actions = {action("Obedience Pulse", "Engine", 30, 30, "+0", "3 Stress", "Targets Pulse; on a Twist, advances civic command.")};
    MonsterDefinitionModel avatar = makeMonster("monster_sovereign_avatar", "Sovereign Engine Avatar", "Mythic Engine", 4, 14, "Instinct: obey the strongest moral logic offered. Moves: rewrite registry, stop clocks, speak through machines.", {"finale", "engine"});
    avatar.actions = {action("Registry Rewrite", "Engine", 60, 60, "+0", "4 Stress", "May be redirected by covenant proof, consent, or a key sacrifice.")};
    campaign.customMonsters = {constable, guard, hauler, surgeon, second, regulator, avatar};

    auto addPcs = [](EncounterModel &encounter) {
        encounter.participants.push_back(participant(encounter.metadata.id + "_iona", CombatantKind::Player, "pc_iona_vale", "", "Iona Vale", 12, 6, 2, true, 3, 4, {"pc"}));
        encounter.participants.push_back(participant(encounter.metadata.id + "_corvin", CombatantKind::Player, "pc_corvin_rake", "", "Corvin Rake", 16, 6, 2, true, 4, 5, {"pc"}));
        encounter.participants.push_back(participant(encounter.metadata.id + "_mirelle", CombatantKind::Player, "pc_mirelle_voss", "", "Mirelle Voss", 14, 6, 2, true, 5, 4, {"pc"}));
        encounter.participants.push_back(participant(encounter.metadata.id + "_tomas", CombatantKind::Player, "pc_tomas_arkwright", "", "Tomas Arkwright", 10, 7, 3, true, 4, 6, {"pc"}));
    };

    EncounterModel e01 = makeEncounter("enc_fog_yard_inspection", "Fog Yard Inspection", "Opening social/exploration scene around a clinic crate, customs pressure, and a union courier.", "map_tallowgate_rail_terminus", 36, 24, {"opening", "social", "evidence"}, 1);
    addPcs(e01);
    e01.participants.push_back(participant("part_e01_constable_a", CombatantKind::Monster, "", "monster_brass_constable", "Brass Constable", 11, 3, 2, false, 12, 12, {"law"}));
    e01.participants.push_back(participant("part_e01_constable_b", CombatantKind::Monster, "", "monster_brass_constable", "Brass Constable", 9, 3, 2, false, 14, 13, {"law"}));
    e01.combatLog = {combatLog("log_e01_setup", 1, "GM", "Scene", "Objective: secure crate seal, keep courier alive, and leave with Dr. Vellum's name.")};

    EncounterModel e02 = makeEncounter("enc_foundry_shift_riot", "Foundry Shift Riot", "Riot/rescue encounter around an overheating blood-reactive hauler.", "map_holtz_vane_foundry", 34, 24, {"riot", "rescue", "machine"}, 2);
    addPcs(e02);
    e02.participants.push_back(participant("part_e02_hauler", CombatantKind::Monster, "", "monster_blood_reactive_hauler", "Blood-Reactive Hauler", 8, 10, 3, false, 17, 12, {"engine"}));
    e02.participants.push_back(participant("part_e02_guard", CombatantKind::Monster, "", "monster_aurelian_guard", "Aurelian Gear Guard", 13, 6, 2, false, 24, 8, {"company"}));

    EncounterModel e03 = makeEncounter("enc_patent_office_burglary", "Patent Office Burglary", "Stealth puzzle through rotating archive carousels and ink alarms.", "map_royal_patent_office", 30, 24, {"stealth", "clue"}, 3);
    e03.theaterOfMind = false;
    addPcs(e03);
    e03.participants.push_back(participant("part_e03_guard", CombatantKind::Monster, "", "monster_aurelian_guard", "Night Archive Guard", 12, 6, 2, false, 19, 7, {"guard"}));

    EncounterModel e04 = makeEncounter("enc_sky_tram_sabotage", "Sky Tram Sabotage", "Three-zone moving hazard to save Lysa Fen while an assassin frames labor radicals.", "map_sky_tram_switch", 32, 20, {"chase", "hazard"}, 4);
    addPcs(e04);
    e04.participants.push_back(participant("part_e04_assassin", CombatantKind::Monster, "", "monster_red_tinker_surgeon", "Spring-Pistol Assassin", 15, 8, 3, false, 18, 5, {"assassin"}));

    EncounterModel e05 = makeEncounter("enc_porcelain_countess_salon", "Porcelain Countess Salon", "High-society social encounter with recording songbirds, mask tells, and inheritance pressure.", "map_porcelain_salon", 32, 24, {"social", "noble"}, 5);
    e05.theaterOfMind = true;
    addPcs(e05);

    EncounterModel e06 = makeEncounter("enc_boiler_choir_underworks", "Boiler Choir Underworks", "Hazard-clock descent through steam locks and an off-tempo inheritance valve.", "map_boiler_choir_underworks", 36, 26, {"dungeon", "hazard"}, 6);
    addPcs(e06);
    e06.participants.push_back(participant("part_e06_regulator", CombatantKind::Monster, "", "monster_regulator_rig", "Valve Guardian Rig", 7, 12, 4, false, 20, 13, {"engine"}));

    EncounterModel e07 = makeEncounter("enc_vellum_clinic_midnight", "Vellum Clinic Midnight Ward", "Investigation and rescue in a clinic extracting hereditary-compatible blood.", "map_porcelain_salon", 32, 24, {"clinic", "investigation"}, 7);
    addPcs(e07);
    e07.participants.push_back(participant("part_e07_surgeon", CombatantKind::Monster, "", "monster_red_tinker_surgeon", "Red Tinker Surgeon", 14, 8, 3, false, 24, 18, {"doctor"}));

    EncounterModel e08 = makeEncounter("enc_glasshouse_canal_parley", "Glasshouse Canal Parley", "Social/combat optional parley with Sump Saints and excise officers.", "map_glasshouse_canal_market", 34, 22, {"canal", "parley"}, 8);
    addPcs(e08);
    e08.participants.push_back(participant("part_e08_constable", CombatantKind::Monster, "", "monster_brass_constable", "Excise Officer", 11, 3, 2, false, 20, 12, {"law"}));

    EncounterModel e09 = makeEncounter("enc_clockmaker_dueling_hall", "Clockmaker's Dueling Hall", "Formal duel, puzzle duel, or engineering race against clockwork seconds.", "map_royal_patent_office", 30, 24, {"duel", "social"}, 9);
    addPcs(e09);
    e09.participants.push_back(participant("part_e09_second_a", CombatantKind::Monster, "", "monster_clockwork_second", "Clockwork Second", 16, 6, 2, false, 12, 10, {"construct"}));
    e09.participants.push_back(participant("part_e09_second_b", CombatantKind::Monster, "", "monster_clockwork_second", "Clockwork Second", 13, 6, 2, false, 16, 10, {"construct"}));

    EncounterModel e10 = makeEncounter("enc_bloodline_gala_attempt", "Bloodline Gala Attempt", "Protect Elian and the alderwoman's evidence during a masked assassination attempt.", "map_porcelain_salon", 32, 24, {"gala", "assassination"}, 10);
    addPcs(e10);
    e10.participants.push_back(participant("part_e10_assassin", CombatantKind::Monster, "", "monster_red_tinker_surgeon", "Cello-Case Assassin", 15, 8, 3, false, 21, 6, {"assassin"}));

    EncounterModel e11 = makeEncounter("enc_submerged_engine_chapel", "Submerged Engine Chapel", "Exploration and potential fight in the first failed Sanguine Governor chapel.", "map_boiler_choir_underworks", 36, 26, {"chapel", "engine"}, 11);
    addPcs(e11);
    e11.participants.push_back(participant("part_e11_regulator", CombatantKind::Monster, "", "monster_regulator_rig", "Diving-Rig Guardian", 9, 12, 4, false, 30, 20, {"guardian"}));

    EncounterModel e12 = makeEncounter("enc_siege_meridian_station", "Siege of Meridian Station", "Multi-front station siege with crowd panic, train departure, pressure overload, and evidence broadcast clocks.", "map_meridian_station_core", 40, 28, {"station", "siege"}, 12);
    addPcs(e12);
    e12.participants.push_back(participant("part_e12_guard_a", CombatantKind::Monster, "", "monster_aurelian_guard", "Aurelian Guard", 12, 6, 2, false, 24, 12, {"company"}));
    e12.participants.push_back(participant("part_e12_regulator", CombatantKind::Monster, "", "monster_regulator_rig", "Platform Regulator", 8, 12, 4, false, 30, 18, {"engine"}));

    EncounterModel e13 = makeEncounter("enc_genealogical_machine", "The Genealogical Machine", "Finale inside the Sanguine Governor core; combat optional, moral choice mandatory.", "map_meridian_station_core", 40, 28, {"finale", "choice"}, 13);
    addPcs(e13);
    e13.participants.push_back(participant("part_e13_avatar", CombatantKind::Monster, "", "monster_sovereign_avatar", "Sovereign Engine Avatar", 20, 14, 4, false, 31, 20, {"finale"}));

    campaign.encounters = {e01, e02, e03, e04, e05, e06, e07, e08, e09, e10, e11, e12, e13};
}

void configureAudio(Campaign &campaign, const std::filesystem::path &root)
{
    for (const AudioSeed &seed : audioSeeds())
    {
        AudioAssetModel asset;
        setMetadata(asset.metadata, seed.id, seed.tags);
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

    auto playlist = [](const std::string &id, const std::string &name, std::vector<EntityId> assets, bool repeat, bool shuffle = false) {
        AudioPlaylistModel model;
        setMetadata(model.metadata, id, {"playlist"});
        model.name = name;
        model.audioAssetIds = std::move(assets);
        model.repeat = repeat;
        model.shuffle = shuffle;
        model.crossfadeSeconds = 2.0f;
        return model;
    };
    campaign.audioPlaylists = {
        playlist("playlist_tallowgate_rain", "Tallowgate Rain and Market", {"audio_rain", "audio_market_day", "audio_short_alert"}, true),
        playlist("playlist_patent_intrigue", "Patent Office Intrigue", {"audio_mystery", "audio_scifi_pulse", "audio_shadow_whisper"}, true),
        playlist("playlist_foundry_danger", "Foundry and Underworks Danger", {"audio_deep_rumble", "audio_underwater_engine", "audio_scifi_urgent"}, true),
        playlist("playlist_combat_pressure", "Combat Under Pressure", {"audio_medieval_battle", "audio_orchestral_battle", "audio_weapon_swing"}, false),
        playlist("playlist_salon_masks", "Salon Masks", {"audio_old_tower_inn", "audio_coin_pouch", "audio_shadow_whisper"}, true),
        playlist("playlist_finale_engine", "Finale Engine", {"audio_fighting_not_option", "audio_deep_rumble", "audio_deep_cast", "audio_arcane_pulse"}, true),
        playlist("playlist_hard_won_dawn", "Hard-Won Dawn", {"audio_victory"}, false)};

    auto layer = [](const std::string &id, const std::string &name, const std::string &assetId, float volume, bool active, int sort) {
        AudioAmbienceLayerModel model;
        setMetadata(model.metadata, id, {"ambience"});
        model.name = name;
        model.audioAssetId = assetId;
        model.volume = volume;
        model.loop = true;
        model.active = active;
        model.sortOrder = sort;
        return model;
    };
    campaign.audioAmbienceLayers = {
        layer("ambience_tallowgate_rain", "Tallowgate Rain", "audio_rain", 0.34f, true, 1),
        layer("ambience_engine_rumble", "Deep Engine Rumble", "audio_deep_rumble", 0.28f, false, 2),
        layer("ambience_canals", "Canal Depths", "audio_deep_sea", 0.30f, false, 3),
        layer("ambience_clinic_cold", "Cold Clinic Ward", "audio_forgotten_tomb", 0.24f, false, 4)};

    auto trigger = [](const std::string &id, const std::string &name, const std::string &sourceId, const std::string &audioId, float volume, bool loop) {
        AudioTriggerModel model;
        setMetadata(model.metadata, id, {"trigger"});
        model.name = name;
        model.sourceType = "StoryBeat";
        model.sourceEntityId = sourceId;
        model.eventName = "Scene Started";
        model.audioAssetId = audioId;
        model.volume = volume;
        model.loop = loop;
        model.enabled = true;
        return model;
    };
    campaign.audioTriggers = {
        trigger("trigger_tallowgate_open", "Start Tallowgate Rain", "beat_tallowgate_arrival", "audio_rain", 0.35f, true),
        trigger("trigger_customs_whistle", "Customs Whistle", "beat_tallowgate_arrival", "audio_short_alert", 0.75f, false),
        trigger("trigger_foundry_riot", "Foundry Rumble", "beat_foundry_shift_riot", "audio_deep_rumble", 0.32f, true),
        trigger("trigger_patent_office", "Patent Mystery", "beat_patent_burglary", "audio_mystery", 0.50f, true),
        trigger("trigger_salon", "Salon Bed", "beat_porcelain_salon", "audio_old_tower_inn", 0.44f, true),
        trigger("trigger_clinic", "Clinic Bottles", "beat_vellum_clinic", "audio_bottle_clink", 0.70f, false),
        trigger("trigger_meridian", "Meridian Alarm", "beat_meridian_siege", "audio_scifi_urgent", 0.56f, true),
        trigger("trigger_finale", "Finale Engine", "beat_genealogical_machine", "audio_fighting_not_option", 0.70f, true)};

    campaign.audioVolumeGroups.master = 0.9f;
    campaign.audioVolumeGroups.music = 0.76f;
    campaign.audioVolumeGroups.ambience = 0.68f;
    campaign.audioVolumeGroups.effects = 0.84f;
    campaign.audioVolumeGroups.voice = 1.0f;
}

void configureStory(Campaign &campaign)
{
    StoryThreadModel main;
    setMetadata(main.metadata, "story_harmonious_succession", {"main", "campaign"});
    main.title = "The Harmonious Succession";
    main.type = StoryThreadType::MainStory;
    main.status = StoryStatus::Seeded;
    main.summary = "Lord-Mechanist Cadrane uses Red Tinker murders, clinic contracts, and civic machinery to collect four bloodline keys and crown himself through the Sovereign Engine.";
    main.stakes = "Veyrbridge may become free, collapse, reform, or obey one immortal command.";
    main.gmNotes = "Every session should show a public institution doing one useful thing and one monstrous thing.";
    main.beats = {
        beat("beat_tallowgate_arrival", "Session 1 - Thirteen Bells at Tallowgate", "The party enters the rain-soaked rail yard as a clinic crate and union punchcard expose the first bloodline route.", "A wet customs dispute turns into a medical and labor mystery.", "Rain rings on the iron roof as every public clock strikes thirteen.", "H01 crate manifest; H02 union punchcard; Mara Quill watches the party.", "If the crate is lost, a vial breaks and smoke names Iona.", "Point all PC ties at the nameless ticking-heart child.", StoryStatus::Seeded, "Jubilee minus 9 days", "audio_rain", {"session1", "opening"}, {storyLink("link_b1_map", StoryLinkType::Reveals, "map_tallowgate_rail_terminus", "Map", "opening map")}),
        beat("beat_foundry_shift_riot", "Session 2 - Foundry Shift Riot", "A worker collapses with glass veins and a blood-reactive hauler threatens apprentices.", "The foundry floor becomes a rescue, riot, and proof scene.", "Heat rolls over you in sheets while the hauler ticks like a second heart.", "H09 valve diagram; worker names; Aurelian guard orders.", "Union trust depends on who the party protects under pressure.", "Make class politics personal and name the workers.", StoryStatus::Planned, "Jubilee minus 8 days", "audio_deep_rumble", {"session2", "foundry"}),
        beat("beat_patent_burglary", "Session 3 - Patent Office Burglary", "The Sanguine Governor appears in patent plates under a false title.", "The party can steal, bargain, or embarrass its way through rotating archives.", "The archive stacks turn with a sigh, closing one aisle and opening another.", "H03 patent plate; ink alarm identities; Countess summons.", "Partial success sends them to the salon under social debt.", "Reward clever noncombat solutions.", StoryStatus::Planned, "Jubilee minus 7 days", "audio_mystery", {"session3", "archive"}),
        beat("beat_sky_tram_sabotage", "Session 4 - Sky Tram Sabotage", "A witness is targeted during a pressure failure above the canal.", "The tram becomes a moving rescue scene with a hidden assassin.", "The cabin drops, the canal swings below, and the brake wheel screams red-hot.", "H06 routing clue if Lysa survives; union bolt frame-up.", "If Lysa dies, her receipt survives but trust suffers.", "Use roof, cabin, and tower as distinct zones.", StoryStatus::Planned, "Jubilee minus 6 days", "audio_scifi_urgent", {"session4", "chase"}),
        beat("beat_porcelain_salon", "Session 5 - Porcelain Countess Salon", "Masks, genealogy, recording songbirds, and noble cruelty reveal Mirror and Crown pressure.", "The party negotiates in a room where manners are weapons.", "Porcelain masks turn as one when your name is announced.", "H04 guest cards; songbird blackmail; Elian rumors.", "Public accusations without proof create debt and press retaliation.", "Let Mirelle shine; make every favor cost shape.", StoryStatus::Planned, "Jubilee minus 5 days", "audio_old_tower_inn", {"session5", "social"}),
        beat("beat_vellum_clinic", "Session 6 - Vellum Clinic Midnight Ward", "The charity clinic extracts hereditary-compatible blood under fraudulent consent.", "Patients signed contracts they could not refuse.", "Behind the ward screen, a young heir asks whether being saved can make you property.", "H05 consent fraud; Elian as victim; cold-room samples.", "Careless exposure harms patients; planned protection creates allies.", "Iona's choices should matter here.", StoryStatus::Planned, "Jubilee minus 4 days", "audio_forgotten_tomb", {"session6", "clinic"}),
        beat("beat_engine_chapel", "Session 7 - Boiler Choir and Submerged Chapel", "The party descends through municipal pressure systems to find the first failed Governor.", "Boiler monks, old Mercy records, and underwater machinery reveal the original sin.", "The hymns are maintenance calls, and the altar is a regulator tank.", "H07 Sump genealogy; H09 valve; Mercy Key truth.", "Success gives pressure control for the finale.", "Split into two sessions if the table loves dungeons.", StoryStatus::Planned, "Jubilee minus 3 days", "audio_underwater_engine", {"session7", "dungeon"}),
        beat("beat_meridian_siege", "Session 8 - Siege of Meridian Station", "Workers, constables, smugglers, and nobles converge as the Governor moves by armored train.", "The party chooses which truth reaches the city.", "A station clock shudders. Then every platform loudspeaker breathes your names.", "H06 routing; H10 broadsheet; public broadcast node.", "Secured objectives reduce finale disaster.", "Run as clocks: crowd panic, train departure, pressure overload, evidence broadcast.", StoryStatus::Planned, "Jubilee eve", "audio_scifi_urgent", {"session8", "siege"}),
        beat("beat_genealogical_machine", "Session 9 - The Genealogical Machine", "The final Engine core asks who may command a city and what consent means under crisis.", "The party can sever, rewrite, destroy, crown, or heal the Engine.", "Brass drums turn with names, contracts, medical tags, and prayers. The machine waits for a moral instruction.", "H08 covenant; H11 choice matrix; Cadrane's Morrow Spool.", "The ending changes public law, labor, divinity, and every PC bond.", "Combat is optional; the final decision is not.", StoryStatus::Planned, "Jubilee night", "audio_fighting_not_option", {"session9", "finale"})};
    main.links = {
        storyLink("link_main_villain", StoryLinkType::Contains, "npc_lord_mechanist_severin_cadrane", "NPC", "villain"),
        storyLink("link_main_finale", StoryLinkType::BranchesTo, "enc_genealogical_machine", "Encounter", "finale")};

    StoryThreadModel pcHooks;
    setMetadata(pcHooks.metadata, "story_party_clockwork_bonds", {"character", "pc-hooks"});
    pcHooks.title = "The Crew's Clockwork Bonds";
    pcHooks.type = StoryThreadType::CharacterStory;
    pcHooks.status = StoryStatus::Seeded;
    pcHooks.summary = "Iona's illegal medicine, Corvin's smoke-voices, Mirelle's debt machine, and Tomas's relic-core are all entrances into the same bloodline conspiracy.";
    pcHooks.stakes = "Each PC can become a key, protect a key, or redefine what the Engine thinks consent means.";
    pcHooks.beats = {
        beat("beat_iona_wrong_heart", "Iona - The Wrong Heart", "A patient arrives with Iona's surgical mark and a Voss debt brand.", "The cure may start a succession crisis.", "", "Alaric's prayers; Kreel's warrants; Mercy No. 7 side effects.", "Iona can save people and still make institutions hunt them.", "Offer triage, not simple healing errands.", StoryStatus::Seeded, "Recurring", "audio_bottle_clink", {"iona"}),
        beat("beat_corvin_sister_smoke", "Corvin - Nari in the Smoke", "Smoke messages and transfer cards point toward Nari inside the automaton program.", "The missing sister may not be fully herself.", "", "Nari's punchcard; Strake's warning; Furnace families.", "Rescue may require sabotaging a station Tomas swore to protect.", "Make worker names concrete.", StoryStatus::Seeded, "Recurring", "audio_shadow_whisper", {"corvin"}),
        beat("beat_mirelle_family_machine", "Mirelle - The Voss Account", "The Voss ancestor-machine predicts the PCs and offers impossible leverage.", "A family name can be restored or abolished.", "", "Aunt Ysabet; debt contracts; Auditor favors.", "Mirelle can win by becoming what they hate unless checked.", "Use manners and paperwork as weapons.", StoryStatus::Seeded, "Recurring", "audio_coin_pouch", {"mirelle"}),
        beat("beat_tomas_relic_prison", "Tomas - The Prisoned God", "The relic-core claims the Argent Engine is a prison built around a god.", "Discipline keeps Tomas whole, but every answered prayer tightens bars.", "", "Malvek's orders; Ada's warnings; core heat.", "Tomas can redeem, reveal, or rupture the Engine temple order.", "Let duty and mercy collide.", StoryStatus::Seeded, "Recurring", "audio_deep_cast", {"tomas"})};
    pcHooks.links = {
        storyLink("link_pc_iona", StoryLinkType::Contains, "pc_iona_vale", "Character", "Mercy Key pressure"),
        storyLink("link_pc_corvin", StoryLinkType::Contains, "pc_corvin_rake", "Character", "Furnace Key pressure"),
        storyLink("link_pc_mirelle", StoryLinkType::Contains, "pc_mirelle_voss", "Character", "Mirror Key pressure"),
        storyLink("link_pc_tomas", StoryLinkType::Contains, "pc_tomas_arkwright", "Character", "Argent Engine pressure")};

    StoryThreadModel factions;
    setMetadata(factions.metadata, "story_city_institutions", {"faction", "city"});
    factions.title = "Institutions Under Pressure";
    factions.type = StoryThreadType::FactionStory;
    factions.status = StoryStatus::Seeded;
    factions.summary = "Government, nobility, labor, temple, information brokers, industry, and undercity aid networks all hold one true reason and one dangerous lie.";
    factions.stakes = "Allies decide whether the finale is a disaster, reform, revolution, or compromise.";
    factions.links = {
        storyLink("link_clockwork_faction_council", StoryLinkType::Contains, "faction_brass_council", "Faction", "law"),
        storyLink("link_faction_union", StoryLinkType::Contains, "faction_gearwright_union", "Faction", "labor"),
        storyLink("link_faction_synod", StoryLinkType::Contains, "faction_ashglass_synod", "Faction", "temple"),
        storyLink("link_faction_saints", StoryLinkType::Contains, "faction_sump_saints", "Faction", "mercy")};

    campaign.storyThreads = {main, pcHooks, factions};

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
        event("timeline_thirteen_bells", "Thirteen Bells", "Every public clock strikes thirteen as the Tallowgate crate is seized.", "Jubilee minus 9 days", {"beat_tallowgate_arrival", "handout_h01_manifest"}, {"opening"}),
        event("timeline_foundry_collapse", "Foundry Collapse Averted or Exploited", "The hauler incident gives the party union trust or company suspicion.", "Jubilee minus 8 days", {"beat_foundry_shift_riot", "enc_foundry_shift_riot"}, {"foundry"}),
        event("timeline_patent_theft", "Inheritance Apparatus Identified", "Patent plates prove the Governor is built for veins and names.", "Jubilee minus 7 days", {"handout_h03_patent", "beat_patent_burglary"}, {"clue"}),
        event("timeline_clinic_truth", "Consent Fraud Exposed", "Vellum Clinic's contracts reveal bloodline harvesting by wage pressure.", "Jubilee minus 4 days", {"handout_h05_consent", "npc_elian_ashworth_lex"}, {"clinic"}),
        event("timeline_meridian_move", "Governor Core Moves", "Cadrane attempts to move the core through Meridian Station under emergency cover.", "Jubilee eve", {"beat_meridian_siege", "map_meridian_station_core"}, {"siege"}),
        event("timeline_jubilee", "Jubilee of Gears", "The city synchronizes bells, rail schedules, boilers, and public clocks for the final command signal.", "Jubilee night", {"beat_genealogical_machine", "enc_genealogical_machine"}, {"finale"})};
}

void configureHandoutsShopsLootDungeonSessions(Campaign &campaign, const std::filesystem::path &root)
{
    auto hPath = [&](const std::string &file) { return projectPath(root, packageRoot(root) / "Assets" / "Handouts" / file); };
    campaign.handouts = {
        handout("handout_h01_manifest", "H01 - Tallowgate Crate Manifest", "Clinic cargo routed under emergency exemption with HEIR RESERVE vials.", hPath("h01_tallowgate_crate_manifest.txt"), "Opening evidence. If missed, smoke names Vellum later.", {"player", "opening", "clue"}),
        handout("handout_h02_punchcard", "H02 - Union Punchcard", "Missing worker names and Furnace Key transfer code.", hPath("h02_union_punchcard.txt"), "Tie Corvin and the union to the main plot.", {"player", "union", "clue"}),
        handout("handout_h03_patent", "H03 - Patent Plate Excerpt", "The Governor intake is shaped for living veins, not fuel lines.", hPath("h03_patent_plate_excerpt.txt"), "Core mechanical proof.", {"player", "patent", "clue"}),
        handout("handout_h04_guest_cards", "H04 - Countess Guest Cards", "Mask maker marks map to Crown, Furnace, Mirror, and Mercy genealogies.", hPath("h04_countess_guest_cards.txt"), "Use for salon social deduction.", {"player", "social"}),
        handout("handout_h05_consent", "H05 - Vellum Clinic Consent", "A contract signed under wage relief pressure.", hPath("h05_vellum_clinic_consent.txt"), "Best proof of medical consent fraud.", {"player", "clinic"}),
        handout("handout_h06_telegram", "H06 - Meridian Routing Telegram", "Move the governor core by private rail at Jubilee minus six hours.", hPath("h06_meridian_routing_telegram.txt"), "Points to Session 8.", {"player", "station"}),
        handout("handout_h07_genealogy", "H07 - Sump Saints Genealogy", "The Mercy Key was hidden among nameless wards and debt children.", hPath("h07_sump_saints_genealogy.txt"), "Humanizes Mercy lineage.", {"player", "mercy"}),
        handout("handout_h08_covenant", "H08 - Engine Covenant Fragment", "No command is lawful unless the city consents.", hPath("h08_engine_covenant_fragment.txt"), "Can redirect the final Engine.", {"player", "finale"}),
        handout("handout_h09_valve", "H09 - Inheritance Valve Diagram", "Blood-reactive governor diagram for boiler systems.", hPath("h09_inheritance_valve_diagram.txt"), "Gives pressure-control advantage in finale.", {"player", "engine"}),
        handout("handout_h10_broadsheet", "H10 - Red Tinker Broadsheet", "Printed murder times match Aurelian shift changes.", hPath("h10_red_tinker_broadsheet.txt"), "Public narrative clue.", {"player", "mystery"}),
        handout("handout_h11_choice", "H11 - Finale Choice Matrix", "Sever, Rewrite, Destroy, Crown, or Heal.", hPath("h11_finale_choice_matrix.txt"), "Reveal just before finale.", {"player", "finale"})};

    ShopModel clinic = shop("shop_iona_clinic_cache", "Iona's Hidden Clinic Cache", "Elsbet Marr", "Back-alley clinic beneath a closed watchmaker's shop.", {"shop", "clinic"});
    clinic.stock = {stock("item_mercy_no_7", 6, 0.9, "Limited", true), stock("item_brass_respirator", 3, 1.0, "Common", true), stock("item_condensed_aether_battery", 4, 1.0, "Common", true), stock("item_clockwork_cardiac_key", 1, 2.0, "Story Locked", false)};
    ShopModel glasshouse = shop("shop_glasshouse_parts", "Glasshouse Canal Parts", "Ros Parel", "Smuggled engine parts, forged identity seals, and evacuation favors.", {"shop", "canal"});
    glasshouse.stock = {stock("item_smoke_egg", 8, 1.0, "Common", true), stock("item_rooftop_grapnel", 2, 1.1, "Limited", false), stock("item_false_press_badge", 3, 1.2, "Risky", false), stock("item_inheritance_valve_diagram", 1, 1.5, "Story Locked", false)};
    ShopModel lacquer = shop("shop_lacquer_court_favors", "Lacquer Court Favors", "The Porcelain Countess", "Masks, credentials, debt rumors, and identity work that never comes clean.", {"shop", "social"});
    lacquer.stock = {stock("item_caldris_vow_seal", 1, 1.8, "Rare", false), stock("item_spirit_debt_ledger", 1, 2.0, "Dangerous", false), stock("item_ivory_dueling_pistol", 1, 1.25, "Limited", false)};
    campaign.shops = {clinic, glasshouse, lacquer};

    LootParcelModel rail = loot("loot_tallowgate_evidence", "Tallowgate Evidence Bundle", "Fog Yard Inspection", "Award for keeping the courier and crate seal safe.", {"loot", "evidence"});
    rail.items = {lootItem("item_false_press_badge", 1, "Taken from a broadsheet runner."), lootItem("item_mercy_no_7", 1, "Cracked but usable.")};
    rail.silver = 8;
    LootParcelModel foundry = loot("loot_foundry_salvage", "Foundry Salvage", "Foundry Shift Riot", "Recovered from the cooled hauler and grateful workers.", {"loot", "foundry"});
    foundry.items = {lootItem("item_inheritance_valve_diagram", 1, "Stamped Holtz & Vane confidential."), lootItem("item_condensed_aether_battery", 3, "Still warm.")};
    foundry.gold = 30;
    LootParcelModel salon = loot("loot_countess_blackmail", "Countess Blackmail Cylinder", "Porcelain Salon", "Can buy access or burn social capital.", {"loot", "social"});
    salon.items = {lootItem("item_caldris_vow_seal", 1, "Only if the party wins the private meeting.")};
    salon.gold = 20;
    LootParcelModel finale = loot("loot_engine_aftermath", "Engine Aftermath", "The Genealogical Machine", "Use after the final choice; rewards are political as much as material.", {"loot", "finale"});
    finale.items = {lootItem("item_sovereign_engine_shard", 1, "Only if the Engine is bound, healed, or broken with care.")};
    finale.platinum = 1;
    finale.gold = 250;
    campaign.lootParcels = {rail, foundry, salon, finale};

    DungeonModel dungeon;
    setMetadata(dungeon.metadata, "dungeon_underworks_governor", {"dungeon", "engine", "finale"});
    dungeon.name = "Underworks and Sanguine Governor";
    dungeon.description = "Room-by-room route from boiler galleries to the submerged chapel, Meridian station service tunnels, and the final Engine core.";
    dungeon.parentMapId = "map_boiler_choir_underworks";
    dungeon.mapIds = {"map_boiler_choir_underworks", "map_meridian_station_core"};
    dungeon.activeRoomId = "room_steam_lock";
    dungeon.gmNotes = "Alliances change room hazards: union aid lowers pressure, Sump aid opens escape, Synod aid reveals oath locks.";
    DungeonRoomModel lock = room("room_steam_lock", "Steam Lock", "Entry chamber with prayer-marked gauges and cycling pressure doors.", "The door breathes like a sleeping giant.", "A scratched symbol of Mother Morrow points to a safe sequence.", 3, 3, DungeonRoomExplorationState::Seen, {"entry"});
    DungeonRoomModel choir = room("room_boiler_choir", "Boiler Choir Gallery", "Monks maintain pressure through call-and-response procedure.", "The hymn is almost beautiful until one voice slips off tempo.", "The leader knows the valve is wrong but cannot halt peak draw.", 9, 7, DungeonRoomExplorationState::Unknown, {"social", "hazard"});
    DungeonRoomModel valve = room("room_inheritance_valve", "Inheritance Valve", "Blood-reactive governor installed in the municipal pressure line.", "A red pulse travels through brass veins in the wall.", "Can locate a key, burst the gallery, or stabilize finale pressure.", 17, 12, DungeonRoomExplorationState::Unknown, {"objective"});
    valve.encounterIds = {"enc_boiler_choir_underworks"};
    DungeonRoomModel chapel = room("room_submerged_chapel", "Submerged Engine Chapel", "Failed Governor prototype under canal water.", "The altar tank exhales bubbles that sound like names.", "Reveals Mercy history and the cost of destroying the final Engine.", 28, 20, DungeonRoomExplorationState::Unknown, {"chapel"});
    chapel.encounterIds = {"enc_submerged_engine_chapel"};
    DungeonRoomModel service = room("room_meridian_service", "Meridian Service Tunnels", "Tight utility route beneath the station.", "Telegraph pulses tick overhead like trapped insects.", "Sister Ada can leave a hidden map here.", 34, 14, DungeonRoomExplorationState::Unknown, {"station"});
    DungeonRoomModel core = room("room_genealogical_core", "Genealogical Machine Core", "Brass memory drums of names, contracts, medical tags, and civic charters.", "Every drum turns toward you, waiting for a command.", "Each final option needs a different protected objective.", 42, 24, DungeonRoomExplorationState::Unknown, {"finale"});
    core.encounterIds = {"enc_genealogical_machine"};
    lock.connections = {roomConnection("conn_lock_choir", "room_boiler_choir", "Gauge stair")};
    choir.connections = {roomConnection("conn_choir_valve", "room_inheritance_valve", "Pressure gallery")};
    valve.connections = {roomConnection("conn_valve_chapel", "room_submerged_chapel", "Flooded crawl", true), roomConnection("conn_valve_service", "room_meridian_service", "Maintenance rail", false, true, false)};
    chapel.connections = {roomConnection("conn_chapel_service", "room_meridian_service", "Canal culvert", false, true, false)};
    service.connections = {roomConnection("conn_service_core", "room_genealogical_core", "Telegraph conduit")};
    dungeon.rooms = {lock, choir, valve, chapel, service, core};
    campaign.dungeons = {dungeon};

    auto session = [](const std::string &id, const std::string &title, const std::string &summary, std::vector<EntityId> encounters, std::vector<EntityId> docs) {
        SessionModel model;
        setMetadata(model.metadata, id, {"session", "campaign"});
        model.title = title;
        model.summary = summary;
        model.recap = "Not yet played.";
        model.unresolvedThreads = "Which keys are PCs, NPCs, or shared inheritances? What institution will the party trust under pressure?";
        model.xpMilestones = "Advance after each major public truth, alliance, or Engine objective.";
        model.participantCharacterIds = {"pc_iona_vale", "pc_corvin_rake", "pc_mirelle_voss", "pc_tomas_arkwright"};
        model.encounterIds = std::move(encounters);
        model.storyDocumentIds = std::move(docs);
        return model;
    };

    campaign.sessions = {
        session("session_clockwork_01", "Session 1: Thirteen Bells at Tallowgate", "Opening crate dispute, union courier, nameless ticking-heart child, and the first Red Tinker clue.", {"enc_fog_yard_inspection"}, {"doc_clockwork_runbook", "doc_cinder_vow_engine"}),
        session("session_clockwork_02", "Session 2: Foundry Shift Riot", "Worker collapse, jammed hauler, union trust, and the first inheritance valve proof.", {"enc_foundry_shift_riot"}, {"doc_clockwork_clue_web"}),
        session("session_clockwork_03", "Session 3: Patent Office Burglary", "Rotating archive heist, ink alarms, false patents, and social debt to the Countess.", {"enc_patent_office_burglary"}, {"doc_clockwork_clue_web"}),
        session("session_clockwork_04", "Session 4: Sky Tram Sabotage", "Moving rescue to save Lysa Fen, expose the frame-up, and secure Meridian routing.", {"enc_sky_tram_sabotage"}, {"doc_clockwork_clue_web"}),
        session("session_clockwork_05", "Session 5: Masks and Mercy", "Porcelain salon, clinic cold room, Elian's question, and noble genealogy pressure.", {"enc_porcelain_countess_salon", "enc_vellum_clinic_midnight"}, {"doc_clockwork_runbook"}),
        session("session_clockwork_06", "Session 6: Underworks Hymn", "Boiler choir, inheritance valve, submerged chapel, and Mercy lineage truth.", {"enc_boiler_choir_underworks", "enc_submerged_engine_chapel"}, {"doc_clockwork_clue_web"}),
        session("session_clockwork_07", "Session 7: Glasshouse Bargains", "Canal parley, Sump Saints evacuation routes, clockmaker duel, and final alliance choices.", {"enc_glasshouse_canal_parley", "enc_clockmaker_dueling_hall"}, {"doc_clockwork_runbook"}),
        session("session_clockwork_08", "Session 8: Siege of Meridian Station", "Multi-front public crisis before the Governor core departs.", {"enc_siege_meridian_station"}, {"doc_clockwork_finale_matrix"}),
        session("session_clockwork_09", "Session 9: The Genealogical Machine", "Final Engine core, Cadrane's grief, and the city's new covenant.", {"enc_genealogical_machine"}, {"doc_clockwork_finale_matrix"})};

    SessionNoteModel note1;
    setMetadata(note1.metadata, "note_clockwork_start_audio", {"cue"});
    note1.timestampUtc = currentUtcTimestamp();
    note1.body = "Start Tallowgate Rain and Market playlist; reveal H01 only if the crate is protected or torn open.";
    note1.linkedEntityId = "playlist_tallowgate_rain";
    note1.linkedEntityType = "AudioPlaylist";
    note1.linkedEntityLabel = "Tallowgate Rain and Market";
    SessionNoteModel note2;
    setMetadata(note2.metadata, "note_clockwork_player_safe", {"player-safe"});
    note2.timestampUtc = currentUtcTimestamp();
    note2.body = "Stage Tallowgate Rail Terminus, visible NPCs Mara Quill and Ma Jessa Pike, and public handouts H01/H02.";
    note2.linkedEntityId = "beat_tallowgate_arrival";
    note2.linkedEntityType = "StoryBeat";
    note2.linkedEntityLabel = "Session 1 - Thirteen Bells at Tallowgate";
    campaign.sessions.front().notes = {note1, note2};
}

void configureCampaignExtras(Campaign &campaign)
{
    campaign.partyInventory.name = "Crew Evidence and Emergency Gear";
    campaign.partyInventory.ownerEntityId = kCampaignId;
    campaign.partyInventory.gold = 42;
    campaign.partyInventory.silver = 80;
    campaign.partyInventory.entries = {
        makeEntry("party_inv_manifest", "item_inheritance_valve_diagram", 1, false, "Evidence Folder"),
        makeEntry("party_inv_ampoules", "item_mercy_no_7", 2, false, "Medical Pouch"),
        makeEntry("party_inv_batteries", "item_condensed_aether_battery", 3, false, "Tool Crate"),
        makeEntry("party_inv_smoke", "item_smoke_egg", 2, false, "Shared Gear")};
    campaign.partyVehicles = {"Borrowed canal skiff", "Union rooftop signal line", "Aurelian armored train access (conditional)"};
    campaign.partyStrongholds = {"Iona's clinic under the watchmaker shop", "Sump Saints canal safehouse", "Boiler Choir side chapel (if earned)"};
    campaign.partyRenown = 8;
    campaign.partyReputation = "Known in the lower wards as dangerous helpers who did not let the first crate vanish quietly.";

    RulesetDefinitionModel rules;
    setMetadata(rules.metadata, "ruleset_cinder_vow_engine", {"ruleset", "homebrew", "clockwork"});
    rules.name = "Cinder Vow Engine";
    rules.version = "1.0";
    rules.baseRulesetId = "original";
    rules.installedGlobally = true;
    rules.definitionJson = R"json({"core":"Roll Attribute + Skill + Asset dice minus Hindrance dice. Each d6 showing 5 or 6 is a Hit; each 6 is a Spark.","attributes":["Iron","Spark","Veil","Grace","Lore","Pulse"],"difficulties":{"Simple":1,"Risky":2,"Hard":3,"Severe":4,"Mythic":5},"resources":["Momentum","Stress","Injury","Favor","Stain","Ethertech Charges"],"notes":"Original steampunk homebrew ruleset for Clockwork Bloodline."})json";
    campaign.rulesetDefinitions = {rules};

    RulesetContentPackModel pack;
    setMetadata(pack.metadata, "pack_clockwork_bloodline", {"homebrew", "content-pack"});
    pack.rulesetId = "ruleset_cinder_vow_engine";
    pack.name = "Clockwork Bloodline Campaign Pack";
    pack.version = "1.0";
    pack.author = "Codex multi-agent seed";
    pack.license = "Project-local original content";
    pack.importedAtUtc = currentUtcTimestamp();
    pack.source = "tools/seed_clockwork_bloodline.cpp";
    pack.definitionJson = "{\"pcs\":4,\"sessions\":9,\"encounters\":13,\"maps\":8,\"handouts\":11,\"audio_cues\":27}";
    campaign.rulesetContentPacks = {pack};

    HouseRuleModel consent;
    setMetadata(consent.metadata, "house_rule_consent_keys", {"finale", "keys"});
    consent.label = "Bloodline Keys Require Consent";
    consent.description = "A key cannot fully awaken the Engine by force. It must consent, be deceived into consent, or be represented by someone who loves them. The party can break deception with proof, public witness, or sacrifice.";
    consent.changeHistory = {{"2026-05-25T00:00:00Z", "Added for Clockwork Bloodline finale."}};
    HouseRuleModel failForward;
    setMetadata(failForward.metadata, "house_rule_smoke_fail_forward", {"mystery", "clues"});
    failForward.label = "Smoke Carries Missed Clues";
    failForward.description = "If the table misses a core clue, return it through smoke messages, debt ledgers, patient symptoms, public clocks, witness fear, or malfunctioning civic machinery.";
    HouseRuleModel spark;
    setMetadata(spark.metadata, "house_rule_sparks", {"rules", "dice"});
    spark.label = "Sparks Count Even With Costs";
    spark.description = "Three or more 6s create a critical opening even if the roll still accepts a cost. Use this to keep invention, prayer, and dangerous heroics moving.";
    campaign.houseRules = {consent, failForward, spark};

    CampaignSavedFilterModel live;
    setMetadata(live.metadata, "filter_clockwork_live_ready", {"search", "smart"});
    live.name = "Clockwork Live-Ready";
    live.query = "player-safe OR session OR opening OR cue";
    live.entityTypes = {"Story", "Encounter", "Map", "Audio", "Handout"};
    live.requiredTags = {"session"};
    live.matchAllTags = false;
    CampaignSavedFilterModel clues;
    setMetadata(clues.metadata, "filter_clockwork_clues", {"search", "smart"});
    clues.name = "Bloodline Clues";
    clues.query = "bloodline OR clue OR key OR consent OR Engine";
    clues.entityTypes = {"Story", "Handout", "NPC", "Item"};
    clues.requiredTags = {"clue"};
    clues.matchAllTags = false;
    CampaignSavedFilterModel pcs;
    setMetadata(pcs.metadata, "filter_clockwork_pc_hooks", {"search", "smart"});
    pcs.name = "PC Hook Pressure";
    pcs.query = "Iona OR Corvin OR Mirelle OR Tomas";
    pcs.entityTypes = {"Story", "NPC", "Character", "Handout"};
    pcs.requiredTags = {"pc"};
    campaign.savedFilters = {live, clues, pcs};

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
        bookmark("bookmark_clockwork_start", "beat_tallowgate_arrival", "StoryBeat", "Thirteen Bells at Tallowgate", true, 6),
        bookmark("bookmark_clockwork_main_map", "map_tallowgate_rail_terminus", "Map", "Tallowgate Rail Terminus", true, 5),
        bookmark("bookmark_clockwork_rules", "doc_cinder_vow_engine", "StoryDocument", "Cinder Vow Engine", true, 4),
        bookmark("bookmark_clockwork_finale", "beat_genealogical_machine", "StoryBeat", "The Genealogical Machine", true, 3),
        bookmark("bookmark_clockwork_audio", "playlist_tallowgate_rain", "AudioPlaylist", "Tallowgate Rain and Market", false, 2)};

    campaign.localPlayerDisplay.enabled = true;
    setMetadata(campaign.localPlayerDisplay.metadata, "display_clockwork_public", {"public", "player-safe"});
    campaign.localPlayerDisplay.publicSceneId = "beat_tallowgate_arrival";
    campaign.localPlayerDisplay.publicMapId = "map_tallowgate_rail_terminus";
    campaign.localPlayerDisplay.publicEncounterId = "enc_fog_yard_inspection";
    campaign.localPlayerDisplay.visibleNpcIds = {"npc_inspector_mara_quill", "npc_ma_jessa_pike", "npc_lord_alaric_penn"};
    campaign.localPlayerDisplay.publicHandoutIds = {"handout_h01_manifest", "handout_h02_punchcard"};
    campaign.localPlayerDisplay.sharedArtAssetIds = {"pc_iona_vale", "pc_corvin_rake", "pc_mirelle_voss", "pc_tomas_arkwright"};
    campaign.presentationModeActive = false;
}

Campaign buildCampaign(const std::filesystem::path &root)
{
    Campaign campaign;
    setMetadata(campaign.metadata, kCampaignId, {"comprehensive", "clean-slate", "steampunk", "homebrew", "multi-agent", "portraits", "audio", "maps"});
    campaign.name = kCampaignName;
    campaign.description = "A comprehensive steampunk campaign for four generated-portrait PCs using the original Cinder Vow Engine: a 9-session bloodline conspiracy with linked PC stories, NPCs, deities, factions, maps, encounters, music, ambience, handouts, shops, loot, dungeons, live-session state, and finale branches.";
    campaign.ruleSet = std::make_shared<ClockworkBloodlineRuleset>();
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
                throw std::runtime_error("Usage: seed_clockwork_bloodline [--clean]");
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
        if (!saveService.saveCampaign(campaign, "seed Clockwork Bloodline clean-slate campaign"))
            throw std::runtime_error("Campaign save failed: " + saveService.state().lastError);

        // Keep the campaign on its behaviorally complete compiled ruleset. A
        // distinct global draft makes the supported no-code model inspectable
        // without moving or replacing the campaign-owned definition row.
        installAndVerifyRulesetStudioDraft();
        addPostSaveMetadata(root);
        CampaignRepository::writeSnapshot((packageRoot(root) / "campaign.db").string());
        std::cout << "Seeded " << kCampaignName << " into " << (root / "TTRPG.db").string() << "\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
