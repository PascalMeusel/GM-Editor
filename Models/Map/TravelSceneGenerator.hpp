#pragma once

#include "Models/Map/MapGeneration.hpp"

#include <cstdint>
#include <string>
#include <vector>

enum class TravelScenePurpose
{
    Quiet,
    Discovery,
    Social,
    Obstacle,
    Danger
};

struct TravelSceneRecipe
{
    int schemaVersion = 2;
    // Schema 3 opts into the current composition; schema 2 remains byte-stable.
    int width = 0;
    int height = 0;
    EntityId sourceMapId;
    EntityId routeId;
    std::string routeName;
    std::string biome;
    std::string season = "Temperate";
    std::string timeOfDay = "Day";
    std::string weather = "Auto";
    TravelScenePurpose purpose = TravelScenePurpose::Discovery;
    int dangerLevel = 1;
    std::uint32_t seed = 240529;
    // Existing callers keep the historical English narrative unless they
    // explicitly inherit a campaign/map language. Generated proper names use
    // the stable culture identifier independently from UI localization.
    std::string contentLanguage = "en";
    std::string nameCulture = "neutral_fantasy";
    std::vector<std::string> requiredElements;
    std::vector<std::string> excludedElements;
    bool combatReady = false;
};

struct TravelSceneCandidate
{
    int rank = 0;
    std::uint32_t seed = 0;
    std::string title;
    std::string summary;
    std::string discovery;
    std::string complication;
    std::string gmSecret;
    MapModel localMap;
    MapGenerationQualityReport quality;
    bool canPrepareEncounter = false;
};

namespace TravelSceneGenerator
{
std::vector<std::string> parseElementList(const std::string &text);
std::vector<TravelSceneCandidate> generate(
    const MapModel &sourceMap,
    const MapRouteModel *route,
    const TravelSceneRecipe &recipe,
    int candidateCount = 3,
    const MapGenerationCancellationCallback &cancellationRequested = {},
    const MapGenerationProgressCallback &progressCallback = {});
std::vector<TravelSceneCandidate> generateStandalone(
    const TravelSceneRecipe &recipe,
    int candidateCount = 3,
    const MapGenerationCancellationCallback &cancellationRequested = {},
    const MapGenerationProgressCallback &progressCallback = {});
}
