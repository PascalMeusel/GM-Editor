#include "ProjectSaveService.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/EntityRegistry.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/ProjectLogger.hpp"
#include "Models/Project/ProjectManifestService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectSaveSettings.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/AtomicProjectFile.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace
{
std::string safePathSegment(const std::string &value)
{
    std::string sanitized;
    sanitized.reserve(value.size());
    for (unsigned char character : value)
    {
        if (std::isalnum(character) || character == '-' || character == '_')
            sanitized.push_back(static_cast<char>(character));
        else
            sanitized.push_back('_');
    }
    return sanitized.empty() ? "campaign" : sanitized;
}

std::filesystem::path findProjectRoot()
{
    return ProjectAssets::findProjectRoot();
}

void pruneOldSnapshots(const std::filesystem::path &directory, int maxSnapshots, const std::filesystem::path &newest)
{
    struct Snapshot {
        std::filesystem::path path;
        std::filesystem::file_time_type modified;
        bool newest;
    };
    std::vector<Snapshot> snapshots;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(mapFileIoPath(directory)))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".db")
            snapshots.push_back({entry.path(), entry.last_write_time(), entry.path().filename() == newest.filename()});
    }

    std::sort(snapshots.begin(), snapshots.end(), [&](const auto &left, const auto &right) {
        if (left.newest != right.newest) return left.newest;
        if (left.modified != right.modified) return left.modified > right.modified;
        return left.path.filename() > right.path.filename();
    });

    for (std::size_t index = static_cast<std::size_t>(std::max(0, maxSnapshots)); index < snapshots.size(); ++index)
        std::filesystem::remove(snapshots[index].path);
}

bool shouldTrackSaveReason(const std::string &reason)
{
    return !reason.empty() &&
           reason != "manual save" &&
           reason != "keyboard shortcut" &&
           reason != "menu save" &&
           reason != "command palette save" &&
           reason != "autosave";
}
}

ProjectSaveService &ProjectSaveService::getInstance()
{
    static ProjectSaveService service;
    return service;
}

ProjectSaveService::ProjectSaveService() : ProjectSaveService(ProjectSaveDependencies{}) {}

ProjectSaveService::ProjectSaveService(ProjectSaveDependencies dependencies)
    : _dependencies(std::move(dependencies))
{
    if (!_dependencies.rebuildRegistry)
        _dependencies.rebuildRegistry = [](const Campaign &campaign) { EntityRegistry::getInstance().rebuild(campaign); };
    if (!_dependencies.monotonicNow)
        _dependencies.monotonicNow = [] { return std::chrono::steady_clock::now(); };
    _firstDirtyActivity = _lastDirtyActivity = _dependencies.monotonicNow();
    const ProjectSaveSettingsLoadResult loaded =
        ProjectSaveSettingsStore::load(ApplicationPaths::settingsPath(), _config);
    if (loaded.succeeded())
        _config = loaded.config;
    else
        ProjectLogger::getInstance().warning("ProjectSaveService", loaded.error);
}

void ProjectSaveService::setAutosaveEnabled(bool enabled)
{
    _config.autosaveEnabled = enabled;
    _lastDirtyActivity = _dependencies.monotonicNow();
}

void ProjectSaveService::setAutosaveIntervalSeconds(int intervalSeconds)
{
    _config.autosaveIntervalSeconds = std::max(10, intervalSeconds);
    _lastDirtyActivity = _dependencies.monotonicNow();
}

void ProjectSaveService::setBackupSnapshotsEnabled(bool enabled)
{
    _config.backupSnapshotsEnabled = enabled;
    if (!enabled) _state.lastBackupError.clear();
}

void ProjectSaveService::setRecoverySnapshotsEnabled(bool enabled)
{
    _config.recoverySnapshotsEnabled = enabled;
    if (!enabled) _state.lastRecoveryError.clear();
}

void ProjectSaveService::setMaxRecoverySnapshots(int maxSnapshots)
{
    _config.maxRecoverySnapshots = std::clamp(maxSnapshots, 1, 50);
}

void ProjectSaveService::setMaxBackupSnapshots(int maxSnapshots)
{
    _config.maxBackupSnapshots = std::clamp(maxSnapshots, 1, 50);
}

bool ProjectSaveService::persistConfig(std::string *error) const
{
    return ProjectSaveSettingsStore::save(ApplicationPaths::settingsPath(), _config, error);
}

void ProjectSaveService::markDirty(const std::string &reason)
{
    const auto now = _dependencies.monotonicNow();
    if (!_state.dirty) _firstDirtyActivity = now;
    _state.dirty = true;
    _state.lastReason = reason;
    _lastDirtyActivity = now;
    recordChangeReason(reason);
}

void ProjectSaveService::discardUnsavedChanges(const std::string &reason)
{
    _state.dirty = false;
    _state.currentlySaving = false;
    _state.lastReason = reason;
    _state.pendingChangeSummary.clear();
    _lastSaveAttempt.reset();
}

bool ProjectSaveService::saveCampaign(Campaign &campaign, const std::string &reason)
{
    return saveCampaignDetailed(campaign, reason).committed();
}

ProjectSaveResult ProjectSaveService::saveCampaignDetailed(Campaign &campaign, const std::string &reason)
{
    ProjectSaveResult result;
    if (_state.currentlySaving)
    {
        result.error = "A campaign save is already in progress.";
        return result;
    }
    if (_state.campaignId != campaign.metadata.id)
    {
        _state.campaignId = campaign.metadata.id;
        _state.lastSavedAtUtc.clear();
        _state.lastAutosavedAtUtc.clear();
        _state.lastSaveChangeSummary.clear();
        _state.lastBackupPath.clear();
        _state.lastBackupAtUtc.clear();
        _state.lastRecoverySnapshotPath.clear();
        _state.lastRecoverySnapshotAtUtc.clear();
        _state.lastBackupError.clear();
        _state.lastRecoveryError.clear();
    }
    _lastSaveAttempt = _dependencies.monotonicNow();
    markDirty(reason);
    std::vector<std::string> changesForThisSave = _state.pendingChangeSummary;
    _state.currentlySaving = true;
    struct SavingGuard { bool &saving; ~SavingGuard() { saving = false; } } guard{_state.currentlySaving};
    _state.lastError.clear();
    _state.lastSaveWarnings.clear();

    const auto warn = [&](std::string warning) {
        result.warnings.push_back(std::move(warning));
    };
    const auto followUp = [&](const char *label, const auto &operation) {
        try { operation(); }
        catch (const std::exception &error) { warn(std::string(label) + ": " + error.what()); }
        catch (...) { warn(std::string(label) + ": unknown failure."); }
    };

    try
    {
        campaign.metadata.touch();
        if (_config.backupSnapshotsEnabled && !campaign.name.empty() &&
            !writePreSaveBackup(campaign, reason.empty() ? "save" : reason))
            warn("Pre-save backup: " + _state.lastBackupError);
        CampaignRepository::saveCampaign(campaign);
        // The repository returns only after its transaction commits. Nothing
        // below this boundary may report that the durable change was rolled back.
        result.outcome = ProjectSaveOutcome::Committed;
        _state.dirty = false;
        _lastSaveAttempt.reset();
        _state.lastSaveChangeSummary.swap(changesForThisSave);
        _state.pendingChangeSummary.clear();
    }
    catch (const std::exception &error) { result.error = error.what(); }
    catch (...) { result.error = "Unknown save error."; }

    if (!result.committed())
    {
        _state.lastError = result.error;
        _state.lastSaveWarnings = result.warnings;
        try { ProjectLogger::getInstance().error("ProjectSaveService", result.error); } catch (...) {}
        return result;
    }

    followUp("Save timestamp", [&] {
        _state.lastSavedAtUtc = ::currentUtcTimestamp();
        if (reason == "autosave")
            _state.lastAutosavedAtUtc = _state.lastSavedAtUtc;
    });
    followUp("Registry rebuild", [&] { _dependencies.rebuildRegistry(campaign); });
    followUp("Recovery snapshot", [&] {
        if (_config.recoverySnapshotsEnabled && !campaign.name.empty() &&
            !writeRecoverySnapshot(campaign, reason.empty() ? "save" : reason))
            warn("Recovery snapshot: " + _state.lastRecoveryError);
    });
    followUp("Project manifest", [&] {
        ProjectManifest::RecoveryMetadata recovery;
        recovery.latestRecoverySnapshotAtUtc = _state.lastRecoverySnapshotAtUtc;
        recovery.latestRecoverySnapshotPath = _state.lastRecoverySnapshotPath;
        recovery.latestBackupAtUtc = _state.lastBackupAtUtc;
        recovery.latestBackupPath = _state.lastBackupPath;
        recovery.maxRecoverySnapshots = _config.maxRecoverySnapshots;
        recovery.maxBackupSnapshots = _config.maxBackupSnapshots;
        std::string manifestError;
        if (!ProjectManifest::writeManifest(campaign, recovery, manifestError))
            warn("Project manifest: " + manifestError);
    });
    followUp("Saved event", [&] {
        for (auto &warning : ProjectEventBus::getInstance().publishDetailed(ProjectEvent{ProjectEventType::CampaignSaved,
                                                            campaign.metadata.id,
                                                            campaign.metadata.id,
                                                            "campaign",
                                                            reason.empty() ? "Campaign saved." : "Campaign saved: " + reason}))
            warn(std::move(warning));
    });
    if (!result.warnings.empty()) result.outcome = ProjectSaveOutcome::CommittedWithWarnings;
    _state.lastSaveWarnings = result.warnings;
    for (const auto &warning : result.warnings)
        try { ProjectLogger::getInstance().warning("ProjectSaveService", warning); } catch (...) {}
    return result;
}

void ProjectSaveService::recordChangeReason(const std::string &reason)
{
    if (!shouldTrackSaveReason(reason))
        return;

    auto duplicate = std::find(_state.pendingChangeSummary.begin(), _state.pendingChangeSummary.end(), reason);
    if (duplicate != _state.pendingChangeSummary.end())
        return;

    constexpr std::size_t maxTrackedChanges = 8;
    if (_state.pendingChangeSummary.size() >= maxTrackedChanges)
        _state.pendingChangeSummary.erase(_state.pendingChangeSummary.begin());
    _state.pendingChangeSummary.push_back(reason);
}

bool ProjectSaveService::updateAutosave(Campaign &campaign)
{
    if (!_config.autosaveEnabled || !_state.dirty || _state.currentlySaving || campaign.name.empty())
        return false;

    const auto now = _dependencies.monotonicNow();
    // Keep the original dirty deadline on failure, but do not retry every frame.
    if (_lastSaveAttempt && now - *_lastSaveAttempt < std::chrono::seconds(10))
        return false;
    const auto idleSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - _lastDirtyActivity).count();
    const auto dirtySeconds = std::chrono::duration_cast<std::chrono::seconds>(now - _firstDirtyActivity).count();
    if (idleSeconds < _config.autosaveIntervalSeconds && dirtySeconds < 60)
        return false;

    return saveCampaign(campaign, "autosave");
}

bool ProjectSaveService::writePreSaveBackup(const Campaign &campaign, const std::string &reason)
{
    _state.lastBackupError.clear();
    if (!_config.backupSnapshotsEnabled || campaign.name.empty())
        return false;

    try
    {
        const std::string timestamp = ::currentUtcTimestamp();
        const std::filesystem::path backupDirectory = findProjectRoot() / "Resources" / "Backups" / safePathSegment(campaign.metadata.id);
        std::filesystem::create_directories(mapFileIoPath(backupDirectory));

        const std::string fileName = safePathSegment(timestamp + "_pre_" + reason.substr(0, 64)) + "_" + AtomicProjectFile::uniqueSuffix() + ".db";
        const std::filesystem::path backupPath = backupDirectory / fileName;

        CampaignRepository::writeRecoverySnapshot(mapFileIoPath(backupPath).u8string());

        _state.lastBackupAtUtc = timestamp;
        _state.lastBackupPath = backupPath.string();
        pruneOldSnapshots(backupDirectory, _config.maxBackupSnapshots, backupPath);
        return true;
    }
    catch (const std::exception &e)
    {
        _state.lastBackupError = e.what();
        return false;
    }
    catch (...) { _state.lastBackupError = "Unknown backup failure."; return false; }
}

bool ProjectSaveService::writeRecoverySnapshot(const Campaign &campaign, const std::string &reason)
{
    _state.lastRecoveryError.clear();
    if (!_config.recoverySnapshotsEnabled || campaign.name.empty())
        return false;

    try
    {
        const std::string timestamp = ::currentUtcTimestamp();
        const std::filesystem::path snapshotDirectory = findProjectRoot() / "Resources" / "Recovery" / safePathSegment(campaign.metadata.id);
        std::filesystem::create_directories(mapFileIoPath(snapshotDirectory));

        const std::string fileName = safePathSegment(timestamp + "_" + reason.substr(0, 64)) + "_" + AtomicProjectFile::uniqueSuffix() + ".db";
        const std::filesystem::path snapshotPath = snapshotDirectory / fileName;

        CampaignRepository::writeRecoverySnapshot(mapFileIoPath(snapshotPath).u8string());
        _state.lastRecoverySnapshotAtUtc = timestamp;
        _state.lastRecoverySnapshotPath = snapshotPath.string();

        std::ostringstream manifest;
        manifest << "campaign_id=" << campaign.metadata.id << "\n";
        manifest << "campaign_name=" << campaign.name << "\n";
        manifest << "reason=" << reason << "\n";
        manifest << "created_at_utc=" << timestamp << "\n";
        manifest << "snapshot=" << snapshotPath.filename().string() << "\n";
        AtomicProjectFile::write(snapshotDirectory / "latest.txt", manifest.str());

        pruneOldSnapshots(snapshotDirectory, _config.maxRecoverySnapshots, snapshotPath);
        return true;
    }
    catch (const std::exception &e)
    {
        _state.lastRecoveryError = e.what();
        return false;
    }
    catch (...) { _state.lastRecoveryError = "Unknown recovery failure."; return false; }
}
