#pragma once

#include "Models/Map/MapModels.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <regex>
#include <utility>
#include <vector>

struct MapWorldBrief
{
    std::vector<std::string> nationNames;
    std::vector<std::string> cityNames;
    std::string capitalName;
    int statedNationCount = 0;
    bool requiresSharedGreatRiver = false;
    std::vector<MapGenerationFact> facts;
    // Clauses which the deterministic interpreter could not safely turn into
    // executable facts remain available to the Facts step.  They must never
    // disappear merely because another part of the same note was understood.
    std::vector<std::string> unrecognizedText;

    bool hasFacts() const
    {
        return !facts.empty() || !nationNames.empty() || !cityNames.empty() ||
               !capitalName.empty() || statedNationCount > 0 || requiresSharedGreatRiver ||
               !unrecognizedText.empty();
    }
};

namespace MapWorldBriefInterpreter
{
inline std::string trim(std::string value)
{
    const auto useful = [](unsigned char character) {
        return !std::isspace(character) && character != ',' && character != ';' && character != ':' && character != '.';
    };
    while (!value.empty() && !useful(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && !useful(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

inline std::string asciiLower(std::string value)
{
    // The parser deliberately stays locale-independent.  Normalize the only
    // non-ASCII upper-case letters used by the German grammar explicitly,
    // then lower-case ASCII bytes.  This yields identical results on Windows
    // and Linux regardless of the process locale.
    const std::array<std::pair<const char *, const char *>, 3> umlauts = {{
        {"\xC3\x84", "\xC3\xA4"}, {"\xC3\x96", "\xC3\xB6"}, {"\xC3\x9C", "\xC3\xBC"}}};
    for (const auto &[upper, lower] : umlauts)
    {
        std::size_t position = 0;
        while ((position = value.find(upper, position)) != std::string::npos)
        {
            value.replace(position, 2, lower);
            position += 2;
        }
    }
    for (char &character : value)
    {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte < 128)
            character = static_cast<char>(std::tolower(byte));
    }
    return value;
}

inline bool isWordByte(unsigned char byte)
{
    return byte >= 128 || std::isalnum(byte) != 0 || byte == '_';
}

inline bool containsTerm(const std::string &lower, const std::string &term)
{
    std::size_t position = 0;
    while ((position = lower.find(term, position)) != std::string::npos)
    {
        const bool leftBoundary = position == 0 || !isWordByte(static_cast<unsigned char>(lower[position - 1]));
        const std::size_t end = position + term.size();
        const bool rightBoundary = end >= lower.size() || !isWordByte(static_cast<unsigned char>(lower[end]));
        if (leftBoundary && rightBoundary)
            return true;
        position = end;
    }
    return false;
}

inline bool containsAnyTerm(const std::string &lower, const std::vector<std::string> &terms)
{
    return std::any_of(terms.begin(), terms.end(), [&](const std::string &term) {
        return containsTerm(lower, term);
    });
}

inline std::uint32_t stableTextHash(const std::string &value)
{
    std::uint32_t hash = 2166136261u;
    for (const unsigned char byte : value)
    {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

inline std::vector<std::string> splitClauses(const std::string &source)
{
    std::string separated = source;
    // Contrast and explicit negative conjunctions start a new semantic scope;
    // this keeps "a forest, but no roads" from excluding both features.
    for (const std::string &marker : {
             " aber ", " but ", " jedoch ", " however ",
             " und kein ", " und keine ", " und keinen ",
             " and no ", " and without "})
    {
        std::size_t position = 0;
        while ((position = asciiLower(separated).find(marker, position)) != std::string::npos)
        {
            separated.replace(position, marker.size(), "\n" + trim(marker) + " ");
            position += marker.size() + 1;
        }
    }
    for (char &character : separated)
        if (character == '.' || character == ';' || character == '\r')
            character = '\n';

    std::vector<std::string> clauses;
    std::size_t begin = 0;
    while (begin <= separated.size())
    {
        const std::size_t end = separated.find('\n', begin);
        std::string clause = trim(separated.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
        if (!clause.empty())
            clauses.push_back(std::move(clause));
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    return clauses;
}

inline void replaceAll(std::string &value, const std::string &needle, const std::string &replacement)
{
    if (needle.empty())
        return;
    std::size_t offset = 0;
    while ((offset = value.find(needle, offset)) != std::string::npos)
    {
        value.replace(offset, needle.size(), replacement);
        offset += replacement.size();
    }
}

inline int countBefore(const std::string &lower, std::size_t keywordPosition)
{
    if (keywordPosition == std::string::npos || keywordPosition == 0)
        return 0;
    std::size_t end = keywordPosition;
    while (end > 0 && std::isspace(static_cast<unsigned char>(lower[end - 1])))
        --end;
    std::size_t begin = end;
    while (begin > 0 && std::isdigit(static_cast<unsigned char>(lower[begin - 1])))
        --begin;
    if (begin != end)
    {
        try
        {
            return std::clamp(std::stoi(lower.substr(begin, end - begin)), 0, 24);
        }
        catch (...)
        {
            return 0;
        }
    }
    begin = end;
    while (begin > 0 && !std::isspace(static_cast<unsigned char>(lower[begin - 1])))
        --begin;
    const std::string word = lower.substr(begin, end - begin);
    const std::vector<std::string> numberWords = {
        "", "eine", "zwei", "drei", "vier", "fünf", "sechs", "sieben", "acht", "neun", "zehn"};
    const std::vector<std::string> englishNumberWords = {
        "", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten"};
    for (int value = 1; value <= 10; ++value)
        if (word == numberWords[static_cast<std::size_t>(value)] || word == englishNumberWords[static_cast<std::size_t>(value)])
            return value;
    return 0;
}

inline std::size_t firstMarkerAfter(const std::string &lower, std::size_t start, const std::vector<std::string> &markers, std::size_t &markerLength)
{
    std::size_t best = std::string::npos;
    markerLength = 0;
    for (const std::string &marker : markers)
    {
        const std::size_t found = lower.find(marker, start);
        if (found != std::string::npos && (best == std::string::npos || found < best))
        {
            best = found;
            markerLength = marker.size();
        }
    }
    return best;
}

inline std::vector<std::string> splitNames(std::string value)
{
    replaceAll(value, " und ", ",");
    replaceAll(value, " and ", ",");
    replaceAll(value, ";", ",");
    std::vector<std::string> names;
    std::size_t start = 0;
    while (start <= value.size())
    {
        const std::size_t end = value.find(',', start);
        std::string name = trim(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (name.size() >= 2 && name.front() == '"' && name.back() == '"')
            name = trim(name.substr(1, name.size() - 2));
        if (!name.empty() && std::none_of(names.begin(), names.end(), [&](const std::string &existing) {
                return asciiLower(existing) == asciiLower(name);
            }))
            names.push_back(std::move(name));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return names;
}

inline std::string extractSingleName(const std::string &source, const std::string &lower, std::size_t keywordPosition)
{
    std::size_t markerLength = 0;
    const std::size_t marker = firstMarkerAfter(lower, keywordPosition,
        {" namens ", " names ", " named ", " called ", " heißt ", " heisst ", " mit namen ", " mit name ", " name "}, markerLength);
    if (marker == std::string::npos || marker > keywordPosition + 80)
        return {};
    const std::size_t begin = marker + markerLength;
    std::size_t end = source.size();
    for (const std::string &stop : {".", ",", ";", " und ", " and ", " in der ", " in the "})
    {
        const std::size_t found = lower.find(stop, begin);
        if (found != std::string::npos)
            end = std::min(end, found);
    }
    return trim(source.substr(begin, end - begin));
}

inline bool plausibleProperName(const std::string &name)
{
    if (name.empty() || name.size() > 80) return false;
    const unsigned char first = static_cast<unsigned char>(name.front());
    if (!(first >= 0xc0 || (first >= 'A' && first <= 'Z') || first == '"')) return false;
    if (name.find_first_of(":;\n.!?") != std::string::npos ||
        containsAnyTerm(asciiLower(name), {"is", "are", "has", "have", "ist", "sind", "hat", "haben", "called", "named", "namens"})) return false;
    for (std::size_t p = name.find(' '); p != std::string::npos; p = name.find(' ', p + 1))
    {
        const auto end = name.find(' ', p + 1);
        const auto word = name.substr(p + 1, end == std::string::npos ? end : end - p - 1);
        if (word.empty() || word == "of" || word == "the" || word == "von" || word == "der" || word == "de") continue;
        const auto c = static_cast<unsigned char>(word.front());
        if (!(c >= 0xc0 || (c >= 'A' && c <= 'Z'))) return false;
    }
    return true;
}

// Accept common declarative prose as well as the explicit "named" grammar.
// Keep location qualifiers in clause review; recognising a name is not a
// promise that every geographical relationship in that sentence was resolved.
} // namespace MapWorldBriefInterpreter
