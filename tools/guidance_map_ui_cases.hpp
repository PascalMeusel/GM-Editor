// Render only real marker/travel controls via friend access: never the map
// canvas, texture cache, generation worker, audio service or native backend.
void guidanceMapControls(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Real marker and watch examples 27px ")+(de?"DE":"EN"));
    Campaign c;c.name="Guidance travel "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    c.clock.useSystemTime=false;c.clock.running=false;c.clock.anchorAbsoluteMinutes=700;
    ItemModel ration;ration.name="Synthetic ration";c.customItems.push_back(ration);
    c.partyInventory.entries.emplace_back(ration.metadata.id,19);
    for(int i=0;i<3;++i){CharacterModel actor;std::snprintf(actor.nameBuffer,sizeof(actor.nameBuffer),"Traveler %d",i);
        actor.maxHitPoints=actor.currentHitPoints=10+i;c.characters.push_back(actor);}
    MapModel map;map.name="Guidance atlas";map.width=12;map.height=9;map.atlasLevel=MapLevel::Region;
    MapTokenModel token;token.name="Unmoved character token";token.linkedEntityType="Character";
    token.linkedEntityId=c.characters[0].metadata.id;token.x=2;token.y=3;map.tokens.push_back(token);
    MapGeneratedFeature road;road.name="Illustrated road without travel route";road.kind=MapGeneratedFeatureKind::Road;
    road.points={{1,1},{4,4}};map.generation.graph.features.push_back(road);
    c.maps.push_back(map);c.partyLocation={map.metadata.id,1,1};
    require(ProjectSaveService::getInstance().saveCampaign(c,"guidance travel fixture"),"Cannot save travel fixture");
    MapViewerController viewer(c,nullptr);require(viewer.selectMapById(map.metadata.id),"Cannot select atlas fixture");
    SessionEventCounter events(c.metadata.id);bool travel=false;const auto actors=c.characters;
    ui.draw=[&]{if(travel)UiGuidanceTestProbe::travel(viewer);else {UiGuidanceTestProbe::marker(viewer);
        ImGui::Button("After marker controls");}};ui.frame(4);
    const char *place=de?"Gruppenmarker setzen":"Place party marker";
    const char *save=de?"Gruppenposition speichern":"Save party location";
    const char *advance=de?"Reiseabschnitt fortschreiben":"Advance travel watch";
    const auto untouched=[&](const Campaign &value){
        const auto &m=correctionById(value.maps,map.metadata.id);
        require(m.tokens.size()==1&&m.tokens[0].metadata.id==token.metadata.id&&m.tokens[0].x==2&&m.tokens[0].y==3&&
            value.characters.size()==3&&value.partyInventory.entries.size()==1&&value.partyInventory.entries[0].quantity==19&&
            value.clock.anchorAbsoluteMinutes==700&&!value.clock.running,"Marker/watch moved token, deducted inventory or advanced game clock");
        for(const auto &a:value.characters)require(a.currentHitPoints==correctionById(actors,a.metadata.id).currentHitPoints,
            "Marker/watch changed source character health");};
    ui.compactClick(place);ui.reviewState(save,true);
    const auto initialRow=ui.compactItem("After marker controls");
    UiGuidanceTestProbe::destination(viewer,5,4);ui.frame(3);
    const auto stableRow=[&]{const auto row=ui.compactItem("After marker controls");
        require(row.minimum.x==initialRow.minimum.x&&row.minimum.y==initialRow.minimum.y&&
            row.maximum.x==initialRow.maximum.x&&row.maximum.y==initialRow.maximum.y,
            "Pending marker confirmation moved the following canvas/control row");};
    stableRow();ui.compactClick(gm::localization::trUi("Cancel"));stableRow();
    require(events.saves==0&&c.partyLocation.x==1&&c.partyLocation.y==1,"Cancelling marker preview saved/moved the campaign");
    UiGuidanceTestProbe::destination(viewer,5,4);ui.frame(3);stableRow();
    const auto pending=UiGuidanceTestProbe::state(viewer);guidanceNoAction(ui);
    require(UiGuidanceTestProbe::state(viewer)==pending&&events.saves==0&&c.partyLocation.x==1&&c.partyLocation.y==1,
        "Marker guidance changed pending destination, canvas/view state or persisted location");
    ui.compactClick(save);require(events.saves==1,"Marker Save did not commit exactly once");
    for(const auto &value:{c,menuReload(c.metadata.id)}){untouched(value);
        require(value.partyLocation.mapId==map.metadata.id&&value.partyLocation.x==5&&value.partyLocation.y==4&&
            value.partyTravel.watches==0&&value.partyTravel.milesTraveled==0&&value.partyTravel.rationsUsed==0,
            "Marker placement altered travel progress or failed to persist the exact cell");}
    travel=true;ui.frame(4);ui.compactReach(advance);ui.reviewState(advance,true);
    ui.compactReach(gm::localization::trUi("Start Route"));ui.reviewState(gm::localization::trUi("Start Route"),true);
    require(contains(ui.rendered,de?"Gezeichnete Straßen":"Illustrated roads"),"Route prerequisite omits illustrated-road distinction");
    const auto beforeHelp=UiGuidanceTestProbe::state(viewer);guidanceNoAction(ui);
    require(UiGuidanceTestProbe::state(viewer)==beforeHelp&&events.saves==1&&c.partyTravel.watches==0,
        "Travel guidance started route or changed selection/progress");
    // Fixture setup supplies the authored route. Every subsequent start/watch
    // operation uses a real observed production button and actual SQLite save.
    MapRouteModel route;route.name="Seventeen mile route";route.distanceMiles=17;c.maps[0].routes.push_back(route);
    const char *paces[]={"Slow","Normal","Fast"};const int expected[]={6,8,10};
    for(int i=0;i<3;++i){
        c.partyTravel.travelPace=paces[i];c.partyTravel.routeId.clear();c.partyTravel.currentMapId.clear();
        c.partyTravel.distanceMiles=0;c.partyTravel.milesTraveled=0;c.partyTravel.watches=0;
        c.partyTravel.rationsUsed=0;c.partyTravel.randomEncounterChecks=0;ui.frame(4);
        ui.compactClick(gm::localization::trUi("Start Route"));
        ui.compactClick(advance);
        const auto saved=menuReload(c.metadata.id);const auto &t=saved.partyTravel;
        require(t.routeId==route.metadata.id&&t.milesTraveled==expected[i]&&t.watches==1&&t.rationsUsed==3&&
            t.randomEncounterChecks==1&&saved.partyLocation.x==5&&saved.partyLocation.y==4,
            "Actual watch differs from independent slow/normal/fast example or changes marker");untouched(saved);
    }
    ui.compactClick(advance);ui.compactReach(advance);ui.reviewState(advance,true);
    auto saved=menuReload(c.metadata.id);
    require(saved.partyTravel.milesTraveled==17&&saved.partyTravel.watches==2&&saved.partyTravel.rationsUsed==6&&
        saved.partyTravel.randomEncounterChecks==2&&events.saves==8,"Fast watch cap/count differs or disabled watch saved again");
    untouched(saved);
    c.characters.clear();c.partyTravel.milesTraveled=0;c.partyTravel.rationsUsed=0;c.partyTravel.watches=0;
    c.partyTravel.randomEncounterChecks=0;c.partyTravel.travelPace="Normal";ui.frame(3);ui.compactClick(advance);
    saved=menuReload(c.metadata.id);require(saved.partyTravel.rationsUsed==1&&saved.partyTravel.milesTraveled==8&&
        saved.partyTravel.watches==1&&saved.partyInventory.entries[0].quantity==19,
        "Empty campaign-character roster does not use the documented minimum-one ration tally");
}
