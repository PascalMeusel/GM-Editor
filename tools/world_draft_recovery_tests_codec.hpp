#pragma once
#include "world_draft_recovery_tests_fixture.hpp"

namespace world_recovery_test
{
inline std::vector<std::string> tokens(const std::string &payload)
{
    std::vector<std::string> out;std::size_t i=0;
    while(i<payload.size()){const auto end=payload.find(':',i);require(end!=std::string::npos,"Bad test token");
        const auto n=std::stoull(payload.substr(i,end-i));i=end+1;out.push_back(payload.substr(i,n));i+=n;}return out;
}
inline std::string join(const std::vector<std::string> &values)
{std::string out;for(const auto &v:values)out+=std::to_string(v.size())+":"+v;return out;}
inline void codecCases()
{
    const auto c=campaign();const auto original=authored(c);auto recovered=roundTrip(c,original);
    auto linked=original;auto &anchor=linked.places.at("place");anchor.base.linkedMapId="base-map";
    anchor.edited.linkedMapId="edited-map";anchor.edited.nationId="other-nation";anchor.edited.hasMapPosition=false;
    linked.nations.at("nation").edited.capitalPlaceId="other-capital";
    roundTrip(c,linked); // Codec preserves authored links; Save resolves them separately.
    require(recovered.nations.at("nation").base.territories[0].metadata.id=="territory-a"&&
        recovered.nations.at("nation").edited.territories[0].metadata.id=="territory-b"&&
        recovered.shops.at("shop").base.stock[0].itemId=="item-a"&&recovered.shops.at("shop").edited.stock[0].itemId=="item-b",
        "Independent child order was normalized");
    const auto payload=encoded(c,recovered);
    for(const auto &hidden:{"EXCLUDED_FEATURE","EXCLUDED_PRIVATE_METADATA","EXCLUDED_FACTION","EXCLUDED_INVENTORY"})
        require(payload.find(hidden)==std::string::npos,"Unrelated model data entered checkpoint");
    require(!decode("other-campaign",payload).success&&!decode(c.metadata.id,payload+"x").success&&
        !decode(c.metadata.id,payload.substr(0,payload.size()-1)).success,"Foreign/trailing/truncated payload accepted");
    Store one;one.places=original.places;const auto validTokens=tokens(encoded(c,one));require(validTokens.size()==28,"Unexpected test wire shape");
    for(const auto &change:std::vector<std::pair<std::size_t,std::string>>{{0,"GM_WORLD_DRAFT_99"},{2,"9999999999999999999999"},
        {3,""},{6,"99"},{12,"2"},{13,"nan"},{13,"1.0x"},{15,"different-id"}})
    {auto bad=validTokens;bad[change.first]=change.second;const auto r=decode(c.metadata.id,join(bad));require(!r.success&&r.drafts.empty(),"Invalid wire published partial drafts");}
    auto duplicate=validTokens;duplicate[2]="2";
    duplicate.insert(duplicate.end()-2,validTokens.begin()+3,validTokens.end()-2);
    require(!decode(c.metadata.id,join(duplicate)).success,"Duplicate decoded parent accepted");
    auto bad=original;bad.nations.at("nation").edited.metadata.id="wrong";require(!encode(c.metadata.id,bad).success,"Edited parent identity changed");
    bad=original;bad.nations.at("nation").base.metadata.id=bad.nations.at("nation").edited.metadata.id="place";
    auto node=bad.nations.extract("nation");node.key()="place";bad.nations.insert(std::move(node));
    require(!encode(c.metadata.id,bad).success,"Cross-kind parent collision accepted");
    // Do not prune ambiguous or unchanged retained child vectors while decoding.
    auto ambiguous=original;auto &s=ambiguous.shops.at("shop");s.base.stock.push_back(s.base.stock[0]);s.edited=s.base;s.edited.stock[0].haggleNotes="Ambiguous authored text";
    auto &n=ambiguous.nations.at("nation");n.base.territories.push_back(n.base.territories[0]);n.edited=n.base;n.edited.territories.back().name="Ambiguous territory";
    s.base.stock[1].itemId=s.edited.stock[1].itemId="";
    auto saved=roundTrip(c,ambiguous);require(saved.shops.at("shop").edited.stock.size()==3&&saved.shops.at("shop").edited.stock[1].itemId.empty(),"Ambiguous child bytes disappeared");
    auto live=c;live.shops[0]=s.base;live.nations[0]=n.base;
    require(!commit(live,saved,Scope::all()).applied()&&!saved.empty(),"Ambiguous recovered changes were applied");
    // Invalid finite authored values remain recoverable; Save still validates.
    bad=original;bad.shops.at("shop").edited.stock[1].priceMultiplier=-2;auto finite=roundTrip(c,bad);live=c;
    require(!commit(live,finite,Scope::all()).applied(),"Recovery normalized invalid authored values");
    bad=one;bad.places.at("place").edited.mapPosition.x=std::numeric_limits<double>::denorm_min();
    bad.places.at("place").edited.mapPosition.y=-0.0;auto edge=roundTrip(c,bad);
    require(std::signbit(edge.places.at("place").edited.mapPosition.y),"Signed zero lost");
    struct Comma : std::numpunct<char>{char do_decimal_point()const override{return ',';}};
    const auto previous=std::locale();std::locale::global(std::locale(previous,new Comma));
    try{require(encoded(c,one)==join(validTokens),"Locale changed checkpoint numbers");roundTrip(c,one);}
    catch(...){std::locale::global(previous);throw;}std::locale::global(previous);
}
inline void limitCases()
{
    const auto c=campaign();auto s=authored(c);s.places.at("place").edited.notes.assign(MaxTextBytes+1,'x');
    require(!encode(c.metadata.id,s).success,"Text size bound missing");
    require(!encode(std::string(MaxIdentityBytes+1,'i'),authored(c)).success,"Campaign identity size bound missing");
    s=authored(c);s.places.at("place").edited.mapPosition.x=std::numeric_limits<double>::infinity();
    require(!encode(c.metadata.id,s).success,"Nonfinite value checkpointed");
    s={};for(std::size_t i=0;i<MaxParents+1;++i){WorldPlaceModel p;p.metadata.id="p"+std::to_string(i);s.places.emplace(p.metadata.id,Draft<WorldPlaceModel>{p,p});}
    require(!encode(c.metadata.id,s).success,"Aggregate parent bound missing");
    s=authored(c);auto &n=s.nations.at("nation");n.base.territories.resize(MaxChildren/2+1);n.edited.territories=n.base.territories;
    require(!encode(c.metadata.id,s).success,"Base+edited child bound missing");
    s=authored(c);auto &t=s.nations.at("nation");t.base.territories[0].polygon.resize(MaxPoints/2+1);t.edited.territories=t.base.territories;
    require(!encode(c.metadata.id,s).success,"Base+edited point bound missing");
    s=authored(c);s.places.at("place").base.notes.assign(MaxTextBytes,'a');s.places.at("place").edited.notes.assign(MaxTextBytes,'b');
    s.nations.at("nation").base.description.assign(MaxTextBytes,'c');s.nations.at("nation").edited.description.assign(MaxTextBytes,'d');
    require(!encode(c.metadata.id,s).success&&!decode(c.metadata.id,std::string(MaxPayloadBytes+1,'x')).success,"Aggregate byte bound missing");
}
inline void mergeCases(Store restored=Store{})
{
    auto c=campaign();if(restored.empty())restored=roundTrip(c,authored(c));const auto checkpoint=encoded(c,restored);
    c.worldPlaces[0].linkedFeatureId="NEW_LIVE_FEATURE";c.worldPlaces[0].metadata.customProperties["unrelated"]="NEW_LIVE_PROPERTY";
    c.nations[0].factionIds={"NEW_LIVE_FACTION"};c.shops[0].inventoryId="NEW_LIVE_INVENTORY";
    std::swap(c.shops[0].stock[0],c.shops[0].stock[1]);std::swap(c.nations[0].territories[0],c.nations[0].territories[1]);
    c.shops[0].stock[0].quantity=123;c.nations[0].territories[0].name="Unrelated live territory";
    require(!commit(c,restored,Scope::all(),false).applied()&&encoded(c,restored)==checkpoint&&c.worldPlaces[0].name=="Original place","Failed canonical save consumed recovered draft");
    require(commit(c,restored,{Part::ShopDetails,"shop",{}}).applied()&&restored.shops.count("shop")&&c.shops[0].stock[1].quantity==5,"Scoped save consumed stock edits");
    require(commit(c,restored,Scope::all()).applied()&&restored.empty(),"Recovered fields could not commit");
    require(c.worldPlaces[0].linkedFeatureId=="NEW_LIVE_FEATURE"&&c.worldPlaces[0].metadata.customProperties.at("unrelated")=="NEW_LIVE_PROPERTY"&&
        c.nations[0].factionIds[0]=="NEW_LIVE_FACTION"&&c.shops[0].inventoryId=="NEW_LIVE_INVENTORY"&&
        c.shops[0].stock[0].quantity==123&&c.shops[0].stock[1].quantity==7&&c.nations[0].territories[0].name=="Unrelated live territory",
        "Recovery overwrote unrelated canonical fields/order");
    auto again=roundTrip(campaign(),authored(campaign()));require(commit(c,again,Scope::all()).applied(),"Already committed checkpoint did not acknowledge idempotently");
    for(int conflict=0;conflict<5;++conflict){c=campaign();auto pending=roundTrip(c,authored(c));
        if(conflict==0)c.worldPlaces[0].notes="Later authored notes";
        if(conflict==1)c.worldPlaces.clear();if(conflict==2)c.nations[0].metadata.tags.push_back("archived");
        if(conflict==3)c.shops[0].stock.erase(c.shops[0].stock.begin());if(conflict==4)c.nations[0].territories.erase(c.nations[0].territories.begin());
        require(!commit(c,pending,Scope::all()).applied()&&!pending.empty(),"Recovered stale/missing target silently saved or disappeared");}
}
}
