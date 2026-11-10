#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Core/ProductInfo.hpp"
#include "Models/Persistence/DatabaseSchemaVersion.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ProjectManifest
{
inline constexpr int CurrentSchemaVersion = DatabaseSchema::CurrentVersion;
struct RulesetPackEntry
{
    std::string id;
    std::string rulesetId;
    std::string name;
    std::string version;
    bool enabled = true;
};

struct RecoveryMetadata
{
    std::string latestRecoverySnapshotAtUtc;
    std::string latestRecoverySnapshotPath;
    std::string latestBackupAtUtc;
    std::string latestBackupPath;
    int maxRecoverySnapshots = 5;
    int maxBackupSnapshots = 10;
};

struct ProjectManifestModel
{
    std::string format = "ttrpg-editor.project.v1";
    std::string appVersion = ProductInfo::version;
    int schemaVersion = CurrentSchemaVersion;
    std::string campaignId;
    std::string campaignName;
    std::string databasePath = "campaign.db";
    std::vector<std::string> assetFolders;
    std::vector<RulesetPackEntry> rulesetPacks;
    RecoveryMetadata recovery;
};

ProjectManifestModel buildManifest(const Campaign &campaign, const RecoveryMetadata &recovery = RecoveryMetadata{});
std::string serializeManifest(const ProjectManifestModel &manifest);
bool writeManifest(const Campaign &campaign, const RecoveryMetadata &recovery, std::string &errorMessage);
ProjectManifestModel loadManifest(const std::filesystem::path &manifestPath, std::string &errorMessage);
}
