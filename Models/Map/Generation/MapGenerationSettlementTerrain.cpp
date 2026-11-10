#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
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
#include "Models/Map/Generation/MapGenerationSettlementBuilder.hpp"

void MapGenerationSettlementBuilder::shapeTerrain()
{
    centerX = map.width / 2;
    centerY = map.height / 2;
    radiusX = std::max(7, map.width / 3);
    radiusY = std::max(6, map.height / 3);
    mapGenReportProgress(options, 2, totalSteps, "Shaping settlement terrain");
    std::vector<unsigned char> settlementGround(static_cast<std::size_t>(map.width * map.height), 0);
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const float dx = static_cast<float>(x - centerX) / static_cast<float>(radiusX);
            const float dy = static_cast<float>(y - centerY) / static_cast<float>(radiusY);
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance > 1.08f && mapGenChance(rng, options.forestDensity / 3))
                mapGenSetCell(map, x, y, MapTileKind::Forest, MapSymbolVariantMode::Continuous);
            else if (distance < 1.02f)
            {
                // A settlement needs one readable built-up footprint.  The
                // earlier noise threshold produced unrelated rectangular dirt
                // islands, making a town look like a few stamps scattered over
                // open grass.  Keep a broad connected centre and use low-
                // frequency noise only to feather its perimeter.
                const float noise =
                    mapGenV2FractalNoise(options.seed + 4409u, static_cast<float>(x), static_cast<float>(y), 11);
                const float edge = 0.74f + (noise - 0.5f) * 0.34f;
                settlementGround[static_cast<std::size_t>(y * map.width + x)] = distance < edge ? 1 : 0;
            }
        }
    }
    std::vector<unsigned char> smoothedGround = settlementGround;
    for (int y = 1; y + 1 < map.height; ++y)
    {
        for (int x = 1; x + 1 < map.width; ++x)
        {
            int adjacent = 0;
            adjacent += settlementGround[static_cast<std::size_t>(y * map.width + x - 1)] != 0;
            adjacent += settlementGround[static_cast<std::size_t>(y * map.width + x + 1)] != 0;
            adjacent += settlementGround[static_cast<std::size_t>((y - 1) * map.width + x)] != 0;
            adjacent += settlementGround[static_cast<std::size_t>((y + 1) * map.width + x)] != 0;
            const std::size_t index = static_cast<std::size_t>(y * map.width + x);
            if (settlementGround[index] && adjacent <= 1)
                smoothedGround[index] = 0;
            else if (!settlementGround[index] && adjacent >= 3)
                smoothedGround[index] = 1;
        }
    }
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
            if (smoothedGround[static_cast<std::size_t>(y * map.width + x)] != 0)
                mapGenSetCell(map, x, y, MapTileKind::Dirt, MapSymbolVariantMode::Continuous);

    // Parent roads, rivers and places are structural input to the settlement,
    // not a decorative merge performed after buildings have been placed.
    mapGenInstallPreservedFeatures(map, options);
}

