#pragma once

#include "Models/Map/MapIllustratedAtlasDecorations.hpp"
#include "Models/Map/MapBotanicalInk.hpp"
#include "Models/Map/MapRockEngravingInk.hpp"
#include "Models/Map/MapAtlasPolygonInk.hpp"

namespace gm::map_render
{
inline std::vector<AtlasInkPrimitive> buildIllustratedAtlasV2Ink(const MapModel &map,
    const std::vector<gm::map_geometry::MapRoadCorridor> &roadCorridors)
{
    std::vector<AtlasInkPrimitive> ink;
    const bool v3 = usesIllustratedAtlasV3(map) || (usesIllustratedAtlasV4(map) || usesIllustratedAtlasV5(map));
    const bool v4 = (usesIllustratedAtlasV4(map) || usesIllustratedAtlasV5(map));
    const bool v5 = usesIllustratedAtlasV5(map);
    const auto atlasRidges = buildOverviewRidgePaths(map);
    auto roads = roadCorridors;
    // Water occupies a valley, not the face of an illustrated summit. The
    // plan passes only geometry that belongs to the current visible layers.
    for (const auto &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::River && feature.points.size() >= 2 &&
            std::none_of(roads.begin(),roads.end(),[&](const auto &corridor){ return corridor.featureId == feature.id; }))
            roads.push_back({feature.id,illustratedAtlasFinalRiverPoints(map,feature),
                             std::max(.62f,feature.width*.12f+.24f)});
    const auto hashAt = [&](int x, int y, unsigned salt = 0u) {
        unsigned h = static_cast<unsigned>(x)*73856093u ^ static_cast<unsigned>(y)*19349663u ^
                     map.generation.recipe.seed ^ salt;
        h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu;
        return h ^ (h >> 16);
    };
    const auto triangle = [&](MapFeaturePoint a, MapFeaturePoint b, MapFeaturePoint c, OverviewRenderColor color) {
        ink.push_back({{a,b,c},color,0,true});
    };
    const auto stroke = [&](std::vector<MapFeaturePoint> p, OverviewRenderColor c, float width = .045f) {
        ink.push_back({std::move(p),c,width,false});
    };
    const auto land = [&](MapFeaturePoint p) {
        const int x = static_cast<int>(std::floor(p.x)), y = static_cast<int>(std::floor(p.y));
        return isMapCellInside(map,x,y) && overviewGroundKindIsLand(overviewGroundKindForRender(map,x,y));
    };
    const auto clear = [&](const std::vector<MapFeaturePoint> &p) {
        return std::all_of(p.begin(),p.end(),land) && atlasSilhouetteClearsRoads(p,roads,.055f);
    };
    // Small marks describe actual ground. Their irregular groups leave quiet
    // areas for labels and routes instead of repeating a texture over the map.
    for (int y = 1; y < map.height-1; ++y)
        for (int x = 1; x < map.width-1; ++x)
        {
            const unsigned h = hashAt(x,y);
            const auto kind = overviewGroundKindForRender(map,x,y);
            const bool composedRegion = v5 && map.atlasLevel == MapLevel::Region;
            const bool rolling = !composedRegion && kind == MapTileKind::Empty &&
                                 illustratedAtlasElevation(map,x+.5f,y+.5f) >= .60f;
            if (h % (rolling || (composedRegion && kind == MapTileKind::Hill) ? 8 : 13) != 0 || !overviewGroundKindIsLand(kind) || kind == MapTileKind::Forest ||
                kind == MapTileKind::Mountain || kind == MapTileKind::Stone) continue;
            const float cx = x+.25f+((h>>8)%100)*.005f, cy = y+.5f;
            if (detail::overviewForestPointNeedsClearance(map,{cx,cy}) ||
                !clear({{cx-.7f,cy-.5f},{cx+.75f,cy-.5f},{cx+.75f,cy+.4f},{cx-.7f,cy+.4f}})) continue;
            if (kind == MapTileKind::Hill || kind == MapTileKind::Desert || rolling)
            {
                stroke({{cx-.7f,cy+.17f},{cx-.48f,cy-.05f},{cx-.18f,cy-.20f},{cx+.03f,cy-.19f},
                        {cx+.32f,cy-.05f},{cx+.62f,cy+.18f}}, {139,143,105,155},.04f);
                stroke({{cx-.15f,cy+.22f},{cx+.10f,cy+.08f},{cx+.38f,cy+.09f},{cx+.72f,cy+.35f}}, {155,151,112,130},.03f);
            }
            else if (kind == MapTileKind::Swamp)
            {
                stroke({{cx-.57f,cy+.18f},{cx+.44f,cy+.18f}},{90,127,108,150});
                stroke({{cx-.37f,cy+.32f},{cx+.60f,cy+.32f}},{90,127,108,115},.03f);
                for (float dx : {-.3f,.02f,.31f})
                    stroke({{cx+dx-.1f,cy-.17f},{cx+dx,cy+.13f},{cx+dx+.04f,cy-.28f}},{84,119,90,190},.04f);
            }
            else if (h % 3 == 0)
            {
                for (int blade = 0; blade < 3; ++blade)
                {
                    const float dx = -.3f+blade*.25f, dy = blade == 1 ? -.1f : .04f;
                    stroke({{cx+dx-.07f,cy+dy-.13f},{cx+dx,cy+dy},{cx+dx+.09f,cy+dy-.18f}}, {143,151,109,110},.03f);
                }
            }
        }
    // These bounds depend only on this invocation's reviewed artwork. Keep
    // feature order followed by ridge/mark order, with the original arithmetic.
    struct CanopyExclusion { float left, right, top, bottom; };
    std::vector<CanopyExclusion> canopyExclusions;
    if (v5)
    {
        for (const auto &feature : map.generation.graph.features)
        {
            if (feature.points.empty() || (feature.kind != MapGeneratedFeatureKind::Settlement &&
                feature.kind != MapGeneratedFeatureKind::Landmark)) continue;
            const auto a=feature.points.front();
            const bool town=feature.kind==MapGeneratedFeatureKind::Settlement;
            const float half=town?.88f:.67f, above=town?1.43f:.74f, below=town?.45f:.70f;
            canopyExclusions.push_back({a.x-half,a.x+half,a.y-above,a.y+below});
        }
        for (const auto &ridge : atlasRidges)
            for (const auto &mark : ridge.marks)
            {
                const float half = (mark.rightBase.x-mark.leftBase.x)*.76f;
                const float center = (mark.rightBase.x+mark.leftBase.x)*.5f;
                canopyExclusions.push_back({center-half*1.9f,center+half*1.9f,
                                           mark.peak.y-.5f,mark.leftBase.y+.7f});
            }
    }
    // Jittered canopy spacing is finer than gameplay cells. Terrain semantics
    // choose the grove boundary; zoom changes neither placement nor clearance.
    const float rowSpacing = v5 ? .72f : v3 ? .80f : .64f, colSpacing = v5 ? .78f : v3 ? .89f : .70f;
    const auto treeAnchor=[&](int col,int row){
        const unsigned h=hashAt(col,row,0x6391b7a5u);
        const float gridX=col*colSpacing+(row%2)*colSpacing*.5f+.32f;
        const float gridY=row*rowSpacing+.32f;
        // Broad drift composes groves; independent offsets dissolve the
        // residual horizontal planting rows without changing forest cells.
        return MapFeaturePoint{gridX+(static_cast<float>((h>>8)%100)-50.f)*(v5?.0082f:.0054f)+
                (v5?.20f*std::sin(gridY*.67f+gridX*.23f+map.generation.recipe.seed%37):0.f),
            gridY+(static_cast<float>((h>>16)%100)-50.f)*(v5?.0080f:.0048f)+
                (v5?.23f*std::sin(gridX*.59f-gridY*.19f+map.generation.recipe.seed%53):0.f)};
    };
    for (int row = 0; row*rowSpacing < map.height; ++row)
        for (int col = 0; col*colSpacing < map.width; ++col)
        {
            const unsigned h = hashAt(col,row,0x6391b7a5u);
            const auto anchor=treeAnchor(col,row);
            const float x=anchor.x,y=anchor.y;
            const int tx = static_cast<int>(std::floor(x)), ty = static_cast<int>(std::floor(y));
            if (!isMapCellInside(map,tx,ty) || overviewGroundKindForRender(map,tx,ty) != MapTileKind::Forest ||
                detail::overviewForestPointNeedsClearance(map,{x,y},!v5 || (map.atlasLevel == MapLevel::Region && map.generation.recipe.styleVersion == 6))) continue;
            if(v5)
            {
                bool crowded=false;
                for(int nr=std::max(0,row-2);nr<=row+2 && !crowded;++nr)
                    for(int nc=std::max(0,col-2);nc<=col+2;++nc)
                    {
                        if((nc==col && nr==row) || nr*rowSpacing>=map.height || nc*colSpacing>=map.width)continue;
                        const unsigned other=hashAt(nc,nr,0x6391b7a5u);
                        if(other>h || (other==h && (nr>row || (nr==row && nc>col))))continue;
                        const auto p=treeAnchor(nc,nr);
                        const int px=static_cast<int>(std::floor(p.x)),py=static_cast<int>(std::floor(p.y));
                        if(isMapCellInside(map,px,py) && overviewGroundKindForRender(map,px,py)==MapTileKind::Forest &&
                            (p.x-x)*(p.x-x)+(p.y-y)*(p.y-y)<.33f*.33f)
                        {crowded=true;break;}
                    }
                if(crowded)continue;
            }
            // Continuous canopy openings and mixed species avoid square biome
            // blocks while retaining the semantic forest underneath.
            const float grove = std::sin(x*.43f+std::sin(y*.29f)*1.8f+map.generation.recipe.seed%61)*
                                std::cos(y*.39f+std::sin(x*.31f)*1.3f);
            if (v3 && (grove > (v5 ? .78f : .60f) || (grove > (v5 ? .55f : .22f) && h%4 == 0))) continue;
            float s = v3 ? .78f+(h%100)*.0070f : .85f+(h%100)*.0041f;
            const auto canopyFits = [&](float scale) {
                const std::vector<MapFeaturePoint> box{{x-scale*.49f,y-scale*.90f},{x+scale*.49f,y-scale*.90f},
                                                       {x+scale*.49f,y+scale*.18f},{x-scale*.49f,y+scale*.18f}};
                if (!clear(box)) return false;
                if (!v5) return true;
                // Test the complete canopy against the actual point-symbol
                // footprint rather than clearing a circle around the whole grove.
                for (const auto &exclusion : canopyExclusions)
                    if (box[0].x<exclusion.right && box[1].x>exclusion.left &&
                        box[0].y<exclusion.bottom && box[2].y>exclusion.top) return false;
                return true;
            };
            if (!canopyFits(s))
            {
                if (!v5) continue;
                // Smaller edge trees retain a coherent grove in narrow wooded
                // corridors without painting across roads, water or symbols.
                s *= .62f;
                if (!canopyFits(s)) continue;
            }
            const bool pine = v3 ? (static_cast<int>(h%100) < 35+static_cast<int>(25.f*std::sin(x*.17f+y*.12f)))
                                 : hashAt(tx/6,ty/6,71u)%5 < 2;
            const OverviewRenderColor dark = v3 ? OverviewRenderColor{49,79,61,225} : OverviewRenderColor{66,95,68,220};
            const OverviewRenderColor leaf = v4 ? (pine ? OverviewRenderColor{105,153,125,255} : h%9==0 ? OverviewRenderColor{173,178,105,255} : OverviewRenderColor{static_cast<std::uint8_t>(112+h%23),static_cast<std::uint8_t>(158+h%17),static_cast<std::uint8_t>(105+h%19),255}) : v3 ? OverviewRenderColor{static_cast<std::uint8_t>(132+h%21),static_cast<std::uint8_t>(160+h%18),static_cast<std::uint8_t>(106+h%16),255} : OverviewRenderColor{static_cast<std::uint8_t>(126+h%15),static_cast<std::uint8_t>(149+h%14),static_cast<std::uint8_t>(99+h%12),255},
                shade = v4 ? OverviewRenderColor{58,111,86,255} : v3 ? OverviewRenderColor{79,119,86,255} : OverviewRenderColor{98,129,85,255};
            stroke({{x,y-.13f*s},{x-.015f*s,y+.18f*s}},dark,.04f);
            std::vector<MapFeaturePoint> crown;
            MapFeaturePoint center{x,y-.36f*s};
            if (pine)
            {
                for (const auto p : std::vector<MapFeaturePoint>{{0,-.90f},{-.15f,-.67f},{-.09f,-.67f},
                    {-.30f,-.42f},{-.19f,-.44f},{-.43f,-.12f},{-.22f,-.15f},{-.36f,.015f},
                    {0,.08f},{.36f,.015f},{.22f,-.15f},{.43f,-.12f},{.19f,-.44f},
                    {.30f,-.42f},{.09f,-.67f},{.15f,-.67f}})
                    crown.push_back({x+p.x*s,y+p.y*s});
            }
            else
                for (int i = 0; i < 48; ++i)
                {
                    const float a = i*6.2831853f/48.f;
                    const float r = v4 ? .88f+.065f*std::cos(a*(5+h%2)+(h%7)*.73f)+.04f*std::sin(a*3.f+(h%31)*.21f) : v3 ? .90f+.05f*std::cos(a*(4+h%2)+(h%7)*.73f)+.05f*std::sin(a*3.f+(h%31)*.21f)
                        : .84f + .09f*std::cos(a*(5+h%3)+(h%7)*.73f)+.07f*std::sin(a*3.f+(h%31)*.21f);
                    crown.push_back({center.x+std::cos(a)*s*(v3?.47f:.46f)*r,center.y+std::sin(a)*s*(v3?.40f:.48f)*r});
                }
            if (map.generation.recipe.recipeVersion >= 10)
            {
                appendBotanicalCanopyInk(ink,crown,center,.47f*s,.48f*s,h,pine,false);
                continue;
            }
            for (std::size_t i = 0; i < crown.size(); ++i)
                triangle(center,crown[i],crown[(i+1)%crown.size()],(pine || (v3 && !v4)) && crown[i].x < x-.06f*s ? shade : leaf);
            if (v4 && !pine)
            {
                // A crescent follows the crown edge; a vertical half-disc
                // split made every broadleaf tree resemble the same token.
                std::vector<MapFeaturePoint> crescent;
                for (int i=6;i<=30;++i) crescent.push_back(crown[i]);
                for (int i=30;i>=6;--i)
                    crescent.push_back({center.x+(crown[i].x-center.x)*.63f,
                                        center.y+(crown[i].y-center.y)*.63f});
                appendAtlasPolygonInk(ink,crescent,shade);
            }
            crown.push_back(crown.front());
            stroke(crown,dark,.038f);
            if (!pine)
            {
                stroke({{x-.25f*s,y-.20f*s},{x-.23f*s,y-.13f*s},{x-.17f*s,y-.10f*s},
                        {x-.11f*s,y-.12f*s},{x-.05f*s,y-.10f*s}}, {71,105,72,185},.032f);
                stroke({{x+.06f*s,y-.63f*s},{x+.13f*s,y-.65f*s},{x+.22f*s,y-.60f*s}}, {193,202,148,190},.035f);
            }
        }

    if (map.generation.recipe.recipeVersion == 8) {
        auto landforms = buildIllustratedAtlasMountainPassInk(map,roads);
        ink.insert(ink.end(),landforms.begin(),landforms.end());
    }
    struct Summit { float x,y,width,height; unsigned hash; bool main; };
    const auto mountainInkStart = ink.size();
    std::vector<AtlasMountainFootprint> mountainFootprints;
    std::vector<Summit> summits;
    for (const auto &ridge : atlasRidges)
        for (const auto &m : ridge.marks)
        {
            const float x = (m.leftBase.x+m.rightBase.x)*.5f;
            const unsigned h = hashAt(static_cast<int>(x*79),static_cast<int>(m.leftBase.y*83),0xa64bu);
            const float w = (m.rightBase.x-m.leftBase.x)*(.56f+(h%19)*.01f)*(v5 ? 1.15f : 1.f);
            const float height = (m.leftBase.y-m.peak.y)*(v5 ? .69f+((h>>8)%47)*.012f : v4 ? 1.04f+((h>>8)%31)*.019f : 1.0f+((h>>8)%31)*.012f);
            // Broad, low shoulders connect the high crest to surrounding land.
            summits.push_back({x-w*.79f,m.leftBase.y+.30f,w*(v3?.72f:.91f),height*(v5 ? .47f+((h>>17)%29)*.012f : v3?.68f:.56f),h^19u,false});
            summits.push_back({x+w*.87f,m.leftBase.y+.38f,w*(v3?.78f:.93f),height*(v5 ? .50f+((h>>21)%29)*.012f : v3?.74f:.63f),h^53u,false});
            summits.push_back({x,m.leftBase.y,w,height,h,true});
        }
    std::stable_sort(summits.begin(),summits.end(),[](const auto &a,const auto &b){ return a.y < b.y; });
    for (const auto &m : summits)
    {
        const float drift = (static_cast<float>(m.hash%101)/100.f-.5f)*.30f;
        std::vector<MapFeaturePoint> shape{{-1,0},{-.82f,-.10f},{-.64f,-.34f},{-.53f,-.29f},
            {-.32f,-.65f},{-.20f,-.61f},{drift,-1},{.11f+drift,-.84f},{.26f,-.72f},
            {.34f,-.49f},{.58f,-.34f},{.76f,-.11f},{1,0}};
        if (!m.main)
            shape = {{-1,0},{-.86f,-.26f},{-.68f,-.55f},{-.52f,-.73f},{-.35f,-.89f},{-.17f,-.97f},
                     {drift,-1},{.22f,-.92f},{.39f,-.76f},{.57f,-.55f},{.73f,-.30f},{.88f,-.08f},{1,0}};
        if (v3 && !m.main)
            shape = {{-1,0},{-.84f,-.16f},{-.67f,-.38f},{-.51f,-.34f},{-.33f,-.71f},{-.17f,-.66f},
                     {drift,-1},{.17f,-.79f},{.33f,-.70f},{.47f,-.40f},{.63f,-.29f},{.84f,-.13f},{1,0}};
        else if (m.hash%3 == 0 && m.main)
        {
            shape[2] = {-.67f,-.48f}; shape[3] = {-.49f,-.39f}; shape[4] = {-.34f,-.79f};
            shape[5] = {-.21f,-.71f}; shape[8] = {.34f,-.82f}; shape[9] = {.47f,-.49f};
        }
        else if (m.hash%3 == 1 && m.main)
        {
            shape[2] = {-.72f,-.24f}; shape[3] = {-.48f,-.43f}; shape[4] = {-.35f,-.51f};
            shape[5] = {-.24f,-.83f}; shape[8] = {.29f,-.56f}; shape[9] = {.49f,-.62f};
        }
        std::vector<MapFeaturePoint> silhouette;
        for (const auto p : shape) silhouette.push_back({m.x+p.x*m.width,m.y+p.y*m.height});
        if (!clear(silhouette)) continue;
        if (v5)
        {
            // Only reviewed point features reach this artwork map. A private
            // landmark must not disclose itself through a gap in public relief.
            bool coversPoint = false;
            for (const auto &feature : map.generation.graph.features)
            {
                if ((feature.kind != MapGeneratedFeatureKind::Settlement &&
                     feature.kind != MapGeneratedFeatureKind::Landmark) || feature.points.empty()) continue;
                const auto anchor = feature.points.front();
                for (const auto offset : {MapFeaturePoint{0,0},MapFeaturePoint{-.65f,0},
                                         MapFeaturePoint{.65f,0},MapFeaturePoint{0,-.75f},MapFeaturePoint{0,.55f}})
                    coversPoint = coversPoint || atlasPointInsidePolygon(silhouette,{anchor.x+offset.x,anchor.y+offset.y});
                if (coversPoint) break;
            }
            if (coversPoint) continue;
        }
        if (map.generation.recipe.recipeVersion >= 9)
            mountainFootprints.push_back({silhouette,{m.x,m.y},m.width,m.height,m.hash,m.main});
        appendAtlasPolygonInk(ink,silhouette,v4 ? OverviewRenderColor{235,220,191,255} : v3 ? OverviewRenderColor{234,226,195,255} : OverviewRenderColor{229,218,182,255});
        // Interrupted slope contours and uneven hatching replace the flat
        // triangular facets of V1. Every point stays within the tested relief.
        const auto leftAt = [&](float t) {
            const float y = m.y-t*m.height;
            float x = m.x+drift*m.width;
            bool found = false;
            for (std::size_t i = 1; i <= 6; ++i)
            {
                const auto a = silhouette[i-1], b = silhouette[i];
                if (y < std::min(a.y,b.y) || y > std::max(a.y,b.y) || std::abs(a.y-b.y)<.0001f) continue;
                const float candidate = a.x+(b.x-a.x)*(y-a.y)/(b.y-a.y);
                if (!found || candidate < x) x = candidate;
                found = true;
            }
            return MapFeaturePoint{x,y};
        };
        const auto foldAt = [&](float t) {
            const float bend = std::sin(t*8.f)*.075f*(1.f-t);
            return MapFeaturePoint{m.x+(drift*t+.20f*(1.f-t)+bend)*m.width,m.y-t*m.height};
        };
        std::vector<MapFeaturePoint> shadow(silhouette.begin(),silhouette.begin()+7);
        for (int band = 11; band >= 0; --band) shadow.push_back(foldAt(band/12.f));
        appendAtlasPolygonInk(ink,shadow,v4 ? (m.main ? OverviewRenderColor{130,142,157,255} : OverviewRenderColor{164,172,166,255}) : v3 ? (m.main ? OverviewRenderColor{139,153,139,255} : OverviewRenderColor{163,172,147,255}) :
                    m.main ? OverviewRenderColor{173,172,140,255} : OverviewRenderColor{193,190,153,255});
        stroke(silhouette,m.main ? OverviewRenderColor{77,86,64,240} : OverviewRenderColor{116,124,92,205},m.main?.064f:.046f);
        std::vector<MapFeaturePoint> fold;
        for (int i = 10; i >= 1; --i) fold.push_back(foldAt(i/10.f));
        stroke(fold,{107,119,88,220},.04f);
        for (int hatch = 1; hatch < (m.main ? 9 : 5); ++hatch)
        {
            const float t = .24f+hatch*.073f;
            auto a = leftAt(t);
            a.x += .085f*m.width; a.y += .025f*m.height;
            const auto lower = leftAt(t-.23f), foldEnd = foldAt(t-.29f);
            const MapFeaturePoint b{lower.x*.63f+foldEnd.x*.37f,foldEnd.y};
            bool contained = true;
            for (int sample = 0; sample <= 12; ++sample)
            {
                const float t2 = sample/12.f;
                if (!atlasPointInsidePolygon(silhouette,{a.x+(b.x-a.x)*t2,a.y+(b.y-a.y)*t2})) { contained = false; break; }
            }
            if (contained) stroke({a,b}, {113,126,95,m.main ? static_cast<std::uint8_t>(150) : static_cast<std::uint8_t>(95)},.027f);
        }
        if(map.generation.recipe.recipeVersion>=10)
            appendAtlasRockEngravingInk(ink,silhouette,{m.x,m.y},m.width,m.height,m.hash,m.main);
        if (v4 && m.main)
        {
            // Snow follows the actual peak polygon. Its broken lower edge
            // creates a crest, rather than a repeated isolated white triangle.
            std::vector<MapFeaturePoint> cap;
            const float snowLine=.68f+(m.hash%5)*.025f;
            const float cutoff=m.y-snowLine*m.height;
            for (std::size_t i=1;i<silhouette.size();++i)
            {
                const auto a=silhouette[i-1],b=silhouette[i];
                if ((a.y<cutoff)!=(b.y<cutoff))
                {
                    const float t=(cutoff-a.y)/(b.y-a.y);
                    cap.push_back({a.x+(b.x-a.x)*t,cutoff});
                }
                if (b.y<cutoff) cap.push_back(b);
            }
            if (cap.size()>=3)
            {
                const auto l=cap.front(),r=cap.back();
                const MapFeaturePoint notch{l.x*.43f+r.x*.57f,cutoff-.065f*m.height};
                cap.push_back(notch);
                appendAtlasPolygonInk(ink,cap,{247,240,221,255});
                stroke({l,notch,r},{168,178,180,255},.028f);
            }
        }
        else if (m.main)
        {
            const auto a = silhouette[5], peak = silhouette[6], b = silhouette[7];
            const MapFeaturePoint l{peak.x+(a.x-peak.x)*.46f,peak.y+(a.y-peak.y)*.46f};
            const MapFeaturePoint r{peak.x+(b.x-peak.x)*.74f,peak.y+(b.y-peak.y)*.74f};
            triangle(peak,l,r,{245,238,211,255});
        }
    }
    if (map.generation.recipe.recipeVersion >= 9) {
        auto buttresses=buildAtlasMountainButtressInk(map,roads,mountainFootprints);
        ink.insert(ink.begin()+mountainInkStart,buttresses.begin(),buttresses.end());
    }
    appendIllustratedAtlasWorldCompass(ink, map);
    appendIllustratedAtlasCountrysideAndPasses(ink, map, roads, roadCorridors);
    return ink;
}

} // namespace gm::map_render
