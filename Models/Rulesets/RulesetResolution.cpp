#include "RulesetResolution.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "DefinitionBackedRuleSet.hpp"
#include "RulesetDefinitionSerialization.hpp"
#include "RulesetDefinitionValidator.hpp"
#include "DND/DND5e.hpp"
#include "Clockwork/ClockworkBloodline.hpp"
#include <algorithm>

namespace
{
using Reason = SpellcastingValues::Reason;
std::string idOf(const RulesetDefinitionModel &d) { return d.rulesetId.empty()?d.metadata.id:d.rulesetId; }
bool versionMatches(const std::string &id,const std::string &version,const std::string &plain)
{ return !version.empty()&&(version==plain||version==id+"@"+plain); }
bool providerMatches(const IRuleSet &provider,const RulesetReference &pin)
{
    if(!pin.overlayId.empty()||!pin.overlayVersion.empty()||pin.rulesetId!=provider.getId())return false;
    if(const auto *defined=dynamic_cast<const DefinitionBackedRuleSet *>(&provider))
        return versionMatches(pin.rulesetId,pin.version,defined->definition().version);
    return !pin.version.empty()&&pin.version==provider.getVersionId();
}
bool equivalent(const RulesetDefinitionModel &a,const RulesetDefinitionModel &b)
{
    return a.name==b.name&&a.version==b.version&&a.baseRulesetId==b.baseRulesetId&&a.published==b.published&&
        RulesetDefinitionSerialization::serializeDefinitionContent(a)==RulesetDefinitionSerialization::serializeDefinitionContent(b);
}
const RulesetDefinitionModel *find(const std::vector<RulesetDefinitionModel> &all,
    const std::string &id,const std::string &version,bool overlay,Reason &reason)
{
    const RulesetDefinitionModel *found=nullptr;
    for(const auto &definition:all) {
        if(idOf(definition)!=id||!versionMatches(id,version,definition.version))continue;
        if(found&&!equivalent(*found,definition)) {reason=overlay?Reason::AmbiguousOverlay:Reason::AmbiguousBase;return nullptr;}
        found=&definition;
    }
    if(!found)reason=overlay?Reason::MissingOverlay:Reason::MissingBase;
    else if(!found->published) {reason=Reason::InvalidDefinition;return nullptr;}
    return found;
}
bool valid(const RulesetDefinitionModel &definition)
{
    const auto diagnostics=validateRulesetDefinition(definition);
    return std::none_of(diagnostics.begin(),diagnostics.end(),[](const auto &d){return d.severity==RulesetDiagnosticSeverity::Error;});
}
}

bool noRulesetPins(const RulesetReference &r)
{ return r.rulesetId.empty()&&r.version.empty()&&r.overlayId.empty()&&r.overlayVersion.empty(); }
bool sameRulesetPins(const RulesetReference &a,const RulesetReference &b)
{ return a.rulesetId==b.rulesetId&&a.version==b.version&&a.overlayId==b.overlayId&&a.overlayVersion==b.overlayVersion; }
const RulesetDefinitionModel *findPinnedRulesetDefinition(const std::vector<RulesetDefinitionModel> &all,
    const std::string &id,const std::string &version,bool overlay,Reason &reason)
{ reason=Reason::None;return find(all,id,version,overlay,reason); }

bool resolveCampaignRuleset(Campaign &campaign,const std::vector<RulesetDefinitionModel> &available)
{
    Reason reason=Reason::None;
    const auto &pin=campaign.rulesetReference;
    const auto finish=[&](Reason value) {
        campaign.rulesetResolution={true,pin,campaign.ruleSet,value};
        return value==Reason::None;
    };
    if(noRulesetPins(pin))return finish(campaign.ruleSet?Reason::None:Reason::NoProvider);
    if(pin.rulesetId.empty()||pin.version.empty())return finish(Reason::MissingBase);
    if(pin.overlayId.empty()!=pin.overlayVersion.empty())return finish(Reason::MissingOverlay);
    const auto *base=find(available,pin.rulesetId,pin.version,false,reason);
    if(!base) {
        if(reason!=Reason::MissingBase)return finish(reason);
        // Only the two shipped immutable providers can satisfy a missing
        // definition. A provider selected by an old display name is not proof.
        std::shared_ptr<IRuleSet> compiled;
        auto dnd=std::make_shared<DND5e>();
        auto clockwork=std::make_shared<ClockworkBloodlineRuleset>();
        auto basePin=pin;basePin.overlayId.clear();basePin.overlayVersion.clear();
        if(providerMatches(*dnd,basePin))compiled=std::move(dnd);
        else if(providerMatches(*clockwork,basePin))compiled=std::move(clockwork);
        if(!compiled)return finish(reason);
        if(!pin.overlayId.empty()) {
            // Layering needs a definition-backed base; do not silently ignore
            // a requested overlay on a compiled-only provider.
            if(!find(available,pin.overlayId,pin.overlayVersion,true,reason))return finish(reason);
            return finish(Reason::InvalidDefinition);
        }
        campaign.ruleSet=std::move(compiled);return finish(Reason::None);
    }
    if(!valid(*base))return finish(Reason::InvalidDefinition);
    if(pin.overlayId.empty()) {
        campaign.ruleSet=std::make_shared<DefinitionBackedRuleSet>(*base);
        return finish(Reason::None);
    }
    const auto *overlay=find(available,pin.overlayId,pin.overlayVersion,true,reason);
    if(!overlay)return finish(reason);
    const auto &parent=overlay->baseRulesetId;
    if(!parent.empty()&&parent!=idOf(*base)&&parent!=base->metadata.id&&parent!=idOf(*base)+"@"+base->version)
        return finish(Reason::InvalidDefinition);
    auto provider=std::make_shared<DefinitionBackedRuleSet>(*base,*overlay);
    if(!valid(provider->definition()))return finish(Reason::InvalidDefinition);
    campaign.ruleSet=std::move(provider);return finish(Reason::None);
}

SpellcastingValues::Reason campaignRulesetResolution(const Campaign &campaign)
{
    const auto &state=campaign.rulesetResolution;
    if(state.bound) {
        if(!sameRulesetPins(state.requested,campaign.rulesetReference))return Reason::PinsChanged;
        if(state.provider.lock()!=campaign.ruleSet)return Reason::ProviderChanged;
        if(!campaign.ruleSet&&state.reason==Reason::None)return Reason::NoProvider;
        return state.reason;
    }
    if(!campaign.ruleSet)return Reason::NoProvider;
    if(noRulesetPins(campaign.rulesetReference))return Reason::None;
    // Pure new/synthetic campaigns can already carry their exact provider.
    // An overlay always needs the explicit load operation's binding evidence.
    if(providerMatches(*campaign.ruleSet,campaign.rulesetReference))return Reason::None;
    return campaign.rulesetReference.overlayId.empty()?Reason::MissingBase:Reason::MissingOverlay;
}
