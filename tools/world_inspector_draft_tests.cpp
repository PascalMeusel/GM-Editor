#include "Models/Project/WorldInspectorDrafts.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
using namespace WorldInspectorDrafts;
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
Campaign fixture()
{
    Campaign c; c.name="World draft fixture";
    WorldPlaceModel place;place.name="Original place";place.description="Original description";place.linkedFeatureId="feature-original";
    c.worldPlaces.push_back(place);
    NationModel nation;nation.name="Original nation";
    NationTerritoryModel a;a.name="Territory A";a.polygon={{.1,.2},{.3,.4}};
    NationTerritoryModel b;b.name="Territory B";nation.territories={a,b};c.nations.push_back(nation);
    ItemModel item;item.name="Item A";c.customItems.push_back(item);
    ItemModel other;other.name="Item B";c.customItems.push_back(other);
    ShopModel shop;shop.name="Original shop";shop.stock={{item.metadata.id,5},{other.metadata.id,8}};c.shops.push_back(shop);
    return c;
}
WorldInspector::MutationResult commit(Campaign &c,Store &store,Scope scope,bool save=true)
{
    const auto result=WorldInspector::applyWithRollback(c,[&](Campaign &v){return store.apply(v,scope);},[&](Campaign &){return save;});
    if(result.applied())store.acknowledge(scope);
    return result;
}
void placeMerge()
{
    auto c=fixture();Store store;Draft<WorldPlaceModel> d{c.worldPlaces[0],c.worldPlaces[0]};
    d.edited.name="Authored name";d.edited.description="Authored description";d.edited.type=WorldPlaceType::Site;store.retain(d);
    c.worldPlaces[0].notes="Live private note";c.worldPlaces[0].linkedFeatureId="live-feature";
    c.worldPlaces[0].metadata.customProperties["canary"]="live property";
    require(commit(c,store,Scope::all()).applied()&&store.empty(),"Place merge did not acknowledge successful save");
    require(c.worldPlaces[0].name=="Authored name"&&c.worldPlaces[0].notes=="Live private note"&&
        c.worldPlaces[0].linkedFeatureId=="live-feature"&&c.worldPlaces[0].metadata.customProperties.at("canary")=="live property",
        "Place merge overwrote an untouched live field");
    d={c.worldPlaces[0],c.worldPlaces[0]};d.edited.notes="New private text";store.retain(d);
    c.worldPlaces[0].notes="Concurrent private text";
    require(!commit(c,store,Scope::all()).applied()&&c.worldPlaces[0].notes=="Concurrent private text"&&!store.empty(),"Stale place silently overwrote current text");
    c.worldPlaces[0].notes=d.edited.notes;
    require(commit(c,store,Scope::all()).applied(),"Already applied field did not permit idempotent acknowledgement");
    d={c.worldPlaces[0],c.worldPlaces[0]};d.edited.mapPosition.x=.8;store.retain(d);c.worldPlaces[0].linkedMapId="new-map";
    require(!commit(c,store,Scope::all()).applied(),"Anchor edit applied to a different map");
    store.clear();d={c.worldPlaces[0],c.worldPlaces[0]};d.edited.name="Must survive";store.retain(d);
    c.worldPlaces[0].metadata.tags.push_back("archived");require(!commit(c,store,Scope::all()).applied(),"Archived target accepted");
    c.worldPlaces.clear();require(!commit(c,store,Scope::all()).applied()&&c.worldPlaces.empty()&&!store.empty(),"Deleted target resurrected or draft lost");
    store.discard(Scope::entity(d.base.metadata.id));require(store.empty(),"Explicit missing draft discard failed");
}
void atomicAndScopes()
{
    auto c=fixture();Store store;
    Draft<WorldPlaceModel> p{c.worldPlaces[0],c.worldPlaces[0]};p.edited.name="Place draft";store.retain(p);
    Draft<NationModel> n{c.nations[0],c.nations[0]};n.edited.name="Nation draft";store.retain(n);
    require(!commit(c,store,Scope::all(),false).applied()&&c.worldPlaces[0].name==p.base.name&&c.nations[0].name==n.base.name&&
        store.places.size()==1&&store.nations.size()==1,"Failed persistence partially committed or acknowledged drafts");
    c.nations[0].name="Concurrent nation";
    require(!commit(c,store,Scope::all()).applied()&&c.worldPlaces[0].name==p.base.name,"Conflict after first record failed to roll back batch");
    require(commit(c,store,Scope::entity(p.base.metadata.id)).applied()&&store.places.empty()&&store.nations.size()==1,
        "Affected-record save consumed unrelated draft");
    const auto created=WorldInspector::applyWithRollback(c,[&](Campaign &v){
        const auto ready=store.apply(v,{});return ready.applied()?WorldInspector::createWorldPlace(v,WorldPlaceType::City,"New place"):ready;
    },[](Campaign &){return true;});
    require(created.applied()&&store.nations.size()==1&&c.nations[0].name=="Concurrent nation","Independent creation applied conflicting unrelated draft");
}
void territoryMerge()
{
    auto c=fixture();Store store;Draft<NationModel> n{c.nations[0],c.nations[0]};
    const auto first=n.base.territories[0].metadata.id;
    n.edited.description="Draft nation description";n.edited.territories[0].name="Authored territory";
    n.edited.territories[0].polygon[0].x=.6;store.retain(n);
    std::swap(c.nations[0].territories[0],c.nations[0].territories[1]);
    c.nations[0].territories[0].name="Live other territory";c.nations[0].factionIds.push_back("live faction");
    NationTerritoryModel fresh;fresh.name="New live territory";c.nations[0].territories.push_back(fresh);
    require(commit(c,store,{Part::Territory,n.base.metadata.id,first}).applied()&&!store.empty(),"Territory save consumed owner text");
    require(c.nations[0].territories[0].name=="Live other territory"&&c.nations[0].territories[1].name=="Authored territory"&&
        c.nations[0].territories[1].polygon[0].x==.6&&c.nations[0].territories.size()==3&&c.nations[0].factionIds[0]=="live faction",
        "Territory merge lost order/new rows/live fields");
    require(commit(c,store,Scope::all()).applied()&&c.nations[0].description=="Draft nation description","Remaining nation draft failed");
    n={c.nations[0],c.nations[0]};n.edited.territories[0].name="Missing territory draft";store.retain(n);
    c.nations[0].territories.erase(c.nations[0].territories.begin());
    require(!commit(c,store,Scope::all()).applied()&&c.nations[0].territories.size()==2,"Missing territory resurrected");
    auto retained=store.nations.begin()->second;refresh(retained,c.nations[0]);
    require(changed(retained),"Refreshing missing territory lost retained values");
}
void stockMerge()
{
    auto c=fixture();Store store;Draft<ShopModel> s{c.shops[0],c.shops[0]};
    const auto item=s.base.stock[0].itemId;s.edited.name="Authored shop";s.edited.stock[0].availability="Reserved";store.retain(s);
    std::swap(c.shops[0].stock[0],c.shops[0].stock[1]);c.shops[0].stock[1].quantity=11;c.shops[0].inventoryId="live inventory";
    require(commit(c,store,{Part::ShopDetails,s.base.metadata.id,{}}).applied()&&!store.empty(),"Details consumed stock draft");
    require(c.shops[0].stock[1].availability==s.base.stock[0].availability&&c.shops[0].stock[1].quantity==11,"Details changed stock");
    require(commit(c,store,{Part::Stock,s.base.metadata.id,item}).applied()&&store.empty()&&c.shops[0].stock[1].availability=="Reserved"&&
        c.shops[0].stock[1].quantity==11&&c.shops[0].inventoryId=="live inventory","Stock merge used stale index or quantity");
    s={c.shops[0],c.shops[0]};s.edited.stock[0].quantity=3;store.retain(s);c.shops[0].stock.erase(c.shops[0].stock.begin());
    require(!commit(c,store,Scope::all()).applied(),"Missing stock row resurrected");
    store.clear();c=fixture();c.shops[0].stock.push_back(c.shops[0].stock[0]);s={c.shops[0],c.shops[0]};s.edited.stock[0].quantity=9;store.retain(s);
    require(!store.empty()&&!commit(c,store,Scope::all()).applied(),"Ambiguous baseline stock silently discarded or applied edit");
    auto ambiguous=s;c.shops[0].stock.insert(c.shops[0].stock.begin(),ShopStockEntryModel{c.customItems[1].metadata.id,20});
    refresh(ambiguous,c.shops[0]);store.retain(ambiguous);
    require(!store.empty()&&store.shops.begin()->second.edited.stock[0].quantity==9,"Refresh/retain erased ambiguous stock edit after insertion");
    store.clear();c=fixture();s={c.shops[0],c.shops[0]};s.edited.stock[0].priceMultiplier=-1;store.retain(s);
    require(!commit(c,store,Scope::all()).applied(),"Invalid stock modifier bypassed validation");
    s.edited=s.base;store.retain(s);require(store.empty(),"Returning fields to original kept draft dirty");
}
void refreshAndTransfer()
{
    auto c=fixture();Draft<ShopModel> s{c.shops[0],c.shops[0]};s.edited.description="Retained description";
    c.shops[0].merchantName="Live merchant";c.shops[0].stock[0].quantity=7;refresh(s,c.shops[0]);
    require(s.edited.description=="Retained description"&&s.base.description.empty()&&s.edited.merchantName=="Live merchant"&&
        s.edited.stock[0].quantity==7,"Refresh failed to combine authored and current fields");
    const StockTransfer review{c.shops[0].metadata.id,c.shops[0].stock[0].itemId,2,7,0,true};
    auto bad=review;bad.shopBefore=5;require(!applyTransfer(c,bad).applied()&&c.shops[0].stock[0].quantity==7,"Stale confirmation accepted");
    std::swap(c.shops[0].stock[0],c.shops[0].stock[1]);
    require(applyTransfer(c,review).applied()&&c.shops[0].stock[1].quantity==5&&WorldInspector::partyQuantity(c,review.itemId)==2,
        "Transfer failed to resolve reviewed item after reorder");
    require(!applyTransfer(c,review).applied()&&WorldInspector::partyQuantity(c,review.itemId)==2,"Repeated confirmation transferred twice");
    auto sale=review;sale.purchase=false;sale.shopBefore=5;sale.partyBefore=2;sale.quantity=1;
    require(applyTransfer(c,sale).applied()&&c.shops[0].stock[1].quantity==6&&WorldInspector::partyQuantity(c,review.itemId)==1,"Reviewed sale failed");
    c.partyInventory.entries[0].quantity=std::numeric_limits<int>::max();
    StockTransfer overflow{c.shops[0].metadata.id,review.itemId,1,6,std::numeric_limits<int>::max(),true};
    require(!applyTransfer(c,overflow).applied()&&c.shops[0].stock[1].quantity==6&&c.partyInventory.entries[0].quantity==std::numeric_limits<int>::max(),
        "Purchase overflow changed stock or Party Inventory");
    c.partyInventory.entries[0].quantity=1;c.shops[0].stock[1].quantity=std::numeric_limits<int>::max();
    overflow={c.shops[0].metadata.id,review.itemId,1,std::numeric_limits<int>::max(),1,false};
    require(!applyTransfer(c,overflow).applied()&&c.shops[0].stock[1].quantity==std::numeric_limits<int>::max()&&c.partyInventory.entries[0].quantity==1,
        "Sale overflow changed stock or Party Inventory");
    c=fixture();c.nations[0].territories.push_back(c.nations[0].territories[0]);c.nations[0].territories.back().name="Other original duplicate";
    Draft<NationModel> clean{c.nations[0],c.nations[0]};require(!changed(clean),"Unchanged duplicate territory falsely dirty");
    clean.edited.description="Scalar edit with untouched duplicates";Store scalar;scalar.retain(clean);
    require(commit(c,scalar,Scope::all()).applied()&&c.nations[0].territories.back().name=="Other original duplicate",
        "Scalar edit treated untouched duplicate territory as changed");
    Draft<NationModel> n{c.nations[0],c.nations[0]};n.edited.territories.back().name="Ambiguous authored text";
    refresh(n,c.nations[0]);Store retained;retained.retain(n);
    require(!retained.empty()&&retained.nations.begin()->second.edited.territories.back().name=="Ambiguous authored text"&&
        !commit(c,retained,Scope::all()).applied(),"Refresh/retain erased ambiguous territory text");
    const auto duplicateId=c.nations[0].territories[0].metadata.id;
    require(!removeTerritory(c,c.nations[0].metadata.id,duplicateId).applied()&&c.nations[0].territories.size()==3,
        "Ambiguous territory removal deleted an arbitrary sibling");
    require(!removeTerritory(c,c.nations[0].metadata.id,"missing territory").applied(),"Missing territory removal acknowledged a draft");
    const auto uniqueId=c.nations[0].territories[1].metadata.id;
    require(removeTerritory(c,c.nations[0].metadata.id,uniqueId).applied()&&c.nations[0].territories.size()==2,
        "Unique territory removal failed");
}
}
int main()
{
    try{placeMerge();atomicAndScopes();territoryMerge();stockMerge();refreshAndTransfer();
        std::cout<<"World drafts: five groups covering typed merges, atomic scopes, territory/stock identities, refresh and reviewed transfers passed\n";return 0;}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
