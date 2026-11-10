#include "SpellcastingValues.hpp"
#include "RulesetResolution.hpp"
#include "DefinitionBackedRuleSet.hpp"
#include "RulesetFormulaEvaluator.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "DND/DND5e.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace SpellcastingValues
{
namespace
{
bool integer(double value)
{ return std::isfinite(value)&&std::floor(value)>=std::numeric_limits<int>::min()&&std::floor(value)<=std::numeric_limits<int>::max(); }
std::optional<int> score(const std::map<std::string,int> &attributes,const std::string &ability,
                         const RulesetDefinitionModel &definition)
{
    auto found=attributes.find(ability);
    if(found==attributes.end())for(const auto &entry:definition.abilityScores) {
        if(entry.id!=ability&&entry.name!=ability)continue;
        found=attributes.find(entry.id);
        if(found==attributes.end())found=attributes.find(entry.name);
        break;
    }
    return found==attributes.end()?std::nullopt:std::optional<int>(found->second);
}
bool components(const IRuleSet &provider,const std::map<std::string,int> &attributes,
                const CharacterSpellcastingModel &casting,int level,ResolvedValue &result)
{
    if(const auto *defined=dynamic_cast<const DefinitionBackedRuleSet *>(&provider)) {
        const auto &d=defined->definition();
        result.abilityFormula=d.abilityModifierFormula;result.proficiencyFormula=d.proficiencyFormula;
        const auto value=score(attributes,casting.ability,d);
        if(value) {
            const auto evaluated=evaluateRulesetFormula(d.abilityModifierFormula,{{"score",static_cast<double>(*value)}});
            if(!evaluated.valid||!integer(evaluated.value))return false;
        }
        const auto proficiency=evaluateRulesetFormula(d.proficiencyFormula,{{"level",static_cast<double>(std::max(1,level))}});
        if(!proficiency.valid||!integer(proficiency.value))return false;
    } // Empty formula strings mean components supplied by a compiled provider.
    result.abilityModifier=provider.getAbilityModifier(attributes,casting.ability);
    result.proficiency=provider.getProficiencyBonus(level);
    const auto sum=static_cast<long long>(*result.abilityModifier)+*result.proficiency+result.base;
    return sum>=std::numeric_limits<int>::min()&&sum<=std::numeric_limits<int>::max();
}
bool healingNeedsAbility(const Campaign &campaign,const SpellModel &spell)
{
    if((campaign.ruleSet&&!dynamic_cast<const DND5e *>(campaign.ruleSet.get()))||spell.resolution!=SpellResolutionType::Healing)return false;
    constexpr auto suffix=" + spellcasting modifier";
    const std::string tail=suffix;
    return (spell.healing.size()>tail.size()&&spell.healing.compare(spell.healing.size()-tail.size(),tail.size(),tail)==0)||
        dnd5e_detail::hasLegacyBuiltInHealingFormula(spell);
}
}
Provenance provenance(const Campaign &campaign)
{
    Provenance result;result.requested=campaign.rulesetReference;
    result.reason=campaignRulesetResolution(campaign);
    if(campaign.ruleSet) {
        result.providerId=campaign.ruleSet->getId();result.providerName=campaign.ruleSet->getName();
        result.providerVersion=campaign.ruleSet->getVersionId();
    }
    return result;
}
ResolvedValue resolve(const Campaign &campaign,const std::map<std::string,int> &attributes,
                      const CharacterSpellcastingModel &casting,int level,Field field)
{
    ResolvedValue result;
    const bool attack=field==Field::Attack;
    result.mode=attack?casting.spellAttackMode:casting.spellSaveDcMode;
    const int stored=attack?casting.spellAttackBonus:casting.spellSaveDc;
    result.base=attack?0:8;
    if(!validSpellValueMode(static_cast<int>(result.mode))) {result.reason=Reason::InvalidDefinition;return result;}
    result.legacyUsesOverride=result.mode==SpellValueMode::Legacy&&(attack?stored!=0:stored>10);
    if(result.mode==SpellValueMode::Manual||result.legacyUsesOverride)result.effective=stored;
    result.reason=campaignRulesetResolution(campaign);
    if(result.reason!=Reason::None)return result;
    if(!campaign.ruleSet) {result.reason=Reason::NoProvider;return result;}
    if(!components(*campaign.ruleSet,attributes,casting,level,result)) {result.reason=Reason::InvalidFormula;return result;}
    auto calculated=casting;
    calculated.spellAttackMode=SpellValueMode::Calculated;calculated.spellSaveDcMode=SpellValueMode::Calculated;
    result.calculated=attack?campaign.ruleSet->getSpellAttackBonus(attributes,calculated,level):
        campaign.ruleSet->getSpellSaveDc(attributes,calculated,level);
    if(!result.effective)result.effective=result.calculated;
    return result;
}
bool readyForSpell(const Campaign &campaign,const std::map<std::string,int> &attributes,
                   const CharacterSpellcastingModel &casting,int level,const SpellModel &spell,Reason *reason)
{
    const auto finish=[&](bool ready,Reason value){if(reason)*reason=value;return ready;};
    if(spell.resolution==SpellResolutionType::SpellAttack&&spell.attackBonus.empty()) {
        const auto value=resolve(campaign,attributes,casting,level,Field::Attack);
        if(!value.effective)return finish(false,value.reason);
    }
    if(spell.resolution==SpellResolutionType::SavingThrow) {
        const auto value=resolve(campaign,attributes,casting,level,Field::SaveDc);
        if(!value.effective)return finish(false,value.reason);
    }
    if(healingNeedsAbility(campaign,spell)) {
        const auto state=campaignRulesetResolution(campaign);
        if(state!=Reason::None)return finish(false,state);
    }
    return finish(true,Reason::None);
}
std::vector<MonsterActionModel> preparedActions(const Campaign &campaign,const CharacterSpellcastingModel &casting,
    const std::vector<SpellModel> &spells,const std::map<std::string,int> &attributes,int level)
{
    if(!campaign.ruleSet)return {};
    // Filter entries before invoking the original provider; preserve its spell
    // lookup, action order and exact construction for every usable entry.
    auto ready=casting;
    ready.spells.erase(std::remove_if(ready.spells.begin(),ready.spells.end(),[&](const auto &entry) {
        const auto *spell=findSpellById(spells,entry.spellId);
        if(spell&&spell->level>0&&!entry.prepared)return false;
        return spell&&!readyForSpell(campaign,attributes,casting,level,*spell);
    }),ready.spells.end());
    return campaign.ruleSet->getPreparedSpellActions(ready,spells,attributes,level);
}
}
