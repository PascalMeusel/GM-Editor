void spellOverrideMagicUi(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Actual Magic value sources 27px ")+(de?"DE":"EN"));
    Campaign c;c.name="Spell mode UI "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    CharacterModel pc;std::snprintf(pc.nameBuffer,sizeof(pc.nameBuffer),"Generated spell caster");
    pc.level=5;pc.selectedClass="Wizard";pc.attributes={{"Intelligence",16}};pc.spellcasting.enabled=true;
    pc.spellcasting.spellAttackBonus=0;pc.spellcasting.spellSaveDc=10;c.characters={pc};
    require(ProjectSaveService::getInstance().saveCampaign(c,"spell mode UI fixture"),"Cannot save Magic fixture");
    CharacterSheetController controller(c);require(controller.selectCharacterById(pc.metadata.id),"Cannot select Magic actor");
    ui.draw=[&]{SpellOverrideUiProbe::draw(controller);};ui.frame(3);
    ui.compactClick(de?"Magie":"Magic");
    const char *attack=de?"Zauberangriff überschreiben":"Spell attack override";
    const char *dc=de?"Zauberrettungs-SG überschreiben":"Spell save DC override";
    const char *source=de?"Wertquelle":"Value source";
    const char *calculateAttack=de?"Berechneten Angriff verwenden":"Use calculated attack";
    const char *calculateDc=de?"Berechneten SG verwenden":"Use calculated DC";
    const auto pane=ui.compactItem(attack).window;
    const auto reach=[&](const char *label){guidanceReachInObservedPane(ui,label,pane);};
    const auto choose=[&](const char *numeric,const char *choice) {
        reach(numeric);const auto number=ui.compactItem(numeric);
        auto candidates=ui.activeControls(source);
        candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](const auto &item){
            const float x=(number.minimum.x+number.maximum.x)*.5f;
            return item.window!=number.window||item.minimum.y>=number.minimum.y||x<item.minimum.x||x>item.maximum.x;}),candidates.end());
        require(!candidates.empty(),"No observed source combo before numeric field");
        const auto combo=*std::max_element(candidates.begin(),candidates.end(),[](const auto &a,const auto &b){return a.minimum.y<b.minimum.y;});
        ui.compactClick(source,combo.id);ui.compactClick(choice,0,true);
    };
    choose(attack,de?"Manuell":"Manual");
    require(c.characters[0].spellcasting.spellAttackMode==SpellValueMode::Manual&&c.characters[0].spellcasting.spellAttackBonus==0&&
        menuReload(c.metadata.id).characters[0].spellcasting.spellAttackMode==SpellValueMode::Manual,
        "Actual Manual choice did not save zero attack without numeric change");
    choose(dc,de?"Manuell":"Manual");reach(dc);ui.compactType(dc,"5");ui.reviewKey(ImGuiKey_Tab);
    require(c.characters[0].spellcasting.spellSaveDc==5&&c.characters[0].spellcasting.spellSaveDcMode==SpellValueMode::Manual,
        "Actual numeric input did not retain manual low DC");
    reach(calculateAttack);ui.reviewTabTo(calculateAttack,true);ui.reviewKey(ImGuiKey_Space);
    require(c.characters[0].spellcasting.spellAttackMode==SpellValueMode::Calculated&&c.characters[0].spellcasting.spellAttackBonus==0&&
        c.characters[0].spellcasting.spellSaveDcMode==SpellValueMode::Manual,"Calculated button changed stored manual number or other field");
    const char *details=de?"Zauberberechnung und Quelle":"Spell calculation and source";
    reach(details);ui.compactClick(details);
    require(contains(ui.rendered,"Intelligence (+3)")&&contains(ui.rendered,"= 6")&&contains(ui.rendered,"= 14"),
        "Actual source details did not show independently expected +6/DC14 breakdown");
    c.rulesetReference={"missing-ui-base","9","missing-ui-overlay","7"};ui.frame(3);
    require(contains(ui.rendered,"missing-ui-base")&&contains(ui.rendered,"missing-ui-overlay")&&
        contains(ui.rendered,de?"nicht verfügbar":"unavailable"),"Requested missing pins/status not visible in actual Magic source panel");
    reach(calculateDc);ui.reviewState(calculateDc,true);
    choose(attack,de?"Manuell":"Manual");
    const auto saved=menuReload(c.metadata.id);
    require(saved.characters[0].spellcasting.spellAttackMode==SpellValueMode::Manual&&saved.characters[0].spellcasting.spellAttackBonus==0&&
        saved.characters[0].spellcasting.spellSaveDcMode==SpellValueMode::Manual&&saved.characters[0].spellcasting.spellSaveDc==5&&
        saved.rulesetReference.rulesetId=="missing-ui-base"&&saved.rulesetReference.overlayId=="missing-ui-overlay",
        "Manual UI fallback failed to persist values or silently rewrote missing pins");
}
