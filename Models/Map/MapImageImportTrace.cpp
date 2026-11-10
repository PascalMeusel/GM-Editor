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

bool addTraceFeature(MapModel &map, const TraceRequest &request, EntityId *createdId, std::string *errorCode)
{
    if (!map.importedReference.enabled)
    {
        if (errorCode) *errorCode = "map.import.reference_missing";
        return false;
    }
    const std::size_t minimumPoints = (featureNeedsPolygon(request.kind) || request.closed)
                                          ? 3u
                                          : featureNeedsLine(request.kind) ? 2u : 1u;
    if (request.points.size() < minimumPoints)
    {
        if (errorCode) *errorCode = "map.import.trace_too_short";
        return false;
    }
    for (const MapFeaturePoint &point : request.points)
    {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < 0.0f || point.y < 0.0f ||
            point.x > static_cast<float>(map.width) || point.y > static_cast<float>(map.height))
        {
            if (errorCode) *errorCode = "map.import.trace_outside_map";
            return false;
        }
    }
    std::string geometryError;
    if (!validTraceGeometry(request, geometryError))
    {
        if (errorCode) *errorCode = geometryError;
        return false;
    }

    MapGeneratedFeature feature;
    feature.id = createEntityId("traced_feature");
    feature.kind = request.kind;
    feature.name = request.name;
    feature.materialId = request.materialId;
    feature.points = request.points;
    feature.closed = featureNeedsPolygon(request.kind) || request.closed;
    feature.locked = true;
    feature.gmOnly = request.gmOnly;
    feature.discovered = !request.gmOnly;
    map.generation.enabled = true;
    map.generation.continuousRendering = true;
    map.generation.graph.features.push_back(std::move(feature));
    map.importedReference.semanticallyTraced = true;
    map.revision += 1;
    map.metadata.touch();
    if (createdId) *createdId = map.generation.graph.features.back().id;
    if (errorCode) errorCode->clear();
    return true;
}

const char *managedReferenceErrorMessage(const std::string &errorCode)
{
    if (errorCode == "map.import.reference_path_unsafe")
        return "The managed map image path is unsafe. Choose the image again to repair it.";
    if (errorCode == "map.import.original_missing")
        return "The original map image is missing. Choose a replacement to repair it.";
    if (errorCode == "map.import.original_changed")
        return "The original map image no longer matches the imported file.";
    if (errorCode == "map.import.proxy_missing")
        return "The map image working copy is missing. Choose the image again to rebuild it.";
    if (errorCode == "map.import.proxy_changed")
        return "The map image working copy no longer matches the imported file.";
    return "The managed map image is not available.";
}

const char *errorMessage(const std::string &errorCode)
{
    if (errorCode == "map.import.file_missing")
        return "The selected map image does not exist.";
    if (errorCode == "map.import.unsupported_format")
        return "This image format is not supported. Use PNG or JPEG.";
    if (errorCode == "map.import.invalid_crop")
        return "The image crop is invalid.";
    if (errorCode == "map.import.invalid_calibration")
        return "The image calibration is invalid.";
    if (errorCode == "map.import.invalid_extent")
        return "The calibrated physical size must be greater than zero.";
    if (errorCode == "map.import.invalid_unit")
        return "Choose a distance unit for a calibrated map.";
    if (errorCode == "map.import.decode_failed")
        return "The map image could not be decoded.";
    if (errorCode == "map.import.read_failed")
        return "The map image could not be read.";
    if (errorCode == "map.import.source_changed")
        return "The map image changed during import. Choose it again.";
    if (errorCode == "map.import.too_large")
        return "The map image exceeds 16384 pixels on one edge or 100 megapixels.";
    if (errorCode == "map.import.copy_failed")
        return "The map image could not be copied into the campaign.";
    if (errorCode == "map.import.proxy_resize_failed")
        return "The map image working copy could not be resized.";
    if (errorCode == "map.import.proxy_temp_failed")
        return "A safe temporary map image could not be created.";
    if (errorCode == "map.import.proxy_encode_failed")
        return "The map image working copy could not be encoded.";
    if (errorCode == "map.import.proxy_read_failed")
        return "The map image working copy could not be verified.";
    if (errorCode == "map.import.proxy_copy_failed")
        return "The map image working copy could not be copied into the campaign.";
    if (errorCode == "map.import.reference_missing")
        return "No managed map image is attached.";
    if (errorCode == "map.import.trace_too_short")
        return "Add more control points for this map feature.";
    if (errorCode == "map.import.trace_outside_map")
        return "A map feature control point lies outside the map.";
    if (errorCode == "map.import.trace_too_many_points")
        return "The traced map feature contains too many control points.";
    if (errorCode == "map.import.trace_degenerate")
        return "The traced map feature does not form a valid line or area.";
    if (errorCode == "map.import.legacy_reference_missing")
        return "This map has no unmanaged legacy background image.";
    if (errorCode.rfind("map.import.", 0) == 0)
        return managedReferenceErrorMessage(errorCode);
    return "The map image operation failed.";
}

const std::string &preferredRenderProjectPath(const MapImportedReference &reference)
{
    return reference.workingProxyGenerated && !reference.workingProxyProjectRelativePath.empty()
               ? reference.workingProxyProjectRelativePath
               : reference.projectRelativePath;
}

const std::string &preferredRenderContentHash(const MapImportedReference &reference)
{
    return reference.workingProxyGenerated && !reference.workingProxyContentHash.empty()
               ? reference.workingProxyContentHash
               : reference.contentHash;
}

const EntityId &preferredRenderAssetId(const MapImportedReference &reference)
{
    return reference.workingProxyGenerated && !reference.workingProxyAssetId.empty()
               ? reference.workingProxyAssetId
               : reference.assetId;
}

void removeReference(MapModel &map)
{
    const std::string originalPath = map.importedReference.projectRelativePath;
    const std::string proxyPath = map.importedReference.workingProxyProjectRelativePath;
    map.importedReference = MapImportedReference{};
    if (map.backgroundImagePath == originalPath || map.backgroundImagePath == proxyPath)
        map.backgroundImagePath.clear();
    map.revision += 1;
    map.metadata.touch();
}
}
