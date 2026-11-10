#pragma once

#include <random>
#include <string>
#include <vector>

#include "Models/Map/MapGeneration.hpp"

class MapGenerationSettlementBuilder final
{
  public:
    MapGenerationSettlementBuilder(MapGenerationResult &result, const MapGenerationOptions &options,
                                   std::mt19937 &rng, int totalSteps);

    void run();

  private:
    void initializeRecipe();
    void shapeTerrain();
    void planStreetsAndCrossings();
    void buildDistrictsAndBlocks();
    void finalizeMap();

    void addLineFeature(const std::string &id, const std::string &name, MapGeneratedFeatureKind kind,
                        const std::string &material, float width, std::vector<MapFeaturePoint> points,
                        bool closed = false);
    void addStreet(const std::string &name, MapWayKind way, std::vector<MapFeaturePoint> points,
                   bool closed = false);

    MapModel &map;
    const MapGenerationOptions &options;
    std::mt19937 &rng;
    int totalSteps = 0;
    int settlementArchetype = 0;
    int centerX = 0;
    int centerY = 0;
    int radiusX = 0;
    int radiusY = 0;
    int westGateX = 0;
    int eastGateX = 0;
    int northGateY = 0;
    int southGateY = 0;
};

