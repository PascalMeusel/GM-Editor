// Real EditorWindowController leave/save entry points, driven by its production
// outcome widgets. Application request/native-close dispatch is source-reviewed;
// no Application, GLFW window, clipboard, or native event is created here.
void outcomeLeaveRecovery(EditorWindowController &editor, Interaction &ui, bool german, bool live)
{
    Campaign campaign; campaign.name = "Synthetic session leave fixture";
    campaign.ruleSet = std::make_shared<DND5e>();
    SessionModel session; session.title = "Session recovery"; session.liveModeActive = live;
    if (live) session.startedAtUtc = currentUtcTimestamp();
    campaign.sessions.push_back(session); campaign.sessionIds.push_back(session.metadata.id);
    require(ProjectSaveService::getInstance().saveCampaign(campaign, "isolated leave fixture"), "Could not save session leave fixture");
    editor.setCurrentCampaign(campaign); editor.activateWorkspaceByIndex(live ? 6 : 0); ui.frame(3);
    if (live) ui.click(german ? "Ergebnisse und Abschluss" : "Outcomes and wrap-up");
    else ui.click(german ? "Ergebnisse und nächste Sitzung" : "Outcomes and next session", "Workspace");
    ui.replaceText("##outcome-private", "BASELINE_PRIVATE_DRAFT");
    require(editor.checkpointOutcomeRecoveryForLeave(), "Initial leave recovery checkpoint failed");
    const auto surface = live ? SessionOutcomeDraftKeys::Surface::Live : SessionOutcomeDraftKeys::Surface::Preparation;
    const auto key = SessionOutcomeDraftKeys::composer(surface, session.metadata.id);
    const auto baseline = TextDraftRecovery::read(campaign, key);
    require(baseline.success && baseline.found, "Initial private recovery checkpoint absent");
    const auto began = ImGui::GetTime();
    ui.replaceText("##outcome-private", "LATEST_PRIVATE_TEXT_BEFORE_EXIT");
    require(ImGui::GetTime() - began < 2.0, "Leave regression did not exercise edits within checkpoint interval");
    require(editor.hasUnsavedChanges(), "Uncheckpointed outcome omitted from application unsaved guard");
    require(TextDraftRecovery::read(campaign, key).fields == baseline.fields, "Ordinary editing bypassed checkpoint cadence");
    const auto path = ProjectPackageLayout::layoutForCampaign(campaign).recoveryDirectory / "Drafts" / (key + ".draft");
    const auto prior = path.parent_path() / (key + ".baseline");
    std::filesystem::rename(path, prior); std::filesystem::create_directory(path);
    { std::ofstream blocker(path / "fixture-blocker"); blocker << "synthetic write failure"; }
    require(!editor.checkpointOutcomeRecoveryForLeave() && editor.hasUnsavedChanges() && !editor.outcomeRecoveryError().empty(),
        "Failed recovery did not block the real application leave guard with an error");
    require(!editor.saveCurrentCampaign("save and continue with blocked recovery") && editor.hasUnsavedChanges(),
        "Canonical save incorrectly counted as successful private recovery");
    require(SessionOutcomes::read(readCampaign(campaign.metadata.id).sessions.front()).ledger.outcomes.empty(),
        "Leaving implicitly recorded unfinished private text as an outcome");
    std::filesystem::remove(path / "fixture-blocker"); std::filesystem::remove(path); std::filesystem::rename(prior, path);
    require(editor.checkpointOutcomeRecoveryForLeave() && editor.outcomeRecoveryError().empty() && !editor.hasUnsavedChanges(),
        "Leave recovery retry failed or left a stale error/pending flag");
    const auto recovered = TextDraftRecovery::read(campaign, key);
    require(recovered.success && recovered.found && recovered.fields.at("ledger").find("LATEST_PRIVATE_TEXT_BEFORE_EXIT") != std::string::npos,
        "Successful leave guard did not recover the exact latest text");
    // The production campaign reset follows a successful leave guard. Reopen
    // and restore through the UI to prove the checkpoint survives teardown.
    if (!live) ui.click(german ? "Zurück zur Sitzung" : "Return to session");
    editor.setCurrentCampaign(readCampaign(campaign.metadata.id)); editor.activateWorkspaceByIndex(live ? 6 : 0); ui.frame(3);
    if (!live) ui.click(german ? "Ergebnisse und nächste Sitzung" : "Outcomes and next session", "Workspace");
    else ui.click(german ? "Ergebnisse und Abschluss" : "Outcomes and wrap-up");
    ui.click(german ? "Ergebnisentwurf wiederherstellen" : "Restore outcome draft");
    require(!editor.hasUnsavedChanges(), "Restoring already durable text incorrectly created pending writes");
    require(editor.checkpointOutcomeRecoveryForLeave(), "Restored private draft was not safe to leave");
    // BeginPopup has a generated ImGui name. Resolve the actual scroll parent
    // from its observed input; the ordinary click path still checks clipping.
    const auto outcomeScrollWindow = ui.observedScrollWindow("##outcome-private");
    ui.click(german ? "Ergebnis festhalten" : "Record outcome", outcomeScrollWindow.c_str());
    const auto recorded = SessionOutcomes::read(readCampaign(campaign.metadata.id).sessions.front());
    require(recorded.success && recorded.ledger.outcomes.size() == 1 &&
        recorded.ledger.outcomes.front().privateText == "LATEST_PRIVATE_TEXT_BEFORE_EXIT",
        "Restored text was not recorded exactly once after explicit review");
    if (live)
    {
        ui.replaceText("##outcome-private", "EXPLICITLY_DISCARDED_OLD_CAMPAIGN_DRAFT", outcomeScrollWindow.c_str());
        require(editor.hasUnsavedChanges(), "Discard reset regression has no pending old-campaign text");
        Campaign replacement; replacement.name = "Synthetic replacement campaign";
        replacement.ruleSet = std::make_shared<DND5e>();
        require(ProjectSaveService::getInstance().saveCampaign(replacement, "replacement fixture"), "Cannot save replacement fixture");
        ProjectSaveService::getInstance().discardUnsavedChanges("explicit test discard");
        editor.setCurrentCampaign(replacement); ui.frame(3);
        require(!editor.hasUnsavedChanges() && editor.checkpointOutcomeRecoveryForLeave(), "Discarded Live drafts blocked the replacement campaign");
        require(!TextDraftRecovery::read(replacement, key).found, "Old campaign draft was written into replacement campaign recovery");
    }
    if (!live)
    {
        const std::string label = german ? "Zurück zur Sitzung" : "Return to session";
        const auto footer = ui.locate(label);
        auto *popup = ImGui::FindWindowByName(footer.window.c_str());
        require(popup && (popup->Flags & ImGuiWindowFlags_Popup) &&
            popup->InnerClipRect.Contains(ImRect(footer.minimum, footer.maximum)),
            "Return to session is not fully visible in its actual popup owner");
        // The fixed footer belongs to the popup, outside the scrolling body
        // observed from the private-text input above.
        ui.click(label);
    }
}
