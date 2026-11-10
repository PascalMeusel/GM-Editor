#pragma once

#include "Models/Map/MapModels.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

// V11 creation silhouettes. The same normalized mask drives the small creation
// preview and the generated semantic terrain; noise may refine, never replace,
// the selected geographical arrangement. Positive values are land.
inline float mapWorldLandformScore(MapLandform landform, std::uint32_t seed,
                                  float x, float y, int waterLevel, bool regionalV12 = false)
{
    constexpr float pi = 3.14159265358979323846f;
    const auto random = [seed](std::uint32_t key) {
        std::uint32_t h = seed ^ (key * 0x9e3779b9u);
        h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15;
        h *= 0x846ca68bu; h ^= h >> 16;
        return static_cast<float>(h & 0x00ffffffu) / 16777215.f;
    };
    const float phase = random(1) * 2.f * pi;
    const float water = std::clamp(waterLevel, 0, 100) / 100.f;
    if (landform == MapLandform::Continental) return 1.f;
    if (landform == MapLandform::SingleIsland)
    {
        const float angle = random(2)*pi;
        const float rx=x*std::cos(angle)-y*std::sin(angle);
        const float ry=x*std::sin(angle)+y*std::cos(angle);
        const float radius=.76f-water*.17f;
        const float dx=rx/radius,dy=ry/(radius*(.79f+random(3)*.12f));
        const float theta=std::atan2(dy,dx);
        const float shore=1.f+.14f*std::sin(theta*3.f+phase)+.06f*std::cos(theta*5.f-phase);
        return std::min(shore-std::hypot(dx,dy),(.94f-std::max(std::abs(x),std::abs(y)))*5.f);
    }
    if (landform == MapLandform::Coast || landform == MapLandform::Peninsula)
    {
        // Quarter turns keep one known landward edge and one seaward edge.
        // A changing seed changes orientation and bays, not the chosen type.
        switch (seed%4u) {
        case 1: { const float old=x; x=y; y=-old; break; }
        case 2: x=-x; y=-y; break;
        case 3: { const float old=x; x=-y; y=old; break; }
        default: break;
        }
        if (landform == MapLandform::Coast)
            return (.18f-water*.36f)+.15f*std::sin(y*3.4f+phase)
                +.07f*std::sin(y*7.f-phase)-x;
        const float mainland=-.56f-water*.12f+.045f*std::sin(y*5.f+phase)-x;
        const float length=.72f-water*.16f;
        const float along=std::clamp((x+.70f)/(length+.70f),0.f,1.f);
        const float center=.12f*std::sin(along*3.8f+phase);
        const float halfWidth=(.37f-water*.07f)*(1.f-.55f*along);
        const float tongue=std::min(length-x,halfWidth-std::abs(y-center));
        return std::max(mainland,tongue);
    }
    if (landform == MapLandform::InlandLake)
    {
        const float cx = (random(2) - .5f) * (regionalV12 ? .34f : .20f);
        const float cy = (random(3) - .5f) * (regionalV12 ? .28f : .16f);
        const float angle = (random(4) - .5f) * (regionalV12 ? 2.4f : .8f);
        const float dx = x-cx, dy = y-cy;
        const float rx = dx*std::cos(angle)-dy*std::sin(angle);
        const float ry = dx*std::sin(angle)+dy*std::cos(angle);
        // New regional designs include long glacial basins and two-lobed
        // shorelines. World and stored V11 recipes retain their exact mask.
        const float radiusX = regionalV12 ? std::min(.58f,(.40f+water*.22f)*(.86f+random(5)*.16f)) : .40f + water*.22f;
        const float radiusY = regionalV12 ? (.29f+water*.18f)*(.78f+random(6)*.28f) : .29f + water*.18f;
        const float theta = std::atan2(ry/radiusY,rx/radiusX);
        const float shore = regionalV12
            ? 1.f+.17f*std::sin(theta*2.f+phase)+.075f*std::cos(theta*3.f-phase)
            : 1.f + .13f*std::sin(theta*3.f+phase)+.06f*std::cos(theta*5.f-phase);
        return std::sqrt(rx*rx/(radiusX*radiusX)+ry*ry/(radiusY*radiusY))-shore;
    }
    if (landform != MapLandform::Islands) return 1.f;
    // Closed archipelagos retain water around the frame. Gently compress the
    // outer tips before applying the final margin, rather than clipping a
    // populated island and producing an off-map land road.
    const float edgeX=std::max(0.f,std::abs(x)-.82f);
    const float edgeY=std::max(0.f,std::abs(y)-.82f);
    const float frameMargin=(.95f-std::max(std::abs(x),std::abs(y)))*5.f;
    float score = -4.f;
    const float mirror = seed&1u ? -1.f : 1.f;
    x *= mirror;
    const float centers[3][5][2]={
        {{-.48f,-.38f},{.41f,-.40f},{-.43f,.41f},{.44f,.41f},{.04f,.03f}},
        {{-.50f,-.47f},{.46f,-.42f},{-.38f,.44f},{.46f,.42f},{-.02f,-.03f}},
        {{-.50f,-.40f},{.13f,-.55f},{.48f,.03f},{.13f,.55f},{-.53f,.39f}}};
    const int arrangement = static_cast<int>(random(9)*2.999f);
    const float turn = (random(8)-.5f)*.32f;
    const float tx=x*std::cos(turn)-y*std::sin(turn);
    y=x*std::sin(turn)+y*std::cos(turn);x=tx;
    const int count = arrangement==2 ? 5 : water < .30f ? 4 : 5;
    for (int i=0;i<count;++i)
    {
        const float cx=centers[arrangement][i][0]+(random(11+i*7)-.5f)*.10f;
        const float cy=centers[arrangement][i][1]+(random(12+i*7)-.5f)*.10f;
        const bool satellite = i==4 && arrangement!=2;
        const float scale=(satellite ? .13f : arrangement==2 ? .245f : .30f)+(1.f-water)*(satellite ? .025f : .04f);
        const float dx=(x-cx)/(scale*(.90f+random(13+i*7)*.26f));
        const float dy=(y-cy)/(scale*(.78f+random(14+i*7)*.30f));
        const float theta=std::atan2(dy,dx);
        const float shore=1.f+.13f*std::sin(theta*3.f+phase+i)
                             +.065f*std::cos(theta*5.f-phase-i);
        score=std::max(score,shore-std::sqrt(dx*dx+dy*dy));
    }
    return std::min(score-12.f*(edgeX*edgeX+edgeY*edgeY),frameMargin);
}
