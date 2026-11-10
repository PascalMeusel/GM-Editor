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

namespace MapImageImport::detail
{
std::string lowerExtension(const std::filesystem::path &path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return extension;
}

bool validCrop(const MapNormalizedRect &crop)
{
    return std::isfinite(crop.x) && std::isfinite(crop.y) && std::isfinite(crop.width) && std::isfinite(crop.height) &&
           crop.x >= 0.0f && crop.y >= 0.0f && crop.width > 0.0f && crop.height > 0.0f &&
           crop.x + crop.width <= 1.0001f && crop.y + crop.height <= 1.0001f;
}

bool validSpatialReference(const MapSpatialReference &spatialReference, std::string &errorCode)
{
    const int unit = static_cast<int>(spatialReference.unit);
    if (unit < static_cast<int>(MapDistanceUnit::Unspecified) ||
        unit > static_cast<int>(MapDistanceUnit::Feet))
    {
        errorCode = "map.import.invalid_unit";
        return false;
    }
    if (!std::isfinite(spatialReference.extentWidth) || !std::isfinite(spatialReference.extentHeight) ||
        !std::isfinite(spatialReference.originX) || !std::isfinite(spatialReference.originY) ||
        !std::isfinite(spatialReference.rotationDegrees) ||
        spatialReference.rotationDegrees < -180.0f || spatialReference.rotationDegrees > 180.0f)
    {
        errorCode = "map.import.invalid_calibration";
        return false;
    }
    if (spatialReference.extentWidth < 0.0f || spatialReference.extentHeight < 0.0f ||
        (spatialReference.calibrated &&
         (!(spatialReference.extentWidth > 0.0f) || !(spatialReference.extentHeight > 0.0f))))
    {
        errorCode = "map.import.invalid_extent";
        return false;
    }
    if (spatialReference.calibrated && spatialReference.unit == MapDistanceUnit::Unspecified)
    {
        errorCode = "map.import.invalid_unit";
        return false;
    }
    errorCode.clear();
    return true;
}

std::string contentAddressedAssetId(const char *role, const std::string &contentHash)
{
    std::string suffix;
    suffix.reserve(contentHash.size());
    for (unsigned char character : contentHash)
        if (std::isalnum(character))
            suffix.push_back(static_cast<char>(std::tolower(character)));
    return std::string(role) + "_" + (suffix.empty() ? "unhashed" : suffix);
}

float squaredDistance(const MapFeaturePoint &left, const MapFeaturePoint &right)
{
    const float dx = left.x - right.x;
    const float dy = left.y - right.y;
    return dx * dx + dy * dy;
}

bool featureNeedsLine(MapGeneratedFeatureKind kind);
bool featureNeedsPolygon(MapGeneratedFeatureKind kind);

bool validTraceGeometry(const MapImageImport::TraceRequest &request, std::string &errorCode)
{
    constexpr float epsilonSquared = 0.000001f;
    if (request.points.size() > MapImageImport::maximumTracePoints)
    {
        errorCode = "map.import.trace_too_many_points";
        return false;
    }
    if (request.points.size() <= 1 && !request.closed && !featureNeedsPolygon(request.kind))
        return true;

    bool hasLength = false;
    for (std::size_t index = 1; index < request.points.size(); ++index)
        hasLength = hasLength || squaredDistance(request.points[index - 1], request.points[index]) > epsilonSquared;
    if (!hasLength)
    {
        errorCode = "map.import.trace_degenerate";
        return false;
    }

    if (featureNeedsPolygon(request.kind) || request.closed)
    {
        std::vector<MapFeaturePoint> distinct;
        for (const MapFeaturePoint &point : request.points)
            if (std::none_of(distinct.begin(), distinct.end(), [&](const MapFeaturePoint &candidate) {
                    return squaredDistance(point, candidate) <= epsilonSquared;
                }))
                distinct.push_back(point);
        if (distinct.size() < 3)
        {
            errorCode = "map.import.trace_degenerate";
            return false;
        }
        double twiceArea = 0.0;
        for (std::size_t index = 0; index < request.points.size(); ++index)
        {
            const MapFeaturePoint &left = request.points[index];
            const MapFeaturePoint &right = request.points[(index + 1) % request.points.size()];
            twiceArea += static_cast<double>(left.x) * right.y - static_cast<double>(right.x) * left.y;
        }
        if (std::abs(twiceArea) <= 0.001)
        {
            errorCode = "map.import.trace_degenerate";
            return false;
        }
    }
    return true;
}

bool featureNeedsLine(MapGeneratedFeatureKind kind)
{
    return kind == MapGeneratedFeatureKind::River || kind == MapGeneratedFeatureKind::Road ||
           kind == MapGeneratedFeatureKind::Trail || kind == MapGeneratedFeatureKind::Coastline;
}

bool featureNeedsPolygon(MapGeneratedFeatureKind kind)
{
    return kind == MapGeneratedFeatureKind::TerrainRegion || kind == MapGeneratedFeatureKind::WaterBody ||
           kind == MapGeneratedFeatureKind::Nation || kind == MapGeneratedFeatureKind::District;
}

bool pathIsBelow(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    for (; rootPart != root.end(); ++rootPart, ++candidatePart)
    {
        if (candidatePart == candidate.end()) return false;
#if defined(_WIN32)
        std::string left = rootPart->u8string();
        std::string right = candidatePart->u8string();
        std::transform(left.begin(), left.end(), left.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        std::transform(right.begin(), right.end(), right.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        if (left != right) return false;
#else
        if (*rootPart != *candidatePart) return false;
#endif
    }
    return candidatePart != candidate.end();
}

bool managedFileHashMatches(const std::filesystem::path &path,
                            const std::string &expectedHash)
{
    struct CachedHash
    {
        std::filesystem::file_time_type lastWriteTime{};
        std::uintmax_t byteSize = 0;
        std::string expectedHash;
        bool matches = false;
    };
    static std::mutex cacheMutex;
    static std::unordered_map<std::string, CachedHash> cache;

    std::error_code error;
    const std::filesystem::file_time_type lastWriteTime =
        std::filesystem::last_write_time(path, error);
    if (error)
        return false;
    const std::uintmax_t byteSize = std::filesystem::file_size(path, error);
    if (error)
        return false;

    const std::string key = path.lexically_normal().generic_string();
    std::lock_guard<std::mutex> lock(cacheMutex);
    const auto found = cache.find(key);
    if (found != cache.end() && found->second.lastWriteTime == lastWriteTime &&
        found->second.byteSize == byteSize &&
        found->second.expectedHash == expectedHash)
        return found->second.matches;

    const bool matches = !expectedHash.empty() &&
                         ProjectAssetImport::computeContentHash(path) == expectedHash;
    cache[key] = {lastWriteTime, byteSize, expectedHash, matches};
    return matches;
}

bool verifyManagedAsset(const ProjectAssetImport::AssetImportResult &asset)
{
    if (!asset.success || asset.contentHash.empty() ||
        !MapImportedReferencePolicy::isSafeProjectRelativePath(asset.projectPath)) return false;
    std::error_code error;
    const std::filesystem::path root = std::filesystem::weakly_canonical(ProjectAssets::findProjectRoot(), error);
    if (error) return false;
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(ProjectAssets::resolveProjectPath(asset.projectPath), error);
    if (error || !pathIsBelow(root, resolved) || !std::filesystem::is_regular_file(resolved, error) || error) return false;
    return ProjectAssetImport::computeContentHash(resolved) == asset.contentHash;
}

bool inspectManagedFile(const std::string &storedPath,
                        const std::string &expectedHash,
                        bool verifyContentHash,
                        const char *unsafeCode,
                        const char *missingCode,
                        const char *changedCode,
                        std::string &errorCode)
{
    if (!MapImportedReferencePolicy::isSafeProjectRelativePath(storedPath))
    {
        errorCode = unsafeCode;
        return false;
    }

    std::error_code error;
    const std::filesystem::path root = std::filesystem::weakly_canonical(
        ProjectAssets::findProjectRoot(), error);
    if (error)
    {
        errorCode = missingCode;
        return false;
    }
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(
        ProjectAssets::resolveProjectPath(storedPath), error);
    if (error || !pathIsBelow(root, resolved) ||
        !std::filesystem::is_regular_file(resolved, error) || error)
    {
        errorCode = missingCode;
        return false;
    }
    if (verifyContentHash && !managedFileHashMatches(resolved, expectedHash))
    {
        errorCode = changedCode;
        return false;
    }
    return true;
}

void removeFailedNewAsset(const ProjectAssetImport::AssetImportResult &asset)
{
    if (!asset.success || asset.deduplicated || !verifyManagedAsset(asset)) return;
    std::error_code ignored;
    std::filesystem::remove(ProjectAssets::resolveProjectPath(asset.projectPath), ignored);
}

TemporaryProxy::~TemporaryProxy()
{
    std::error_code ignored;
    if (!path.empty())
        std::filesystem::remove(path, ignored);
}
} // namespace MapImageImport::detail
