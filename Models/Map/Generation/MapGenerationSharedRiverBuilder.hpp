#pragma once

#include <random>
#include <vector>

#include "Models/Map/MapGeneration.hpp"

class MapGenerationSharedRiverBuilder final
{
  public:
    MapGenerationSharedRiverBuilder(MapModel &map, const MapGenerationOptions &options,
                                    const std::vector<float> &elevation);

    std::vector<MapCellCoordinate> build();

  private:
    bool usableWaypoint(const MapCellModel *cell) const;
    bool selectWaypoint();
    void measureCoastDistance();
    bool selectSource();
    bool selectOutlet();
    bool buildMainRiver();
    void addTributaries();

    MapModel &map;
    const MapGenerationOptions &options;
    const std::vector<float> &elevation;
    int maximumRadius = 0;
    MapCellCoordinate waypoint{-1, -1};
    std::vector<int> coastDistanceField;
    int deepestLandCell = 0;
    int sourceCoastClearance = 0;
    MapCellCoordinate source{-1, -1};
    MapCellCoordinate outlet{-1, -1};
    std::vector<MapCellCoordinate> path;
};

