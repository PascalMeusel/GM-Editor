#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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


void mapGenReportProgress(const MapGenerationOptions &options, int step, int totalSteps, const std::string &stage)
{
    if (options.progressCallback)
        options.progressCallback(
            MapGenerationProgress{std::clamp(step, 0, totalSteps), std::max(0, totalSteps), stage});
}

bool mapGenCancellationRequested(const MapGenerationOptions &options)
{
    return options.cancellationRequested && options.cancellationRequested();
}

