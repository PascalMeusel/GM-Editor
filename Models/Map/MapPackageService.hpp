#pragma once

#include "Models/Map/MapModels.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

enum class MapPackageAssetKind
{
    Background,
    ImportedReference,
    WorkingProxy
};

struct MapPackageAsset
{
    MapPackageAssetKind kind = MapPackageAssetKind::Background;
    std::string packagePath;
    std::string sourceFileName;
    std::string contentHash;
    std::vector<std::uint8_t> bytes;
};

struct MapPackageLimits
{
    std::uint64_t maximumPackageBytes = 256ull * 1024ull * 1024ull;
    std::uint64_t maximumAssetBytes = 128ull * 1024ull * 1024ull;
    std::uint32_t maximumStringBytes = 1024u * 1024u;
    std::uint32_t maximumCollectionItems = 4u * 1024u * 1024u;
    std::uint32_t maximumPathBytes = 4096u;
};

struct MapPackageExportOptions
{
    // Existing files are protected unless this is explicitly enabled.
    bool allowOverwrite = false;
    bool includeBackgroundAsset = true;
    bool includeImportedAsset = true;
    bool includeWorkingProxyAsset = true;

    // Explicit source paths take precedence. Empty paths fall back to the
    // corresponding paths stored on MapModel.
    std::filesystem::path backgroundAssetPath;
    std::filesystem::path importedAssetPath;
    std::filesystem::path workingProxyAssetPath;
    MapPackageLimits limits;
};

struct MapPackageExportResult
{
    bool success = false;
    std::filesystem::path packagePath;
    std::uint64_t packageBytes = 0;
    std::string error;
};

struct MapPackageAssetPreview
{
    MapPackageAssetKind kind = MapPackageAssetKind::Background;
    std::string packagePath;
    std::string sourceFileName;
    std::string contentHash;
    std::uint64_t byteCount = 0;
};

struct MapPackagePreview
{
    bool valid = false;
    // A package can be byte-for-byte valid while still containing duplicate
    // owned identities that cannot be imported safely.  Callers should only
    // enable Import when both valid and importEligible are true.
    bool importEligible = false;
    std::uint32_t formatVersion = 0;
    std::uint64_t packageBytes = 0;
    std::uint64_t embeddedAssetBytes = 0;
    std::size_t embeddedAssetCount = 0;
    EntityId sourceMapId;
    std::string mapName;
    MapCreationIntent creationIntent = MapCreationIntent::WorldOverview;
    MapLevel atlasLevel = MapLevel::World;
    MapKind mapKind = MapKind::General;
    MapVisibility visibility = MapVisibility::GameMasterOnly;
    MapPlayerReleaseProfile playerRelease;
    bool gameMasterOnly = true;
    bool knownToPlayers = false;
    bool visibleToPlayers = false;
    std::size_t enabledPlayerReleaseCategoryCount = 0;
    int width = 0;
    int height = 0;
    std::size_t cellCount = 0;
    std::size_t generatedFeatureCount = 0;
    std::size_t manualFeatureCount = 0;
    std::size_t regionCount = 0;
    std::size_t routeCount = 0;
    std::size_t tokenCount = 0;
    std::size_t layerCount = 0;
    int generatorVersion = 0;
    int recipeVersion = 0;
    int recipeStyleVersion = 0;
    int graphSchemaVersion = 0;
    std::string recipeId;
    std::string styleId;
    std::string styleSnapshotId;

    // Source atlas facts and the exact effect of the default independent-copy
    // import.  Internal self-lineage remains internal after ID remapping;
    // lineage to maps outside the package is detached and unlocked.
    MapAtlasStatus sourceAtlasStatus = MapAtlasStatus::Standalone;
    bool sourceHasAtlasParent = false;
    bool sourceAtlasParentConflict = false;
    bool independentCopyDetachesAtlasParent = false;
    bool independentCopyNormalizesAtlasStatus = false;
    std::size_t sourceFeatureLineageCount = 0;
    std::size_t externalFeatureLineageCount = 0;
    std::size_t independentCopyDetachedLineageCount = 0;
    std::size_t independentCopyRetainedInternalLineageCount = 0;

    // Import/remap summary. Counts never contain identity values, so an import
    // confirmation can explain its impact without exposing technical IDs.
    std::size_t ownedIdFieldCount = 0;
    std::size_t uniqueOwnedIdCount = 0;
    std::size_t duplicateOwnedIdCount = 0;
    std::size_t independentCopyIdRemapCount = 0;
    std::size_t independentCopyAssetIdRemapCount = 0;
    // Duplicate paths/kinds and hash mismatches are rejected by decode; a
    // valid preview therefore reports zero verified internal asset conflicts.
    std::size_t embeddedAssetConflictCount = 0;
    bool embeddedAssetIntegrityVerified = false;
    std::vector<MapPackageAssetPreview> assets;
    std::string error;
};

struct MapPackageImportOptions
{
    // A package is imported as an independent copy by default. This removes
    // its atlas parent and gives the map and its owned children new IDs.
    bool standaloneCopy = true;
    bool remapEntityIds = true;
    MapPackageLimits limits;
};

struct MapPackageImportResult
{
    bool success = false;
    MapModel map;
    std::vector<MapPackageAsset> assets;
    // Old-to-new identity mappings, including the map and owned children.
    std::vector<std::pair<std::string, std::string>> idRemapping;
    std::string error;
};

struct MapPackageMaterializationOptions
{
    // Must name an existing, real directory. Embedded package paths are
    // resolved below this directory and may never escape it.
    std::filesystem::path targetDirectory;
    bool allowOverwrite = false;
    MapPackageLimits limits;
};

struct MapPackageMaterializedAsset
{
    MapPackageAssetKind kind = MapPackageAssetKind::Background;
    std::string packagePath;
    // Relative to MapPackageMaterializationOptions::targetDirectory.
    std::string projectRelativePath;
    std::filesystem::path destinationPath;
    std::string contentHash;
    std::uint64_t byteCount = 0;
};

struct MapPackageMaterializationResult
{
    bool success = false;
    std::vector<MapPackageMaterializedAsset> assets;
    std::string error;
};

class MapPackageService
{
public:
    // V4 adds explicit fail-closed player map categories. V1-V3 packages
    // remain readable and import with every optional category hidden.
    static constexpr std::uint32_t CurrentFormatVersion = 6;

    static MapPackageExportResult exportPackage(
        const MapModel &map,
        const std::filesystem::path &packagePath,
        const MapPackageExportOptions &options = {});

    static MapPackagePreview previewPackage(
        const std::filesystem::path &packagePath,
        const MapPackageLimits &limits = {});

    static MapPackageImportResult importPackage(
        const std::filesystem::path &packagePath,
        const MapPackageImportOptions &options = {});

    // Transactional convenience API: targetMap and targetAssets are unchanged
    // whenever reading, validation, hashing, or ID remapping fails.
    static bool importInto(
        const std::filesystem::path &packagePath,
        MapModel &targetMap,
        std::vector<MapPackageAsset> *targetAssets = nullptr,
        std::string *error = nullptr,
        const MapPackageImportOptions &options = {});

    // Transactionally materializes already-decoded assets. All paths and
    // hashes are revalidated. On failure, no new files remain and replaced
    // files are restored.
    static MapPackageMaterializationResult materializeAssets(
        const std::vector<MapPackageAsset> &assets,
        const MapPackageMaterializationOptions &options);

    // Convenience form that validates and reads the package before staging
    // its embedded assets.
    static MapPackageMaterializationResult materializePackageAssets(
        const std::filesystem::path &packagePath,
        const MapPackageMaterializationOptions &options);

    // Readable aliases for UI call sites.
    static MapPackageExportResult exportMap(
        const MapModel &map,
        const std::filesystem::path &packagePath,
        const MapPackageExportOptions &options = {})
    {
        return exportPackage(map, packagePath, options);
    }

    static MapPackagePreview previewImport(
        const std::filesystem::path &packagePath,
        const MapPackageLimits &limits = {})
    {
        return previewPackage(packagePath, limits);
    }

    static MapPackageImportResult importMap(
        const std::filesystem::path &packagePath,
        const MapPackageImportOptions &options = {})
    {
        return importPackage(packagePath, options);
    }
};
