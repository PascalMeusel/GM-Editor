#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapMountainCrossingAssets.hpp"

namespace gm::map_render
{
std::vector<AtlasInkPrimitive> buildAtlasCountrysideInk(const MapModel &map,
                                                        const std::vector<gm::map_geometry::MapRoadCorridor> &corridors)
{
    std::vector<AtlasInkPrimitive> ink;
    const auto ground = buildOverviewGroundProjection(map);
    const auto kind = [&](float x, float y) {
        return overviewGroundKindFromProjection(map, ground, int(std::floor(x)), int(std::floor(y)));
    };
    for (int y = 2; y < map.height - 2; y += 3)
        for (int x = 2; x < map.width - 2; x += 3)
        {
            unsigned h = unsigned(x) * 73856093u ^ unsigned(y) * 19349663u ^ map.generation.recipe.seed;
            h ^= h >> 16;
            h *= 0x7feb352du;
            h ^= h >> 15;
            if (h % 5 > 1)
                continue;
            const MapFeaturePoint centre{x + .15f + (h % 11) * .12f, y + .10f + ((h >> 8) % 13) * .10f};
            const auto k = kind(centre.x, centre.y);
            if (k != MapTileKind::Empty && k != MapTileKind::Dirt)
                continue;
            bool clear = true;
            for (const auto &site : map.generation.graph.features)
                if ((site.kind == MapGeneratedFeatureKind::Settlement ||
                     site.kind == MapGeneratedFeatureKind::Landmark) &&
                    !site.points.empty() &&
                    std::hypot(centre.x - site.points.front().x, centre.y - site.points.front().y) < 2.f)
                {
                    clear = false;
                    break;
                }
            if (!clear)
                continue;
            for (const auto &route : corridors)
                for (std::size_t i = 1; i < route.centerline.size(); ++i)
                    if (gm::map_geometry::roadPointSegmentDistanceSquared(centre, route.centerline[i - 1],
                                                                          route.centerline[i]) <
                        (route.radiusCells + .75f) * (route.radiusCells + .75f))
                        clear = false;
            if (!clear)
                continue;
            const bool fringe = kind(centre.x + 2, centre.y) == MapTileKind::Forest ||
                                kind(centre.x - 2, centre.y) == MapTileKind::Forest ||
                                kind(centre.x, centre.y + 2) == MapTileKind::Forest ||
                                kind(centre.x, centre.y - 2) == MapTileKind::Forest;
            const int count = fringe ? 5 : 3;
            for (int i = 0; i < count; ++i)
            {
                const float dx = (i - count * .5f) * .17f, dy = ((h >> (i + 1)) % 7) * .027f;
                const MapFeaturePoint p{centre.x + dx, centre.y + dy};
                if (kind(p.x, p.y) != k)
                    continue;
                ink.push_back({{{p.x - .07f, p.y - .08f}, p, {p.x + .05f, p.y - .13f}},
                               fringe ? OverviewRenderColor{107, 135, 92, 140} : OverviewRenderColor{153, 143, 95, 100},
                               .014f,
                               false});
                ink.back().minimumCellPixels = 10.f;
            }
            if (h % 3 == 0)
                ink.push_back({{{centre.x - .32f, centre.y + .17f},
                                {centre.x - .09f, centre.y + .14f},
                                {centre.x + .36f, centre.y + .18f}},
                               {169, 156, 107, 85},
                               .018f,
                               false});
        }
    return ink;
}

// Paired, fractured rock banks occupy the crossing itself; ordinary upright
// peaks remain behind them. Every instance is derived from a visible corridor,
// so editing, undo, export and player filtering share exactly the same assets.
std::vector<AtlasInkPrimitive> buildAtlasMountainCrossingInk(
    const MapModel &map,const std::vector<gm::map_geometry::MapRoadCorridor> &corridors)
{
    std::vector<AtlasInkPrimitive> ink,highlights;
    const auto ground=buildOverviewGroundProjection(map);
    const auto mountain=[&](MapFeaturePoint p){return overviewGroundKindFromProjection(map,ground,int(std::floor(p.x)),int(std::floor(p.y)))==MapTileKind::Mountain;};
    for(const auto &route:corridors) {
        const auto feature=std::find_if(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &f){return f.id==route.featureId;});
        if(feature==map.generation.graph.features.end())continue;
        const bool river=feature->kind==MapGeneratedFeatureKind::River;
        const float length=detail::overviewPathLength(route.centerline);
        for(float along=.45f;along<length-.2f;along+=1.32f) {
            auto centre=detail::sampleOverviewPath(route.centerline,along).point;
            auto a=detail::sampleOverviewPath(route.centerline,std::max(0.f,along-.35f)).point;
            auto b=detail::sampleOverviewPath(route.centerline,std::min(length,along+.35f)).point;
            const float n=std::hypot(b.x-a.x,b.y-a.y);if(n<.001f)continue;
            const float tx=(b.x-a.x)/n,ty=(b.y-a.y)/n;
            const float shoulder=route.radiusCells+1.25f;
            const auto reliefAt=[&](float distance){
                const auto p=detail::sampleOverviewPath(route.centerline,std::clamp(distance,0.f,length)).point;
                return mountain(p) || (mountain({p.x-ty*shoulder,p.y+tx*shoulder}) && mountain({p.x+ty*shoulder,p.y-tx*shoulder}));
            };
            if(!reliefAt(along))continue;
            const bool entrance=along<1.32f || !reliefAt(along-1.32f),exit=along+1.32f>=length || !reliefAt(along+1.32f);
            auto bankClearance=corridors;
            const float inner=river?std::max(.20f,feature->width*.12f+.10f):std::max(.19f,feature->width*.15f+.10f);
            for(auto &c:bankClearance)if(c.featureId==route.featureId)c.radiusCells=inner-.06f;
            const unsigned h=map.generation.recipe.seed^unsigned(centre.x*131.f)^unsigned(centre.y*971.f);
            // Broken wet-rock highlights distinguish a gorge from a dry
            // road cutting. Keep them outside the water body in the shared
            // terrain ink layer, without detached blue boulder/pond dots.
            const auto highlightStart=highlights.size();
            bool bankDrawn=false;
            if(river && feature->width>=.8f && h%3!=0) {
                std::vector<MapFeaturePoint> current;
                for(float d:{-.30f,-.08f,.17f}) {
                    auto q=detail::sampleOverviewPath(route.centerline,std::clamp(along+d,0.f,length)).point;
                    q.x-=ty*(inner+.025f);q.y+=tx*(inner+.025f);current.push_back(q);
                }
                highlights.push_back({current,{182,218,205,165},.018f,false});
            }
            for(int side:{-1,1}) {
                const auto &asset=mountainCrossingAssets[(h+(side<0?1:3))%mountainCrossingAssets.size()];
                const float nx=-ty*side,ny=tx*side;
                std::vector<MapFeaturePoint> foot,ridge;
                for(int j=0;j<7;++j) {
                    const float t=(j/6.f-.5f)*1.36f;
                    const auto p=detail::sampleOverviewPath(route.centerline,std::clamp(along+t+side*.15f,0.f,length)).point;
                    const float fade=std::min(entrance?std::clamp((t+.68f)/1.0f,0.f,1.f):1.f,
                                              exit?std::clamp((.68f-t)/1.0f,0.f,1.f):1.f);
                    foot.push_back({p.x+nx*inner,p.y+ny*inner});
                    ridge.push_back({p.x+nx*(inner+asset.depth*asset.ridge[j]*fade),p.y+ny*(inner+asset.depth*asset.ridge[j]*fade)-.18f*asset.ridge[j]*fade});
                }
                const auto valid=[&](MapFeaturePoint p){return p.x>=0 && p.y>=0 && p.x<map.width && p.y<map.height &&
                    overviewGroundKindIsLand(overviewGroundKindFromProjection(map,ground,int(p.x),int(p.y)));};
                if(!std::all_of(ridge.begin(),ridge.end(),valid) || !atlasSilhouetteClearsRoads(ridge,bankClearance,.02f))continue;
                bankDrawn=true;
                // Both banks share the atlas light direction. Opposing faces
                // have distinct value, so a pass reads as relief at overview zoom.
                const float sunlight=std::clamp(.5f+.35f*nx+.25f*ny,0.f,1.f);
                const auto face=river?OverviewRenderColor{static_cast<std::uint8_t>(116+30*sunlight),static_cast<std::uint8_t>(136+28*sunlight),static_cast<std::uint8_t>(139+17*sunlight),240}
                                     :OverviewRenderColor{static_cast<std::uint8_t>(143+39*sunlight),static_cast<std::uint8_t>(146+31*sunlight),static_cast<std::uint8_t>(125+21*sunlight),235};
                for(int j=1;j<7;++j) {
                    ink.push_back({{foot[j-1],foot[j],ridge[j]},face,0,true});
                    ink.push_back({{foot[j-1],ridge[j],ridge[j-1]},face,0,true});
                    if((river?((h>>(j+2))&1u)!=0u:j%2==0))ink.push_back({{ridge[j],{(ridge[j].x+foot[j].x)*.5f,(ridge[j].y+foot[j].y)*.5f},foot[j]},
                        {90,110,106,155},.021f,false});
                }
                ink.push_back({ridge,{105,119,98,220},.035f,false});
                ink.push_back({foot,river?OverviewRenderColor{201,213,181,175}:OverviewRenderColor{212,197,151,200},.027f,false});
            }
            // A rejected coastal/junction bank must not leave a detached wet
            // highlight floating beside the otherwise unobstructed channel.
            if(!bankDrawn)highlights.resize(highlightStart);
        }
    }
    ink.insert(ink.end(),highlights.begin(),highlights.end());
    return ink;
}

std::vector<AtlasInkPrimitive> buildAtlasValleyReliefInk(
    const MapModel &map, const std::vector<gm::map_geometry::MapRoadCorridor> &corridors)
{
    std::vector<AtlasInkPrimitive> ink;
    const auto ground = buildOverviewGroundProjection(map);
    const auto mountain = [&](MapFeaturePoint p) {
        return overviewGroundKindFromProjection(map, ground, int(std::floor(p.x)), int(std::floor(p.y))) ==
               MapTileKind::Mountain;
    };
    const auto hash = [](unsigned h) {
        h ^= h >> 16;
        h *= 0x7feb352du;
        h ^= h >> 15;
        h *= 0x846ca68bu;
        return h ^ (h >> 16);
    };
    std::vector<MapFeaturePoint> occupied;
    for (const auto &route : corridors)
    {
        const bool integrated = map.generation.recipe.recipeVersion >= 7;
        const auto feature = std::find_if(map.generation.graph.features.begin(), map.generation.graph.features.end(),
            [&](const auto &f) { return f.id == route.featureId; });
        if (integrated && feature == map.generation.graph.features.end()) continue;
        const bool river = feature != map.generation.graph.features.end() && feature->kind == MapGeneratedFeatureKind::River;
        const float width = feature != map.generation.graph.features.end() ? feature->width : 1.f;
        const float bankEdge = river ? std::max(.20f, width*.12f+.10f) : std::max(.19f, width*.15f+.10f);
        auto reliefClearance = corridors;
        if (integrated)
            for (auto &c : reliefClearance) if (c.featureId == route.featureId) c.radiusCells = bankEdge-.04f;
        const float length = detail::overviewPathLength(route.centerline);
        for (float along = .25f; along < length; along += 1.13f)
        {
            const auto p = detail::sampleOverviewPath(route.centerline, along).point;
            if (!integrated && !mountain(p))
                continue;
            const auto before = detail::sampleOverviewPath(route.centerline, std::max(0.f, along - .2f)).point;
            const auto after = detail::sampleOverviewPath(route.centerline, std::min(length, along + .2f)).point;
            const float dx = after.x - before.x, dy = after.y - before.y, n = std::hypot(dx, dy);
            if (n < .001f)
                continue;
            if (integrated && !mountain(p))
            {
                const float shoulder=route.radiusCells+1.25f;
                if (!mountain({p.x-dy/n*shoulder,p.y+dx/n*shoulder}) ||
                    !mountain({p.x+dy/n*shoulder,p.y-dx/n*shoulder})) continue;
            }
            const unsigned salt = hash(map.generation.recipe.seed ^ unsigned(std::max(0.f, p.x) * 113.f) * 73856093u ^
                                       unsigned(std::max(0.f, p.y) * 127.f) * 19349663u);
            for (int side : {-1, 1})
            {
                const unsigned h = hash(salt ^ (side < 0 ? 137u : 971u));
                if (integrated ? h % 7u == 0u : h % 5u < 2u) continue;
                // Stagger banks, keeping peaks upright in the atlas projection.
                const float offset = integrated ? bankEdge + .80f : route.radiusCells + .90f + (h % 17) * .065f;
                const MapFeaturePoint base{p.x - dy / n * side * offset + dx / n * ((h >> 8) % 13) * .027f,
                                           p.y + dx / n * side * offset};
                if ((!integrated && !mountain(base)) || std::any_of(occupied.begin(), occupied.end(), [&](auto q) {
                        return std::hypot(q.x - base.x, q.y - base.y) < .95f;
                    }))
                    continue;
                const float w = (integrated ? .68f : .50f) + ((h >> 12) % 11) * .028f,
                            height = (integrated ? .80f : .60f) + ((h >> 18) % 17) * .045f;
                const MapFeaturePoint left{base.x - w, base.y}, right{base.x + w*1.15f, base.y + .045f},
                    peak{base.x - w * .18f, base.y - height}, fold{base.x + w * .13f, base.y};
                // Match the established foothill silhouette and ink weight.
                // Several broken shoulders avoid a row of triangular markers.
                std::vector<MapFeaturePoint> outline{
                    left, {base.x-w*.84f,base.y-height*.16f}, {base.x-w*.67f,base.y-height*.38f},
                    {base.x-w*.51f,base.y-height*.34f}, {base.x-w*.33f,base.y-height*.71f},
                    {base.x-w*.17f,base.y-height*.66f}, peak,
                    {base.x+w*.17f,base.y-height*.79f}, {base.x+w*.33f,base.y-height*.70f},
                    {base.x+w*.47f,base.y-height*.40f}, {base.x+w*.63f,base.y-height*.29f},
                    {base.x+w*.84f,base.y-height*.13f}, right};
                MapFeaturePoint fittedPeak=peak, fittedFold=fold;
                if (integrated)
                {
                    // Bring distant slopes to the bank. Where a slope already
                    // crosses it, cut that slope at the bank instead of pushing
                    // the entire mountain away and reopening an empty strip.
                    const float nx=-dy/n*side, ny=dx/n*side;
                    float nearest=std::numeric_limits<float>::max();
                    for (auto q:outline) nearest=std::min(nearest,(q.x-p.x)*nx+(q.y-p.y)*ny);
                    const float contact=bankEdge+.09f;
                    const float shift=std::min(0.f,contact-nearest);
                    for (auto &q:outline) {q.x+=nx*shift;q.y+=ny*shift;}
                    fittedPeak.x+=nx*shift;fittedPeak.y+=ny*shift;
                    fittedFold.x+=nx*shift;fittedFold.y+=ny*shift;
                    const auto distance=[&](MapFeaturePoint q){return (q.x-p.x)*nx+(q.y-p.y)*ny-contact;};
                    std::vector<MapFeaturePoint> cut;
                    auto previous=outline.back();
                    for(auto current:outline) {
                        const float a=distance(previous),b=distance(current);
                        if((a>=0)!=(b>=0)) {
                            const float t=a/(a-b);
                            cut.push_back({previous.x+(current.x-previous.x)*t,previous.y+(current.y-previous.y)*t});
                        }
                        if(b>=0)cut.push_back(current);
                        previous=current;
                    }
                    outline=std::move(cut);
                    const float peakOverhang=std::min(0.f,distance(fittedPeak));
                    fittedPeak.x-=nx*peakOverhang;fittedPeak.y-=ny*peakOverhang;
                }
                if (!atlasSilhouetteClearsRoads(outline, reliefClearance, integrated ? .025f : .055f))
                    continue;
                if (!std::all_of(outline.begin(), outline.end(), [&](auto q) {
                        return q.x >= 0 && q.y >= 0 && q.x < map.width && q.y < map.height &&
                               overviewGroundKindIsLand(
                                   overviewGroundKindFromProjection(map, ground, int(q.x), int(q.y)));
                    }))
                    continue;
                for(std::size_t i=1;i<outline.size();++i)
                    ink.push_back({{fittedFold,outline[i-1],outline[i]},i<=6?OverviewRenderColor{164,172,166,255}:OverviewRenderColor{229,218,181,255},0,true});
                if (integrated)
                {
                    // Clipping can move the open ground edge into the middle
                    // of the polygon. Keep it uninked, as on the main mountains.
                    for (std::size_t i=1;i<outline.size();++i)
                        if (outline[i-1].y<fittedFold.y-.06f || outline[i].y<fittedFold.y-.06f)
                            ink.push_back({{outline[i-1],outline[i]}, {116,124,92,205}, .046f, false});
                }
                else ink.push_back({outline, {116, 124, 92, 205}, .046f, false});
                const MapFeaturePoint crease=integrated
                    ? MapFeaturePoint{fittedPeak.x*.43f+fittedFold.x*.57f,fittedPeak.y*.43f+fittedFold.y*.57f}
                    : MapFeaturePoint{base.x+w*.02f,base.y-height*.43f};
                ink.push_back({{fittedPeak,crease,fittedFold},{107,119,88,220},.04f,false});
                occupied.push_back(base);
            }
        }
    }
    return ink;
}
} // namespace gm::map_render
