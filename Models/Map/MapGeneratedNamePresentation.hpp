#pragma once

#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace gm::map_presentation
{
// Generator-owned names stay language-neutral in persisted map semantics.
// Only this deliberately small allow-list is presented in the requested UI
// locale. Unknown strings are campaign content and must pass through byte for
// byte; in particular, names such as "Lake Zurich" are not translated merely
// because they happen to start with a familiar word.
struct GeneratedNamePair
{
    std::string_view english;
    std::string_view german;
};

inline bool localeIsGerman(std::string_view locale)
{
    while (!locale.empty() && std::isspace(static_cast<unsigned char>(locale.front())))
        locale.remove_prefix(1);
    return locale.size() >= 2 &&
           std::tolower(static_cast<unsigned char>(locale[0])) == 'd' &&
           std::tolower(static_cast<unsigned char>(locale[1])) == 'e';
}

inline bool hasNumericGeneratedSuffix(std::string_view name,
                                      std::string_view prefix)
{
    if (name.size() <= prefix.size() || name.substr(0, prefix.size()) != prefix)
        return false;
    bool sawDigit = false;
    for (std::size_t index = prefix.size(); index < name.size(); ++index)
    {
        const unsigned char character = static_cast<unsigned char>(name[index]);
        if (std::isdigit(character))
            sawDigit = true;
        else if (!std::isspace(character))
            return false;
    }
    return sawDigit;
}

inline std::string generatedName(std::string_view name,
                                 std::string_view locale)
{
    if (name.empty())
        return {};

    static constexpr std::array<GeneratedNamePair, 22> exactNames{{
        {"Main river", "Hauptfluss"},
        {"River crossing", "Flussübergang"},
        {"Settlement approach", "Siedlungszufahrt"},
        {"Settlement Center", "Siedlungszentrum"},
        {"City Gate", "Stadttor"},
        {"Main Gate Road", "Straße zum Haupttor"},
        {"Generated Route", "Erzeugte Route"},
        {"Generated Cave", "Erzeugte Höhle"},
        {"Generated Cave Route", "Erzeugte Höhlenroute"},
        {"Generated Map", "Erzeugte Karte"},
        {"Required feature", "Erforderliches Element"},
        {"Atlas crossing", "Atlasübergang"},
        {"Chokepoint", "Engstelle"},
        {"Party Start", "Start der Gruppe"},
        {"Opposition Start", "Start der Gegner"},
        {"Sheltered Rest", "Geschützter Rastplatz"},
        {"Discovery", "Entdeckung"},
        {"Meeting Place", "Treffpunkt"},
        {"Route Obstacle", "Wegeshindernis"},
        {"Danger Site", "Gefahrenort"},
        {"Edited way", "Bearbeiteter Weg"},
        {"Edited crossing", "Bearbeiteter Übergang"},
    }};
    static constexpr std::array<GeneratedNamePair, 3> germanAliases{{
        {"Route Obstacle", "Reisehindernis"},
        {"Danger Site", "Gefahrenstelle"},
        {"Settlement Center", "Ortszentrum"},
    }};
    static constexpr std::array<GeneratedNamePair, 10> numericPrefixes{{
        {"Settlement", "Siedlung"},
        {"Landmark", "Landmarke"},
        {"Lake", "See"},
        {"River branch", "Flussarm"},
        {"East-West Street", "Ost-West-Straße"},
        {"North-South Street", "Nord-Süd-Straße"},
        {"District Landmark", "Bezirkslandmarke"},
        {"Objective", "Ziel"},
        {"Tactical Route", "Taktische Route"},
        {"Journey", "Reise"},
    }};

    const bool german = localeIsGerman(locale);
    for (const GeneratedNamePair &pair : exactNames)
        if (name == pair.english || name == pair.german)
            return std::string(german ? pair.german : pair.english);
    for (const GeneratedNamePair &pair : germanAliases)
        if (name == pair.german)
            return std::string(german ? pair.german : pair.english);

    for (const GeneratedNamePair &pair : numericPrefixes)
    {
        if (hasNumericGeneratedSuffix(name, pair.english))
            return std::string(german ? pair.german : pair.english) +
                   std::string(name.substr(pair.english.size()));
        if (hasNumericGeneratedSuffix(name, pair.german))
            return std::string(german ? pair.german : pair.english) +
                   std::string(name.substr(pair.german.size()));
    }
    return std::string(name);
}
} // namespace gm::map_presentation
