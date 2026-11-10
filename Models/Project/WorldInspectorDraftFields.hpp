#pragma once
#include "Models/Project/WorldInspectorDrafts.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

namespace WorldInspectorDrafts::detail
{
struct Merge
{
    bool checking = false, baseline = false, any = false;
    template<class T, class Equal = std::equal_to<T>>
    void field(const T &base, const T &edited, T &live, const char *name, Equal equal = {})
    {
        if (equal(base, edited)) return;
        any = true;
        if (checking && !equal(live, base) && !equal(live, edited))
            throw std::runtime_error(std::string("Changed elsewhere: ") + name + ". Your draft is retained.");
        live = baseline ? base : edited;
    }
};
inline bool polygonEqual(const std::vector<NormalizedWorldPointModel> &a,
                         const std::vector<NormalizedWorldPointModel> &b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const auto &x, const auto &y) {
        return x.x == y.x && x.y == y.y;
    });
}
inline bool selected(const Scope &scope, const EntityId &id)
{ return scope.part == Part::All || (scope.part != Part::None && scope.entityId == id); }
inline bool whole(const Scope &s) { return s.part == Part::All || s.part == Part::Entity; }
template<class Visitor> void visitPlace(const WorldPlaceModel &b, const WorldPlaceModel &e, WorldPlaceModel &v, Visitor &m)
{
    m.field(b.name,e.name,v.name,"Name"); m.field(b.type,e.type,v.type,"Type");
    m.field(b.description,e.description,v.description,"Description"); m.field(b.notes,e.notes,v.notes,"GM notes");
    m.field(b.nationId,e.nationId,v.nationId,"Nation"); m.field(b.visibility,e.visibility,v.visibility,"Visibility");
    const auto anchor = [](const WorldPlaceModel &p) { return std::make_tuple(p.linkedMapId,p.hasMapPosition,p.mapPosition.x,p.mapPosition.y); };
    auto value = anchor(v); m.field(anchor(b),anchor(e),value,"Map and anchor");
    std::tie(v.linkedMapId,v.hasMapPosition,v.mapPosition.x,v.mapPosition.y)=value;
}
inline void place(const WorldPlaceModel &b,const WorldPlaceModel &e,WorldPlaceModel &v,Merge &m,const Scope &)
{ visitPlace(b,e,v,m); }
template<class Visitor> void visitStock(const ShopStockEntryModel &b,const ShopStockEntryModel &e,ShopStockEntryModel &v,Visitor &m)
{
    m.field(b.quantity,e.quantity,v.quantity,"Stock quantity");
    m.field(b.priceMultiplier,e.priceMultiplier,v.priceMultiplier,"Buy multiplier");
    m.field(b.sellPriceMultiplier,e.sellPriceMultiplier,v.sellPriceMultiplier,"Sell multiplier");
    m.field(b.restocks,e.restocks,v.restocks,"Restocks"); m.field(b.availability,e.availability,v.availability,"Availability");
    m.field(b.haggleNotes,e.haggleNotes,v.haggleNotes,"Haggle notes");
    m.field(b.factionModifier,e.factionModifier,v.factionModifier,"Faction modifier");
    m.field(b.reputationModifier,e.reputationModifier,v.reputationModifier,"Reputation modifier");
}
inline void stock(const ShopStockEntryModel &b,const ShopStockEntryModel &e,ShopStockEntryModel &v,Merge &m)
{
    visitStock(b,e,v,m);
    if(m.checking && (v.quantity<0 || v.priceMultiplier<0 || v.sellPriceMultiplier<0 || v.factionModifier<0 || v.reputationModifier<0 ||
        !std::isfinite(v.priceMultiplier)||!std::isfinite(v.sellPriceMultiplier)||!std::isfinite(v.factionModifier)||!std::isfinite(v.reputationModifier)))
        throw std::runtime_error("Stock quantities and price modifiers must be finite and nonnegative. Your draft is retained.");
}
template<class Visitor> void visitShopDetails(const ShopModel &b,const ShopModel &e,ShopModel &v,Visitor &m)
{
        m.field(b.name,e.name,v.name,"Name"); m.field(b.merchantName,e.merchantName,v.merchantName,"Merchant");
        m.field(b.description,e.description,v.description,"Description");
        m.field(b.factionReputationNotes,e.factionReputationNotes,v.factionReputationNotes,"Faction and reputation notes");
}
inline void shop(const ShopModel &b,const ShopModel &e,ShopModel &v,Merge &m,const Scope &s)
{
    if (whole(s) || s.part == Part::ShopDetails) visitShopDetails(b,e,v,m);
    if (!whole(s) && s.part != Part::Stock) return;
    for (std::size_t editedIndex=0;editedIndex<e.stock.size();++editedIndex) {
        const auto &edited=e.stock[editedIndex];
        if (s.part == Part::Stock && edited.itemId != s.childId) continue;
        const auto original = uniqueStockIndex(b,edited.itemId);
        if (original == MissingStock) {
            if(editedIndex<b.stock.size() && b.stock[editedIndex].itemId==edited.itemId) {
                auto probeRow=b.stock[editedIndex]; Merge probe; stock(b.stock[editedIndex],edited,probeRow,probe);
                if(probe.any) {
                    m.any=true;
                    if(m.checking)throw std::runtime_error("Edited stock identity is ambiguous. Your draft is retained.");
                    if(editedIndex<v.stock.size() && v.stock[editedIndex].itemId==edited.itemId)
                        stock(b.stock[editedIndex],edited,v.stock[editedIndex],m);
                }
            }
            continue;
        }
        auto copy = b.stock[original]; Merge probe; stock(copy,edited,copy,probe);
        if (!probe.any) continue;
        m.any = true;
        const auto target = uniqueStockIndex(v,edited.itemId);
        if (uniqueStockIndex(e,edited.itemId) == MissingStock || target == MissingStock) {
            if (m.checking) throw std::runtime_error("Edited stock is missing or its item identity is ambiguous. Your draft is retained.");
            if (target == MissingStock && std::none_of(v.stock.begin(),v.stock.end(),[&](const auto &row){return row.itemId==edited.itemId;}))
                v.stock.push_back(m.baseline ? b.stock[original] : edited);
            continue;
        }
        stock(b.stock[original],edited,v.stock[target],m);
    }
}
template<class Visitor> void visitTerritory(const NationTerritoryModel &b,const NationTerritoryModel &e,NationTerritoryModel &v,Visitor &m)
{
    m.field(b.name,e.name,v.name,"Territory name");
    m.field(b.polygon,e.polygon,v.polygon,"Territory polygon",polygonEqual);
}
inline void territory(const NationTerritoryModel &b,const NationTerritoryModel &e,NationTerritoryModel &v,Merge &m)
{ visitTerritory(b,e,v,m); }
template<class Visitor> void visitNationDetails(const NationModel &b,const NationModel &e,NationModel &v,Visitor &m)
{
        m.field(b.name,e.name,v.name,"Name"); m.field(b.description,e.description,v.description,"Description");
        m.field(b.government,e.government,v.government,"Government"); m.field(b.culture,e.culture,v.culture,"Culture");
        m.field(b.capitalPlaceId,e.capitalPlaceId,v.capitalPlaceId,"Capital");
        m.field(b.relationships,e.relationships,v.relationships,"Relationships"); m.field(b.visibility,e.visibility,v.visibility,"Visibility");
}
inline void nation(const NationModel &b,const NationModel &e,NationModel &v,Merge &m,const Scope &s)
{
    if (whole(s) || s.part == Part::NationDetails) visitNationDetails(b,e,v,m);
    if (!whole(s) && s.part != Part::Territory) return;
    for (std::size_t editedIndex=0;editedIndex<e.territories.size();++editedIndex) {
        const auto &edited=e.territories[editedIndex];
        const auto &id = edited.metadata.id;
        if (s.part == Part::Territory && id != s.childId) continue;
        const auto count = [&](const auto &rows){return std::count_if(rows.begin(),rows.end(),[&](const auto &r){return r.metadata.id==id;});};
        if(count(b.territories)!=1) {
            if(editedIndex<b.territories.size() && b.territories[editedIndex].metadata.id==id) {
                auto copy=b.territories[editedIndex];Merge probe;territory(b.territories[editedIndex],edited,copy,probe);
                if(probe.any) {
                    m.any=true;
                    if(m.checking)throw std::runtime_error("Edited territory identity is ambiguous. Your draft is retained.");
                    if(editedIndex<v.territories.size() && v.territories[editedIndex].metadata.id==id)
                        territory(b.territories[editedIndex],edited,v.territories[editedIndex],m);
                }
            }
            continue;
        }
        const auto original = std::find_if(b.territories.begin(),b.territories.end(),[&](const auto &r){return r.metadata.id==id;});
        if (original==b.territories.end()) continue;
        auto copy=*original; Merge probe; territory(*original,edited,copy,probe); if (!probe.any) continue;
        m.any=true;
        const auto target=std::find_if(v.territories.begin(),v.territories.end(),[&](const auto &r){return r.metadata.id==id;});
        if(count(b.territories)!=1 || count(e.territories)!=1 || count(v.territories)!=1) {
            if(m.checking)throw std::runtime_error("Edited territory is missing or ambiguous. Your draft is retained.");
            if(target==v.territories.end())v.territories.push_back(m.baseline?*original:edited);
            continue;
        }
        territory(*original,edited,*target,m);
    }
}
}
