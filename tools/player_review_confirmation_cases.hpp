// Compact confirmation must preserve the authoritative boundaries without
// keeping the preview's geometry, strings or owner alive across callbacks.
void compactConfirmationIdentity(const fs::path &root)
{
    auto c=fixture(root);PlayerReviewSession owner;require(owner.prepare(c,"en"),"Cannot prepare compact identity fixture");
    const auto original=owner.review();const auto memo=owner.compileMemo();
    const auto good=ownConfirmation(original);
    require(good.eligible&&good.hasMapPreview&&good.locale==original.locale&&good.contentFingerprint==original.contentFingerprint&&
        good.mapId==original.playerMapPlan.mapId&&good.mapRevision==original.playerMapPlan.mapRevision&&
        good.mapContentHash==original.playerMapPlan.contentHash&&good.images.size()==1&&
        good.images[0].projectRelativePath==original.playerHandouts[0].projectRelativePath&&
        good.images[0].contentHash==original.playerHandouts[0].contentHash,"Compact ownership omitted reviewed identity or asset binding");
    for(int change=0;change<9;++change){
        auto snapshot=original;
        if(change==1)snapshot.eligible=false;
        if(change==2)snapshot.contentFingerprint=0;
        if(change==3)++snapshot.contentFingerprint;
        if(change==4)snapshot.locale="de";
        if(change==5)snapshot.hasMapPreview=false;
        if(change==6)snapshot.playerMapPlan.mapId="UNREVIEWED_MAP";
        if(change==7)++snapshot.playerMapPlan.mapRevision;
        if(change==8)++snapshot.playerMapPlan.contentHash;
        const auto compact=ownConfirmation(snapshot);const bool expected=change==0;
        require(matchesConfirmation(original,compact)==expected&&
            isCurrent(c,snapshot,nullptr,Origin::PreSave,memo.get())==expected&&
            isCurrent(c,compact,nullptr,Origin::PreSave,memo.get())==expected,
            "Snapshot/compact confirmation identity refusal differs");
        if(!expected){int saves=0;auto first=c,second=c;
            const Persist persist=[&](Campaign &){++saves;return true;};
            require(armAndSave(first,snapshot,persist).status==TransactionStatus::ReviewChanged&&
                armAndSave(second,compact,persist).status==TransactionStatus::ReviewChanged&&saves==0&&
                !first.localPlayerDisplay.enabled&&!second.localPlayerDisplay.enabled,
                "Invalid confirmation crossed persistence or changed arm state");}
    }
}
void compactConfirmationCallbackLifetime(const fs::path &root)
{
    for(bool publishing:{false,true})for(bool compactFacade:{false,true}){
        auto c=fixture(root);PlayerReviewSession owner;require(owner.prepare(c,"en"),"Cannot prepare compact callback fixture");
        const auto memo=owner.compileMemo();const auto hits=memo->stats().hits;
        auto snapshot=std::make_unique<PlayerDisplayReviewSnapshot>(owner.review());
        auto compact=std::make_unique<Confirmation>(ownConfirmation(*snapshot));
        const auto fingerprint=compact->contentFingerprint;
        if(publishing)armLocalPlayerDisplay(c.localPlayerDisplay);
        Measurements measured;int saves=0;
        const Persist persist=[&](Campaign &staged){++saves;owner.clear();snapshot.reset();compact.reset();staged.metadata.touch();return true;};
        const auto result=compactFacade?commitReviewedState(c,*compact,persist,publishing,&measured,memo.get()):
            commitReviewedState(c,*snapshot,persist,publishing,&measured,memo.get());
        require(result.applied()&&saves==1&&!owner.prepared()&&!snapshot&&!compact&&memo->stats().hits==hits+2,
            "Compact transaction retained a destroyed source or skipped canonical memo validation");
        require(count(measured,Stage::Build)==2&&count(measured,Stage::PreValidation)==1&&count(measured,Stage::PostValidation)==1&&
            count(measured,Stage::GmCompile)==0&&count(measured,Stage::PlayerCompile)==2&&count(measured,Stage::ConfirmationOwnership)>=1,
            "Compact transaction omitted fresh canonical pre/post builds or compiled private comparison geometry");
        require(c.localPlayerDisplay.enabled&&localPlayerDisplayIsPublished(c.localPlayerDisplay)==publishing&&
            (publishing?c.localPlayerDisplay.publishedReviewFingerprint==fingerprint:c.localPlayerDisplay.blackout),
            "Callback-safe confirmation committed the wrong reviewed display state");
    }
    auto c=fixture(root);PlayerReviewSession owner;require(owner.prepare(c,"en"),"Cannot prepare compact post-save rejection");
    const auto compact=ownConfirmation(owner.review());const auto memo=owner.compileMemo();const auto hits=memo->stats().hits;
    int saves=0;
    require(armAndSave(c,compact,[&](Campaign &staged){++saves;owner.clear();staged.handouts[0].body="CHANGED_DURING_SAVE";return true;},
        nullptr,memo.get()).status==TransactionStatus::ReviewChanged&&saves==1&&memo->stats().hits==hits+2&&
        c.handouts[0].body=="PUBLIC_SNAPSHOT_BODY"&&!c.localPlayerDisplay.enabled,
        "Compact callback lifetime bypassed post-save public-payload refusal");
}
