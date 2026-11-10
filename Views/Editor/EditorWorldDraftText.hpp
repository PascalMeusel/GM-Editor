#pragma once
#include "Models/Project/WorldInspectorDrafts.hpp"
#include <iomanip>
#include <sstream>

namespace WorldDraftText
{
inline std::string show(const WorldPlaceModel &v)
{
    std::ostringstream s;s<<std::setprecision(17);
    s<<"ID: "<<v.metadata.id<<"\nName: "<<v.name<<"\nType: "<<static_cast<int>(v.type)
     <<"\nDescription: "<<v.description<<"\nGM notes: "<<v.notes<<"\nNation ID: "<<v.nationId
     <<"\nMap ID: "<<v.linkedMapId<<"\nFixed map anchor: "<<v.hasMapPosition<<"\nPosition: "<<v.mapPosition.x<<", "<<v.mapPosition.y
     <<"\nVisibility: "<<static_cast<int>(v.visibility);return s.str();
}
inline std::string show(const NationModel &v)
{
    std::ostringstream s;s<<std::setprecision(17);
    s<<"ID: "<<v.metadata.id<<"\nName: "<<v.name<<"\nDescription: "<<v.description<<"\nGovernment: "<<v.government
     <<"\nCulture: "<<v.culture<<"\nRelationships: "<<v.relationships<<"\nCapital ID: "<<v.capitalPlaceId
     <<"\nVisibility: "<<static_cast<int>(v.visibility);
    for(const auto &t:v.territories){s<<"\nTerritory ID: "<<t.metadata.id<<"\nTerritory name: "<<t.name;
        for(const auto &p:t.polygon)s<<"\nPoint: "<<p.x<<", "<<p.y;}
    return s.str();
}
inline std::string show(const ShopModel &v)
{
    std::ostringstream s;s<<std::setprecision(17);
    s<<"ID: "<<v.metadata.id<<"\nName: "<<v.name<<"\nMerchant: "<<v.merchantName<<"\nDescription: "<<v.description
     <<"\nFaction and reputation notes: "<<v.factionReputationNotes;
    for(const auto &r:v.stock)s<<"\nItem ID: "<<r.itemId<<"\nQuantity: "<<r.quantity<<"\nRestocks: "<<r.restocks
        <<"\nAvailability: "<<r.availability<<"\nHaggle notes: "<<r.haggleNotes<<"\nBuy multiplier: "<<r.priceMultiplier
        <<"\nSell multiplier: "<<r.sellPriceMultiplier<<"\nFaction modifier: "<<r.factionModifier<<"\nReputation modifier: "<<r.reputationModifier;
    return s.str();
}
}
