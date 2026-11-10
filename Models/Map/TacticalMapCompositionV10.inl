// Included inside TacticalMapGenerator's implementation namespace. These
// compositions are opt-in by generation provenance; old saved recipes keep
// their historical layouts.
void composeOpenGroundV10(MapModel &map, std::uint32_t seed)
{
    composeOpenGround(map,seed);
    const int cx=map.width/2, cy=map.height/2;
    // Four weathered pillars frame one usable shrine apron. Their compact
    // footprint reads as a purpose-built place instead of random rubble
    // distributed around the entire battlefield.
    for(int i=0;i<4;++i) {
        const int x=cx+(i&1?2:-2),y=cy+(i&2?2:-2);
        const auto *cell=mapCellAt(map,x,y);
        if(cell && cell->wayKind==MapWayKind::None) {
            setCell(map,x,y,MapTileKind::Cover);
            mapCellAt(map,x,y)->blocksLineOfSight=true;
            addFeature(map,MapGeneratedFeatureKind::TacticalSetpiece,"waystone_"+std::to_string(i),
                       "Weathered waystone","fantasy_neutral.tactical.waystone",1.f,{{x,y}});
        }
    }
    paintEllipse(map,map.width/5,map.height-2,std::max(2,map.width/9),std::max(2,map.height/6),
                 (seed&1u)?MapTileKind::Forest:MapTileKind::Stone,seed^0x40b2u,true);
    paintEllipse(map,map.width*4/5,1,std::max(2,map.width/9),std::max(2,map.height/7),MapTileKind::Forest,seed^0x8302u,true);
    paintEllipse(map,cx,cy,2,2,MapTileKind::Stone);
    setCell(map,cx,cy,MapTileKind::Hazard);
}

void composeOpenGroundV12(MapModel &map, std::uint32_t seed)
{
    composeOpenGroundV10(map,seed);
    const int cx=map.width/2,cy=map.height/2;
    const int radius=std::clamp(std::min(map.width,map.height)/6,3,7);
    // An old circular sanctuary gives the objective an authored footprint.
    // Its eroded edges and two open approaches remain actual walkable terrain.
    for(int y=cy-radius;y<=cy+radius;++y)for(int x=cx-radius;x<=cx+radius;++x) {
        auto *cell=mapCellAt(map,x,y);
        const int distance=(x-cx)*(x-cx)+(y-cy)*(y-cy);
        if(!cell || distance>radius*radius || cell->wayKind!=MapWayKind::None ||
           (cell->kind!=MapTileKind::Empty && cell->kind!=MapTileKind::Dirt && cell->kind!=MapTileKind::Stone))continue;
        if(distance<(radius-1)*(radius-1) || tacticalHash(seed,x,y)%5u!=0u)setCell(map,x,y,MapTileKind::Stone);
    }
    const bool northern=(seed>>4u)%2u==0u;
    const int wallY=cy+(northern?-radius:radius);
    for(int dx=-radius+1;dx<radius;++dx) {
        if(std::abs(dx)<=1 || tacticalHash(seed,dx,wallY)%4u==0u)continue;
        auto *cell=mapCellAt(map,cx+dx,wallY);
        if(!cell || cell->wayKind!=MapWayKind::None || mapGenIsStructureLike(cell->kind))continue;
        setCell(map,cx+dx,wallY,MapTileKind::Cover);
        cell->blocksLineOfSight=true;
    }
    addFeature(map,MapGeneratedFeatureKind::TacticalSetpiece,"sanctuary_apron","Weathered sanctuary","fantasy_neutral.tactical.sanctuary",static_cast<float>(radius),{{cx,cy}});
}

void composeForestRoadV10(MapModel &map, std::uint32_t seed)
{
    fill(map,MapTileKind::Empty);
    const int cy=map.height/2, cx=map.width/2;
    const int variation=static_cast<int>((seed>>5)%3u);
    const int amplitude=std::clamp(map.height/7,2,7);
    const float phase=(seed%13u)*.15f;
    std::vector<Point> main;
    for(int x=0;x<map.width;x+=std::max(1,map.width/12))
        main.push_back({x, std::clamp(cy+static_cast<int>(std::lround(std::sin(x*4.8f/std::max(1,map.width-1)+phase)*amplitude)),3,map.height-4)});
    main.push_back({map.width-1,std::clamp(cy+static_cast<int>(std::lround(std::sin(4.8f+phase)*amplitude)),3,map.height-4)});
    for(int i=0;i<6;++i) {
        int x=map.width*(1+i%3*2)/6 + static_cast<int>(tacticalHash(seed,i,0,0xa31u)%5u)-2;
        int y=i<3 ? (variation==1?map.height/5:1) : (variation==2?map.height*4/5:map.height-2);
        paintEllipse(map,x,y,std::max(3,map.width/(i%2?7:5)),std::max(2,map.height/(i%2?4:5)),MapTileKind::Forest,seed+i*97u,true);
    }
    if(map.generation.recipe.generatorVersion>=11 && map.width>=48 && map.height>=36) {
        // Large battlefields need intermediate tree masses: a border forest
        // alone left thirty-cell empty approaches around a tiny objective.
        // Routes are carved afterwards, so their width and bypass stay usable.
        for(int i=0;i<4;++i) {
            const int gx=map.width*(i%2?2:1)/3 + static_cast<int>(tacticalHash(seed,i,61)%5u)-2;
            const int gy=cy+(i<2?-1:1)*std::max(6,map.height/7);
            paintEllipse(map,gx,gy,std::max(3,map.width/12),std::max(3,map.height/13),
                         MapTileKind::Forest,seed^static_cast<unsigned>(i*137+991),true);
        }
    }
    const int by=std::max(2,cy-amplitude-2), left=std::max(2,cx-map.width/4),right=std::min(map.width-3,cx+map.width/4);
    paintEllipse(map,cx,by+2,std::max(2,map.width/12),2,MapTileKind::Forest,seed^0x88u,true);
    carveRoute(map,"forest_road","Tactical Route 1",main,1,MapTileKind::Dirt,MapWayKind::DirtRoad);
    auto roadY=[&](int x) {return std::clamp(cy+static_cast<int>(std::lround(std::sin(x*4.8f/std::max(1,map.width-1)+phase)*amplitude)),3,map.height-4);};
    std::vector<Point> bypass;
    // Repeated rounded x coordinates made small-map trails zigzag vertically.
    const int bypassSteps=map.generation.recipe.generatorVersion>=12?std::min(16,std::max(1,right-left)):16;
    for(int step=0;step<=bypassSteps;++step) {
        const float t=step/static_cast<float>(bypassSteps);
        const int x=left+static_cast<int>(std::lround((right-left)*t));
        const int y=std::clamp(roadY(x)-static_cast<int>(std::lround(std::sin(t*3.14159265f)*std::max(3,map.height/7))),2,map.height-3);
        if(bypass.empty() || bypass.back().x!=x || bypass.back().y!=y) bypass.push_back({x,y});
    }
    carveRoute(map,"forest_trail","Tactical Route 2",bypass,0,MapTileKind::Dirt,MapWayKind::Trail);
    if(variation==2) paintEllipse(map,std::max(3,map.width/5),2,2,1,MapTileKind::Water,seed,true);
    for(int y=0;y<map.height;++y) for(int x=0;x<map.width;++x)
        if(auto *cell=mapCellAt(map,x,y);cell && cell->kind==MapTileKind::Forest)
            cell->blocksLineOfSight=tacticalHash(seed,x,y,0x76u)%4u==0u;
    setCell(map,cx,roadY(cx)-2,MapTileKind::Hazard);
    if(map.generation.recipe.generatorVersion>=11) {
        // A small roadside rune group gives the woodland objective a visible
        // fantasy focus. These are real cover cells, clear of both routes.
        for(int side : {-1,1}) {
            const int sx=cx+side*2, sy=roadY(cx)-3;
            const auto *cell=mapCellAt(map,sx,sy);
            if(cell && cell->wayKind==MapWayKind::None && cell->kind!=MapTileKind::Water) {
                setCell(map,sx,sy,MapTileKind::Cover);
                mapCellAt(map,sx,sy)->blocksLineOfSight=true;
                addFeature(map,MapGeneratedFeatureKind::TacticalSetpiece,"woodland_waystone_"+std::to_string(side),
                    "Woodland rune stone","fantasy_neutral.tactical.waystone",1.f,{{sx,sy}});
            }
        }
    }
    addChokepoint(map,cx,roadY(cx),"A woodland shrine and fallen trunk shelter the winding side approach");
    addRouteZone(map,0,cy);addRouteZone(map,1,by);
}

void composeVillageStreetV10(MapModel &map, std::uint32_t seed)
{
    fill(map,MapTileKind::Empty);
    paintEllipse(map,1,1,3,2,MapTileKind::Forest,seed,true);
    paintEllipse(map,map.width-2,map.height-2,3,2,MapTileKind::Forest,seed^91u,true);
    const bool varied=map.generation.recipe.recipeVersion>=6;
    const int cy=map.height/2+(varied?int(tacticalHash(seed,1,81)%3u)-1:0);
    const int bend=static_cast<int>(seed%(varied?5u:3u))-(varied?2:1);
    const int upper=std::max(3,cy-4),lower=std::min(map.height-4,cy+4);
    const int fork=map.width/4+(varied?int(tacticalHash(seed,2,82)%3u)-1:0);
    const int join=map.width*3/4+(varied?int(tacticalHash(seed,3,83)%3u)-1:0);
    const int exitY=cy-(varied?1+int(tacticalHash(seed,4,84)%4u):3);
    carveRoute(map,"village_main","Tactical Route 1",{{0,cy+2},{fork,cy+bend},{map.width/2,cy},{join,cy-1},{map.width-1,exitY}},1,MapTileKind::Dirt,MapWayKind::DirtRoad);
    const bool joinedLane=map.generation.recipe.generatorVersion>=12;
    const unsigned layout=varied && map.width>=20 && map.height>=16?tacticalHash(seed,5,85)%3u:0u;
    if(layout==1u)
        carveRoute(map,"village_lane","Tactical Route 2",{{map.width/2-2,0},{map.width/2,cy},
                   {map.width/2+2,lower},{map.width*2/3,map.height-1}},0,MapTileKind::Dirt,MapWayKind::Trail);
    else if(layout==2u)
        carveRoute(map,"village_lane","Tactical Route 2",{{fork,cy+bend},{map.width/3,upper-1},
                   {map.width/2,upper-2},{map.width*2/3,upper},{join,cy-1}},0,MapTileKind::Dirt,MapWayKind::Trail);
    else if(joinedLane)
        carveRoute(map,"village_lane","Tactical Route 2",{{fork,cy+bend},{map.width/3,lower},{map.width/2,lower+1},{map.width*2/3,lower},{join,cy-1}},0,MapTileKind::Dirt,MapWayKind::Trail);
    else
        carveRoute(map,"village_lane","Tactical Route 2",{{2,cy+2},{map.width/4,lower},{map.width/2,lower+1},{map.width*3/4,lower},{map.width-3,cy-2}},0,MapTileKind::Dirt,MapWayKind::Trail);
    for(int x=4,i=0;x<map.width-5;x+=map.generation.recipe.generatorVersion>=11?5+static_cast<int>(tacticalHash(seed,x,73)%4u):6,++i) {
        int w=2+static_cast<int>(tacticalHash(seed,x,1)%2u),h=2+static_cast<int>(tacticalHash(seed,x,2)%2u);
        int y=std::max(1,cy-4-h+(map.generation.recipe.generatorVersion>=11?static_cast<int>(tacticalHash(seed,x,74)%3u)-1:i%2));
        const bool moveSouth=varied && tacticalHash(seed,x,86)%3u==0u;
        if(!moveSouth) {
        const bool added=addBuildingBlock(map,x,y,w,h);
        if(added || !varied) {
        paintSegment(map,{x+w/2,y+h},{x+w/2,cy-1},0,MapTileKind::Dirt,MapWayKind::Trail,false);
        for(int gy=std::max(0,y-4);gy<y-1;++gy) for(int gx=x-1;gx<x+w+1;++gx)
            if(auto *garden=mapCellAt(map,gx,gy);garden && garden->kind==MapTileKind::Empty && garden->wayKind==MapWayKind::None &&
               tacticalHash(seed,gx,gy,0xa61u)%4u!=0u) setCell(map,gx,gy,MapTileKind::Forest);
        }
        }
        if(moveSouth || (map.generation.recipe.generatorVersion>=11?tacticalHash(seed,x,75)%3u!=0u:i%2==0)) {
            int bottom=std::min(map.height-h-1,lower+2);
            addBuildingBlock(map,x+1,bottom,w,h);
            paintSegment(map,{x+1,bottom-1},{x+1,lower},0,MapTileKind::Dirt,MapWayKind::Trail,false);
            for(int gy=bottom+h+1;gy<std::min(map.height,bottom+h+3);++gy) for(int gx=x;gx<x+w+2;++gx)
                if(auto *garden=mapCellAt(map,gx,gy);garden && garden->kind==MapTileKind::Empty && garden->wayKind==MapWayKind::None &&
                   tacticalHash(seed,gx,gy,0xa71u)%3u!=0u) setCell(map,gx,gy,MapTileKind::Forest);

        }
    }
    if(map.generation.recipe.generatorVersion>=11) {
        // On a small map seeded row omissions can remove every southern
        // cottage. Keep one dry, route-safe frontage so this reads as a
        // village street rather than three houses beside an empty field.
        bool southernHouse=false;
        for(int y=lower;y<map.height;++y) for(int x=0;x<map.width;++x)
            if(mapCellAt(map,x,y)->kind==MapTileKind::Building) southernHouse=true;
        if(!southernHouse) {
            bool placed=false;
            for(int y=map.height-4;y<=map.height-3 && !placed;++y)
                for(int x=2;x+2<map.width-1 && !placed;++x) {
                    bool clear=true;
                    for(int dy=0;dy<2;++dy) for(int dx=0;dx<3;++dx) {
                        const auto *cell=mapCellAt(map,x+dx,y+dy);
                        clear=clear && cell && cell->kind==MapTileKind::Empty && cell->wayKind==MapWayKind::None;
                    }
                    if(clear) { const bool added=addBuildingBlock(map,x,y,3,2); placed=map.generation.recipe.generatorVersion>=12?added:true; }
                }
        }
    }
    if(map.generation.recipe.generatorVersion>=12) {
        for(int south=0;south<2;++south) {
            const int y0=south?lower+1:1,y1=south?map.height-2:cy-3;
            int houses=0;
            for(int y=y0;y<=y1;++y)for(int x=1;x<map.width-1;++x) {
                const auto *c=mapCellAt(map,x,y),*left=mapCellAt(map,x-1,y),*above=mapCellAt(map,x,y-1);
                if(c && c->kind==MapTileKind::Building && (!left || left->kind!=MapTileKind::Building) && (!above || above->kind!=MapTileKind::Building))++houses;
            }
            for(int y=y0;y+1<=y1 && houses<2;++y)for(int x=3;x+2<map.width-3 && houses<2;++x) {
                bool clear=true;
                for(int oy=0;oy<2;++oy)for(int ox=0;ox<3;++ox) {
                    const auto *c=mapCellAt(map,x+ox,y+oy);
                    clear=clear && c && c->wayKind==MapWayKind::None && (c->kind==MapTileKind::Empty || c->kind==MapTileKind::Dirt || c->kind==MapTileKind::Stone);
                }
                if(clear && addBuildingBlock(map,x,y,3,2))++houses;
            }
        }
    }
    setCell(map,map.width/2,cy-2,MapTileKind::Hazard);
    addChokepoint(map,map.width/2,cy,"Wayside well and market stalls overlook the village bend");
    addRouteZone(map,0,cy);addRouteZone(map,1,lower);
}

void composeCitySquareV10(MapModel &map,std::uint32_t seed)
{
    fill(map,MapTileKind::Stone);
    const bool v11=map.generation.recipe.generatorVersion>=11;
    const int cx=map.width/2,cy=map.height/2;
    const int rx=std::max(4,map.width/5+(v11?static_cast<int>((seed>>3)%3u)-1:0));
    const int ry=std::max(3,map.height/5+(v11?static_cast<int>((seed>>8)%3u)-1:0));
    const int upper=cy-ry,lower=cy+ry;
    carveRoute(map,"square_north","Tactical Route 1",{{0,upper},{cx-rx,upper},{cx,upper-1},{cx+rx,upper},{map.width-1,upper}},1,MapTileKind::Stone,MapWayKind::StoneRoad);
    carveRoute(map,"square_south","Tactical Route 2",{{0,lower},{cx-rx,lower},{cx+rx,lower},{map.width-1,lower}},1,MapTileKind::Stone,MapWayKind::StoneRoad);
    carveRoute(map,"square_cross","Square Cross Street",{{cx,0},{cx,cy-3},{cx+4,cy-3},{cx+4,cy+3},{cx,cy+3},{cx,map.height-1}},0,MapTileKind::Stone,MapWayKind::StoneRoad);
    // Dense frontage establishes the square's silhouette; openings between
    // blocks are real alleys and roofs exactly match collision geometry.
    for(int x=3;x<map.width-4;x+=v11?5+static_cast<int>(tacticalHash(seed,x,41)%4u):6) {
        int w=3+static_cast<int>(tacticalHash(seed,x,3)%2u);
        if(x <= cx && x+w > cx) continue; // retain a complete entrance boulevard
        const int setback=static_cast<int>(tacticalHash(seed,x,8)%(v11?4u:3u));
        addBuildingBlock(map,x,1+setback,w,std::max(2,upper-3-setback));
        addBuildingBlock(map,x+(setback==2?1:0),lower+2+setback,w,std::max(2,map.height-lower-4-setback));
    }
    for(int y=upper+2;y<lower-1;y+=5) {
        const int inset=static_cast<int>(tacticalHash(seed,y,9)%3u);
        addBuildingBlock(map,1+inset,y,std::max(2,cx-rx-3-inset),2+static_cast<int>((seed+y)%2u));
        addBuildingBlock(map,cx+rx+2,y+inset%2,std::max(2,map.width-cx-rx-4-inset),3);
    }
    if(v11) {
        // A setback may reject an entire block at a street. Retain a legible
        // frontage in each corner without painting over any movement route.
        for(int south=0;south<2;++south) for(int east=0;east<2;++east) {
            const int x0=east?cx+2:1,x1=east?map.width-2:cx-2;
            const int y0=south?lower+2:1,y1=south?map.height-2:upper-2;
            bool occupied=false;
            for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x)
                if(const auto *cell=mapCellAt(map,x,y);cell && cell->kind==MapTileKind::Building) occupied=true;
            if(occupied) continue;
            bool placed=false;
            for(int y=y0;y+1<=y1 && !placed;++y) for(int x=x0;x+2<=x1 && !placed;++x) {
                bool clear=true;
                for(int dy=0;dy<2;++dy) for(int dx=0;dx<3;++dx) {
                    const auto *cell=mapCellAt(map,x+dx,y+dy);
                    clear=clear && cell && cell->kind==MapTileKind::Stone && cell->wayKind==MapWayKind::None;
                }
                if(clear) { addBuildingBlock(map,x,y,3,2); placed=true; }
            }
        }
    }
    paintEllipse(map,cx,cy,2,2,MapTileKind::Water);
    setCell(map,cx-3,cy-2,MapTileKind::Hazard);
    setCell(map,cx-3,cy,MapTileKind::Cover);setCell(map,cx+3,cy,MapTileKind::Cover);
    addChokepoint(map,cx,cy,"An old fountain divides the square into two covered approaches");
    addRouteZone(map,0,upper);addRouteZone(map,1,lower);
}

void composeCaveV10(MapModel &map,std::uint32_t seed)
{
    fill(map,MapTileKind::Wall);
    const int upper=std::max(3,map.height/3),lower=std::min(map.height-4,map.height*2/3);
    const int cy=map.height/2;
    const int delta=static_cast<int>(seed%5u)-2;
    std::vector<Point> north{{1,cy},{map.width/4,upper-1},{map.width/2,upper+delta},
                             {map.width*3/4,upper-1},{map.width-2,cy}};
    std::vector<Point> south{{1,cy},{map.width/4,lower+1},{map.width/2,lower-delta},
                             {map.width*3/4,lower},{map.width-2,cy}};
    for(int i=0;i<5;++i) {
        int x=map.width*(1+i*2)/10;
        int y=i==0 || i==4?cy:(i%2?upper:lower);
        y=std::clamp(y+static_cast<int>(tacticalHash(seed,i,4)%5u)-2,3,map.height-4);
        x=std::clamp(x+static_cast<int>(tacticalHash(seed,i,5)%5u)-2,2,map.width-3);
        int rx=std::max(3,map.width/(7+static_cast<int>(tacticalHash(seed,i,6)%5u)));
        int ry=std::max(3,map.height/(3+static_cast<int>(tacticalHash(seed,i,7)%4u)));
        paintEllipse(map,x,y,rx,ry,MapTileKind::Cave,seed+i*117u,true);
    }
    // Both deployment bands need real entrance floor even when the seeded
    // gallery lobes move away from an edge. Rounded entrance chambers connect
    // the two route mouths without adding a rectangular perimeter corridor.
    paintEllipse(map,2,cy,3,std::max(3,lower-cy+2),MapTileKind::Cave,seed^0x512u,true);
    paintEllipse(map,map.width-3,cy,3,std::max(3,lower-cy+2),MapTileKind::Cave,seed^0x721u,true);
    carveRoute(map,"cave_upper","Tactical Route 1",north,1,MapTileKind::Cave,MapWayKind::CaveTunnel,false,false);
    carveRoute(map,"cave_lower","Tactical Route 2",south,1,MapTileKind::Cave,MapWayKind::CaveTunnel,false,false);
    // The central rock island separates the flank route instead of a
    // rectangular corridor wrapped around every outside edge.
    paintEllipse(map,map.width/2,cy,std::max(1,map.width/16),1,MapTileKind::Wall,seed,true);
    // Intersecting galleries can leave single-cell teeth and thin rock ribbons.
    // Keep supported rock masses; remove these fragments as real floor so the
    // picture and movement/line-of-sight metadata continue to agree.
    std::vector<Point> unsupportedRock;
    const auto rockAt=[&](int x,int y) {
        const auto *cell=mapCellAt(map,x,y);
        return cell && cell->kind==MapTileKind::Wall;
    };
    for(int y=1;y<map.height-1;++y) for(int x=1;x<map.width-1;++x) {
        if(!rockAt(x,y)) continue;
        bool supported=false;
        for(int dy=-1;dy<=0;++dy) for(int dx=-1;dx<=0;++dx)
            supported|=rockAt(x+dx,y+dy) && rockAt(x+dx+1,y+dy)
                    && rockAt(x+dx,y+dy+1) && rockAt(x+dx+1,y+dy+1);
        if(!supported) unsupportedRock.push_back({x,y});
    }
    for(const auto &point:unsupportedRock) setCell(map,point.x,point.y,MapTileKind::Cave);
    setCell(map,map.width/2,lower,MapTileKind::Hazard);
    addChokepoint(map,map.width/2,upper,"Two uneven galleries circle a limestone island");
    addRouteZone(map,0,upper);addRouteZone(map,1,lower);
}

void composeStoneDungeonV10(MapModel &map,std::uint32_t seed)
{
    fill(map,MapTileKind::Wall);
    const int cy=map.height/2,upper=std::max(3,map.height/3),lower=std::min(map.height-4,map.height*2/3);
    const int left=map.width/4,right=map.width*3/4;
    const int hallX=map.width/2+static_cast<int>(seed%5u)-2;
    const int rightLower=std::min(map.height-3,lower+static_cast<int>((seed>>5)%3u));
    const int roomW=std::max(4,map.width/6),roomH=std::max(4,map.height/4);
    paintRect(map,1,upper-1,roomW,lower-upper+3,MapTileKind::Room);
    paintRect(map,map.width-roomW-1,upper-1,roomW,lower-upper+3,MapTileKind::Room);
    paintRect(map,hallX-roomW/2,std::max(1,upper-roomH+2),roomW,roomH,MapTileKind::Room);
    paintRect(map,left-roomW/2,lower-1,roomW,std::min(roomH,map.height-lower),MapTileKind::Room);
    paintRect(map,right-roomW/2,rightLower-1,roomW+static_cast<int>((seed>>8)%3u),std::min(roomH,map.height-rightLower),MapTileKind::Room);
    // One side chapel survives beyond the loop; it is a real accessible room.
    if(map.width>=32) paintRect(map,left-1,std::max(1,upper-roomH/2),std::max(3,roomW-1),roomH/2+2,MapTileKind::Room);
    carveRoute(map,"dungeon_upper","Tactical Route 1",{{1,cy},{left,cy},{left,upper},{hallX,upper},{right,upper},{right,cy},{map.width-2,cy}},1,MapTileKind::Room,MapWayKind::DungeonCorridor,false,false);
    carveRoute(map,"dungeon_lower","Tactical Route 2",{{left,cy},{left,lower},{hallX,lower},{hallX,rightLower},{right,rightLower},{right,cy}},1,MapTileKind::Room,MapWayKind::DungeonCorridor,false,false);
    // Offset transverse passage supports improvisation and avoids a single
    // repeated mirrored dumbbell as the only dungeon encounter composition.
    carveRoute(map,"dungeon_link","Sanctum Passage",{{hallX,upper},{hallX,lower}},0,MapTileKind::Room,MapWayKind::DungeonCorridor,false,false);
    setCell(map,left,upper,MapTileKind::Door,MapWayKind::DungeonCorridor);
    setCell(map,right,rightLower,MapTileKind::Door,MapWayKind::DungeonCorridor);
    setCell(map,hallX,upper-1,MapTileKind::Hazard);
    addChokepoint(map,left,upper,"A guarded arch opens toward the offset sanctum");
    addRouteZone(map,0,upper);addRouteZone(map,1,lower);
}

void composeRuinV10(MapModel &map,std::uint32_t seed)
{
    composeRuinV8(map,seed);
    const int variant=static_cast<int>((seed>>5)%3);
    const int left=std::max(5,map.width/4-(variant==1?1:0));
    const int right=std::min(map.width-5,map.width*3/4+(variant==2?1:0));
    const int top=std::max(3,map.height/4-static_cast<int>((seed>>9)%3));
    const int bottom=std::min(map.height-3,map.height*3/4+(variant==1?1:0));
    const int centre=map.height/2;
    // A collapsed corner and surviving stepped apse make the outline read as
    // a ruin in the landscape, rather than a pristine rectangle plus rubble.
    const bool east=(seed&1u)!=0u;
    const int cornerWidth=std::max(3,(right-left)/3);
    for(int y=bottom-2;y<=bottom;++y) for(int i=0;i<cornerWidth;++i) {
        const int x=east?right-i:left+i;
        const auto *cell=mapCellAt(map,x,y);
        if(cell && cell->wayKind==MapWayKind::None)
            setCell(map,x,y,(i==cornerWidth-1 && y==bottom-2)?MapTileKind::Rubble:MapTileKind::Empty);
    }
    if(map.width>=36)
        for(int x=left+4;x<right-3;x+=6) for(int y:{top+3,bottom-3})
            if(const auto *cell=mapCellAt(map,x,y);cell && cell->kind==MapTileKind::Stone && cell->wayKind==MapWayKind::None)
                setCell(map,x,y,MapTileKind::Wall);
    if(variant==1 && centre-top>=4) {
        const int partition=left+std::max(3,(right-left)/3);
        for(int y=top+1;y<centre-1;++y) {
            const auto *cell=mapCellAt(map,partition,y);
            if(cell && cell->kind==MapTileKind::Stone && cell->wayKind==MapWayKind::None && y!=top+3)
                setCell(map,partition,y,MapTileKind::Wall);
        }
    }
    else if(variant==2 && bottom-centre>=4) {
        for(int x=right-5;x<right-1;++x)
            if(const auto *cell=mapCellAt(map,x,bottom-2);cell && cell->wayKind==MapWayKind::None && x!=right-3)
                setCell(map,x,bottom-2,MapTileKind::Wall);
    }
    if(map.generation.recipe.generatorVersion>=11 && variant==0) {
        // One ruin family preserves a ritual court: a pair of standing rune
        // stones gives the open interior a focal point and usable cover.
        for(int side : {-1,1}) {
            const int sx=map.width/2+side*2, sy=centre-1;
            const auto *cell=mapCellAt(map,sx,sy);
            if(cell && cell->kind==MapTileKind::Stone && cell->wayKind==MapWayKind::None) {
                setCell(map,sx,sy,MapTileKind::Cover);
                mapCellAt(map,sx,sy)->blocksLineOfSight=true;
                addFeature(map,MapGeneratedFeatureKind::TacticalSetpiece,"ruin_waystone_"+std::to_string(side),
                    "Ritual court stone","fantasy_neutral.tactical.waystone",1.f,{{sx,sy}});
            }
        }
    }

}

void composeStreetFurnitureV12(MapModel &map,unsigned seed)
{
    int placed=0;
    for(int index=0;index<map.width*map.height;++index) {
        const int offset=(index+int(seed%unsigned(map.width*map.height)))%(map.width*map.height);
        const int x=offset%map.width,y=offset/map.width;
        if(x<3 || y<2 || x+2>=map.width-3 || y>=map.height-2)continue;
        bool clear=true,frontage=false;
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=2;++dx) {
            const auto *c=mapCellAt(map,x+dx,y+dy);
            clear=clear && c && c->wayKind==MapWayKind::None &&
                (c->kind==MapTileKind::Empty || c->kind==MapTileKind::Dirt || c->kind==MapTileKind::Stone);
        }
        for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=4;++dx) {
            const auto *c=mapCellAt(map,x+dx,y+dy);frontage=frontage || (c && c->kind==MapTileKind::Building);
        }
        if(!clear || !frontage)continue;
        for(int dx=0;dx<2;++dx){setCell(map,x+dx,y,MapTileKind::Cover);mapCellAt(map,x+dx,y)->blocksLineOfSight=false;}
        addFeature(map,MapGeneratedFeatureKind::TacticalSetpiece,"market_stall_"+std::to_string(placed),"Market stall","fantasy_neutral.tactical.market_stall",2.f,{{x,y},{x+1,y}});
        if(++placed>=std::clamp(map.width/14,1,3))break;
    }
}

void refineTacticalPathsV12(MapModel &map)
{
    const auto road=[](const auto &f){return f.kind==MapGeneratedFeatureKind::Road || f.kind==MapGeneratedFeatureKind::Trail;};
    std::vector<MapFeaturePoint> junctions;
    for(const auto &f:map.generation.graph.features)if(road(f) && f.points.size()>1){junctions.push_back(f.points.front());junctions.push_back(f.points.back());}
    for(auto &f:map.generation.graph.features) {
        if(!road(f) || f.locked || f.points.size()<3)continue;
        auto original=f.points;
        const auto clearPoint=[&](MapFeaturePoint p){const auto *c=mapCellAt(map,int(p.x),int(p.y));return c &&
            (c->kind==MapTileKind::Empty || c->kind==MapTileKind::Dirt || c->kind==MapTileKind::Stone || c->kind==MapTileKind::Road) && !mapWayKindIsWaterLike(c->wayKind);};
        // Remove raster stair controls only when their dry chord stays within
        // the original corridor and does not skip a authored junction.
        std::vector<MapFeaturePoint> simplified{original.front()};
        for(std::size_t begin=0;begin+1<original.size();) {
            std::size_t chosen=begin+1;
            for(std::size_t end=begin+2;end<original.size() && end<=begin+8;++end) {
                const auto a=original[begin],z=original[end];const float dx=z.x-a.x,dy=z.y-a.y,length=std::hypot(dx,dy);
                if(length<.01f || length>6.f)break;
                bool safe=true;
                for(std::size_t mid=begin+1;mid<end;++mid) {
                    const auto p=original[mid];
                    if(std::abs(dx*(a.y-p.y)-(a.x-p.x)*dy)/length>.65f ||
                       std::any_of(junctions.begin(),junctions.end(),[&](auto j){return std::hypot(j.x-p.x,j.y-p.y)<.05f;})){safe=false;break;}
                }
                const int steps=std::max(2,int(std::ceil(length*5.f)));
                for(int k=0;k<=steps && safe;++k){const float t=float(k)/steps;safe=clearPoint({a.x+dx*t,a.y+dy*t});}
                if(safe)chosen=end;
            }
            simplified.push_back(original[chosen]);begin=chosen;
        }
        original=std::move(simplified);std::vector<MapFeaturePoint> curve{original.front()};
        for(std::size_t i=1;i+1<original.size();++i) {
            const auto a=original[i-1],b=original[i],z=original[i+1];
            if(std::any_of(junctions.begin(),junctions.end(),[&](auto p){return std::hypot(p.x-b.x,p.y-b.y)<.05f;})){curve.push_back(b);continue;}
            const float before=std::hypot(a.x-b.x,a.y-b.y),after=std::hypot(z.x-b.x,z.y-b.y);
            const float radius=std::min(2.f,std::min(before,after)*.44f);
            if(radius<.1f){curve.push_back(b);continue;}
            const MapFeaturePoint entry{b.x+(a.x-b.x)*radius/before,b.y+(a.y-b.y)*radius/before};
            const MapFeaturePoint exit{b.x+(z.x-b.x)*radius/after,b.y+(z.y-b.y)*radius/after};
            std::vector<MapFeaturePoint> arc;bool clear=true;
            for(int k=0;k<=12;++k) {
                const float t=k/12.f,u=1.f-t;
                const MapFeaturePoint p{u*u*entry.x+2*u*t*b.x+t*t*exit.x,u*u*entry.y+2*u*t*b.y+t*t*exit.y};
                const auto *c=mapCellAt(map,int(p.x),int(p.y));
                clear=clear && c && (c->kind==MapTileKind::Empty || c->kind==MapTileKind::Dirt || c->kind==MapTileKind::Stone || c->kind==MapTileKind::Road) && !mapWayKindIsWaterLike(c->wayKind);
                arc.push_back(p);
            }
            if(clear)curve.insert(curve.end(),arc.begin(),arc.end());else curve.push_back(b);
        }
        curve.push_back(original.back());f.points=std::move(curve);
        for(auto p:f.points) {
            auto *c=mapCellAt(map,int(p.x),int(p.y));
            if(c && (c->kind==MapTileKind::Empty || c->kind==MapTileKind::Dirt || c->kind==MapTileKind::Stone || c->kind==MapTileKind::Road))
                c->wayKind=f.kind==MapGeneratedFeatureKind::Trail?MapWayKind::Trail:f.materialId.find(".stone")!=std::string::npos?MapWayKind::StoneRoad:MapWayKind::DirtRoad;
        }
    }
}
