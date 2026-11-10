// Typed patch tests use the real World mutation/normalization functions.
// Persistence and controller lifecycle are covered separately by UI fixtures.
namespace WorldHistoryCases
{
using WorldInspectorDrafts::Draft;
using WorldInspectorDrafts::Scope;
Campaign fixture()
{
    Campaign c;c.name="World history model fixture";
    WorldPlaceModel p;p.metadata.id="place-a";p.name="Harbor";p.description="Original description";p.notes="Original notes";
    p.hasMapPosition=true;p.mapPosition={.2,.3};c.worldPlaces.push_back(p);
    WorldPlaceModel other;other.metadata.id="place-b";other.name="Other harbor";c.worldPlaces.push_back(other);
    NationModel n;n.metadata.id="nation-a";n.name="Coast";n.government="Council";n.culture="Original culture";
    for(const char *id:{"territory-a","territory-b"}){NationTerritoryModel t;t.metadata.id=id;t.name=id;t.polygon={{.2,.3},{.4,.5}};n.territories.push_back(t);}
    c.nations.push_back(n);
    ShopModel shop;shop.metadata.id="shop-a";shop.name="Lantern shop";shop.description="Original merchant prose";
    shop.stock={{"item-a",5},{"item-b",8}};c.shops.push_back(shop);
    return c;
}
void addMaps(Campaign &c)
{
    c.worldPlaces[0].linkedMapId="map-a";c.worldPlaces[0].linkedFeatureId="place-feature";
    c.worldPlaces[1].linkedMapId="map-b";c.worldPlaces[1].linkedFeatureId="other-feature";
    for(const char *id:{"map-a","map-b"}) {
        MapModel m;m.metadata.id=id;m.width=m.height=2;m.cells.resize(4);
        MapGeneratedFeature nation;nation.id="nation-feature";nation.kind=MapGeneratedFeatureKind::Nation;nation.name="Coast";
        nation.gmOnly=true;nation.discovered=false;nation.points={{0,0},{1,1}};
        MapGeneratedFeature p;p.id=std::string(id)=="map-a"?"place-feature":"other-feature";
        p.kind=MapGeneratedFeatureKind::Settlement;p.name=std::string(id)=="map-a"?"Harbor":"Other harbor";
        p.gmOnly=std::string(id)=="map-a";p.discovered=!p.gmOnly;p.points={{.5f,.5f}};
        m.generation.graph.features={nation,p};
        m.generation.recipe.facts=mapSetupFacts({{"nation-a","Coast",true,true,{},nation.id}});
        c.maps.push_back(m);
    }
}
WorldInspectorHistory::Patch patch(const Campaign &before,const Campaign &after,Scope scope=Scope::all())
{
    auto captured=WorldInspectorHistory::capture(before,after,scope);
    require(captured.success&&!captured.patch.empty(),"World committed change did not produce a typed patch");return std::move(captured.patch);
}
void authoredFields()
{
    const auto before=fixture();auto after=before;WorldInspectorDrafts::Store drafts;
    auto p=before.worldPlaces[0];p.name="Edited harbor";p.notes="Edited private notes";drafts.retain(Draft<WorldPlaceModel>{before.worldPlaces[0],p});
    auto n=before.nations[0];n.government="Assembly";n.territories[0].name="Edited territory";drafts.retain(Draft<NationModel>{before.nations[0],n});
    auto s=before.shops[0];s.description="Edited merchant prose";s.stock[0].availability="Reserved";drafts.retain(Draft<ShopModel>{before.shops[0],s});
    require(drafts.apply(after,Scope::all()).applied(),"Cannot commit typed World fixture");const auto delta=patch(before,after);
    after.worldPlaces[0].description="Newer independent description";after.worldPlaces[0].metadata.customProperties["canary"]="keep";
    after.nations[0].culture="Newer culture";after.nations[0].factionIds={"new faction"};
    after.shops[0].stock[0].quantity=17;after.shops[0].inventoryId="new inventory";
    std::swap(after.nations[0].territories[0],after.nations[0].territories[1]);std::swap(after.shops[0].stock[0],after.shops[0].stock[1]);
    NationTerritoryModel fresh;fresh.metadata.id="newer-territory";fresh.name="Later independent territory";
    after.nations[0].territories.push_back(fresh);after.shops[0].stock.emplace_back("later-item",23);
    require(WorldInspectorHistory::apply(after,delta,false).applied(),"Strict World undo failed");
    require(after.worldPlaces[0].name=="Harbor"&&after.worldPlaces[0].notes=="Original notes"&&after.worldPlaces[0].description=="Newer independent description"&&
        after.worldPlaces[0].metadata.customProperties.at("canary")=="keep"&&after.nations[0].government=="Council"&&
        after.nations[0].culture=="Newer culture"&&after.nations[0].factionIds==std::vector<EntityId>{"new faction"}&&
        after.nations[0].territories[0].metadata.id=="territory-b"&&after.nations[0].territories[1].name=="territory-a"&&
        after.shops[0].stock[0].itemId=="item-b"&&after.shops[0].stock[1].quantity==17&&after.shops[0].stock[1].availability==before.shops[0].stock[0].availability&&
        after.shops[0].inventoryId=="new inventory"&&after.nations[0].territories.size()==3&&after.nations[0].territories.back().name=="Later independent territory"&&
        after.shops[0].stock.size()==3&&after.shops[0].stock.back().quantity==23,"World undo replaced unrelated fields, metadata or child order");
    require(WorldInspectorHistory::apply(after,delta,true).applied()&&after.worldPlaces[0].name=="Edited harbor"&&
        after.nations[0].territories[1].name=="Edited territory"&&after.shops[0].stock[1].availability=="Reserved","World redo lost typed edits");
    after.shops[0].description="Conflicting later merchant prose";
    require(!WorldInspectorHistory::apply(after,delta,false).applied()&&after.worldPlaces[0].name=="Edited harbor"&&
        after.nations[0].government=="Assembly"&&after.shops[0].description=="Conflicting later merchant prose",
        "Late multi-record conflict partially changed earlier records");
}
void normalizationAndRefusal()
{
    const auto before=fixture();auto after=before;auto p=after.worldPlaces[0];p.mapPosition={-2,3};
    auto n=after.nations[0];n.territories[0].polygon={{-1,2},{.4,.5}};
    require(WorldInspector::updateWorldPlace(after,p.metadata.id,p).applied()&&WorldInspector::updateNation(after,n.metadata.id,n).applied(),
        "Cannot produce normalized World save");const auto delta=patch(before,after);
    require(after.worldPlaces[0].mapPosition.x==0&&after.worldPlaces[0].mapPosition.y==1&&after.nations[0].territories[0].polygon[0].y==1,
        "Positive control did not normalize authored geometry");
    require(WorldInspectorHistory::apply(after,delta,false).applied()&&after.worldPlaces[0].mapPosition.x==.2&&
        after.nations[0].territories[0].polygon[0].y==.3,"Undo did not restore original normalized geometry");
    require(WorldInspectorHistory::apply(after,delta,true).applied()&&after.worldPlaces[0].mapPosition.y==1&&
        after.nations[0].territories[0].polygon[0].x==0,"Redo restored raw invalid input rather than committed geometry");
    auto raw=fixture();raw.nations[0].territories[0].polygon[0]={-2,3};auto details=raw;
    auto edited=raw.nations[0];edited.description="Details save";edited.government="Assembly";
    WorldInspectorDrafts::Store detailDraft;detailDraft.retain(Draft<NationModel>{raw.nations[0],edited});
    const Scope detailsScope{WorldInspectorDrafts::Part::NationDetails,"nation-a",{}};
    require(detailDraft.apply(details,detailsScope).applied()&&details.nations[0].territories[0].polygon[0].x==0&&
        details.nations[0].territories[0].polygon[0].y==1,"NationDetails save did not normalize its existing territory positive control");
    const auto normalizedDetails=patch(raw,details,detailsScope);
    require(WorldInspectorHistory::apply(details,normalizedDetails,false).applied()&&details.nations[0].government=="Council"&&
        details.nations[0].territories[0].polygon[0].x==-2&&details.nations[0].territories[0].polygon[0].y==3,
        "NationDetails history omitted normalization outside the explicit draft field scope");
    require(WorldInspectorHistory::apply(details,normalizedDetails,true).applied()&&details.nations[0].description=="Details save"&&
        details.nations[0].territories[0].polygon[0].x==0&&details.nations[0].territories[0].polygon[0].y==1,
        "NationDetails redo did not restore the actual normalized commit");
    auto text=before;text.worldPlaces[0].notes="Edited";const auto notes=patch(before,text,Scope::entity("place-a"));
    for(int mode=0;mode<4;++mode){auto rejected=text;
        if(mode==0)rejected.worldPlaces.erase(rejected.worldPlaces.begin());
        if(mode==1)rejected.worldPlaces[0].metadata.tags.push_back("archived");
        if(mode==2)rejected.worldPlaces.push_back(rejected.worldPlaces[0]);
        if(mode==3)rejected.worldPlaces[0].notes="Original notes";
        const auto count=rejected.worldPlaces.size();require(!WorldInspectorHistory::apply(rejected,notes,false).applied()&&
            rejected.worldPlaces.size()==count,"History resurrected/resolved ambiguous target or accepted already-before value as strict undo");}
    auto child=before;child.nations[0].territories[0].name="Edited";child.shops[0].stock[0].quantity=6;
    const auto children=patch(before,child);child.shops[0].stock.push_back(child.shops[0].stock[0]);
    require(!WorldInspectorHistory::apply(child,children,false).applied()&&child.nations[0].territories[0].name=="Edited",
        "Ambiguous stock identity partially undid independent territory");
    child=before;child.nations[0].territories[0].name="Edited";child.shops[0].stock[0].quantity=6;child.nations[0].territories.erase(child.nations[0].territories.begin());
    require(!WorldInspectorHistory::apply(child,children,false).applied()&&child.shops[0].stock[0].quantity==6,"Missing territory was resurrected or stock partly undone");
    auto unrelated=fixture();unrelated.worldPlaces.push_back(unrelated.worldPlaces[1]);
    unrelated.nations[0].territories.push_back(unrelated.nations[0].territories[1]);
    unrelated.shops[0].stock.push_back(unrelated.shops[0].stock[1]);auto changed=unrelated;
    changed.worldPlaces[0].notes="Unique touched place";changed.nations[0].government="Assembly";changed.shops[0].description="Shop details only";
    const auto unaffected=patch(unrelated,changed);
    require(WorldInspectorHistory::apply(changed,unaffected,false).applied()&&changed.worldPlaces.size()==3&&
        changed.nations[0].territories.size()==3&&changed.shops[0].stock.size()==3&&changed.worldPlaces[0].notes=="Original notes"&&
        changed.nations[0].government=="Council"&&changed.shops[0].description=="Original merchant prose",
        "Untouched duplicate identities blocked strict unrelated authored fields");
}
void derivedVisibility()
{
    auto before=fixture();addMaps(before);auto after=before;auto n=after.nations[0];n.visibility=WorldObjectVisibility::Public;
    auto p=after.worldPlaces[0];p.visibility=WorldObjectVisibility::Discovered;
    require(WorldInspector::updateNation(after,n.metadata.id,n).applied()&&WorldInspector::updateWorldPlace(after,p.metadata.id,p).applied(),"Cannot commit global visibility fixture");
    const auto delta=patch(before,after);require(delta.visibility.size()==4,"Global visibility positive control missed cross-map or unrelated repair flags");
    after.maps[0].generation.graph.features[0].points.push_back({.7f,.8f});after.maps[1].name="Newer map name";
    require(WorldInspectorHistory::apply(after,delta,false).applied()&&after.maps[0].generation.graph.features[0].gmOnly&&
        !after.maps[1].generation.graph.features[1].gmOnly&&after.maps[1].generation.graph.features[1].discovered&&
        after.maps[0].generation.graph.features[0].points.size()==3&&after.maps[1].name=="Newer map name",
        "Visibility undo reran global synchronization or replaced unrelated geometry/name");
    require(WorldInspectorHistory::apply(after,delta,true).applied()&&!after.maps[0].generation.graph.features[0].gmOnly&&
        after.maps[1].generation.graph.features[1].gmOnly,"Visibility redo lost exact saved flag pairs");
    const auto committed=after;
    for(int mode=0;mode<5;++mode){auto changed=committed;
        if(mode==0)changed.maps[1].generation.graph.features.erase(changed.maps[1].generation.graph.features.begin());
        if(mode==1)changed.maps.push_back(changed.maps[0]);
        if(mode==2)changed.maps[0].generation.graph.features.push_back(changed.maps[0].generation.graph.features[0]);
        if(mode==3)changed.maps[0].generation.recipe.facts[0].entityRefs[0]="different-nation";
        if(mode==4)changed.worldPlaces[1].linkedFeatureId="different-feature";
        require(!WorldInspectorHistory::apply(changed,delta,false).applied()&&changed.worldPlaces[0].visibility==WorldObjectVisibility::Discovered&&
            changed.nations[0].visibility==WorldObjectVisibility::Public,"Visibility history accepted missing/ambiguous/rebound derived target");}
}
void draftOverlap()
{
    const auto before=fixture();auto after=before;after.worldPlaces[0].notes="Committed notes";after.shops[0].stock[0].availability="Reserved";
    const auto delta=patch(before,after);WorldInspectorDrafts::Store drafts;auto p=after.worldPlaces[0];p.description="Unfinished other field";
    drafts.retain(Draft<WorldPlaceModel>{after.worldPlaces[0],p});auto s=after.shops[0];s.stock[0].quantity=19;drafts.retain(Draft<ShopModel>{after.shops[0],s});
    require(!WorldInspectorHistory::overlaps(delta,drafts),"Unrelated same-entity draft fields blocked history");
    p.notes="Unfinished overlapping note";drafts.retain(Draft<WorldPlaceModel>{after.worldPlaces[0],p});
    require(WorldInspectorHistory::overlaps(delta,drafts),"Overlapping unfinished note did not block history");
    drafts.clear();s.stock[0].availability="Unfinished availability";drafts.retain(Draft<ShopModel>{after.shops[0],s});
    require(WorldInspectorHistory::overlaps(delta,drafts),"Overlapping child field was ignored");
}
void run(){authoredFields();normalizationAndRefusal();derivedVisibility();draftOverlap();}
}
