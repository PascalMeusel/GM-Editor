#include "Models/Map/MapGeneration.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapImageTraceAuthoring.hpp"

#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAILED: " << message << '\n';
    return condition;
}
}

int main()
{
    using namespace MapImageTraceAuthoring;
    bool ok = true;

    ok &= require(featureKind(Mode::Coastline) == MapGeneratedFeatureKind::Coastline &&
                      featureKind(Mode::TerrainArea) == MapGeneratedFeatureKind::TerrainRegion &&
                      featureKind(Mode::Road) == MapGeneratedFeatureKind::Road &&
                      featureKind(Mode::River) == MapGeneratedFeatureKind::River &&
                      featureKind(Mode::Settlement) == MapGeneratedFeatureKind::Settlement &&
                      featureKind(Mode::PointOfInterest) == MapGeneratedFeatureKind::Landmark,
                  "every trace choice maps to the intended semantic feature kind");
    ok &= require(minimumPointCount(Mode::Coastline) == 3u && isClosed(Mode::Coastline) &&
                      minimumPointCount(Mode::TerrainArea) == 3u && isClosed(Mode::TerrainArea) &&
                      minimumPointCount(Mode::Road) == 2u && isLine(Mode::Road) &&
                      minimumPointCount(Mode::River) == 2u && isLine(Mode::River) &&
                      minimumPointCount(Mode::Settlement) == 1u &&
                      minimumPointCount(Mode::PointOfInterest) == 1u,
                  "polygon, spline, and point traces enforce useful minimums");

    const MapFeaturePoint snapped = constrainPoint({1.26f, -3.0f}, 10, 8, true);
    ok &= require(std::abs(snapped.x - 1.5f) < 0.0001f && snapped.y == 0.0f,
                  "optional snap is deterministic and remains inside the map");

    const std::vector<MapFeaturePoint> line{{0.0f, 1.0f}, {2.0f, 3.0f}, {5.0f, 4.0f}};
    const std::vector<MapFeaturePoint> smoothLine = smoothedPoints(line, false, true);
    ok &= require(smoothLine.size() == 6u && smoothLine.front().x == line.front().x &&
                      smoothLine.front().y == line.front().y && smoothLine.back().x == line.back().x &&
                      smoothLine.back().y == line.back().y,
                  "line smoothing preserves authored endpoints");
    const std::vector<MapFeaturePoint> smoothArea = smoothedPoints(line, true, true);
    ok &= require(smoothArea.size() == 6u,
                  "closed smoothing produces a loop without duplicating the first point");

    const MapImageImport::TraceRequest request = makeRequest(
        Mode::Coastline, "West coast", line, true, false);
    ok &= require(request.kind == MapGeneratedFeatureKind::Coastline && request.closed && !request.gmOnly &&
                      request.materialId == "trace.coastline" && request.points.size() == smoothArea.size(),
                  "trace request carries semantic kind, visibility, geometry, and renderer role");

    const std::vector<std::string> cultures{
        "germanic_central_european", "nordic", "romance", "neutral_fantasy"};
    std::set<std::string> settlementNames;
    std::set<std::string> landmarkNames;
    for (const std::string &culture : cultures)
    {
        const std::string settlement = mapGenV4SettlementName(240529u, 0, culture);
        const std::string landmark = mapGenV4LandmarkName(240529u, 0, culture);
        ok &= require(settlement == mapGenV4SettlementName(240529u, 0, culture) &&
                          landmark == mapGenV4LandmarkName(240529u, 0, culture),
                      "culture-aware names are deterministic");
        settlementNames.insert(settlement);
        landmarkNames.insert(landmark);
    }
    ok &= require(settlementNames.size() == cultures.size() && landmarkNames.size() == cultures.size(),
                  "the four exposed cultures produce distinguishable names");
    ok &= require(mapGenNormalizedNameCulture("de") == "germanic_central_european" &&
                      mapGenNormalizedNameCulture("romanisch") == "romance",
                  "culture aliases normalize to persisted stable IDs");

    if (ok)
        std::cout << "Map image trace workflow tests passed.\n";
    return ok ? 0 : 1;
}
