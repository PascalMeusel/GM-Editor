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

namespace MapPackageServiceInternal
{
std::uint64_t fnv1a(const std::uint8_t *data, std::size_t size)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= data[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string contentHash(const std::vector<std::uint8_t> &bytes)
{
    static constexpr char Digits[] = "0123456789abcdef";
    const std::uint64_t hash = fnv1a(bytes.data(), bytes.size());
    std::string result = "fnv1a64:";
    for (int shift = 60; shift >= 0; shift -= 4)
        result.push_back(Digits[(hash >> shift) & 0x0f]);
    return result;
}

bool safePackagePath(const std::string &value, const MapPackageLimits &limits)
{
    if (value.empty() || value.size() > limits.maximumPathBytes || value.front() == '/' || value.front() == '\\')
        return false;
    if (value.find(':') != std::string::npos || value.find('\\') != std::string::npos || value.find('\0') != std::string::npos)
        return false;
    std::filesystem::path path(value);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) return false;
    for (const auto &part : path)
        if (part == ".." || part == ".") return false;
    return true;
}

bool safeLeafName(const std::string &value, const MapPackageLimits &limits)
{
    return safePackagePath(value, limits) && std::filesystem::path(value).filename().u8string() == value;
}

std::string safeFileName(const std::filesystem::path &path, const std::string &fallback)
{
    std::string name = path.filename().u8string();
    if (name.empty()) name = fallback;
    for (char &character : name)
    {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte < 32 || character == '/' || character == '\\' || character == ':') character = '_';
    }
    if (name == "." || name == "..") name = fallback;
    return name;
}

bool readAssetFile(const std::filesystem::path &path, const MapPackageLimits &limits,
                   std::vector<std::uint8_t> &bytes, std::string &error)
{
    std::error_code ec;
    const auto ioPath = mapFileIoPath(path);
    if (!std::filesystem::is_regular_file(ioPath, ec) || ec)
    {
        error = "Asset is not a readable regular file: " + path.u8string();
        return false;
    }
    const std::uint64_t size = std::filesystem::file_size(ioPath, ec);
    if (ec || size > limits.maximumAssetBytes || size > std::numeric_limits<std::size_t>::max())
    {
        error = "Asset exceeds the package safety limit: " + path.u8string();
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    std::ifstream stream(ioPath, std::ios::binary);
    if (!stream || (size != 0 && !stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))))
    {
        error = "Could not read asset: " + path.u8string();
        return false;
    }
    return true;
}

std::filesystem::path resolveAssetSourcePath(const std::filesystem::path &path)
{
    if (path.empty() || path.is_absolute()) return path;
    std::error_code error;
    for (const std::filesystem::path &root : ApplicationPaths::runtimeSearchRoots())
    {
        const std::filesystem::path candidate = root / path;
        if (std::filesystem::is_regular_file(mapFileIoPath(candidate), error) && !error)
            return candidate;
        error.clear();
    }
    return path;
}

bool reasonableMap(const MapModel &map, const MapPackageLimits &limits, std::string &error)
{
    if (map.width <= 0 || map.height <= 0 || map.width > 32768 || map.height > 32768)
    {
        error = "Map dimensions are outside the package safety limits.";
        return false;
    }
    if (map.backgroundImagePath.size() > limits.maximumPathBytes ||
        map.importedReference.projectRelativePath.size() > limits.maximumPathBytes ||
        map.importedReference.workingProxyProjectRelativePath.size() > limits.maximumPathBytes ||
        map.importedReference.sourceFileName.size() > limits.maximumPathBytes)
    {
        error = "Map contains an asset path outside the package safety limits.";
        return false;
    }
    const std::uint64_t sourcePixels = static_cast<std::uint64_t>(std::max(0, map.importedReference.sourceWidth)) *
                                       static_cast<std::uint64_t>(std::max(0, map.importedReference.sourceHeight));
    if (map.importedReference.sourceWidth < 0 || map.importedReference.sourceHeight < 0 ||
        map.importedReference.sourceWidth > 16384 || map.importedReference.sourceHeight > 16384 ||
        sourcePixels > 100000000ull ||
        map.importedReference.workingProxyWidth < 0 || map.importedReference.workingProxyHeight < 0 ||
        map.importedReference.exifOrientation < 1 || map.importedReference.exifOrientation > 8 ||
        (map.importedReference.workingProxyGenerated &&
         (map.importedReference.workingProxyAssetId.empty() ||
          map.importedReference.workingProxyProjectRelativePath.empty() ||
          map.importedReference.workingProxyContentHash.empty() ||
          map.importedReference.workingProxyWidth <= 0 || map.importedReference.workingProxyHeight <= 0 ||
          map.importedReference.workingProxyWidth > 8192 || map.importedReference.workingProxyHeight > 8192)))
    {
        error = "Map contains invalid imported-image metadata.";
        return false;
    }
    const auto within = [&limits](std::size_t count) { return count <= limits.maximumCollectionItems; };
    const auto metadataWithin = [&within](const EntityMetadata &metadata) {
        return within(metadata.tags.size()) && within(metadata.customProperties.size());
    };
    if (!within(map.cells.size()) || !within(map.layers.size()) || !within(map.tokens.size()) ||
        !within(map.regions.size()) || !within(map.features.size()) || !within(map.routes.size()) ||
        !within(map.generation.graph.features.size()) || !within(map.generation.recipe.knownFacts.size()) ||
        !within(map.generation.recipe.requiredElements.size()) || !within(map.generation.recipe.optionalElements.size()) ||
        !within(map.generation.recipe.excludedElements.size()) || !within(map.generation.recipe.lockedFeatureIds.size()) ||
        !within(map.generation.recipe.constraints.size()) || !within(map.generation.recipe.facts.size()))
    {
        error = "Map contains a collection outside the package safety limits.";
        return false;
    }
    if (!metadataWithin(map.metadata))
    {
        error = "Map metadata exceeds the package safety limits.";
        return false;
    }
    for (const MapLayerModel &item : map.layers)
        if (!metadataWithin(item.metadata)) { error = "Layer metadata exceeds the package safety limits."; return false; }
    for (const MapTokenModel &item : map.tokens)
        if (!metadataWithin(item.metadata)) { error = "Token metadata exceeds the package safety limits."; return false; }
    for (const MapRegionModel &item : map.regions)
        if (!metadataWithin(item.metadata)) { error = "Region metadata exceeds the package safety limits."; return false; }
    for (const MapFeatureModel &item : map.features)
        if (!metadataWithin(item.metadata)) { error = "Feature metadata exceeds the package safety limits."; return false; }
    for (const MapRouteModel &item : map.routes)
        if (!metadataWithin(item.metadata)) { error = "Route metadata exceeds the package safety limits."; return false; }
    for (const MapGenerationFact &fact : map.generation.recipe.facts)
        if (!within(fact.entityRefs.size()))
        {
            error = "Map contains a fact outside the package safety limits.";
            return false;
        }
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (!within(feature.points.size()))
        {
            error = "Map contains a generated feature outside the package safety limits.";
            return false;
        }
    return true;
}

void writeAsset(Writer &writer, const MapPackageAsset &asset)
{
    writeEnum(writer, asset.kind); writer.string(asset.packagePath); writer.string(asset.sourceFileName);
    writer.string(asset.contentHash); writer.bytes(asset.bytes);
}

void readAssets(Reader &reader, std::vector<MapPackageAsset> &assets, const MapPackageLimits &limits,
                std::uint32_t formatVersion)
{
    assets.clear();
    std::unordered_set<std::string> paths;
    std::unordered_set<std::uint32_t> kinds;
    for (std::uint32_t count = reader.count(); count && reader.good(); --count)
    {
        const std::uint32_t maximumKind = formatVersion >= 2 ? 2u : 1u;
        MapPackageAsset asset; asset.kind = reader.enumeration<MapPackageAssetKind>(maximumKind);
        asset.packagePath = reader.string(limits.maximumPathBytes);
        asset.sourceFileName = reader.string(limits.maximumPathBytes); asset.contentHash = reader.string(128);
        asset.bytes = reader.bytes();
        if (!reader.good()) break;
        if (!safePackagePath(asset.packagePath, limits) || !safeLeafName(asset.sourceFileName, limits))
        {
            reader.fail("Package contains an unsafe embedded asset path.");
            break;
        }
        if (!paths.insert(asset.packagePath).second || !kinds.insert(static_cast<std::uint32_t>(asset.kind)).second)
        {
            reader.fail("Package contains a duplicate embedded asset entry.");
            break;
        }
        if (asset.contentHash != contentHash(asset.bytes))
        {
            reader.fail("Embedded asset content hash does not match its bytes.");
            break;
        }
        assets.push_back(std::move(asset));
    }
}

bool validateInternalAssetReferences(const MapModel &map,
                                     const std::vector<MapPackageAsset> &assets,
                                     std::string &error)
{
    const auto isInternal = [](const std::string &path) {
        return path.rfind("assets/background/", 0) == 0 ||
               path.rfind("assets/imported/", 0) == 0 ||
               path.rfind("assets/working-proxy/", 0) == 0;
    };
    const auto matches = [&assets](MapPackageAssetKind kind, const std::string &path, const std::string &hash) {
        return std::any_of(assets.begin(), assets.end(), [&](const MapPackageAsset &asset) {
            return asset.kind == kind && asset.packagePath == path && (hash.empty() || asset.contentHash == hash);
        });
    };
    if (isInternal(map.importedReference.projectRelativePath) &&
        !matches(MapPackageAssetKind::ImportedReference, map.importedReference.projectRelativePath,
                 map.importedReference.contentHash))
    {
        error = "Package map references a missing or mismatched original image asset.";
        return false;
    }
    if (isInternal(map.importedReference.workingProxyProjectRelativePath) &&
        !matches(MapPackageAssetKind::WorkingProxy, map.importedReference.workingProxyProjectRelativePath,
                 map.importedReference.workingProxyContentHash))
    {
        error = "Package map references a missing or mismatched working proxy asset.";
        return false;
    }
    if (isInternal(map.backgroundImagePath) &&
        std::none_of(assets.begin(), assets.end(), [&](const MapPackageAsset &asset) {
            return asset.packagePath == map.backgroundImagePath;
        }))
    {
        error = "Package map references a missing background image asset.";
        return false;
    }
    return true;
}

OwnedIdentitySummary summarizeOwnedIdentities(const MapModel &map)
{
    OwnedIdentitySummary summary;
    std::unordered_set<std::string> identities;
    std::unordered_set<std::string> assetIdentities;
    std::unordered_set<std::string> scopedIdentities;
    const auto add = [&](const std::string &id, bool asset = false, const char *scope = "entity:") {
        if (id.empty()) return;
        ++summary.fieldCount;
        identities.insert(id);
        // Recipe facts and placement constraints deliberately share derivation
        // IDs with generated features (e.g. brief_city_0). Their typed namespaces
        // are separate; two actual map entities with one ID are still rejected.
        if (!scopedIdentities.insert(std::string(scope) + id).second)
            ++summary.duplicateCount;
        if (asset)
            assetIdentities.insert(id);
    };

    add(map.metadata.id);
    for (const auto &item : map.layers) add(item.metadata.id);
    for (const auto &item : map.tokens) add(item.metadata.id);
    for (const auto &item : map.regions) add(item.metadata.id);
    for (const auto &item : map.features) add(item.metadata.id);
    for (const auto &item : map.routes) add(item.metadata.id);
    for (const auto &item : map.generation.graph.features) add(item.id);
    for (const auto &item : map.generation.recipe.constraints) add(item.id, false, "constraint:");
    for (const auto &item : map.generation.recipe.facts) add(item.id, false, "fact:");
    add(map.importedReference.assetId, true);
    add(map.importedReference.workingProxyAssetId, true);

    summary.uniqueCount = identities.size();
    summary.uniqueAssetIdCount = assetIdentities.size();
    return summary;
}

bool hasLineage(const MapFeatureLineage &lineage)
{
    return !lineage.sourceMapId.empty() || !lineage.sourceFeatureId.empty() ||
           lineage.sourceRevision != 0;
}

FeatureLineageSummary summarizeFeatureLineage(const MapModel &map)
{
    FeatureLineageSummary summary;
    std::unordered_set<std::string> ownedFeatureIds;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (!feature.id.empty()) ownedFeatureIds.insert(feature.id);

    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (!hasLineage(feature.lineage)) continue;
        ++summary.total;
        const bool internal = feature.lineage.sourceMapId == map.metadata.id &&
                              !feature.lineage.sourceFeatureId.empty() &&
                              ownedFeatureIds.find(feature.lineage.sourceFeatureId) !=
                                  ownedFeatureIds.end();
        if (internal) ++summary.internal;
        else ++summary.external;
    }
    return summary;
}
}
