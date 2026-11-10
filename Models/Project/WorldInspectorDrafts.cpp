#include "Models/Project/WorldInspectorDraftFields.hpp"
#include <limits>

namespace WorldInspectorDrafts
{
std::size_t uniqueStockIndex(const ShopModel &shop,const EntityId &id)
{
    std::size_t index=MissingStock;
    for(std::size_t i=0;i<shop.stock.size();++i)if(shop.stock[i].itemId==id) {
        if(index!=MissingStock)return MissingStock; index=i;
    }
    return index;
}
WorldInspector::MutationResult applyTransfer(Campaign &c,const StockTransfer &review)
{
    const auto *shop=WorldInspector::findShop(c,review.shopId);
    const auto index=shop?uniqueStockIndex(*shop,review.itemId):MissingStock;
    const auto destination=review.purchase?review.partyBefore:review.shopBefore;
    if(!shop || index==MissingStock || review.quantity<=0 || destination<0 ||
       review.quantity>std::numeric_limits<int>::max()-destination || shop->stock[index].quantity!=review.shopBefore ||
       WorldInspector::partyQuantity(c,review.itemId)!=review.partyBefore ||
       std::find(shop->metadata.tags.begin(),shop->metadata.tags.end(),"archived")!=shop->metadata.tags.end())
        return {WorldInspector::MutationStatus::Conflict,"Stock or Party Inventory changed. Review the transaction again.",review.shopId};
    return review.purchase?WorldInspector::transferStockToParty(c,review.shopId,index,review.quantity):
        WorldInspector::transferPartyToShop(c,review.shopId,review.itemId,review.quantity);
}
WorldInspector::MutationResult removeTerritory(Campaign &c,const EntityId &nationId,const EntityId &territoryId)
{
    const auto *nation=WorldInspector::findNation(c,nationId);
    if(!nation || std::count_if(nation->territories.begin(),nation->territories.end(),[&](const auto &row){return row.metadata.id==territoryId;})!=1)
        return {WorldInspector::MutationStatus::Conflict,"Territory is missing or ambiguous. Your draft is retained.",nationId};
    return WorldInspector::deleteNationTerritory(c,nationId,territoryId);
}
namespace
{
template<class T,class Fields> void rebase(Draft<T> &d,const T &live,Fields fields)
{
    // A missing/ambiguous edited row must remain byte-for-byte recoverable.
    // Do not project it into an arbitrary current row during a display refresh.
    auto checked=live; detail::Merge guard{true};
    try{fields(d.base,d.edited,checked,guard,Scope::all());}
    catch(const std::exception &){return;}
    T base=live,edited=live; detail::Merge b{false,true},e;
    fields(d.base,d.edited,base,b,Scope::all()); fields(d.base,d.edited,edited,e,Scope::all());
    d={std::move(base),std::move(edited)};
}
template<class T,class Fields> void accept(Draft<T> &d,const Scope &s,Fields fields)
{ detail::Merge m; fields(d.base,d.edited,d.base,m,s); }
template<class T> void keep(std::map<EntityId,Draft<T>> &values,Draft<T> d)
{ const auto id=d.base.metadata.id; if(changed(d))values[id]=std::move(d); else values.erase(id); }
template<class T,class Find,class Fields,class Update>
WorldInspector::MutationResult applyRows(Campaign &c,const std::map<EntityId,Draft<T>> &rows,
                                        const Scope &s,Find find,Fields fields,Update update)
{
    using S=WorldInspector::MutationStatus;
    for(const auto &[id,draft]:rows) {
        if(!detail::selected(s,id))continue;
        auto *live=find(c,id);
        if(!live || std::find(live->metadata.tags.begin(),live->metadata.tags.end(),"archived")!=live->metadata.tags.end())
            return {S::NotFound,"The drafted World entry is missing or archived. Copy or discard its retained draft.",id};
        auto merged=*live; detail::Merge m{true};
        try { fields(draft.base,draft.edited,merged,m,s); }
        catch(const std::exception &error){return {S::Conflict,error.what(),id};}
        if(m.any) { const auto result=update(c,id,merged); if(!result.applied())return result; }
    }
    return {S::Applied,{},s.entityId};
}
}
bool changed(const WorldPlaceModel &b,const WorldPlaceModel &e)
{
    return std::tie(b.name,b.type,b.description,b.notes,b.nationId,b.visibility,b.linkedMapId,b.hasMapPosition,b.mapPosition.x,b.mapPosition.y)!=
        std::tie(e.name,e.type,e.description,e.notes,e.nationId,e.visibility,e.linkedMapId,e.hasMapPosition,e.mapPosition.x,e.mapPosition.y);
}
bool changed(const ShopModel &b,const ShopModel &e)
{
    if(std::tie(b.name,b.merchantName,b.description,b.factionReputationNotes)!=std::tie(e.name,e.merchantName,e.description,e.factionReputationNotes))return true;
    for(std::size_t i=0;i<e.stock.size();++i) {
        auto index=uniqueStockIndex(b,e.stock[i].itemId);
        if(index==MissingStock) {if(i>=b.stock.size()||b.stock[i].itemId!=e.stock[i].itemId)return true;index=i;}
        const auto &x=b.stock[index],&y=e.stock[i];
        if(std::tie(x.quantity,x.priceMultiplier,x.sellPriceMultiplier,x.restocks,x.availability,x.haggleNotes,x.factionModifier,x.reputationModifier)!=
           std::tie(y.quantity,y.priceMultiplier,y.sellPriceMultiplier,y.restocks,y.availability,y.haggleNotes,y.factionModifier,y.reputationModifier))return true;
    }
    return false;
}
bool changed(const NationModel &b,const NationModel &e)
{
    if(std::tie(b.name,b.description,b.government,b.culture,b.capitalPlaceId,b.relationships,b.visibility)!=
       std::tie(e.name,e.description,e.government,e.culture,e.capitalPlaceId,e.relationships,e.visibility))return true;
    for(std::size_t i=0;i<e.territories.size();++i) {
        const auto &row=e.territories[i];
        const auto match=[&](const auto &v){return v.metadata.id==row.metadata.id;};
        const NationTerritoryModel *base=nullptr;
        if(std::count_if(b.territories.begin(),b.territories.end(),match)==1)base=&*std::find_if(b.territories.begin(),b.territories.end(),match);
        else if(i<b.territories.size()&&b.territories[i].metadata.id==row.metadata.id)base=&b.territories[i];
        if(!base||base->name!=row.name||!detail::polygonEqual(base->polygon,row.polygon))return true;
    }
    return false;
}
bool changed(const Draft<WorldPlaceModel> &d){return changed(d.base,d.edited);}
bool changed(const Draft<NationModel> &d){return changed(d.base,d.edited);}
bool changed(const Draft<ShopModel> &d){return changed(d.base,d.edited);}
void refresh(Draft<WorldPlaceModel> &d,const WorldPlaceModel &v){rebase(d,v,detail::place);}
void refresh(Draft<NationModel> &d,const NationModel &v){rebase(d,v,detail::nation);}
void refresh(Draft<ShopModel> &d,const ShopModel &v){rebase(d,v,detail::shop);}
void acknowledge(Draft<WorldPlaceModel> &d,const Scope &s){accept(d,s,detail::place);}
void acknowledge(Draft<NationModel> &d,const Scope &s){accept(d,s,detail::nation);}
void acknowledge(Draft<ShopModel> &d,const Scope &s){accept(d,s,detail::shop);}
void Store::retain(Draft<WorldPlaceModel> d){keep(places,std::move(d));}
void Store::retain(Draft<NationModel> d){keep(nations,std::move(d));}
void Store::retain(Draft<ShopModel> d){keep(shops,std::move(d));}
WorldInspector::MutationResult Store::apply(Campaign &c,const Scope &scope)const
{
    auto result=applyRows(c,places,scope,[](Campaign &v,const EntityId &id){return WorldInspector::findWorldPlace(v,id);},detail::place,WorldInspector::updateWorldPlace);
    if(result.applied())result=applyRows(c,nations,scope,[](Campaign &v,const EntityId &id){return WorldInspector::findNation(v,id);},detail::nation,WorldInspector::updateNation);
    if(result.applied())result=applyRows(c,shops,scope,[](Campaign &v,const EntityId &id){return WorldInspector::findShop(v,id);},detail::shop,WorldInspector::updateShop);
    return result;
}
void Store::acknowledge(const Scope &scope)
{
    const auto acceptRows=[&](auto &rows){for(auto i=rows.begin();i!=rows.end();) {
        if(detail::selected(scope,i->first))WorldInspectorDrafts::acknowledge(i->second,scope);
        if(!changed(i->second))i=rows.erase(i); else ++i;
    }};
    acceptRows(places);acceptRows(nations);acceptRows(shops);
}
void Store::discard(const Scope &scope)
{
    // Removing a child discards only its edited fields; other owner edits stay.
    if(scope.part==Part::Stock || scope.part==Part::Territory){acknowledge(scope);return;}
    if(scope.part==Part::All){clear();return;}
    places.erase(scope.entityId);nations.erase(scope.entityId);shops.erase(scope.entityId);
}
}
