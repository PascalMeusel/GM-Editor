#pragma once

#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "Models/Map/MapGeneration.hpp"

class MapGenerationOverworldBuilder final
{
  public:
    MapGenerationOverworldBuilder(MapGenerationResult &result, const MapGenerationOptions &options,
                                  std::mt19937 &rng, int totalSteps);

    void run();

  private:
    struct PlannedRoadEdge
    {
        int first = -1;
        int second = -1;
    };

    void initializeRecipe();
    void prepareTerrain();
    void paintTerrain();
    void establishLakes();
    void traceHydrology();
    void placeSettlements();
    void prepareRoadNetwork();
    void growRoadNetwork();
    void addSecondaryRoadLoops();
    void connectSettlementApproaches();
    void addRegionalRiverCrossing();
    void publishCrossings();
    void placeLandmarks();
    void finalizeMap();

    void addUsableEdgeNode(int side, int preferred);
    int roadPathCost(const std::vector<MapCellCoordinate> &path) const;

    MapModel &map;
    const MapGenerationOptions &options;
    std::mt19937 &rng;
    int totalSteps = 0;

    bool structuredRegion = false;
    bool sharedGreatRiver = false;
    int noiseScale = 0;
    float worldForestShiftX = 0.0f;
    float worldForestShiftY = 0.0f;
    float secondaryForestShiftX = 0.0f;
    float secondaryForestShiftY = 0.0f;
    bool islandWoodlands = false;
    std::vector<float> elevation;
    std::vector<float> moisture;
    std::vector<MapCellCoordinate> river;
    std::vector<MapGenerationConstraint> effectiveConstraints;
    std::vector<MapCellCoordinate> settlements;
    std::vector<const MapGenerationConstraint *> settlementConstraints;

    MapWayKind roadKind = MapWayKind::DirtRoad;
    bool omitRoads = false;
    std::vector<MapCellCoordinate> networkNodes;
    std::size_t settlementNodeCount = 0;
    std::vector<bool> connected;
    std::vector<PlannedRoadEdge> plannedRoadEdges;
    int roadIndex = 0;
    bool growingOverviewTree = false;
    std::vector<std::string> grownRoadIds;
};

