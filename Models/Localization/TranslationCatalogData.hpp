#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace gm::localization::detail
{
struct TranslationEntry
{
    std::string_view key;
    std::string_view english;
    std::string_view german;
};

struct UiLiteralEntry
{
    std::string_view english;
    std::string_view german;
};

template <typename Entry>
struct CatalogSegment
{
    const Entry *entries = nullptr;
    std::size_t size = 0;
};

template <typename Entry, std::size_t Size>
constexpr CatalogSegment<Entry> catalogSegment(const std::array<Entry, Size> &entries)
{
    return {entries.data(), entries.size()};
}

template <typename Entry, std::size_t SegmentCount>
constexpr std::size_t catalogSize(const std::array<CatalogSegment<Entry>, SegmentCount> &segments)
{
    std::size_t size = 0;
    for (const CatalogSegment<Entry> &segment : segments)
        size += segment.size;
    return size;
}
} // namespace gm::localization::detail
