#pragma once

#include "Models/Map/MapPackageService.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

namespace MapPackageServiceInternal
{
inline constexpr std::array<std::uint8_t, 12> Magic{{'T', 'T', 'R', 'P', 'G', 'M', 'A', 'P', 'P', 'K', 'G', 0}};
inline constexpr std::size_t HeaderSize = Magic.size() + sizeof(std::uint32_t) + sizeof(std::uint64_t) + sizeof(std::uint64_t);

class Writer
{
public:
    explicit Writer(const MapPackageLimits *limits = nullptr,
                    std::size_t maximumBytes = std::numeric_limits<std::size_t>::max())
        : limits_(limits), maximumBytes_(maximumBytes) {}
    const std::vector<std::uint8_t> &data() const { return data_; }
    std::vector<std::uint8_t> take() { return std::move(data_); }

    void u8(std::uint8_t value) { ensure(1); data_.push_back(value); }
    void boolean(bool value) { u8(value ? 1u : 0u); }
    void u32(std::uint32_t value)
    {
        for (int shift = 0; shift != 32; shift += 8)
            u8(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }
    void i32(int value) { u32(static_cast<std::uint32_t>(value)); }
    void u64(std::uint64_t value)
    {
        for (int shift = 0; shift != 64; shift += 8)
            u8(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }
    void floating(float value)
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t), "32-bit float required");
        if (limits_ && !std::isfinite(value))
            throw std::domain_error("A map package number is not finite.");
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u32(bits);
    }
    void string(const std::string &value)
    {
        if (value.size() > std::numeric_limits<std::uint32_t>::max() ||
            (limits_ && value.size() > limits_->maximumStringBytes))
            throw std::length_error("A map package string exceeds the safety limit.");
        u32(static_cast<std::uint32_t>(value.size()));
        ensure(value.size());
        data_.insert(data_.end(), value.begin(), value.end());
    }
    void bytes(const std::vector<std::uint8_t> &value)
    {
        if (limits_ && value.size() > limits_->maximumAssetBytes)
            throw std::length_error("A map package asset exceeds the safety limit.");
        u64(static_cast<std::uint64_t>(value.size()));
        ensure(value.size());
        data_.insert(data_.end(), value.begin(), value.end());
    }

private:
    void ensure(std::size_t bytes)
    {
        if (bytes > maximumBytes_ - data_.size())
            throw std::length_error("Encoded map exceeds the byte budget.");
        // Existing package callers retain their allocation behavior. Bounded
        // callers cannot grow vector capacity beyond their explicit budget.
        if (maximumBytes_ == std::numeric_limits<std::size_t>::max()) return;
        const auto needed = data_.size() + bytes;
        if (needed <= data_.capacity()) return;
        const auto growth = data_.capacity() > maximumBytes_ / 2
            ? maximumBytes_ : data_.capacity() * 2;
        data_.reserve(std::max(needed, std::min(maximumBytes_, std::max(std::size_t{64}, growth))));
    }
    std::vector<std::uint8_t> data_;
    const MapPackageLimits *limits_ = nullptr;
    std::size_t maximumBytes_;
};

class Reader
{
public:
    Reader(const std::uint8_t *data, std::size_t size, const MapPackageLimits &limits)
        : data_(data), size_(size), limits_(limits)
    {
    }

    bool good() const { return error_.empty(); }
    bool atEnd() const { return offset_ == size_; }
    const std::string &error() const { return error_; }

    std::uint8_t u8()
    {
        if (!need(1)) return 0;
        return data_[offset_++];
    }
    bool boolean()
    {
        const std::uint8_t value = u8();
        if (good() && value > 1) fail("Invalid Boolean value.");
        return value != 0;
    }
    std::uint32_t u32()
    {
        if (!need(4)) return 0;
        std::uint32_t value = 0;
        for (int shift = 0; shift != 32; shift += 8)
            value |= static_cast<std::uint32_t>(data_[offset_++]) << shift;
        return value;
    }
    int i32()
    {
        static_assert(sizeof(int) == sizeof(std::uint32_t), "32-bit int required");
        const std::uint32_t bits = u32();
        int value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    std::uint64_t u64()
    {
        if (!need(8)) return 0;
        std::uint64_t value = 0;
        for (int shift = 0; shift != 64; shift += 8)
            value |= static_cast<std::uint64_t>(data_[offset_++]) << shift;
        return value;
    }
    float floating()
    {
        const std::uint32_t bits = u32();
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        if (good() && !std::isfinite(value)) fail("Package contains a non-finite number.");
        return value;
    }
    std::string string(std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max())
    {
        const std::uint32_t length = u32();
        const std::uint32_t limit = maximum == std::numeric_limits<std::uint32_t>::max()
                                        ? limits_.maximumStringBytes
                                        : maximum;
        if (good() && length > limit)
        {
            fail("Package string exceeds the safety limit.");
            return {};
        }
        if (!need(length)) return {};
        const char *begin = reinterpret_cast<const char *>(data_ + offset_);
        std::string value(begin, begin + length);
        offset_ += length;
        return value;
    }
    std::string pathString() { return string(limits_.maximumPathBytes); }
    std::vector<std::uint8_t> bytes()
    {
        const std::uint64_t length = u64();
        if (good() && length > limits_.maximumAssetBytes)
        {
            fail("Embedded asset exceeds the safety limit.");
            return {};
        }
        if (length > std::numeric_limits<std::size_t>::max() || !need(static_cast<std::size_t>(length))) return {};
        std::vector<std::uint8_t> value(data_ + offset_, data_ + offset_ + static_cast<std::size_t>(length));
        offset_ += static_cast<std::size_t>(length);
        return value;
    }
    std::uint32_t count()
    {
        const std::uint32_t value = u32();
        if (good() && value > limits_.maximumCollectionItems)
            fail("Package collection exceeds the safety limit.");
        return value;
    }
    template <typename Enum>
    Enum enumeration(std::uint32_t maximum)
    {
        const std::uint32_t value = u32();
        if (good() && value > maximum) fail("Package contains an invalid enumeration value.");
        return static_cast<Enum>(value);
    }
    void fail(const std::string &message)
    {
        if (error_.empty()) error_ = message;
    }

private:
    bool need(std::size_t bytes)
    {
        if (!good()) return false;
        if (bytes > size_ - offset_)
        {
            fail("Package is truncated.");
            return false;
        }
        return true;
    }

    const std::uint8_t *data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t offset_ = 0;
    const MapPackageLimits &limits_;
    std::string error_;
};

template <typename Enum>
void writeEnum(Writer &writer, Enum value)
{
    writer.u32(static_cast<std::uint32_t>(value));
}

void writeMetadata(Writer &writer, const EntityMetadata &metadata);
void readMetadata(Reader &reader, EntityMetadata &metadata);
void writeStrings(Writer &writer, const std::vector<std::string> &values);
void readStrings(Reader &reader, std::vector<std::string> &values);
void writeRect(Writer &writer, const MapNormalizedRect &rect);
void readRect(Reader &reader, MapNormalizedRect &rect);
void writeMap(Writer &writer, const MapModel &map);
void readMap(Reader &reader, MapModel &map, std::uint32_t formatVersion);

std::uint64_t fnv1a(const std::uint8_t *data, std::size_t size);
std::string contentHash(const std::vector<std::uint8_t> &bytes);
bool safePackagePath(const std::string &value, const MapPackageLimits &limits);
bool safeLeafName(const std::string &value, const MapPackageLimits &limits);
std::string safeFileName(const std::filesystem::path &path, const std::string &fallback);
bool readAssetFile(const std::filesystem::path &path, const MapPackageLimits &limits,
                   std::vector<std::uint8_t> &bytes, std::string &error);
std::filesystem::path resolveAssetSourcePath(const std::filesystem::path &path);
bool reasonableMap(const MapModel &map, const MapPackageLimits &limits, std::string &error);
void writeAsset(Writer &writer, const MapPackageAsset &asset);
void readAssets(Reader &reader, std::vector<MapPackageAsset> &assets,
                const MapPackageLimits &limits, std::uint32_t formatVersion);
bool validateInternalAssetReferences(const MapModel &map,
                                     const std::vector<MapPackageAsset> &assets,
                                     std::string &error);

struct DecodedPackage
{
    std::uint32_t version = 0;
    std::uint64_t fileBytes = 0;
    MapModel map;
    std::vector<MapPackageAsset> assets;
    std::string error;
};

struct OwnedIdentitySummary
{
    std::size_t fieldCount = 0;
    std::size_t uniqueCount = 0;
    std::size_t duplicateCount = 0;
    std::size_t uniqueAssetIdCount = 0;
};

struct FeatureLineageSummary
{
    std::size_t total = 0;
    std::size_t internal = 0;
    std::size_t external = 0;
};

OwnedIdentitySummary summarizeOwnedIdentities(const MapModel &map);
bool hasLineage(const MapFeatureLineage &lineage);
FeatureLineageSummary summarizeFeatureLineage(const MapModel &map);
DecodedPackage decode(const std::filesystem::path &path, const MapPackageLimits &limits);
void remapOwnedIds(MapModel &map,
                   std::vector<std::pair<std::string, std::string>> &reported,
                   bool standalone, bool remap);
bool samePathComponent(const std::filesystem::path &left,
                       const std::filesystem::path &right);
bool pathIsBelow(const std::filesystem::path &root,
                 const std::filesystem::path &candidate);
bool validateMaterializationDestination(const std::filesystem::path &root,
                                        const std::filesystem::path &relative,
                                        const std::filesystem::path &destination,
                                        bool allowOverwrite,
                                        std::string &error);
}
