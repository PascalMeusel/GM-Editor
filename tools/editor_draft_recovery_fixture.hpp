#pragma once
#include "Models/Project/EditorDraftRecovery.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/TextDraftRecovery.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <tuple>

namespace editor_recovery_test {
namespace fs=std::filesystem;
using namespace EditorDraftRecovery;
inline void require(bool ok,const std::string &message){if(!ok)throw std::runtime_error(message);}
inline void env(const char *name,const fs::path &value){
#ifdef _WIN32
    require(_putenv_s(name,value.u8string().c_str())==0,"Cannot isolate environment");
#else
    require(setenv(name,value.u8string().c_str(),1)==0,"Cannot isolate environment");
#endif
}
inline void isolate(const fs::path &root){
    fs::create_directories(root/"data");fs::create_directories(root/"temp");
    env("TTRPG_DATA_ROOT",root/"data");env("TTRPG_DB_PATH",root/"data/test.db");
    env("TTRPG_SETTINGS_PATH",root/"data/settings.ini");env("TEMP",root/"temp");env("TMP",root/"temp");env("TMPDIR",root/"temp");
}
inline std::string bytes(const fs::path &p){std::ifstream f(p,std::ios::binary);require(bool(f),"Cannot read fixture bytes");return {std::istreambuf_iterator<char>(f),{}};}
inline fs::path path(const Campaign &c){return ProjectPackageLayout::layoutForCampaign(c).recoveryDirectory/"Drafts"/(std::string(Key)+".draft");}
inline MonsterActionModel action(const std::string &suffix){
    MonsterActionModel a;a.name="Power "+suffix;a.actionKind="Bonus";a.sourceLabel="Source "+suffix;a.sourceEntityId="source-"+suffix;
    a.linkedResourceId="resource-"+suffix;a.reachFeet=-7;a.normalRangeFeet=33;a.rangeFeet=77;
    a.attackBonus="+5";a.damage="2d8 cold";a.notes="PRIVATE_POWER\n"+suffix;a.sourceSpellId="spell-"+suffix;
    a.spellLevel=4;a.consumesSpellSlot=true;return a;
}
inline ItemModel item(const std::string &id){
    ItemModel i;i.metadata.id=id;i.metadata.createdAtUtc="created";i.metadata.updatedAtUtc="updated";
    i.metadata.tags={"z","a","z"};i.metadata.customProperties={{"private","VALUE"},{"item.image","Assets/Images/x.png"},{"gm.context.place","old-place"}};
    i.name="Unfinished "+id;i.description=std::string("PRIVATE_ITEM\n\0bytes",19);i.rulesetKey="rule";i.category="Weapon";i.itemType="Sword";
    i.rarity=static_cast<ItemRarity>(4);i.weight=-1.75;i.baseValue=-123;i.stackable=false;i.armorClassBonus=3;i.damageReduction=4;
    i.weaponAttackBonus=5;i.weaponDamage="1d8";i.weaponDamageType="cold";i.weaponAbility="Dexterity";i.weaponReachFeet=8;
    i.weaponRangeFeet=20;i.weaponLongRangeFeet=60;i.weaponAttackKind="Ranged";i.weaponMastery="Cleave";i.weaponVersatileDamage="1d10";
    i.ammunitionType="shards";i.weaponProperties={"z","a","z"};i.container=true;i.containerCapacityWeight=-4.5;i.containerCapacityItems=-2;
    i.requiresAttunement=true;i.maxCharges=-5;i.rechargeRule="dawn";i.cursed=true;i.activation="reaction";i.linkedSpellId="spell";
    i.commandWords="open\nclose";i.consumable=true;i.ammunition=true;i.maxDurability=-8;i.effects={"z","a","z"};
    i.powers={action("two"),action("one")};return i;
}
inline auto actionFields(const MonsterActionModel &a){return std::tie(a.name,a.actionKind,a.sourceLabel,a.sourceEntityId,a.linkedResourceId,
    a.reachFeet,a.normalRangeFeet,a.rangeFeet,a.attackBonus,a.damage,a.notes,a.sourceSpellId,a.spellLevel,a.consumesSpellSlot);}
inline auto itemFields(const ItemModel &i){return std::tie(i.metadata.id,i.metadata.createdAtUtc,i.metadata.updatedAtUtc,i.metadata.tags,
    i.metadata.customProperties,i.name,i.description,i.rulesetKey,i.category,i.itemType,i.rarity,i.weight,i.baseValue,i.stackable,
    i.armorClassBonus,i.damageReduction,i.weaponAttackBonus,i.weaponDamage,i.weaponDamageType,i.weaponAbility,i.weaponReachFeet,
    i.weaponRangeFeet,i.weaponLongRangeFeet,i.weaponAttackKind,i.weaponMastery,i.weaponVersatileDamage,i.ammunitionType,i.weaponProperties,
    i.container,i.containerCapacityWeight,i.containerCapacityItems,i.requiresAttunement,i.maxCharges,i.rechargeRule,i.cursed,i.activation,
    i.linkedSpellId,i.commandWords,i.consumable,i.ammunition,i.maxDurability,i.effects);}
inline void compareItem(const ItemModel &a,const ItemModel &b){
    require(itemFields(a)==itemFields(b)&&a.powers.size()==b.powers.size(),"Item authored field or metadata differs");
    for(std::size_t n=0;n<a.powers.size();++n)require(actionFields(a.powers[n])==actionFields(b.powers[n]),"Power fields/order differ");
}
inline Store fixture(){
    Store s;ItemDraft d;d.base=item("existing");d.item=*d.base;d.item.description+="edited";d.item.metadata.tags.push_back("new");
    d.power=action("pending");d.power.name.clear();d.powerDirty=true;s.items[d.item.metadata.id]=d;
    d.base.reset();d.item=item("new-stable-id");d.powerDirty=false;s.items[d.item.metadata.id]=d;
    s.notes=Notes{"character","BACKSTORY","PLAYER_NOTES","GM_NOTES","old backstory","old player","old gm"};return s;
}
inline void compare(const Store &a,const Store &b){
    require(a.items.size()==b.items.size()&&bool(a.notes)==bool(b.notes),"Draft selection differs");
    for(const auto &[id,d]:a.items){const auto &e=b.items.at(id);compareItem(d.item,e.item);require(bool(d.base)==bool(e.base),"Baseline presence differs");
        if(d.base)compareItem(*d.base,*e.base);require(d.powerDirty==e.powerDirty&&actionFields(d.power)==actionFields(e.power),"Pending power differs");}
    if(a.notes){const auto &x=*a.notes,&y=*b.notes;require(std::tie(x.characterId,x.backstory,x.playerNotes,x.gmNotes,x.savedBackstory,x.savedPlayerNotes,x.savedGmNotes)==
        std::tie(y.characterId,y.backstory,y.playerNotes,y.gmNotes,y.savedBackstory,y.savedPlayerNotes,y.savedGmNotes),"Note fields/baselines differ");}
}
inline Campaign campaign(){Campaign c;c.metadata.id="editor-recovery-fixture";c.name="Isolated draft recovery";return c;}
}
