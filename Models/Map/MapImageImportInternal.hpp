#pragma once

#include "Models/Map/MapImageImportService.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"

namespace MapImageImport::detail
{
std::string lowerExtension(const std::filesystem::path &path);
bool validCrop(const MapNormalizedRect &crop);
bool validSpatialReference(const MapSpatialReference &spatialReference, std::string &errorCode);
std::string contentAddressedAssetId(const char *role, const std::string &contentHash);
float squaredDistance(const MapFeaturePoint &left, const MapFeaturePoint &right);
bool validTraceGeometry(const TraceRequest &request, std::string &errorCode);
bool featureNeedsLine(MapGeneratedFeatureKind kind);
bool featureNeedsPolygon(MapGeneratedFeatureKind kind);
bool pathIsBelow(const std::filesystem::path &root, const std::filesystem::path &candidate);
bool managedFileHashMatches(const std::filesystem::path &path, const std::string &expectedHash);
bool verifyManagedAsset(const ProjectAssetImport::AssetImportResult &asset);
bool inspectManagedFile(const std::string &storedPath, const std::string &expectedHash,
                        bool verifyContentHash, const char *unsafeCode, const char *missingCode,
                        const char *changedCode, std::string &errorCode);
void removeFailedNewAsset(const ProjectAssetImport::AssetImportResult &asset);

struct TemporaryProxy final
{
    std::filesystem::path path;
    ~TemporaryProxy();
};
} // namespace MapImageImport::detail

