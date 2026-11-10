#include "MapImageImportService.hpp"

#include "Models/Map/MapImportedReferencePolicy.hpp"

#include "Models/Project/ProjectAssetImportService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Media/RgbaImageLoader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <mutex>
#include <system_error>
#include <unordered_map>

#include "Models/Map/MapImageImportInternal.hpp"

namespace MapImageImport
{
using namespace detail;

bool isSupportedSource(const std::filesystem::path &sourcePath)
{
    const std::string extension = lowerExtension(sourcePath);
    if (extension == ".png")
    {
#if defined(_WIN32) || defined(TTRPG_HAS_LIBPNG)
        return true;
#else
        return false;
#endif
    }
    if (extension == ".jpg" || extension == ".jpeg")
    {
#if defined(_WIN32) || defined(TTRPG_HAS_LIBJPEG)
        return true;
#else
        return false;
#endif
    }
    return false;
}

Result importReference(const Campaign &campaign, MapModel &map, const Request &request)
{
    Result result;
    if (request.sourcePath.empty() || !std::filesystem::is_regular_file(request.sourcePath))
    {
        result.errorCode = "map.import.file_missing";
        return result;
    }
    if (!isSupportedSource(request.sourcePath))
    {
        result.errorCode = "map.import.unsupported_format";
        result.errorDetail = request.sourcePath.extension().string();
        return result;
    }
    if (!validCrop(request.crop))
    {
        result.errorCode = "map.import.invalid_crop";
        return result;
    }
    if (!std::isfinite(request.rotationDegrees) || request.rotationDegrees < -180.0f ||
        request.rotationDegrees > 180.0f)
    {
        result.errorCode = "map.import.invalid_calibration";
        return result;
    }
    if (!validSpatialReference(request.spatialReference, result.errorCode))
        return result;

    const std::string sourceHashBeforeDecode = ProjectAssetImport::computeContentHash(request.sourcePath);
    if (sourceHashBeforeDecode.empty())
    {
        result.errorCode = "map.import.read_failed";
        return result;
    }
    LoadedRgbaImage image;
    RgbaImageLoadOptions loadOptions;
    loadOptions.maximumSourceEdge = maximumSourceEdge;
    loadOptions.maximumSourcePixels = static_cast<std::uint64_t>(maximumSourcePixels);
    loadOptions.applyExifOrientation = true;
    std::string imageError;
    if (!loadRgbaImage(request.sourcePath, image, loadOptions, &imageError))
    {
        result.errorCode = imageError == "image.too_large" ? "map.import.too_large" : "map.import.decode_failed";
        result.errorDetail = imageError;
        result.sourceWidth = image.sourceWidth;
        result.sourceHeight = image.sourceHeight;
        return result;
    }
    result.sourceWidth = image.sourceWidth;
    result.sourceHeight = image.sourceHeight;
    result.displayWidth = image.width;
    result.displayHeight = image.height;
    result.exifOrientation = image.exifOrientation;
    if (ProjectAssetImport::computeContentHash(request.sourcePath) != sourceHashBeforeDecode)
    {
        result.errorCode = "map.import.source_changed";
        return result;
    }

    TemporaryProxy temporaryProxy;
    LoadedRgbaImage proxyImage;
    const bool needsProxy = std::max(image.width, image.height) > maximumWorkingProxyEdge;
    if (needsProxy)
    {
        const int longEdge = std::max(image.width, image.height);
        const int proxyWidth = std::max(1, static_cast<int>(
            (static_cast<std::int64_t>(image.width) * maximumWorkingProxyEdge + longEdge / 2) / longEdge));
        const int proxyHeight = std::max(1, static_cast<int>(
            (static_cast<std::int64_t>(image.height) * maximumWorkingProxyEdge + longEdge / 2) / longEdge));
        if (!resizeRgbaImage(image, proxyWidth, proxyHeight, proxyImage))
        {
            result.errorCode = "map.import.proxy_resize_failed";
            return result;
        }
        try
        {
            temporaryProxy.path = std::filesystem::temp_directory_path() /
                                  (createEntityId("map_working_proxy") + ".png");
        }
        catch (const std::exception &exception)
        {
            result.errorCode = "map.import.proxy_temp_failed";
            (void)exception;
            return result;
        }
        if (!saveRgbaImageAsPng(temporaryProxy.path, proxyImage, &imageError))
        {
            result.errorCode = "map.import.proxy_encode_failed";
            result.errorDetail = imageError;
            return result;
        }
    }

    ProjectAssetImport::AssetImportRequest assetRequest;
    assetRequest.kind = ProjectAssetImport::AssetKind::Map;
    assetRequest.sourcePath = request.sourcePath;
    assetRequest.assetId = contentAddressedAssetId("map_reference_original", sourceHashBeforeDecode);
    assetRequest.displayName = request.sourcePath.stem().string();
    assetRequest.sourceReference = request.sourcePath.filename().string();
    assetRequest.deduplicateByHash = true;
    const ProjectAssetImport::AssetImportResult imported = ProjectAssetImport::importAsset(campaign, assetRequest);
    if (!imported.success || imported.contentHash != sourceHashBeforeDecode || !verifyManagedAsset(imported))
    {
        result.errorCode = "map.import.copy_failed";
        removeFailedNewAsset(imported);
        return result;
    }

    ProjectAssetImport::AssetImportResult importedProxy;
    if (needsProxy)
    {
        const std::string proxyHash = ProjectAssetImport::computeContentHash(temporaryProxy.path);
        if (proxyHash.empty())
        {
            result.errorCode = "map.import.proxy_read_failed";
            removeFailedNewAsset(imported);
            return result;
        }
        ProjectAssetImport::AssetImportRequest proxyRequest;
        proxyRequest.kind = ProjectAssetImport::AssetKind::Map;
        proxyRequest.sourcePath = temporaryProxy.path;
        proxyRequest.assetId = contentAddressedAssetId("map_reference_proxy", proxyHash);
        proxyRequest.displayName = request.sourcePath.stem().string() + " working proxy";
        proxyRequest.sourceReference = request.sourcePath.filename().string() + "#working-proxy";
        proxyRequest.deduplicateByHash = true;
        importedProxy = ProjectAssetImport::importAsset(campaign, proxyRequest);
        if (!importedProxy.success || !verifyManagedAsset(importedProxy))
        {
            result.errorCode = "map.import.proxy_copy_failed";
            removeFailedNewAsset(importedProxy);
            removeFailedNewAsset(imported);
            return result;
        }
    }

    MapImportedReference reference;
    reference.enabled = true;
    reference.assetId = imported.assetId;
    reference.projectRelativePath = imported.projectPath;
    reference.sourceFileName = imported.originalFileName;
    reference.contentHash = imported.contentHash;
    reference.sourceWidth = image.sourceWidth;
    reference.sourceHeight = image.sourceHeight;
    reference.exifOrientation = image.exifOrientation;
    if (needsProxy)
    {
        reference.workingProxyAssetId = importedProxy.assetId;
        reference.workingProxyProjectRelativePath = importedProxy.projectPath;
        reference.workingProxyContentHash = importedProxy.contentHash;
        reference.workingProxyWidth = proxyImage.width;
        reference.workingProxyHeight = proxyImage.height;
        reference.workingProxyGenerated = true;
    }
    reference.crop = request.crop;
    reference.rotationDegrees = canonicalRotationDegrees(request.rotationDegrees);
    reference.gmOnly = true;
    reference.semanticallyTraced = request.preserveExistingMapContract &&
                                   map.importedReference.semanticallyTraced;

    MapModel updated = map;
    if (!request.preserveExistingMapContract)
        applyMapCreationIntent(updated, request.intent, request.sizePreset, request.sizePreset != MapSizePreset::Custom);
    updated.importedReference = reference;
    updated.backgroundImagePath = preferredRenderProjectPath(reference);
    updated.spatialReference = request.spatialReference;
    updated.revision += 1;
    updated.metadata.touch();
    map = std::move(updated);

    result.success = true;
    result.assetId = imported.assetId;
    result.projectPath = imported.projectPath;
    result.contentHash = imported.contentHash;
    result.displayWidth = needsProxy ? proxyImage.width : image.width;
    result.displayHeight = needsProxy ? proxyImage.height : image.height;
    result.workingProxyGenerated = needsProxy;
    result.workingProxyAssetId = reference.workingProxyAssetId;
    result.workingProxyProjectPath = reference.workingProxyProjectRelativePath;
    result.workingProxyContentHash = reference.workingProxyContentHash;
    result.deduplicated = imported.deduplicated;
    return result;
}

bool updateCalibration(MapModel &map,
                       const MapNormalizedRect &crop,
                       float rotationDegrees,
                       const MapSpatialReference &spatialReference,
                       std::string *errorCode)
{
    if (!map.importedReference.enabled)
    {
        if (errorCode) *errorCode = "map.import.reference_missing";
        return false;
    }
    if (!validCrop(crop) || !std::isfinite(rotationDegrees) || rotationDegrees < -180.0f || rotationDegrees > 180.0f)
    {
        if (errorCode) *errorCode = "map.import.invalid_calibration";
        return false;
    }
    std::string spatialError;
    if (!validSpatialReference(spatialReference, spatialError))
    {
        if (errorCode) *errorCode = spatialError;
        return false;
    }
    map.importedReference.crop = crop;
    map.importedReference.rotationDegrees = canonicalRotationDegrees(rotationDegrees);
    map.spatialReference = spatialReference;
    map.revision += 1;
    map.metadata.touch();
    if (errorCode) errorCode->clear();
    return true;
}
}
