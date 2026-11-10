#pragma once
#include "Models/Project/WorldDraftRecoveryCodec.hpp"

namespace WorldDraftRecovery::detail
{
template<class IO,class T> void point(IO &io,T &v){io.number(v.x);io.number(v.y);}
template<class IO,class T> void territory(IO &io,T &v)
{
    io.id(v.metadata.id);io.text(v.name);
    const auto n=io.count(v.polygon.size(),io.points,MaxPoints);
    if constexpr(IO::Reading)v.polygon.resize(n);
    for(auto &p:v.polygon)point(io,p);
}
template<class IO,class T> void stock(IO &io,T &v)
{
    io.id(v.itemId);io.integer(v.quantity);io.number(v.priceMultiplier);io.number(v.sellPriceMultiplier);
    io.flag(v.restocks);io.text(v.availability);io.text(v.haggleNotes);io.number(v.factionModifier);io.number(v.reputationModifier);
}
template<class IO,class T> void place(IO &io,T &v)
{
    io.id(v.metadata.id);io.text(v.name);io.enumeration(v.type,static_cast<int>(WorldPlaceType::Site));
    io.text(v.description);io.text(v.notes);io.id(v.nationId);io.enumeration(v.visibility,static_cast<int>(WorldObjectVisibility::Public));
    io.id(v.linkedMapId);io.flag(v.hasMapPosition);point(io,v.mapPosition);
}
template<class IO,class T> void nation(IO &io,T &v)
{
    io.id(v.metadata.id);io.text(v.name);io.text(v.description);io.text(v.government);io.text(v.culture);
    io.id(v.capitalPlaceId);io.text(v.relationships);io.enumeration(v.visibility,static_cast<int>(WorldObjectVisibility::Public));
    const auto n=io.count(v.territories.size(),io.children,MaxChildren);
    if constexpr(IO::Reading)v.territories.resize(n);
    for(auto &t:v.territories)territory(io,t);
}
template<class IO,class T> void shop(IO &io,T &v)
{
    io.id(v.metadata.id);io.text(v.name);io.text(v.merchantName);io.text(v.description);io.text(v.factionReputationNotes);
    const auto n=io.count(v.stock.size(),io.children,MaxChildren);
    if constexpr(IO::Reading)v.stock.resize(n);
    for(auto &s:v.stock)stock(io,s);
}
template<class IO,class Rows,class Fields> void rows(IO &io,Rows &values,Fields fields)
{
    const auto n=io.count(values.size(),io.parents,MaxParents);
    if constexpr(IO::Reading)
    {
        for(std::size_t i=0;i<n;++i)
        {
            EntityId key;typename Rows::mapped_type draft;
            io.id(key);fields(io,draft.base);fields(io,draft.edited);
            io.parent(key,draft.base.metadata.id,draft.edited.metadata.id);
            values.emplace(std::move(key),std::move(draft));
        }
    }
    else for(const auto &[key,draft]:values)
    {
        io.parent(key,draft.base.metadata.id,draft.edited.metadata.id);
        io.id(key);fields(io,draft.base);fields(io,draft.edited);
    }
}
template<class IO,class T> void store(IO &io,T &s)
{
    rows(io,s.places,[](auto &a,auto &v){place(a,v);});
    rows(io,s.nations,[](auto &a,auto &v){nation(a,v);});
    rows(io,s.shops,[](auto &a,auto &v){shop(a,v);});
}
}
