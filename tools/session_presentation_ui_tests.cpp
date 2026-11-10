#include "workflow_test_database.hpp"
#include "Views/Editor/SessionOutcomesPanel.hpp"
#include "Views/Editor/SessionWorkspace.hpp"
#include "Views/Editor/Widgets/UiInteractionTrace.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Session/LiveOutcomeSessionSelection.hpp"
#include "Models/Session/PlayerDisplayReviewConfirmation.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardController.hpp"
#include "Controllers/Editor/Widgets/BattleTracker/BattleTrackerController.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include "session_target_ui_probe.hpp"
#include "player_review_ui_probe.hpp"
#include "player_review_async_test_support.hpp"
#include "Models/Map/MapRenderCompileMemo.hpp"
#include "map_render_plan_snapshot.hpp"
#include "Views/Editor/Presentation/PlayerReviewContent.hpp"
#include "Views/Editor/Widgets/ScopedImageTextureCache.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"
#include "guidance_ui_probe.hpp"
#include "selected_preview_fixture.hpp"
#include "context_preview_ui_probe.hpp"
#include "Views/Editor/Presentation/SelectedPlayerPreview.hpp"
#include "spell_override_ui_probe.hpp"
#include "battle_tracker_session_probe.hpp"
#include "Views/Editor/ExchangePanel.hpp"
#include "markdown_task_test_support.hpp"
#include "frozen_image_decode_test_support.hpp"
#include "Views/Editor/Widgets/FrozenImageUi.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace
{
constexpr const char *PrivateSession = "CANARY_PRIVATE_SESSION_7821";
constexpr const char *SavedOutcome = "CANARY_SAVED_OUTCOME_4103";
constexpr const char *DraftOutcome = "CANARY_UNFINISHED_OUTCOME_9562";
constexpr const char *SavedNotes = "CANARY_SAVED_PREPARATION_6470";
constexpr const char *DraftNotes = "CANARY_UNFINISHED_PREPARATION_5834";
void require(bool condition, const char *message)
{ if (!condition) throw std::runtime_error(message); }
bool contains(const std::string &text, const char *value) { return text.find(value) != std::string::npos; }
void requirePrivateTextHidden(const std::string &rendered)
{
    require(!rendered.empty(), "Presentation render produced no observable text");
    for (const auto *canary : {PrivateSession, SavedOutcome, DraftOutcome, SavedNotes, DraftNotes})
        require(!contains(rendered, canary), "Private canary was submitted in presentation mode");
}
Campaign fixture()
{
    Campaign campaign; campaign.name = "Synthetic presentation fixture";
    SessionModel session; session.title = PrivateSession; session.roughNotes = SavedNotes;
    SessionOutcomes::Ledger ledger;
    SessionOutcomes::Outcome outcome; outcome.privateText = SavedOutcome;
    ledger.outcomes.push_back(outcome); SessionOutcomes::write(session, ledger);
    campaign.sessionIds.push_back(session.metadata.id); campaign.sessions.push_back(session);
    return campaign;
}

#include "session_headless_panel.hpp"

void outcomeGate(bool german, bool launcher)
{
    HeadlessPanel ui(german); auto campaign = fixture();
    SessionOutcomesPanel panel;
    const auto sessionId = campaign.sessions.front().metadata.id;
    int commits = 0;
    const SessionPlan::Persist persist = [&](Campaign &) { ++commits; return false; };
    ui.draw = [&] {
        if (launcher) panel.drawLauncher(campaign, sessionId, persist);
        else panel.draw(campaign, sessionId, persist);
    };
    ui.frame(3);
    if (launcher) ui.click(german ? "Ergebnisse und nächste Sitzung" : "Outcomes and next session");
    ui.type("##outcome-private", DraftOutcome);
    require(contains(ui.rendered, DraftOutcome), "GM positive control did not render unfinished outcome text");
    ui.click(german ? "Prüfen" : "Review");
    require(contains(ui.rendered, SavedOutcome), "GM positive control did not render saved outcome text");
    campaign.presentationModeActive = true; ui.frame(2);
    requirePrivateTextHidden(ui.rendered);
    require(contains(ui.rendered, german ? "Private Ergebnisse sind ausgeblendet" : "Private outcomes are hidden"),
        "Outcomes presentation gate did not render its safe explanation");
    ui.absent("##outcome-private"); ui.absent("##review-select");
    ui.absent(german ? "Ergebnis festhalten" : "Record outcome");
    ui.absent(german ? "Vorschlag verwerfen" : "Reject proposal");
    ui.absent(german ? "Ergebnisse und nächste Sitzung" : "Outcomes and next session");
    require(!contains(ui.rendered, german ? "Was ist passiert? (nur SL)" : "What happened? (GM only)"),
        "Private composer label leaked through presentation gate");
    campaign.presentationModeActive = false; ui.frame(3);
    // A popup may have closed while its launcher was gated. Reopening is an
    // ordinary production action; the owned draft must survive either case.
    if (launcher && !contains(ui.rendered, SavedOutcome))
        ui.click(german ? "Ergebnisse und nächste Sitzung" : "Outcomes and next session");
    require(contains(ui.rendered, SavedOutcome), "Saved outcome disappeared when returning to GM mode");
    ui.click(german ? "Festhalten" : "Record");
    require(contains(ui.rendered, DraftOutcome), "Unfinished outcome draft was lost across presentation mode");
    const auto saved = SessionOutcomes::read(campaign.sessions.front());
    require(commits == 0 && saved.success && saved.ledger.outcomes.size() == 1 &&
        saved.ledger.outcomes.front().privateText == SavedOutcome, "Presentation toggle altered committed outcomes");
}

void workspaceGate(bool german)
{
    HeadlessPanel ui(german); auto campaign = fixture(); SessionWorkspace workspace;
    int commits = 0; SessionWorkspace::Actions actions;
    actions.persist = [&](Campaign &) { ++commits; return false; };
    ui.draw = [&] { workspace.draw(campaign, actions); };
    ui.frame(3);
    require(contains(ui.rendered, SavedNotes), "GM positive control did not render saved preparation");
    ui.type("##rough-notes", DraftNotes);
    require(workspace.dirty() && contains(ui.rendered, DraftNotes), "GM preparation edit did not create a visible unfinished draft");
    const auto sessionId = workspace.sessionId();
    campaign.presentationModeActive = true; ui.frame(2);
    requirePrivateTextHidden(ui.rendered);
    require(contains(ui.rendered, german ? "Die private Sitzungsvorbereitung ist ausgeblendet" : "Private session preparation is hidden"),
        "Preparation presentation gate did not render its safe explanation");
    ui.absent("##rough-notes"); ui.absent("##session-picker");
    ui.absent(german ? "Szene aus Auswahl erstellen" : "Create scene from selection");
    ui.absent(german ? "Ergebnisse und nächste Sitzung" : "Outcomes and next session");
    require(!contains(ui.rendered, german ? "Ideen und Notizen" : "Rough notes"), "Private preparation controls leaked into the rendered log");
    campaign.presentationModeActive = false; ui.frame(3);
    require(workspace.dirty() && workspace.sessionId() == sessionId && contains(ui.rendered, DraftNotes),
        "Preparation draft or selection was lost across presentation mode");
    require(commits == 0 && campaign.sessions.front().roughNotes == SavedNotes, "Presentation toggle committed or changed private preparation");
}
#include "session_outcome_draft_ui_cases.hpp"
#include "session_outcome_source_ui_cases.hpp"
#include "session_menu_ui_cases.hpp"
#include "combat_correction_ui_cases.hpp"
#include "compact_outcome_popup_ui_cases.hpp"
#include "compact_combat_popup_ui_cases.hpp"
#include "session_start_ui_cases.hpp"
#include "session_report_ui_cases.hpp"
#include "player_review_gm_ui_cases.hpp"
#include "player_review_editor_ui_cases.hpp"
#include "player_review_memo_ui_cases.hpp"
#include "player_review_async_ui_cases.hpp"
#include "player_review_async_guard_cases.hpp"
#include "player_review_handout_slot_ui_cases.hpp"
#include "frozen_image_pending_ui_cases.hpp"
#include "guidance_ui_cases.hpp"
#include "guidance_workflow_ui_cases.hpp"
#include "guidance_map_ui_cases.hpp"
#include "session_navigation_ui_cases.hpp"
#include "session_carry_navigation_ui_cases.hpp"
#include "selected_preview_ui_cases.hpp"
#include "npc_read_ui_cases.hpp"
#include "context_preview_workflow_ui_cases.hpp"
#include "spell_override_ui_cases.hpp"
#include "spell_override_combat_cases.hpp"
#include "markdown_exchange_ui_cases.hpp"
}

int main()
{
    try
    {
        isolateWorkflowDatabase();
        // Keep automatic recovery checkpoints with this invocation's generated
        // database, rather than the development tree's campaign packages.
        const auto root = ApplicationPaths::databasePath().parent_path().string();
#ifdef _WIN32
        require(_putenv_s("TTRPG_DATA_ROOT", root.c_str()) == 0, "Cannot isolate UI recovery fixtures");
#else
        require(setenv("TTRPG_DATA_ROOT", root.c_str(), 1) == 0, "Cannot isolate UI recovery fixtures");
#endif
        for (const bool german : {false, true})
        {
            markdownUpdatesUi(german);markdownPendingUi(german);
            spellOverrideMagicUi(german);
            selectedPreviewUi(german,false);selectedPreviewUi(german,true);npcReadWorkflow(german);
            contextItemDraftReview(german);contextInventoryReview(german);contextReviewRevocation(german);
            sessionRunTabNavigation(german);sessionPreparedRun(german);sessionStrictPreparationTarget(german);
            sessionCarryNavigation(german);sessionCarryExistingDestination(german);
            guidanceSharedTopics(german);guidanceNestedModal(german);
            guidanceOutcomeDraft(german);guidanceParticipants(german);guidanceMapControls(german);guidanceEmbeddedLive(german);
            compactOutcomePopup(german);compactCombatPopup(german);
            outcomeGate(german, false); outcomeGate(german, true); workspaceGate(german);
            retainedRecap(german, 0); retainedRecap(german, 1); retainedRecap(german, 2);
            pristineOutcomeSource(german);
            for (int mode = 0; mode < 7; ++mode) pinnedOutcomeSource(german, mode);
            for (int mode = 0; mode < 3; ++mode) unavailableAutomaticOutcomeSource(german, mode);
            liveOutcomeSourceCaller(german);
            for(const bool compact:{false,true}) {
                sessionCreateMenu(german,compact);sessionArrangeMenu(german,compact);combatCorrectionPopup(german,compact);
                for(bool legacy:{false,true})for(bool prepared:{false,true})reviewedStartUi(german,compact,legacy,prepared);
                completedReportUi(german,compact);finishAndHistoryUi(german,compact);
                playerReviewGmUi(german,compact);
                playerReviewEditorUi(german,compact);
            }
            staleStartUi(german);
            playerReviewMemoIntegration(german);
            playerPublicationCanonicalCopy(german);
            playerReviewHandoutSlotUi(german);
            frozenPendingSlotUi(german);
            playerReviewAsyncPendingUi(german,false);playerReviewAsyncPendingUi(german,true);
            playerReviewPendingPublishedGuard(german);
            for(int boundary=0;boundary<3;++boundary)playerReviewReadyBeforeClose(german,boundary);
            for(bool legacy:{false,true}){playerReviewAsyncForegroundUi(german,legacy);playerReviewCompactKeyboard(german,legacy);}
        }
        outcomeCheckpointCadence(); recapFailedCheckpointDiscard(); liveSelectionAndDrafts();
        spellOverrideCombatGuards();spellOverrideCharacterCastGuard();
        markdownOwnerRevocationUi();
        frozenUnseenRecapUi();
        std::cout << "Headless session checks passed: prior presentation/recovery/source-scene checks, 12 menu/combat groups and 26 reviewed-start/completed-report/finish UI groups (EN/DE, 1282x752 and 960x720 at 150% text).\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
