// Included in the CPU-only session panel test. No native window or audio
// service is created; Live receives a null playback service and no audio assets.
struct OutcomeSourceFixture
{
    Campaign campaign;
    EntityId sessionId, sceneA, sceneB, entryA, entryB;
    OutcomeSourceFixture()
    {
        campaign.name = "Synthetic outcome source fixture";
        campaign.ruleSet = std::make_shared<DND5e>();
        StoryThreadModel thread; thread.title = "Source fixture thread";
        StoryBeatModel a, b; a.title = "Source scene A"; b.title = "Source scene B";
        sceneA = a.metadata.id; sceneB = b.metadata.id; thread.beats = {a, b};
        campaign.storyThreads.push_back(thread);
        SessionModel session; session.title = "Source fixture session";
        session.liveModeActive = true; session.startedAtUtc = currentUtcTimestamp();
        SessionPlanEntry first, second; first.storyBeatId = sceneA; second.storyBeatId = sceneB;
        entryA = first.metadata.id; entryB = second.metadata.id;
        session.planEntries = {first, second}; session.liveState.initialized = true;
        session.liveState.phase = SessionPhase::Scene; session.liveState.activePlanEntryId = entryA;
        sessionId = session.metadata.id; campaign.sessions.push_back(session); campaign.sessionIds.push_back(sessionId);
    }
    EntityId currentScene() const
    {
        const auto *session = SessionPlan::findSession(campaign, sessionId);
        const auto *entry = session ? SessionPlan::findEntry(*session, session->liveState.activePlanEntryId) : nullptr;
        return entry ? entry->storyBeatId : EntityId{};
    }
    void activate(const EntityId &entry)
    {
        require(SessionPlan::activateEntry(campaign, sessionId, entry, [](Campaign &) { return true; }).applied(),
            "Source fixture could not activate its other real planned scene");
    }
    std::string key() const
    { return SessionOutcomeDraftKeys::composer(SessionOutcomeDraftKeys::Surface::Preparation, sessionId); }
    SessionOutcomes::Ledger ledger() const
    { return SessionOutcomes::read(*SessionPlan::findSession(campaign, sessionId)).ledger; }
};

void chooseOutcomeSource(HeadlessPanel &ui, bool german, const EntityId &scene)
{
    const char *sourceLabel = german ? "Ursprungsszene" : "Source scene";
    // This pinned ImGui version emits geometry, but no label hook for inline
    // BeginCombo. BeginTabItem pushes the observed tab ID as the widget seed;
    // resolve that exact native ID, retaining unique/visible rectangle checks.
    const ImGuiID sourceId = ImHashStr(sourceLabel, 0, ui.controlId(german ? "Festhalten" : "Record"));
    if (!ui.hasControl(sourceLabel, sourceId)) ui.click(german ? "Ursprung und Spielertext" : "Origin and player text");
    ui.click(sourceLabel, sourceId);
    ui.click(scene.empty() ? "-" : ("Source scene A##" + scene).c_str());
}

void pristineOutcomeSource(bool german)
{
    HeadlessPanel ui(german); OutcomeSourceFixture fixture; SessionOutcomesPanel panel;
    std::optional<EntityId> contextOverride;
    ui.draw = [&] { panel.draw(fixture.campaign, fixture.sessionId, [](Campaign &) { return true; },
        contextOverride ? *contextOverride : fixture.currentScene()); };
    ui.frame(3); fixture.activate(fixture.entryB); ui.frame(3);
    require(!panel.hasPendingDraftWrites() && panel.checkpointDrafts(fixture.campaign) &&
        !TextDraftRecovery::read(fixture.campaign, fixture.key()).found, "Pristine scene changes created recovery writes");
    const char *record = german ? "Ergebnis festhalten" : "Record outcome";
    ui.type("##outcome-private", "First text belongs to B"); ui.click(record);
    require(fixture.ledger().outcomes.size() == 1 && fixture.ledger().outcomes.back().sourceSceneId == fixture.sceneB,
        "Pristine composer silently retained its first scene A");
    fixture.activate(fixture.entryA); ui.frame(3);
    ui.type("##outcome-private", "Next outcome follows A"); ui.click(record);
    require(fixture.ledger().outcomes.size() == 2 && fixture.ledger().outcomes.back().sourceSceneId == fixture.sceneA,
        "Successful Record did not reset source following for the next draft");
    // Losing valid context must clear the automatic origin, rather than retain
    // whichever scene happened to be drawn previously.
    for (const EntityId context : {EntityId{}, EntityId{"not-in-the-plan"}})
    {
        ui.frame(2); contextOverride = context; ui.frame(2);
        require(!panel.hasPendingDraftWrites() && !TextDraftRecovery::read(fixture.campaign, fixture.key()).found,
            "Invalid/empty context alone created a private draft");
        ui.type("##outcome-private", "No scene context"); ui.click(record);
        require(fixture.ledger().outcomes.back().sourceSceneId.empty(), "Pristine invalid/empty context kept an old scene");
        contextOverride.reset();
    }
}

void pinnedOutcomeSource(bool german, int mode)
{
    HeadlessPanel ui(german); OutcomeSourceFixture fixture; SessionOutcomesPanel panel;
    // 0: edited/checkpointed; 1: same explicit source; 2: unchanged explicit '-';
    // 3: recovered empty draft; 4: edited then erased; 5: failed Record;
    // 6: deliberately selected source removed after the draft starts.
    bool acceptSave = mode != 5;
    bool emptyContext = mode == 2;
    ui.draw = [&] { panel.draw(fixture.campaign, fixture.sessionId, [&](Campaign &) { return acceptSave; },
        emptyContext ? EntityId{} : fixture.currentScene()); };
    if (mode == 3)
    {
        SessionOutcomes::Ledger recovered; SessionOutcomes::Outcome value;
        value.sourceSceneId = fixture.sceneA; // Deliberate source, still no prose.
        recovered.outcomes.push_back(value);
        require(TextDraftRecovery::save(fixture.campaign, fixture.key(), {{"session", fixture.sessionId},
            {"ledger", SessionOutcomes::serialize(recovered)}}).empty(), "Could not seed an isolated source recovery draft");
        fixture.activate(fixture.entryB); ui.frame(3);
        ui.click(german ? "Ergebnisentwurf wiederherstellen" : "Restore outcome draft"); ui.frame(2);
    }
    else
    {
        ui.frame(3);
        if (mode == 1 || mode == 2 || mode == 6) chooseOutcomeSource(ui, german, mode == 2 ? EntityId{} : fixture.sceneA);
        else
        {
            ui.type("##outcome-private", "Draft started in A");
            if (mode == 4)
            {
                ui.type("##outcome-private", "");
                require(!contains(ui.rendered, "Draft started in A"), "Erase-source regression did not clear the visible text");
            }
            if (mode == 5)
            {
                ui.click(german ? "Ergebnis festhalten" : "Record outcome");
                require(fixture.ledger().outcomes.empty() && contains(ui.rendered, "Draft started in A"),
                    "Failed Record changed canonical outcomes or lost text");
            }
        }
        require(panel.checkpointDrafts(fixture.campaign) && !panel.hasPendingDraftWrites(), "Started source draft could not checkpoint");
        fixture.activate(fixture.entryB); emptyContext = false; ui.frame(3);
        if (mode == 6)
        {
            fixture.campaign.storyThreads.front().beats.erase(fixture.campaign.storyThreads.front().beats.begin());
            auto &entries = fixture.campaign.sessions.front().planEntries; entries.erase(entries.begin());
            ui.frame(2);
        }
    }
    acceptSave = true;
    ui.type("##outcome-private", "Pinned origin retained");
    ui.click(german ? "Ergebnis festhalten" : "Record outcome");
    if (mode == 6)
    {
        require(fixture.ledger().outcomes.empty() && contains(ui.rendered, "Pinned origin retained"),
            "Removed explicit source was silently replaced or discarded");
        const auto saved = TextDraftRecovery::read(fixture.campaign, fixture.key());
        require(saved.success && saved.found && SessionOutcomes::deserialize(saved.fields.at("ledger")).ledger.outcomes.front().sourceSceneId == fixture.sceneA,
            "Rejected missing source was not preserved for correction");
    }
    else require(fixture.ledger().outcomes.size() == 1 && fixture.ledger().outcomes.front().sourceSceneId ==
        (mode == 2 ? EntityId{} : fixture.sceneA), "An edited, selected, or recovered origin followed a later scene");
}

void unavailableAutomaticOutcomeSource(bool german, int mode)
{
    HeadlessPanel ui(german); OutcomeSourceFixture fixture; SessionOutcomesPanel panel;
    ui.draw = [&] { panel.draw(fixture.campaign, fixture.sessionId, [](Campaign &) { return true; }, fixture.currentScene()); };
    ui.frame(3);
    auto &thread = fixture.campaign.storyThreads.front();
    if (mode == 0) thread.beats.erase(thread.beats.begin());
    else if (mode == 1) thread.beats.front().metadata.tags.push_back("archived");
    else thread.metadata.tags.push_back("archived");
    // Keep the plan entry deliberately: unavailable story data can leave an
    // old live cursor. Automatic context must not pin this stale reference.
    ui.frame(3);
    require(!panel.hasPendingDraftWrites() && !TextDraftRecovery::read(fixture.campaign, fixture.key()).found,
        "Unavailable automatic source created an unfinished draft");
    ui.type("##outcome-private", "Session-only outcome after unavailable scene");
    ui.click(german ? "Ergebnis festhalten" : "Record outcome");
    require(fixture.ledger().outcomes.size() == 1 && fixture.ledger().outcomes.front().sourceSceneId.empty(),
        "Automatic source followed a missing scene or archived scene/thread");
}

void liveOutcomeSourceCaller(bool german)
{
    HeadlessPanel ui(german); OutcomeSourceFixture fixture;
    CampaignRepository::saveCampaign(fixture.campaign);
    LiveSessionDashboardController live(fixture.campaign, nullptr);
    ui.draw = [&] { live.renderEmbedded(); };
    ui.frame(3); ui.click(german ? "Ergebnisse und Abschluss" : "Outcomes and wrap-up");
    ui.frame(2); ui.click(german ? "Sitzung leiten" : "Run session");
    // Use the real live-session transition service. The real Live view must
    // resolve the new active entry and pass B to its owned outcome composer.
    require(SessionPlan::activateEntry(fixture.campaign, fixture.sessionId, fixture.entryB,
        [](Campaign &staged) { return ProjectSaveService::getInstance().saveCampaign(staged, "source fixture scene transition"); }).applied(),
        "Live source fixture could not commit its scene transition");
    ui.frame(2); ui.click(german ? "Ergebnisse und Abschluss" : "Outcomes and wrap-up");
    ui.type("##outcome-private", "Actual Live caller source B");
    ui.click(german ? "Ergebnis festhalten" : "Record outcome");
    bool verified = false;
    for (const auto &saved : CampaignRepository::loadAllCampaigns())
        if (saved.metadata.id == fixture.campaign.metadata.id)
        {
            const auto *session = SessionPlan::findSession(saved, fixture.sessionId);
            const auto read = session ? SessionOutcomes::read(*session) : SessionOutcomes::ReadResult{};
            verified = read.success && read.ledger.outcomes.size() == 1 &&
                read.ledger.outcomes.front().privateText == "Actual Live caller source B" &&
                read.ledger.outcomes.front().sourceSceneId == fixture.sceneB;
        }
    require(verified, "Real Live caller recorded the new outcome against the previous scene");
}
