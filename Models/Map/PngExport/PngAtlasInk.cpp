#include "Models/Map/PngExport/PngAtlasInk.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace MapPngExportRaster
{
void drawAtlasInk(std::vector<Pixel> &pixels, int width, int height,
                  const std::vector<gm::map_render::AtlasInkPrimitive> &ink,
                  int pixelsPerCell, float mapOffsetX, float mapOffsetY)
{
    for (const auto &primitive : ink)
    {
        if (pixelsPerCell < primitive.minimumCellPixels ||
            (primitive.maximumCellPixels > 0.f && pixelsPerCell >= primitive.maximumCellPixels)) continue;
        const auto c = primitive.color;
        const auto &p = primitive.points;
        if (primitive.filled && p.size()==3 && primitive.vertexColors.size()==3)
        {
            struct Vertex { double x,y; gm::map_render::OverviewRenderColor color; };
            Vertex v[3];
            for(int i=0;i<3;++i) v[i]={static_cast<double>(p[i].x)*pixelsPerCell-static_cast<double>(mapOffsetX)*pixelsPerCell,
                static_cast<double>(p[i].y)*pixelsPerCell-static_cast<double>(mapOffsetY)*pixelsPerCell,primitive.vertexColors[i]};
            const auto edge=[](const Vertex &a,const Vertex &b,double x,double y) {return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);};
            double area=edge(v[0],v[1],v[2].x,v[2].y);
            if (std::abs(area)<1.e-10) continue;
            if (area<0) {std::swap(v[1],v[2]);area=-area;}
            const auto topLeft=[](const Vertex &a,const Vertex &b) {return b.y<a.y || (b.y==a.y && b.x>a.x);};
            const bool tl[3]={topLeft(v[1],v[2]),topLeft(v[2],v[0]),topLeft(v[0],v[1])};
            const int minX=std::max(0,static_cast<int>(std::floor(std::min({v[0].x,v[1].x,v[2].x}))));
            const int maxX=std::min(width-1,static_cast<int>(std::ceil(std::max({v[0].x,v[1].x,v[2].x}))));
            const int minY=std::max(0,static_cast<int>(std::floor(std::min({v[0].y,v[1].y,v[2].y}))));
            const int maxY=std::min(height-1,static_cast<int>(std::ceil(std::max({v[0].y,v[1].y,v[2].y}))));
            for(int y=minY;y<=maxY;++y) for(int x=minX;x<=maxX;++x)
            {
                const double e[3]={edge(v[1],v[2],x+.5,y+.5),edge(v[2],v[0],x+.5,y+.5),edge(v[0],v[1],x+.5,y+.5)};
                bool covered=true;
                for(int i=0;i<3;++i) covered=covered && (e[i]>1.e-9 || (std::abs(e[i])<=1.e-9 && tl[i]));
                if (!covered) continue;
                const auto channel=[&](int a,int b,int c) {return static_cast<std::uint8_t>(std::clamp(std::lround((a*e[0]+b*e[1]+c*e[2])/area),0l,255l));};
                const Pixel color{channel(v[0].color.r,v[1].color.r,v[2].color.r),channel(v[0].color.g,v[1].color.g,v[2].color.g),
                    channel(v[0].color.b,v[1].color.b,v[2].color.b),channel(v[0].color.a,v[1].color.a,v[2].color.a)};
                auto &destination=pixels[static_cast<std::size_t>(y)*width+x];
                destination=alphaBlend(destination,color,255);
            }
            continue;
        }
        const auto px = [&](std::size_t i) { return static_cast<int>(std::lround(p[i].x * pixelsPerCell) - std::lround(mapOffsetX * pixelsPerCell)); };
        const auto py = [&](std::size_t i) { return static_cast<int>(std::lround(p[i].y * pixelsPerCell) - std::lround(mapOffsetY * pixelsPerCell)); };
        if (primitive.filled && p.size() == 3)
            blendTriangle(pixels, width, height, px(0), py(0), px(1), py(1), px(2), py(2), {c.r,c.g,c.b,c.a});
        else for (std::size_t i = 1; i < p.size(); ++i)
            drawCartographicLine(pixels, width, height, px(i - 1), py(i - 1), px(i), py(i),
                std::max(1, static_cast<int>(std::lround(primitive.strokeCells * pixelsPerCell))), {c.r,c.g,c.b,c.a});
    }
}

void drawOverviewCoastTreatment(std::vector<Pixel> &pixels,
                                int width,
                                int height,
                                const MapModel &map,
                                int pixelsPerCell)
{
    if (map.atlasLevel != MapLevel::World || pixelsPerCell <= 0)
        return;
    if (gm::map_render::kOverviewCoastWaterShadow.a == 0 &&
        gm::map_render::kOverviewCoastLine.a == 0 &&
        gm::map_render::kOverviewCoastLandHighlight.a == 0)
        return;
    const std::vector<gm::map_render::OverviewCoastSegment> segments =
        gm::map_render::buildOverviewCoastSegments(map);
    if (segments.empty())
        return;

    const auto asPixel = [](gm::map_render::OverviewRenderColor color) {
        return Pixel{color.r, color.g, color.b, color.a};
    };
    const auto drawPass = [&](float offsetTowardLandCells,
                              float strokeCells,
                              int minimumWidth,
                              int maximumWidth,
                              gm::map_render::OverviewRenderColor color) {
        if (color.a == 0)
            return;
        const int strokeWidth = std::clamp(
            static_cast<int>(std::lround(strokeCells * pixelsPerCell)),
            minimumWidth, maximumWidth);
        for (const gm::map_render::OverviewCoastSegment &segment : segments)
        {
            const int startX = static_cast<int>(std::lround(
                (segment.startX + segment.landNormalX * offsetTowardLandCells) * pixelsPerCell));
            const int startY = static_cast<int>(std::lround(
                (segment.startY + segment.landNormalY * offsetTowardLandCells) * pixelsPerCell));
            const int endX = static_cast<int>(std::lround(
                (segment.endX + segment.landNormalX * offsetTowardLandCells) * pixelsPerCell));
            const int endY = static_cast<int>(std::lround(
                (segment.endY + segment.landNormalY * offsetTowardLandCells) * pixelsPerCell));
            drawCartographicLine(pixels, width, height,
                                  startX, startY, endX, endY,
                                  strokeWidth, asPixel(color));
        }
    };

    drawPass(-gm::map_render::kOverviewCoastWaterOffsetCells,
             gm::map_render::kOverviewCoastWaterStrokeCells,
             1, 4, gm::map_render::kOverviewCoastWaterShadow);
    drawPass(0.0f, gm::map_render::kOverviewCoastLineStrokeCells,
             1, 2, gm::map_render::kOverviewCoastLine);
    drawPass(gm::map_render::kOverviewCoastLandOffsetCells,
             gm::map_render::kOverviewCoastLandStrokeCells,
             1, 2, gm::map_render::kOverviewCoastLandHighlight);
}


}
