#pragma once

#include "Models/Map/MapCheckpointStorage.hpp"

namespace gm::map_checkpoint::detail
{
void appendU8(std::vector<std::uint8_t> &data, std::uint8_t value) { data.push_back(value); }
void appendU32(std::vector<std::uint8_t> &data, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) appendU8(data, static_cast<std::uint8_t>((value >> shift) & 0xffu));
}
void appendU64(std::vector<std::uint8_t> &data, std::uint64_t value)
{
    for (int shift = 0; shift < 64; shift += 8) appendU8(data, static_cast<std::uint8_t>((value >> shift) & 0xffu));
}
void appendString(std::vector<std::uint8_t> &data, const std::string &value)
{
    appendU32(data, static_cast<std::uint32_t>(value.size()));
    data.insert(data.end(), value.begin(), value.end());
}

bool writeMetadataAtomic(const std::filesystem::path &path, const MapCheckpointInfo &info, std::string &error)
{
    if (info.id.size() > 128 || info.mapId.size() > 4096 || info.name.size() > 512 ||
        info.createdAtUtc.size() > 128 || info.transactionLabel.size() > 2048 ||
        info.packageFingerprint.size() > 128)
    {
        error = "Checkpoint metadata exceeds the safety limit.";
        return false;
    }
    std::vector<std::uint8_t> data(MetadataMagic.begin(), MetadataMagic.end());
    appendU32(data, MetadataVersion);
    appendU8(data, info.kind == MapCheckpointKind::CrashJournal ? 1u : 0u);
    appendU64(data, info.createdUnixNanos);
    appendU64(data, info.sourceRevision);
    appendString(data, info.id);
    appendString(data, info.mapId);
    appendString(data, info.name);
    appendString(data, info.createdAtUtc);
    appendString(data, info.transactionLabel);
    appendString(data, info.packageFingerprint);
    appendU64(data, fnv1a(data.data(), data.size()));

    std::filesystem::path temporary = path;
    temporary += std::filesystem::u8path(".tmp-" + createEntityId("metadata"));
    {
        std::ofstream stream(mapFileIoPath(temporary), std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "Could not create checkpoint metadata.";
            return false;
        }
        stream.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
        stream.flush();
        if (!stream)
        {
            error = "Could not write checkpoint metadata.";
            std::error_code cleanupError;
            std::filesystem::remove(temporary, cleanupError);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temporary, path, ec);
    if (ec)
    {
        error = "Could not atomically publish checkpoint metadata.";
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

class MetadataReader
{
public:
    explicit MetadataReader(const std::vector<std::uint8_t> &data) : data_(data) {}
    bool good() const { return error_.empty(); }
    const std::string &error() const { return error_; }
    std::size_t offset() const { return offset_; }
    std::uint8_t u8()
    {
        if (!need(1)) return 0;
        return data_[offset_++];
    }
    std::uint32_t u32()
    {
        if (!need(4)) return 0;
        std::uint32_t result = 0;
        for (int shift = 0; shift < 32; shift += 8) result |= static_cast<std::uint32_t>(data_[offset_++]) << shift;
        return result;
    }
    std::uint64_t u64()
    {
        if (!need(8)) return 0;
        std::uint64_t result = 0;
        for (int shift = 0; shift < 64; shift += 8) result |= static_cast<std::uint64_t>(data_[offset_++]) << shift;
        return result;
    }
    std::string string(std::size_t maximum)
    {
        const std::uint32_t length = u32();
        if (!good()) return {};
        if (length > maximum || !need(length))
        {
            if (good()) error_ = "Checkpoint metadata field exceeds the safety limit.";
            return {};
        }
        const char *begin = reinterpret_cast<const char *>(data_.data() + offset_);
        std::string value(begin, begin + length);
        offset_ += length;
        return value;
    }
private:
    bool need(std::size_t amount)
    {
        if (!good()) return false;
        if (amount > data_.size() - offset_)
        {
            error_ = "Checkpoint metadata is truncated.";
            return false;
        }
        return true;
    }
    const std::vector<std::uint8_t> &data_;
    std::size_t offset_ = 0;
    std::string error_;
};

bool readMetadata(const std::filesystem::path &path, StoredMetadata &metadata, std::string &error)
{
    std::ifstream stream(mapFileIoPath(path), std::ios::binary);
    if (!stream)
    {
        error = "Checkpoint metadata is missing.";
        return false;
    }
    std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (data.size() < MetadataMagic.size() + 4 + 1 + 8 + 8 + 6 * 4 + 8 || data.size() > 16 * 1024)
    {
        error = "Checkpoint metadata has an invalid size.";
        return false;
    }
    if (!std::equal(MetadataMagic.begin(), MetadataMagic.end(), data.begin()))
    {
        error = "Checkpoint metadata has an invalid signature.";
        return false;
    }
    std::uint64_t storedChecksum = 0;
    for (int shift = 0; shift < 64; shift += 8)
        storedChecksum |= static_cast<std::uint64_t>(data[data.size() - 8 + shift / 8]) << shift;
    if (storedChecksum != fnv1a(data.data(), data.size() - 8))
    {
        error = "Checkpoint metadata checksum failed.";
        return false;
    }

    MetadataReader reader(data);
    for (std::size_t index = 0; index < MetadataMagic.size(); ++index) reader.u8();
    if (reader.u32() != MetadataVersion)
    {
        error = "Unsupported checkpoint metadata version.";
        return false;
    }
    const std::uint8_t kind = reader.u8();
    if (kind > 1)
    {
        error = "Checkpoint metadata contains an invalid kind.";
        return false;
    }
    metadata.info.kind = kind == 1 ? MapCheckpointKind::CrashJournal : MapCheckpointKind::Named;
    metadata.info.createdUnixNanos = reader.u64();
    metadata.info.sourceRevision = reader.u64();
    metadata.info.id = reader.string(128);
    metadata.info.mapId = reader.string(4096);
    metadata.info.name = reader.string(512);
    metadata.info.createdAtUtc = reader.string(128);
    metadata.info.transactionLabel = reader.string(2048);
    metadata.info.packageFingerprint = reader.string(128);
    if (!reader.good() || reader.offset() != data.size() - 8)
    {
        error = reader.good() ? "Checkpoint metadata contains trailing data." : reader.error();
        return false;
    }
    return true;
}

} // namespace gm::map_checkpoint::detail
