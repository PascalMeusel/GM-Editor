// Generator 10 settlement composition; intentionally isolated from legacy recipes.
inline void mapGenV10ComposeSettlement(MapModel &map, const MapGenerationOptions &options)
{
    mapGenClear(map, MapTileKind::Empty);
    map.generation.graph.features.clear();
    map.features.clear();
    map.routes.clear();
    map.metadata.customProperties["settlement.composition_revision"] = options.generatorVersion>=12?"12":"10";
    std::mt19937 rng(options.seed ^ 0x73a41b29u);
    const bool de = options.contentLanguage == "de";
    const int archetype = static_cast<int>((options.seed / 17u) % 6u);
    if(options.generatorVersion>=11)map.metadata.customProperties["settlement.layout_family"]=std::to_string(archetype);
    const float phase = mapGenV2Hash01(options.seed, 14, 6) * 6.2831853f;
    const float cx = map.width * (.48f + mapGenV2Hash01(options.seed, 2, 7)*.06f);
    const float cy = map.height * (.46f + mapGenV2Hash01(options.seed, 9, 4)*.08f);
    const float settlementExtent=options.generatorVersion>=12 && options.settlementDensity<20?
        std::clamp(std::sqrt(std::max(1,options.settlementDensity)/30.f),.48f,1.f):1.f;
    const float rx = map.width * .38f*settlementExtent, ry = map.height * .37f*settlementExtent;
    const auto coast = [&](float y) {return map.width*.74f + std::sin(y*.13f+phase)*(options.generatorVersion>=12?std::max(2.1f,map.width*.065f):2.1f) + std::sin(y*.27f-phase)*.7f;};
    const auto river = [&](float y) {return cx + rx*.42f + std::sin(y*.14f+phase)*1.4f + std::sin(y*.057f-phase)*1.3f;};
    for (int y=0;y<map.height;++y) for(int x=0;x<map.width;++x) {
        const float dx=(x+.5f-cx)/rx,dy=(y+.5f-cy)/ry;
        const float distance=dx*dx+dy*dy;
        const float woodland=mapGenV2FractalNoise(options.seed+421u,float(x),float(y),9);
        if(archetype==3 && x+.5f>coast(y+.5f)) mapGenSetCell(map,x,y,MapTileKind::Water);
        else if((options.generatorVersion<11 || options.forestDensity>0) && distance>1.04f && woodland>.68f-float(options.forestDensity)*.0035f)
            mapGenSetCell(map,x,y,MapTileKind::Forest,MapSymbolVariantMode::Continuous);
        else if(distance<.90f+(mapGenV2FractalNoise(options.seed+445u,float(x),float(y),13)-.5f)*.23f)
            mapGenSetCell(map,x,y,MapTileKind::Dirt,MapSymbolVariantMode::Continuous);
    }
    if(archetype==2) {
        std::vector<MapCellCoordinate> channel;
        for(int y=0;y<map.height;++y) {
            const int x=std::clamp(int(river(y+.5f)),1,map.width-2);
            mapGenSetRiverCell(map,x,y);channel.push_back({x,y});
            if(map.width>=48) mapGenSetRiverCell(map,x+1,y);
        }
        mapGenV2AddPathFeature(map,MapGeneratedFeatureKind::River,mapGenV2FeatureId(options.seed,"city_river",0),
            de?"Stadtfluss":"Town River","fantasy_neutral.water.river",map.width>=48?1.8f:1.2f,channel);
    }
    mapGenInstallPreservedFeatures(map,options);
    auto line = [&](const std::string &tag,const std::string &name,MapGeneratedFeatureKind kind,
                    const std::string &material,float width,std::vector<MapFeaturePoint> points,bool closed=false) {
        MapGeneratedFeature f;f.id=mapGenV2FeatureId(options.seed,tag.c_str(),int(map.generation.graph.features.size()));
        f.kind=kind;f.name=name;f.materialId=material;f.width=width;f.points=std::move(points);f.closed=closed;
        map.generation.graph.features.push_back(std::move(f));
    };
    auto street = [&](const std::string &name,std::vector<MapFeaturePoint> points,bool major=false) {
        points.erase(std::unique(points.begin(),points.end(),[](const auto &a,const auto &b){return std::hypot(a.x-b.x,a.y-b.y)<.05f;}),points.end());
        if(points.size()<2)return;
        // Interpolation gives the raster and renderer the same flowing street.
        std::vector<MapFeaturePoint> smooth;
        for(std::size_t i=0;i+1<points.size();++i) {
            const auto a=points[i?i-1:i],b=points[i],c=points[i+1],d=points[std::min(i+2,points.size()-1)];
            const int steps=std::max(2,int(std::hypot(c.x-b.x,c.y-b.y)*3));
            for(int k=0;k<steps;++k) {
                float t=float(k)/steps,t2=t*t,t3=t2*t;
                auto axis=[&](float aa,float bb,float cc,float dd){return .5f*((2*bb)+(-aa+cc)*t+(2*aa-5*bb+4*cc-dd)*t2+(-aa+3*bb-3*cc+dd)*t3);};
                smooth.push_back({std::clamp(axis(a.x,b.x,c.x,d.x),.5f,map.width-.5f),std::clamp(axis(a.y,b.y,c.y,d.y),.5f,map.height-.5f)});
            }
        }
        smooth.push_back(points.back());
        if(archetype==3) for(auto &p:smooth) p.x=std::min(p.x,coast(p.y)-2.4f);
        const bool closed=std::hypot(smooth.front().x-smooth.back().x,smooth.front().y-smooth.back().y)<.1f;
        for(std::size_t i=1;i<smooth.size();++i)
            mapGenDrawWay(map,int(smooth[i-1].x),int(smooth[i-1].y),int(smooth[i].x),int(smooth[i].y),major?MapWayKind::StoneRoad:MapWayKind::DirtRoad,rng,0,false);
        line("street",name,MapGeneratedFeatureKind::Road,major?"fantasy_neutral.road.stone":"fantasy_neutral.road.dirt",major?(options.generatorVersion>=12 && std::min(map.width,map.height)<28?.88f:1.18f):.76f,std::move(smooth),closed);
    };
    const int approaches=std::clamp(options.edgeConnectionCount,1,archetype==3?3:4);
    map.generation.recipe.edgeConnectionCount=approaches;
    const float left=cx-rx*.88f,right=archetype==3?coast(cy)-2.0f:cx+rx*.88f;
    const float top=cy-ry*.86f,bottom=cy+ry*.86f;
    const float westY=cy+std::sin(phase)*ry*.25f,eastY=cy-std::cos(phase)*ry*.22f;
    // A readable civic square anchors irregular blocks; its routes meet at its
    // edges, leaving a place to gather rather than a road crossing a city icon.
    const float plaza=options.generatorVersion>=11 ? (map.width<32?1.1f:map.width<40?1.5f:2.0f) : (map.width<40?1.6f:2.2f);
    street(de?u8"Alte Marktstra\u00dfe":"Old Market Road",{{.5f,westY},{left,westY},{cx-rx*.34f,cy+.8f},{cx-plaza,cy},{cx+plaza,cy},{right,eastY},{archetype==3 || approaches<2?right:map.width-.5f,eastY}},true);
    const float northX=cx-rx*.28f,southX=cx+rx*(archetype==2?-.12f:.18f);
    street(de?"Tempelgasse":"Temple Lane",{{approaches>=(archetype==3?2:3)?northX:northX+.8f,approaches>=(archetype==3?2:3)?.5f:top},{northX+.8f,top},{cx-1.f,cy-ry*.3f},{cx,cy-plaza},{cx,cy+plaza},{southX,bottom},{approaches>=(archetype==3?3:4)?southX+1.f:southX,approaches>=(archetype==3?3:4)?map.height-.5f:bottom}});
    if(archetype==2 || archetype==3) {
        std::vector<MapFeaturePoint> bank;
        for(int k=0;k<=6;++k) {
            const float y=top+(bottom-top)*k/6.f;
            bank.push_back({archetype==2?river(y)-3.0f:coast(y)-2.0f,y});
        }
        street(de?u8"Uferstra\u00dfe":"Riverside Street",bank,true);
    }
    if(archetype==1 || archetype==4) {
        std::vector<MapFeaturePoint> ring;
        for(int k=0;k<=16;++k) {
            const float a=k*6.2831853f/16.f;
            const float wobble=1.f+.07f*std::sin(a*3+phase);
            ring.push_back({cx+std::cos(a)*rx*.78f*wobble,cy+std::sin(a)*ry*.76f*wobble});
        }
        if(options.generatorVersion<11 || options.routeDensity>=25)
            street(de?"Ringgasse":"Ring Lane",ring,archetype==4);
        if(options.generatorVersion>=11 && options.routeDensity>=60 && std::min(map.width,map.height)>=28) {
            const int crossLanes=options.routeDensity>=85?2:1;
            for(int k=0;k<crossLanes;++k) {
                const float y=cy+(k==0?-1.f:1.f)*ry*.43f;
                street(de?"Nachbarschaftsgasse":"Neighborhood Lane",{{left+rx*.18f,y},{cx-rx*.15f,y+std::sin(phase)*.5f},{right-rx*.18f,y}});
            }
        }
    }
    else {
        // Lanes connect into the main streets; no disconnected grid stubs.
        const int laneLimit=options.generatorVersion>=12 && options.settlementDensity<30?1:std::min(map.width,map.height)<=24?2:std::min(map.width,map.height)<=32?3:5;
        const int lanes=options.generatorVersion>=11?std::clamp((mapGenClampPercent(options.routeDensity)+19)/20,0,laneLimit):(archetype==5?4:2);
        for(int k=0;k<lanes;++k) {
            const float side=k%2?-1.f:1.f;
            const float scale=options.generatorVersion>=11?.28f+float(k/2)*.24f:(k<2?.52f:.76f);
            const float y=cy+side*ry*scale;
            const float bend=std::sin(phase+k)*1.2f;
            // A higher street setting must not put another bridge on every
            // parallel lane. Secondary neighborhoods join the riverside road;
            // only the main market crossing and first craft lane span water.
            const float laneRight=options.generatorVersion>=11 && archetype==2 && (k>0 || std::min(map.width,map.height)<28) ? river(y)-3.0f : options.generatorVersion>=11 && archetype==3 ? coast(y)-2.6f : right-.7f;
            if(options.generatorVersion>=11) {
                // Strictly ordered controls avoid folded hooks where short
                // streets join the bank; leave their final point on that road.
                const float start=left+.8f,span=std::max(.5f,laneRight-start);
                street(de?"Handwerkergasse":"Crafts Lane",{{start,y},{start+span*.35f,y+bend*.55f},{start+span*.72f,y-bend*.22f},{laneRight,y}});
            } else street(de?"Handwerkergasse":"Crafts Lane",{{left+.8f,y},{std::min(cx-rx*.35f,laneRight),y+bend},{std::min(cx+rx*.2f,laneRight),y-bend*.4f},{laneRight,y}});
        }
        if(archetype==5 && (options.generatorVersion<11 || lanes>=2)) {
            for(float offset:{-.57f,.55f}) {
                float x=cx+rx*offset;
                street(de?"Verbindungsgasse":"Connecting Lane",{{x,cy-ry*.76f},{x+std::sin(phase)*.7f,cy},{x,cy+ry*.76f}});
            }
        }
    }
    if(options.generatorVersion>=12 && archetype!=1 && archetype!=4 && archetype!=5 && options.routeDensity>=40) {
        // Join the ends of inhabited lanes into a neighborhood, rather than
        // presenting a row of unrelated cul-de-sacs across the town.
        const int limit=options.settlementDensity<30?1:std::min(map.width,map.height)<=24?2:std::min(map.width,map.height)<=32?3:5;
        const int lanes=std::clamp((mapGenClampPercent(options.routeDensity)+19)/20,0,limit);
        if(lanes>=2) {
            const float x=left+.8f,span=ry*(.28f+float((lanes-1)/2)*.24f);
            street(de?"Gartengasse":"Garden Lane",{{x,cy-span},{x-.45f,cy-span*.35f},{x+.28f,cy+span*.35f},{x,cy+span}});
        }
    }
    if(options.generatorVersion>=12 && archetype==3) {
        // Working waterfronts are real walkable bridge cells extending from
        // the bank street, not painted boats or a fake traversal promise.
        const int piers=std::min(map.width,map.height)<30?1:options.settlementDensity>=60?3:2;
        for(int dock=0;dock<piers;++dock) {
            const int y=std::clamp(int(cy+(dock-(piers-1)*.5f)*std::max(4.f,ry*.50f)),2,map.height-3);
            const int start=std::clamp(int(coast(y+.5f)-3.f),1,map.width-3);
            const int end=std::clamp(int(coast(y+.5f)+3.f+(dock%2)),start+1,map.width-2);
            for(int x=start;x<=end;++x) {
                auto*c=mapCellAt(map,x,y);if(!c)continue;
                const bool water=c->kind==MapTileKind::Water;
                mapGenSetCell(map,x,y,water?MapTileKind::Bridge:MapTileKind::Dirt);
                mapCellAt(map,x,y)->wayKind=MapWayKind::StoneRoad;
            }
            line("harbor_pier",de?"Hafensteg":"Harbor pier",MapGeneratedFeatureKind::Bridge,"fantasy_neutral.settlement.pier",.86f,{{start+.5f,y+.5f},{end+.5f,y+.5f}});
        }
    }
    // Streets paint semantic bridges; represent these spans explicitly too.
    // One illustrated span per connected crossing, including diagonal roads.
    // Per-row spans formerly stacked two independent bridge stamps.
    std::vector<bool> bridgeSeen(static_cast<std::size_t>(map.width*map.height),false);
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
        const auto *c=mapCellAt(map,x,y);const auto index=static_cast<std::size_t>(y*map.width+x);
        if(options.generatorVersion>=12 && archetype==3)continue; // Piers already have their own exact editable feature.
        if(!c || c->kind!=MapTileKind::Bridge || bridgeSeen[index])continue;
        std::vector<MapCellCoordinate> crossing{{x,y}};bridgeSeen[index]=true;
        int minX=x,maxX=x;
        for(std::size_t q=0;q<crossing.size();++q) {
            const auto p=crossing[q];minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                const int nx=p.x+dx,ny=p.y+dy;const auto *neighbor=mapCellAt(map,nx,ny);
                if(!neighbor || neighbor->kind!=MapTileKind::Bridge)continue;
                const auto ni=static_cast<std::size_t>(ny*map.width+nx);
                if(!bridgeSeen[ni]){bridgeSeen[ni]=true;crossing.push_back({nx,ny});}
            }
        }
        float westY=0,eastY=0;int westCount=0,eastCount=0;
        for(auto p:crossing){if(p.x==minX){westY+=p.y+.5f;++westCount;}if(p.x==maxX){eastY+=p.y+.5f;++eastCount;}}
        line("bridge",de?u8"Steinbr\u00fccke":"Stone Bridge",MapGeneratedFeatureKind::Bridge,"fantasy_neutral.bridge.stone",1.0f,
            {{minX-.18f,westY/westCount},{maxX+1.18f,eastY/eastCount}});
    }
    if(options.generatorVersion>=11 && archetype==4) {
        // The fortified recipe needs an actual defensive boundary. Reserve
        // road-width gates before placing houses; walls are semantic cells,
        // so artwork, editing and traversal use the same blocked footprint.
        const float wallRx=rx*1.08f,wallRy=ry*1.08f;
        auto wallSide=[&](MapFeaturePoint p) {
            const float dx=(p.x-cx)/wallRx,dy=(p.y-cy)/wallRy;
            return std::sqrt(dx*dx+dy*dy)-(1.f+.035f*std::sin(std::atan2(dy,dx)*3.f+phase));
        };
        std::vector<MapFeaturePoint> gates;
        for(const auto&f:map.generation.graph.features)if(f.kind==MapGeneratedFeatureKind::Road)
            for(std::size_t i=1;i<f.points.size();++i) {
                const auto a=f.points[i-1],b=f.points[i];const float da=wallSide(a),db=wallSide(b);
                if((da<0.f)==(db<0.f) || std::abs(da-db)<.00001f)continue;
                const float t=std::clamp(da/(da-db),0.f,1.f);
                gates.push_back({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t});
            }
        for(int y=1;y+1<map.height;++y)for(int x=1;x+1<map.width;++x) {
            const float dx=(x+.5f-cx)/wallRx,dy=(y+.5f-cy)/wallRy;
            const float angle=std::atan2(dy,dx);
            const float radius=1.f+.035f*std::sin(angle*3.f+phase);
            const float distance=std::abs(std::sqrt(dx*dx+dy*dy)-radius)*std::min(wallRx,wallRy);
            if(distance>.75f)continue;
            const bool gate=std::any_of(gates.begin(),gates.end(),[&](auto p){return std::hypot(x+.5f-p.x,y+.5f-p.y)<1.65f;});
            if(gate) {
                mapGenSetCell(map,x,y,MapTileKind::Dirt);
                mapCellAt(map,x,y)->wayKind=MapWayKind::StoneRoad;
            } else mapGenSetCell(map,x,y,MapTileKind::Wall);
        }
    }
    for(int y=int(cy-plaza);y<=int(cy+plaza);++y)for(int x=int(cx-plaza);x<=int(cx+plaza);++x) {
        auto *c=mapCellAt(map,x,y);if(c && c->kind!=MapTileKind::Water && c->kind!=MapTileKind::Bridge) {
            const auto way=c->wayKind;mapGenSetCell(map,x,y,MapTileKind::Dirt);mapCellAt(map,x,y)->wayKind=way;
        }
    }
    struct Plot {int x,y,w,h;};std::vector<Plot> plots;
    auto nearRoad=[&](float x,float y) {
        float distance=100000.f;
        for(const auto &f:map.generation.graph.features)if(f.kind==MapGeneratedFeatureKind::Road)
            for(std::size_t i=1;i<f.points.size();++i) {
                const auto a=f.points[i-1],b=f.points[i];float dx=b.x-a.x,dy=b.y-a.y;
                const float t=std::clamp(((x-a.x)*dx+(y-a.y)*dy)/std::max(.001f,dx*dx+dy*dy),0.f,1.f);
                distance=std::min(distance,std::hypot(x-a.x-dx*t,y-a.y-dy*t));
            }
        return distance;
    };
    const int target=options.generatorVersion>=12?std::clamp(int(map.width*map.height*(options.settlementDensity<=0?0.f:.006f+options.settlementDensity*.00044f)),0,165):std::clamp(int(map.width*map.height*(.012f+options.settlementDensity*.00028f)),9,105);
    for(int attempt=0;attempt<target*160 && int(plots.size())<target;++attempt) {
        const int x=mapGenRandomBetween(rng,std::max(1,int(cx-rx)),std::min(map.width-3,int(cx+rx)));
        const int y=mapGenRandomBetween(rng,std::max(1,int(cy-ry)),std::min(map.height-3,int(cy+ry)));
        int w=mapGenChance(rng,26)?3:2,h=mapGenChance(rng,24)?1:2;
        const bool compact=options.generatorVersion>=11 && std::min(map.width,map.height)<=24;
        if(compact)w=std::min(w,2);
        const float squareClearance=options.generatorVersion>=12?(compact?.8f:1.15f):(compact?1.1f:1.8f);
        if(mapGenChance(rng,50)) std::swap(w,h);
        const float dx=(x+w*.5f-cx)/rx,dy=(y+h*.5f-cy)/ry;
        if(dx*dx+dy*dy>1.02f || (std::abs(x+w*.5f-cx)<plaza+squareClearance && std::abs(y+h*.5f-cy)<plaza+squareClearance))continue;
        bool valid=true;
        for(int yy=y-1;yy<=y+h && valid;++yy)for(int xx=x-1;xx<=x+w;++xx) {
            const auto *c=mapCellAt(map,xx,yy);
            if(!c || (c->kind==MapTileKind::Building && !(options.generatorVersion>=12 && (xx<x || xx>=x+w) && (yy<y || yy>=y+h))) || (options.generatorVersion>=11 && c->kind==MapTileKind::Wall) || c->kind==MapTileKind::Water || c->kind==MapTileKind::Bridge || c->kind==MapTileKind::Landmark || c->kind==MapTileKind::Settlement) {valid=false;break;}
            if(xx>=x&&xx<x+w&&yy>=y&&yy<y+h && (c->wayKind!=MapWayKind::None || nearRoad(xx+.5f,yy+.5f)<1.0f)) {valid=false;break;}
        }
        if(!valid || nearRoad(x+w*.5f,y+h*.5f)>(options.generatorVersion>=12?2.3f+mapGenClampPercent(options.settlementDensity)*.045f:3.5f))continue;
        for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)mapGenSetCell(map,xx,yy,MapTileKind::Building);
        plots.push_back({x,y,w,h});
        line("building",de?"Haus":"House",MapGeneratedFeatureKind::BuildingGroup,"fantasy_neutral.settlement.building",1.f,{{x+.5f,y+.5f}});
    }
    if(options.generatorVersion>=11) {
        // Every visible house gets an actual short approach to the street.
        // Search only dry non-building cells; do not draw a line through a
        // neighbor or water merely to make a house look connected.
        for(const auto &plot:plots) {
            struct Step {int x,y,parent;};std::vector<Step> search;
            std::vector<bool> visited(static_cast<std::size_t>(map.width*map.height),false);
            auto enter=[&](int x,int y,int parent) {
                const auto *cell=mapCellAt(map,x,y);
                if(!cell || cell->kind==MapTileKind::Building || cell->kind==MapTileKind::Wall || cell->kind==MapTileKind::Water || cell->kind==MapTileKind::Bridge || cell->kind==MapTileKind::Landmark || cell->kind==MapTileKind::Settlement)return;
                const int reach=options.generatorVersion>=12?8:5;
                if(x<plot.x-reach || x>plot.x+plot.w+reach-1 || y<plot.y-reach || y>plot.y+plot.h+reach-1)return;
                const auto index=static_cast<std::size_t>(y*map.width+x);
                if(visited[index])return;visited[index]=true;search.push_back({x,y,parent});
            };
            enter(plot.x-1,plot.y+plot.h/2,-1);enter(plot.x+plot.w,plot.y+plot.h/2,-1);
            enter(plot.x+plot.w/2,plot.y-1,-1);enter(plot.x+plot.w/2,plot.y+plot.h,-1);
            int found=-1;
            for(std::size_t i=0;i<search.size() && i<(options.generatorVersion>=12?700u:220u);++i) {
                const auto step=search[i];const auto *cell=mapCellAt(map,step.x,step.y);
                if(cell && mapGenWayKindIsRoadLike(cell->wayKind)){found=int(i);break;}
                enter(step.x-1,step.y,int(i));enter(step.x+1,step.y,int(i));enter(step.x,step.y-1,int(i));enter(step.x,step.y+1,int(i));
            }
            if(found<0)continue;
            std::vector<MapFeaturePoint> path;
            for(int i=found;i>=0;i=search[static_cast<std::size_t>(i)].parent) {
                const auto step=search[static_cast<std::size_t>(i)];path.push_back({step.x+.5f,step.y+.5f});
            }
            std::reverse(path.begin(),path.end());if(path.empty())continue;
            const auto first=path.front();MapFeaturePoint door=first;
            if(first.x<plot.x)door.x=plot.x+.03f;
            else if(first.x>plot.x+plot.w)door.x=plot.x+plot.w-.03f;
            else if(first.y<plot.y)door.y=plot.y+.03f;
            else door.y=plot.y+plot.h-.03f;
            path.insert(path.begin(),door);
            if(options.generatorVersion>=12 && path.size()>2) {
                // Round only inside each known dry BFS cell; endpoints and the
                // actual door/street connection are kept unchanged.
                std::vector<MapFeaturePoint> rounded{path.front()};
                for(std::size_t i=1;i+1<path.size();++i){
                    const auto prev=path[i-1],cur=path[i],next=path[i+1];
                    const MapFeaturePoint a{cur.x+(prev.x-cur.x)*.22f,cur.y+(prev.y-cur.y)*.22f},b{cur.x+(next.x-cur.x)*.22f,cur.y+(next.y-cur.y)*.22f};
                    rounded.push_back(a);
                    for(int k=1;k<=3;++k){float t=k/3.f,u=1.f-t;rounded.push_back({u*u*a.x+2*u*t*cur.x+t*t*b.x,u*u*a.y+2*u*t*cur.y+t*t*b.y});}
                }
                rounded.push_back(path.back());path=std::move(rounded);
                // Pull unnecessary grid detours taut only where the entire
                // narrow approach remains on dry, unobstructed ground. The
                // short door segment is deliberately retained inside its plot.
                auto clearApproach=[&](MapFeaturePoint a,MapFeaturePoint b) {
                    const int samples=std::max(1,int(std::ceil(std::hypot(b.x-a.x,b.y-a.y)*12.f)));
                    for(int i=0;i<=samples;++i) {
                        const float t=float(i)/samples,px=a.x+(b.x-a.x)*t,py=a.y+(b.y-a.y)*t;
                        for(const auto offset:std::array<MapFeaturePoint,5>{{{0,0},{-.16f,0},{.16f,0},{0,-.16f},{0,.16f}}}) {
                            const auto *cell=mapCellAt(map,int(std::floor(px+offset.x)),int(std::floor(py+offset.y)));
                            if(!cell || cell->kind==MapTileKind::Building || cell->kind==MapTileKind::Wall || cell->kind==MapTileKind::Water || cell->kind==MapTileKind::Bridge || cell->kind==MapTileKind::Landmark || cell->kind==MapTileKind::Settlement)return false;
                        }
                    }
                    return true;
                };
                std::vector<MapFeaturePoint> direct{path.front(),path[1]};
                for(std::size_t i=1;i+1<path.size();) {
                    std::size_t next=i+1;
                    for(std::size_t j=path.size()-1;j>i+1;--j)if(clearApproach(path[i],path[j])){next=j;break;}
                    direct.push_back(path[next]);i=next;
                }
                path=std::move(direct);
            }
            line("house_access",de?"Hauseingang":"House entrance",MapGeneratedFeatureKind::Trail,"fantasy_neutral.road.dirt",.25f,path);
        }
    }
    map.metadata.customProperties["settlement.building_count"]=std::to_string(plots.size());
    map.metadata.customProperties["settlement.building_target"]=std::to_string(target);
    MapGeneratedFeature settlement;
    settlement.id=mapGenV2FeatureId(options.seed,"settlement",0);settlement.kind=MapGeneratedFeatureKind::Settlement;
    settlement.name=mapGenV4SettlementName(options.seed,0,options.nameCulture);
    for(const auto &constraint:options.constraints)if(constraint.featureKind==MapGeneratedFeatureKind::Settlement && !constraint.label.empty())settlement.name=constraint.label;
    settlement.materialId="fantasy_neutral.settlement.city";settlement.points={{cx,cy}};settlement.locked=true;
    map.generation.graph.features.push_back(std::move(settlement));
    mapGenSetCell(map,int(cx),int(cy),MapTileKind::Settlement);
    int landmark=0;
    for(const auto &p:plots) {
        if(landmark>=std::min(4,map.generation.recipe.landmarkCount))break;
        if(p.w*p.h<4)continue;
        const char *en[]={"Guildhall","Moon Temple","Old Inn","Watch House"};
        const char *deNames[]={"Gildenhalle","Mondtempel","Altes Gasthaus","Wachhaus"};
        auto &feature=map.generation.graph.features;
        for(auto &f:feature)if(f.kind==MapGeneratedFeatureKind::BuildingGroup && !f.points.empty() && int(f.points[0].x)==p.x && int(f.points[0].y)==p.y) {
            f.name=de?deNames[landmark]:en[landmark];
            if(options.generatorVersion>=11) {
                const char *roles[]={"guildhall","temple","inn","watch_house"};
                f.materialId=std::string("fantasy_neutral.settlement.building.")+roles[landmark];
            }
        }
        ++landmark;
    }
    // Named quarters summarize actual house clusters instead of fixed empty boxes.
    for(int quadrant=0;quadrant<4;++quadrant) {
        float l=float(map.width),t=float(map.height),r=0,b=0;int count=0;
        for(const auto &p:plots) if(((p.x+p.w*.5f>=cx)?1:0)+((p.y+p.h*.5f>=cy)?2:0)==quadrant) {
            l=std::min(l,float(p.x));t=std::min(t,float(p.y));r=std::max(r,float(p.x+p.w));b=std::max(b,float(p.y+p.h));++count;
        }
        if(count<2)continue;
        const char *names[]={"Old Quarter","Merchants' Quarter","Temple Quarter","Craft Quarter"};
        const char *deNames[]={"Altstadt","Handelsviertel","Tempelviertel","Handwerkerviertel"};
        line("district",de?deNames[quadrant]:names[quadrant],MapGeneratedFeatureKind::District,"fantasy_neutral.district."+std::to_string(quadrant),.6f,
             {{l+.5f,t},{r-.5f,t},{r,t+.5f},{r,b-.5f},{r-.5f,b},{l+.5f,b},{l,b-.5f},{l,t+.5f}},true);
    }
    // Small editable public places occupy dry, unbuilt land next to streets.
    const char *poiNames[]={"Moon Shrine","Covered Market","Old Watchstone","Pilgrims' Well"};
    const char *poiDe[]={"Mondschrein","Markthalle","Alter Wachstein","Pilgerbrunnen"};
    const char *poiKinds[]={"shrine","market","watchstone","well"};
    const int poiTarget=std::clamp(map.generation.recipe.landmarkCount,0,8);
    std::vector<MapCellCoordinate> pointsOfInterest;
    for(int poi=0;poi<poiTarget;++poi) {
        MapCellCoordinate best{-1,-1};float bestScore=1e9f;
        const float angle=phase+poi*2.399963f;
        const float tx=cx+std::cos(angle)*rx*.48f,ty=cy+std::sin(angle)*ry*.48f;
        for(int y=2;y+2<map.height;++y)for(int x=2;x+2<map.width;++x) {
            const auto *c=mapCellAt(map,x,y);if(!c || c->wayKind!=MapWayKind::None || (c->kind!=MapTileKind::Dirt && c->kind!=MapTileKind::Empty))continue;
            if(std::hypot(x+.5f-cx,y+.5f-cy)<plaza+2.f || nearRoad(x+.5f,y+.5f)>3.5f)continue;
            bool clear=true;for(int yy=y-1;yy<=y+1&&clear;++yy)for(int xx=x-1;xx<=x+1;++xx) {
                const auto *n=mapCellAt(map,xx,yy);if(!n || n->kind==MapTileKind::Building || n->kind==MapTileKind::Water || n->kind==MapTileKind::Bridge) {clear=false;break;}
            }
            for(const auto &p:pointsOfInterest)if(std::hypot(float(p.x-x),float(p.y-y))<3.f)clear=false;
            if(!clear)continue;
            const float score=std::hypot(x+.5f-tx,y+.5f-ty);
            if(score<bestScore){bestScore=score;best={x,y};}
        }
        if(best.x<0)break;
        mapGenSetCell(map,best.x,best.y,MapTileKind::Landmark);pointsOfInterest.push_back(best);
        line("landmark",de?poiDe[poi%4]:poiNames[poi%4],MapGeneratedFeatureKind::Landmark,"fantasy_neutral.settlement.poi."+std::string(poiKinds[poi%4]),1.f,{{best.x+.5f,best.y+.5f}});
    }
    for(const auto &feature:map.generation.graph.features)if(feature.kind==MapGeneratedFeatureKind::Road && feature.points.size()>=2) {
        MapRouteModel route;route.name=de?"Marktweg":"Market Road";route.fromMapId=map.metadata.id;
        route.startX=int(feature.points.front().x);route.startY=int(feature.points.front().y);route.endX=int(feature.points.back().x);route.endY=int(feature.points.back().y);
        route.distanceMiles=std::max(.1f,map.width*.025f);route.terrain=de?u8"Stadtstra\u00dfe":"Town street";map.routes.push_back(std::move(route));break;
    }
    if(options.generatorVersion>=11) {
        // Keep trampled ground around actual streets and occupied plots.
        // Empty spaces become grass courtyards, rather than one giant disk of
        // brown earth unrelated to the settlement's inhabited structure.
        const int count=map.width*map.height;
        std::vector<int> distance(static_cast<std::size_t>(count),999);
        std::vector<MapCellCoordinate> queue;
        for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
            const auto *c=mapCellAt(map,x,y);
            if(c && (mapGenWayKindIsRoadLike(c->wayKind) || c->kind==MapTileKind::Building || c->kind==MapTileKind::Landmark || std::hypot(x+.5f-cx,y+.5f-cy)<plaza+1.f)) {
                distance[static_cast<std::size_t>(y*map.width+x)]=0;queue.push_back({x,y});
            }
        }
        for(std::size_t i=0;i<queue.size();++i) {
            const auto p=queue[i];const int d=distance[static_cast<std::size_t>(p.y*map.width+p.x)]+1;
            for(auto offset:std::array<MapCellCoordinate,4>{{{-1,0},{1,0},{0,-1},{0,1}}}) {
                const int x=p.x+offset.x,y=p.y+offset.y;
                if(x<0||y<0||x>=map.width||y>=map.height)continue;
                const auto index=static_cast<std::size_t>(y*map.width+x);
                if(d<distance[index]){distance[index]=d;queue.push_back({x,y});}
            }
        }
        for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
            const auto *c=mapCellAt(map,x,y);const int d=distance[static_cast<std::size_t>(y*map.width+x)];
            if(c && c->kind==MapTileKind::Dirt && c->wayKind==MapWayKind::None && d>=3)
                mapGenSetCell(map,x,y,MapTileKind::Empty);
        }
        map.metadata.customProperties["settlement.composition_revision"]=options.generatorVersion>=12?"12":"11";
    }
    if(options.generatorVersion>=12 && options.constraints.empty() && options.preservedFeatures.empty() && options.lockedFeatureIds.empty()) {
        const int orientation=int((options.seed/103u)%4u);const bool flipX=(orientation&1)!=0,flipY=(orientation&2)!=0;
        map.metadata.customProperties["settlement.layout_orientation"]=std::to_string(orientation);
        if(flipX||flipY) {
            auto oldCells=map.cells;
            auto direction=[&](int d){d=normalizeMapRotation(d);if(flipX&&(d==1||d==3))d=4-d;if(flipY&&(d==0||d==2))d=2-d;return d;};
            auto mask=[&](int m){int out=0;for(int d=0;d<4;++d)if(m&(1<<d))out|=1<<direction(d);return out;};
            for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
                auto c=oldCells[std::size_t(y*map.width+x)];c.rotationQuarterTurns=direction(c.rotationQuarterTurns);
                c.movementBlockedMask=mask(c.movementBlockedMask);c.lineOfSightBlockedMask=mask(c.lineOfSightBlockedMask);
                c.movementOneWayBlockedMask=mask(c.movementOneWayBlockedMask);c.lineOfSightOneWayBlockedMask=mask(c.lineOfSightOneWayBlockedMask);
                map.cells[std::size_t((flipY?map.height-1-y:y)*map.width+(flipX?map.width-1-x:x))]=c;
            }
            for(auto&f:map.generation.graph.features){for(auto&p:f.points){if(flipX)p.x=map.width-p.x;if(flipY)p.y=map.height-p.y;}if(f.closed && flipX!=flipY)std::reverse(f.points.begin(),f.points.end());}
            for(auto&r:map.routes){if(flipX){r.startX=map.width-1-r.startX;r.endX=map.width-1-r.endX;}if(flipY){r.startY=map.height-1-r.startY;r.endY=map.height-1-r.endY;}}
        }
    }
    map.description=de?u8"Siedlung mit verbundenen Stra\u00dfen, Marktplatz und begehbaren Hausgrundst\u00fccken.":"Settlement with connected streets, a civic square and accessible building plots.";
}
