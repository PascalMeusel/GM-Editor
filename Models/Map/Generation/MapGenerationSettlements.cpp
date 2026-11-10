#include "Models/Map/Generation/MapGenerationSettlementBuilder.hpp"

void mapGenV3BuildSettlement(MapGenerationResult &result, const MapGenerationOptions &options, std::mt19937 &rng,
                             int totalSteps)
{
    MapGenerationSettlementBuilder{result, options, rng, totalSteps}.run();
}
