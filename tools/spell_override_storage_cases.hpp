// Bounded persistence/transfer cases; callers already isolated the repository.
void spellOverrideStorage()
{
    Campaign c;c.name="Spell modes persistence "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    CharacterModel pc;pc.spellcasting.enabled=true;pc.spellcasting.spellAttackBonus=0;pc.spellcasting.spellSaveDc=5;
    pc.spellcasting.spellAttackMode=SpellValueMode::Manual;pc.spellcasting.spellSaveDcMode=SpellValueMode::Manual;
    NPCModel npc;npc.name="Mode NPC";npc.spellcasting=pc.spellcasting;npc.spellcasting.spellAttackMode=SpellValueMode::Calculated;
    c.characters={pc};c.npcs={npc};
    require(ProjectSaveService::getInstance().saveCampaign(c,"spell modes fixture"),"Cannot persist spell modes");
    const auto assertDisk=[&](SpellValueMode attack,SpellValueMode dc) {
        const auto loaded=read(c.metadata.id);
        require(loaded.characters.size()==1 && loaded.npcs.size()==1,"Mode fixture identities changed");
        const auto &s=loaded.characters[0].spellcasting;
        require(s.spellAttackMode==attack&&s.spellSaveDcMode==dc&&s.spellAttackBonus==0&&s.spellSaveDc==5,
            "SQLite reload lost independent PC modes/manual sentinels");
        require(loaded.npcs[0].spellcasting.spellAttackMode==SpellValueMode::Calculated&&
            loaded.npcs[0].spellcasting.spellSaveDcMode==SpellValueMode::Manual&&loaded.npcs[0].spellcasting.spellSaveDc==5,
            "SQLite reload lost independent NPC modes");
    };
    assertDisk(SpellValueMode::Manual,SpellValueMode::Manual);
    c.characters[0].spellcasting.spellAttackMode=SpellValueMode::Calculated;
    c.characters[0].spellcasting.spellSaveDcMode=SpellValueMode::Legacy;
    SQLite::Database sql(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
    sql.exec("CREATE TRIGGER reject_spell_modes BEFORE INSERT ON characters BEGIN SELECT RAISE(ABORT,'mode save failure'); END;");
    require(!ProjectSaveService::getInstance().saveCampaign(c,"failed mode save"),"Injected mode save unexpectedly succeeded");
    assertDisk(SpellValueMode::Manual,SpellValueMode::Manual);
    require(c.characters[0].spellcasting.spellAttackMode==SpellValueMode::Calculated,"Failed persistence altered authored RAM mode");
    sql.exec("DROP TRIGGER reject_spell_modes;");
    require(ProjectSaveService::getInstance().saveCampaign(c,"retry mode save"),"Mode save retry failed");
    assertDisk(SpellValueMode::Calculated,SpellValueMode::Legacy);
    std::string error;CharacterModel decoded;
    const auto exported=CharacterTransfer::serializeCharacterForExport(pc);
    require(exported.rfind("TTRPG_CHARACTER_V4\n",0)==0&&CharacterTransfer::deserializeCharacterForImport(exported,decoded,error)&&
        decoded.spellcasting.spellAttackMode==SpellValueMode::Manual&&decoded.spellcasting.spellSaveDcMode==SpellValueMode::Manual&&
        decoded.spellcasting.spellAttackBonus==0&&decoded.spellcasting.spellSaveDc==5,"V4 did not preserve explicit manual sentinel values");
    require(!CharacterTransfer::deserializeCharacterForImport(exported+"unexpected",decoded,error)&&
        !CharacterTransfer::deserializeCharacterForImport(exported.substr(0,exported.size()-1),decoded,error),"Malformed V4 accepted");
    // Independent historical field layout. Never derive old payloads by editing
    // the current serializer's output: that could conceal a compatibility bug.
    for(int version:{1,2,3,4}) {
        std::vector<std::string> fields(version==1?34:(version==2?39:(version==3?40:42)));
        fields[0]="Historical caster";fields[1]="5";fields[2]="Wizard";
        for(int index:{16,17,18,21,22,23,24,26})fields[index]="0";
        fields[27]="Intelligence";fields[28]="0";fields[29]="5";
        if(version==1)fields[33]=" ";
        else {fields.back()="END_V"+std::to_string(version);if(version>=3)fields[38]="4";}
        if(version==4){fields[39]="0";fields[40]="0";}
        const auto encode=[&]{
            std::string old="TTRPG_CHARACTER_V"+std::to_string(version)+(version==1?"":"\n");
            for(const auto &field:fields)old+=version==1?std::string(1,'\x1f')+field:std::to_string(field.size())+"\n"+field;
            return old;
        };
        const auto old=encode();
        require(CharacterTransfer::deserializeCharacterForImport(old,decoded,error),"Historical V1/V2/V3 payload refused");
        require(decoded.spellcasting.spellAttackMode==SpellValueMode::Legacy&&decoded.spellcasting.spellSaveDcMode==SpellValueMode::Legacy&&
            decoded.spellcasting.spellAttackBonus==0&&decoded.spellcasting.spellSaveDc==5,"Historical export stopped preserving Legacy semantics");
        if(version==4)for(int field:{39,40})for(const char *bad:{"-1","3","2junk",""}) {
            fields[field]=bad;
            require(!CharacterTransfer::deserializeCharacterForImport(encode(),decoded,error),"V4 invalid mode spelling accepted");
            fields[field]="0";
        }
    }
}
