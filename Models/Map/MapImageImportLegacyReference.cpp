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

LegacyReferencePreview previewLegacyReference(const MapModel &map)
{
    LegacyReferencePreview preview;
    preview.storedPath = map.backgroundImagePath;
    if (map.importedReference.enabled || map.backgroundImagePath.empty())
    {
        preview.errorCode = "map.import.legacy_reference_missing";
        return preview;
    }
    preview.resolvedSourcePath = ProjectAssets::resolveProjectPath(map.backgroundImagePath);
    std::error_code error;
    if (!std::filesystem::is_regular_file(preview.resolvedSourcePath, error) || error)
    {
        preview.errorCode = "map.import.file_missing";
        return preview;
    }
    if (!isSupportedSource(preview.resolvedSourcePath))
    {
        preview.errorCode = "map.import.unsupported_format";
        return preview;
    }
    preview.contentHash = ProjectAssetImport::computeContentHash(preview.resolvedSourcePath);
    if (preview.contentHash.empty())
    {
        preview.errorCode = "map.import.read_failed";
        return preview;
    }
    preview.available = true;
    return preview;
}

ManagedReferenceInspection inspectManagedReference(const MapImportedReference &reference,
                                                    bool verifyContentHashes)
{
    ManagedReferenceInspection inspection;
    if (!reference.enabled)
    {
        inspection.errorCode = "map.import.reference_missing";
        return inspection;
    }

    inspection.originalAvailable = inspectManagedFile(
        reference.projectRelativePath, reference.contentHash, verifyContentHashes,
        "map.import.reference_path_unsafe", "map.import.original_missing",
        "map.import.original_changed", inspection.errorCode);
    if (!inspection.originalAvailable)
        return inspection;

    inspection.workingProxyAvailable = !reference.workingProxyGenerated;
    if (reference.workingProxyGenerated)
    {
        inspection.workingProxyAvailable = inspectManagedFile(
            reference.workingProxyProjectRelativePath,
            reference.workingProxyContentHash,
            verifyContentHashes,
            "map.import.reference_path_unsafe", "map.import.proxy_missing",
            "map.import.proxy_changed", inspection.errorCode);
        if (!inspection.workingProxyAvailable)
            return inspection;
    }

    inspection.healthy = true;
    inspection.contentHashesVerified = verifyContentHashes;
    inspection.errorCode.clear();
    return inspection;
}

Result adoptLegacyReference(const Campaign &campaign, MapModel &map)
{
    const LegacyReferencePreview preview = previewLegacyReference(map);
    if (!preview.available)
    {
        Result result;
        result.errorCode = preview.errorCode;
        return result;
    }
    Request request;
    request.sourcePath = preview.resolvedSourcePath;
    request.intent = map.creationIntent;
    request.sizePreset = map.sizePreset;
    request.spatialReference = map.spatialReference;
    request.crop = {0.0f, 0.0f, 1.0f, 1.0f};
    request.rotationDegrees = 0.0f;
    request.preserveExistingMapContract = true;
    return importReference(campaign, map, request);
}
}
