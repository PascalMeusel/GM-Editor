#include "Models/Map/MapPackageService.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include "Models/Project/ApplicationPaths.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <unordered_set>
#include <unordered_map>

#include "Models/Map/MapPackageServiceInternal.hpp"

using namespace MapPackageServiceInternal;

MapPackagePreview MapPackageService::previewPackage(const std::filesystem::path &packagePath, const MapPackageLimits &limits)
{
    MapPackagePreview preview;
    try
    {
        DecodedPackage decoded = decode(packagePath, limits);
        // The filesystem size is safe to expose after the bounded regular-file
        // check. All package-derived metadata remains empty until the complete
        // envelope, payload hash, map and embedded asset hashes have passed.
        preview.packageBytes = decoded.fileBytes;
        if (!decoded.error.empty()) { preview.error = decoded.error; return preview; }
        const MapModel &map = decoded.map;
        preview.formatVersion = decoded.version;
        preview.sourceMapId = map.metadata.id; preview.mapName = map.name; preview.creationIntent = map.creationIntent;
        preview.atlasLevel = map.atlasLevel; preview.mapKind = map.mapKind; preview.visibility = map.visibility;
        preview.playerRelease = map.playerRelease;
        preview.gameMasterOnly = map.visibility == MapVisibility::GameMasterOnly;
        preview.knownToPlayers = map.visibility != MapVisibility::GameMasterOnly;
        preview.visibleToPlayers = map.visibility == MapVisibility::PlayersVisible;
        preview.enabledPlayerReleaseCategoryCount =
            static_cast<std::size_t>(map.playerRelease.politicalBorders) +
            static_cast<std::size_t>(map.playerRelease.waysAndRoutes) +
            static_cast<std::size_t>(map.playerRelease.placesAndMarkers) +
            static_cast<std::size_t>(map.playerRelease.labels) +
            static_cast<std::size_t>(map.playerRelease.tokensAndAnnotations);
        preview.width = map.width; preview.height = map.height; preview.cellCount = map.cells.size();
        preview.generatedFeatureCount = map.generation.graph.features.size(); preview.manualFeatureCount = map.features.size();
        preview.regionCount = map.regions.size(); preview.routeCount = map.routes.size(); preview.tokenCount = map.tokens.size();
        preview.layerCount = map.layers.size(); preview.generatorVersion = map.generation.recipe.generatorVersion;
        preview.recipeVersion = map.generation.recipe.recipeVersion;
        preview.recipeStyleVersion = map.generation.recipe.styleVersion;
        preview.graphSchemaVersion = map.generation.graph.schemaVersion; preview.recipeId = map.generation.recipe.recipeId;
        preview.styleId = map.generation.recipe.styleId;
        preview.styleSnapshotId = map.generation.recipe.styleSnapshotId;

        preview.sourceAtlasStatus = map.atlasStatus;
        preview.sourceHasAtlasParent = !map.parentMapId.empty() || !map.atlasPlacement.parentId.empty();
        preview.sourceAtlasParentConflict = !map.parentMapId.empty() &&
                                            !map.atlasPlacement.parentId.empty() &&
                                            map.parentMapId != map.atlasPlacement.parentId;
        preview.independentCopyDetachesAtlasParent = preview.sourceHasAtlasParent;
        preview.independentCopyNormalizesAtlasStatus = map.atlasStatus != MapAtlasStatus::Standalone;
        const FeatureLineageSummary lineage = summarizeFeatureLineage(map);
        preview.sourceFeatureLineageCount = lineage.total;
        preview.externalFeatureLineageCount = lineage.external;
        preview.independentCopyDetachedLineageCount = lineage.external;
        preview.independentCopyRetainedInternalLineageCount = lineage.internal;

        const OwnedIdentitySummary identities = summarizeOwnedIdentities(map);
        preview.ownedIdFieldCount = identities.fieldCount;
        preview.uniqueOwnedIdCount = identities.uniqueCount;
        preview.duplicateOwnedIdCount = identities.duplicateCount;
        preview.independentCopyIdRemapCount = identities.uniqueCount;
        preview.independentCopyAssetIdRemapCount = identities.uniqueAssetIdCount;
        for (const MapPackageAsset &asset : decoded.assets)
        {
            if (preview.embeddedAssetBytes >
                std::numeric_limits<std::uint64_t>::max() - asset.bytes.size())
            {
                preview.error = "Embedded asset byte total exceeds the package safety limit.";
                return preview;
            }
            preview.embeddedAssetBytes += static_cast<std::uint64_t>(asset.bytes.size());
            preview.assets.push_back({asset.kind, asset.packagePath, asset.sourceFileName, asset.contentHash, asset.bytes.size()});
        }
        preview.embeddedAssetCount = preview.assets.size();
        preview.embeddedAssetConflictCount = 0;
        preview.embeddedAssetIntegrityVerified = true;
        preview.valid = true;
        preview.importEligible = preview.duplicateOwnedIdCount == 0;
    }
    catch (const std::exception &exception)
    {
        preview.error = std::string("Map package preview failed: ") + exception.what();
    }
    return preview;
}

MapPackageImportResult MapPackageService::importPackage(
    const std::filesystem::path &packagePath, const MapPackageImportOptions &options)
{
    MapPackageImportResult result;
    try
    {
        DecodedPackage decoded = decode(packagePath, options.limits);
        if (!decoded.error.empty()) { result.error = decoded.error; return result; }
        const OwnedIdentitySummary identities = summarizeOwnedIdentities(decoded.map);
        if (identities.duplicateCount != 0)
        {
            result.error = "Map package contains conflicting owned entity identities.";
            return result;
        }
        remapOwnedIds(decoded.map, result.idRemapping, options.standaloneCopy, options.remapEntityIds);
        result.map = std::move(decoded.map); result.assets = std::move(decoded.assets); result.success = true;
    }
    catch (const std::exception &exception)
    {
        result.error = std::string("Map package import failed: ") + exception.what();
    }
    return result;
}

bool MapPackageService::importInto(const std::filesystem::path &packagePath, MapModel &targetMap,
                                   std::vector<MapPackageAsset> *targetAssets, std::string *error,
                                   const MapPackageImportOptions &options)
{
    MapPackageImportResult result = importPackage(packagePath, options);
    if (!result.success)
    {
        if (error) *error = result.error;
        return false;
    }
    targetMap = std::move(result.map);
    if (targetAssets) *targetAssets = std::move(result.assets);
    if (error) error->clear();
    return true;
}
