#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gm::map_render
{
// A small, seamless, map-anchored cloud texture. It is generated once, never
// from map contents; concealed geography cannot influence its appearance.
constexpr int fogTextureSize=128;
constexpr float fogTexturePeriodCells=24.f;
using FogTexel=std::array<unsigned char,4>;

inline float fogLattice(int x,int y,int period)
{
    x=(x%period+period)%period; y=(y%period+period)%period;
    std::uint32_t h=static_cast<std::uint32_t>(x)*374761393u+static_cast<std::uint32_t>(y)*668265263u+914731u;
    h=(h^(h>>13))*1274126177u; h^=h>>16;
    return static_cast<float>(h&65535u)/65535.f;
}
inline float fogNoise(float x,float y,int period)
{
    const int ix=static_cast<int>(std::floor(x)),iy=static_cast<int>(std::floor(y));
    float u=x-ix,v=y-iy; u=u*u*u*(u*(u*6-15)+10); v=v*v*v*(v*(v*6-15)+10);
    const float a=fogLattice(ix,iy,period),b=fogLattice(ix+1,iy,period);
    const float c=fogLattice(ix,iy+1,period),d=fogLattice(ix+1,iy+1,period);
    return (a+(b-a)*u)*(1-v)+(c+(d-c)*u)*v;
}
inline const std::array<float,fogTextureSize*fogTextureSize> &fogCloudField()
{
    static const auto field=[] {
        std::array<float,fogTextureSize*fogTextureSize> result{};
        for(int y=0;y<fogTextureSize;++y) for(int x=0;x<fogTextureSize;++x)
        {
            const float u=static_cast<float>(x)/fogTextureSize,v=static_cast<float>(y)/fogTextureSize;
            const float warpX=.17f*(fogNoise(u*3,v*3,3)-.5f);
            const float warpY=.17f*(fogNoise(u*3+1.3f,v*3+2.1f,3)-.5f);
            float value=0,weight=.56f;
            for(int frequency:{3,6,12,24})
            { value+=weight*fogNoise((u+warpX)*frequency,(v+warpY)*frequency,frequency);weight*=.5f; }
            result[y*fogTextureSize+x]=std::clamp((value-.19f)*1.8f,0.f,1.f);
        }
        return result;
    }();
    return field;
}
inline FogTexel fogCloudColor(float amount,bool editor)
{
    const float base=editor ? 102.f : 27.f, range=editor ? 119.f : 65.f;
    const auto r=static_cast<unsigned char>(base+range*amount);
    return {r,static_cast<unsigned char>(r+(editor?7:6)),static_cast<unsigned char>(r+(editor?11:12)),255};
}
inline const std::array<FogTexel,fogTextureSize*fogTextureSize> &fogTexturePixels(bool editor)
{
    static const auto gm=[] { std::array<FogTexel,fogTextureSize*fogTextureSize> a{};
        for(std::size_t i=0;i<a.size();++i)a[i]=fogCloudColor(fogCloudField()[i],true);return a; }();
    static const auto player=[] { std::array<FogTexel,fogTextureSize*fogTextureSize> a{};
        for(std::size_t i=0;i<a.size();++i)a[i]=fogCloudColor(fogCloudField()[i],false);return a; }();
    return editor ? gm : player;
}
inline FogTexel fogTextureAt(float mapX,float mapY,bool editor=false)
{
    const float tx=mapX/fogTexturePeriodCells*fogTextureSize-.5f;
    const float ty=mapY/fogTexturePeriodCells*fogTextureSize-.5f;
    const int ix=static_cast<int>(std::floor(tx)),iy=static_cast<int>(std::floor(ty));
    const float u=tx-ix,v=ty-iy;
    const auto &pixels=fogTexturePixels(editor);
    const auto sample=[&](int x,int y)->const FogTexel & {
        return pixels[((y%fogTextureSize+fogTextureSize)%fogTextureSize)*fogTextureSize+
                      (x%fogTextureSize+fogTextureSize)%fogTextureSize]; };
    FogTexel result{};
    for(int channel=0;channel<3;++channel)
        result[channel]=static_cast<unsigned char>(std::lround(
            (sample(ix,iy)[channel]*(1-u)+sample(ix+1,iy)[channel]*u)*(1-v)+
            (sample(ix,iy+1)[channel]*(1-u)+sample(ix+1,iy+1)[channel]*u)*v));
    result[3]=255;return result;
}
}
