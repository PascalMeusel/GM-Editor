// Actual controller APIs + actual SQLite. No render/update after changing pins:
// the action is intentionally the previously generated, now stale action.
struct SpellCombatFixture
{
    Campaign campaign;EntityId encounter,spell,casterId,targetId;bool npc=false;
    SpellCombatFixture(bool npcCaster,SpellResolutionType resolution,int level,bool ritual=false):npc(npcCaster)
    {
        auto &c=campaign;c.name="Spell guard "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
        SpellModel s;s.name="Generated guarded spell";s.resolution=resolution;s.level=level;s.ritual=ritual;
        s.range="60 feet";s.damage="1d4";s.savingThrowAbility="Dexterity";spell=s.metadata.id;c.customSpells={s};
        CharacterSpellcastingModel casting;casting.enabled=true;casting.spellAttackMode=SpellValueMode::Calculated;
        casting.spellSaveDcMode=SpellValueMode::Calculated;casting.slotMaximums[1]=2;
        casting.spells={{spell,true,"",""}};
        EntityId source;
        if(npc){NPCModel n;n.name="Generated NPC caster";n.attributes={{"Intelligence",16}};n.spellcasting=casting;
            source=n.metadata.id;c.npcs={n};}
        else {CharacterModel p;std::snprintf(p.nameBuffer,sizeof(p.nameBuffer),"Generated PC caster");p.level=5;
            p.maxHitPoints=p.currentHitPoints=20;p.attributes={{"Intelligence",16}};p.spellcasting=casting;
            source=p.metadata.id;c.characters.push_back(p);}
        CharacterModel target;std::snprintf(target.nameBuffer,sizeof(target.nameBuffer),"Unchanged target");
        target.maxHitPoints=target.currentHitPoints=20;target.attributes={{"Dexterity",10}};c.characters.push_back(target);
        casterId=source;targetId=target.metadata.id;
        EncounterModel e;e.name="Generated spell encounter";e.theaterOfMind=true;e.combatPhase=CombatPhase::ActiveTurn;e.round=3;
        EncounterParticipantModel a;a.kind=npc?CombatantKind::NPC:CombatantKind::Player;a.sourceEntityId=source;
        a.displayName="Caster";a.maxHitPoints=a.currentHitPoints=20;
        EncounterParticipantModel b;b.kind=CombatantKind::Player;b.sourceEntityId=target.metadata.id;
        b.displayName=target.nameBuffer;b.maxHitPoints=b.currentHitPoints=20;
        e.participants={a,b};encounter=e.metadata.id;c.encounters={e};
    }
    CharacterSpellcastingModel &casting(){return npc?campaign.npcs[0].spellcasting:campaign.characters[0].spellcasting;}
    const CharacterModel &target(const Campaign &c)const{return correctionById(c.characters,targetId);}
    void save(){require(ProjectSaveService::getInstance().saveCampaign(campaign,"spell controller fixture"),"Cannot save spell controller fixture");}
};
void spellOverrideCombatGuards()
{
    HeadlessPanel context(false); // CPU context required by real widget constructors; never render a map.
    for(bool npc:{false,true})for(int variant=0;variant<3;++variant) {
        SpellCombatFixture f(npc,variant==1?SpellResolutionType::SavingThrow:SpellResolutionType::SpellAttack,
            variant==0?0:1,variant==1);f.save();
        auto &c=f.campaign;BattleTrackerController tracker(c,nullptr);
        require(tracker.selectEncounterById(f.encounter)&&BattleTrackerTurnsProbe::selectSpell(tracker,f.spell,variant==1),
            "Resolved fixture did not generate actual prepared spell action");
        require(BattleTrackerTurnsProbe::canCast(tracker),"Resolved positive spell control cannot cast");
        const auto before=BattleTrackerTurnsProbe::draft(tracker);SessionEventCounter events(c.metadata.id);
        c.rulesetReference={"missing-combat-base","1","missing-combat-overlay","1"};
        RuleSpellResolutionSummary summary;
        require(!BattleTrackerTurnsProbe::canCast(tracker)&&!BattleTrackerTurnsProbe::summary(tracker,summary)&&
            !BattleTrackerTurnsProbe::spend(tracker),"Stale cantrip/ritual/slot action bypassed calculation guard");
        BattleTrackerTurnsProbe::cast(tracker);BattleTrackerTurnsProbe::resolveSpell(tracker);
        BattleTrackerTurnsProbe::resolveAttack(tracker);BattleTrackerTurnsProbe::damage(tracker);
        require(!BattleTrackerTurnsProbe::hit(tracker)&&!BattleTrackerTurnsProbe::feedback(tracker).empty(),"Direct stale attack was accepted without feedback");
        const auto disk=menuReload(c.metadata.id);const auto &after=BattleTrackerTurnsProbe::draft(tracker);
        require(events.saves==0&&f.casting().slotUsed[1]==0&&f.target(c).currentHitPoints==20&&
            f.target(disk).currentHitPoints==20&&correctionById(disk.encounters,f.encounter).combatLog.size()==before.combatLog.size()&&
            after.combatLog.size()==before.combatLog.size()&&after.round==before.round&&after.activeParticipantIndex==before.activeParticipantIndex&&
            after.combatPhase==before.combatPhase&&after.participants[1].currentHitPoints==20&&after.participants[1].conditions.empty(),
            "Refused spell changed resources, HP, conditions, log, turn or SQL");
        const auto &savedCasting=npc?correctionById(disk.npcs,f.casterId).spellcasting:correctionById(disk.characters,f.casterId).spellcasting;
        require(savedCasting.slotUsed[1]==0,"Refused spell spent persisted slot");
    }
    for(int mode=0;mode<3;++mode) {
        SpellCombatFixture f(false,mode==1?SpellResolutionType::SavingThrow:SpellResolutionType::SpellAttack,mode==0?1:0);
        auto &c=f.campaign;
        if(mode==0){f.casting().spellAttackMode=SpellValueMode::Manual;f.casting().spellAttackBonus=0;f.casting().slotMaximums[1]=1;}
        if(mode==1){f.casting().spellSaveDcMode=SpellValueMode::Manual;f.casting().spellSaveDc=5;}
        if(mode==2)c.customSpells[0].attackBonus="+7";
        f.save();BattleTrackerController tracker(c,nullptr);
        require(tracker.selectEncounterById(f.encounter)&&BattleTrackerTurnsProbe::selectSpell(tracker,f.spell),"Manual control has no real action");
        c.rulesetReference={"missing-combat-base","1","missing-combat-overlay","1"};
        RuleSpellResolutionSummary summary;
        require(BattleTrackerTurnsProbe::canCast(tracker)&&BattleTrackerTurnsProbe::summary(tracker,summary),"Explicit manual/spell control wrongly blocked");
        if(mode==0)require(summary.attackBonus==0&&BattleTrackerTurnsProbe::total(tracker)==15,"Manual zero became calculated +6");
        if(mode==1)require(summary.saveDc==5,"Manual low DC became calculated DC14");
        if(mode==2)require(BattleTrackerTurnsProbe::total(tracker)==22,"Per-spell +7 no longer controls actual attack");
        if(mode==0){
            BattleTrackerTurnsProbe::cast(tracker);
            require(f.casting().slotUsed[1]==1&&!BattleTrackerTurnsProbe::canCast(tracker),"Last-slot positive control did not spend exactly its slot");
            BattleTrackerTurnsProbe::resolveAttack(tracker);BattleTrackerTurnsProbe::damage(tracker);
        }else BattleTrackerTurnsProbe::resolveSpell(tracker);
        require(BattleTrackerTurnsProbe::hit(tracker)&&f.target(c).currentHitPoints==16&&
            f.target(menuReload(c.metadata.id)).currentHitPoints==16,"Actual manual spell positive control did not apply/persist damage");
    }
    SpellCombatFixture weapon(false,SpellResolutionType::SpellAttack,0);
    ItemModel club;club.name="Generated simple club";club.itemType="Weapon";club.category="Weapon";club.weaponDamage="1d4";
    weapon.campaign.customItems.push_back(club);InventoryEntryModel equipped;equipped.itemId=club.metadata.id;equipped.quantity=1;equipped.equipped=true;
    weapon.campaign.characters[0].inventory.entries.push_back(equipped);weapon.save();
    BattleTrackerController tracker(weapon.campaign,nullptr);
    require(tracker.selectEncounterById(weapon.encounter)&&BattleTrackerTurnsProbe::selectSpell(tracker,{}),"Actual non-spell weapon/unarmed action missing");
    weapon.campaign.rulesetReference={"missing-combat-base","1","missing-overlay","1"};
    BattleTrackerTurnsProbe::resolveAttack(tracker);
    require(BattleTrackerTurnsProbe::hit(tracker),"Spell calculation guard blocked ordinary weapon attack");
    BattleTrackerTurnsProbe::damage(tracker);
    require(weapon.target(menuReload(weapon.campaign.metadata.id)).currentHitPoints<20,"Ordinary weapon positive control did not persist damage");
}
void spellOverrideCharacterCastGuard()
{
    HeadlessPanel context(false);
    SpellCombatFixture f(false,SpellResolutionType::SavingThrow,1,true);f.campaign.customSpells[0].concentration=true;f.save();
    CharacterSheetController sheet(f.campaign);require(sheet.selectCharacterById(f.campaign.characters[0].metadata.id),"Cannot select direct cast actor");
    SessionEventCounter events(f.campaign.metadata.id);f.campaign.rulesetReference={"missing-sheet-base","1","missing-sheet-overlay","1"};
    SpellOverrideUiProbe::cast(sheet,0);SpellOverrideUiProbe::cast(sheet,0,true);
    require(!SpellOverrideUiProbe::error(sheet).empty()&&events.saves==0&&f.casting().slotUsed[1]==0&&
        f.campaign.characters[0].conditions.empty()&&correctionById(menuReload(f.campaign.metadata.id).characters,f.casterId).spellcasting.slotUsed[1]==0,
        "Direct character cast/ritual spent slot or concentrated on unavailable calculation");
    f.casting().spellSaveDcMode=SpellValueMode::Manual;f.casting().spellSaveDc=5;
    SpellOverrideUiProbe::cast(sheet,0);
    const auto saved=correctionById(menuReload(f.campaign.metadata.id).characters,f.casterId);
    require(saved.spellcasting.slotUsed[1]==1&&saved.spellcasting.spellSaveDcMode==SpellValueMode::Manual&&
        saved.spellcasting.spellSaveDc==5&&!saved.conditions.empty(),"Character manual low-DC cast failed or dropped mode during staged copyback");
}
