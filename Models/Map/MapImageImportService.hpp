#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Map/MapModels.hpp"

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace MapImageImport
{
constexpr int maximumSourceEdge = 16384;
constexpr std::int64_t maximumSourcePixels = 100000000;
constexpr int maximumWorkingProxyEdge = 8192;
constexpr std::size_t maximumTracePoints = 8192;

// Immutable sampling geometry shared by previews and export code. Image
// decoders apply EXIF orientation before this transform is used, so crop
// coordinates always address the visually oriented source. `mapViewport`
// describes which normalized part of the complete map a regional/tiled render
// covers; composing it here avoids rotating each export tile around its own
// centre.
struct NormalizedSamplingTransform
{
    float uFromX = 0.0f;
    float uFromY = 0.0f;
    float uOffset = 0.0f;
    float vFromX = 0.0f;
    float vFromY = 0.0f;
    float vOffset = 0.0f;
    float minimumU = 0.0f;
    float maximumU = 0.0f;
    float minimumV = 0.0f;
    float maximumV = 0.0f;
    bool valid = false;
};

inline float canonicalRotationDegrees(float rotationDegrees)
{
    if (!std::isfinite(rotationDegrees))
        return rotationDegrees;
    const float nearestQuarterTurn = std::round(rotationDegrees / 90.0f) * 90.0f;
    return std::abs(rotationDegrees - nearestQuarterTurn) <= 0.0001f
               ? nearestQuarterTurn
               : rotationDegrees;
}

inline NormalizedSamplingTransform makeNormalizedSamplingTransform(
    const MapNormalizedRect &crop,
    float rotationDegrees,
    const MapNormalizedRect &mapViewport = MapNormalizedRect{})
{
    NormalizedSamplingTransform transform;
    const auto finiteRect = [](const MapNormalizedRect &rect) {
        return std::isfinite(rect.x) && std::isfinite(rect.y) &&
               std::isfinite(rect.width) && std::isfinite(rect.height);
    };
    if (!finiteRect(crop) || !finiteRect(mapViewport) ||
        crop.x < 0.0f || crop.y < 0.0f || crop.width <= 0.0f || crop.height <= 0.0f ||
        crop.x + crop.width > 1.0001f || crop.y + crop.height > 1.0001f ||
        mapViewport.x < 0.0f || mapViewport.y < 0.0f ||
        mapViewport.width <= 0.0f || mapViewport.height <= 0.0f ||
        mapViewport.x + mapViewport.width > 1.0001f ||
        mapViewport.y + mapViewport.height > 1.0001f ||
        !std::isfinite(rotationDegrees))
        return transform;

    const float canonical = canonicalRotationDegrees(rotationDegrees);
    float cosine = 0.0f;
    float sine = 0.0f;
    const float quarterTurns = -canonical / 90.0f;
    const float roundedQuarterTurns = std::round(quarterTurns);
    if (std::abs(quarterTurns - roundedQuarterTurns) <= 0.0001f)
    {
        int turn = static_cast<int>(roundedQuarterTurns) % 4;
        if (turn < 0) turn += 4;
        switch (turn)
        {
        case 0: cosine = 1.0f; sine = 0.0f; break;
        case 1: cosine = 0.0f; sine = 1.0f; break;
        case 2: cosine = -1.0f; sine = 0.0f; break;
        default: cosine = 0.0f; sine = -1.0f; break;
        }
    }
    else
    {
        constexpr float degreesToRadians = 0.01745329251994329577f;
        const float radians = -canonical * degreesToRadians;
        cosine = std::cos(radians);
        sine = std::sin(radians);
    }

    const float viewportCenterX = mapViewport.x - 0.5f;
    const float viewportCenterY = mapViewport.y - 0.5f;
    transform.uFromX = crop.width * cosine * mapViewport.width;
    transform.uFromY = crop.width * -sine * mapViewport.height;
    transform.uOffset = crop.x + crop.width *
        (cosine * viewportCenterX - sine * viewportCenterY + 0.5f);
    transform.vFromX = crop.height * sine * mapViewport.width;
    transform.vFromY = crop.height * cosine * mapViewport.height;
    transform.vOffset = crop.y + crop.height *
        (sine * viewportCenterX + cosine * viewportCenterY + 0.5f);
    transform.minimumU = crop.x;
    transform.maximumU = crop.x + crop.width;
    transform.minimumV = crop.y;
    transform.maximumV = crop.y + crop.height;
    transform.valid = true;
    return transform;
}

inline bool sampleNormalizedImage(const NormalizedSamplingTransform &transform,
                                  float normalizedX,
                                  float normalizedY,
                                  float &sourceU,
                                  float &sourceV)
{
    if (!transform.valid || !std::isfinite(normalizedX) ||
        !std::isfinite(normalizedY))
        return false;
    sourceU = transform.uOffset + transform.uFromX * normalizedX +
              transform.uFromY * normalizedY;
    sourceV = transform.vOffset + transform.vFromX * normalizedX +
              transform.vFromY * normalizedY;
    constexpr float boundaryEpsilon = 0.000001f;
    return sourceU >= transform.minimumU - boundaryEpsilon &&
           sourceU <= transform.maximumU + boundaryEpsilon &&
           sourceV >= transform.minimumV - boundaryEpsilon &&
           sourceV <= transform.maximumV + boundaryEpsilon;
}

struct Request
{
    std::filesystem::path sourcePath;
    MapCreationIntent intent = MapCreationIntent::ImportedReference;
    MapSizePreset sizePreset = MapSizePreset::Standard;
    MapSpatialReference spatialReference;
    MapNormalizedRect crop;
    float rotationDegrees = 0.0f;
    // Used only when explicitly adopting an already displayed legacy
    // background. The current map contract and cell payload remain intact.
    bool preserveExistingMapContract = false;
};

struct Result
{
    bool success = false;
    std::string errorCode;
    std::string errorDetail;
    EntityId assetId;
    std::string projectPath;
    std::string contentHash;
    int sourceWidth = 0;
    int sourceHeight = 0;
    int displayWidth = 0;
    int displayHeight = 0;
    int exifOrientation = 1;
    bool workingProxyGenerated = false;
    EntityId workingProxyAssetId;
    std::string workingProxyProjectPath;
    std::string workingProxyContentHash;
    bool deduplicated = false;
};

struct TraceRequest
{
    MapGeneratedFeatureKind kind = MapGeneratedFeatureKind::Landmark;
    std::string name;
    std::string materialId;
    std::vector<MapFeaturePoint> points;
    bool closed = false;
    bool gmOnly = true;
};

struct LegacyReferencePreview
{
    bool available = false;
    std::string errorCode;
    std::string storedPath;
    std::filesystem::path resolvedSourcePath;
    std::string contentHash;
};

struct ManagedReferenceInspection
{
    bool healthy = false;
    bool originalAvailable = false;
    bool workingProxyAvailable = false;
    bool contentHashesVerified = false;
    std::string errorCode;
};

bool isSupportedSource(const std::filesystem::path &sourcePath);
Result importReference(const Campaign &campaign, MapModel &map, const Request &request);
bool updateCalibration(MapModel &map,
                       const MapNormalizedRect &crop,
                       float rotationDegrees,
                       const MapSpatialReference &spatialReference,
                       std::string *errorCode = nullptr);
bool addTraceFeature(MapModel &map, const TraceRequest &request, EntityId *createdId = nullptr,
                     std::string *errorCode = nullptr);
// Inspecting a legacy background is read-only. Adoption is a separate,
// explicit operation which copies the source into managed campaign storage.
LegacyReferencePreview previewLegacyReference(const MapModel &map);
// Read-only validation for an existing managed reference. The lightweight
// form checks safe paths and file presence for UI status; hash verification is
// intended for explicit validation/export gates because large originals can
// take noticeable time to hash.
ManagedReferenceInspection inspectManagedReference(const MapImportedReference &reference,
                                                    bool verifyContentHashes = false);
// Stable inspection codes stay in persistence/tests; this returns the
// user-facing English source text consumed by the normal DE/EN UI catalog.
const char *managedReferenceErrorMessage(const std::string &errorCode);
const char *errorMessage(const std::string &errorCode);
Result adoptLegacyReference(const Campaign &campaign, MapModel &map);
// Runtime/editor/PNG consumers should resolve this path first. It selects the
// bounded, already oriented proxy when available and otherwise the immutable
// original. The returned value is always a stored project-relative path.
const std::string &preferredRenderProjectPath(const MapImportedReference &reference);
const std::string &preferredRenderContentHash(const MapImportedReference &reference);
const EntityId &preferredRenderAssetId(const MapImportedReference &reference);
void removeReference(MapModel &map);
}
