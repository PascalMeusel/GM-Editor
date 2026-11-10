#pragma once
#include "Models/Project/CampaignMarkdownExchange.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include "Models/Project/ProjectPackageExportPaths.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include <fstream>
#include <cctype>
#include <set>

namespace CampaignMarkdownExchange::detail
{
inline constexpr std::size_t MaxImportBytes = 4U * 1024 * 1024;
inline constexpr std::uintmax_t MaxAttachmentBytes = 64U * 1024 * 1024;
inline constexpr std::uintmax_t MaxAttachmentTotal = 256U * 1024 * 1024;
inline void require(bool condition, const char *message)
{ if (!condition) throw std::runtime_error(message); }
inline std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}
inline bool utf8(const std::string &text)
{
    std::size_t i = 0;
    while (i < text.size())
    {
        const auto first = static_cast<unsigned char>(text[i++]);
        if (first == 0) return false;
        if (first < 0x80) continue;
        unsigned int value = 0, minimum = 0; std::size_t count = 0;
        if ((first & 0xe0) == 0xc0) { value = first & 0x1f; count = 1; minimum = 0x80; }
        else if ((first & 0xf0) == 0xe0) { value = first & 0x0f; count = 2; minimum = 0x800; }
        else if ((first & 0xf8) == 0xf0) { value = first & 7; count = 3; minimum = 0x10000; }
        else return false;
        if (i + count > text.size()) return false;
        while (count--) { const auto ch = static_cast<unsigned char>(text[i++]); if ((ch & 0xc0) != 0x80) return false; value = (value << 6) | (ch & 63); }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    }
    return true;
}
inline std::string readText(const std::filesystem::path &path)
{
    ProjectPackageExportPaths::rejectLinks(path);
    require(std::filesystem::is_regular_file(path), "Select an existing regular Markdown file.");
    const auto size = std::filesystem::file_size(path);
    require(size <= MaxImportBytes, "Markdown imports are limited to 4 MiB.");
    std::ifstream input(mapFileIoPath(path), std::ios::binary);
    require(static_cast<bool>(input), "Cannot read Markdown file.");
    std::string bytes(static_cast<std::size_t>(size), '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(input.gcount() == static_cast<std::streamsize>(bytes.size()) && input.peek() == std::char_traits<char>::eof(),
        "Markdown file changed while being read; preview again.");
    require(!input.bad() && utf8(bytes), "Markdown must be UTF-8 text without NUL bytes. Convert the source explicitly first.");
    return bytes;
}
inline bool allowedAttachment(const std::filesystem::path &path)
{
    static const std::set<std::string> extensions = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".svg",
        ".wav", ".mp3", ".ogg", ".flac", ".aiff", ".aif", ".wma", ".mid", ".midi", ".pdf", ".txt", ".md"};
    return extensions.count(lower(path.extension().string())) != 0;
}
inline std::filesystem::path attachmentPath(const Campaign &c, const std::string &relative)
{
    const auto path = std::filesystem::u8path(relative);
    require(!relative.empty() && !path.is_absolute() && !path.has_root_name(), "Attachment path must be relative.");
    require(relative.find(':') == std::string::npos && relative.find('\0') == std::string::npos, "Attachment path contains an invalid stream or NUL component.");
    for (const auto &part : path) require(part != ".." && part != ".", "Attachment path cannot traverse directories.");
    const auto buckets = ProjectPackageLayout::assetFolderNames();
    require(path.begin() != path.end() && std::find(buckets.begin(), buckets.end(), path.begin()->string()) != buckets.end(),
        "Attachment is outside the managed asset buckets.");
    const auto root = ProjectPackageLayout::layoutForCampaign(c).assetsDirectory;
    const auto full = root / path;
    ProjectPackageExportPaths::rejectLinks(full);
    require(ProjectPackageExportPaths::inside(full, root) && std::filesystem::is_regular_file(full) && allowedAttachment(full),
        "Attachment must be a supported regular managed asset.");
    return full;
}
inline void inventory(const Campaign &c, ExportPreview &preview)
{
    const auto root = ProjectPackageLayout::layoutForCampaign(c).assetsDirectory;
    ProjectPackageExportPaths::rejectLinks(root);
    std::uintmax_t total = 0; std::size_t visited = 0;
    for (const auto &bucket : ProjectPackageLayout::assetFolderNames())
    {
        const auto folder = root / bucket;
        ProjectPackageExportPaths::rejectLinks(folder);
        if (!std::filesystem::exists(folder)) continue;
        for (std::filesystem::recursive_directory_iterator it(folder), end; it != end; ++it)
        {
            require(++visited <= 10000, "Managed attachment inventory exceeds 10,000 entries.");
            try { ProjectPackageExportPaths::rejectLinks(it->path()); }
            catch (const std::exception &) { it.disable_recursion_pending(); preview.warnings.push_back("Skipped linked asset: " + it->path().filename().u8string()); continue; }
            if (!it->is_regular_file()) continue;
            const auto relative = it->path().lexically_relative(root).generic_u8string();
            if (!allowedAttachment(it->path())) { preview.warnings.push_back("Unsupported attachment omitted: " + relative); continue; }
            const auto size = it->file_size();
            if (size > MaxAttachmentBytes || total + size > MaxAttachmentTotal)
            { preview.warnings.push_back("Attachment exceeds preview size budget: " + relative); continue; }
            const auto hash = ProjectAssetImport::computeContentHash(it->path());
            require(!hash.empty(), "Cannot fingerprint a managed attachment.");
            preview.attachments.push_back({relative, hash, size}); total += size;
        }
    }
    std::sort(preview.attachments.begin(), preview.attachments.end(), [](const auto &a, const auto &b) { return a.relativePath < b.relativePath; });
}
}
