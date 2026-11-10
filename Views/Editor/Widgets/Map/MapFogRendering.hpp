#pragma once
#include "Models/Map/MapFogTexture.hpp"
#include "Models/Map/MapModels.hpp"
#include "imgui.h"

inline void drawMapFogSpan(ImDrawList *draw,ImTextureID texture,ImVec2 origin,float cellSize,
                          int x0,int y0,int x1,int y1,bool editor=false,float opacity=1.f,bool opaqueBackstop=true)
{
    const ImVec2 a(origin.x+x0*cellSize,origin.y+y0*cellSize);
    const ImVec2 b(origin.x+x1*cellSize,origin.y+y1*cellSize);
    const int alpha=static_cast<int>(std::clamp(opacity,0.f,1.f)*255);
    if(!editor && opaqueBackstop) // An independent opaque backstop also covers failed texture uploads.
        draw->AddRectFilled(a,b,IM_COL32(24,28,32,255));
    if(texture)
    {
        constexpr float period=gm::map_render::fogTexturePeriodCells;
        draw->AddImage(texture,a,b,ImVec2(x0/period,y0/period),ImVec2(x1/period,y1/period),
            IM_COL32(255,255,255,editor?alpha:255));
    }
    else if(editor)
        draw->AddRectFilled(a,b,IM_COL32(116,128,137,alpha));
}

inline void drawOpaqueMapFog(ImDrawList *draw,ImTextureID texture,ImVec2 origin,float cellSize,
    const MapModel &map,int minX,int minY,int maxX,int maxY)
{
    minX=std::max(0,minX);minY=std::max(0,minY);
    maxX=std::min(map.width-1,maxX);maxY=std::min(map.height-1,maxY);
    const auto hidden=[&](int x,int y){const auto *cell=mapCellAt(map,x,y);return !cell || cell->fogged;};
    // Keep the backstop and texture in separate batches: alternating them for
    // every span would issue hundreds of texture switches on fragmented fog.
    for(int pass=0;pass<(texture?2:1);++pass)
    for(int y=minY;y<=maxY;++y)
        for(int x=minX;x<=maxX;)
        {
            if(!hidden(x,y)){++x;continue;}
            const int start=x;while(x<=maxX && hidden(x,y))++x;
            drawMapFogSpan(draw,pass?texture:0,origin,cellSize,start,y,x,y+1,false,1.f,pass==0);
        }
}
