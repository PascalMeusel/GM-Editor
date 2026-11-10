#pragma once

#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/EntityRegistry.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/ProjectManifestService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/WorldInspectorService.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/DataBaseManager.hpp"

#include <fstream>
#include <functional>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace SaveReliabilityTests
{
inline void check(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
struct Subscriptions
{
    std::vector<int> ids;
    void add(ProjectEventBus::Handler handler) { ids.push_back(ProjectEventBus::getInstance().subscribe(std::move(handler))); }
    ~Subscriptions() { for (int id : ids) ProjectEventBus::getInstance().unsubscribe(id); }
};
inline void noSnapshots(ProjectSaveService &service)
{
    service.setBackupSnapshotsEnabled(false);
    service.setRecoverySnapshotsEnabled(false);
}
inline std::string read(const std::filesystem::path &path)
{
    std::ifstream input(mapFileIoPath(path), std::ios::binary);
    check(static_cast<bool>(input), "Could not read generated regression file");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

inline bool run(const std::function<Campaign(const std::string &)> &fixture)
{
    {
        auto campaign = fixture("save_commit_boundary");
        ProjectSaveService service(ProjectSaveDependencies{});
        noSnapshots(service);
        check(service.saveCampaignDetailed(campaign).outcome == ProjectSaveOutcome::Committed, "Initial save should commit without warnings");
        const auto originalDescription = campaign.description;
        SQLite::Database control(ApplicationPaths::databasePath().u8string(), SQLite::OPEN_READWRITE);
        control.exec("CREATE TRIGGER abort_save_boundary BEFORE UPDATE ON campaigns WHEN NEW.stable_id = 'test_campaign_save_commit_boundary' BEGIN SELECT RAISE(ABORT, 'injected commit failure'); END;");
        struct TriggerGuard { SQLite::Database &db; ~TriggerGuard() { try { db.exec("DROP TRIGGER IF EXISTS abort_save_boundary"); } catch (...) {} } } trigger{control};
        campaign.description = "Retain this draft for retry";
        service.markDirty("description draft");
        const auto failed = service.saveCampaignDetailed(campaign, "manual save");
        check(failed.outcome == ProjectSaveOutcome::NotCommitted && !failed.error.empty(), "Failed transaction must return NotCommitted and an error");
        check(service.state().dirty && !service.state().currentlySaving && !service.state().pendingChangeSummary.empty(), "Failed transaction must retain dirty state and pending changes");
        check(campaign.description == "Retain this draft for retry", "Failed transaction discarded the draft");
        check(CampaignRepository::loadCampaign(campaign.name).description == originalDescription, "Failed transaction changed disk contents");
        control.exec("DROP TRIGGER abort_save_boundary");
        const auto retried = service.saveCampaignDetailed(campaign, "retry");
        check(retried.outcome == ProjectSaveOutcome::Committed && retried.error.empty(), "Retained draft did not commit on retry");
        const auto reopened = CampaignRepository::loadCampaign(campaign.name);
        check(reopened.metadata.id == campaign.metadata.id && reopened.description == campaign.description && reopened.characters.front().metadata.id == campaign.characters.front().metadata.id,
              "Retry changed identity or lost campaign content");
        check(!service.state().dirty && service.state().pendingChangeSummary.empty(), "Commit did not clear the saved draft state");
    }
    {
        auto campaign = fixture("save_subscribers");
        ProjectSaveService service(ProjectSaveDependencies{});
        noSnapshots(service);
        int notified = 0;
        Subscriptions subscriptions;
        subscriptions.add([](const ProjectEvent &) { throw std::runtime_error("first subscriber failure"); });
        subscriptions.add([&](const ProjectEvent &) { ++notified; });
        subscriptions.add([](const ProjectEvent &) { throw 7; });
        subscriptions.add([&](const ProjectEvent &) { ++notified; });
        const auto saved = service.saveCampaignDetailed(campaign, "subscriber regression");
        check(saved.outcome == ProjectSaveOutcome::CommittedWithWarnings && saved.error.empty() && saved.warnings.size() == 2, "Subscriber failures must be committed warnings");
        check(notified == 2 && !service.state().dirty && service.state().lastError.empty(), "A subscriber suppressed another or changed the commit state");
        const auto mutation = WorldInspector::applyWithRollback(campaign, [](Campaign &draft) {
            draft.description = "World inspector edit survives a notification failure";
            return WorldInspector::MutationResult{WorldInspector::MutationStatus::Applied, "", draft.metadata.id};
        }, [&](Campaign &draft) { return service.saveCampaign(draft, "world inspector regression"); });
        check(mutation.applied() && notified == 4, "Boolean compatibility wrapper reported a committed edit as failed");
        check(CampaignRepository::loadCampaign(campaign.name).description == campaign.description, "World inspector restored old memory after a successful commit");
    }
    {
        auto campaign = fixture("save_registry");
        bool failRegistry = true;
        ProjectSaveService service(ProjectSaveDependencies{[&](const Campaign &saved) {
            if (failRegistry) throw std::runtime_error("injected registry failure");
            EntityRegistry::getInstance().rebuild(saved);
        }});
        noSnapshots(service);
        int notified = 0;
        Subscriptions subscriptions;
        subscriptions.add([&](const ProjectEvent &) { ++notified; });
        const auto result = service.saveCampaignDetailed(campaign);
        check(result.outcome == ProjectSaveOutcome::CommittedWithWarnings && !service.state().dirty && notified == 1, "Registry failure changed save outcome or suppressed later follow-ups");
        check(std::filesystem::is_regular_file(ProjectPackageLayout::layoutForCampaign(campaign).manifestPath), "Registry failure suppressed manifest writing");
        check(CampaignRepository::loadCampaign(campaign.name).metadata.id == campaign.metadata.id, "Registry failure rolled back committed data");
        failRegistry = false;
        check(service.saveCampaignDetailed(campaign).outcome == ProjectSaveOutcome::Committed && service.state().lastSaveWarnings.empty(), "Successful follow-up retry retained stale warnings");
        check(EntityRegistry::getInstance().exists(campaign.metadata.id), "Registry retry did not install the current campaign");
    }
    {
        auto campaign = fixture("save_reentrant");
        ProjectSaveService service(ProjectSaveDependencies{});
        noSnapshots(service);
        bool cleanAtCommit = false;
        ProjectSaveResult recursive;
        Subscriptions subscriptions;
        subscriptions.add([&](const ProjectEvent &) {
            cleanAtCommit = !service.state().dirty && service.state().pendingChangeSummary.empty();
            service.markDirty("new edit after commit");
            recursive = service.saveCampaignDetailed(campaign, "recursive save");
        });
        check(service.saveCampaignDetailed(campaign).committed(), "Outer save did not commit");
        check(cleanAtCommit && !recursive.committed() && !recursive.error.empty(), "Commit boundary or reentrant-save guard failed");
        check(service.state().dirty && service.state().pendingChangeSummary == std::vector<std::string>{"new edit after commit"}, "Post-commit edits were silently marked clean");
        check(!service.state().currentlySaving && service.state().lastError.empty(), "Reentrant save corrupted the outer save state");
    }
    {
        auto &bus = ProjectEventBus::getInstance();
        Subscriptions subscriptions;
        int second = 0, next = 0, calls = 0;
        bool changed = false;
        subscriptions.add([&](const ProjectEvent &) {
            if (!changed) {
                changed = true;
                bus.unsubscribe(second);
                subscriptions.add([&](const ProjectEvent &) { ++next; });
            }
        });
        subscriptions.add([&](const ProjectEvent &) { ++calls; });
        second = subscriptions.ids.back();
        check(bus.publishDetailed(ProjectEvent{}).empty() && calls == 1 && next == 0, "Subscription mutation invalidated the current dispatch snapshot");
        check(bus.publishDetailed(ProjectEvent{}).empty() && calls == 1 && next == 1, "Subscription mutation did not take effect on the next event");
    }
    {
        auto campaign = fixture("save_snapshots");
        ProjectSaveService service(ProjectSaveDependencies{});
        service.setBackupSnapshotsEnabled(true);
        service.setRecoverySnapshotsEnabled(true);
        service.setMaxBackupSnapshots(50);
        service.setMaxRecoverySnapshots(50);
        std::set<std::string> backups, recovery;
        for (int index = 0; index < 8; ++index) {
            campaign.description = "Snapshot revision " + std::to_string(index);
            check(service.saveCampaignDetailed(campaign, "same reason").outcome == ProjectSaveOutcome::Committed, "Snapshot save reported an unexpected warning");
            backups.insert(service.state().lastBackupPath);
            recovery.insert(service.state().lastRecoverySnapshotPath);
        }
        check(backups.size() == 8 && recovery.size() == 8, "Repeated saves reused a snapshot filename");
        for (const auto &path : recovery) check(std::filesystem::is_regular_file(path), "A rapid save overwrote or removed a retained recovery snapshot");
        const auto recoveryDirectory = std::filesystem::path(service.state().lastRecoverySnapshotPath).parent_path();
        // Simulate coarse/copied timestamps and clock skew: old files can look
        // newer than the snapshot just written. The current snapshot still wins.
        const auto equalTime = std::filesystem::file_time_type::clock::now() + std::chrono::hours(1);
        for (const auto &path : recovery) std::filesystem::last_write_time(path, equalTime);
        service.setMaxBackupSnapshots(2);
        service.setMaxRecoverySnapshots(2);
        campaign.description = "Latest retained snapshot";
        check(service.saveCampaignDetailed(campaign, "same reason").outcome == ProjectSaveOutcome::Committed, "Pruned snapshot save failed");
        int count = 0;
        for (const auto &entry : std::filesystem::directory_iterator(recoveryDirectory)) if (entry.path().extension() == ".db") ++count;
        check(count == 2 && std::filesystem::is_regular_file(service.state().lastRecoverySnapshotPath), "Pruning did not retain the newest snapshot and configured limit");
        check(read(recoveryDirectory / "latest.txt").find("snapshot=" + std::filesystem::path(service.state().lastRecoverySnapshotPath).filename().string()) != std::string::npos, "Recovery metadata does not identify the newest snapshot");
        Campaign restored;
        std::string error;
        check(DatabaseManager::readCampaignFromSnapshot(service.state().lastRecoverySnapshotPath, campaign.metadata.id, restored, error), "Recovery snapshot could not be restored");
        check(restored.metadata.id == campaign.metadata.id && restored.description == campaign.description && restored.characters.front().metadata.id == campaign.characters.front().metadata.id, "Recovery round trip lost identities or content");
        std::filesystem::remove(recoveryDirectory / "latest.txt");
        std::filesystem::create_directory(recoveryDirectory / "latest.txt");
        check(service.saveCampaignDetailed(campaign).outcome == ProjectSaveOutcome::CommittedWithWarnings && !service.state().lastRecoveryError.empty(), "Recovery metadata failure was not a committed warning");
        check(!service.state().dirty && std::filesystem::is_regular_file(service.state().lastRecoverySnapshotPath), "Recovery metadata failure hid a valid snapshot or dirtied a committed save");
        std::filesystem::remove(recoveryDirectory / "latest.txt");
        check(service.saveCampaignDetailed(campaign).outcome == ProjectSaveOutcome::Committed, "Recovery metadata retry did not clear the warning");
        check(service.saveCampaignDetailed(campaign, "autosave").committed() && !service.state().lastAutosavedAtUtc.empty(), "Autosave timestamp was not recorded");
        noSnapshots(service);
        auto other = fixture("save_other_campaign");
        check(service.saveCampaignDetailed(other).outcome == ProjectSaveOutcome::Committed && service.state().lastBackupPath.empty() && service.state().lastRecoverySnapshotPath.empty() && service.state().lastAutosavedAtUtc.empty(), "Save/recovery state leaked between campaigns or disabled snapshots produced warnings");
    }
    {
        auto campaign = fixture("save_blocked_followups");
        ProjectSaveService service(ProjectSaveDependencies{});
        service.setBackupSnapshotsEnabled(true);
        service.setRecoverySnapshotsEnabled(false);
        const auto backupDirectory = ApplicationPaths::writableRoot() / "Resources" / "Backups" / campaign.metadata.id;
        std::filesystem::create_directories(backupDirectory.parent_path());
        AtomicProjectFile::write(backupDirectory, "block directory creation");
        auto result = service.saveCampaignDetailed(campaign);
        check(result.outcome == ProjectSaveOutcome::CommittedWithWarnings && !service.state().lastBackupError.empty() && !service.state().dirty, "Best-effort backup failure prevented the database commit or was hidden");
        std::filesystem::remove(backupDirectory);
        noSnapshots(service);
        const auto manifest = ProjectPackageLayout::layoutForCampaign(campaign).manifestPath;
        std::filesystem::remove(manifest);
        std::filesystem::create_directory(manifest);
        campaign.description = "Commit despite blocked project manifest";
        result = service.saveCampaignDetailed(campaign);
        check(result.outcome == ProjectSaveOutcome::CommittedWithWarnings && result.error.empty() && !result.warnings.empty(), "Blocked manifest falsely reported success or rolled back a commit");
        check(CampaignRepository::loadCampaign(campaign.name).description == campaign.description && !service.state().dirty, "Manifest failure changed disk/memory commit state");
        std::filesystem::remove(manifest);
        check(service.saveCampaignDetailed(campaign).outcome == ProjectSaveOutcome::Committed, "Manifest retry did not recover");
    }
    {
        const auto directory = ApplicationPaths::writableRoot() / "atomic-metadata-regression";
        std::filesystem::create_directories(directory);
        const auto path = directory / "latest.txt";
        AtomicProjectFile::write(path, "previous complete metadata");
#if defined(_WIN32)
        const HANDLE locked = CreateFileW(mapFileIoPath(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        check(locked != INVALID_HANDLE_VALUE, "Could not arrange metadata replacement failure");
        bool rejected = false;
        try { AtomicProjectFile::write(path, "replacement must not truncate the old file"); }
        catch (const std::system_error &) { rejected = true; }
        CloseHandle(locked);
        check(rejected && read(path) == "previous complete metadata", "Failed atomic replacement altered previous metadata");
#endif
        AtomicProjectFile::write(path, "next complete metadata");
        check(read(path) == "next complete metadata", "Atomic metadata retry did not replace the file");
        for (const auto &entry : std::filesystem::directory_iterator(directory))
            check(entry.path().filename() == "latest.txt", "Atomic metadata write leaked a temporary file");
    }
    return true;
}

// Compatibility oracle retained from the pre-stabilization on-disk format.
inline bool serializationContract()
{
    check(PersistenceSerialization::serializeMapCells({}).empty(), "Empty map-cell encoding changed");
    std::vector<MapCellModel> cells;
    const int extremes[] = {0, 1, -1, 15, -19, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()};
    for (int value : extremes) {
        MapCellModel cell;
        cell.rotationQuarterTurns = cell.movementCost = cell.elevation = value;
        cell.movementBlockedMask = cell.lineOfSightBlockedMask = value;
        cell.movementOneWayBlockedMask = cell.lineOfSightOneWayBlockedMask = value;
        const auto index = cells.size();
        cell.kind = static_cast<decltype(cell.kind)>(index % 4);
        cell.lighting = static_cast<decltype(cell.lighting)>(index % 3);
        cell.symbolVariantMode = static_cast<decltype(cell.symbolVariantMode)>(index % 2);
        cell.wayKind = static_cast<decltype(cell.wayKind)>(index % 3);
        cell.difficultTerrain = (index & 1) != 0;
        cell.blocksLineOfSight = (index & 2) != 0;
        cell.fogged = (index & 4) != 0;
        cell.rotationLocked = index % 3 == 0;
        cells.push_back(cell);
    }
    std::ostringstream legacy;
    for (const auto &cell : cells)
        legacy << static_cast<int>(cell.kind) << ':' << normalizeMapRotation(cell.rotationQuarterTurns) << ':'
               << std::max(1, cell.movementCost) << ':' << (cell.difficultTerrain ? 1 : 0) << ':'
               << (cell.blocksLineOfSight ? 1 : 0) << ':' << static_cast<int>(cell.lighting) << ':'
               << (cell.fogged ? 1 : 0) << ':' << cell.elevation << ':' << normalizeMapEdgeMask(cell.movementBlockedMask) << ':'
               << normalizeMapEdgeMask(cell.lineOfSightBlockedMask) << ':' << static_cast<int>(cell.symbolVariantMode) << ':'
               << static_cast<int>(cell.wayKind) << ':' << (cell.rotationLocked ? 1 : 0) << ':'
               << normalizeMapEdgeMask(cell.movementOneWayBlockedMask) << ':' << normalizeMapEdgeMask(cell.lineOfSightOneWayBlockedMask) << ',';
    const auto encoded = PersistenceSerialization::serializeMapCells(cells);
    check(encoded == legacy.str(), "Map-cell encoding changed existing serialized bytes");
    const auto decoded = PersistenceSerialization::deserializeMapCells(encoded);
    check(decoded.size() == cells.size(), "Map-cell round trip changed the record count");
    const int expectedElevation[] = {0, 1, -1, 15, -19, -20, 20};
    for (std::size_t index = 0; index < cells.size(); ++index)
        check(decoded[index].elevation == expectedElevation[index], "Map-cell loading changed the existing signed-elevation normalization contract");
    return true;
}
}
