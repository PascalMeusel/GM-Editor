#include "Models/Map/MapPackageDraftInstallService.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <system_error>
#include <type_traits>
#include <vector>
#include "Models/Map/MapPackageDraftInstallInternal.hpp"


namespace MapPackageDraftInstallDetail
{
std::string createShortTransactionId()
{
    static std::atomic<std::uint32_t> sequence{0};
    const auto ticks = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::ostringstream stream;
    stream << "mi_" << std::hex << ticks << '_' << sequence.fetch_add(1, std::memory_order_relaxed);
    return stream.str();
}

bool safeLeaf(const std::string &value)
{
    if (value.empty() || value.size() > 240 || value == "." || value == "..")
        return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char value) {
        return std::isalnum(value) != 0 || value == '_' || value == '-';
    });
}

bool safeRelative(const std::filesystem::path &path)
{
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
        return false;
    for (const auto &part : path)
    {
        if (part == "." || part == ".." || part.empty())
            return false;
    }
    return true;
}

std::string comparableComponent(std::filesystem::path component)
{
    std::string value = component.generic_u8string();
#if defined(_WIN32)
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
#endif
    return value;
}

bool pathInside(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    for (; rootPart != root.end(); ++rootPart, ++candidatePart)
    {
        if (candidatePart == candidate.end() ||
            comparableComponent(*rootPart) != comparableComponent(*candidatePart))
            return false;
    }
    return true;
}

bool validateRoots(const std::filesystem::path &projectRoot,
                   const std::filesystem::path &assetLibraryDirectory,
                   ValidatedRoots &roots,
                   std::string &error)
{
    std::error_code ec;
    if (projectRoot.empty() || assetLibraryDirectory.empty())
    {
        error = "Map package import storage is not configured.";
        return false;
    }
    const auto projectStatus = std::filesystem::symlink_status(projectRoot, ec);
    if (ec || !std::filesystem::is_directory(projectStatus) || std::filesystem::is_symlink(projectStatus))
    {
        error = "Map package project root must be an existing real directory.";
        return false;
    }
    ec.clear();
    const auto libraryStatus = std::filesystem::symlink_status(assetLibraryDirectory, ec);
    if (ec || !std::filesystem::is_directory(libraryStatus) || std::filesystem::is_symlink(libraryStatus))
    {
        error = "Map package asset library must be an existing real directory.";
        return false;
    }
    roots.project = std::filesystem::canonical(projectRoot, ec);
    if (ec)
    {
        error = "Map package project root could not be resolved.";
        return false;
    }
    roots.assetLibrary = std::filesystem::canonical(assetLibraryDirectory, ec);
    if (ec || !pathInside(roots.project, roots.assetLibrary))
    {
        error = "Map package asset library must stay inside the project.";
        return false;
    }
    return true;
}

bool writeTextFile(const std::filesystem::path &path, const std::string &text, std::string &error)
{
    std::filesystem::path temporary = path;
    temporary += std::filesystem::u8path(".tmp");
    std::error_code ec;
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream || !stream.write(text.data(), static_cast<std::streamsize>(text.size())))
        {
            error = "Map package import recovery record could not be written.";
            std::filesystem::remove(temporary, ec);
            return false;
        }
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec)
    {
        error = "Map package import recovery record could not be committed.";
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

std::string serializeRecord(const TransactionRecord &record)
{
    std::ostringstream stream;
    stream << TransactionMagic << '\n'
           << std::quoted(record.transactionId) << '\n'
           << std::quoted(record.finalDirectoryName) << '\n'
           << record.assets.size() << '\n';
    for (const TransactionAssetRecord &asset : record.assets)
    {
        stream << static_cast<unsigned int>(asset.kind) << ' '
               << std::quoted(asset.relativePath.generic_u8string()) << ' '
               << std::quoted(asset.contentHash) << ' '
               << asset.byteCount << '\n';
    }
    return stream.str();
}

bool readRecord(const std::filesystem::path &path,
                const MapPackageLimits &limits,
                TransactionRecord &record,
                std::string &error)
{
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status) ||
        std::filesystem::file_size(path, ec) > std::min<std::uint64_t>(limits.maximumPackageBytes, 1024ull * 1024ull))
    {
        error = "Map package import recovery record is missing or invalid.";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    std::string magic;
    std::getline(stream, magic);
    std::size_t assetCount = 0;
    if (!stream || magic != TransactionMagic ||
        !(stream >> std::quoted(record.transactionId)) ||
        !(stream >> std::quoted(record.finalDirectoryName)) ||
        !(stream >> assetCount) ||
        !safeLeaf(record.transactionId) || !safeLeaf(record.finalDirectoryName) ||
        assetCount > std::min<std::size_t>(limits.maximumCollectionItems, 1024u))
    {
        error = "Map package import recovery record is malformed.";
        return false;
    }
    record.assets.clear();
    record.assets.reserve(assetCount);
    for (std::size_t index = 0; index < assetCount; ++index)
    {
        unsigned int kind = 0;
        std::string pathValue;
        TransactionAssetRecord asset;
        if (!(stream >> kind >> std::quoted(pathValue) >> std::quoted(asset.contentHash) >> asset.byteCount) ||
            kind > static_cast<unsigned int>(MapPackageAssetKind::WorkingProxy) ||
            pathValue.size() > limits.maximumPathBytes || asset.contentHash.size() > 128 ||
            asset.byteCount > limits.maximumAssetBytes)
        {
            error = "Map package import recovery asset record is malformed.";
            return false;
        }
        asset.kind = static_cast<MapPackageAssetKind>(kind);
        asset.relativePath = std::filesystem::path(pathValue).lexically_normal();
        if (!safeRelative(asset.relativePath))
        {
            error = "Map package import recovery asset path is unsafe.";
            return false;
        }
        record.assets.push_back(std::move(asset));
    }
    return true;
}

std::string fileHash(const std::filesystem::path &path,
                     const MapPackageLimits &limits,
                     std::uint64_t expectedBytes,
                     std::string &error)
{
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status) ||
        std::filesystem::file_size(path, ec) != expectedBytes || ec || expectedBytes > limits.maximumAssetBytes)
    {
        error = "Recovered map package asset is missing or has the wrong size.";
        return {};
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "Recovered map package asset could not be read.";
        return {};
    }
    std::uint64_t hash = 14695981039346656037ull;
    std::vector<char> buffer(64 * 1024);
    while (stream)
    {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = stream.gcount();
        for (std::streamsize index = 0; index < count; ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]);
            hash *= 1099511628211ull;
        }
    }
    if (!stream.eof())
    {
        error = "Recovered map package asset could not be verified.";
        return {};
    }
    static constexpr char Digits[] = "0123456789abcdef";
    std::string result = "fnv1a64:";
    for (int shift = 60; shift >= 0; shift -= 4)
        result.push_back(Digits[(hash >> shift) & 0x0f]);
    return result;
}

bool hasOwnership(const std::filesystem::path &directory, const std::string &transactionId)
{
    std::ifstream stream(directory / OwnershipMarker, std::ios::binary);
    std::string owner;
    std::getline(stream, owner);
    return stream.good() || stream.eof() ? owner == transactionId : false;
}

bool removeOwnedFinalDirectory(const std::filesystem::path &finalDirectory,
                               const std::string &transactionId,
                               std::string &error)
{
    std::error_code ec;
    if (!std::filesystem::exists(finalDirectory, ec))
        return true;
    if (ec || std::filesystem::is_symlink(std::filesystem::symlink_status(finalDirectory, ec)) ||
        !std::filesystem::is_directory(finalDirectory, ec) || !hasOwnership(finalDirectory, transactionId))
    {
        error = "Map package import rollback refused an unowned asset directory.";
        return false;
    }
    std::filesystem::remove_all(finalDirectory, ec);
    if (ec)
    {
        error = "Map package import rollback could not remove staged assets.";
        return false;
    }
    return true;
}

bool shouldFail(const MapPackageDraftInstallOptions &options,
                MapPackageDraftInstallFailurePoint point)
{
    return options.failurePoint == point;
}

void patchImportedAssetPaths(MapModel &map,
                             const std::vector<MapPackageMaterializedAsset> &assets,
                             const std::filesystem::path &projectRoot,
                             const std::filesystem::path &finalDirectory,
                             TransactionRecord &record)
{
    for (const MapPackageMaterializedAsset &asset : assets)
    {
        const std::string oldPackagePath = asset.packagePath;
        const std::filesystem::path finalPath = finalDirectory / asset.projectRelativePath;
        const std::filesystem::path stored = std::filesystem::relative(finalPath, projectRoot);
        if (!safeRelative(stored))
            throw std::runtime_error("Imported map asset could not be represented as a project-relative path.");
        const std::string storedPath = stored.generic_u8string();
        if (map.backgroundImagePath == oldPackagePath)
            map.backgroundImagePath = storedPath;
        if (asset.kind == MapPackageAssetKind::Background)
            map.backgroundImagePath = storedPath;
        else if (asset.kind == MapPackageAssetKind::ImportedReference)
        {
            map.importedReference.enabled = true;
            map.importedReference.projectRelativePath = storedPath;
            map.importedReference.contentHash = asset.contentHash;
        }
        else if (asset.kind == MapPackageAssetKind::WorkingProxy)
        {
            map.importedReference.enabled = true;
            map.importedReference.workingProxyProjectRelativePath = storedPath;
            map.importedReference.workingProxyContentHash = asset.contentHash;
            map.importedReference.workingProxyGenerated = true;
        }
        record.assets.push_back({asset.kind, asset.projectRelativePath, asset.contentHash, asset.byteCount});
    }
}

std::filesystem::path transactionDirectory(const std::filesystem::path &assetLibrary,
                                           const std::string &transactionId)
{
    return assetLibrary / (std::string(TransactionPrefix) + transactionId);
}
} // namespace MapPackageDraftInstallDetail

