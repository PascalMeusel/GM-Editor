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
DecodedPackage decode(const std::filesystem::path &path, const MapPackageLimits &limits)
{
    DecodedPackage decoded;
    std::error_code ec;
    if (path.u8string().size() > limits.maximumPathBytes)
    {
        decoded.error = "Package file path exceeds the safety limit.";
        return decoded;
    }
    const auto ioPath = mapFileIoPath(path);
    if (!std::filesystem::is_regular_file(ioPath, ec) || ec)
    {
        decoded.error = "Package is not a readable regular file.";
        return decoded;
    }
    decoded.fileBytes = std::filesystem::file_size(ioPath, ec);
    if (ec || decoded.fileBytes > limits.maximumPackageBytes || decoded.fileBytes > std::numeric_limits<std::size_t>::max())
    {
        decoded.error = "Package exceeds the file-size safety limit.";
        return decoded;
    }
    if (decoded.fileBytes < HeaderSize)
    {
        decoded.error = "Package is truncated.";
        return decoded;
    }
    std::vector<std::uint8_t> file(static_cast<std::size_t>(decoded.fileBytes));
    std::ifstream stream(ioPath, std::ios::binary);
    if (!stream || !stream.read(reinterpret_cast<char *>(file.data()), static_cast<std::streamsize>(file.size())))
    {
        decoded.error = "Could not read package file.";
        return decoded;
    }
    if (!std::equal(Magic.begin(), Magic.end(), file.begin()))
    {
        decoded.error = "File is not a TTRPG map package.";
        return decoded;
    }
    Reader header(file.data() + Magic.size(), file.size() - Magic.size(), limits);
    decoded.version = header.u32();
    const std::uint64_t payloadSize = header.u64();
    const std::uint64_t expectedHash = header.u64();
    if (!header.good() || decoded.version == 0 || decoded.version > MapPackageService::CurrentFormatVersion)
    {
        decoded.error = header.good() ? "Map package format version is not supported." : header.error();
        return decoded;
    }
    if (payloadSize != file.size() - HeaderSize)
    {
        decoded.error = "Package payload length is invalid.";
        return decoded;
    }
    const std::uint8_t *payload = file.data() + HeaderSize;
    if (fnv1a(payload, static_cast<std::size_t>(payloadSize)) != expectedHash)
    {
        decoded.error = "Package integrity check failed.";
        return decoded;
    }
    Reader reader(payload, static_cast<std::size_t>(payloadSize), limits);
    readMap(reader, decoded.map, decoded.version);
    readAssets(reader, decoded.assets, limits, decoded.version);
    if (!reader.good()) decoded.error = reader.error();
    else if (!reader.atEnd()) decoded.error = "Package contains unexpected trailing data.";
    else if (!reasonableMap(decoded.map, limits, decoded.error)) {}
    else if (!validateInternalAssetReferences(decoded.map, decoded.assets, decoded.error)) {}
    return decoded;
}

void replaceId(std::string &id, const std::unordered_map<std::string, std::string> &mapping)
{
    const auto found = mapping.find(id);
    if (found != mapping.end()) id = found->second;
}

void remapOwnedIds(MapModel &map, std::vector<std::pair<std::string, std::string>> &reported,
                   bool standalone, bool remap)
{
    const std::string oldMapId = map.metadata.id;
    std::unordered_map<std::string, std::string> mapping;
    const auto add = [&mapping, &reported](const std::string &oldId, const std::string &prefix) {
        if (oldId.empty() || mapping.find(oldId) != mapping.end()) return;
        const std::string newId = createEntityId(prefix);
        mapping.emplace(oldId, newId); reported.emplace_back(oldId, newId);
    };
    if (remap)
    {
        add(map.metadata.id, "map");
        for (const auto &item : map.layers) add(item.metadata.id, "map_layer");
        for (const auto &item : map.tokens) add(item.metadata.id, "token");
        for (const auto &item : map.regions) add(item.metadata.id, "map_region");
        for (const auto &item : map.features) add(item.metadata.id, "map_feature");
        for (const auto &item : map.routes) add(item.metadata.id, "map_route");
        for (const auto &item : map.generation.graph.features) add(item.id, "map_generated_feature");
        for (const auto &item : map.generation.recipe.constraints) add(item.id, "map_constraint");
        for (const auto &item : map.generation.recipe.facts) add(item.id, "map_fact");
        add(map.importedReference.assetId, "asset");
        add(map.importedReference.workingProxyAssetId, "asset_proxy");
        replaceId(map.metadata.id, mapping);
        for (auto &item : map.layers) replaceId(item.metadata.id, mapping);
        for (auto &item : map.tokens) replaceId(item.metadata.id, mapping);
        for (auto &item : map.regions) replaceId(item.metadata.id, mapping);
        for (auto &item : map.features) replaceId(item.metadata.id, mapping);
        for (auto &item : map.routes) replaceId(item.metadata.id, mapping);
        for (auto &item : map.generation.graph.features) replaceId(item.id, mapping);
        for (auto &item : map.generation.recipe.constraints) replaceId(item.id, mapping);
        for (auto &item : map.generation.recipe.facts) replaceId(item.id, mapping);
        replaceId(map.importedReference.assetId, mapping);
        replaceId(map.importedReference.workingProxyAssetId, mapping);
        for (auto &item : map.tokens)
        {
            replaceId(item.linkedEntityId, mapping); replaceId(item.layerId, mapping); replaceId(item.assetId, mapping);
        }
        for (auto &item : map.features)
        {
            replaceId(item.linkedEntityId, mapping); replaceId(item.targetMapId, mapping);
        }
        for (auto &item : map.routes)
        {
            replaceId(item.fromMapId, mapping); replaceId(item.toMapId, mapping);
        }
        for (std::string &id : map.generation.recipe.lockedFeatureIds) replaceId(id, mapping);
        for (auto &fact : map.generation.recipe.facts)
            for (std::string &id : fact.entityRefs) replaceId(id, mapping);
        for (auto &item : map.generation.graph.features)
        {
            replaceId(item.lineage.sourceMapId, mapping); replaceId(item.lineage.sourceFeatureId, mapping);
        }
    }
    if (standalone)
    {
        // A standalone map package contains no campaign city/nation records.
        // Preserve the named requirements, detach those external identities.
        for (auto &fact : map.generation.recipe.facts)
            if (fact.sourceText == "map_setup:new" || fact.sourceText == "map_setup:existing")
            { fact.entityRefs.clear(); fact.sourceText.clear(); }
        map.parentMapId.clear(); map.atlasPlacement.parentId.clear(); map.atlasPlacement.parentRevision = 0;
        map.atlasPlacement.sourceRevision = map.revision; map.atlasStatus = MapAtlasStatus::Standalone;
        std::unordered_set<std::string> ownedFeatureIds;
        for (const MapGeneratedFeature &feature : map.generation.graph.features)
            if (!feature.id.empty()) ownedFeatureIds.insert(feature.id);
        for (MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (!hasLineage(feature.lineage)) continue;
            const bool internal = feature.lineage.sourceMapId == map.metadata.id &&
                                  !feature.lineage.sourceFeatureId.empty() &&
                                  ownedFeatureIds.find(feature.lineage.sourceFeatureId) !=
                                      ownedFeatureIds.end();
            if (!internal)
            {
                // An independent copy cannot be rebased against a source map
                // that is not part of the package. Match the atlas editor's
                // explicit Detach semantics and make the copied feature local.
                feature.lineage = {};
                feature.locked = false;
            }
        }
        // Legacy self-parent references are also normalized to the copied map.
        if (!remap)
        {
            for (auto &route : map.routes)
            {
                if (route.fromMapId == oldMapId) route.fromMapId = map.metadata.id;
                if (route.toMapId == oldMapId) route.toMapId = map.metadata.id;
            }
        }
    }
}

bool samePathComponent(const std::filesystem::path &left, const std::filesystem::path &right)
{
#if defined(_WIN32)
    std::string leftText = left.u8string();
    std::string rightText = right.u8string();
    std::transform(leftText.begin(), leftText.end(), leftText.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    std::transform(rightText.begin(), rightText.end(), rightText.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return leftText == rightText;
#else
    return left == right;
#endif
}

bool pathIsBelow(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    for (; rootPart != root.end(); ++rootPart, ++candidatePart)
    {
        if (candidatePart == candidate.end() || !samePathComponent(*rootPart, *candidatePart)) return false;
    }
    return candidatePart != candidate.end();
}

bool validateMaterializationDestination(const std::filesystem::path &root,
                                        const std::filesystem::path &relative,
                                        const std::filesystem::path &destination,
                                        bool allowOverwrite,
                                        std::string &error)
{
    std::error_code ec;
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(mapFileIoPath(destination), ec);
    if (ec || !pathIsBelow(root, resolved))
    {
        error = "Embedded asset destination escapes the target directory.";
        return false;
    }

    std::filesystem::path current = root;
    auto part = relative.begin();
    while (part != relative.end())
    {
        current /= *part;
        const bool finalPart = std::next(part) == relative.end();
        const std::filesystem::file_status status = std::filesystem::symlink_status(mapFileIoPath(current), ec);
        if (ec && ec != std::errc::no_such_file_or_directory)
        {
            error = "Could not inspect an embedded asset destination.";
            return false;
        }
        if (!ec && std::filesystem::exists(status))
        {
            if (std::filesystem::is_symlink(status))
            {
                error = "Embedded assets may not traverse symbolic links.";
                return false;
            }
            if (!finalPart && !std::filesystem::is_directory(status))
            {
                error = "An embedded asset parent path is not a directory.";
                return false;
            }
            if (finalPart && (std::filesystem::is_directory(status) || !allowOverwrite))
            {
                error = std::filesystem::is_directory(status)
                            ? "An embedded asset destination is a directory."
                            : "An embedded asset destination already exists; overwrite was not allowed.";
                return false;
            }
        }
        ec.clear();
        ++part;
    }
    return true;
}
}
