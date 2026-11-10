#include "Models/Map/PngExport/PngRasterAssets.hpp"
#include "Models/Map/MapImportedReferencePolicy.hpp"
#include "Models/Map/PngExport/PngTextRenderer.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Media/RgbaImageLoader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace MapPngExportRaster
{
const RasterAsset *loadRasterAsset(const MapStylePackedRuntimeAsset *asset)
{
    if (!asset || asset->atlasPath.empty())
        return nullptr;
    static thread_local std::unordered_map<std::string, std::shared_ptr<RasterAsset>> cache;
    const auto cached = cache.find(asset->atlasPath);
    if (cached != cache.end())
        return !cached->second || cached->second->pixels.empty() ? nullptr : cached->second.get();
    LoadedRgbaImage loaded;
    RasterAsset raster;
    if (loadRgbaImage(asset->atlasPath, loaded) && loaded.width > 0 && loaded.height > 0)
    {
        raster.width = loaded.width;
        raster.height = loaded.height;
        raster.pixels.resize(static_cast<std::size_t>(loaded.width * loaded.height));
        for (int index = 0; index < loaded.width * loaded.height; ++index)
            raster.pixels[static_cast<std::size_t>(index)] = {loaded.pixels[index * 4], loaded.pixels[index * 4 + 1], loaded.pixels[index * 4 + 2], loaded.pixels[index * 4 + 3]};
    }
    std::shared_ptr<RasterAsset> stored = std::make_shared<RasterAsset>(std::move(raster));
    cache.emplace(asset->atlasPath, stored);
    return stored->pixels.empty() ? nullptr : stored.get();
}

std::filesystem::path resolveBackgroundPath(const std::string &storedPath)
{
    const std::filesystem::path path(storedPath);
    if (path.empty())
        return {};
    std::error_code error;
    if (path.is_absolute() && std::filesystem::is_regular_file(path, error))
        return path;
    std::vector<std::filesystem::path> roots;
    roots.push_back(std::filesystem::current_path(error));
    roots.push_back(pngExporterExecutableDirectory());
    const std::size_t initialCount = roots.size();
    for (std::size_t index = 0; index < initialCount; ++index)
    {
        std::filesystem::path current = roots[index];
        for (int depth = 0; depth < 8 && !current.empty(); ++depth)
        {
            roots.push_back(current);
            const std::filesystem::path parent = current.parent_path();
            if (parent == current)
                break;
            current = parent;
        }
    }
    for (const std::filesystem::path &root : roots)
    {
        const std::filesystem::path candidate = (root / path).lexically_normal();
        if (std::filesystem::is_regular_file(candidate, error))
            return candidate;
    }
    return path;
}

bool pngPathIsBelow(const std::filesystem::path &root,
                    const std::filesystem::path &candidate)
{
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    for (; rootPart != root.end(); ++rootPart, ++candidatePart)
    {
        if (candidatePart == candidate.end())
            return false;
#if defined(_WIN32)
        std::string left = rootPart->u8string();
        std::string right = candidatePart->u8string();
        std::transform(left.begin(), left.end(), left.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        std::transform(right.begin(), right.end(), right.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        if (left != right)
            return false;
#else
        if (*rootPart != *candidatePart)
            return false;
#endif
    }
    return candidatePart != candidate.end();
}

std::filesystem::path resolveManagedBackgroundPath(const std::string &storedPath)
{
    if (!MapImportedReferencePolicy::isSafeProjectRelativePath(storedPath))
        return {};
    std::error_code error;
    const std::filesystem::path root = std::filesystem::weakly_canonical(
        ApplicationPaths::writableRoot(), error);
    if (error || root.empty())
        return {};
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(
        root / std::filesystem::path(storedPath), error);
    if (error || !pngPathIsBelow(root, resolved) ||
        !std::filesystem::is_regular_file(resolved, error) || error)
        return {};
    return resolved;
}

std::string pngFileContentHash(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};
    std::uint64_t hash = 14695981039346656037ull;
    char buffer[4096];
    while (file)
    {
        file.read(buffer, sizeof(buffer));
        const std::streamsize bytesRead = file.gcount();
        for (std::streamsize index = 0; index < bytesRead; ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= 1099511628211ull;
        }
    }
    std::ostringstream stream;
    stream << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

const RasterAsset *loadRasterPath(const std::filesystem::path &path,
                                  const std::string &expectedContentHash,
                                  bool *contentHashMismatch)
{
    if (contentHashMismatch)
        *contentHashMismatch = false;
    if (path.empty())
        return nullptr;
    struct CachedRasterPath
    {
        std::shared_ptr<RasterAsset> raster;
        std::filesystem::file_time_type lastWriteTime{};
        std::uintmax_t byteSize = 0;
        std::string boundContentHash;
    };
    static thread_local std::unordered_map<std::string, CachedRasterPath> cache;
    const std::string key = path.lexically_normal().generic_string();
    std::error_code fileError;
    if (!std::filesystem::is_regular_file(path, fileError) || fileError)
    {
        cache.erase(key);
        return nullptr;
    }
    const std::filesystem::file_time_type lastWriteTime =
        std::filesystem::last_write_time(path, fileError);
    if (fileError)
    {
        cache.erase(key);
        return nullptr;
    }
    const std::uintmax_t byteSize = std::filesystem::file_size(path, fileError);
    if (fileError)
    {
        cache.erase(key);
        return nullptr;
    }
    // Export is a trust boundary rather than a frame loop. Verify the actual
    // bytes before accepting a metadata cache hit: files can be replaced with
    // the same size while preserving/coalescing their modification timestamp.
    if (!expectedContentHash.empty() &&
        pngFileContentHash(path) != expectedContentHash)
    {
        cache.erase(key);
        if (contentHashMismatch)
            *contentHashMismatch = true;
        return nullptr;
    }
    const auto cached = cache.find(key);
    if (cached != cache.end() && cached->second.lastWriteTime == lastWriteTime &&
        cached->second.byteSize == byteSize &&
        cached->second.boundContentHash == expectedContentHash)
        return !cached->second.raster || cached->second.raster->pixels.empty()
                   ? nullptr
                   : cached->second.raster.get();
    LoadedRgbaImage loaded;
    RasterAsset raster;
    RgbaImageLoadOptions loadOptions;
    loadOptions.maximumSourceEdge = 16384;
    loadOptions.maximumSourcePixels = 100000000ull;
    if (loadRgbaImage(path, loaded, loadOptions) && loaded.width > 0 && loaded.height > 0)
    {
        raster.width = loaded.width;
        raster.height = loaded.height;
        raster.pixels.resize(static_cast<std::size_t>(loaded.width * loaded.height));
        for (int index = 0; index < loaded.width * loaded.height; ++index)
            raster.pixels[static_cast<std::size_t>(index)] = {
                loaded.pixels[index * 4], loaded.pixels[index * 4 + 1],
                loaded.pixels[index * 4 + 2], loaded.pixels[index * 4 + 3]};
    }
    std::shared_ptr<RasterAsset> stored = std::make_shared<RasterAsset>(std::move(raster));
    cache[key] = {stored, lastWriteTime, byteSize, expectedContentHash};
    return stored->pixels.empty() ? nullptr : stored.get();
}

bool drawBackgroundReference(std::vector<Pixel> &pixels, int width, int height,
                              const std::filesystem::path &resolvedPath,
                              const MapImageImport::NormalizedSamplingTransform &transform,
                              std::uint8_t opacity,
                              const std::string &expectedContentHash,
                              bool *contentHashMismatch)
{
    const RasterAsset *source = loadRasterPath(resolvedPath, expectedContentHash,
                                               contentHashMismatch);
    if (!source || width <= 0 || height <= 0)
        return false;
    if (!transform.valid)
        return false;
    for (int y = 0; y < height; ++y)
    {
        const float normalizedY = (static_cast<float>(y) + 0.5f) /
                                  static_cast<float>(height);
        for (int x = 0; x < width; ++x)
        {
            const float normalizedX = (static_cast<float>(x) + 0.5f) /
                                      static_cast<float>(width);
            float sourceU = 0.0f;
            float sourceV = 0.0f;
            if (!MapImageImport::sampleNormalizedImage(
                    transform, normalizedX, normalizedY, sourceU, sourceV))
                continue;
            const int sourceX = std::clamp(static_cast<int>(sourceU * source->width), 0, source->width - 1);
            const int sourceY = std::clamp(static_cast<int>(sourceV * source->height), 0, source->height - 1);
            Pixel &destination = pixels[static_cast<std::size_t>(y * width + x)];
            destination = alphaBlend(destination,
                                     source->pixels[static_cast<std::size_t>(sourceY * source->width + sourceX)],
                                     opacity);
        }
    }
    return true;
}

void drawRasterAsset(std::vector<Pixel> &pixels, int width, int height,
                     int left, int top, int right, int bottom,
                     const MapStylePackedRuntimeAsset *asset, std::uint8_t opacity,
                     Pixel tint)
{
    const RasterAsset *raster = loadRasterAsset(asset);
    if (!raster || right <= left || bottom <= top)
        return;
    const int clippedLeft = std::max(0, left);
    const int clippedTop = std::max(0, top);
    const int clippedRight = std::min(width, right);
    const int clippedBottom = std::min(height, bottom);
    for (int y = clippedTop; y < clippedBottom; ++y)
    {
        const int sourceY = std::clamp((y - top) * raster->height / std::max(1, bottom - top), 0, raster->height - 1);
        for (int x = clippedLeft; x < clippedRight; ++x)
        {
            const int sourceX = std::clamp((x - left) * raster->width / std::max(1, right - left), 0, raster->width - 1);
            Pixel source = raster->pixels[static_cast<std::size_t>(sourceY * raster->width + sourceX)];
            source.r = static_cast<std::uint8_t>(static_cast<unsigned int>(source.r) * tint.r / 255u);
            source.g = static_cast<std::uint8_t>(static_cast<unsigned int>(source.g) * tint.g / 255u);
            source.b = static_cast<std::uint8_t>(static_cast<unsigned int>(source.b) * tint.b / 255u);
            source.a = static_cast<std::uint8_t>(static_cast<unsigned int>(source.a) * tint.a / 255u);
            Pixel &destination = pixels[static_cast<std::size_t>(y * width + x)];
            destination = alphaBlend(destination, source, opacity);
        }
    }
}

void drawRasterAssetPreservingAspect(std::vector<Pixel> &pixels, int width, int height,
                                     int centerX, int centerY, int nominalExtent,
                                     const MapStylePackedRuntimeAsset *asset,
                                     std::uint8_t opacity)
{
    if (!asset || nominalExtent <= 0)
        return;
    float aspect = 1.0f;
    if (asset->width > 0 && asset->height > 0)
        aspect = static_cast<float>(asset->width) / static_cast<float>(asset->height);
    else if (asset->footprintWidth > 0 && asset->footprintHeight > 0)
        aspect = static_cast<float>(asset->footprintWidth) / static_cast<float>(asset->footprintHeight);
    aspect = std::clamp(aspect, 0.5f, 2.5f);
    const float ratioRoot = std::sqrt(aspect);
    const int targetWidth = std::max(1, static_cast<int>(std::round(nominalExtent * ratioRoot)));
    const int targetHeight = std::max(1, static_cast<int>(std::round(nominalExtent / ratioRoot)));
    drawRasterAsset(pixels, width, height,
                    centerX - targetWidth / 2, centerY - targetHeight / 2,
                    centerX + (targetWidth + 1) / 2, centerY + (targetHeight + 1) / 2,
                    asset, opacity);
}

void drawRotatedRasterAsset(std::vector<Pixel> &pixels, int width, int height,
                            float centerX, float centerY, float targetLength, float targetWidth,
                            float angleRadians, const MapStylePackedRuntimeAsset *asset,
                            std::uint8_t opacity,
                            Pixel tint)
{
    const RasterAsset *raster = loadRasterAsset(asset);
    if (!raster || targetLength <= 0.0f || targetWidth <= 0.0f)
        return;
    const float cosine = std::cos(angleRadians);
    const float sine = std::sin(angleRadians);
    const float halfLength = targetLength * 0.5f;
    const float halfWidth = targetWidth * 0.5f;
    const float boundsX = std::abs(cosine) * halfLength + std::abs(sine) * halfWidth;
    const float boundsY = std::abs(sine) * halfLength + std::abs(cosine) * halfWidth;
    const int left = std::max(0, static_cast<int>(std::floor(centerX - boundsX)));
    const int right = std::min(width - 1, static_cast<int>(std::ceil(centerX + boundsX)));
    const int top = std::max(0, static_cast<int>(std::floor(centerY - boundsY)));
    const int bottom = std::min(height - 1, static_cast<int>(std::ceil(centerY + boundsY)));
    for (int y = top; y <= bottom; ++y)
    {
        for (int x = left; x <= right; ++x)
        {
            const float deltaX = static_cast<float>(x) + 0.5f - centerX;
            const float deltaY = static_cast<float>(y) + 0.5f - centerY;
            const float localX = cosine * deltaX + sine * deltaY;
            const float localY = -sine * deltaX + cosine * deltaY;
            if (std::abs(localX) > halfLength || std::abs(localY) > halfWidth)
                continue;
            const float u = localX / targetLength + 0.5f;
            const float v = localY / targetWidth + 0.5f;
            const int sourceX = std::clamp(static_cast<int>(u * raster->width), 0, raster->width - 1);
            const int sourceY = std::clamp(static_cast<int>(v * raster->height), 0, raster->height - 1);
            Pixel source = raster->pixels[static_cast<std::size_t>(sourceY * raster->width + sourceX)];
            source.r = static_cast<std::uint8_t>(static_cast<unsigned int>(source.r) * tint.r / 255u);
            source.g = static_cast<std::uint8_t>(static_cast<unsigned int>(source.g) * tint.g / 255u);
            source.b = static_cast<std::uint8_t>(static_cast<unsigned int>(source.b) * tint.b / 255u);
            source.a = static_cast<std::uint8_t>(static_cast<unsigned int>(source.a) * tint.a / 255u);
            Pixel &destination = pixels[static_cast<std::size_t>(y * width + x)];
            destination = alphaBlend(destination, source, opacity);
        }
    }
}

int stableMapVariant(int x, int y, int salt, int count)
{
    if (count <= 1)
        return 0;
    std::uint32_t value = static_cast<std::uint32_t>(x) * 0x9e3779b9u ^ static_cast<std::uint32_t>(y) * 0x85ebca6bu ^ static_cast<std::uint32_t>(salt);
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    return static_cast<int>(value % static_cast<std::uint32_t>(count));
}

bool isVisualStampAnchor(const MapModel &map, int x, int y, MapTileKind kind, int radius, int salt)
{
    const MapCellModel *cell = mapCellAt(map, x, y);
    if (!cell || cell->kind != kind)
        return false;
    constexpr int scoreRange = 104729;
    const int centerScore = stableMapVariant(x, y, salt, scoreRange);
    for (int neighborY = std::max(0, y - radius); neighborY <= std::min(map.height - 1, y + radius); ++neighborY)
    {
        for (int neighborX = std::max(0, x - radius); neighborX <= std::min(map.width - 1, x + radius); ++neighborX)
        {
            if (neighborX == x && neighborY == y)
                continue;
            const MapCellModel *neighbor = mapCellAt(map, neighborX, neighborY);
            if (!neighbor || neighbor->kind != kind)
                continue;
            const int neighborScore = stableMapVariant(neighborX, neighborY, salt, scoreRange);
            if (neighborScore < centerScore ||
                (neighborScore == centerScore && (neighborY < y || (neighborY == y && neighborX < x))))
                return false;
        }
    }
    return true;
}


}
