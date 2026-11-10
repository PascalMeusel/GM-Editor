void reviewedSessionServiceCases()
{
    Campaign fresh; fresh.name="Reviewed session"; CharacterModel character; fresh.characters.push_back(character);
    const auto review=SessionPlan::reviewStart(fresh,"Exact reviewed new title");
    const auto before=fresh.metadata.updatedAtUtc; int attempts=0;
    check(review.valid&&review.create&&!review.sessionId.empty(),"New review must own one allocated identity");
    for(int mode=0;mode<2;++mode) {
        const auto failed=SessionPlan::startReviewed(fresh,review,[&](Campaign &staged)->bool {
            ++attempts; const auto *session=SessionPlan::findSession(staged,review.sessionId);
            check(staged.sessions.size()==1&&staged.sessionIds.size()==1&&session&&session->liveModeActive&&
                session->liveState.phase==SessionPhase::Setup,"New session was not complete in the single staged write");
            if(mode)throw std::runtime_error("injected save exception");return false;
        });
        check(!failed.applied()&&fresh.sessions.empty()&&fresh.sessionIds.empty()&&fresh.metadata.updatedAtUtc==before,
            "Failed reviewed start leaked a new session or changed metadata");
    }
    const auto save=[&](Campaign &){++attempts;return true;};
    check(SessionPlan::startReviewed(fresh,review,save).applied()&&attempts==3,"New start retry must use exactly one write");
    const auto *started=SessionPlan::findSession(fresh,review.sessionId);
    check(started&&started->title==review.title&&started->liveModeActive&&!started->startedAtUtc.empty()&&
        started->participantCharacterIds==std::vector<EntityId>{character.metadata.id}&&fresh.sessionIds==std::vector<EntityId>{review.sessionId},
        "New start did not preserve reviewed identity/title/party");
    check(!SessionPlan::startReviewed(fresh,review,save).applied()&&attempts==3,"Repeated start was not refused before saving");
    Campaign prepared; SessionModel selected;selected.title="Actual prepared title";selected.scheduledAtUtc="2026-10-05";
    prepared.sessions.push_back(selected);prepared.sessionIds.push_back(selected.metadata.id);
    const auto captured=SessionPlan::reviewStart(prepared,"Ignored default");
    check(!captured.create&&captured.title==selected.title,"Prepared review showed a different title");
    for(int mode=0;mode<8;++mode) {
        Campaign stale=prepared;
        if(mode==0)stale.sessions[0].title="Renamed";
        if(mode==1)stale.sessions[0].metadata.tags.push_back("archived");
        if(mode==2)stale.sessions.clear();
        if(mode==3)stale.sessions[0].startedAtUtc="started";
        if(mode==4)stale.sessions[0].endedAtUtc="ended";
        if(mode==5) {SessionModel active;active.liveModeActive=true;stale.sessions.push_back(active);}
        if(mode==6) {SessionModel earlier;earlier.scheduledAtUtc="2026-10-04";stale.sessions.push_back(earlier);}
        if(mode==7)stale.metadata.id="another-campaign";
        check(!SessionPlan::startReviewed(stale,captured,save).applied()&&attempts==3,"Stale prepared review saved or silently changed target");
    }
    check(SessionPlan::startReviewed(prepared,captured,save).applied()&&attempts==4&&prepared.sessions.size()==1,
        "Prepared start must save once and preserve existing identity");
    Campaign collision;auto newReview=SessionPlan::reviewStart(collision,"Valid");collision.sessionIds.push_back(newReview.sessionId);
    check(!SessionPlan::startReviewed(collision,newReview,save).applied()&&attempts==4,"Orphan ID collision was not refused");
    collision.sessionIds.clear();newReview.title=" \t\n";
    check(!SessionPlan::startReviewed(collision,newReview,save).applied()&&attempts==4,"Blank new title persisted");

    Campaign reports;SessionModel older,latest,future;
    older.title="OLDER";older.startedAtUtc="2026-01-01";older.endedAtUtc="2026-01-02";
    latest.title="LATEST";latest.startedAtUtc="2026-02-01";latest.endedAtUtc="2026-02-02";future.title="FUTURE";
    reports.sessions={older,latest,future};
    check(SessionReport::resolve(reports)->metadata.id==latest.metadata.id,"Automatic report selected future prepared tail");
    const auto report=SessionReport::review(reports,older.metadata.id);
    check(SessionReport::validate(reports,report)->metadata.id==older.metadata.id,"Explicit completed report target lost");
    std::reverse(reports.sessions.begin(),reports.sessions.end());
    check(SessionReport::resolve(reports)->metadata.id==latest.metadata.id,"Report depends on vector order");
    for(int mode=0;mode<4;++mode) {
        auto changed=reports;auto *target=SessionPlan::findSession(changed,older.metadata.id);
        if(mode==0)target->title="renamed";
        if(mode==1)target->metadata.tags.push_back("archived");
        if(mode==2)target->endedAtUtc.clear();
        if(mode==3)changed.sessions.erase(std::remove_if(changed.sessions.begin(),changed.sessions.end(),[&](const auto &s){return s.metadata.id==older.metadata.id;}),changed.sessions.end());
        check(!SessionReport::validate(changed,report),"Changed report review silently retargeted");
    }
    auto *live=SessionPlan::findSession(reports,future.metadata.id);live->startedAtUtc="now";live->liveModeActive=true;
    check(SessionReport::resolve(reports)->metadata.id==future.metadata.id&&SessionReport::resolve(reports,older.metadata.id)->metadata.id==older.metadata.id,
        "Automatic active or deliberate historical selection incorrect");
    check(!SessionReport::resolve(reports,"deleted-id"),"Missing explicit report silently fell through");
    const auto activeReport=SessionReport::review(reports);live->liveModeActive=false;live->endedAtUtc="later";
    check(!SessionReport::validate(reports,activeReport),"A reviewed active report accepted a changed completed status");
    for(auto &session:reports.sessions)session.metadata.tags.push_back("archived");
    check(!SessionReport::resolve(reports)&&!SessionReport::review(reports).valid,"All-archived reports must be unavailable");
    reports.sessions.clear();check(!SessionReport::resolve(reports),"An empty campaign has a report target");
}
