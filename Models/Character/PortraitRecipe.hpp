#pragma once
#include "Models/Core/EntityMetadata.hpp"
#include <algorithm>
#include <charconv>
#include <cstdint>

namespace gm::portraits
{
inline constexpr int currentCollection = 1;
// Persisted alongside the entity, so copying, recovery and campaign transfer
// retain the portrait without depending on a remote generator or local cache.
struct Recipe
{
    // `expression` is the legacy persisted slot. It now selects one of eight
    // distinct people for the species rather than an emotion of one person.
    int species = 0, features = 0, expression = 0, skin = 0, hair = 0;
    int hairStyle = 0, beard = 0;
    // Collection 0 is the immutable Distinct library. Collection 1 adds an
    // independently cast Company library; changing it is an explicit edit.
    int collection = currentCollection;
    std::uint32_t seed = 1;
};
// Append-only IDs preserve existing campaign portraits.
inline constexpr const char *speciesNames[] = {
    "Human", "Elf", "Dwarf", "Orc", "Tiefling", "Dragonborn", "Tabaxi", "Undead",
    "Aasimar", "Gnome", "Goliath", "Halfling", "Half-Elf", "Half-Orc", "Goblin", "Hobgoblin",
    "Bugbear", "Kobold", "Aarakocra", "Kenku", "Tortle", "Lizardfolk", "Firbolg", "Genasi",
    "Gith", "Triton", "Yuan-ti", "Changeling", "Shifter", "Warforged", "Satyr", "Fairy",
    "Harengon", "Owlin", "Leonin", "Minotaur", "Centaur", "Loxodon", "Giff", "Thri-kreen",
    "Plasmoid", "Autognome", "Hadozee", "Kalashtar", "Verdan", "Locathah", "Grung"
};
inline constexpr int speciesCount = int(std::size(speciesNames));
inline int speciesForRace(const std::string &race)
{
    for(int i=0;i<speciesCount;++i) if(race == speciesNames[i]) return i;
    if(race == "Reptilian") return 5;
    if(race == "Beastfolk") return 6;
    if(race.find("Elf") != std::string::npos || race == "Drow" || race == "Eladrin" || race == "Shadar-kai") return 1;
    if(race.find("Genasi") != std::string::npos) return 23;
    if(race.find("Dwarf") != std::string::npos || race == "Duergar") return 2;
    if(race.find("Gnome") != std::string::npos || race == "Deep Gnome") return 9;
    if(race.find("Halfling") != std::string::npos) return 11;
    if(race == "Githyanki" || race == "Githzerai") return 24;
    return 0;
}
inline int value(const EntityMetadata &metadata, const char *key, int fallback, int maximum)
{
    const auto found = metadata.customProperties.find(key);
    if (found == metadata.customProperties.end())
        return fallback;
    int result = 0;
    const auto &text = found->second;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() ? std::clamp(result, 0, maximum)
                                                                               : fallback;
}
inline bool exists(const EntityMetadata &m)
{
    return m.customProperties.count("portrait.recipe") != 0;
}
inline Recipe read(const EntityMetadata &m)
{
    Recipe r;
    r.collection = value(m, "portrait.collection", exists(m) ? 0 : currentCollection, currentCollection);
    r.species = value(m, "portrait.species", 0, speciesCount - 1);
    r.features = value(m, "portrait.features", 0, 3);
    r.expression = value(m, "portrait.expression", 0, 7);
    r.skin = value(m, "portrait.skin", 0, 5);
    r.hair = value(m, "portrait.hair", 0, 4);
    r.hairStyle = value(m, "portrait.hairStyle", 0, 4);
    r.beard = value(m, "portrait.beard", 0, 2);
    r.seed = std::uint32_t(value(m, "portrait.seed", 1, 2147483647));
    return r;
}
inline void write(EntityMetadata &m, const Recipe &r)
{
    m.customProperties["portrait.recipe"] = "1";
    for (const auto &p :
         std::initializer_list<std::pair<const char *, int>>{{"portrait.species", r.species},
                                                             {"portrait.collection", std::clamp(r.collection, 0, currentCollection)},
                                                             {"portrait.features", r.features},
                                                             {"portrait.expression", r.expression},
                                                             {"portrait.skin", r.skin},
                                                             {"portrait.hair", r.hair},
                                                             {"portrait.hairStyle", r.hairStyle},
                                                             {"portrait.beard", r.beard},
                                                             {"portrait.seed", int(r.seed & 0x7fffffffu)}})
        m.customProperties[p.first] = std::to_string(p.second);
}
inline void vary(Recipe &r)
{
    r.seed = (r.seed * 1664525u + 1013904223u) & 0x7fffffffu;
    r.features = int((r.seed >> 4) % 4);
    r.skin = int((r.seed >> 8) % 6);
    r.hair = int((r.seed >> 12) % 5);
    r.hairStyle = int((r.seed >> 17) % 5);
    r.beard = int((r.seed >> 22) % 3);
}
} // namespace gm::portraits
