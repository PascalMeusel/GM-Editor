#pragma once

#include "Models/Core/EntityMetadata.hpp"

#include <algorithm>
#include <string>
#include <vector>

enum class WorldObjectVisibility
{
    GmOnly,
    Discovered,
    Public
};

enum class WorldPlaceType
{
    Capital,
    City,
    Town,
    Village,
    Landmark,
    Site
};

struct NormalizedWorldPointModel
{
    double x = 0.0;
    double y = 0.0;
};

struct NationTerritoryModel
{
    EntityMetadata metadata{"nation_territory"};
    std::string name;
    std::vector<NormalizedWorldPointModel> polygon;
};

struct NationModel
{
    EntityMetadata metadata{"nation"};
    std::string name;
    std::string description;
    std::string government;
    std::string culture;
    EntityId capitalPlaceId;
    std::string relationships;
    std::vector<EntityId> factionIds;
    std::vector<NationTerritoryModel> territories;
    WorldObjectVisibility visibility = WorldObjectVisibility::GmOnly;
};

struct WorldPlaceModel
{
    EntityMetadata metadata{"world_place"};
    WorldPlaceType type = WorldPlaceType::Site;
    std::string name;
    std::string description;
    std::string notes;
    EntityId nationId;
    EntityId linkedMapId;
    EntityId linkedFeatureId;
    bool hasMapPosition = false;
    NormalizedWorldPointModel mapPosition;
    WorldObjectVisibility visibility = WorldObjectVisibility::GmOnly;
};

inline double normalizedWorldCoordinate(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

inline void normalizeNationTerritory(NationTerritoryModel &territory)
{
    for (NormalizedWorldPointModel &point : territory.polygon)
    {
        point.x = normalizedWorldCoordinate(point.x);
        point.y = normalizedWorldCoordinate(point.y);
    }
}

inline void normalizeWorldPlacePosition(WorldPlaceModel &place)
{
    place.mapPosition.x = normalizedWorldCoordinate(place.mapPosition.x);
    place.mapPosition.y = normalizedWorldCoordinate(place.mapPosition.y);
}
