#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include "Models/Map/MapCaveCompositionV12.hpp"
#include "Models/Map/MapDungeonCompositionV12.hpp"
#include "Models/Map/MapTileSemantics.hpp"
#include "Models/Map/MapMountainPassRouting.hpp"
#include "Models/Map/MapRoadClearance.hpp"
#include "Models/Map/MapRegionComposition.hpp"
#include "Models/Map/MapWorldLandform.hpp"


void mapGenBuildMountains(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng)
{
    const int area = std::max(1, map.width * map.height);
    const int density = mapGenClampPercent(options.mountainDensity);
    if (density == 0)
        return;

    if (options.mountainPattern == MapMountainPattern::Ranges || options.mountainPattern == MapMountainPattern::Mixed)
    {
        const int ridgeCount = std::max(1, density / 34);
        for (int ridge = 0; ridge < ridgeCount; ++ridge)
        {
            const int startX = mapGenRandomBetween(rng, 1, std::max(1, map.width / 4));
            const int startY = mapGenRandomBetween(rng, 1, std::max(1, map.height - 2));
            const int endX = mapGenRandomBetween(rng, std::max(1, map.width * 2 / 3), std::max(1, map.width - 2));
            const int endY = std::clamp(startY + mapGenRandomBetween(rng, -map.height / 3, map.height / 3), 1,
                                        std::max(1, map.height - 2));
            mapGenDrawMountainRidge(map, startX, startY, endX, endY, options, rng);
            if (density > 76 && mapGenChance(rng, 10))
            {
                int branchOffset = mapGenRandomBetween(rng, -6, 6);
                if (std::abs(branchOffset) < 4)
                    branchOffset = branchOffset < 0 ? -4 : 4;
                mapGenDrawMountainRidge(
                    map, startX + mapGenRandomBetween(rng, 0, 3),
                    std::clamp(startY + branchOffset, 1, std::max(1, map.height - 2)), endX,
                    std::clamp(endY + branchOffset + mapGenRandomBetween(rng, -1, 1), 1, std::max(1, map.height - 2)),
                    options, rng);
            }
        }
    }

    if (options.mountainPattern == MapMountainPattern::IndividualPeaks ||
        options.mountainPattern == MapMountainPattern::Mixed)
    {
        const int peakCount = std::max(1, area * density / 1450);
        for (int i = 0; i < peakCount; ++i)
        {
            for (int attempt = 0; attempt < 36; ++attempt)
            {
                const int x = mapGenRandomBetween(rng, 1, map.width - 2);
                const int y = mapGenRandomBetween(rng, 1, map.height - 2);
                if (mapGenNearMountain(map, x, y, options.mountainPattern == MapMountainPattern::Mixed ? 2 : 1))
                    continue;
                mapGenSetCell(map, x, y, MapTileKind::Mountain, MapSymbolVariantMode::Individual);
                const MapCellCoordinate halo[] = {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}};
                for (const MapCellCoordinate &neighbor : halo)
                {
                    MapCellModel *cell = mapCellAt(map, neighbor.x, neighbor.y);
                    if (cell && cell->kind == MapTileKind::Empty && mapGenChance(rng, 42))
                        mapGenSetCell(map, neighbor.x, neighbor.y,
                                      mapGenChance(rng, 55) ? MapTileKind::Hill : MapTileKind::Stone);
                }
                break;
            }
        }
    }

    if (options.mountainPattern != MapMountainPattern::IndividualPeaks)
    {
        mapGenBreakUpMountainSlabs(map, rng);
        mapGenBreakUpMountainSlabs(map, rng);
        mapGenBreakUpMountainSlabs(map, rng);
    }

    mapGenAddTerrainBorder(map, MapTileKind::Mountain, MapTileKind::Stone, MapTileKind::Empty, 1,
                           std::min(84, 30 + density), rng);
    mapGenAddTerrainBorder(map, MapTileKind::Mountain, MapTileKind::Hill, MapTileKind::Empty, 2,
                           std::min(70, 18 + density / 2), rng);
}

float mapGenV2Hash01(std::uint32_t seed, int x, int y)
{
    std::uint32_t value =
        seed ^ (static_cast<std::uint32_t>(x) * 0x9e3779b9u) ^ (static_cast<std::uint32_t>(y) * 0x85ebca6bu);
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return static_cast<float>(value & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

float mapGenV2Smooth(float value)
{
    return value * value * (3.0f - 2.0f * value);
}

float mapGenV2ValueNoise(std::uint32_t seed, float x, float y, int scale)
{
    const float scaledX = x / static_cast<float>(std::max(1, scale));
    const float scaledY = y / static_cast<float>(std::max(1, scale));
    const int x0 = static_cast<int>(std::floor(scaledX));
    const int y0 = static_cast<int>(std::floor(scaledY));
    const float tx = mapGenV2Smooth(scaledX - static_cast<float>(x0));
    const float ty = mapGenV2Smooth(scaledY - static_cast<float>(y0));
    const float a = mapGenV2Hash01(seed, x0, y0);
    const float b = mapGenV2Hash01(seed, x0 + 1, y0);
    const float c = mapGenV2Hash01(seed, x0, y0 + 1);
    const float d = mapGenV2Hash01(seed, x0 + 1, y0 + 1);
    const float top = a + (b - a) * tx;
    const float bottom = c + (d - c) * tx;
    return top + (bottom - top) * ty;
}

float mapGenV2FractalNoise(std::uint32_t seed, float x, float y, int baseScale)
{
    return mapGenV2ValueNoise(seed, x, y, baseScale) * 0.56f +
           mapGenV2ValueNoise(seed + 1013u, x, y, std::max(2, baseScale / 2)) * 0.29f +
           mapGenV2ValueNoise(seed + 7919u, x, y, std::max(2, baseScale / 4)) * 0.15f;
}

std::string mapGenV2FeatureId(std::uint32_t seed, const char *family, int index)
{
    return std::string("mapgen2_") + family + "_" + std::to_string(seed) + "_" + std::to_string(index);
}

float mapGenV4WorldLandScore(std::uint32_t seed, float normalizedX, float normalizedY)
{
    // World maps need recognisably different silhouettes.  A single noisy
    // ellipse made every seed read as the same central island, so V4 combines
    // a small set of deterministic continental arrangements.  The continuous
    // score is still fed through the terrain noise below, keeping coastlines
    // organic without making the semantic grid authoritative for the art.
    constexpr float pi = 3.14159265358979323846f;
    const float angle = (mapGenV2Hash01(seed + 2789u, 3, 11) - 0.5f) * pi * 0.42f;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float rotatedX = normalizedX * cosine - normalizedY * sine;
    const float rotatedY = normalizedX * sine + normalizedY * cosine;
    const auto blob = [](float x, float y, float centerX, float centerY, float radiusX, float radiusY) {
        const float dx = (x - centerX) / std::max(0.05f, radiusX);
        const float dy = (y - centerY) / std::max(0.05f, radiusY);
        return 1.0f - std::sqrt(dx * dx + dy * dy);
    };

    float score = -4.0f;
    switch ((seed / 29u) % 4u)
    {
    case 0: // one broad continent with a pronounced peninsula
        score = std::max(blob(rotatedX, rotatedY, -0.12f, 0.02f, 0.82f, 0.69f),
                         blob(rotatedX, rotatedY, 0.48f, -0.20f, 0.40f, 0.34f));
        break;
    case 1: // two distinct continents separated by a navigable sea
        score = std::max(blob(rotatedX, rotatedY, -0.43f, -0.04f, 0.48f, 0.69f),
                         blob(rotatedX, rotatedY, 0.45f, 0.09f, 0.46f, 0.62f));
        break;
    case 2: // archipelago / broken land bridge
        score = std::max({blob(rotatedX, rotatedY, -0.52f, -0.28f, 0.39f, 0.34f),
                          blob(rotatedX, rotatedY, -0.12f, 0.24f, 0.43f, 0.38f),
                          blob(rotatedX, rotatedY, 0.43f, -0.14f, 0.42f, 0.46f),
                          blob(rotatedX, rotatedY, 0.58f, 0.48f, 0.22f, 0.20f)});
        break;
    default: // long diagonal continental chain
        score = std::max({blob(rotatedX, rotatedY, -0.48f, 0.36f, 0.48f, 0.42f),
                          blob(rotatedX, rotatedY, -0.03f, 0.02f, 0.55f, 0.45f),
                          blob(rotatedX, rotatedY, 0.48f, -0.34f, 0.45f, 0.38f)});
        break;
    }
    return score;
}

float mapGenV10WorldLandScore(std::uint32_t seed, float x, float y, int waterLevel, MapLandscapeProfile landscape)
{
    const float phase = mapGenV2Hash01(seed + 907u, 4, 13) * 6.2831853f;
    const float wx = x + .10f * std::sin(y * 5.1f + phase) + .035f * std::sin(y * 11.f - phase);
    const float wy = y + .095f * std::sin(x * 4.6f - phase) + .030f * std::cos(x * 12.f + phase);
    const auto blob = [](float x, float y, float cx, float cy, float rx, float ry) {
        const float dx = (x - cx) / rx, dy = (y - cy) / ry;
        return 1.f - std::sqrt(dx * dx + dy * dy);
    };
    if (waterLevel >= 55 && landscape != MapLandscapeProfile::LakeCountry)
    {
        // A maritime setting has genuine channels and offshore destinations,
        // not merely a wetter wash on the same landlocked-road continent.
        const float mirror = (seed & 1u) ? -1.f : 1.f;
        const float ax = wx * mirror;
        const float main = std::max(blob(ax, wy, -.26f, .02f, .60f, .68f), blob(ax, wy, -.04f, -.34f, .37f, .32f));
        const float phaseY = (mapGenV2Hash01(seed + 109u, 3, 7) - .5f) * .12f;
        const auto drift = [&](int key) {
            return mapGenV2Hash01(seed + static_cast<std::uint32_t>(key * 317), key, 19) - .5f;
        };
        return std::max({main,
                         blob(ax, wy, .61f + drift(1) * .22f, -.48f + phaseY + drift(2) * .13f, .21f + drift(3) * .10f,
                              .20f + drift(4) * .08f),
                         blob(ax, wy, .66f + drift(5) * .21f, .01f + phaseY + drift(6) * .22f, .19f + drift(7) * .09f,
                              .19f + drift(8) * .08f),
                         blob(ax, wy, .55f + drift(9) * .28f, .53f + phaseY + drift(10) * .13f, .22f + drift(11) * .10f,
                              .18f + drift(12) * .09f)});
    }
    if (waterLevel <= 25)
    {
        // Wide continental coasts intentionally continue beyond a world
        // viewport. The coast remains a curve, never a hard margin clamp.
        const float mirror = (seed & 1u) ? -1.f : 1.f;
        return std::max(blob(wx * mirror, wy, -.36f, .02f, .98f, .84f), blob(wx * mirror, wy, .18f, .35f, .53f, .35f));
    }
    return mapGenV4WorldLandScore(seed, wx / .87f, wy / .87f);
}

float mapGenV4SharedRiverWorldLandScore(std::uint32_t seed, float normalizedX, float normalizedY)
{
    // A fact shared by several nations needs one continental drainage basin.
    // Selecting the normal two-continent or archipelago silhouettes makes that
    // fact physically impossible and previously produced one unrelated river
    // per island.  Keep seeded coastal character, but guarantee a broad,
    // connected heartland through which a single major river can run.
    constexpr float pi = 3.14159265358979323846f;
    const float phase = mapGenV2Hash01(seed + 3251u, 7, 13) * pi * 2.0f;
    const float bend = std::sin(normalizedY * pi * 0.82f + phase) * 0.10f;
    const float shiftedX = normalizedX - bend;
    const float mainX = shiftedX / 0.82f;
    const float mainY = normalizedY / 0.72f;
    float score = 1.0f - std::sqrt(mainX * mainX + mainY * mainY);

    const auto blob = [](float x, float y, float centerX, float centerY, float radiusX, float radiusY) {
        const float dx = (x - centerX) / std::max(0.05f, radiusX);
        const float dy = (y - centerY) / std::max(0.05f, radiusY);
        return 1.0f - std::sqrt(dx * dx + dy * dy);
    };
    score = std::max(score, blob(normalizedX, normalizedY, -0.56f, 0.12f, 0.34f, 0.43f));
    score = std::max(score, blob(normalizedX, normalizedY, 0.53f, -0.16f, 0.36f, 0.40f));
    return score;
}

float mapGenV5SharedRiverWorldLandScore(std::uint32_t seed, float normalizedX, float normalizedY)
{
    // A shared river still needs one connected drainage basin, but that does
    // not imply a featureless oval continent.  Compose the heartland from
    // overlapping lobes, add one or two narrow peninsulas, then cut bays back
    // into the outer coast.  Small offshore islands are deliberately detached
    // and never carry the required river or principal nations.
    constexpr float pi = 3.14159265358979323846f;
    const float angle = (mapGenV2Hash01(seed + 4721u, 3, 17) - 0.5f) * pi * 0.34f;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float x = normalizedX * cosine - normalizedY * sine;
    const float y = normalizedX * sine + normalizedY * cosine;
    const auto blob = [](float sampleX, float sampleY, float centerX, float centerY, float radiusX, float radiusY) {
        const float dx = (sampleX - centerX) / std::max(0.05f, radiusX);
        const float dy = (sampleY - centerY) / std::max(0.05f, radiusY);
        return 1.0f - std::sqrt(dx * dx + dy * dy);
    };

    float score = -4.0f;
    switch ((seed / 31u) % 3u)
    {
    case 0: // broad western shelf, hooked eastern peninsula
        score = std::max({blob(x, y, -0.34f, 0.08f, 0.52f, 0.57f), blob(x, y, 0.06f, -0.08f, 0.58f, 0.59f),
                          blob(x, y, 0.42f, 0.12f, 0.43f, 0.43f), blob(x, y, 0.69f, -0.18f, 0.25f, 0.20f),
                          blob(x, y, -0.18f, 0.57f, 0.20f, 0.27f)});
        break;
    case 1: // diagonal continental chain with opposing capes
        score = std::max({blob(x, y, -0.42f, 0.28f, 0.48f, 0.43f), blob(x, y, -0.08f, 0.02f, 0.58f, 0.54f),
                          blob(x, y, 0.35f, -0.27f, 0.47f, 0.42f), blob(x, y, -0.70f, 0.04f, 0.25f, 0.20f),
                          blob(x, y, 0.62f, -0.50f, 0.21f, 0.26f)});
        break;
    default: // asymmetric crescent with a long southern reach
        score = std::max({blob(x, y, -0.36f, -0.22f, 0.48f, 0.47f), blob(x, y, 0.08f, 0.02f, 0.61f, 0.56f),
                          blob(x, y, 0.45f, 0.28f, 0.39f, 0.38f), blob(x, y, 0.12f, 0.62f, 0.20f, 0.29f),
                          blob(x, y, -0.67f, -0.42f, 0.25f, 0.22f)});
        break;
    }

    const float baySide = mapGenV2Hash01(seed + 4751u, 11, 5) < 0.5f ? -1.0f : 1.0f;
    const float primaryBay = blob(x, y, baySide * (0.67f + mapGenV2Hash01(seed + 4759u, 7, 13) * 0.06f),
                                  (mapGenV2Hash01(seed + 4783u, 19, 2) - 0.5f) * 0.42f, 0.30f, 0.20f);
    if (primaryBay > 0.0f)
        score = std::min(score, 0.015f - primaryBay * 0.66f);
    if (mapGenV2Hash01(seed + 4799u, 3, 29) > 0.38f)
    {
        const float secondaryBay =
            blob(x, y, (mapGenV2Hash01(seed + 4813u, 31, 7) - 0.5f) * 0.48f, -baySide * 0.65f, 0.20f, 0.27f);
        if (secondaryBay > 0.0f)
            score = std::min(score, 0.018f - secondaryBay * 0.58f);
    }

    // Roughly two thirds of worlds gain a small island, while only a minority
    // gain a second.  Their positions are chosen from the open-water corners
    // rather than merely extending the mainland by another round lobe.
    const float islandRoll = mapGenV2Hash01(seed + 4831u, 13, 37);
    if (islandRoll > 0.31f)
    {
        const float islandSide = mapGenV2Hash01(seed + 4849u, 41, 3) < 0.5f ? -1.0f : 1.0f;
        score = std::max(score, blob(x, y, islandSide * 0.88f, -baySide * 0.67f, 0.095f + islandRoll * 0.035f,
                                     0.075f + islandRoll * 0.030f));
        if (islandRoll > 0.78f)
            score = std::max(score, blob(x, y, -islandSide * 0.86f, baySide * 0.72f, 0.075f, 0.065f));
    }

    // Broad, low-frequency deformation changes capes and bays between adjacent
    // seeds without introducing the one-cell coastline static caused by using
    // high-frequency terrain noise as the only silhouette variation.
    const float phase = mapGenV2Hash01(seed + 4871u, 5, 43) * pi * 2.0f;
    score += std::sin(x * pi * 2.15f + phase) * 0.035f + std::sin(y * pi * 2.70f - phase * 0.61f) * 0.024f;
    return score;
}

