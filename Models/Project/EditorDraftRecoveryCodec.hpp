#pragma once
#include "Models/Project/EditorDraftRecovery.hpp"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace EditorDraftRecovery::detail
{
inline void check(bool value,const char *message) { if(!value)throw std::runtime_error(message); }
struct Writer {
    static constexpr bool reading=false;
    std::string bytes;
    void text(std::string &value) {
        const auto prefix=std::to_string(value.size())+":";
        check(value.size()<=2U*1024U*1024U && prefix.size()+value.size()<=MaxPayload-bytes.size(),"Draft recovery size limit exceeded.");
        bytes+=prefix;bytes+=value;
    }
    void integer(int &value) { auto s=std::to_string(value);text(s); }
    void flag(bool &value) { std::string s=value?"1":"0";text(s); }
    void number(double &value) {
        check(std::isfinite(value),"Nonfinite draft number cannot be checkpointed.");
        std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(std::numeric_limits<double>::max_digits10)<<value;
        auto v=s.str();text(v);
    }
};
struct Reader {
    static constexpr bool reading=true;
    std::string_view bytes;
    std::size_t offset=0;
    explicit Reader(std::string_view value):bytes(value) { check(value.size()<=MaxPayload,"Draft recovery is too large."); }
    void text(std::string &value) {
        const auto end=bytes.find(':',offset);
        check(end!=std::string_view::npos && end>offset && end-offset<=8,"Invalid draft recovery length.");
        std::size_t n=0;auto p=std::from_chars(bytes.data()+offset,bytes.data()+end,n);
        check(p.ec==std::errc{} && p.ptr==bytes.data()+end,"Invalid draft recovery length.");
        offset=end+1;check(n<=2U*1024U*1024U && n<=bytes.size()-offset,"Truncated draft recovery field.");
        value.assign(bytes.data()+offset,n);offset+=n;
    }
    void integer(int &value) { std::string s;text(s);auto p=std::from_chars(s.data(),s.data()+s.size(),value);
        check(!s.empty()&&p.ec==std::errc{}&&p.ptr==s.data()+s.size(),"Invalid draft recovery number."); }
    void flag(bool &value) { std::string s;text(s);check(s=="0"||s=="1","Invalid draft recovery flag.");value=s=="1"; }
    void number(double &value) { std::string s;text(s);auto p=std::from_chars(s.data(),s.data()+s.size(),value,std::chars_format::general);
        check(!s.empty()&&p.ec==std::errc{}&&p.ptr==s.data()+s.size()&&std::isfinite(value),"Invalid draft recovery number."); }
};
struct Budget { std::size_t children=0; };
template<class A> std::size_t count(A &a,std::size_t size,Budget &budget,int limit=8192) {
    check(size<=static_cast<std::size_t>(limit),"Too many draft records.");int n=static_cast<int>(size);a.integer(n);
    check(n>=0&&n<=limit&&static_cast<std::size_t>(n)<=8192-budget.children,"Too many draft records.");
    budget.children+=static_cast<std::size_t>(n);return static_cast<std::size_t>(n);
}
template<class A> void identity(A &a,std::string &value,bool required=true) {
    a.text(value);check(value.size()<=512&&(!required||!value.empty()),"Invalid draft identity.");
}
template<class A> void strings(A &a,std::vector<std::string> &values,Budget &budget) {
    const auto n=count(a,values.size(),budget);
    if constexpr(A::reading)values.resize(n);
    for(auto &value:values)a.text(value);
}
template<class A> void metadata(A &a,EntityMetadata &v,Budget &budget) {
    identity(a,v.id);a.text(v.createdAtUtc);a.text(v.updatedAtUtc);strings(a,v.tags,budget);
    const auto n=count(a,v.customProperties.size(),budget);
    if constexpr(A::reading) {
        v.customProperties.clear();
        for(std::size_t i=0;i<n;++i) {std::string key,value;a.text(key);a.text(value);
            check(!key.empty()&&v.customProperties.emplace(key,value).second,"Duplicate draft property.");}
    } else for(auto &[key,value]:v.customProperties) {check(!key.empty(),"Empty draft property key.");auto k=key;a.text(k);a.text(value);}
}
template<class A> void power(A &a,MonsterActionModel &v) {
    a.text(v.name);a.text(v.actionKind);a.text(v.sourceLabel);identity(a,v.sourceEntityId,false);identity(a,v.linkedResourceId,false);
    a.integer(v.reachFeet);a.integer(v.normalRangeFeet);a.integer(v.rangeFeet);a.text(v.attackBonus);a.text(v.damage);a.text(v.notes);
    identity(a,v.sourceSpellId,false);a.integer(v.spellLevel);a.flag(v.consumesSpellSlot);
}
template<class A> void item(A &a,ItemModel &v,Budget &budget) {
    metadata(a,v.metadata,budget);a.text(v.name);a.text(v.description);a.text(v.rulesetKey);a.text(v.category);a.text(v.itemType);
    int rarity=static_cast<int>(v.rarity);a.integer(rarity);check(rarity>=0&&rarity<=6,"Invalid draft rarity.");v.rarity=static_cast<ItemRarity>(rarity);
    a.number(v.weight);a.integer(v.baseValue);a.flag(v.stackable);a.integer(v.armorClassBonus);a.integer(v.damageReduction);
    a.integer(v.weaponAttackBonus);a.text(v.weaponDamage);a.text(v.weaponDamageType);a.text(v.weaponAbility);
    a.integer(v.weaponReachFeet);a.integer(v.weaponRangeFeet);a.integer(v.weaponLongRangeFeet);a.text(v.weaponAttackKind);
    a.text(v.weaponMastery);a.text(v.weaponVersatileDamage);a.text(v.ammunitionType);strings(a,v.weaponProperties,budget);
    a.flag(v.container);a.number(v.containerCapacityWeight);a.integer(v.containerCapacityItems);a.flag(v.requiresAttunement);
    a.integer(v.maxCharges);a.text(v.rechargeRule);a.flag(v.cursed);a.text(v.activation);identity(a,v.linkedSpellId,false);
    a.text(v.commandWords);a.flag(v.consumable);a.flag(v.ammunition);a.integer(v.maxDurability);strings(a,v.effects,budget);
    const auto n=count(a,v.powers.size(),budget);
    if constexpr(A::reading)v.powers.resize(n);
    for(auto &entry:v.powers)power(a,entry);
}
template<class A> void notes(A &a,Notes &v) {
    identity(a,v.characterId);a.text(v.backstory);a.text(v.playerNotes);a.text(v.gmNotes);
    a.text(v.savedBackstory);a.text(v.savedPlayerNotes);a.text(v.savedGmNotes);
}
template<class A> void draft(A &a,ItemDraft &v,Budget &budget) {
    item(a,v.item,budget);bool base=v.base.has_value();a.flag(base);
    if constexpr(A::reading) {if(base)v.base.emplace();else v.base.reset();}
    if(base) {item(a,*v.base,budget);check(v.base->metadata.id==v.item.metadata.id,"Item baseline identity differs.");}
    power(a,v.power);a.flag(v.powerDirty);
}
template<class A> void store(A &a,Store &v) {
    Budget budget;const auto n=count(a,v.items.size(),budget,256);
    if constexpr(A::reading) {
        for(std::size_t i=0;i<n;++i) {ItemDraft d;draft(a,d,budget);
            const auto id=d.item.metadata.id;check(v.items.emplace(id,std::move(d)).second,"Duplicate item draft.");}
    } else for(auto &[id,d]:v.items) {check(id==d.item.metadata.id,"Item draft key differs.");draft(a,d,budget);}
    bool has=v.notes.has_value();a.flag(has);
    if constexpr(A::reading) {if(has)v.notes.emplace();else v.notes.reset();}
    if(has)notes(a,*v.notes);
}
}
