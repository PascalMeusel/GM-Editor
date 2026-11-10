#include "fixtures/DeterministicSessionFixture.hpp"
#include "tools/workflow_test_database.hpp"
#include "Controllers/Editor/Widgets/BattleTracker/BattleTrackerController.hpp"
#include "Controllers/Editor/EditorWindowController.hpp"
#include "Controllers/Editor/Widgets/LocalPlayerDisplay/LocalPlayerDisplayController.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ProjectPackageExportService.hpp"
#include "Models/Project/ProjectPackageImportService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Audio/AudioBackend.hpp"
#include <fstream>
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <algorithm>

struct WorkspaceInteractionProbe
{
    static void attachAudio(EditorWindowController &editor, const std::shared_ptr<AudioPlaybackService> &audio)
    {
        editor._audioPlaybackService = audio;
    }
};

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
        throw std::runtime_error(message);
}
class RecordingAudioBackend final : public AudioCommandBackend
{
  public:
    const char *name() const override
    {
        return "Generated session command recorder";
    }
    bool sendCommand(const std::string &, std::string &error) const override
    {
        error.clear();
        return true;
    }
    bool queryCommand(const std::string &, std::string &result, std::string &error) const override
    {
        result = "1000";
        error.clear();
        return true;
    }
};
Campaign reload(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns())
        if (campaign.metadata.id == id)
        {
            // Database collection ordering is not an identity contract. Arrange
            // fixture collections by their stable IDs; leave plan order intact.
            const auto byId = [](const auto &a, const auto &b) { return a.metadata.id < b.metadata.id; };
            std::sort(campaign.maps.begin(), campaign.maps.end(), byId);
            std::sort(campaign.characters.begin(), campaign.characters.end(), byId);
            return campaign;
        }
    throw std::runtime_error("Generated campaign missing from SQLite");
}
}

// Access to the same commands used by the combat view. This executable links
// the complete production controller, rules, save service and SQLite repository.
// It is service integration coverage; actual input/framebuffer tests are separate.
struct BattleTrackerTurnsProbe
{
#include "complete_session_correction_tests.hpp"

    static void exercise(Campaign &campaign, const EntityId &encounterId)
    {
        auto audio = std::make_shared<AudioPlaybackService>(std::make_unique<RecordingAudioBackend>());
        AudioAssetModel sound;
        gm::qa::identify(sound.metadata, "test-audio");
        sound.name = "Generated audio command fixture";
        const auto audioPath = ApplicationPaths::databasePath().parent_path() / "generated-command-fixture.wav";
        {
            std::ofstream file(audioPath, std::ios::binary);
            file << "command-adapter fixture; not hardware playback";
        }
        std::string audioError;
        require(audio->play(sound, audioPath, .5f, true, audioError), "Audio service did not enter playing state");
        BattleTrackerController tracker(campaign, audio);
        require(tracker.selectEncounterById(encounterId), "Encounter selection failed");
        tracker.beginCombat();
        require(!tracker._encounterSaveFailed && tracker._draftEncounter.combatPhase == CombatPhase::ActiveTurn,
                "Combat did not start durably");
        tracker._selectedParticipantIndex = 0;
        exerciseFailedSaveCorrections(campaign, tracker);
        tracker.applyHitPointDeltaToSelected(-4);
        tracker.setSelectedSourceResourceUsed(0, 1);
        tracker._selectedParticipantIndex = 1;
        tracker.setSelectedSpellSlotUsed(1, 1);
        auto disk = reload(campaign.metadata.id);
        require(disk.characters[0].currentHitPoints == 24 && disk.characters[0].resourceTrackers[0].used == 1 &&
                    disk.characters[1].spellcasting.slotUsed[1] == 1,
                "Combat resources were not persisted to canonical characters");
        tracker.nextTurn();
        const auto activeId =
            tracker._draftEncounter.participants[tracker._draftEncounter.activeParticipantIndex].metadata.id;
        tracker.startCombatTableBreak(true);
        require(audio->isPaused(sound.metadata.id), "Table break did not pause the actual audio service");
        disk = reload(campaign.metadata.id);
        require(disk.sessions[0].liveState.phase == SessionPhase::Break &&
                    disk.encounters[0].combatPhase == CombatPhase::PausedBreak,
                "Break lost the session/combat transaction");
        campaign = std::move(disk);
        // Exercise the production campaign-load boundary with active audio,
        // instead of stopping playback from the test itself.
        {
            EditorWindowController editor;
            WorkspaceInteractionProbe::attachAudio(editor, audio);
            editor.setCurrentCampaign(campaign);
            require(audio->activeAssetIds().empty(), "Campaign reload failed to stop playback");
            require(!editor.currentCampaign().localPlayerDisplay.enabled &&
                        editor.currentCampaign().localPlayerDisplay.blackout &&
                        !editor.currentCampaign().localPlayerDisplay.published,
                    "Campaign reload reopened or published the player window");
        }
        auto restartedAudio = std::make_shared<AudioPlaybackService>(std::make_unique<RecordingAudioBackend>());
        require(restartedAudio->activeAssetIds().empty() && !restartedAudio->playlistPlaybackState().active,
                "Restart restored audio or playlist playback");
        BattleTrackerController resumed(campaign, restartedAudio);
        require(resumed.selectEncounterById(encounterId), "Reloaded encounter unavailable");
        resumed.resumeCombatFromTableBreak(true);
        require(restartedAudio->activeAssetIds().empty(), "Resume after restart invented prior playback");
        disk = reload(campaign.metadata.id);
        require(disk.sessions[0].liveState.phase == SessionPhase::Combat &&
                    disk.encounters[0].combatPhase == CombatPhase::ActiveTurn &&
                    disk.encounters[0].participants[disk.encounters[0].activeParticipantIndex].metadata.id == activeId,
                "Break/reload/resume lost active participant or phase");
    }
};

int main()
{
    try
    {
        isolateWorkflowDatabase();
        auto &save = ProjectSaveService::getInstance();
        save.setBackupSnapshotsEnabled(false);
        save.setRecoverySnapshotsEnabled(false);
        auto campaign = gm::qa::threeSceneCampaign();
        const auto campaignId = campaign.metadata.id;
        const auto sessionId = campaign.sessions[0].metadata.id;
        const auto encounterId = campaign.encounters[0].metadata.id;
        std::vector<EntityId> entries;
        for (const auto &entry : campaign.sessions[0].planEntries)
            entries.push_back(entry.metadata.id);
        const SessionPlan::Persist persist = [&](Campaign &value) {
            return save.saveCampaign(value, "Generated complete session QA");
        };
        require(persist(campaign), "Initial fixture save failed");
        campaign = reload(campaignId);
        require(campaign.maps.size() == 3 && campaign.sessions[0].planEntries.size() == 3 &&
                    campaign.encounters[0].mapId == campaign.maps[2].metadata.id,
                "Linked preparation did not round trip");
        require(SessionPlan::updateEntry(campaign, sessionId, entries[0], "Ask Lenna about the bridge",
                                         SessionEntryState::Planned, persist)
                    .applied(),
                "Preparation entry update failed");
        SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
        control.exec(
            "CREATE TRIGGER reject_session_qa BEFORE INSERT ON custom_items BEGIN SELECT RAISE(ABORT, 'injected session failure'); END;");
        require(!SessionPlan::startWithEntry(campaign, sessionId, entries[0], persist).applied() &&
                    !campaign.sessions[0].liveModeActive && !reload(campaignId).sessions[0].liveModeActive,
                "Failed start changed live or durable state");
        control.exec("DROP TRIGGER reject_session_qa;");
        require(SessionPlan::startWithEntry(campaign, sessionId, entries[0], persist).applied(), "Start retry failed");
        require(SessionPlan::setPhase(campaign, sessionId, SessionPhase::Exploration, persist).applied(),
                "Exploration transition failed");
        require(SessionPlan::advanceClock(campaign, "10 minutes", "Walk to bridge", persist).applied(),
                "Travel clock did not advance");
        require(SessionPlan::activateEntry(campaign, sessionId, entries[1], persist).applied() &&
                    SessionPlan::activateEncounter(campaign, sessionId, encounterId, persist).applied(),
                "Linked combat launch failed");
        BattleTrackerTurnsProbe::exercise(campaign, encounterId);
        LocalPlayerDisplayController display(campaign);
        campaign.localPlayerDisplay.enabled = true;
        campaign.localPlayerDisplay.publicMapId = campaign.maps[2].metadata.id;
        campaign.localPlayerDisplay.publicSceneId = campaign.storyThreads[0].beats[1].metadata.id;
        auto review = PlayerDisplayReview::build(campaign);
        require(review.eligible && display.publish(review), "Reviewed player map could not publish");
        require(buildPlayerSafeMapTokens(campaign.maps[2]).empty() && !campaign.maps[2].tokens.empty(),
                "Hidden token negative control failed");
        const auto publicScene = buildPlayerSafeStoryScene(campaign);
        require(publicScene.playerSummary == gm::qa::publicMarker &&
                    publicScene.playerSummary.find(gm::qa::secretMarker) == std::string::npos,
                "Public scene omitted approved text or exposed GM notes");
        display.blackout();
        require(campaign.localPlayerDisplay.blackout && !campaign.localPlayerDisplay.published, "Blackout failed");
        require(display.publish(PlayerDisplayReview::build(campaign)) && persist(campaign), "Publication/save failed");
        campaign = reload(campaignId);
        display.onCampaignLoaded();
        require(!campaign.localPlayerDisplay.enabled && campaign.localPlayerDisplay.blackout &&
                    !campaign.localPlayerDisplay.published,
                "Restart reopened or published the player window");
        require(SessionPlan::activateEntry(campaign, sessionId, entries[2], persist).applied(),
                "Third scene launch failed");
        SessionPlan::WrapUpDraft wrap;
        wrap.summary = "Bridge secured";
        wrap.recap = "The party reached the watchtower";
        wrap.unresolvedThreads = "Who hired the raiders?";
        wrap.lootAwarded = "Recovered supplies";
        require(SessionPlan::finishWithDraft(campaign, sessionId, wrap, persist).applied(), "Wrap-up failed");
        campaign = reload(campaignId);
        require(!campaign.sessions[0].liveModeActive && campaign.sessions[0].liveState.phase == SessionPhase::WrapUp,
                "Finished session resumed as live");
        save.setRecoverySnapshotsEnabled(true);
        require(persist(campaign) && save.state().lastRecoveryError.empty() &&
                    !save.state().lastRecoverySnapshotPath.empty(),
                "Complete-session recovery snapshot failed");
        const auto recoveryPath = save.state().lastRecoverySnapshotPath;
        Campaign recovered;
        std::string recoveryError;
        const bool recoveryOpened =
            CampaignRepository::readCampaignFromSnapshot(recoveryPath, campaignId, recovered, recoveryError);
        require(recoveryOpened, "Complete-session recovery snapshot could not be opened: " + recoveryError);
        require(recovered.metadata.id == campaignId && recovered.sessions[0].metadata.id == sessionId &&
                    recovered.sessions[0].planEntries.size() == 3 &&
                    recovered.sessions[0].liveState.phase == SessionPhase::WrapUp &&
                    recovered.encounters[0].mapId == campaign.encounters[0].mapId,
                "Recovery changed campaign/session identities, ordered scenes or map links");
        // Exercise a deep recovery path even when the local build directory is
        // shorter than the SHA-scoped CI checkout. Do not shorten the fixture
        // to hide a Windows MAX_PATH failure in production snapshot reopening.
        auto deepSnapshot = ApplicationPaths::databasePath().parent_path() / "long-snapshot";
        for (int depth = 0; deepSnapshot.native().size() <= 280; ++depth)
            deepSnapshot /= std::string(64, static_cast<char>('a' + depth % 26));
        std::filesystem::create_directories(mapFileIoPath(deepSnapshot));
        deepSnapshot /= "recovery.db";
        require(deepSnapshot.native().size() > 260, "Deep-path regression did not exceed MAX_PATH");
        std::filesystem::copy_file(mapFileIoPath(std::filesystem::path(recoveryPath)), mapFileIoPath(deepSnapshot));
        Campaign deepRecovered;
        const bool deepOpened = CampaignRepository::readCampaignFromSnapshot(deepSnapshot.string(), campaignId,
                                                                             deepRecovered, recoveryError);
        require(deepOpened, "Deep recovery snapshot could not be opened: " + recoveryError);
        require(deepRecovered.metadata.id == campaignId && deepRecovered.sessions[0].metadata.id == sessionId &&
                    deepRecovered.sessions[0].liveState.phase == SessionPhase::WrapUp,
                "Deep recovery path changed campaign/session content");
        recovered.description = "Changed after recovery snapshot";
        require(persist(recovered), "Post-snapshot edit failed");
        require(CampaignRepository::readCampaignFromSnapshot(recoveryPath, campaignId, recovered, recoveryError),
                "Retained recovery snapshot became unreadable after a later save");
        require(persist(recovered), "Recovered campaign could not be committed");
        campaign = reload(campaignId);
        require(campaign.description != "Changed after recovery snapshot" &&
                    campaign.characters[0].currentHitPoints == 24 &&
                    campaign.characters[1].spellcasting.slotUsed[1] == 1,
                "Recovery did not restore authored content and combat resource changes");
        save.setRecoverySnapshotsEnabled(false);
        ProjectPackageExport::ExportRequest request;
        request.destinationDirectory = ApplicationPaths::databasePath().parent_path() / "session-transfer";
        const auto exported = ProjectPackageExport::exportCampaignPackage(campaign, request);
        if (!exported.success)
            throw std::runtime_error("Completed campaign export failed: " + exported.errorMessage);
#if defined(_WIN32)
        const auto packageDatabase = ProjectPackageLayout::layoutForCampaign(campaign).databasePath;
        // Fresh exports do not refresh the live recovery package. Create that
        // independent fixture explicitly, with older content that must survive.
        ProjectPackageLayout::ensureCampaignPackageLayout(campaign);
        Campaign liveSnapshot = campaign;
        liveSnapshot.description = "Generated retained live snapshot, independent of transfer";
        CampaignRepository::writeCampaignSnapshot(liveSnapshot, packageDatabase.u8string());
        const auto snapshotBytes = [](const std::filesystem::path &path) {
            std::ifstream input(mapFileIoPath(path), std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        };
        const auto beforeLockedSnapshot = snapshotBytes(packageDatabase);
        require(!beforeLockedSnapshot.empty(), "Generated live snapshot fixture is empty");
        const HANDLE lockedSnapshot = CreateFileW(mapFileIoPath(packageDatabase).c_str(), GENERIC_READ, FILE_SHARE_READ,
                                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        require(lockedSnapshot != INVALID_HANDLE_VALUE, "Could not hold the generated live package snapshot");
        auto blockedRequest = request;
        blockedRequest.destinationDirectory /= "blocked-transfer";
        const auto blockedExport = ProjectPackageExport::exportCampaignPackage(campaign, blockedRequest);
        CloseHandle(lockedSnapshot);
        require(blockedExport.success,
                "Fresh transfer should not replace the locked live package snapshot: " + blockedExport.errorMessage);
        require(snapshotBytes(packageDatabase) == beforeLockedSnapshot,
                "Fresh transfer changed bytes in the locked live package snapshot");
        {
            SQLite::Database isolated(mapFileIoPath(blockedExport.databasePath).u8string(), SQLite::OPEN_READONLY);
            require(isolated.execAndGet("SELECT COUNT(*) FROM campaigns;").getInt() == 1 &&
                    isolated.execAndGet("SELECT stable_id FROM campaigns;").getString() == campaignId &&
                    isolated.execAndGet("SELECT description FROM campaigns;").getString() == campaign.description,
                    "Transfer created while live snapshot was locked did not isolate the selected campaign");
        }
        Campaign retainedSnapshot;
        std::string retainedError;
        require(CampaignRepository::readCampaignFromSnapshot(packageDatabase.string(), campaignId, retainedSnapshot,
                                                             retainedError) &&
                    retainedSnapshot.metadata.id == campaignId && retainedSnapshot.sessions[0].metadata.id == sessionId &&
                    retainedSnapshot.description == liveSnapshot.description,
                "Fresh transfer damaged the locked live recovery snapshot");
#endif
        auto secondRequest = request;
        secondRequest.destinationDirectory /= "second-transfer";
        const auto secondExport = ProjectPackageExport::exportCampaignPackage(campaign, secondRequest);
        if (!secondExport.success)
            throw std::runtime_error("Repeated campaign export failed: " + secondExport.errorMessage);
        ProjectPackageImport::ImportRequest importedRequest;
        importedRequest.packagePath = exported.exportPath;
        importedRequest.conflictResolution = ProjectPackageImport::ConflictResolution::CloneAsCopy;
        importedRequest.cloneCampaignName = "Generated transferred session";
        const auto imported = ProjectPackageImport::importCampaignPackage(importedRequest);
        require(imported.success && imported.importedAsCopy, "Completed campaign transfer failed");
        bool found = false;
        for (const auto &copy : CampaignRepository::loadAllCampaigns())
            if (copy.name == importedRequest.cloneCampaignName)
            {
                found = true;
                const auto map = std::find_if(copy.maps.begin(), copy.maps.end(),
                                              [](const auto &value) { return value.name == "Old bridge"; });
                const auto arin = std::find_if(copy.characters.begin(), copy.characters.end(), [](const auto &value) {
                    return std::string(value.nameBuffer) == "Arin";
                });
                require(copy.metadata.id != campaignId && copy.sessions[0].metadata.id != sessionId &&
                            map != copy.maps.end() && copy.encounters[0].mapId == map->metadata.id &&
                            arin != copy.characters.end() && arin->currentHitPoints == 24 &&
                            copy.sessions[0].planEntries.size() == 3 && !copy.localPlayerDisplay.published,
                        "Transferred session lost identities, links, resources or fail-closed output");
            }
        require(found, "Transferred campaign missing");
        std::cout
            << "Production three-scene session, failure retry, combat, break/reload/resume, wrap-up and transfer passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
