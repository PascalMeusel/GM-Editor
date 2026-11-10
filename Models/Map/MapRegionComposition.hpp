#pragma once

#include "Models/Map/MapModels.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

// The region generator composes a traversable landscape before hydrology and
// roads. This is deliberately independent of rendering and preserves the old
// generator's terrain field for every saved recipe before version 10.
namespace gm::map_generation::region
{
inline std::string landmarkMaterial(std::uint32_t seed,int ordinal) {
    constexpr const char *types[]={"ruins","watch","shrine","stones","monolith","gate"};
    return std::string("illustrated.region.")+types[(seed/31u+static_cast<unsigned>(ordinal))%6u];
}
inline float seedUnit(std::uint32_t seed, std::uint32_t salt)
{
    std::uint32_t h = seed ^ (salt * 0x9e3779b9u);
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return static_cast<float>(h & 0x00ffffffu) / 16777215.0f;
}

struct TerrainSample
{
    MapTileKind kind = MapTileKind::Empty;
    float elevation = 0.0f;
};

inline std::string landmarkName(const std::string &stem, std::uint32_t seed, int ordinal,
                                const std::string &language)
{
    const unsigned kind = (seed / 31u + static_cast<unsigned>(ordinal)) % 6u;
    const bool german = language == "de" || language.rfind("de-", 0) == 0;
    constexpr const char *de[] = {"Ruine ", "Wacht ", "Schrein ", "Steine ", "Monolith ", "Tor "};
    constexpr const char *en[] = {" Ruins", " Watch", " Shrine", " Stones", " Monolith", " Gate"};
    return german ? std::string(de[kind]) + stem : stem + en[kind];
}

inline TerrainSample terrain(std::uint32_t seed, int x, int y, int width, int height,
                             MapLandscapeProfile landscape, int mountainDensity,
                             int forestDensity, int waterLevel,
                             float heightNoise, float moistureNoise)
{
    const float scale = static_cast<float>(std::max(1, std::min(width, height)));
    const float px = (2.0f * (x + .5f) - width) / scale;
    const float py = (2.0f * (y + .5f) - height) / scale;
    // Oblique orientation families prevent candidate ranking from repeatedly
    // selecting a vertical spine. Small seeded variation keeps each distinct.
    constexpr float orientations[] = {-1.40f, -.90f, -.42f, .42f, .90f, 1.40f};
    const int orientation = std::min(5, static_cast<int>(seedUnit(seed, 7) * 6));
    const float angle = orientations[orientation] + (seedUnit(seed, 71) - .5f) * .18f;
    const float u = px * std::cos(angle) - py * std::sin(angle);
    const float v = px * std::sin(angle) + py * std::cos(angle);
    const float phase = seedUnit(seed, 11) * 6.2831853f;
    const float mountainAmount = std::clamp(mountainDensity / 100.0f, 0.0f, 1.0f);
    const bool highland = landscape == MapLandscapeProfile::Highlands;
    const bool pass = landscape == MapLandscapeProfile::MountainPass;
    const float bend = .21f * std::sin(v * 1.65f + phase) + .055f * std::sin(v * 4.7f + phase);
    const float center = (seedUnit(seed, 19) - .5f) * .62f + bend;
    const float ridgeWidth = (.065f + mountainAmount * .135f) *
                             (.92f + .18f * std::sin(v * 2.4f + phase));
    const auto ridgeAt = [&](float ridgeCenter, float widthMultiplier, float crestCenter, float crestLength) {
        const float distance = (u - ridgeCenter) / (ridgeWidth * widthMultiplier);
        const float along = std::abs((v - crestCenter) / crestLength);
        const float taper = std::exp(-std::pow(along, 6.0f));
        return std::exp(-distance * distance) * taper;
    };
    float ridge = ridgeAt(center + (pass ? .44f : .0f), 1.0f,
                         (seedUnit(seed, 23) - .5f) * .75f, .95f + seedUnit(seed, 31) * .65f);
    if (pass)
        ridge = std::max(ridge, ridgeAt(center - .51f + .08f * std::sin(v * 1.8f - phase),
            .65f + seedUnit(seed, 43) * .35f, (seedUnit(seed, 47) - .5f) * .95f,
            .75f + seedUnit(seed, 53) * .65f) * .90f);
    else if (highland && mountainDensity >= 45)
    {
        // A tapering spur meets the main highland crest instead of forming a
        // second railway-like parallel row. It leaves the other side open.
        const float side = seedUnit(seed, 79) < .5f ? -1.0f : 1.0f;
        const float spurOffset = side * std::max(.0f, .35f - .43f * (v + .15f));
        ridge = std::max(ridge, ridgeAt(center + spurOffset, .72f,
                         -.32f, .79f) * .90f);
    }
    // A broad saddle interrupts one crest without scattering isolated peaks.
    const float saddleV = (seedUnit(seed, 29) - .5f) * 1.0f;
    const float saddle = std::exp(-std::pow((v - saddleV) / .17f, 2.0f));
    ridge *= 1.0f - saddle * (pass ? .68f : .42f);
    ridge *= mountainDensity <= 0 ? 0.0f : std::min(1.0f, mountainAmount * 2.8f + .22f);

    const float forestCenterU = (seedUnit(seed, 37) - .5f) * 1.4f;
    const float forestCenterV = (seedUnit(seed, 41) - .5f) * 1.5f;
    const float forestX = (u - forestCenterU) / (.52f + forestDensity * .003f);
    const float forestY = (v - forestCenterV) / (.65f + forestDensity * .004f);
    const float province = std::exp(-(forestX * forestX + forestY * forestY));
    const float gladeCenterU = forestCenterU + (seedUnit(seed, 83) - .5f) * .78f;
    const float gladeCenterV = forestCenterV + (seedUnit(seed, 89) - .5f) * .90f;
    const float gladeX = (u - gladeCenterU + .045f * std::sin(v * 7 + phase)) /
                         (.24f + seedUnit(seed, 97) * .12f);
    const float gladeY = (v - gladeCenterV + .035f * std::sin(u * 8 - phase)) /
                         (.31f + seedUnit(seed, 101) * .18f);
    const float glade = std::exp(-(gladeX * gladeX + gladeY * gladeY));
    const float secondX = (u - forestCenterU + .55f) / .29f;
    const float secondY = (v - forestCenterV - .48f) / .34f;
    const float secondGlade = std::exp(-(secondX * secondX + secondY * secondY));
    const float clearing = glade * .36f + secondGlade * .25f;
    float forestField = moistureNoise + province * .32f - .18f - clearing;
    // A requested dense region must still contain a woodland
    // province when the coarse moisture field happens to be unusually dry.
    // The same glades remain genuinely open, rather than merely hiding stamps.
    if (forestDensity > 40)
    {
        const float strength = std::clamp((forestDensity - 40) / 25.0f, 0.0f, 1.0f);
        forestField += std::max(0.0f, .46f + province * .25f - clearing - forestField) * strength;
    }
    const float forestThreshold = .66f - std::clamp(forestDensity, 0, 100) * .0019f;

    TerrainSample result;
    result.elevation = std::clamp(.27f + heightNoise * .28f + ridge * .44f - py * .035f, .08f, .98f);
    if (ridge > .54f)
        result.kind = MapTileKind::Mountain;
    else if (ridge > .16f)
        result.kind = MapTileKind::Hill;
    else if (forestDensity > 0 && forestField > forestThreshold)
        result.kind = MapTileKind::Forest;
    else if (waterLevel > 44 && moistureNoise > .70f && ridge < .05f)
        result.kind = MapTileKind::Swamp;
    return result;
}
}
