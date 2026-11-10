// Focused installation/controller regression. Broad projection coverage lives
// in player_safety_tests; only text is rendered here, never the compiled map.
void playerPublicationCanonicalCopy(bool de)
{
    HeadlessPanel ui(de);ui.context(de?"Canonical player-only installation DE":"Canonical player-only installation EN");
    auto fixture=playerReviewMemoFixture();
    MapFeatureModel secret;secret.name="PRIVATE_GM_PUBLICATION_CANARY";secret.gmOnly=true;secret.x=secret.y=1;
    fixture.maps[0].features.push_back(secret);
    auto &character=fixture.characters[0];std::snprintf(character.nameBuffer,sizeof(character.nameBuffer),"Public Iris");
    character.selectedClass="Cleric";character.level=3;character.maxHitPoints=30;character.currentHitPoints=24;
    character.temporaryHitPoints=3;character.speedFeet=30;character.conditions={"Shielded"};
    fixture.localPlayerDisplay.showParty=true;fixture.localPlayerDisplay.visibleCharacterIds={character.metadata.id};
    fixture.localPlayerDisplay.showInitiative=true;
    EncounterModel encounter;encounter.name="Public gatehouse";encounter.round=1;encounter.combatPhase=CombatPhase::ActiveTurn;
    for(int i=0;i<2;++i){EncounterParticipantModel p;p.displayName=i?"Public guard":"Public Iris";p.initiative=18-i*6;
        encounter.participants.push_back(p);}
    fixture.encounters.push_back(encounter);fixture.localPlayerDisplay.publicEncounterId=encounter.metadata.id;
    EditorWindowController editor;require(editor.setCurrentCampaign(fixture),"Cannot install canonical-copy fixture");
    auto &c=PlayerReviewUiProbe::campaign(editor);auto &owner=PlayerReviewUiProbe::owner(editor);
    auto &display=PlayerReviewUiProbe::display(editor);const auto locale=PlayerDisplayReview::currentLocaleCode();
    require(owner.prepare(c,locale)&&editor.armLocalPlayerDisplayFromReview(owner.review()),"Cannot arm canonical-copy fixture");
    owner.clear();require(owner.prepare(c,locale),"Cannot prepare canonical-copy publication");
    const auto reviewed=owner.review();
    require(std::any_of(reviewed.gmMapPlan.primitives.begin(),reviewed.gmMapPlan.primitives.end(),
        [&](const auto &p){return p.sourceId==secret.metadata.id;}),"GM comparison positive geometry canary absent");
    require(render_plan_test::snapshot(reviewed.playerMapPlan).find(secret.name)==std::string::npos,
        "Private canary entered reviewed player plan");
    require(editor.publishLocalPlayerDisplay(owner.review()),"Actual Editor publication rejected canonical-copy fixture");
    const auto canonical=PlayerDisplayReview::build(c,locale,PlayerDisplayReview::BuildPurpose::PlayerOnly);
    const auto assertRetained=[&](const PlayerDisplayReviewSnapshot &expected){
        require(PlayerReviewUiProbe::publicationProof(display),"Canonical publication proof/cache is not valid");
        for(const auto *actual:{&PlayerReviewUiProbe::approved(display),&PlayerReviewUiProbe::cached(display)}){
            require(actual->eligible&&actual->locale==expected.locale&&actual->hasMapPreview==expected.hasMapPreview&&
                actual->contentFingerprint==expected.contentFingerprint&&actual->campaignName==expected.campaignName&&
                render_plan_test::snapshot(actual->gmMapPlan)==render_plan_test::snapshot(MapRenderPlan{})&&
                render_plan_test::snapshot(actual->playerMapPlan)==render_plan_test::snapshot(expected.playerMapPlan),
                "Retained publication copied private Comparison geometry or changed canonical player plan");
            require(actual->playerHandouts.size()==1&&expected.playerHandouts.size()==1,"Canonical handout count changed");
            const auto &a=actual->playerHandouts[0],&b=expected.playerHandouts[0];
            require(a.assetId==b.assetId&&a.title==b.title&&a.body==b.body&&a.projectRelativePath==b.projectRelativePath&&a.contentHash==b.contentHash,
                "Canonical publication changed a reviewed handout field");
            require(actual->playerParty.size()==1&&actual->playerInitiative.size()==2,"Canonical party/initiative count changed");
            const auto &party=actual->playerParty[0],&want=expected.playerParty[0];
            require(party.entityId==want.entityId&&party.name==want.name&&party.characterClass==want.characterClass&&party.level==want.level&&
                party.currentHitPoints==want.currentHitPoints&&party.maxHitPoints==want.maxHitPoints&&party.temporaryHitPoints==want.temporaryHitPoints&&
                party.speedFeet==want.speedFeet&&party.conditions==want.conditions&&party.spellSlots.empty(),"Retained public party fields changed");
            require(actual->playerEncounterName==expected.playerEncounterName&&actual->playerEncounterRound==expected.playerEncounterRound,
                "Retained public encounter fields changed");
            for(std::size_t i=0;i<2;++i){const auto &a=actual->playerInitiative[i],&b=expected.playerInitiative[i];
                require(a.displayName==b.displayName&&a.initiative==b.initiative&&a.active==b.active,"Retained initiative field changed");}
        }
    };
    assertRetained(canonical);owner.clear();
    ui.draw=[&]{display.update(nullptr);};ui.frame(3);assertRetained(canonical);
    require(contains(ui.rendered,"FROZEN_PUBLIC_TEXT")&&!contains(ui.rendered,secret.name.c_str()),
        "Owner clear lost independent player text or rendered private comparison content");
    // Ordinary HP/temp/turn/round edits remain frozen until the real explicit
    // bounded refresh. That refresh must retain a PlayerOnly snapshot too.
    c.characters[0].currentHitPoints=9;c.characters[0].temporaryHitPoints=0;
    c.encounters[0].round=2;c.encounters[0].activeParticipantIndex=1;ui.frame(2);
    require(display.combatStatsUpdatePending()&&localPlayerDisplayIsPublished(c.localPlayerDisplay),"Allowed combat refresh did not remain pending");
    assertRetained(canonical);require(display.updateApprovedCombatStats(),"Explicit bounded combat refresh failed");
    ui.frame(2);assertRetained(PlayerDisplayReview::build(c,locale,PlayerDisplayReview::BuildPurpose::PlayerOnly));
    require(!display.combatStatsUpdatePending(),"Applied combat refresh stayed pending");
    c.handouts[0].body="CHANGED_PUBLIC_OUTSIDE_COMBAT";ui.frame(2);
    require(c.localPlayerDisplay.blackout&&!localPlayerDisplayIsPublished(c.localPlayerDisplay)&&
        !contains(ui.rendered,"FROZEN_PUBLIC_TEXT")&&!display.updateApprovedCombatStats(),"Non-combat text bypassed canonical runtime guard");
    c.handouts[0].body="FROZEN_PUBLIC_TEXT";
    const auto valid=PlayerDisplayReview::build(c,locale);
    for(bool compactFacade:{false,true})for(int change=0;change<7;++change){
        require(PlayerDisplayReview::publish(c,valid),"Cannot establish valid flags for installation refusal control");
        auto altered=valid;
        if(change==0)altered.locale=de?"en":"de";
        if(change==1)altered.hasMapPreview=false;
        if(change==2)altered.playerMapPlan.mapId="UNREVIEWED_MAP_ID";
        if(change==3)++altered.playerMapPlan.mapRevision;
        if(change==4)++altered.playerMapPlan.contentHash;
        if(change==5)altered.eligible=false;
        if(change==6)++altered.contentFingerprint;
        const bool installed=compactFacade?display.publicationCommitted(PlayerDisplayReview::ownConfirmation(altered)):
            display.publicationCommitted(altered);
        require(!installed&&c.localPlayerDisplay.blackout&&!c.localPlayerDisplay.published&&
            !PlayerReviewUiProbe::publicationProof(display),"Installation accepted mismatched locale/map identity with reused fingerprint");
    }
    // Same-size/mtime source replacement must still refuse installation before
    // any renderer sees it. These bytes are deliberately never decoded/drawn.
    auto &handout=c.handouts[0];handout.filePath="Resources/Campaigns/"+c.metadata.id+"/Assets/Handouts/install-canary.png";
    const auto path=ApplicationPaths::writableRoot()/std::filesystem::u8path(handout.filePath);
    std::filesystem::create_directories(path.parent_path());
    {std::ofstream out(path,std::ios::binary);out<<"OLD_REVIEW_BYTES";require(bool(out),"Cannot write isolated reviewed bytes");}
    const auto fileReview=PlayerDisplayReview::build(c,locale);
    const auto fileTime=std::filesystem::last_write_time(path);const auto fileSize=std::filesystem::file_size(path);
    require(fileReview.eligible&&PlayerDisplayReview::publish(c,fileReview),"Cannot establish actual bound-file review");
    {std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<"NEW_REVIEW_BYTES";require(bool(out),"Cannot replace isolated reviewed bytes");}
    std::filesystem::last_write_time(path,fileTime);
    require(std::filesystem::file_size(path)==fileSize&&std::filesystem::last_write_time(path)==fileTime,
        "Installation replacement did not preserve reviewed size/mtime");
    require(!display.publicationCommitted(fileReview)&&c.localPlayerDisplay.blackout&&!c.localPlayerDisplay.published&&
        !PlayerReviewUiProbe::publicationProof(display),"Same-metadata file replacement bypassed installation validation");
    ui.draw=[]{};ui.frame();
}
