#pragma once
#include "Models/Map/MapIllustratedAtlas.hpp"
#include <map>
#include "Models/Map/MapUndergroundIllustrationV6.hpp"
#include "Models/Map/MapUndergroundMaterialInk.hpp"
namespace gm::map_render {
inline bool usesNaturalCaveInk(const MapModel &map) {
    const auto type=map.metadata.customProperties.find("tactical.template_kind");
    const bool tacticalCave=map.creationIntent==MapCreationIntent::TacticalEncounter &&
        type!=map.metadata.customProperties.end() && type->second=="6";
    return map.scope==MapScope::Dungeon && (map.creationIntent==MapCreationIntent::Cave || tacticalCave) && map.generation.recipe.generatorVersion>=11;
}
// All surfaces share the same continuous contour solution. The collision grid
// stays editable; rounding is bounded below half a cell and preserves islands.
inline std::vector<std::vector<MapFeaturePoint>> naturalCaveContours(const MapModel &map,bool applyFog,MapTileKind only=MapTileKind::Empty) {
    struct Edge {int ax,ay,bx,by;bool used=false;};std::vector<Edge> edges;
    const auto inside=[&](const MapCellModel *c) {
        return c&&(!applyFog||!c->fogged)&&(only==MapTileKind::Empty?(c->kind!=MapTileKind::Wall&&c->kind!=MapTileKind::Empty):c->kind==only);
    };
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
        if(!inside(mapCellAt(map,x,y)))continue;
        if(!inside(mapCellAt(map,x,y-1)))edges.push_back({x,y,x+1,y});
        if(!inside(mapCellAt(map,x+1,y)))edges.push_back({x+1,y,x+1,y+1});
        if(!inside(mapCellAt(map,x,y+1)))edges.push_back({x+1,y+1,x,y+1});
        if(!inside(mapCellAt(map,x-1,y)))edges.push_back({x,y+1,x,y});
    }
    std::multimap<std::pair<int,int>,std::size_t> starts;
    for(std::size_t i=0;i<edges.size();++i)starts.emplace(std::pair{edges[i].ax,edges[i].ay},i);
    std::vector<std::vector<MapFeaturePoint>> result;
    for(std::size_t first=0;first<edges.size();++first) {
        if(edges[first].used)continue;std::vector<MapFeaturePoint> chain;std::size_t current=first;
        for(std::size_t guard=0;guard<=edges.size();++guard) {
            auto &e=edges[current];if(e.used)break;e.used=true;chain.push_back({static_cast<float>(e.ax),static_cast<float>(e.ay)});
            auto range=starts.equal_range({e.bx,e.by});std::size_t next=edges.size();int bestTurn=-2;
            for(auto it=range.first;it!=range.second;++it)if(!edges[it->second].used) {
                const auto &n=edges[it->second];const int turn=(e.bx-e.ax)*(n.by-n.ay)-(e.by-e.ay)*(n.bx-n.ax);
                if(turn>bestTurn){bestTurn=turn;next=it->second;}
            }
            if(next==edges.size())break;current=next;
        }
        if(chain.size()<3)continue;
        for(int pass=0;pass<2;++pass) {
            std::vector<MapFeaturePoint> smooth;smooth.reserve(chain.size()*2);
            for(std::size_t i=0;i<chain.size();++i) {
                const auto a=chain[i],b=chain[(i+1)%chain.size()];
                smooth.push_back({a.x*.75f+b.x*.25f,a.y*.75f+b.y*.25f});
                smooth.push_back({a.x*.25f+b.x*.75f,a.y*.25f+b.y*.75f});
            }
            chain=std::move(smooth);
        }
        std::vector<MapFeaturePoint> compact;
        for(std::size_t i=0;i<chain.size();++i) {
            const auto a=chain[(i+chain.size()-1)%chain.size()],b=chain[i],c=chain[(i+1)%chain.size()];
            const float cross=(b.x-a.x)*(c.y-b.y)-(b.y-a.y)*(c.x-b.x);
            if(std::abs(cross)>.000001f)compact.push_back(b);
        }
        auto resolved=compact.size()>=3?std::move(compact):std::move(chain);
        if(usesUndergroundIllustrationV6(map) && only==MapTileKind::Empty) {
            // Sub-cell erosion is shared by floor and wall ink. The maximum
            // displacement stays well below the collision-cell boundary.
            std::vector<MapFeaturePoint> detailed;
            for(std::size_t i=0;i<resolved.size();++i) {
                const auto a=resolved[i],b=resolved[(i+1)%resolved.size()];
                const int steps=std::max(1,static_cast<int>(std::hypot(b.x-a.x,b.y-a.y)/1.1f));
                for(int j=0;j<steps;++j) {
                    const float t=static_cast<float>(j)/steps;
                    detailed.push_back({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t});
                }
            }
            const auto original=detailed;
            const float phase=(map.generation.recipe.seed%997)*.017f;
            for(std::size_t i=0;i<detailed.size();++i) {
                const auto a=original[(i+original.size()-1)%original.size()],b=original[(i+1)%original.size()];
                const float dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);
                if(len<.001f)continue;
                const auto p=original[i];
                const float erosion=.10f*std::sin(p.x*1.23f+p.y*.79f+phase);
                detailed[i]={p.x-dy/len*erosion,p.y+dx/len*erosion};
            }
            resolved=std::move(detailed);
        }
        result.push_back(std::move(resolved));
    }
    return result;
}
inline float naturalCaveContourArea(const std::vector<MapFeaturePoint> &p) {
    float a=0;for(std::size_t i=0;i<p.size();++i)a+=p[i].x*p[(i+1)%p.size()].y-p[(i+1)%p.size()].x*p[i].y;return a*.5f;
}
inline void fillNaturalCavePolygon(std::vector<AtlasInkPrimitive> &ink,const std::vector<MapFeaturePoint> &p,OverviewRenderColor color) {
    if(p.size()<3)return;
    const auto cross=[](MapFeaturePoint a,MapFeaturePoint b,MapFeaturePoint c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);};
    std::vector<std::size_t> indices;for(std::size_t i=0;i<p.size();++i)indices.push_back(i);
    if(naturalCaveContourArea(p)<0)std::reverse(indices.begin(),indices.end());
    while(indices.size()>=3) {
        bool clipped=false;
        for(std::size_t i=0;i<indices.size();++i) {
            const auto a=p[indices[(i+indices.size()-1)%indices.size()]],b=p[indices[i]],c=p[indices[(i+1)%indices.size()]];
            if(cross(a,b,c)<=.000001f)continue;bool occupied=false;
            for(std::size_t j=0;j<indices.size();++j) {
                if(j==i||j==(i+1)%indices.size()||j==(i+indices.size()-1)%indices.size())continue;const auto q=p[indices[j]];
                if(cross(a,b,q)>=-.000001f&&cross(b,c,q)>=-.000001f&&cross(c,a,q)>=-.000001f){occupied=true;break;}
            }
            if(occupied)continue;ink.push_back({{a,b,c},color,0,true});indices.erase(indices.begin()+static_cast<std::ptrdiff_t>(i));clipped=true;break;
        }
        if(!clipped)break;
    }
}
inline std::vector<AtlasInkPrimitive> buildNaturalCaveFloorInk(const MapModel &map,bool applyFog) {
    std::vector<AtlasInkPrimitive> ink;if(!usesNaturalCaveInk(map))return ink;
    auto contours=naturalCaveContours(map,applyFog);if(contours.empty())return ink;
    const bool v6=usesUndergroundIllustrationV6(map);
    const OverviewRenderColor floor=v6?OverviewRenderColor{190,180,146,255}:OverviewRenderColor{139,135,111,255},voidColor=v6?OverviewRenderColor{38,43,42,255}:OverviewRenderColor{31,34,31,255};
    const float w=static_cast<float>(map.width),h=static_cast<float>(map.height);
    ink.push_back({{{0,0},{w,0},{w,h}},voidColor,0,true});ink.push_back({{{0,0},{w,h},{0,h}},voidColor,0,true});
    for(const auto &p:contours)if(naturalCaveContourArea(p)>0)fillNaturalCavePolygon(ink,p,floor);
    for(const auto &p:contours)if(naturalCaveContourArea(p)<0)fillNaturalCavePolygon(ink,p,voidColor);
    if(v6 && map.generation.recipe.recipeVersion>=10)shadeCaveFloorMaterial(ink,map,floor);
    for(int y=1;y<map.height-1;++y)for(int x=1;x<map.width-1;++x) {
        const auto *c=mapCellAt(map,x,y);if(!c||c->kind!=MapTileKind::Cave||(applyFog&&c->fogged))continue;
        unsigned hash=static_cast<unsigned>(x*197+y*397)^map.generation.recipe.seed;hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;
        if(v6) {
            const float cx=x+.5f,cy=y+.5f;
            bool broad=true;
            for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx) {
                const auto*n=mapCellAt(map,x+dx,y+dy);
                if(!n||n->kind!=MapTileKind::Cave||(applyFog&&n->fogged))broad=false;
            }
            if(broad&&hash%11==0) {
                undergroundInkPolygon(ink,undergroundInkOval(cx,cy,1.7f,1.05f,hash,16),{148,141,111,24});
                undergroundInkLine(ink,{{cx-1.1f,cy+.35f},{cx-.3f,cy+.48f},{cx+.65f,cy+.4f},{cx+1.3f,cy+.12f}},{149,139,107,70},.03f);
            }
            if(broad&&hash%53==0) {
                // A worn fossil embedded in the floor is decorative geology,
                // not an unmodelled obstacle or a creature token.
                std::vector<MapFeaturePoint> fossil;
                const float phase=(hash%628)*.01f;
                for(int i=0;i<=56;++i) {
                    const float a=i*.19f,r=.08f+i*.018f;
                    fossil.push_back({cx+std::cos(a+phase)*r,cy+std::sin(a+phase)*r*.72f});
                }
                undergroundInkLine(ink,fossil,{139,137,107,100},.065f);
                for(int i=22;i<56;i+=3) {
                    const float a=i*.19f+phase,r=.08f+i*.018f;
                    undergroundInkLine(ink,{{cx+std::cos(a)*r,cy+std::sin(a)*r*.72f},{cx+std::cos(a+.035f)*(r+.16f),cy+std::sin(a+.035f)*(r+.16f)*.72f}},{220,205,162,165},.045f);
                }
            }
            if(hash%4==0) {
                const float ox=((hash>>8)%70)*.01f,oy=((hash>>16)%60)*.01f;
                undergroundInkLine(ink,{{x+.13f+ox*.4f,y+.2f+oy},{x+.33f+ox*.4f,y+.23f+oy}},{139,133,106,86},.025f);
                undergroundInkLine(ink,{{x+.32f+ox*.3f,y+.32f+oy},{x+.45f+ox*.3f,y+.35f+oy}},{218,207,166,160},.03f);
            }
            if(hash%13==0)for(int i=0;i<3;++i) {
                const float xx=cx-.25f+i*.19f,yy=cy+(i%2?.16f:-.1f);
                undergroundInkLine(ink,{{xx-.045f,yy},{xx+.055f,yy-.015f}},{223,211,172,180},.065f);
            }
        }
        if(hash%19!=0||c->wayKind!=MapWayKind::None)continue;
        const float fx=static_cast<float>(x),fy=static_cast<float>(y);
        ink.push_back({{{fx+.25f,fy+.35f},{fx+.42f,fy+.41f},{fx+.48f,fy+.60f},{fx+.70f,fy+.68f}},{85,87,70,100},.027f,false});
    }
    return ink;
}
inline std::vector<AtlasInkPrimitive> buildNaturalCavePoolInk(const MapModel &map,bool applyFog,bool showWater=true) {
    std::vector<AtlasInkPrimitive> ink;if(!usesNaturalCaveInk(map))return ink;
    const bool v6=usesUndergroundIllustrationV6(map);
    for(auto kind:{MapTileKind::Water,MapTileKind::Pit}) {
        if(kind==MapTileKind::Water&&!showWater)continue;auto contours=naturalCaveContours(map,applyFog,kind);
        if(v6) for(auto &p:contours) {
            const auto original=p;const float phase=(map.generation.recipe.seed%631)*.01f;
            for(std::size_t i=0;i<p.size();++i) {
                const auto a=original[(i+original.size()-1)%original.size()],b=original[(i+1)%original.size()];
                const float dx=b.x-a.x,dy=b.y-a.y,len=std::max(.001f,std::hypot(dx,dy));
                const float shift=(kind==MapTileKind::Pit?.14f:.075f)*std::sin(original[i].x*2.6f+original[i].y*3.1f+phase);
                p[i].x-=dy/len*shift;p[i].y+=dx/len*shift;
            }
        }
        const OverviewRenderColor color=kind==MapTileKind::Water?OverviewRenderColor{63,118,119,255}:OverviewRenderColor{43,47,43,255};
        for(const auto &p:contours)if(naturalCaveContourArea(p)>0)fillNaturalCavePolygon(ink,p,color);
        for(const auto &p:contours)if(naturalCaveContourArea(p)<0)fillNaturalCavePolygon(ink,p,v6?OverviewRenderColor{190,180,146,255}:OverviewRenderColor{139,135,111,255});
        if(v6&&kind==MapTileKind::Pit)for(const auto &p:contours) {
            if(naturalCaveContourArea(p)<=0)continue;
            // Depth strata follow the actual pit boundary; never paint an
            // additional obstacle on traversable floor.
            for(std::size_t i=1;i<p.size();i+=3) {
                const auto a=p[i-1],b=p[i];
                const float dx=b.x-a.x,dy=b.y-a.y,len=std::max(.001f,std::hypot(dx,dy));
                const MapFeaturePoint normal{-dy/len,dx/len};
                undergroundInkLine(ink,{{a.x+normal.x*.12f,a.y+normal.y*.12f},{b.x+normal.x*.12f,b.y+normal.y*.12f}},{86,89,70,220},.06f);
            }
        }
        for(auto p:contours){
            p.push_back(p.front());
            if(v6) {
                ink.push_back({p,kind==MapTileKind::Water?OverviewRenderColor{113,166,154,255}:OverviewRenderColor{123,122,96,255},.24f,false});
                ink.push_back({p,kind==MapTileKind::Water?OverviewRenderColor{42,91,93,255}:OverviewRenderColor{42,46,41,255},.055f,false});
            } else ink.push_back({p,{42,66,60,255},.055f,false});
        }
    }
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
        const auto *c=mapCellAt(map,x,y);if(!showWater||!c||c->kind!=MapTileKind::Water||(applyFog&&c->fogged)||(x*17+y*31)%4!=0)continue;
        ink.push_back({{{x+.25f,y+.51f},{x+.40f,y+.47f},{x+.60f,y+.50f},{x+.75f,y+.47f}},{132,171,152,190},.035f,false});
    }
    return ink;
}
inline std::vector<AtlasInkPrimitive> buildNaturalCaveSymbolInk(const MapModel &map,bool applyFog) {
    std::vector<AtlasInkPrimitive> ink;if(!usesNaturalCaveInk(map))return ink;
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
        const auto *c=mapCellAt(map,x,y);if(!c||(applyFog&&c->fogged))continue;
        if(c->kind!=MapTileKind::Cover&&c->kind!=MapTileKind::Rubble&&c->kind!=MapTileKind::Hazard)continue;
        const float cx=x+.5f,cy=y+.5f;
        if(usesUndergroundIllustrationV6(map)) {
            const unsigned h=undergroundInkHash(map.generation.recipe.seed,x,y,67);
            if(c->kind==MapTileKind::Cover) {
                const float wide=.29f+((h>>10)%10)*.009f;
                const float deep=.26f+((h>>18)%9)*.011f;
                undergroundInkStone(ink,cx-.05f,cy-.03f,wide,deep,h);
                undergroundInkStone(ink,cx+.26f,cy+.23f,.16f,.12f,h>>3);
            } else if(c->kind==MapTileKind::Rubble) {
                for(int i=0;i<4;++i) {
                    const float a=(h%628)*.01f+i*1.57f;
                    undergroundInkStone(ink,cx+std::cos(a)*.23f,cy+std::sin(a)*.23f,.15f+(i%2)*.025f,.12f,h+i*83);
                }
            } else {
                for(int i=0;i<5;++i) {
                    const float a=(h%628)*.01f+i*1.2566f;
                    undergroundInkStone(ink,cx+std::cos(a)*.22f,cy+std::sin(a)*.22f,.15f,.18f,h+i*73,true);
                }
                undergroundInkStone(ink,cx-.03f,cy-.02f,.18f,.17f,h+91,true);
            }
            continue;
        }
        const bool crystal=c->kind==MapTileKind::Hazard;
        const OverviewRenderColor dark=crystal?OverviewRenderColor{57,85,105,255}:OverviewRenderColor{61,68,54,255};
        const OverviewRenderColor light=crystal?OverviewRenderColor{152,194,196,255}:OverviewRenderColor{182,181,140,255};
        const OverviewRenderColor middle=crystal?OverviewRenderColor{94,147,166,255}:OverviewRenderColor{123,131,102,255};
        for(int shard=0;shard<3;++shard) {
            const float ox=(shard-1)*.34f,oy=shard==1?-.12f:.14f;
            const float tall=c->kind==MapTileKind::Rubble?.25f:(shard==1?.85f:.54f);
            const float half=c->kind==MapTileKind::Rubble?.27f:.28f;
            const MapFeaturePoint top{cx+ox-.03f,cy+oy-tall*.65f},left{cx+ox-half,cy+oy+tall*.35f},right{cx+ox+half,cy+oy+tall*.35f},mid{cx+ox+.04f,cy+oy+tall*.35f};
            ink.push_back({{top,left,mid},light,0,true});ink.push_back({{top,mid,right},middle,0,true});
            ink.push_back({{top,left,right,top},dark,.045f,false});
            if(crystal)ink.push_back({{top,mid},{203,226,202,210},.03f,false});
        }
    }
    return ink;
}
inline std::vector<AtlasInkPrimitive> buildNaturalCaveInk(const MapModel &map,bool applyFog) {
    std::vector<AtlasInkPrimitive> ink;if(!usesNaturalCaveInk(map))return ink;
    const bool v6=usesUndergroundIllustrationV6(map);
    const bool surfaceAccess=map.metadata.customProperties.count("cave.surface_entrances")!=0;
    const auto atOpenFrame=[&](MapFeaturePoint a,MapFeaturePoint b) {
        return surfaceAccess && ((a.x<.20f&&b.x<.20f)||(a.y<.20f&&b.y<.20f)||
            (a.x>map.width-.20f&&b.x>map.width-.20f)||(a.y>map.height-.20f&&b.y>map.height-.20f));
    };
    for(auto smooth:naturalCaveContours(map,applyFog)) {
        std::vector<MapFeaturePoint> dense;
        for(std::size_t i=0;i<smooth.size();++i) {
            const auto a=smooth[i],b=smooth[(i+1)%smooth.size()];const float dx=b.x-a.x,dy=b.y-a.y;
            const int samples=std::max(1,static_cast<int>(std::ceil(std::sqrt(dx*dx+dy*dy)/.75f)));
            for(int step=0;step<samples;++step){const float t=static_cast<float>(step)/samples;dense.push_back({a.x+dx*t,a.y+dy*t});}
        }
        smooth=std::move(dense);smooth.push_back(smooth.front());
        if(surfaceAccess) {
            // A clipped map edge at a surface mouth is open space, not a rock
            // face. Stop the wall silhouette here while retaining the floor.
            std::vector<MapFeaturePoint> section;
            for(std::size_t i=1;i<smooth.size();++i) {
                if(atOpenFrame(smooth[i-1],smooth[i])) {
                    if(section.size()>1)ink.push_back({section,{36,39,35,255},v6?.38f:.65f,false});
                    section.clear();
                } else {if(section.empty())section.push_back(smooth[i-1]);section.push_back(smooth[i]);}
            }
            if(section.size()>1)ink.push_back({section,{36,39,35,255},v6?.38f:.65f,false});
        } else ink.push_back({smooth,{36,39,35,255},v6?.38f:.65f,false});
        const auto normal=[&](std::size_t at) {
            const std::size_t n=smooth.size()-1;at%=n;
            const auto a=smooth[(at+n-1)%n],b=smooth[at],c=smooth[(at+1)%n];
            const float l1=std::max(.001f,std::hypot(b.x-a.x,b.y-a.y)),l2=std::max(.001f,std::hypot(c.x-b.x,c.y-b.y));
            const float nx=-(b.y-a.y)/l1-(c.y-b.y)/l2,ny=(b.x-a.x)/l1+(c.x-b.x)/l2,l=std::max(.001f,std::hypot(nx,ny));
            return MapFeaturePoint{nx/l,ny/l};
        };
        const float phase=(map.generation.recipe.seed%997)*.0063f;
        for(std::size_t i=1;i<smooth.size();++i) {
            const auto a=smooth[i-1],b=smooth[i];const float dx=b.x-a.x,dy=b.y-a.y,length=std::max(.01f,std::sqrt(dx*dx+dy*dy));const float nx=-dy/length,ny=dx/length;
            if(atOpenFrame(a,b))continue;
            const auto depth=[&](MapFeaturePoint p){return (v6?.58f:.35f)+.12f*std::sin(p.x*1.07f+p.y*.81f+phase)+.06f*std::cos(p.x*.43f-p.y*1.17f)+(v6?.1f*std::sin(p.x*3.1f+p.y*2.7f+phase):0.f);};
            const auto na=normal(i-1),nb=normal(i);
            const float aa=depth(a),bb=depth(b),ia=.15f+.055f*std::sin(a.x*.91f-a.y*.63f+phase),ib=.15f+.055f*std::sin(b.x*.91f-b.y*.63f+phase);
            const MapFeaturePoint ao{a.x-na.x*aa,a.y-na.y*aa},bo{b.x-nb.x*bb,b.y-nb.y*bb};
            const MapFeaturePoint ai{a.x+na.x*ia,a.y+na.y*ia},bi{b.x+nb.x*ib,b.y+nb.y*ib};
            const unsigned char shade=static_cast<unsigned char>(7.f+6.f*std::sin((a.x+a.y)*.73f+phase));
            if(v6 && map.generation.recipe.recipeVersion>=10)
                appendUndergroundWallMaterial(ink,map,ao,bo,ai,bi,true);
            else {
                ink.push_back({{ao,bo,ai},v6?OverviewRenderColor{static_cast<unsigned char>(112+shade),static_cast<unsigned char>(113+shade),static_cast<unsigned char>(95+shade),255}:OverviewRenderColor{static_cast<unsigned char>(83+shade),static_cast<unsigned char>(88+shade),static_cast<unsigned char>(74+shade),255},0,true});
                ink.push_back({{ai,bo,bi},v6?OverviewRenderColor{static_cast<unsigned char>(112+shade),static_cast<unsigned char>(113+shade),static_cast<unsigned char>(95+shade),255}:OverviewRenderColor{static_cast<unsigned char>(66+shade),static_cast<unsigned char>(72+shade),static_cast<unsigned char>(62+shade),255},0,true});
            }
            if(v6) {
                undergroundInkLine(ink,{ao,bo},{48,56,50,255},.07f);
                undergroundInkLine(ink,{{a.x-na.x*.08f,a.y-na.y*.08f},{b.x-nb.x*.08f,b.y-nb.y*.08f}},{173,167,132,230},.055f);
                if(i%13==0)undergroundInkLine(ink,{ao,{a.x-na.x*.27f+dx*.4f,a.y-na.y*.27f+dy*.4f},{a.x-na.x*.11f+dx*.7f,a.y-na.y*.11f+dy*.7f}},{63,70,58,160},.035f);
                if(i%9==0) {
                    const auto h=undergroundInkHash(map.generation.recipe.seed,static_cast<int>(a.x*9),static_cast<int>(a.y*9),71);
                    undergroundInkStone(ink,a.x-na.x*.32f,a.y-na.y*.32f,.17f+(h%10)*.012f,.16f+((h>>5)%8)*.015f,h);
                }
            }
            ink.push_back({{ai,bi},{35,40,34,245},.06f,false});
            if(i%11==0)ink.push_back({{ao,{a.x+nx*.04f,a.y+ny*.04f},{a.x+dx*.2f+nx*.15f,a.y+dy*.2f+ny*.15f}},{42,48,39,180},.035f,false});
        }
    }
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
        const auto *c=mapCellAt(map,x,y);if(!c||c->kind!=MapTileKind::Stairs||(applyFog&&c->fogged))continue;
        if(v6) {
            // An actual stair cell is an entrance to another level. Give it a
            // dark opening, pale stone jambs and lit threshold so it remains
            // legible among the cavern's equally coloured floor scratches.
            // Keep the architecture inside this traversable semantic cell.
            const auto p=[&](float a,float b) {
                for(int turn=0;turn<c->rotationQuarterTurns%4;++turn){const float old=a;a=-b;b=old;}
                return MapFeaturePoint{x+.5f+a,y+.5f+b};
            };
            fillNaturalCavePolygon(ink,{p(-.45f,-.46f),p(.45f,-.46f),p(.45f,.45f),p(-.45f,.45f)},{29,39,37,255});
            undergroundInkLine(ink,{p(-.45f,.42f),p(-.45f,-.43f),p(.45f,-.43f),p(.45f,.42f)},{199,195,158,255},.13f);
            undergroundInkLine(ink,{p(-.34f,-.34f),p(.34f,-.34f)},{58,87,85,255},.16f);
            for(int step=0;step<5;++step) {
                const float yy=-.24f+step*.14f,half=.20f+step*.034f;
                undergroundInkLine(ink,{p(-half,yy+.045f),p(half,yy+.045f)},{34,40,35,255},.12f);
                undergroundInkLine(ink,{p(-half,yy),p(half,yy)},{232,215,166,255},.075f);
            }
            for(float side:{-.43f,.43f}) {
                undergroundInkLine(ink,{p(side,.30f),p(side,.12f)},{103,68,40,255},.095f);
                ink.push_back({{p(side-.065f,.14f),p(side,.0f),p(side+.065f,.14f)},{246,171,68,255},0,true});
            }
            continue;
        }
        for(int step=0;step<5;++step) {
            const float half=.18f+step*.047f,yy=y+.16f+step*.16f;
            ink.push_back({{{x+.5f-half,yy+.04f},{x+.5f+half,yy+.04f}},{28,29,28,255},.12f,false});
            ink.push_back({{{x+.5f-half,yy},{x+.5f+half,yy}},{191,185,156,255},.06f,false});
        }
    }
    return ink;
}
}
