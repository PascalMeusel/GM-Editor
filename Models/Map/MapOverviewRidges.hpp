#pragma once

#include "Models/Map/MapOverviewLabels.hpp"

namespace gm::map_render
{
inline std::vector<OverviewRidgePath> buildOverviewRidgePaths(
    const MapModel &map)
{
    std::vector<OverviewRidgePath> result;
    if ((map.atlasLevel != MapLevel::World && !(map.atlasLevel == MapLevel::Region &&
            map.generation.recipe.styleId == "illustrated_atlas" &&
            ((map.generation.recipe.styleVersion == 1 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v1") ||
             (map.generation.recipe.styleVersion == 2 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v2") ||
             (map.generation.recipe.styleVersion == 3 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v3") ||
         ((map.generation.recipe.styleVersion == 4 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v4") ||
          (((map.generation.recipe.styleVersion == 5 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v5") || (map.generation.recipe.styleVersion == 6 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v6"))))))) || map.width <= 0 || map.height <= 0)
        return result;
    // Generated ranges supply explicit spines, but a terrain brush only edits
    // cells. Resolve unrepresented mountain terrain into temporary ridges
    // so painting, erasing and moving an area all affect the same artwork.
    // These are render data, never additional saved/generated objects.
    std::vector<MapGeneratedFeature> ranges;
    for (const auto &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::MountainRange && feature.points.size() >= 2)
            ranges.push_back(feature);
    const auto ground = buildOverviewGroundProjection(map);
    std::vector<unsigned char> uncovered(static_cast<std::size_t>(map.width)*map.height,0);
    for (int y=0;y<map.height;++y) for (int x=0;x<map.width;++x)
    {
        if (overviewGroundKindFromProjection(map,ground,x,y) != MapTileKind::Mountain) continue;
        const MapFeaturePoint p{x+.5f,y+.5f};
        bool represented=false;
        for (const auto &range:ranges)
        {
            const float radius=std::max(2.2f,range.width*.6f+1.f);
            for (std::size_t i=1;i<range.points.size();++i)
                if (detail::politicalPointSegmentDistanceSquared(p,range.points[i-1],range.points[i]) < radius*radius)
                { represented=true;break; }
            if (represented) break;
        }
        if (!represented) uncovered[static_cast<std::size_t>(y)*map.width+x]=1;
    }
    const auto painted=[&](int x,int y) {
        return isMapCellInside(map,x,y) && uncovered[static_cast<std::size_t>(y)*map.width+x];
    };
    const bool composedPaint=map.generation.recipe.styleId=="illustrated_atlas" &&
        ((map.generation.recipe.styleVersion==5 && map.generation.recipe.styleSnapshotId=="illustrated_atlas_v5") ||
         (map.generation.recipe.styleVersion==6 && map.generation.recipe.styleSnapshotId=="illustrated_atlas_v6"));
    if(composedPaint)
    {
        const auto rank=[&](int x,int y){
            unsigned h=static_cast<unsigned>(x)*73856093u ^ static_cast<unsigned>(y)*19349663u ^
                map.generation.recipe.seed ^ 0xa6417bu;
            h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;return h^(h>>16);
        };
        const auto anchor=[&](int x,int y){
            const auto h=rank(x,y);
            return MapFeaturePoint{x+.5f+(static_cast<float>((h>>8)%101)/100.f-.5f)*.76f,
                y+.5f+(static_cast<float>((h>>17)%101)/100.f-.5f)*.76f};
        };
        // Local spacing selects a loose field of overlapping mountain groups.
        // A painted stroke has no horizontal row baseline; each short spine
        // bends independently, with its anchor remaining in the painted cell.
        for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x)
        {
            if(!painted(x,y))continue;
            const auto p=anchor(x,y);const auto h=rank(x,y);
            bool crowded=false;
            for(int ny=y-2;ny<=y+2 && !crowded;++ny)for(int nx=x-2;nx<=x+2;++nx)
            {
                if((nx==x && ny==y) || !painted(nx,ny))continue;
                const auto q=anchor(nx,ny);const auto other=rank(nx,ny);
                if((q.x-p.x)*(q.x-p.x)+(q.y-p.y)*(q.y-p.y)<1.35f*1.35f &&
                    (other<h || (other==h && (ny<y || (ny==y && nx<x)))))
                {crowded=true;break;}
            }
            if(crowded)continue;
            const float half=.77f+(h%17)*.013f;
            const float tilt=(static_cast<float>((h>>9)%101)/100.f-.5f)*.58f;
            MapGeneratedFeature range;
            range.kind=MapGeneratedFeatureKind::MountainRange;
            range.id="terrain_relief_"+std::to_string(x)+"_"+std::to_string(y);
            range.width=1.f;
            range.points={{p.x-half,p.y-tilt},{p.x,p.y-.12f},{p.x+half,p.y+tilt}};
            ranges.push_back(std::move(range));
        }
    }
    else for (int y=0;y<map.height;++y)
        for (int x=0;x<map.width;)
        {
            // Alternate rows in broad patches; retain narrow/isolated strokes
            // regardless of grid parity. Neighbouring peaks share one spine.
            const auto candidate=[&](int column) {
                return painted(column,y) && (!(y&1) || !painted(column,y-1) || !painted(column,y+1));
            };
            if (!candidate(x)) {++x;continue;}
            const int start=x;
            while(x<map.width && candidate(x)) ++x;
            MapGeneratedFeature range;
            range.kind=MapGeneratedFeatureKind::MountainRange;
            range.id="terrain_relief_"+std::to_string(start)+"_"+std::to_string(y);
            range.width=1.f;
            range.points={{start-.2f,y+.5f},{x+.2f,y+.5f}};
            ranges.push_back(std::move(range));
        }
    for (const MapGeneratedFeature &feature : ranges)
    {
        OverviewRidgePath ridge;
        ridge.spine = detail::softenOverviewPath(feature.points, 2);
        const float length = detail::overviewPathLength(ridge.spine);
        if (length < 1.2f)
            continue;

        const std::uint32_t hash = overviewStableFeatureHash(
            feature, map.generation.recipe.seed ^ 0xA24BAED5u);
        // Overlapping, uneven silhouettes follow the whole mountain spine.
        // Widely spaced, equally sized peaks read as isolated map markers.
        const float spacing = 1.30f + static_cast<float>(hash % 9u) * 0.032f;
        const float phase = spacing * (0.42f +
            static_cast<float>((hash >> 8u) % 17u) / 100.0f);
        int markIndex = 0;
        for (float distance = phase; distance < length - spacing * 0.22f;
             distance += spacing, ++markIndex)
        {
            const detail::OverviewPathSample sample =
                detail::sampleOverviewPath(ridge.spine, distance);
            if (detail::overviewRidgePointNeedsClearance(map, sample.point))
                continue;
            const int cellX = static_cast<int>(std::floor(sample.point.x));
            const int cellY = static_cast<int>(std::floor(sample.point.y));
            if (!isMapCellInside(map, cellX, cellY) ||
                !overviewGroundKindIsLand(
                    overviewGroundKindForRender(map, cellX, cellY)))
                continue;

            std::uint32_t markHash = hash ^ (static_cast<std::uint32_t>(markIndex) * 0x9E3779B9u);
            markHash ^= markHash >> 16u;
            markHash *= 0x7FEB352Du;
            markHash ^= markHash >> 15u;
            const float variation = static_cast<float>(markHash % 997u) / 996.0f;
            const float halfWidth = 0.85f + variation * 0.48f;
            const float endTaper = std::min(1.0f, std::min(distance, length - distance) / 2.3f);
            const float height = (1.20f + variation * 0.90f) * (0.65f + endTaper * 0.35f);
            const float peakDrift = (variation - 0.5f) * 0.65f;
            const float centerY = sample.point.y + std::sin(distance * 0.73f + phase) * 0.58f;
            const float baseY = centerY + height * 0.34f;
            OverviewRidgeMark mark;
            mark.leftBase = {sample.point.x - halfWidth, baseY};
            mark.rightBase = {sample.point.x + halfWidth, baseY};
            mark.peak = {sample.point.x + peakDrift,
                         centerY - height * 0.72f};
            mark.shadeEnd = {sample.point.x + peakDrift * 0.25f, baseY};
            mark.leftShoulder = {sample.point.x - halfWidth * (0.40f + variation * 0.12f),
                                  centerY - height * (0.15f + variation * 0.14f)};
            mark.rightShoulder = {sample.point.x + halfWidth * (0.28f + variation * 0.20f),
                                   centerY - height * (0.08f + variation * 0.12f)};

            const float secondarySide = ((hash + static_cast<std::uint32_t>(markIndex)) & 1u) == 0u
                                            ? 1.0f
                                            : -1.0f;
            const float secondaryCenterX = sample.point.x + secondarySide * halfWidth * 0.58f;
            const float secondaryHalfWidth = halfWidth * 0.70f;
            const float secondaryHeight = height * 0.68f;
            const float secondaryBaseY = centerY + height * 0.53f;
            mark.secondaryLeftBase = {secondaryCenterX - secondaryHalfWidth,
                                      secondaryBaseY};
            mark.secondaryRightBase = {secondaryCenterX + secondaryHalfWidth,
                                       secondaryBaseY};
            mark.secondaryPeak = {secondaryCenterX + peakDrift * 0.35f,
                                  centerY - secondaryHeight * 0.32f};
            mark.secondaryShadeEnd = {secondaryCenterX, secondaryBaseY};
            mark.emphasis = 0.82f + variation * 0.18f;
            ridge.marks.push_back(mark);
        }
        std::stable_sort(ridge.marks.begin(), ridge.marks.end(), [](const auto &left, const auto &right) {
            return left.leftBase.y < right.leftBase.y;
        });
        result.push_back(std::move(ridge));
    }
    return result;
}

} // namespace gm::map_render
