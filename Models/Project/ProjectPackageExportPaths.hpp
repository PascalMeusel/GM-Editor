#pragma once
#include "Models/Project/AtomicProjectFile.hpp"
#include <cctype>
#include <cstring>
#include <cwctype>
#include <fstream>

namespace ProjectPackageExportPaths
{
inline bool equalComponent(const std::filesystem::path &a, const std::filesystem::path &b)
{
#if defined(_WIN32)
    auto left = a.native(), right = b.native();
    std::transform(left.begin(), left.end(), left.begin(), ::towlower);
    std::transform(right.begin(), right.end(), right.begin(), ::towlower);
    return left == right;
#else
    return a == b;
#endif
}

inline bool inside(const std::filesystem::path &path, const std::filesystem::path &directory)
{
    const auto p = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
    const auto d = std::filesystem::weakly_canonical(std::filesystem::absolute(directory));
    auto part = p.begin();
    for (const auto &parent : d)
        if (part == p.end() || !equalComponent(*part++, parent)) return false;
    return true;
}

inline bool excludedAssetComponent(const std::filesystem::path &component)
{
    auto name = component.u8string();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (name == "recovery" || name == "backups" || name == "exports") return true;
    const auto ends = [&](const std::string &suffix) {
        return name.size() >= suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    for (const auto *suffix : {".ttrpgpkg", ".previous", ".staging"})
    {
        for (auto position = name.find(suffix); position != std::string::npos; position = name.find(suffix, position + 1))
        {
            const auto next = position + std::strlen(suffix);
            if (next == name.size() || name[next] == '.' || name[next] == '_') return true;
        }
    }
    for (const auto *suffix : {".db", ".db3", ".sqlite", ".sqlite3", "-wal", "-shm", "-journal",
                              ".bak", ".backup", ".bkp", ".old", ".orig", "~"})
        if (ends(suffix)) return true;
    return false;
}

// One inventory rule for copied files and explicit references. Inspect only
// fixed SQLite signatures, never arbitrary attachment contents or GM prose.
inline bool transferableAsset(const std::filesystem::path &path, const std::filesystem::path &assetsRoot)
{
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    const auto root = std::filesystem::absolute(assetsRoot).lexically_normal();
    auto part = absolute.begin();
    for (const auto &parent : root)
        if (part == absolute.end() || !equalComponent(*part++, parent)) return false;
    for (; part != absolute.end(); ++part)
        if (excludedAssetComponent(*part)) return false;
    if (!std::filesystem::is_regular_file(path)) return true;
    std::ifstream input(mapFileIoPath(path), std::ios::binary);
    if (!input) throw std::runtime_error("Could not inspect managed asset: " + path.u8string());
    unsigned char header[16]{};
    input.read(reinterpret_cast<char *>(header), sizeof(header));
    if (input.bad()) throw std::runtime_error("Could not inspect managed asset: " + path.u8string());
    const auto count = input.gcount();
    if (count >= 16 && std::memcmp(header, "SQLite format 3\0", 16) == 0) return false;
    if (count >= 4 && header[0] == 0x37 && header[1] == 0x7f && header[2] == 0x06 &&
        (header[3] == 0x82 || header[3] == 0x83)) return false;
    constexpr unsigned char journal[] = {0xd9, 0xd5, 0x05, 0xf9, 0x20, 0xa1, 0x63, 0xd7};
    return count < 8 || std::memcmp(header, journal, sizeof(journal)) != 0;
}

inline void rejectLinks(const std::filesystem::path &path)
{
    std::filesystem::path current;
    for (const auto &part : std::filesystem::absolute(path).lexically_normal())
    {
        current /= part;
        std::error_code error;
        const auto status = std::filesystem::symlink_status(current, error);
        if (error && error != std::errc::no_such_file_or_directory)
            throw std::filesystem::filesystem_error("Inspect export path", current, error);
        if (std::filesystem::is_symlink(status))
            throw std::runtime_error("Campaign export does not follow symbolic links.");
#if defined(_WIN32)
        const DWORD attributes = GetFileAttributesW(mapFileIoPath(current).c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("Campaign export does not follow junctions or reparse points.");
#endif
    }
}
}
