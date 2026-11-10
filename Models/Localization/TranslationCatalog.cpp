#include "Models/Localization/TranslationCatalog.hpp"
#include "Models/Localization/TranslationCatalogSegments.hpp"

#include <array>

namespace gm::localization
{
namespace
{
using detail::CatalogSegment;
using detail::TranslationEntry;
using detail::UiLiteralEntry;

template <typename Entry, std::size_t SegmentCount>
const Entry *entryAt(
    const std::array<CatalogSegment<Entry>, SegmentCount> &segments,
    std::size_t index)
{
    for (const CatalogSegment<Entry> &segment : segments)
    {
        if (index < segment.size)
            return segment.entries + index;
        index -= segment.size;
    }
    return nullptr;
}

template <std::size_t SegmentCount>
const UiLiteralEntry *findLiteral(
    const std::array<CatalogSegment<UiLiteralEntry>, SegmentCount> &segments,
    std::string_view english)
{
    for (const CatalogSegment<UiLiteralEntry> &segment : segments)
        for (std::size_t index = 0; index < segment.size; ++index)
            if (segment.entries[index].english == english)
                return segment.entries + index;
    return nullptr;
}

template <typename Entry, std::size_t SegmentCount>
bool segmentsAreComplete(const std::array<CatalogSegment<Entry>, SegmentCount> &segments)
{
    for (const CatalogSegment<Entry> &segment : segments)
        for (std::size_t index = 0; index < segment.size; ++index)
            if (segment.entries[index].english.empty() || segment.entries[index].german.empty())
                return false;
    return true;
}

template <std::size_t SegmentCount>
std::string translateSystemFrom(
    const std::array<CatalogSegment<UiLiteralEntry>, SegmentCount> &segments,
    std::string_view english)
{
    if (const UiLiteralEntry *entry = findLiteral(segments, english))
        return std::string(entry->german);

    // Entries deliberately ending in ": " describe system-owned dynamic
    // details such as a file name. Translate the stable prefix while
    // preserving only its data suffix.
    for (const CatalogSegment<UiLiteralEntry> &segment : segments)
    {
        for (std::size_t index = 0; index < segment.size; ++index)
        {
            const UiLiteralEntry &entry = segment.entries[index];
            if (entry.english.size() < 2 || entry.english.substr(entry.english.size() - 2) != ": ")
                continue;
            if (english.size() >= entry.english.size()
                && english.substr(0, entry.english.size()) == entry.english)
            {
                return std::string(entry.german) + std::string(english.substr(entry.english.size()));
            }
        }
    }
    return {};
}
} // namespace

std::string_view translationKey(TextId id)
{
    const TranslationEntry *entry = entryAt(
        detail::kTranslationSegments,
        static_cast<std::size_t>(id));
    return entry ? entry->key : std::string_view{};
}

std::string_view translation(Locale locale, TextId id)
{
    const TranslationEntry *entry = entryAt(
        detail::kTranslationSegments,
        static_cast<std::size_t>(id));
    if (!entry)
        return {};
    return locale == Locale::German ? entry->german : entry->english;
}

bool catalogIsComplete(Locale locale)
{
    for (std::size_t index = 0; index < static_cast<std::size_t>(TextId::Count); ++index)
    {
        const TranslationEntry *entry = entryAt(detail::kTranslationSegments, index);
        if (!entry || entry->key.empty() || translation(locale, static_cast<TextId>(index)).empty())
            return false;
    }
    return true;
}

std::string_view uiLiteralTranslation(Locale locale, std::string_view english)
{
    if (locale != Locale::German || english.empty())
        return english;
    if (const UiLiteralEntry *entry = findLiteral(detail::kUiLiteralSegments, english))
        return entry->german;
    if (const UiLiteralEntry *entry = findLiteral(detail::kSpecialistUiLiteralSegments, english))
        return entry->german;
    return english;
}

std::string uiSystemMessageTranslation(Locale locale, std::string_view english)
{
    if (locale != Locale::German || english.empty())
        return std::string(english);

    std::string translated = translateSystemFrom(detail::kUiLiteralSegments, english);
    if (translated.empty())
        translated = translateSystemFrom(detail::kSpecialistUiLiteralSegments, english);
    return translated.empty() ? std::string("Unbekannter technischer Fehler.") : translated;
}

bool uiLiteralCatalogIsComplete()
{
    return segmentsAreComplete(detail::kUiLiteralSegments)
        && segmentsAreComplete(detail::kSpecialistUiLiteralSegments);
}

std::size_t uiLiteralCatalogSize()
{
    return detail::catalogSize(detail::kUiLiteralSegments)
        + detail::catalogSize(detail::kSpecialistUiLiteralSegments);
}
} // namespace gm::localization