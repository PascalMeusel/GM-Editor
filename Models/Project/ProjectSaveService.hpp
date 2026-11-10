#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

struct ProjectSaveConfig
{
    bool autosaveEnabled = true;
    // Save completed edits after idle activity; a separate 60-second dirty-age
    // deadline also protects continuously edited campaign state.
    int autosaveIntervalSeconds = 10;
    bool backupSnapshotsEnabled = true;
    bool recoverySnapshotsEnabled = true;
    int maxRecoverySnapshots = 5;
    int maxBackupSnapshots = 10;
};

struct ProjectSaveState
{
    EntityId campaignId;
    bool dirty = false;
    bool currentlySaving = false;
    std::string lastSavedAtUtc;
    std::string lastAutosavedAtUtc;
    std::string lastRecoverySnapshotAtUtc;
    std::string lastRecoverySnapshotPath;
    std::string lastRecoveryError;
    std::string lastBackupAtUtc;
    std::string lastBackupPath;
    std::string lastBackupError;
    std::string lastError;
    std::string lastReason;
    std::vector<std::string> lastSaveWarnings;
    std::vector<std::string> pendingChangeSummary;
    std::vector<std::string> lastSaveChangeSummary;
};

enum class ProjectSaveOutcome { NotCommitted, Committed, CommittedWithWarnings };

struct ProjectSaveResult
{
    ProjectSaveOutcome outcome = ProjectSaveOutcome::NotCommitted;
    std::string error;
    std::vector<std::string> warnings;
    bool committed() const { return outcome != ProjectSaveOutcome::NotCommitted; }
};

struct ProjectSaveDependencies
{
    // The durable repository remains the transaction coordinator. Indexing is
    // a replaceable follow-up, also allowing hosts to supply their own index.
    std::function<void(const Campaign &)> rebuildRegistry;
    // A monotonic clock keeps autosave policy independent of wall-clock changes.
    std::function<std::chrono::steady_clock::time_point()> monotonicNow;
};

class ProjectSaveService
{
public:
    static ProjectSaveService &getInstance();
    explicit ProjectSaveService(ProjectSaveDependencies dependencies);

    const ProjectSaveConfig &config() const { return _config; }
    const ProjectSaveState &state() const { return _state; }
    void setAutosaveEnabled(bool enabled);
    void setAutosaveIntervalSeconds(int intervalSeconds);
    void setBackupSnapshotsEnabled(bool enabled);
    void setRecoverySnapshotsEnabled(bool enabled);
    void setMaxRecoverySnapshots(int maxSnapshots);
    void setMaxBackupSnapshots(int maxSnapshots);
    bool persistConfig(std::string *error = nullptr) const;
    void markDirty(const std::string &reason = "");
    void discardUnsavedChanges(const std::string &reason = "");
    bool saveCampaign(Campaign &campaign, const std::string &reason = "");
    ProjectSaveResult saveCampaignDetailed(Campaign &campaign, const std::string &reason = "");
    bool updateAutosave(Campaign &campaign);

private:
    ProjectSaveService();
    void recordChangeReason(const std::string &reason);
    bool writePreSaveBackup(const Campaign &campaign, const std::string &reason);
    bool writeRecoverySnapshot(const Campaign &campaign, const std::string &reason);

    ProjectSaveConfig _config;
    ProjectSaveDependencies _dependencies;
    ProjectSaveState _state;
    std::chrono::steady_clock::time_point _lastDirtyActivity{};
    std::chrono::steady_clock::time_point _firstDirtyActivity{};
    std::optional<std::chrono::steady_clock::time_point> _lastSaveAttempt;
};
