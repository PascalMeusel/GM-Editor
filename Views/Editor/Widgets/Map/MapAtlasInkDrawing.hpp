#pragma once

#include "Models/Map/MapIllustratedAtlas.hpp"
#include "imgui.h"

inline void drawMapIllustratedAtlasInk(ImDrawList *drawList, ImVec2 origin, float cellSize,
    const std::vector<gm::map_render::AtlasInkPrimitive> &ink, bool clipGeometry = true)
{
    if (!drawList || cellSize <= .05f) return;
    std::vector<ImVec2> points;
    points.reserve(64);
    const auto clipMin=drawList->GetClipRectMin(),clipMax=drawList->GetClipRectMax();
    for (const auto &primitive : ink)
    {
        if (cellSize < primitive.minimumCellPixels ||
            (primitive.maximumCellPixels > 0.f && cellSize >= primitive.maximumCellPixels)) continue;
        // Keep silhouettes at overview zoom; omit only subpixel fine strokes.
        // Previously crossing four pixels/cell erased every cave/local wall.
        if (!primitive.filled && primitive.strokeCells * cellSize < .28f && cellSize < 4.f) continue;
        const auto c = primitive.color;
        if (primitive.filled && primitive.points.size() == 3)
        {
            // These are an already tessellated surface, not three separate
            // silhouette edges. ImGui's convex-fill AA adds a transparent
            // fringe to EVERY triangle: internal edges then become visible
            // cracks, and skinny cave triangles produce long radial streaks.
            // Surface outlines are separate strokes and retain their own AA.
            const auto point=[&](size_t i){return ImVec2(origin.x+primitive.points[i].x*cellSize,origin.y+primitive.points[i].y*cellSize);};
            const auto a=point(0),b=point(1),d=point(2);
            if(clipGeometry && (std::max({a.x,b.x,d.x})<clipMin.x || std::min({a.x,b.x,d.x})>clipMax.x ||
               std::max({a.y,b.y,d.y})<clipMin.y || std::min({a.y,b.y,d.y})>clipMax.y)) continue;
            if (primitive.vertexColors.size()==3)
            {
                // Adjacent mesh triangles share their exact edge and vertex
                // color. AddTriangleFilled would insert an AA fringe between
                // them; raw vertices leave antialiasing to the outer mesh.
                const ImVec2 uv=ImGui::GetIO().Fonts->TexUvWhitePixel;
                drawList->PrimReserve(3,3);
                const ImDrawIdx base=static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
                drawList->PrimWriteIdx(base); drawList->PrimWriteIdx(base+1); drawList->PrimWriteIdx(base+2);
                for (int i=0;i<3;++i) {
                    const auto color=primitive.vertexColors[i];
                    drawList->PrimWriteVtx(point(i),uv,IM_COL32(color.r,color.g,color.b,color.a));
                }
                continue;
            }
            const ImDrawListFlags savedFlags = drawList->Flags;
            drawList->Flags &= ~ImDrawListFlags_AntiAliasedFill;
            drawList->AddTriangleFilled(a,b,d,IM_COL32(c.r,c.g,c.b,c.a));
            drawList->Flags = savedFlags;
            continue;
        }
        points.clear();
        float minX=FLT_MAX,minY=FLT_MAX,maxX=-FLT_MAX,maxY=-FLT_MAX;
        for (const auto p : primitive.points)
        {
            const ImVec2 point(origin.x + p.x * cellSize, origin.y + p.y * cellSize);
            points.push_back(point);
            if(clipGeometry){minX=std::min(minX,point.x);minY=std::min(minY,point.y);
                maxX=std::max(maxX,point.x);maxY=std::max(maxY,point.y);}
        }
        const float margin=std::max(1.f,primitive.strokeCells*cellSize);
        if(clipGeometry && (maxX+margin<clipMin.x || minX-margin>clipMax.x || maxY+margin<clipMin.y || minY-margin>clipMax.y))continue;
        if (points.size() >= 2)
            drawList->AddPolyline(points.data(), static_cast<int>(points.size()), IM_COL32(c.r,c.g,c.b,c.a),
                ImDrawFlags_None, std::max(.65f, primitive.strokeCells * cellSize));
    }
}
