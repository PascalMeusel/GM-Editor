#pragma once

#include "Models/Map/MapStyleTypes.hpp"

inline std::string mapStyleReadTextFile(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file)
        return {};
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

inline std::filesystem::path mapStyleExecutableDirectory()
{
#if defined(_WIN32)
    std::wstring buffer(4096, L'\0');
    for (;;)
    {
        const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (size == 0)
            return {};
        if (static_cast<std::size_t>(size) < buffer.size())
        {
            buffer.resize(size);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    std::vector<char> buffer(4096);
    for (;;)
    {
        const ssize_t size = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (size < 0)
            return {};
        if (static_cast<std::size_t>(size) < buffer.size())
            return std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(size))).parent_path();
        buffer.resize(buffer.size() * 2);
    }
#endif
}

inline void mapStyleAppendUniqueSearchRoot(
    std::vector<std::filesystem::path> &roots,
    const std::filesystem::path &root)
{
    if (root.empty())
        return;

    const std::filesystem::path normalized = root.lexically_normal();
    for (const std::filesystem::path &existing : roots)
    {
        if (existing == normalized)
            return;
    }
    roots.push_back(normalized);
}

inline void mapStyleAppendSearchRootAndParents(
    std::vector<std::filesystem::path> &roots,
    std::filesystem::path cursor)
{
    if (cursor.empty())
        return;

    for (;;)
    {
        mapStyleAppendUniqueSearchRoot(roots, cursor);
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor)
            break;
        cursor = cursor.parent_path();
    }
}

inline const std::vector<std::filesystem::path> &mapStyleResourceSearchRoots()
{
    static const std::vector<std::filesystem::path> roots = [] {
        std::vector<std::filesystem::path> collectedRoots;
        try
        {
            mapStyleAppendSearchRootAndParents(collectedRoots, std::filesystem::current_path());
        }
        catch (...)
        {
        }
        mapStyleAppendSearchRootAndParents(collectedRoots, mapStyleExecutableDirectory());
        return collectedRoots;
    }();
    return roots;
}
