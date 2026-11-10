#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ProjectAssetImport
{
enum class AssetKind
{
    Image,
    Portrait,
    Token,
    Map,
    Audio,
    Handout,
    GenericFile
};

struct AssetImportRequest
{
    AssetKind kind = AssetKind::GenericFile;
    std::filesystem::path sourcePath;
    EntityId assetId;
    std::string displayName;
    std::string sourceReference;
    bool deduplicateByHash = true;
};

struct AssetImportResult
{
    bool success = false;
    std::string errorMessage;
    AssetKind kind = AssetKind::GenericFile;
    EntityId assetId;
    std::string displayName;
    std::string originalFileName;
    std::string originalSourcePath;
    std::string projectPath;
    std::string assetFolder;
    std::string mimeType;
    std::string contentHash;
    std::uintmax_t byteSize = 0;
    bool deduplicated = false;
};

const char *assetKindLabel(AssetKind kind);
std::string assetFolderName(AssetKind kind);
std::vector<std::string> supportedExtensions(AssetKind kind);
bool isSupportedAssetFile(AssetKind kind, const std::filesystem::path &sourcePath);
std::string computeContentHash(const std::filesystem::path &sourcePath);
AssetImportResult importAsset(const Campaign &campaign, const AssetImportRequest &request);
}
