#include "ProjectAssetImportService.hpp"

#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace
{
std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string fallbackExtension(ProjectAssetImport::AssetKind kind)
{
    switch (kind)
    {
    case ProjectAssetImport::AssetKind::Image:
    case ProjectAssetImport::AssetKind::Portrait:
    case ProjectAssetImport::AssetKind::Token:
    case ProjectAssetImport::AssetKind::Map:
        return ".image";
    case ProjectAssetImport::AssetKind::Audio:
        return ".audio";
    case ProjectAssetImport::AssetKind::Handout:
        return ".handout";
    case ProjectAssetImport::AssetKind::GenericFile:
        return ".asset";
    }
    return ".asset";
}

std::string mimeTypeForExtension(const std::filesystem::path &sourcePath)
{
    static const std::map<std::string, std::string> mimeTypes{
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".bmp", "image/bmp"},
        {".gif", "image/gif"},
        {".webp", "image/webp"},
        {".svg", "image/svg+xml"},
        {".pdf", "application/pdf"},
        {".txt", "text/plain"},
        {".md", "text/markdown"},
        {".html", "text/html"},
        {".htm", "text/html"},
        {".json", "application/json"},
        {".wav", "audio/wav"},
        {".mp3", "audio/mpeg"},
        {".ogg", "audio/ogg"},
        {".flac", "audio/flac"},
        {".aiff", "audio/aiff"},
        {".aif", "audio/aiff"},
        {".wma", "audio/x-ms-wma"},
        {".mid", "audio/midi"},
        {".midi", "audio/midi"}};

    const std::string extension = lowerCopy(sourcePath.extension().string());
    const auto found = mimeTypes.find(extension);
    return found == mimeTypes.end() ? "application/octet-stream" : found->second;
}

std::string projectRelativePath(const std::filesystem::path &path)
{
    try
    {
        return std::filesystem::relative(path, ProjectAssets::findProjectRoot()).generic_string();
    }
    catch (const std::exception &)
    {
        return path.generic_string();
    }
}

std::filesystem::path findExistingAssetWithHash(const Campaign &campaign, ProjectAssetImport::AssetKind kind, const std::string &contentHash)
{
    const std::filesystem::path directory = ProjectPackageLayout::assetDirectory(campaign, ProjectAssetImport::assetFolderName(kind));
    if (contentHash.empty() || !std::filesystem::exists(directory))
        return {};

    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file())
            continue;
        if (ProjectAssetImport::computeContentHash(entry.path()) == contentHash)
            return entry.path();
    }

    return {};
}
}

namespace ProjectAssetImport
{
const char *assetKindLabel(AssetKind kind)
{
    switch (kind)
    {
    case AssetKind::Image:
        return "Image";
    case AssetKind::Portrait:
        return "Portrait";
    case AssetKind::Token:
        return "Token";
    case AssetKind::Map:
        return "Map";
    case AssetKind::Audio:
        return "Audio";
    case AssetKind::Handout:
        return "Handout";
    case AssetKind::GenericFile:
        return "File";
    }
    return "File";
}

std::string assetFolderName(AssetKind kind)
{
    switch (kind)
    {
    case AssetKind::Image:
        return "Images";
    case AssetKind::Portrait:
        return "Portraits";
    case AssetKind::Token:
        return "Tokens";
    case AssetKind::Map:
        return "Maps";
    case AssetKind::Audio:
        return "Audio";
    case AssetKind::Handout:
        return "Handouts";
    case AssetKind::GenericFile:
        return "Files";
    }
    return "Files";
}

std::vector<std::string> supportedExtensions(AssetKind kind)
{
    switch (kind)
    {
    case AssetKind::Image:
    case AssetKind::Portrait:
    case AssetKind::Token:
    case AssetKind::Map:
        return {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".svg"};
    case AssetKind::Audio:
        return {".wav", ".mp3", ".ogg", ".flac", ".aiff", ".aif", ".wma", ".mid", ".midi"};
    case AssetKind::Handout:
        return {".pdf", ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".txt", ".md", ".html", ".htm"};
    case AssetKind::GenericFile:
        return {};
    }
    return {};
}

bool isSupportedAssetFile(AssetKind kind, const std::filesystem::path &sourcePath)
{
    if (kind == AssetKind::GenericFile)
        return true;

    const std::vector<std::string> extensions = supportedExtensions(kind);
    const std::string extension = lowerCopy(sourcePath.extension().string());
    return std::find(extensions.begin(), extensions.end(), extension) != extensions.end();
}

std::string computeContentHash(const std::filesystem::path &sourcePath)
{
    std::ifstream file(sourcePath, std::ios::binary);
    if (!file)
        return "";

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

AssetImportResult importAsset(const Campaign &campaign, const AssetImportRequest &request)
{
    AssetImportResult result;
    result.kind = request.kind;
    result.assetFolder = assetFolderName(request.kind);
    result.assetId = request.assetId.empty() ? createEntityId("asset") : request.assetId;

    if (request.sourcePath.empty() || !std::filesystem::exists(request.sourcePath) || !std::filesystem::is_regular_file(request.sourcePath))
    {
        result.errorMessage = std::string("Choose an existing ") + assetKindLabel(request.kind) + " file.";
        return result;
    }

    if (!isSupportedAssetFile(request.kind, request.sourcePath))
    {
        result.errorMessage = std::string("Unsupported ") + assetKindLabel(request.kind) + " file extension: " + request.sourcePath.extension().string();
        return result;
    }

    try
    {
        result.originalFileName = request.sourcePath.filename().string();
        result.originalSourcePath = request.sourceReference.empty() ? request.sourcePath.string() : request.sourceReference;
        result.displayName = request.displayName.empty() ? request.sourcePath.stem().string() : request.displayName;
        result.mimeType = mimeTypeForExtension(request.sourcePath);
        result.byteSize = std::filesystem::file_size(request.sourcePath);
        result.contentHash = computeContentHash(request.sourcePath);

        if (request.deduplicateByHash)
        {
            const std::filesystem::path existingAssetPath = findExistingAssetWithHash(campaign, request.kind, result.contentHash);
            if (!existingAssetPath.empty())
            {
                result.projectPath = projectRelativePath(existingAssetPath);
                result.deduplicated = true;
                result.success = true;
                return result;
            }
        }

        result.projectPath = ProjectAssets::copyFileToCampaignAssetFolder(campaign, request.sourcePath, result.assetId, result.assetFolder, fallbackExtension(request.kind));
        result.success = true;
    }
    catch (const std::exception &e)
    {
        result.errorMessage = std::string("Could not import ") + assetKindLabel(request.kind) + " file: " + e.what();
    }

    return result;
}
}
