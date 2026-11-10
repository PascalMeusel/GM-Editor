#pragma once

#include "Models/Session/PlayerDisplayReviewMapPolicy.hpp"

namespace PlayerDisplayReview
{
inline void appendByte(std::uint64_t &hash, unsigned char value)
{
    hash ^= static_cast<std::uint64_t>(value);
    hash *= 1099511628211ull;
}

inline void appendString(std::uint64_t &hash, const std::string &value)
{
    for (const unsigned char character : value)
        appendByte(hash, character);
    appendByte(hash, 0xffu);
}

inline void appendUint64(std::uint64_t &hash, std::uint64_t value)
{
    for (int shift = 0; shift < 64; shift += 8)
        appendByte(hash, static_cast<unsigned char>((value >> shift) & 0xffu));
}

inline void appendInt(std::uint64_t &hash, int value)
{
    appendUint64(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(value)));
}

inline void appendBool(std::uint64_t &hash, bool value)
{
    appendByte(hash, value ? 1u : 0u);
}

inline void appendFloat(std::uint64_t &hash, float value)
{
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    appendUint64(hash, bits);
}

inline std::string managedCampaignPathSegment(const std::string &value)
{
    std::string sanitized;
    sanitized.reserve(value.size());
    for (const char character : value)
    {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) || character == '_' || character == '-')
            sanitized.push_back(character);
        else if (character == ' ')
            sanitized.push_back('_');
    }
    return sanitized.empty() ? "campaign" : sanitized;
}

inline bool pathIsInside(const std::filesystem::path &candidate,
                         const std::filesystem::path &directory)
{
    auto candidatePart = candidate.begin();
    for (auto directoryPart = directory.begin(); directoryPart != directory.end();
         ++directoryPart, ++candidatePart)
    {
        if (candidatePart == candidate.end() || *candidatePart != *directoryPart)
            return false;
    }
    return true;
}

inline std::string handoutContentHash(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};
    std::uint64_t hash = 14695981039346656037ull;
    char buffer[4096];
    while (file)
    {
        file.read(buffer, sizeof(buffer));
        const std::streamsize count = file.gcount();
        for (std::streamsize index = 0; index < count; ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= 1099511628211ull;
        }
    }
    if (file.bad())
        return {};
    std::ostringstream encoded;
    encoded << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return encoded.str();
}

// Bind a selected handout to a regular file inside this campaign's managed
// Assets/Handouts directory. Canonical containment rejects traversal and
// symlink escapes. Re-hashing actual bytes is intentional: a same-size or
// timestamp-preserving replacement must still invalidate publication.
inline bool bindManagedHandout(const Campaign &campaign,
                               const HandoutModel &source,
                               PlayerSafeHandout &safe,
                               bool verifyBytes = true)
{
    safe.assetId = source.metadata.id;
    safe.title = source.title;
    safe.body = source.body;
    safe.projectRelativePath.clear();
    safe.contentHash.clear();
    if (source.filePath.empty())
        return true;
    if (!MapImportedReferencePolicy::isSafeProjectRelativePath(source.filePath))
        return false;

    std::error_code error;
    const std::filesystem::path writableRoot =
        std::filesystem::weakly_canonical(ApplicationPaths::writableRoot(), error);
    if (error || writableRoot.empty())
        return false;
    const std::filesystem::path managedDirectory = std::filesystem::weakly_canonical(
        writableRoot / "Resources" / "Campaigns" /
            managedCampaignPathSegment(campaign.metadata.id) / "Assets" / "Handouts",
        error);
    if (error)
        return false;
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(
        writableRoot / std::filesystem::path(source.filePath), error);
    if (error || !pathIsInside(resolved, managedDirectory) ||
        !std::filesystem::is_regular_file(resolved, error) || error)
        return false;

    std::string contentHash;
    if (verifyBytes)
        contentHash = handoutContentHash(resolved);
    else
    {
        const auto size = std::filesystem::file_size(resolved, error);
        if (error) return false;
        const auto modified = std::filesystem::last_write_time(resolved, error);
        if (error) return false;
        // Runtime cache invalidation only. Publication renders frozen bytes;
        // a background verifier detects replacements preserving this metadata.
        contentHash = resolved.u8string() + ":" + std::to_string(size) + ":" +
                      std::to_string(modified.time_since_epoch().count());
    }
    if (contentHash.empty())
        return false;
    safe.projectRelativePath =
        std::filesystem::path(source.filePath).lexically_normal().generic_string();
    safe.contentHash = contentHash;
    return true;
}

} // namespace PlayerDisplayReview
