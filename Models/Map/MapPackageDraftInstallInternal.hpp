#pragma once

#include "Models/Map/MapPackageDraftInstallService.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace MapPackageDraftInstallDetail
{
inline constexpr const char *TransactionPrefix = ".mpi-";
inline constexpr const char *TransactionMarker = "txn";
inline constexpr const char *RecoveryPackage = "draft.map";
inline constexpr const char *PayloadDirectory = "p";
inline constexpr const char *OwnershipMarker = ".owner";
inline constexpr const char *TransactionMagic = "GM_EDITOR_MAP_IMPORT_TRANSACTION_V1";

struct TransactionAssetRecord
{
    MapPackageAssetKind kind = MapPackageAssetKind::Background;
    std::filesystem::path relativePath;
    std::string contentHash;
    std::uint64_t byteCount = 0;
};

struct TransactionRecord
{
    std::string transactionId;
    std::string finalDirectoryName;
    std::vector<TransactionAssetRecord> assets;
};

struct ValidatedRoots
{
    std::filesystem::path project;
    std::filesystem::path assetLibrary;
};

std::string createShortTransactionId();
bool safeLeaf(const std::string &value);
bool safeRelative(const std::filesystem::path &path);
std::string comparableComponent(std::filesystem::path component);
bool pathInside(const std::filesystem::path &root, const std::filesystem::path &candidate);
bool validateRoots(const std::filesystem::path &projectRoot,
                   const std::filesystem::path &assetLibraryDirectory,
                   ValidatedRoots &roots, std::string &error);
bool writeTextFile(const std::filesystem::path &path, const std::string &text, std::string &error);
std::string serializeRecord(const TransactionRecord &record);
bool readRecord(const std::filesystem::path &path, const MapPackageLimits &limits,
                TransactionRecord &record, std::string &error);
std::string fileHash(const std::filesystem::path &path, const MapPackageLimits &limits,
                     std::uint64_t expectedBytes, std::string &error);
bool hasOwnership(const std::filesystem::path &directory, const std::string &transactionId);
bool removeOwnedFinalDirectory(const std::filesystem::path &finalDirectory,
                               const std::string &transactionId, std::string &error);
bool shouldFail(const MapPackageDraftInstallOptions &options,
                MapPackageDraftInstallFailurePoint point);
void patchImportedAssetPaths(MapModel &map, const std::vector<MapPackageMaterializedAsset> &assets,
                             const std::filesystem::path &projectRoot,
                             const std::filesystem::path &finalDirectory,
                             TransactionRecord &record);
std::filesystem::path transactionDirectory(const std::filesystem::path &assetLibrary,
                                           const std::string &transactionId);
} // namespace MapPackageDraftInstallDetail

