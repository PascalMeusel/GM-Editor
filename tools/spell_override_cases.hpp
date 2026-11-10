// Included inside spell_workflow_tests' anonymous namespace. No database or UI.
void spellOverrideValues()
{
    using namespace SpellcastingValues;
    Campaign c; c.ruleSet=std::make_shared<DND5e>();
    CharacterSpellcastingModel s; s.enabled=true;
    const std::map<std::string,int> attributes{{"Intelligence",16}};
    const auto value=[&](Field field){return resolve(c,attributes,s,5,field);};
    require(value(Field::Attack).effective==6 && value(Field::SaveDc).effective==14,
        "Legacy sentinels no longer calculate INT16/level5 as +6/DC14");
    s.spellAttackBonus=-2;s.spellSaveDc=11;
    require(value(Field::Attack).effective==-2 && value(Field::Attack).legacyUsesOverride &&
        value(Field::SaveDc).effective==11 && value(Field::SaveDc).legacyUsesOverride,"Legacy explicit values changed");
    for(int dc:{1,5,10}){s.spellSaveDc=dc;require(value(Field::SaveDc).effective==14,"Legacy low DC stopped using calculation");}
    s.spellAttackMode=SpellValueMode::Manual;s.spellSaveDcMode=SpellValueMode::Manual;
    s.spellAttackBonus=0;s.spellSaveDc=5;
    require(value(Field::Attack).effective==0 && value(Field::SaveDc).effective==5,
        "Explicit manual zero attack or low DC was treated as a sentinel");
    s.spellAttackMode=SpellValueMode::Calculated;s.spellAttackBonus=99;
    require(value(Field::Attack).effective==6 && value(Field::SaveDc).effective==5,
        "Calculated attack used stored override or changed independent manual DC");
    s.spellSaveDcMode=SpellValueMode::Calculated;s.spellSaveDc=99;
    const auto dc=value(Field::SaveDc);
    require(dc.effective==14 && dc.calculated==14 && dc.abilityModifier==3 && dc.proficiency==3 && dc.base==8,
        "Calculation breakdown differs from independent DND example");
    require(resolve(c,{{"Intelligence",8}},s,1,Field::Attack).effective==1 &&
        resolve(c,{{"Intelligence",8}},s,1,Field::SaveDc).effective==9,"Negative ability example wrong");
    SpellModel attack;attack.name="Generated attack";attack.resolution=SpellResolutionType::SpellAttack;
    SpellModel save=attack;save.metadata.id="override-save";save.resolution=SpellResolutionType::SavingThrow;
    SpellModel utility=attack;utility.metadata.id="override-utility";utility.resolution=SpellResolutionType::Utility;
    s.spells={{attack.metadata.id,true,"",""},{save.metadata.id,true,"",""},{utility.metadata.id,true,"",""}};
    c.rulesetReference={"missing-pinned-base","1","missing-overlay","1"};
    require(!readyForSpell(c,attributes,s,5,attack) && !readyForSpell(c,attributes,s,5,save) &&
        readyForSpell(c,attributes,s,5,utility),"Missing pins did not gate only the required calculation");
    s.spellAttackMode=SpellValueMode::Manual;s.spellAttackBonus=0;
    require(readyForSpell(c,attributes,s,5,attack) && !readyForSpell(c,attributes,s,5,save),
        "Independent manual attack was blocked by unresolved DC");
    s.spellSaveDcMode=SpellValueMode::Manual;s.spellSaveDc=5;
    require(readyForSpell(c,attributes,s,5,save),"Manual low DC required an unavailable calculation");
    s.spellAttackMode=SpellValueMode::Calculated;attack.attackBonus="+7";
    require(readyForSpell(c,attributes,s,5,attack),"Explicit per-spell attack override was lost");
    auto actions=preparedActions(c,s,{attack,save,utility},attributes,5);
    const auto found=std::find_if(actions.begin(),actions.end(),[&](const auto &a){return a.sourceSpellId==attack.metadata.id;});
    require(found!=actions.end() && found->attackBonus=="+7","Prepared action replaced explicit per-spell attack bonus");
    attack.attackBonus.clear();s.spellAttackMode=SpellValueMode::Legacy;s.spellAttackBonus=-2;
    require(readyForSpell(c,attributes,s,5,attack),"Legacy explicit attack wrongly required missing pins");
    s.spellAttackBonus=0;require(!readyForSpell(c,attributes,s,5,attack),"Legacy sentinel silently fell back to wrong provider");
    actions=preparedActions(c,s,{attack,save,utility},attributes,5);
    require(std::none_of(actions.begin(),actions.end(),[&](const auto &a){return a.sourceSpellId==attack.metadata.id;}),
        "Unavailable calculated attack entered prepared actions");
    SpellModel healing;healing.resolution=SpellResolutionType::Healing;healing.healing="1d4 + spellcasting modifier";
    s.spellAttackMode=SpellValueMode::Manual;s.spellSaveDcMode=SpellValueMode::Manual;
    require(!readyForSpell(c,attributes,s,5,healing)&&readyForSpell(c,attributes,s,5,utility),
        "Manual attack/DC bypassed healing's required ability calculation or blocked unrelated utility");
    healing.healing="1d4";
    require(readyForSpell(c,attributes,s,5,healing),"Authored flat healing formula unnecessarily required unresolved ability");
}

RulesetDefinitionModel spellPinnedDefinition(const char *id)
{
    RulesetDefinitionModel d;d.metadata.id=std::string(id)+"@1";d.rulesetId=id;
    d.name=id;d.version="1";d.published=true;
    d.abilityScores={{"intelligence","Intelligence","INT",""}};
    d.abilityModifierFormula="(score - 10) / 2";d.proficiencyFormula="2 + (level - 1) / 4";
    return d;
}
void spellPinnedResolution()
{
    using namespace SpellcastingValues;
    auto base=spellPinnedDefinition("generated-base");
    auto overlay=spellPinnedDefinition("generated-overlay");overlay.baseRulesetId=base.rulesetId;overlay.proficiencyFormula="4";
    Campaign c;c.ruleSet=std::make_shared<DND5e>();c.rulesetReference={base.rulesetId,"1",overlay.rulesetId,"1"};
    const auto pins=c.rulesetReference;
    CharacterSpellcastingModel s;s.enabled=true;s.spellAttackMode=SpellValueMode::Calculated;
    const auto calculated=[&]{return resolve(c,{{"Intelligence",16}},s,5,Field::Attack);};
    require(resolveCampaignRuleset(c,{base,overlay}) && calculated().effective==7,"Exact overlay formula not applied");
    const auto p=provenance(c);
    require(p.reason==Reason::None && sameRulesetPins(p.requested,pins) && !p.providerName.empty(),"Resolved provenance lost exact requested pins");
    require(resolveCampaignRuleset(c,{base,overlay,base,overlay}) && calculated().effective==7,"Identical embedded/installed definitions were falsely ambiguous");
    auto unpublished=overlay;unpublished.published=false;
    for(bool reversed:{false,true}) {
        const std::vector<RulesetDefinitionModel> candidates=reversed?std::vector<RulesetDefinitionModel>{base,unpublished,overlay}:
            std::vector<RulesetDefinitionModel>{base,overlay,unpublished};
        require(!resolveCampaignRuleset(c,candidates)&&calculated().reason==Reason::AmbiguousOverlay&&!calculated().effective,
            "Conflicting published-state duplicate depended on definition ordering");
    }
    auto contradictory=overlay;contradictory.proficiencyFormula="9";
    require(!resolveCampaignRuleset(c,{base,overlay,contradictory}) && calculated().reason==Reason::AmbiguousOverlay &&
        !calculated().effective && sameRulesetPins(c.rulesetReference,pins),"Contradictory overlay silently selected first or changed pins");
    require(!resolveCampaignRuleset(c,{base}) && calculated().reason==Reason::MissingOverlay && !calculated().effective,
        "Missing pinned overlay silently used base/old runtime provider");
    require(!resolveCampaignRuleset(c,{overlay}) && calculated().reason==Reason::MissingBase,"Missing pinned base resolved by display name");
    auto otherBase=base;otherBase.proficiencyFormula="8";
    require(!resolveCampaignRuleset(c,{base,otherBase,overlay}) && calculated().reason==Reason::AmbiguousBase,"Conflicting base accepted");
    require(resolveCampaignRuleset(c,{base,overlay}),"Cannot restore valid exact definitions");
    c.rulesetReference.overlayVersion="2";
    require(calculated().reason==Reason::PinsChanged && !calculated().effective,"Changed pins reused cached resolution");
    c.rulesetReference=pins;require(resolveCampaignRuleset(c,{base,overlay}),"Cannot rebind pins");
    c.ruleSet=std::make_shared<DND5e>();
    require(calculated().reason==Reason::ProviderChanged && !calculated().effective,"Changed runtime provider reused cached proof");
    overlay.published=false;
    require(!resolveCampaignRuleset(c,{base,overlay}) && calculated().reason==Reason::InvalidDefinition,"Unpublished pinned overlay accepted");
    overlay.published=true;overlay.rulesetId.clear();
    c.rulesetReference={base.rulesetId,base.rulesetId+"@1",overlay.metadata.id,overlay.metadata.id+"@1"};
    require(resolveCampaignRuleset(c,{base,overlay}),"Exact metadata-ID fallback/combined version refused");
    const auto priorOverlay=c.rulesetReference;auto next=base;next.version="2";next.metadata.id=base.rulesetId+"@2";
    std::vector<std::string> errors;
    require(migrateCampaignToRuleset(c,next,&overlay,errors)&&c.rulesetReference.overlayId==priorOverlay.overlayId&&
        c.rulesetReference.overlayVersion==priorOverlay.overlayVersion,"Base upgrade rewrote exact overlay pin spelling");
    c.rulesetReference={base.rulesetId,"1",{}, {}};
    base.abilityModifierFormula="1 / (score - 10)";
    require(resolveCampaignRuleset(c,{base}),"Valid formula fixture refused before runtime-only singularity");
    const auto invalid=resolve(c,{{"Intelligence",10}},s,5,Field::Attack);
    require(!invalid.effective&&invalid.reason==Reason::InvalidFormula,"Runtime formula failure silently became zero bonus");
}
