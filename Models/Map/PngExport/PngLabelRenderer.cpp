#include "Models/Map/PngExport/PngLabelRenderer.hpp"
#include "Models/Map/MapGeneratedNamePresentation.hpp"
#include "Models/Map/MapOverviewLabelLod.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapRoutePresentation.hpp"
#include "Models/Map/MapTypography.hpp"
#include "Models/Map/PngExport/PngLabelLayout.hpp"
#include "Models/Map/PngExport/PngTextRenderer.hpp"
#include "Models/Map/PngExport/PngVisualPolicy.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace MapPngExportRaster
{
enum class PngMapLabelKind
{
    Nation,
    Settlement,
    Landmark
};

struct PngMapLabel
{
    const MapGeneratedFeature *feature = nullptr;
    PngMapLabelKind kind = PngMapLabelKind::Landmark;
    MapFeaturePoint anchor;
    int priority = 0;
};

bool pngHasGeneratedNumericName(std::string_view name, std::string_view prefix)
{
    if (name.size() <= prefix.size() || name.substr(0, prefix.size()) != prefix)
        return false;
    for (std::size_t index = prefix.size(); index < name.size(); ++index)
        if (!std::isdigit(static_cast<unsigned char>(name[index])) && name[index] != ' ')
            return false;
    return true;
}

bool pngIsSemanticCartographicName(const MapGeneratedFeature &feature)
{
    const std::string_view name = feature.name;
    if (name.empty() || name.size() > 64 || name.find('\n') != std::string_view::npos ||
        name.find('\r') != std::string_view::npos || name.find(';') != std::string_view::npos)
        return false;
    if (name == "Required feature" || name == "Erforderliches Element" ||
        pngHasGeneratedNumericName(name, "Settlement") || pngHasGeneratedNumericName(name, "Siedlung") ||
        pngHasGeneratedNumericName(name, "Landmark") || pngHasGeneratedNumericName(name, "Landmarke"))
        return false;
    // A full sentence is usually the unmatched remainder of a GM note, not a
    // place name. Explicit short labels such as "Abandoned tower" remain valid.
    const std::size_t words = static_cast<std::size_t>(std::count(name.begin(), name.end(), ' ')) + 1;
    if (words > 7 || (!name.empty() && (name.back() == '.' || name.back() == '!' || name.back() == '?')))
        return false;
    std::string lowered;
    lowered.reserve(name.size() + 2);
    lowered.push_back(' ');
    for (const unsigned char character : name)
        lowered.push_back(static_cast<char>(std::tolower(character)));
    lowered.push_back(' ');
    constexpr const char *noteWords[] = {
        " share ", " shares ", " shared ", " has ", " have ", " is ", " are ",
        " there ", " with ", " without ", " contains ", " include ", " includes ",
        " teilen ", " teilt ", " gibt ", " haben ", " hat ", " sind ", " ist ",
        " mit ", " ohne ", " enthält ", " enthaelt ", " soll ", " sollen "};
    return std::none_of(std::begin(noteWords), std::end(noteWords), [&](const char *word) {
        return lowered.find(word) != std::string::npos;
    });
}

void drawGeneratedMapLabels(std::vector<Pixel> &pixels, int width, int height,
                            const MapModel &map, int pixelsPerCell, bool playerSafe,
                            std::string_view locale, const gm::map_render::AtlasLabelInkMask &inkMask)
{
    if ((map.atlasLevel != MapLevel::World && map.atlasLevel != MapLevel::Region) ||
        pixelsPerCell <= 0)
        return;
    if(map.generation.recipe.generatorVersion>=11 && pixelsPerCell<5)
        return; // Match the editor's tiny-overview entity-label LOD.
    const PngLabelFont *font = pngLabelFont(gm::map_render::usesAtlasSerifTypography(map));
    if (!font)
        return; // Export remains valid when a deliberately minimal install omits fonts.
    const auto labelCorridors=map.generation.recipe.recipeVersion>=4
        ? mapPresentationCorridors(map) : std::vector<gm::map_geometry::MapRoadCorridor>{};

    std::vector<PngMapLabel> labels;
    labels.reserve(map.generation.graph.features.size());
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (!pngIsSemanticCartographicName(feature) ||
            (playerSafe && (feature.gmOnly || !feature.discovered)))
            continue;
        if (feature.kind == MapGeneratedFeatureKind::Settlement && !feature.points.empty())
        {
            const MapFeaturePoint anchor = feature.points.front();
            if (!pngMapPointIsVisibleLand(map, anchor.x, anchor.y, playerSafe))
                continue;
            int priority = feature.id == "brief_capital" ||
                                   feature.materialId.find("capital") != std::string::npos
                               ? 500
                               : (feature.locked ? 430 : 400);
            labels.push_back({&feature, PngMapLabelKind::Settlement, anchor, priority});
        }
        else if (feature.kind == MapGeneratedFeatureKind::Landmark && !feature.points.empty())
        {
            const MapFeaturePoint anchor = feature.points.front();
            if (map.atlasLevel == MapLevel::World &&
                !pngMapPointIsVisibleLand(map, anchor.x, anchor.y, playerSafe))
                continue;
            const MapCellModel *cell = mapCellAt(map, static_cast<int>(std::floor(anchor.x)),
                                                 static_cast<int>(std::floor(anchor.y)));
            if (playerSafe && (!cell || cell->fogged))
                continue;
            labels.push_back({&feature, PngMapLabelKind::Landmark, anchor, 200});
        }
        else if ((feature.kind == MapGeneratedFeatureKind::Nation ||
                  (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
                   feature.materialId.find("territory.") != std::string::npos)) &&
                 feature.points.size() >= 3)
        {
            MapFeaturePoint anchor{};
            if (pngTerritoryLabelAnchor(map, feature, playerSafe, anchor))
                labels.push_back({&feature, PngMapLabelKind::Nation, anchor, 300});
        }
    }
    std::stable_sort(labels.begin(), labels.end(), [](const PngMapLabel &left, const PngMapLabel &right) {
        if (left.priority != right.priority)
            return left.priority > right.priority;
        if (left.feature->name != right.feature->name)
            return left.feature->name < right.feature->name;
        return left.feature->id < right.feature->id;
    });

    std::vector<PngLabelBounds> occupied;
    occupied.reserve(labels.size());
    struct SettlementLabelObstacle
    {
        const MapGeneratedFeature *feature = nullptr;
        PngLabelBounds bounds;
    };
    std::vector<SettlementLabelObstacle> settlementObstacles;
    if (map.generation.recipe.recipeVersion>=5 && gm::map_render::usesIllustratedAtlas(map))
    {
        // All visible point symbols are obstacles, including unnamed landmarks
        // and regional sites. Their labels may be processed after a city name.
        for(const auto& feature:map.generation.graph.features) {
            const bool settlement=feature.kind==MapGeneratedFeatureKind::Settlement;
            if((!settlement && feature.kind!=MapGeneratedFeatureKind::Landmark) || feature.points.empty())continue;
            const auto anchor=feature.points.front();
            const auto* cell=mapCellAt(map,static_cast<int>(std::floor(anchor.x)),static_cast<int>(std::floor(anchor.y)));
            if(!cell || (playerSafe && (feature.gmOnly || !feature.discovered || cell->fogged)))continue;
            const auto box=gm::map_render::illustratedAtlasSymbolLabelBounds(anchor,settlement);
            settlementObstacles.push_back({&feature,{static_cast<int>(std::floor(box[0]*pixelsPerCell)),
                static_cast<int>(std::floor(box[1]*pixelsPerCell)),static_cast<int>(std::ceil(box[2]*pixelsPerCell)),
                static_cast<int>(std::ceil(box[3]*pixelsPerCell))}});
        }
    }
    else if (map.atlasLevel == MapLevel::World)
    {
        for (const PngMapLabel &candidate : labels)
        {
            if (candidate.kind != PngMapLabelKind::Settlement)
                continue;
            const gm::map_render::OverviewSettlementGlyphStyle glyphStyle =
                gm::map_render::overviewSettlementGlyphStyle(
                    gm::map_render::overviewSettlementGlyphKind(
                        overviewSettlementSemanticRole(*candidate.feature)));
            const int halfExtent = std::max(4, static_cast<int>(std::lround(
                pixelsPerCell * (glyphStyle.outerRadiusCells + 0.16f))));
            const int x = static_cast<int>(std::lround(candidate.anchor.x * pixelsPerCell));
            const int y = static_cast<int>(std::lround(candidate.anchor.y * pixelsPerCell));
            settlementObstacles.push_back({candidate.feature,
                {x - halfExtent, y - halfExtent, x + halfExtent, y + halfExtent}});
        }
    }
    const int maximumLabels = map.atlasLevel == MapLevel::World ? 28 : 18;
    int labelsDrawn = 0;
    for (const PngMapLabel &label : labels)
    {
        if (labelsDrawn >= maximumLabels)
            break;
        float requestedHeight = label.kind == PngMapLabelKind::Nation
                                          ? std::clamp(pixelsPerCell * 1.30f, 17.0f, 36.0f)
                                          : (label.kind == PngMapLabelKind::Settlement
                                                 ? std::clamp(pixelsPerCell * 0.98f, 15.0f, 29.0f)
                                                 : std::clamp(pixelsPerCell * 0.76f, 12.0f, 22.0f));
        std::string presentedName = gm::map_presentation::generatedName(
            label.feature->name, locale);
        if (label.kind == PngMapLabelKind::Nation && map.atlasLevel == MapLevel::World)
            std::transform(presentedName.begin(), presentedName.end(), presentedName.begin(),
                           [](unsigned char character) {
                               return static_cast<char>(std::toupper(character));
                           });
        PngTextMetrics metrics = measurePngText(*font, presentedName,
                                                      requestedHeight);
        if(label.kind==PngMapLabelKind::Nation)
        {
            requestedHeight=gm::map_render::overviewTerritoryLabelFontSize(map,float(pixelsPerCell),requestedHeight,float(metrics.width));
            if(requestedHeight<=0.f)continue;
            metrics=measurePngText(*font,presentedName,requestedHeight);
        }
        // Padding reserves collision and player-safety space for the glyph
        // halo; it is no longer rendered as an opaque UI-style plate.
        const int paddingX = std::max(3, static_cast<int>(std::round(requestedHeight * 0.18f)));
        const int paddingY = std::max(2, static_cast<int>(std::round(requestedHeight * 0.13f)));
        const int plateWidth = metrics.width + paddingX * 2;
        const int plateHeight = metrics.height + paddingY * 2;
        if (plateWidth >= width - 4 || plateHeight >= height - 4)
            continue;
        const int anchorX = static_cast<int>(std::lround(label.anchor.x * pixelsPerCell));
        const int anchorY = static_cast<int>(std::lround(label.anchor.y * pixelsPerCell));
        float symbolClearanceCells = label.kind == PngMapLabelKind::Settlement
                                         ? 1.62f
                                         : (label.kind == PngMapLabelKind::Landmark
                                                ? (gm::map_render::usesIllustratedAtlasV5(map) ? 0.85f : 0.70f)
                                                : 0.0f);
        if (label.kind == PngMapLabelKind::Settlement &&
            map.atlasLevel == MapLevel::World)
        {
            const gm::map_render::OverviewSettlementGlyphStyle glyphStyle =
                gm::map_render::overviewSettlementGlyphStyle(
                    gm::map_render::overviewSettlementGlyphKind(
                        overviewSettlementSemanticRole(*label.feature)));
            symbolClearanceCells = glyphStyle.outerRadiusCells + 0.42f;
        }
        if (label.kind == PngMapLabelKind::Settlement && (gm::map_render::usesIllustratedAtlasV4(map) || gm::map_render::usesIllustratedAtlasV5(map)))
            symbolClearanceCells=std::max(symbolClearanceCells,gm::map_render::usesIllustratedAtlasV5(map)?1.60f:1.30f);
        const int symbolClearance = static_cast<int>(std::round(
            pixelsPerCell * symbolClearanceCells));
        std::vector<std::pair<int, int>> positions;
        if (label.kind == PngMapLabelKind::Nation)
        {
            const int spreadX = std::max(plateWidth / 2, pixelsPerCell * 3);
            const int spreadY = std::max(plateHeight, pixelsPerCell * 2);
            positions = {
                {anchorX - plateWidth / 2, anchorY - plateHeight / 2},
                {anchorX - plateWidth / 2, anchorY - plateHeight / 2 - spreadY},
                {anchorX - plateWidth / 2, anchorY - plateHeight / 2 + spreadY},
                {anchorX - plateWidth / 2 - spreadX, anchorY - plateHeight / 2},
                {anchorX - plateWidth / 2 + spreadX, anchorY - plateHeight / 2}};
        }
        else
        {
            positions = {
                {anchorX - plateWidth / 2, anchorY - symbolClearance - plateHeight},
                {anchorX - plateWidth / 2, anchorY + symbolClearance},
                {anchorX + symbolClearance, anchorY - plateHeight / 2},
                {anchorX - symbolClearance - plateWidth, anchorY - plateHeight / 2},
                {anchorX + symbolClearance, anchorY - symbolClearance - plateHeight},
                {anchorX - symbolClearance - plateWidth, anchorY + symbolClearance},
                {anchorX + symbolClearance, anchorY + symbolClearance},
                {anchorX - symbolClearance - plateWidth, anchorY - symbolClearance - plateHeight}};
        }

        // A second ring gives crowded point labels clear ground beyond nearby
        // tree crowns while the shared distance score keeps clear labels close.
        if(map.generation.recipe.recipeVersion>=5 && label.kind!=PngMapLabelKind::Nation) {
            const auto nearby=positions;
            for(const auto& p:nearby) {
                const float dx=p.first+plateWidth*.5f-anchorX,dy=p.second+plateHeight*.5f-anchorY;
                const float length=std::max(.001f,std::hypot(dx,dy));
                positions.push_back({p.first+static_cast<int>(std::lround(pixelsPerCell*dx/length)),
                                     p.second+static_cast<int>(std::lround(pixelsPerCell*dy/length))});
            }
        }
        bool placed = false;
        PngLabelBounds selected{};
        float selectedTerrainCost = std::numeric_limits<float>::max();
        for (const auto &position : positions)
        {
            if(map.generation.recipe.generatorVersion>=11 && (plateWidth+4>width || plateHeight+4>height))continue;
            PngLabelBounds bounds{
                std::clamp(position.first, 2, width - plateWidth - 2),
                std::clamp(position.second, 2, height - plateHeight - 2), 0, 0};
            bounds.right = bounds.left + plateWidth;
            bounds.bottom = bounds.top + plateHeight;
            if(!gm::map_render::overviewLabelBoxFitsMap(map,
                static_cast<float>(bounds.left)/pixelsPerCell,static_cast<float>(bounds.top)/pixelsPerCell,
                static_cast<float>(bounds.right)/pixelsPerCell,static_cast<float>(bounds.bottom)/pixelsPerCell))continue;
            if (!pngLabelAreaIsPlayerSafe(map, bounds, pixelsPerCell, playerSafe))
                continue;
            const bool collision = std::any_of(occupied.begin(), occupied.end(), [&](const PngLabelBounds &other) {
                return pngLabelBoundsOverlap(bounds, other, std::max(3, pixelsPerCell / 5));
            });
            const bool symbolCollision = std::any_of(
                settlementObstacles.begin(), settlementObstacles.end(),
                [&](const SettlementLabelObstacle &obstacle) {
                    return obstacle.feature != label.feature &&
                           pngLabelBoundsOverlap(bounds, obstacle.bounds,
                                                 std::max(3, pixelsPerCell / 6));
                });
            if (collision || symbolCollision)
                continue;
            if (gm::map_render::usesIllustratedAtlasV5(map) && label.kind != PngMapLabelKind::Nation &&
                !gm::map_render::illustratedAtlasLabelClearsOwnSymbol(label.anchor,
                    static_cast<float>(bounds.left)/pixelsPerCell,static_cast<float>(bounds.top)/pixelsPerCell,
                    static_cast<float>(bounds.right)/pixelsPerCell,static_cast<float>(bounds.bottom)/pixelsPerCell,
                    label.kind == PngMapLabelKind::Settlement)) continue;
            const float terrainCost = gm::map_render::usesIllustratedAtlasV5(map)
                ? gm::map_render::illustratedAtlasLabelPlacementCost(map,label.anchor,
                    static_cast<float>(bounds.left)/pixelsPerCell,static_cast<float>(bounds.top)/pixelsPerCell,
                    static_cast<float>(bounds.right)/pixelsPerCell,static_cast<float>(bounds.bottom)/pixelsPerCell,&labelCorridors,&inkMask) : 0.f;
            if (terrainCost < selectedTerrainCost) { selected = bounds; placed = true; selectedTerrainCost = terrainCost; }
            if (terrainCost <= 0.f) break;
        }
        if (!placed)
            continue;

        if (gm::map_render::usesIllustratedAtlasV5(map) && label.kind != PngMapLabelKind::Nation)
        {
            const auto leader=gm::map_render::illustratedAtlasLabelLeader(label.anchor,
                static_cast<float>(selected.left)/pixelsPerCell,static_cast<float>(selected.top)/pixelsPerCell,
                static_cast<float>(selected.right)/pixelsPerCell,static_cast<float>(selected.bottom)/pixelsPerCell);
            if (leader.size()==2)
                drawCartographicLine(pixels,width,height,static_cast<int>(std::lround(leader[0].x*pixelsPerCell)),
                    static_cast<int>(std::lround(leader[0].y*pixelsPerCell)),static_cast<int>(std::lround(leader[1].x*pixelsPerCell)),
                    static_cast<int>(std::lround(leader[1].y*pixelsPerCell)),1,{104,94,67,150});
        }
        const Pixel textColor = (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map))
                                    ? (label.kind == PngMapLabelKind::Nation ? Pixel{51, 71, 61, 240} : Pixel{48, 48, 35, 255})
                                    : label.kind == PngMapLabelKind::Settlement
                                    ? Pixel{250, 239, 211, 255}
                                    : (label.kind == PngMapLabelKind::Nation
                                           ? Pixel{230, 219, 191, 214}
                                           : Pixel{229, 226, 204, 245});
        const int textLeft = selected.left + paddingX;
        const int textTop = selected.top + paddingY;
        const bool regionalV6 = map.atlasLevel == MapLevel::Region && map.generation.recipe.styleVersion == 6;
        const int haloRadius = regionalV6 || requestedHeight >= 18.0f ? 2 : 1;
        drawPngTextWithHalo(pixels, width, height, textLeft, textTop, *font,
                            metrics, textColor, (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map))
                                ? (regionalV6 ? Pixel{235, 230, 193, 250} : Pixel{224, 221, 178, 190}) : Pixel{29, 27, 22, 220}, haloRadius);
        occupied.push_back(selected);
        ++labelsDrawn;
    }
}


}
