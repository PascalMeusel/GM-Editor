#pragma once

#include "Models/Combat/EncounterModel.hpp"
#include "Models/Map/MapAtlasService.hpp"

#include <string>
#include <vector>

struct EncounterAtlasTacticalRequest
{
    MapNormalizedRect sourceRect;
    int width = 24;
    int height = 18;
    float cellSize = 1.0f;
    EntityId tacticalMapId;
    std::string name;
    std::uint64_t revision = 1;
};

struct EncounterAtlasTacticalResult
{
    bool valid = false;
    MapModel tacticalMap;
    std::vector<MapGenerationConstraint> boundaryConstraints;
    std::vector<std::string> errors;
};

namespace EncounterAtlasService
{
EncounterAtlasTacticalResult deriveTacticalMap(
    const MapModel &source,
    const EncounterAtlasTacticalRequest &request);

bool transferToEncounter(
    const MapModel &source,
    const MapModel &tacticalMap,
    EncounterModel &encounter,
    std::vector<std::string> *errors = nullptr);

EncounterAtlasTacticalResult deriveTacticalMap(
    const MapModel &source,
    const EncounterAtlasTacticalRequest &request,
    EncounterModel *encounter);
}
