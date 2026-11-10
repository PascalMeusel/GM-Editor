#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ProjectBackupRestore
{
enum class BackupKind
{
    PreSave,
    ImportReplacement,
    BeforeRestore
};
enum class PackageRestoreAction { Keep, Restore, Remove };

struct BackupEntry
{
    BackupKind kind = BackupKind::PreSave;
    std::filesystem::path snapshotPath;
    std::filesystem::path packageBackupPath;
    std::string campaignId;
    std::string campaignName;
    std::string label;
    std::uintmax_t byteCount = 0;
    std::string timestampUtc;
    std::string selectionFingerprint;
    bool campaignPresent = true;
    PackageRestoreAction packageAction = PackageRestoreAction::Keep;
    bool legacySafety = false;
};

struct RestoreResult
{
    bool success = false;
    std::string errorMessage;
    std::string restoredCampaignName;
    std::filesystem::path safetyBackupPath;
    bool packageRestored = false;
    bool campaignRemoved = false;
    bool packageRemoved = false;
    std::vector<std::string> warnings;
    std::vector<std::filesystem::path> retainedRecoveryPaths;
};

std::vector<BackupEntry> listBackups(std::string &errorMessage);
RestoreResult restoreBackup(const BackupEntry &backup);
}
