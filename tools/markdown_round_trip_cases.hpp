StoryThreadModel &markdownNote(Campaign &c,const EntityId &id)
{
    const auto found=std::find_if(c.storyThreads.begin(),c.storyThreads.end(),[&](const auto &n){return n.metadata.id==id;});
    check(found!=c.storyThreads.end(),"Expected stable note identity is missing");return *found;
}
UpdatePreview markdownChanges(const Campaign &c)
{
    UpdatePreview preview;preview.success=true;preview.campaignId=c.metadata.id;
    for(const auto &n:c.storyThreads)preview.entries.push_back({n.metadata.id,n.title,n.gmNotes,n.gmNotes+" edited","reader-validated"});
    return preview;
}
void markdownClassification()
{
    auto c=fixture();c.storyThreads.clear();
    for(int i=0;i<4;++i){StoryThreadModel note;note.title="Same title";note.gmNotes="BASE"+std::to_string(i);c.storyThreads.push_back(note);}
    auto p=markdownChanges(c);p.entries[0].editedText=p.entries[0].baseText;
    c.storyThreads[1].gmNotes=p.entries[1].editedText;c.storyThreads[3].gmNotes="LIVE CONFLICT";
    const auto id=c.storyThreads[2].metadata.id;c.storyThreads[2].title="Renamed exact note";
    std::reverse(c.storyThreads.begin(),c.storyThreads.end());const auto rows=classifyUpdates(c,p);
    check(rows.size()==4&&rows[0].state==UpdateState::Unchanged&&rows[1].state==UpdateState::AlreadyApplied&&
        rows[2].state==UpdateState::Eligible&&rows[2].id==id&&rows[2].title=="Renamed exact note"&&
        rows[3].state==UpdateState::Conflict,"Stable-ID classification did not distinguish four independent states");
    for(int mode=0;mode<4;++mode) {
        auto altered=c;auto reviewed=p;
        if(mode==0)altered.storyThreads.erase(std::remove_if(altered.storyThreads.begin(),altered.storyThreads.end(),
            [&](const auto &n){return n.metadata.id==p.entries[0].id;}),altered.storyThreads.end());
        if(mode==1)altered.storyThreads.push_back(markdownNote(altered,p.entries[0].id));
        if(mode==2)CampaignTags::setEntityArchived(altered,p.entries[0].id,true);
        if(mode==3)reviewed.entries.push_back(reviewed.entries[0]);
        check(classifyUpdates(altered,reviewed)[0].state==UpdateState::Unavailable,
            "Missing/archived/duplicate identity escaped through unchanged-text shortcut");
    }
    auto wrong=c;wrong.metadata.id=createEntityId("other");
    check(classifyUpdates(wrong,p)[2].state==UpdateState::Unavailable,"Cross-campaign update classified eligible");
    const auto captured=captureExport(c);check(sameExport(captured,captureExport(c)),"Unchanged export capture differs");
    markdownNote(c,id).gmNotes+="new";check(!sameExport(captured,captureExport(c)),"Changed raw note passed export capture comparison");
}
void markdownApplyPersistence()
{
    auto c=fixture();StoryThreadModel second;second.title=c.storyThreads[0].title;second.gmNotes="SECOND_BASE";c.storyThreads.push_back(second);
    CampaignRepository::saveCampaign(c);const auto first=c.storyThreads[0].metadata.id,other=second.metadata.id;
    auto p=markdownChanges(c);int saves=0;const DurableSave save=[&](Campaign &staged){++saves;CampaignRepository::saveCampaign(staged);return ProjectSaveResult{ProjectSaveOutcome::Committed,{},{}};};
    const auto liveBefore=c.storyThreads[0].gmNotes;
    SQLite::Database sql(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
    sql.exec("CREATE TRIGGER reject_note_update BEFORE INSERT ON story_threads BEGIN SELECT RAISE(ABORT,'injected roundtrip save failure'); END;");
    const auto failed=applyUpdates(c,p,{first,other},save,[]{return true;});
    check(failed.state==ApplyState::NotCommitted&&saves==1&&!failed.error.empty()&&
        markdownNote(c,first).gmNotes==liveBefore&&markdownNote(c,other).gmNotes=="SECOND_BASE",
        "Failed multi-note save changed live text or hid refusal");
    auto loaded=reload(c.metadata.id);check(markdownNote(loaded,first).gmNotes==liveBefore&&markdownNote(loaded,other).gmNotes=="SECOND_BASE",
        "Failed multi-note save changed persisted notes");sql.exec("DROP TRIGGER reject_note_update;");
    const auto applied=applyUpdates(c,p,{first,other},save,[]{return true;});loaded=reload(c.metadata.id);
    check(applied.state==ApplyState::Applied&&applied.noteIds.size()==2&&saves==2&&
        markdownNote(loaded,first).gmNotes==p.entries[0].editedText&&markdownNote(loaded,other).gmNotes==p.entries[1].editedText,
        "One explicit multi-note retry did not commit both exact texts once");
    check(applyUpdates(c,p,{first,other},save,[]{return true;}).state==ApplyState::NoChange&&saves==2,
        "Already-applied retry repeated persistence");
    auto next=markdownChanges(c);
    check(applyUpdates(c,next,{first,first},save,[]{return true;}).state==ApplyState::NotCommitted&&saves==2,
        "Duplicate selected identity reached persistence");
    check(applyUpdates(c,next,{first},save,{}).state==ApplyState::NotCommitted&&saves==2,
        "Missing owner guard reached persistence");
    check(applyUpdates(c,next,{first},save,[]{return false;}).state==ApplyState::NotCommitted&&saves==2,
        "Revoked owner reached persistence");
}
void markdownPostSaveMerge()
{
    for(int mode=0;mode<3;++mode) {
        auto c=fixture();CampaignRepository::saveCampaign(c);auto p=markdownChanges(c);const auto id=p.entries[0].id;
        int saves=0;bool current=true;
        const auto result=applyUpdates(c,p,{id},[&](Campaign &staged){
            ++saves;CampaignRepository::saveCampaign(staged);
            c.description="LATE_UNRELATED_CAMPAIGN";markdownNote(c,id).summary="LATE_UNRELATED_NOTE";
            if(mode==1)markdownNote(c,id).gmNotes="LATE_TOUCHED_NOTE";
            if(mode==2){auto replacement=c;replacement.description="SAME_ID_REPLACEMENT";c=std::move(replacement);current=false;}
            return ProjectSaveResult{ProjectSaveOutcome::Committed,{},{}};
        },[&]{return current;});
        auto loaded=reload(c.metadata.id);
        check(saves==1&&markdownNote(loaded,id).gmNotes==p.entries[0].editedText,"Callback case lost durable committed update");
        check(result.state==(mode==0?ApplyState::Applied:ApplyState::CommittedContextChanged),
            "Post-save result did not distinguish committed context change");
        check(markdownNote(c,id).summary=="LATE_UNRELATED_NOTE"&&
            c.description==(mode==2?"SAME_ID_REPLACEMENT":"LATE_UNRELATED_CAMPAIGN"),"Post-save merge overwrote unrelated live changes");
        check(markdownNote(c,id).gmNotes==(mode==0?p.entries[0].editedText:mode==1?"LATE_TOUCHED_NOTE":p.entries[0].baseText),
            "Post-save merge overwrote newer touched/reset state");
    }
}
