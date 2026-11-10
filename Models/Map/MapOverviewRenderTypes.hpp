#pragma once

#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapRoadClearance.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gm::map_render
{

// A scale-appropriate mountain painting is placed per coherent section of a
// connected range.  This is intentionally derived render data: it neither
// changes the feature graph nor the synchronized gameplay grid.
struct OverviewMountainStamp
{
    float centerX = 0.0f;
    float centerY = 0.0f;
    float angleRadians = 0.0f;
    float lengthCells = 0.0f;
    float widthCells = 0.0f;
    int anchorX = 0;
    int anchorY = 0;
    int componentCellCount = 0;
    int sectionIndex = 0;
};

// A renderer-neutral coastline segment. Coordinates and normals are expressed
// in map-cell space so the Dear ImGui canvas and PNG exporter can apply the
// same cartographic treatment without duplicating coastline geometry.
struct OverviewCoastSegment
{
    float startX = 0.0f;
    float startY = 0.0f;
    float endX = 0.0f;
    float endY = 0.0f;
    float landNormalX = 0.0f;
    float landNormalY = 0.0f;
};

// Political regions are stored as independent clipped polygons. Drawing every
// polygon perimeter duplicates shared edges and turns coastline fragments into
// borders. The overview renderer instead assigns each land cell to exactly one
// territory and derives a single internal-border graph from owner changes.
struct OverviewPoliticalOwnerProjection
{
    int width = 0;
    int height = 0;
    int territoryCount = 0;
    std::vector<int> owners;
};

struct OverviewPoliticalBorderRun
{
    int firstOwner = -1;
    int secondOwner = -1;
    std::vector<MapFeaturePoint> points;
};

struct OverviewPoliticalStrokeSegment
{
    MapFeaturePoint start;
    MapFeaturePoint end;
};

enum class OverviewSettlementGlyphKind
{
    Town,
    City,
    Capital
};

struct OverviewSettlementGlyphStyle
{
    float outerRadiusCells = 0.40f;
    float innerRadiusCells = 0.25f;
};

// World mountain ranges are cartographic line work, not close-range raster
// paintings.  The renderer-neutral geometry below is shared by ImGui and PNG
// so previews, editing and export use the same ridge silhouette.
struct OverviewRidgeMark
{
    MapFeaturePoint leftBase;
    MapFeaturePoint leftShoulder;
    MapFeaturePoint peak;
    MapFeaturePoint rightShoulder;
    MapFeaturePoint rightBase;
    MapFeaturePoint shadeEnd;
    MapFeaturePoint secondaryLeftBase;
    MapFeaturePoint secondaryPeak;
    MapFeaturePoint secondaryRightBase;
    MapFeaturePoint secondaryShadeEnd;
    float emphasis = 1.0f;
};

struct OverviewRidgePath
{
    std::vector<MapFeaturePoint> spine;
    std::vector<OverviewRidgeMark> marks;
};

struct OverviewForestGlyph
{
    MapFeaturePoint center;
    // Radius scale of one multi-lobed canopy cluster. World forests use a few
    // broad crowns over a continuous biome, never one tactical tree per cell.
    float sizeCells = 0.62f;
    float variation = 0.0f;
};

// Sparse, abstract brush hachures give a continental forest enough internal
// structure to read as woodland without pasting tactical trees onto a world
// map.  Geometry is expressed in map space and shared by editor and export.
struct OverviewForestHachure
{
    MapFeaturePoint start;
    MapFeaturePoint crest;
    MapFeaturePoint end;
    float emphasis = 1.0f;
};

struct OverviewRenderColor
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

// Keep these values beside the shared geometry policy. They are deliberately
// subdued: the coastline should clarify the landmass, not compete with rivers
// and roads, while the ridge tint lets the warm terrain show through the much
// sharper source painting.
inline constexpr OverviewRenderColor kOverviewCoastWaterShadow{33, 68, 76, 38};
inline constexpr OverviewRenderColor kOverviewCoastLine{70, 83, 61, 160};
inline constexpr OverviewRenderColor kOverviewCoastLandHighlight{225, 216, 169, 88};
inline constexpr OverviewRenderColor kOverviewOceanBase{61, 111, 124, 255};
inline constexpr OverviewRenderColor kOverviewPlainBase{153, 164, 112, 255};
inline constexpr OverviewRenderColor kOverviewForestBase{101, 132, 91, 255};
inline constexpr OverviewRenderColor kOverviewMountainGround{158, 149, 119, 255};
inline constexpr OverviewRenderColor kOverviewForestInk{44, 77, 51, 155};
inline constexpr OverviewRenderColor kOverviewForestHighlight{164, 184, 117, 160};
inline constexpr OverviewRenderColor kOverviewMountainTint{238, 232, 208, 186};
inline constexpr OverviewRenderColor kOverviewRidgeShadow{54, 48, 38, 105};
inline constexpr OverviewRenderColor kOverviewRidgeInk{81, 73, 57, 222};
inline constexpr OverviewRenderColor kOverviewRidgeHighlight{220, 204, 160, 225};
inline constexpr OverviewRenderColor kOverviewSettlementInk{56, 43, 30, 242};
inline constexpr OverviewRenderColor kOverviewSettlementPaper{221, 194, 132, 242};
inline constexpr OverviewRenderColor kOverviewSettlementAccent{232, 178, 69, 252};
// Shared cartographic inks. Keeping them here prevents the editor and PNG
// export from slowly drifting into two different-looking maps.
inline constexpr OverviewRenderColor kOverviewBorderHalo{39, 34, 29, 0};
inline constexpr OverviewRenderColor kOverviewBorderInk{157, 116, 98, 158};
inline constexpr OverviewRenderColor kOverviewRoadShoulder{63, 50, 35, 138};
inline constexpr OverviewRenderColor kOverviewRoadSurface{184, 148, 96, 218};
inline constexpr OverviewRenderColor kOverviewRiverBank{24, 61, 70, 166};
inline constexpr OverviewRenderColor kOverviewRiverWater{38, 101, 116, 232};
inline constexpr OverviewRenderColor kOverviewRiverGlint{132, 167, 164, 46};
inline constexpr float kOverviewRoadWidthScale = 1.12f;
// Region, site and tactical curves use a quieter, opaque two-tone treatment.
// The previous three nested strokes (bank, dark blue rim and bright centre
// glint) read as plastic tubing at fit-to-window scale.  Keeping these colours
// beside the World inks also guarantees that Dear ImGui and PNG exports do not
// drift apart again.
inline constexpr OverviewRenderColor kLocalRoadShoulder{104, 85, 59, 255};
inline constexpr OverviewRenderColor kLocalRoadSurface{156, 124, 80, 255};
inline constexpr OverviewRenderColor kLocalTrailSurface{125, 99, 65, 255};
inline constexpr OverviewRenderColor kLocalRoadTrack{112, 86, 55, 255};
inline constexpr OverviewRenderColor kLocalRiverBank{43, 82, 86, 255};
inline constexpr OverviewRenderColor kLocalRiverWater{45, 102, 113, 255};
// The immutable V3.3 snapshot resolves the neutral modulation by semantic
// role. It is mapped once over the complete world canvas so no repeat seam or
// tactical-scale detail can appear. Keep its opacity restrained: biome colour
// and hydrology stay primary.
inline constexpr std::uint8_t kOverviewPainterlySurfaceOpacity = 20;
// Runtime cartography revision. Immutable source-art snapshot IDs and saved
// map semantics retain their existing versions; no asset pin is rewritten.
inline constexpr int kOverviewCartographyRevision = 2;
inline constexpr float kOverviewMountainLengthScale = 0.84f;
inline constexpr float kOverviewMountainWidthScale = 1.16f;
// V3.3 sources are long, transparent macro paintings with substantially less
// built-in ground plate than V3.2. Give them enough transverse mass to read as
// ranges at fit-to-window scale, and cool the formerly beige runtime tint.
inline constexpr float kOverviewV33MountainWidthScale =
    kOverviewMountainWidthScale * 1.36f;
inline constexpr OverviewRenderColor kOverviewV33MountainTint{221, 217, 197, 228};
inline constexpr float kOverviewCoastWaterOffsetCells = 0.10f;
inline constexpr float kOverviewCoastLandOffsetCells = 0.05f;
inline constexpr float kOverviewCoastWaterStrokeCells = 0.12f;
inline constexpr float kOverviewCoastLineStrokeCells = 0.06f;
inline constexpr float kOverviewCoastLandStrokeCells = 0.08f;
inline constexpr float kOverviewCoastBlendRadiusCells = 2.20f;
inline constexpr float kOverviewTerrainBlendRadiusCells = 1.18f;
inline constexpr float kOverviewPoliticalDashLengthCells = 0.86f;
inline constexpr float kOverviewPoliticalGapLengthCells = 0.50f;

struct OverviewSamplePoint
{
    float x = 0.0f;
    float y = 0.0f;
};

inline float overviewValueNoise(float mapX, float mapY, std::uint32_t seed);
inline OverviewSamplePoint overviewOrganicTerrainSample(const MapModel &map,
                                                        float mapX,
                                                        float mapY);
inline float overviewCartographicLandCoverage(const MapModel &map,
                                               float mapX, float mapY,
                                               float rawCoverage, bool sharpen = true);

namespace detail
{
class OverviewGroundSamplingCache;
inline OverviewSamplePoint overviewOrganicTerrainSampleCached(
    const MapModel &map, float mapX, float mapY,
    const OverviewGroundSamplingCache *cache);
inline float overviewCartographicLandCoverageCached(
    const MapModel &map, float mapX, float mapY, float rawCoverage, bool sharpen,
    const OverviewGroundSamplingCache *cache);
} // namespace detail

} // namespace gm::map_render
