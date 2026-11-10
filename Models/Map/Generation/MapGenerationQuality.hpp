#pragma once
#include "Models/Map/Generation/MapGenerationTypes.hpp"

#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapStyleRegistry.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <queue>
#include <random>
#include <string>
#include <vector>

void mapGenEnsureGenerationMetadata(MapModel &map, const MapGenerationOptions &options);

bool mapGenFeatureSatisfiesRequirement(const MapFeatureGraph &graph, const std::string &id, const std::string &label);

bool mapGenPlaceRequiredFeature(MapModel &map, const MapGenerationOptions &options, const std::string &id,
                                const std::string &label, MapGeneratedFeatureKind kind, float requestedX,
                                float requestedY, bool locked, int salt);

void mapGenApplyRequiredFeatures(MapModel &map, const MapGenerationOptions &options);

MapGenerationResult mapGenGenerateCandidate(const MapGenerationOptions &options);

bool mapGenQualityTraversable(const MapCellModel &cell);

bool mapGenQualityWayCell(const MapCellModel &cell, MapScope scope);

template <typename Predicate>
inline int mapGenLargestComponentSize(const MapModel &map, Predicate predicate, bool includeDiagonals)
{
    if (map.cells.empty())
        return 0;

    std::vector<bool> visited(map.cells.size(), false);
    int largest = 0;
    for (int startY = 0; startY < map.height; ++startY)
    {
        for (int startX = 0; startX < map.width; ++startX)
        {
            const int startIndex = startY * map.width + startX;
            const MapCellModel *start = mapCellAt(map, startX, startY);
            if (!start || visited[static_cast<std::size_t>(startIndex)] || !predicate(*start))
                continue;

            int componentSize = 0;
            std::queue<MapCellCoordinate> pending;
            pending.push({startX, startY});
            visited[static_cast<std::size_t>(startIndex)] = true;
            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                ++componentSize;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if ((dx == 0 && dy == 0) || (!includeDiagonals && dx != 0 && dy != 0))
                            continue;
                        const int nextX = current.x + dx;
                        const int nextY = current.y + dy;
                        if (!isMapCellInside(map, nextX, nextY))
                            continue;
                        const int nextIndex = nextY * map.width + nextX;
                        if (visited[static_cast<std::size_t>(nextIndex)])
                            continue;
                        const MapCellModel *next = mapCellAt(map, nextX, nextY);
                        if (!next || !predicate(*next))
                            continue;
                        visited[static_cast<std::size_t>(nextIndex)] = true;
                        pending.push({nextX, nextY});
                    }
                }
            }
            largest = std::max(largest, componentSize);
        }
    }
    return largest;
}

template <typename Predicate> inline int mapGenCountMatchingCells(const MapModel &map, Predicate predicate)
{
    return static_cast<int>(std::count_if(map.cells.begin(), map.cells.end(), predicate));
}

bool mapGenCoordinatesConnected(const MapModel &map, const MapCellCoordinate &start, const MapCellCoordinate &end,
                                bool requireWays);

int mapGenCoherentTerrainPercent(const MapModel &map);

int mapGenV2RoadEfficiencyPercent(const MapModel &map);

bool mapGenV4PointInsidePolygon(const MapFeaturePoint &point, const std::vector<MapFeaturePoint> &polygon);

bool mapGenV4PointTouchesPolygon(const MapFeaturePoint &point, const std::vector<MapFeaturePoint> &polygon,
                                 float boundaryDistance = 1.25f);

bool mapGenV4SharedGreatRiverIsValid(const MapModel &map, std::string *failureCode = nullptr);

int mapGenV5WorldRiverCompositionPercent(const MapModel &map);

int mapGenV2HydrologyPercent(const MapModel &map);

int mapGenV2BridgeValidityPercent(const MapModel &map);

int mapGenV2SettlementSuitabilityPercent(const MapModel &map);

int mapGenV5WorldRoadTerrainPercent(const MapModel &map);

int mapGenV5WorldRoadNetworkPercent(const MapModel &map);

int mapGenV5WorldTerritoryStraightRayPercent(const MapModel &map);

int mapGenV5WorldTerritoryNaturalismPercent(const MapModel &map);

int mapGenV5WorldTerritoryLandAdjacencyPercent(const MapModel &map);

int mapGenV5WorldTerritoryGeographyPercent(const MapModel &map);

int mapGenV5WorldSettlementSpreadPercent(const MapModel &map);

int mapGenV5WorldMountainCompositionPercent(const MapModel &map);

int mapGenV5WorldLargestLandMotifPercent(const MapModel &map);

int mapGenV5WorldCoastCompositionPercent(const MapModel &map);

int mapGenV5WorldBiomeCompositionPercent(const MapModel &map);

int mapGenV2CompositionPercent(const MapModel &map);

int mapGenV4ScaleCorrectnessPercent(const MapModel &map);

int mapGenV4VisualHierarchyPercent(const MapModel &map);

const char *mapGenV4TerrainAssetSemantic(MapTileKind kind);

int mapGenV4AssetCoveragePercent(const MapModel &map);

MapGenerationQualityReport analyzeMapGenerationQuality(const MapModel &map);

std::uint32_t mapGenCandidateSeed(std::uint32_t baseSeed, int candidateIndex);

std::string mapGenNormalizedElementText(const std::string &value);

bool mapGenExcludedElementMatches(const std::string &value, const std::vector<std::string> &excludedElements);

bool mapGenNormalizedContainsToken(const std::string &normalized, const std::string &token);

std::string mapGenSemanticElementKey(const std::string &value);

// Only a standalone semantic family name is allowed to suppress an entire
// generated family.  Legacy recipes may contain descriptive exclusions such
// as "modern highway"; treating every phrase that happens to mention a road
// as "remove all roads" erased ordinary fantasy roads and even bridge cells
// that form part of an otherwise valid river.  Typed V5 facts are normalized
// to the canonical keys before this point, so this keeps the broad behaviour
// for explicit "road", "river", ... exclusions without over-interpreting
// arbitrary legacy prose.
std::string mapGenCanonicalExcludedSemanticKey(const std::string &value);

bool mapGenCellHasSemantic(const MapCellModel &cell, const std::string &key);

bool mapGenHasCoast(const MapModel &map);

bool mapGenHasWaterComponent(const MapModel &map, bool mustTouchBoundary);

bool mapGenHasIsland(const MapModel &map);

bool mapGenSemanticElementPresent(const MapModel &map, const std::string &key);

void mapGenApplyFactsToOptions(MapGenerationOptions &options);

bool mapGenFeatureMatchesText(const MapGeneratedFeature &feature, const std::string &text);

const MapGeneratedFeature *mapGenV5FindNationRelationFeature(const MapModel &map, const std::string &name);

float mapGenV5PointSegmentDistanceSquared(const MapFeaturePoint &point, const MapFeaturePoint &start,
                                          const MapFeaturePoint &end);

bool mapGenV5TerritoriesShareBorder(const MapModel &map, const MapGeneratedFeature &left,
                                    const MapGeneratedFeature &right);

bool mapGenV5RelativeFactIsSatisfied(const MapModel &map, const MapGenerationFact &fact);

bool mapGenFactIsSatisfied(const MapModel &map, const MapGenerationFact &fact);

MapGenerationFactValidation mapGenValidateFactWithEvidence(const MapModel &map, const MapGenerationFact &fact);

MapGenerationValidationReport validateMapGeneration(const MapModel &map, const MapGenerationQualityReport &quality);

bool mapGenQualityMeetsCandidateFloor(const MapModel &map, const MapGenerationQualityReport &quality);

MapGenerationValidationReport validateMapGenerationRequest(const MapGenerationOptions &options);

bool mapGenApplyExcludedElements(MapGenerationResult &result, const MapGenerationOptions &options);

int mapGenCompositionDistancePercent(const MapModel &left, const MapModel &right);

std::vector<MapGenerationResult> mapGenSelectDiverseCandidates(std::vector<MapGenerationResult> ranked,
                                                               int resultCount);

MapGenerationBatchResult generateSemanticMapCandidateBatch(const MapGenerationOptions &options, int resultCount = 3);

std::vector<MapGenerationResult> generateSemanticMapCandidates(const MapGenerationOptions &options,
                                                               int resultCount = 3);

MapGenerationResult generateSemanticMap(const MapGenerationOptions &options);

