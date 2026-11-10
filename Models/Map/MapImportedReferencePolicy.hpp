#pragma once

#include "Models/Map/MapModels.hpp"

#include <filesystem>
#include <string>

namespace MapImportedReferencePolicy
{
// Managed map references are never allowed to escape the project through a
// persisted absolute path or a parent-directory segment. Import/package code
// already writes safe paths; this second boundary also protects renderers from
// damaged or manually edited campaign data.
inline bool isSafeProjectRelativePath(const std::string &storedPath)
{
    if (storedPath.empty() || storedPath.find('\0') != std::string::npos)
        return false;
    const std::filesystem::path path(storedPath);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
        return false;
    for (const std::filesystem::path &part : path)
        if (part == "." || part == "..")
            return false;
    return true;
}

inline bool hasSafeManagedPaths(const MapImportedReference &reference)
{
    if (!reference.enabled || !isSafeProjectRelativePath(reference.projectRelativePath))
        return false;
    return !reference.workingProxyGenerated ||
           isSafeProjectRelativePath(reference.workingProxyProjectRelativePath);
}
}
