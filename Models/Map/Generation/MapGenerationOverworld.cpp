#include "Models/Map/Generation/MapGenerationOverworldBuilder.hpp"

void mapGenV2BuildOverworld(MapGenerationResult &result, const MapGenerationOptions &options, std::mt19937 &rng,
                            int totalSteps)
{
    MapGenerationOverworldBuilder{result, options, rng, totalSteps}.run();
}
