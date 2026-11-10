#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/EntityRegistry.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace {
void stamp(EntityMetadata &metadata, const std::string &id) {
    metadata.id = id;
    metadata.createdAtUtc = metadata.updatedAtUtc = "2026-01-01T00:00:00Z";
}
Campaign fixture(bool large) {
    Campaign campaign;
    stamp(campaign.metadata, large ? "benchmark_large" : "benchmark_small");
    campaign.name = campaign.metadata.id;
    campaign.description = "Deterministic generated persistence workload, revision 1.";
    campaign.ruleSet = std::make_shared<DND5e>();
    stamp(campaign.partyInventory.metadata, "benchmark_party_inventory");
    for (int i = 0; i < (large ? 1000 : 10); ++i) {
        ItemModel item;
        stamp(item.metadata, "benchmark_item_" + std::to_string(i));
        item.name = "Generated item " + std::to_string(i);
        item.description = std::string(120, static_cast<char>('a' + i % 26));
        item.category = item.itemType = "Equipment";
        item.baseValue = 10 + i;
        item.weight = 0.5;
        campaign.customItems.push_back(std::move(item));
    }
    for (int i = 0; i < (large ? 24 : 4); ++i) {
        CharacterModel character;
        stamp(character.metadata, "benchmark_character_" + std::to_string(i));
        stamp(character.inventory.metadata, character.metadata.id + "_inventory");
        std::snprintf(character.nameBuffer, sizeof(character.nameBuffer), "Generated character %d", i);
        character.level = 1 + i % 10;
        character.attributes = {{"Strength", 12}, {"Dexterity", 14}, {"Constitution", 10}};
        InventoryEntryModel entry;
        stamp(entry.metadata, character.metadata.id + "_entry");
        entry.itemId = campaign.customItems[i % campaign.customItems.size()].metadata.id;
        entry.quantity = 2;
        character.inventory.entries.push_back(std::move(entry));
        campaign.characters.push_back(std::move(character));
    }
    for (int i = 0; i < (large ? 600 : 8); ++i) {
        NPCModel npc;
        stamp(npc.metadata, "benchmark_npc_" + std::to_string(i));
        stamp(npc.inventory.metadata, npc.metadata.id + "_inventory");
        npc.name = "Generated NPC " + std::to_string(i);
        npc.description = std::string(240, static_cast<char>('a' + i % 26));
        npc.secrets = "Generated private fixture note.";
        npc.attributes = {{"Strength", 10}, {"Dexterity", 12}};
        campaign.npcs.push_back(std::move(npc));
    }
    for (int i = 0; i < (large ? 16 : 2); ++i) {
        MapModel map;
        stamp(map.metadata, "benchmark_map_" + std::to_string(i));
        map.name = "Generated map " + std::to_string(i);
        map.width = map.height = large ? 80 : 16;
        resizeMapCells(map);
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
                applyMapCellTerrainDefaults(*mapCellAt(map, x, y),
                    (x + y + i) % 7 == 0 ? MapTileKind::Forest : MapTileKind::Dirt);
        campaign.maps.push_back(std::move(map));
    }
    for (int i = 0; i < (large ? 30 : 3); ++i) {
        SessionModel session;
        stamp(session.metadata, "benchmark_session_" + std::to_string(i));
        session.title = "Generated session " + std::to_string(i);
        session.summary = std::string(500, static_cast<char>('a' + i % 26));
        for (int n = 0; n < 3; ++n) {
            SessionNoteModel note;
            stamp(note.metadata, session.metadata.id + "_note_" + std::to_string(n));
            note.timestampUtc = "2026-01-01T00:00:00Z";
            note.body = "Generated scene preparation note.";
            session.notes.push_back(std::move(note));
        }
        campaign.sessions.push_back(std::move(session));
    }
    return campaign;
}
struct Sample {
    std::string phase;
    int iteration;
    double milliseconds;
    unsigned long long writeBytes;
    unsigned long long writeOperations;
};
IO_COUNTERS ioCounters() {
    IO_COUNTERS counters{};
    if (!GetProcessIoCounters(GetCurrentProcess(), &counters))
        throw std::runtime_error("GetProcessIoCounters failed.");
    return counters;
}
template <class Operation>
Sample measure(const char *phase, int iteration, Operation operation) {
    const auto before = ioCounters();
    const auto start = std::chrono::steady_clock::now();
    operation();
    const auto end = std::chrono::steady_clock::now();
    const auto after = ioCounters();
    return {phase, iteration, std::chrono::duration<double, std::milli>(end-start).count(),
            after.WriteTransferCount-before.WriteTransferCount,
            after.WriteOperationCount-before.WriteOperationCount};
}
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
}
int main(int argc, char **argv) {
    try {
        require(argc == 3 || (argc == 4 && std::string(argv[3]) == "--pace-writes"),
                "Usage: persistence_benchmark small|large NEW_OUTPUT_DIRECTORY [--pace-writes]");
        const bool paceWrites = argc == 4;
        const bool large = std::string(argv[1]) == "large";
        require(large || std::string(argv[1]) == "small", "Unknown fixture size.");
        const auto root = std::filesystem::absolute(argv[2]).lexically_normal();
        require(!std::filesystem::exists(root), "Benchmark output must be a new disposable directory.");
        std::filesystem::create_directories(root);
        _putenv_s("TTRPG_DATA_ROOT", root.string().c_str());
        _putenv_s("TTRPG_DB_PATH", (root / "TTRPG.db").string().c_str());
        _putenv_s("TTRPG_SETTINGS_PATH", (root / "GMEditor.settings").string().c_str());
        auto campaign = fixture(large);
        CampaignRepository::saveCampaign(campaign); // Exclude first schema/insert setup from warm-save costs.
        std::string lastWriteSecond = currentUtcTimestamp();
        const auto waitForNextWriteSecond = [&] {
            // Outside measured phases: force every save into a new timestamp
            // interval so SQLite cannot skip unchanged timestamp-only pages
            // merely because one implementation completed within the same second.
            while (paceWrites && currentUtcTimestamp() == lastWriteSecond)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        };
        std::vector<Sample> samples;
        samples.reserve(31 * 5);
        for (int iteration = -3; iteration < 31; ++iteration) {
            auto retain = [&](Sample sample) { if (iteration >= 0) samples.push_back(std::move(sample)); };
            std::optional<Campaign> copied;
            retain(measure("campaign_copy", iteration, [&] { copied.emplace(campaign); }));
            require(copied->maps.size() == campaign.maps.size(), "Copy lost maps.");
            copied.reset();
            campaign.description = "Deterministic save iteration " + std::to_string(iteration);
            waitForNextWriteSecond();
            retain(measure("database_write", iteration, [&] { CampaignRepository::saveCampaign(campaign); }));
            lastWriteSecond = currentUtcTimestamp();
            retain(measure("registry_rebuild", iteration, [&] { EntityRegistry::getInstance().rebuild(campaign); }));
            require(EntityRegistry::getInstance().exists(campaign.maps.back().metadata.id), "Registry lost final map.");
            const auto snapshot = root / ("phase_snapshot_" + std::to_string(iteration) + ".db");
            retain(measure("snapshot", iteration, [&] { CampaignRepository::writeSnapshot(snapshot.string()); }));
            require(std::filesystem::file_size(snapshot) > 0, "Snapshot is empty.");
            std::filesystem::remove(snapshot); // Exact generated file; outside measured phase.
            waitForNextWriteSecond();
            retain(measure("complete_save", iteration, [&] {
                require(ProjectSaveService::getInstance().saveCampaign(campaign, "persistence benchmark"), "Save failed.");
            }));
            lastWriteSecond = currentUtcTimestamp();
            const auto &saved = ProjectSaveService::getInstance().state();
            require(saved.lastBackupError.empty() && saved.lastRecoveryError.empty(),
                    "A complete-save follow-up failed; timing this as a complete save would be misleading.");
#if !defined(TTRPG_LEGACY_SAVE_RESULT)
            require(saved.lastSaveWarnings.empty(), "A complete-save follow-up reported a warning.");
#endif
        }
        const auto loaded = CampaignRepository::loadAllCampaigns();
        require(loaded.size() == 1, "Fixture campaign count changed.");
        const auto &roundTrip = loaded.front();
        require(roundTrip.metadata.id == campaign.metadata.id && roundTrip.description == campaign.description,
                "Campaign identity/content round trip failed.");
        require(roundTrip.maps.size() == campaign.maps.size() && roundTrip.npcs.size() == campaign.npcs.size() &&
                roundTrip.characters.size() == campaign.characters.size() && roundTrip.customItems.size() == campaign.customItems.size() &&
                roundTrip.sessions.size() == campaign.sessions.size(), "Entity counts changed during round trip.");
        for (std::size_t i = 0; i < campaign.maps.size(); ++i) {
            const auto found = std::find_if(roundTrip.maps.begin(), roundTrip.maps.end(), [&](const MapModel &map) {
                return map.metadata.id == campaign.maps[i].metadata.id;
            });
            require(found != roundTrip.maps.end() && found->cells.size() == campaign.maps[i].cells.size(), "Map identity/cells changed.");
            for (std::size_t cell = 0; cell < found->cells.size(); ++cell)
                require(found->cells[cell].kind == campaign.maps[i].cells[cell].kind, "Map terrain changed.");
        }
        std::ofstream output(root / "samples.csv", std::ios::binary);
        output << "phase,iteration,milliseconds,process_write_transfer_bytes,process_write_operations\n";
        for (const auto &sample : samples)
            output << sample.phase << ',' << sample.iteration << ',' << sample.milliseconds << ',' << sample.writeBytes << ',' << sample.writeOperations << '\n';
        output.close();
        require(static_cast<bool>(output), "Could not write benchmark samples.");
        std::cout << "Persistence fixture " << argv[1] << ": " << samples.size() << " samples; round trip passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
