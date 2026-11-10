// Included inside the presentation test's anonymous namespace. Exercises real
// production ImGui controls and isolated recovery files, without a native app.
void retainedRecap(bool german, int change)
{
    HeadlessPanel ui(german); auto campaign = fixture(); SessionOutcomesPanel panel;
    const auto sid = campaign.sessions.front().metadata.id;
    auto initial = SessionOutcomes::read(campaign.sessions.front()).ledger;
    initial.publicTitle = "Saved player recap title";
    SessionOutcomes::write(campaign.sessions.front(), initial);
    const auto key = SessionOutcomeDraftKeys::recap(SessionOutcomeDraftKeys::Surface::Preparation, sid);
    int commits = 0;
    const SessionPlan::Persist persist = [&](Campaign &) { ++commits; return true; };
    ui.draw = [&] { panel.draw(campaign, sid, persist); };
    ui.frame(3); ui.click(german ? "Spieler-Rückblick" : "Player recap");
    ui.type("##player-outcome", "UNSAVED_RETAINED_PLAYER_TEXT");
    ui.click(german ? "Prüfen" : "Review");
    require(TextDraftRecovery::read(campaign, key).found, "Recap tab departure did not checkpoint text");
    if (change == 0) ui.click(german ? "Vorschlag verwerfen" : "Reject proposal");
    else
    {
        auto ledger = SessionOutcomes::read(campaign.sessions.front()).ledger;
        if (change == 1) ledger.outcomes.clear();
        else { ledger.outcomes.front().publicText = "NEW_SAVED_PUBLIC_TEXT"; ledger.outcomes.front().shareWithPlayers = true; }
        SessionOutcomes::write(campaign.sessions.front(), ledger);
    }
    ui.click(german ? "Spieler-Rückblick" : "Player recap");
    require(contains(ui.rendered, "UNSAVED_RETAINED_PLAYER_TEXT") &&
        contains(ui.rendered, german ? "Zurückbehaltene Entwürfe" : "Retained drafts"), "Non-publishable draft was hidden");
    ui.click(german ? "Spieler-Rückblick prüfen" : "Review player recap");
    require(contains(ui.rendered, german ? "Genau dieser Text wird ausgegeben" : "Exactly what players will receive"),
        "Retained draft blocked eligible recap review");
    const auto reviewed = SessionOutcomes::reviewRecap(campaign, sid);
    require(reviewed.success && !contains(SessionOutcomes::markdown(reviewed.document), "UNSAVED_RETAINED_PLAYER_TEXT"),
        "Retained draft escaped into player projection");
    require(commits == (change == 0 ? 1 : 0), "Draft review unexpectedly committed campaign changes");
    panel.checkpointDrafts(campaign);
    panel = SessionOutcomesPanel{}; ui.frame(3);
    ui.click(german ? "Rückblick-Entwurf wiederherstellen" : "Restore recap draft"); ui.frame(2);
    require(contains(ui.rendered, "UNSAVED_RETAINED_PLAYER_TEXT"), "Rejected/deleted/stale draft did not restore after restart");
    ui.click(german ? "Spieler-Rückblick prüfen" : "Review player recap");
    require(contains(ui.rendered, german ? "Genau dieser Text wird ausgegeben" : "Exactly what players will receive"),
        "Restored retained draft blocked eligible player review");
    require(!contains(SessionOutcomes::markdown(SessionOutcomes::reviewRecap(campaign, sid).document), "UNSAVED_RETAINED_PLAYER_TEXT"),
        "Recovered retained text escaped into player projection");
    ui.click(german ? "Zurückbehaltenen Entwurf verwerfen" : "Discard retained draft"); ui.frame(2);
    require(!contains(ui.rendered, "UNSAVED_RETAINED_PLAYER_TEXT"), "Explicit retained draft discard did not clear the UI");
    require(!TextDraftRecovery::read(campaign, key).found, "Explicit discard left a recoverable stale draft");
}

void outcomeCheckpointCadence()
{
    HeadlessPanel ui(false); auto campaign = fixture(); SessionOutcomesPanel panel;
    const auto sid = campaign.sessions.front().metadata.id;
    const auto key = SessionOutcomeDraftKeys::composer(SessionOutcomeDraftKeys::Surface::Preparation, sid);
    int commits = 0;
    ui.draw = [&] { panel.draw(campaign, sid, [&](Campaign &) { ++commits; return true; }); };
    ui.frame(3); ui.type("##outcome-private", "FIRST_CHECKPOINT_TEXT");
    require(!TextDraftRecovery::read(campaign, key).found, "Ordinary typing forced a checkpoint before two seconds");
    ImGui::GetIO().DeltaTime = .25f; ui.frame(9); ImGui::GetIO().DeltaTime = 1.f / 60.f;
    auto saved = TextDraftRecovery::read(campaign, key);
    require(saved.success && saved.found && contains(saved.fields.at("ledger"), "FIRST_CHECKPOINT_TEXT"), "Two-second checkpoint did not run");
    ui.type("##outcome-private", "SECOND_CHECKPOINT_TEXT");
    require(TextDraftRecovery::read(campaign, key).fields == saved.fields, "Ordinary edit rewrote checkpoint before cadence");
    ui.click("Review");
    require(contains(TextDraftRecovery::read(campaign, key).fields.at("ledger"), "SECOND_CHECKPOINT_TEXT"), "Tab switch did not force latest checkpoint");
    ui.click("Record");
    // A directory at this isolated fixture's file path injects a failed write.
    const auto path = ProjectPackageLayout::layoutForCampaign(campaign).recoveryDirectory / "Drafts" / (key + ".draft");
    std::filesystem::remove(path); std::filesystem::create_directory(path);
    { std::ofstream blocker(path / "fixture-blocker"); blocker << "keep"; }
    ui.type("##outcome-private", "RETRY_RETAINED_TEXT");
    campaign.presentationModeActive = true; ui.frame(2);
    // Remove only the exact synthetic blocker and its now-empty directory.
    std::filesystem::remove(path / "fixture-blocker"); std::filesystem::remove(path);
    ui.frame(3);
    require(!TextDraftRecovery::read(campaign, key).found, "Presentation retried a failed checkpoint every frame");
    ImGui::GetIO().DeltaTime = .25f; ui.frame(9); ImGui::GetIO().DeltaTime = 1.f / 60.f;
    require(contains(TextDraftRecovery::read(campaign, key).fields.at("ledger"), "RETRY_RETAINED_TEXT"), "Failed checkpoint text was lost or not retried");
    campaign.presentationModeActive = false; ui.frame(3);
    require(contains(ui.rendered, "RETRY_RETAINED_TEXT") && commits == 0, "Checkpoint lifecycle changed campaign or lost draft");
}

void recapFailedCheckpointDiscard()
{
    HeadlessPanel ui(false); auto campaign = fixture(); SessionOutcomesPanel panel;
    const auto sid = campaign.sessions.front().metadata.id;
    ui.draw = [&] { panel.draw(campaign, sid, [](Campaign &) { return true; }); };
    ui.frame(3); ui.click("Player recap"); ui.type("##player-outcome", "FAILED_FIRST_CHECKPOINT");
    const auto directory = ProjectPackageLayout::layoutForCampaign(campaign).recoveryDirectory / "Drafts";
    std::filesystem::create_directories(directory.parent_path());
    { std::ofstream blocker(directory); blocker << "synthetic blocked draft directory"; }
    ui.click("Review"); ui.click("Reject proposal"); ui.click("Player recap");
    require(contains(ui.rendered, "FAILED_FIRST_CHECKPOINT"), "Failed first checkpoint lost retained text");
    std::filesystem::remove(directory);
    ui.click("Discard retained draft"); ui.frame(2);
    require(!contains(ui.rendered, "FAILED_FIRST_CHECKPOINT"), "Stale write error prevented an explicit no-file discard");
}

void liveSelectionAndDrafts()
{
    HeadlessPanel ui(false); auto campaign = fixture();
    const auto first = campaign.sessions.front().metadata.id;
    SessionModel second; second.title = "Second session"; campaign.sessions.push_back(second);
    const auto next = second.metadata.id;
    LiveOutcomeSessionSelection selection;
    require(selection.synchronize(campaign, first) && selection.sessionId == first && !selection.historical, "Initial Live selection did not follow active session");
    SessionOutcomesPanel panel(SessionOutcomeDraftKeys::Surface::Live);
    bool save = false; int attempts = 0, commits = 0;
    ui.draw = [&] { panel.draw(campaign, selection.sessionId, [&](Campaign &) { ++attempts; if (save) ++commits; return save; }); };
    ui.frame(3); ui.type("##outcome-private", "FIRST_SESSION_DRAFT");
    require(selection.synchronize(campaign, next) && selection.sessionId == next, "Live did not follow a newly active session");
    ui.frame(3); ui.type("##outcome-private", "SECOND_SESSION_OUTCOME"); ui.click("Record outcome");
    require(attempts == 1 && commits == 0 && contains(ui.rendered, "SECOND_SESSION_OUTCOME"), "Failed explicit save lost draft or committed twice");
    const auto key = SessionOutcomeDraftKeys::composer(SessionOutcomeDraftKeys::Surface::Live, next);
    const auto failedSave = TextDraftRecovery::read(campaign, key);
    require(failedSave.success && failedSave.found && contains(failedSave.fields.at("ledger"), "SECOND_SESSION_OUTCOME"),
        "Failed explicit save did not retain a recoverable checkpoint");
    save = true; ui.click("Record outcome");
    require(attempts == 2 && commits == 1 && SessionOutcomes::read(campaign.sessions[1]).ledger.outcomes.size() == 1,
        "Retry did not commit exactly once in current session");
    require(SessionOutcomes::read(campaign.sessions[0]).ledger.outcomes.size() == 1, "Live recorded into previous session");
    require(!TextDraftRecovery::read(campaign, key).found, "Successful retry left a duplicate recovery draft");
    selection.choose(first, next); require(selection.historical, "Explicit historical selection was not pinned");
    selection.synchronize(campaign, next); ui.frame(3);
    require(selection.sessionId == first && contains(ui.rendered, "FIRST_SESSION_DRAFT"), "Historical selection or old draft lost on active refresh");
    selection.followCurrent(); selection.synchronize(campaign, next);
    require(selection.sessionId == next && !selection.historical, "Follow current session did not clear historical mode");
    selection.choose(first, next); selection.synchronize(campaign, first);
    require(selection.sessionId == first && !selection.historical, "Selected session becoming active retained a misleading historical mode");
    selection.synchronize(campaign, next);
    require(selection.sessionId == next, "Selection did not follow Live after the selected session became active");
    selection.choose(first, next); campaign.sessions.front().metadata.tags.push_back("archived"); selection.synchronize(campaign, next);
    require(selection.sessionId == next && !selection.historical, "Archived historical session did not fall back to active");
    selection.choose(first, next); campaign.sessions.erase(campaign.sessions.begin()); selection.synchronize(campaign, next);
    require(selection.sessionId == next && !selection.historical, "Deleted historical session did not fall back to active");
    require(SessionOutcomeDraftState::checkpointDue(12, 10, false) && !SessionOutcomeDraftState::checkpointDue(11.9, 10, false) &&
        SessionOutcomeDraftState::checkpointDue(10, 10, true), "Checkpoint cadence boundary is incorrect");
}
