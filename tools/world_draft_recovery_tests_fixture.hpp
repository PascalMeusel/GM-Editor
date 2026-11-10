#pragma once
#include "Models/Project/WorldDraftRecovery.hpp"
#include "Models/Project/TextDraftRecovery.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <locale>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

namespace world_recovery_test
{
namespace fs=std::filesystem;
using namespace WorldDraftRecovery;
using WorldInspectorDrafts::Draft;
using WorldInspectorDrafts::Scope;
using WorldInspectorDrafts::Part;
inline void require(bool ok,const std::string &message){if(!ok)throw std::runtime_error(message);}
inline void env(const char *key,const fs::path &path)
{
#ifdef _WIN32
    require(_putenv_s(key,path.u8string().c_str())==0,"Cannot isolate fixture environment");
#else
    require(setenv(key,path.u8string().c_str(),1)==0,"Cannot isolate fixture environment");
#endif
}
inline void isolate(const fs::path &root)
{
    fs::create_directories(root/"temp");env("TTRPG_DATA_ROOT",root/"data");env("TTRPG_DB_PATH",root/"data/fixture.db");
    env("TTRPG_SETTINGS_PATH",root/"data/settings.ini");env("TMP",root/"temp");env("TEMP",root/"temp");env("TMPDIR",root/"temp");
}
inline std::string bytes(const fs::path &path)
{std::ifstream f(path,std::ios::binary);require(bool(f),"Cannot read fixture file");return {std::istreambuf_iterator<char>(f),{}};}
inline fs::path path(const Campaign &c)
{return ProjectPackageLayout::layoutForCampaign(c).recoveryDirectory/"Drafts"/(std::string(Key)+".draft");}
inline Campaign campaign()
{
    Campaign c;c.metadata.id="world-recovery-fixture";c.name="Isolated World recovery";
    WorldPlaceModel p;p.metadata.id="place";p.name="Original place";p.type=WorldPlaceType::City;p.description="Description";
    p.notes="GM baseline";p.nationId="nation";p.linkedFeatureId="EXCLUDED_FEATURE";p.hasMapPosition=true;p.mapPosition={.25,.75};
    p.metadata.customProperties["unrelated"]="EXCLUDED_PRIVATE_METADATA";c.worldPlaces.push_back(p);
    NationModel n;n.metadata.id="nation";n.name="Original nation";n.description="History";n.government="Council";
    n.culture="Culture";n.capitalPlaceId="place";n.relationships="Neighbours";n.factionIds={"EXCLUDED_FACTION"};
    for(const auto &id:{"territory-a","territory-b"}){NationTerritoryModel t;t.metadata.id=id;t.name=id;t.polygon={{.1,.2},{.3,.4},{.5,.6}};n.territories.push_back(t);}
    c.nations.push_back(n);
    for(const auto &id:{"item-a","item-b"}){ItemModel item;item.metadata.id=id;item.name=id;c.customItems.push_back(item);}
    ShopModel s;s.metadata.id="shop";s.name="Original shop";s.merchantName="Merchant";s.description="Goods";s.inventoryId="EXCLUDED_INVENTORY";
    s.factionReputationNotes="Known locally";s.stock={{"item-a",5},{"item-b",8}};c.shops.push_back(s);return c;
}
inline Store authored(const Campaign &c)
{
    Store s;Draft<WorldPlaceModel> p{c.worldPlaces[0],c.worldPlaces[0]};
    p.edited.name="Recovered \xC3\x84 place";p.edited.type=WorldPlaceType::Landmark;p.edited.description="First line\nSecond line";
    p.edited.notes=std::string("private\0bytes",13);p.edited.visibility=WorldObjectVisibility::Discovered;p.edited.mapPosition={.125,.875};
    s.places.emplace("place",p);
    Draft<NationModel> n{c.nations[0],c.nations[0]};n.edited.name="Authored nation";n.edited.description="New history";
    n.edited.government="Republic";n.edited.culture="New culture";n.edited.relationships="Peace";n.edited.visibility=WorldObjectVisibility::Public;
    n.edited.territories[0].name="Authored territory";n.edited.territories[0].polygon={{.2,.3},{.4,.5},{.6,.7}};
    std::swap(n.edited.territories[0],n.edited.territories[1]);s.nations.emplace("nation",n);
    Draft<ShopModel> shop{c.shops[0],c.shops[0]};shop.edited.name="Authored shop";shop.edited.merchantName="New merchant";
    shop.edited.description="Rare goods";shop.edited.factionReputationNotes="Trusted";
    auto &stock=shop.edited.stock[0];stock.quantity=7;stock.priceMultiplier=1.2345678901234567;stock.sellPriceMultiplier=.75;
    stock.restocks=true;stock.availability="Reserved";stock.haggleNotes="Ask privately";stock.factionModifier=.8;stock.reputationModifier=1.2;
    std::swap(shop.edited.stock[0],shop.edited.stock[1]);s.shops.emplace("shop",shop);return s;
}
inline std::string encoded(const Campaign &c,const Store &s)
{auto r=encode(c.metadata.id,s);require(r.success,r.error);return r.payload;}
inline bool sameAuthored(const WorldPlaceModel &a,const WorldPlaceModel &b)
{return a.metadata.id==b.metadata.id&&!WorldInspectorDrafts::changed(a,b)&&!WorldInspectorDrafts::changed(b,a);}
inline bool sameAuthored(const NationModel &a,const NationModel &b)
{
    if(a.metadata.id!=b.metadata.id||a.territories.size()!=b.territories.size()||
        WorldInspectorDrafts::changed(a,b)||WorldInspectorDrafts::changed(b,a))return false;
    for(std::size_t i=0;i<a.territories.size();++i)if(a.territories[i].metadata.id!=b.territories[i].metadata.id||
        a.territories[i].name!=b.territories[i].name||a.territories[i].polygon.size()!=b.territories[i].polygon.size())return false;
    for(std::size_t i=0;i<a.territories.size();++i)for(std::size_t p=0;p<a.territories[i].polygon.size();++p)
        if(a.territories[i].polygon[p].x!=b.territories[i].polygon[p].x||a.territories[i].polygon[p].y!=b.territories[i].polygon[p].y)return false;
    return true;
}
inline bool sameAuthored(const ShopModel &a,const ShopModel &b)
{
    if(a.metadata.id!=b.metadata.id||a.stock.size()!=b.stock.size()||
        WorldInspectorDrafts::changed(a,b)||WorldInspectorDrafts::changed(b,a))return false;
    for(std::size_t i=0;i<a.stock.size();++i)if(a.stock[i].itemId!=b.stock[i].itemId)return false;
    return true;
}
inline void compareStores(const Store &a,const Store &b)
{
    const auto compare=[](const auto &x,const auto &y){require(x.size()==y.size(),"Record count changed");
        for(const auto &[id,draft]:x){const auto found=y.find(id);require(found!=y.end()&&sameAuthored(draft.base,found->second.base)&&
            sameAuthored(draft.edited,found->second.edited),"Original authored fields/IDs/order differ from decoded Store");}};
    compare(a.places,b.places);compare(a.nations,b.nations);compare(a.shops,b.shops);
}
inline Store roundTrip(const Campaign &c,const Store &s)
{auto payload=encoded(c,s);auto r=decode(c.metadata.id,payload);require(r.success,r.error);compareStores(s,r.drafts);
 require(encoded(c,r.drafts)==payload,"Nondeterministic authored-field round-trip");return std::move(r.drafts);}
inline WorldInspector::MutationResult commit(Campaign &c,Store &s,Scope scope,bool success=true)
{
    const auto result=WorldInspector::applyWithRollback(c,[&](Campaign &v){return s.apply(v,scope);},[&](Campaign &){return success;});
    if(result.applied())s.acknowledge(scope);return result;
}
inline std::string unbase64(const std::string &copy)
{
    const auto newline=copy.find('\n');require(newline!=std::string::npos,"Unlabeled recovery copy");
    const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;unsigned value=0;int bits=0;
    for(std::size_t i=newline+1;i<copy.size()&&copy[i]!='=';++i){const auto n=alphabet.find(copy[i]);require(n!=std::string::npos,"Invalid copy encoding");
        value=(value<<6)|unsigned(n);bits+=6;if(bits>=8){bits-=8;out+=char((value>>bits)&255u);}}
    return out;
}
}
