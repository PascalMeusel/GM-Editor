#pragma once

#include "Models/Map/TravelSceneGenerator.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace TravelSceneGenerator::detail
{
std::string trim(std::string value);
std::uint32_t stableHash(const std::string &value);
std::string stableIdPart(std::string value);
std::string normalizedContentLanguage(std::string value);
std::string normalizedNameCulture(const TravelSceneRecipe &recipe);
bool usesGermanNarrative(const TravelSceneRecipe &recipe);
const char *travelPurposeName(TravelScenePurpose purpose, bool german);
const char *travelSummary(TravelScenePurpose purpose, bool german);
MapLandscapeProfile landscapeFor(const TravelSceneRecipe &recipe);
bool travelRecipeRequestsWater(const TravelSceneRecipe &recipe);

template <std::size_t N>
const char *pick(const std::array<const char *, N> &values, std::uint32_t seed, int salt)
{
    return values[(seed ^ static_cast<std::uint32_t>(salt * 2654435761u)) % N];
}

int travelWaterCompositionV4(const TravelSceneRecipe &recipe);
bool composeTravelWaterV4(MapModel &map, const TravelSceneRecipe &recipe, unsigned seed);
void composeTravelRoad(MapModel &map, const TravelSceneRecipe &recipe, std::uint32_t seed);
void composeTravelPurpose(MapModel &map, const TravelSceneRecipe &recipe, std::uint32_t seed);
void composeTravelFurnishingsV4(MapModel &map, const TravelSceneRecipe &recipe);
} // namespace TravelSceneGenerator::detail

