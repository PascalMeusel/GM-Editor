#pragma once
#include "WorldInspectorHistory.hpp"
#include "WorldInspectorDraftFields.hpp"
#include <set>
#include <stdexcept>
#include <type_traits>

namespace WorldInspectorHistory::detail
{
inline bool archived(const EntityMetadata &m)
{ return std::find(m.tags.begin(),m.tags.end(),"archived") != m.tags.end(); }
template<class Rows,class Id> auto &unique(Rows &rows,const EntityId &id,Id identity)
{
    if(id.empty() || std::count_if(rows.begin(),rows.end(),[&](const auto &r){return identity(r)==id;})!=1)
        throw std::runtime_error("A recorded World identity is missing or ambiguous.");
    return *std::find_if(rows.begin(),rows.end(),[&](const auto &r){return identity(r)==id;});
}
struct MetadataId { template<class T> const EntityId &operator()(const T &v) const {return v.metadata.id;} };
struct StockId { const EntityId &operator()(const ShopStockEntryModel &v) const {return v.itemId;} };
struct FeatureId { const EntityId &operator()(const MapGeneratedFeature &v) const {return v.id;} };
template<class T> void available(const T &v)
{ if(archived(v.metadata))throw std::runtime_error("A recorded World entry is archived."); }
template<class Rows,class Id> void sameChildren(const Rows &a,const Rows &b,Id id)
{
    if(a.size()!=b.size())throw std::runtime_error("World history cannot create or remove child records.");
    (void)id;
}
struct Strict
{
    bool any=false;
    template<class T,class Equal=std::equal_to<T>>
    void field(const T &expected,const T &desired,T &value,const char *name,Equal equal={})
    {
        if(equal(expected,desired))return;
        any=true;
        if(!equal(value,expected))throw std::runtime_error(std::string("A recorded World field changed: ")+name+".");
        value=desired;
    }
};
struct Fields
{
    std::set<std::string> keys;
    std::string prefix;
    template<class T,class Equal=std::equal_to<T>>
    void field(const T &a,const T &b,T &,const char *name,Equal equal={})
    { if(!equal(a,b))keys.insert(prefix+name); }
};
inline std::string childPrefix(const char *kind,const EntityId &id)
{ return std::string(kind)+std::to_string(id.size())+":"+id+"/"; }
inline bool sameChild(const ShopStockEntryModel &a,const ShopStockEntryModel &b)
{ auto v=a;Fields f;WorldInspectorDrafts::detail::visitStock(a,b,v,f);return f.keys.empty(); }
inline bool sameChild(const NationTerritoryModel &a,const NationTerritoryModel &b)
{ auto v=a;Fields f;WorldInspectorDrafts::detail::visitTerritory(a,b,v,f);return f.keys.empty(); }
template<class Rows,class Id> const typename Rows::value_type &peer(const Rows &a,const Rows &b,std::size_t index,Id id)
{
    const auto &old=a[index];const auto key=id(old);
    const auto count=[&](const auto &rows){return std::count_if(rows.begin(),rows.end(),[&](const auto &v){return id(v)==key;});};
    if(!key.empty()&&count(a)==1&&count(b)==1)return unique(b,key,id);
    if(index<b.size()&&id(b[index])==key&&sameChild(old,b[index]))return b[index];
    throw std::runtime_error("An edited World child identity is missing or ambiguous.");
}
template<class V> void visit(const WorldPlaceModel &a,const WorldPlaceModel &b,WorldPlaceModel &v,V &visitor)
{ WorldInspectorDrafts::detail::visitPlace(a,b,v,visitor); }
template<class V> void visit(const ShopModel &a,const ShopModel &b,ShopModel &v,V &visitor)
{
    WorldInspectorDrafts::detail::visitShopDetails(a,b,v,visitor);
    sameChildren(a.stock,b.stock,StockId{});
    for(std::size_t i=0;i<a.stock.size();++i) {
        const auto &old=a.stock[i],&next=peer(a.stock,b.stock,i,StockId{});
        auto probe=old; Fields changed;WorldInspectorDrafts::detail::visitStock(old,next,probe,changed);
        if(changed.keys.empty())continue;
        auto &target=unique(v.stock,old.itemId,StockId{});
        WorldInspectorDrafts::detail::visitStock(old,next,target,visitor);
    }
}
template<class V> void visit(const NationModel &a,const NationModel &b,NationModel &v,V &visitor)
{
    WorldInspectorDrafts::detail::visitNationDetails(a,b,v,visitor);
    sameChildren(a.territories,b.territories,MetadataId{});
    for(std::size_t i=0;i<a.territories.size();++i) {
        const auto &old=a.territories[i],&next=peer(a.territories,b.territories,i,MetadataId{});
        auto probe=old; Fields changed;WorldInspectorDrafts::detail::visitTerritory(old,next,probe,changed);
        if(changed.keys.empty())continue;
        available(old);available(next);
        auto &target=unique(v.territories,old.metadata.id,MetadataId{});available(target);
        WorldInspectorDrafts::detail::visitTerritory(old,next,target,visitor);
    }
}
// Field keys carry child identities: edits in separate stock rows/territories
// do not overlap simply because their displayed field labels are identical.
inline Fields fields(const WorldPlaceModel &a,const WorldPlaceModel &b)
{ auto v=a;Fields f;WorldInspectorDrafts::detail::visitPlace(a,b,v,f);return f; }
inline Fields fields(const ShopModel &a,const ShopModel &b)
{
    auto v=a;Fields f;WorldInspectorDrafts::detail::visitShopDetails(a,b,v,f);
    sameChildren(a.stock,b.stock,StockId{});
    for(std::size_t i=0;i<a.stock.size();++i) {
        const auto &row=a.stock[i];
        f.prefix=childPrefix("stock/",row.itemId);auto copy=row;
        WorldInspectorDrafts::detail::visitStock(row,peer(a.stock,b.stock,i,StockId{}),copy,f);
    }
    return f;
}
inline Fields fields(const NationModel &a,const NationModel &b)
{
    auto v=a;Fields f;WorldInspectorDrafts::detail::visitNationDetails(a,b,v,f);
    sameChildren(a.territories,b.territories,MetadataId{});
    for(std::size_t i=0;i<a.territories.size();++i) {
        const auto &row=a.territories[i];
        f.prefix=childPrefix("territory/",row.metadata.id);auto copy=row;
        WorldInspectorDrafts::detail::visitTerritory(row,peer(a.territories,b.territories,i,MetadataId{}),copy,f);
    }
    return f;
}
inline WorldPlaceModel scopedEdited(const WorldInspectorDrafts::Draft<WorldPlaceModel> &d,const WorldInspectorDrafts::Scope &s)
{ auto v=d.base;WorldInspectorDrafts::detail::Merge m;WorldInspectorDrafts::detail::place(d.base,d.edited,v,m,s);return v; }
inline ShopModel scopedEdited(const WorldInspectorDrafts::Draft<ShopModel> &d,const WorldInspectorDrafts::Scope &s)
{ auto v=d.base;WorldInspectorDrafts::detail::Merge m;WorldInspectorDrafts::detail::shop(d.base,d.edited,v,m,s);return v; }
inline NationModel scopedEdited(const WorldInspectorDrafts::Draft<NationModel> &d,const WorldInspectorDrafts::Scope &s)
{ auto v=d.base;WorldInspectorDrafts::detail::Merge m;WorldInspectorDrafts::detail::nation(d.base,d.edited,v,m,s);return v; }
template<class T> void acknowledge(WorldInspectorDrafts::Draft<T> &current,const WorldInspectorDrafts::Draft<T> &submitted,
                                   const WorldInspectorDrafts::Scope &scope)
{
    const auto selected=scopedEdited(submitted,scope);
    auto edited=current.edited;Strict unchanged;
    visit(selected,submitted.base,edited,unchanged); // Validate submitted fields only.
    Strict accept;visit(submitted.base,selected,current.base,accept);
}
void captureVisibility(const Campaign &,const Campaign &,Patch &);
void validateVisibility(const Campaign &,const Patch &,bool forward,bool desired);
void applyVisibility(Campaign &,const Patch &,bool forward);
}
