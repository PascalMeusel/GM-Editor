#include "map_creator_campaign_boundary_test_support.hpp"

using namespace MapBoundaryTests;
using Probe = MapCampaignBoundaryProbe;
namespace fs = std::filesystem;

namespace
{
void discardAndSameIdReload()
{
    EditorWindowController editor; auto campaign = fixture("same-id ");
    editor.setCurrentCampaign(campaign); auto &map = Probe::map(editor);
    require(map.openMapForEditing(campaign.maps[0].metadata.id), "Cannot open fixture map");
    Probe::edit(map, "DISCARDED PRIVATE MAP");
    require(map.hasUnsavedDraft(), "Fixture edit was not dirty");
    editor.discardUnsavedChanges("discard before main menu");
    require(!map.hasUnsavedDraft() && !map.isOpen(), "Discard retained map RAM draft");
    editor.setCurrentCampaign(reload(campaign.metadata.id));
    require(map.openMapForEditing(campaign.maps[0].metadata.id), "Same-ID reopen failed");
    require(Probe::draft(map).name == campaign.maps[0].name, "Same-ID reopen resurrected discarded edits");
    require(editor.saveCurrentCampaign("save after discard"), "Root save failed after clean reload");
    require(reload(campaign.metadata.id).maps[0].name == campaign.maps[0].name, "Later Ctrl+S persisted discarded map");
}

void switchClearsDraftAndBindings()
{
    EditorWindowController editor; auto a = fixture("A "), b = fixture("B ");
    editor.setCurrentCampaign(a); auto &map = Probe::map(editor);
    map.beginNewMap(); Probe::edit(map, "UNSAVED A ONLY"); const auto oldId = Probe::draft(map).metadata.id;
    Probe::dirtyView(map);
    Probe::transfer(map).importExportPath = "A private package path";
    Probe::transfer(map).hasMapPackagePreview = Probe::transfer(map).confirmMapPackageImport = true;
    Probe::transfer(map).pendingPackageImportTransactionId = "A-transaction-never-owned-by-B";
    auto &integration = Probe::integration(map);
    integration.attachSavedMapToEncounterHandler = [](const EntityId &) { return true; };
    require(bool(integration.reviewSavedMapHandler) && bool(integration.openEncounterPreparationHandler), "Editor callbacks absent");
    editor.setCurrentCampaign(b);
    require(!map.hasUnsavedDraft() && Probe::draft(map).metadata.id != oldId && Probe::cleanView(map), "A draft/gesture survived B load");
    require(Probe::transfer(map).importExportPath.empty() && !Probe::transfer(map).hasMapPackagePreview &&
        !Probe::transfer(map).confirmMapPackageImport && Probe::transfer(map).pendingPackageImportTransactionId.empty(),
        "Package preview or transaction crossed campaign boundary");
    require(!integration.attachSavedMapToEncounterHandler && integration.reviewSavedMapHandler &&
        integration.openSavedMapHandler && integration.attachSavedMapToSceneHandler && integration.openEncounterPreparationHandler,
        "Transient encounter closure survived or permanent callbacks were lost");
    require(editor.saveCurrentCampaign("save B"), "B save failed after replacement");
    require(reload(b.metadata.id).maps.size() == 1 && reload(a.metadata.id).maps.size() == 1,
        "An unsaved A map was inserted into either campaign");
    require(map.openMapForEditing(b.maps[0].metadata.id) && Probe::draft(map).name == b.maps[0].name,
        "B map opened a replacement review of A instead of its own map");
}

void saveContinueAndFailure()
{
    EditorWindowController editor; auto a = fixture("save "), b = fixture("next ");
    editor.setCurrentCampaign(a); auto &map = Probe::map(editor); map.openMapForEditing(a.maps[0].metadata.id);
    Probe::edit(map, "KEPT BY SAVE AND CONTINUE");
    SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
    control.exec("CREATE TRIGGER fail_boundary_save BEFORE INSERT ON maps BEGIN SELECT RAISE(ABORT,'injected save failure'); END;");
    const bool failed = !editor.saveCurrentCampaign("failed leave save");
    control.exec("DROP TRIGGER fail_boundary_save;");
    require(failed && map.hasUnsavedDraft() && Probe::draft(map).name == "KEPT BY SAVE AND CONTINUE",
        "Failed save discarded map draft before a successful leave");
    require(editor.saveCurrentCampaign("save and continue"), "Retry could not save map");
    editor.setCurrentCampaign(b);
    require(reload(a.metadata.id).maps[0].name == "KEPT BY SAVE AND CONTINUE" && !map.hasUnsavedDraft(),
        "Successful Save and Continue lost committed map or dirtied B");
}

void pendingJobsRetireWithoutJoin()
{
    Gate gate;
    EditorWindowController editor; auto a = fixture("workers "), b = fixture("worker destination ");
    GateRelease release{gate};
    editor.setCurrentCampaign(a); auto &map = Probe::map(editor); map.openMapForEditing(a.maps[0].metadata.id);
    Probe::blockedJobs(map, gate);
    require(gate.entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "Worker gate never entered");
    const auto cancel = Probe::generation(map).cancellation;
    editor.discardUnsavedChanges("discard pending workers");
    editor.setCurrentCampaign(b);
    require(!gate.timedOut && !gate.opened && cancel->load() && Probe::pendingRetired(map),
        "Campaign boundary joined/destroyed active futures or failed to cancel");
    require(!map.hasUnsavedDraft() && Probe::cleanView(map), "Retired A worker dirtied B");
    require(map.openMapForEditing(b.maps[0].metadata.id), "Retiring generation blocked loading B map");
    Probe::queue(map); const auto firstQueued = Probe::generation(map).queuedSerial;
    Probe::dispatch(map);
    require(Probe::generation(map).queued && !Probe::generation(map).inProgress && !gate.timedOut && !gate.opened,
        "A new B request was dropped or overwrote/joined A's retiring future");
    Probe::cancel(map);
    require(!Probe::generation(map).queued && !map.hasUnsavedDraft(), "Cancelled queued B request remained dirty");
    Probe::queue(map); const auto nextQueued = Probe::generation(map).queuedSerial;
    require(nextQueued > firstQueued, "Requeued B request reused a cancelled serial");
    map.hide(); editor.setWindowOpacity(false); gate.open();
    // The same maintenance entry point used by ApplicationRun while the main
    // menu is active. No MapCreator/Editor update, rendering or audio call.
    until([&] { editor.pollRetiredMapWork(); return !Probe::pendingRetired(map); });
    require(Probe::draft(map).name == b.maps[0].name && Probe::cleanView(map) &&
        Probe::generation(map).candidates.empty() && Probe::integration(map).statusMessage.find("OLD") == std::string::npos,
        "Late generation/preview/inspection result entered B");
    // Invoke the same non-render dispatch boundary used by MapCreator::update.
    Probe::dispatch(map);
    require(Probe::generation(map).inProgress && !Probe::generation(map).queued &&
        Probe::generation(map).activeSerial == nextQueued && Probe::generation(map).future.valid(),
        "Queued B generation could not start after A retired");
    map.prepareForCampaignReplacement(); map.onCampaignReplaced();
    until([&] { editor.pollRetiredMapWork(); return !Probe::pendingRetired(map); });
}

void exportEpochPreventsSameIdPublication(const fs::path &root)
{
    EditorWindowController editor; auto campaign = fixture("export epoch "); editor.setCurrentCampaign(campaign);
    auto &map = Probe::map(editor); map.openMapForEditing(campaign.maps[0].metadata.id);
    MapPngExportJob job; job.map = Probe::draft(map); job.options.pixelsPerCell = kMapPngMinimumPixelsPerCell;
    job.options.showBackground = false; job.authoringSerial = Probe::editing(map).authoringSerial;
    const auto sizing = calculateMapPngExportSizing(job.map, job.options.pixelsPerCell);
    if (!sizing.valid) throw std::runtime_error("Invalid PNG boundary fixture: " + sizing.error);
    job.destination = root / "previous-good.png"; job.overwrite = true;
    { std::ofstream output(job.destination, std::ios::binary); output << "PREVIOUS_GOOD_OUTPUT"; }
    Probe::startExport(map, job);
    require(Probe::transfer(map).pngExportTask.busy(), "Export fixture did not start");
    editor.setCurrentCampaign(reload(campaign.metadata.id));
    // Deliberately recreate the old ID/serial: epoch is the remaining guard.
    Probe::exportCollision(map, job); map.hide();
    until([&] { editor.pollRetiredMapWork(); return !Probe::transfer(map).pngExportTask.busy(); });
    require(bytes(job.destination) == "PREVIOUS_GOOD_OUTPUT" && Probe::transfer(map).lastExportPath.empty(),
        "Same-ID reload published an old export over the previous output");
    // Positive control: the same immutable input succeeds with current consent.
    Probe::startExport(map, job);
    until([&] { Probe::pollExport(map); return !Probe::transfer(map).pngExportTask.busy(); });
    const auto png = bytes(job.destination);
    if (png.size() <= 8 || png.compare(0, 8, "\x89PNG\r\n\x1a\n", 8) != 0 ||
        Probe::transfer(map).lastExportPath.empty())
        throw std::runtime_error("Current-epoch PNG control did not publish valid output: " + map.draftStatusMessage() +
            "; output bytes=" + std::to_string(png.size()) + "; destination=" + job.destination.u8string() +
            "; published path=" + Probe::transfer(map).lastExportPath);
}

void recoveryOwnershipAndJournal(const fs::path &root)
{
    auto a = fixture("recovery A "), b = fixture("recovery B ");
    ProjectPackageLayout::ensureCampaignPackageLayout(a);
    ProjectPackageLayout::ensureCampaignPackageLayout(b);
    const auto aRoot = ProjectPackageLayout::layoutForCampaign(a).rootDirectory;
    MapCheckpointService checkpoints(aRoot);
    auto recoveredMap = a.maps[0]; recoveredMap.name = "EXPLICIT RESTORE ONLY";
    const auto recoveryCanary = checkpoints.writeCrashJournal(recoveredMap, "retained recovery");
    require(recoveryCanary.success, "Cannot create recovery canary: " + recoveryCanary.error);
    const auto assetRoot = ProjectAssets::campaignAssetDirectory(a, "MapPackages"); fs::create_directories(assetRoot);
    const auto package = root / "boundary-import.mapbundle";
    const auto exported = MapPackageService::exportPackage(recoveredMap, package);
    require(exported.success, "Cannot create generated map package: " + exported.error);
    MapPackageDraftInstallOptions options; options.projectRoot = root; options.assetLibraryDirectory = assetRoot;
    options.deferRecoveryAcknowledgement = true;
    MapModel imported; const auto installed = MapPackageDraftInstallService::installIntoDraft(package, imported, options);
    require(installed.success, "Cannot create pending import recovery: " + installed.error);
    const auto before = files(aRoot);
    EditorWindowController editor; editor.setCurrentCampaign(a); auto &editorMap = Probe::map(editor);
    Probe::transfer(editorMap).pendingPackageImportTransactionId = installed.transactionId;
    Probe::draft(editorMap) = imported; Probe::edit(editorMap, "IMPORTED UNSAVED A");
    editor.discardUnsavedChanges("discard imported draft"); editor.setCurrentCampaign(b);
    require(files(aRoot) == before && !editorMap.hasUnsavedDraft(), "Reset deleted/acknowledged A recovery or reapplied import");
    editor.setCurrentCampaign(reload(a.metadata.id)); editorMap.openMapForEditing(a.maps[0].metadata.id);
    require(Probe::draft(editorMap).name == a.maps[0].name && Probe::history(editorMap).hasRecoveryPreview,
        "Retained recovery was silently applied instead of offered for explicit restore");

    // Keep the journal cancellation fixture independent of pending imports;
    // construction intentionally offers interrupted imports for explicit use.
    auto journalA = fixture("journal A "), journalB = fixture("journal B ");
    ProjectPackageLayout::ensureCampaignPackageLayout(journalA);
    ProjectPackageLayout::ensureCampaignPackageLayout(journalB);
    const auto journalARoot = ProjectPackageLayout::layoutForCampaign(journalA).rootDirectory;
    const auto journalBRoot = ProjectPackageLayout::layoutForCampaign(journalB).rootDirectory;
    auto journalCanary = journalA.maps[0]; journalCanary.name = "PREVIOUS GOOD JOURNAL";
    const auto previousJournal = MapCheckpointService(journalARoot).writeCrashJournal(journalCanary, "previous good");
    require(previousJournal.success, "Cannot create previous journal canary: " + previousJournal.error);
    const auto journalBefore = files(journalARoot);
    Gate gate; MapCreatorController controller(journalA, [&] { gate.wait(); }); GateRelease release{gate};
    require(controller.openMapForEditing(journalA.maps[0].metadata.id) &&
        Probe::draft(controller).metadata.id == journalA.maps[0].metadata.id, "Cannot open gated journal fixture");
    Probe::edit(controller, "LATE JOURNAL MUST NOT PUBLISH");
    Probe::journal(controller);
    require(gate.entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "Journal gate never entered");
    controller.prepareForCampaignReplacement(); journalA = journalB; controller.onCampaignReplaced();
    require(!gate.timedOut && !gate.opened, "Reset joined journal worker");
    gate.open();
    // A later job on the same worker proves the cancelled job passed its
    // publication point before checking both campaign roots.
    Probe::history(controller).crashJournalWorker.submit(journalBRoot, journalB.maps[0], "B journal", 999, false);
    std::optional<MapCrashJournalCompletion> completion;
    until([&] { completion = Probe::history(controller).crashJournalWorker.takeCompletion(); return completion.has_value(); });
    require(completion->success && completion->projectDirectory == journalBRoot &&
        completion->mapId == journalB.maps[0].metadata.id && completion->authoringSerial == 999,
        "Current B journal control failed: " + completion->error);
    const auto publishedB = MapCheckpointService(journalBRoot).previewRecovery(journalB.maps[0]);
    require(publishedB.valid && publishedB.checkpoint.mapId == journalB.maps[0].metadata.id &&
        publishedB.package.mapName == journalB.maps[0].name && publishedB.checkpoint.transactionLabel == "B journal" &&
        publishedB.checkpoint.packageBytes > 0,
        "Current B journal control did not create valid recovery: " + publishedB.error);
    require(files(journalARoot) == journalBefore && !MapCheckpointService(journalBRoot).previewRecovery(journalCanary).valid,
        "Cancelled A journal published into A or the new B root");
}
}

int main()
{
    try
    {
        const auto root = fs::temp_directory_path() / createEntityId("map-campaign-boundary-tests"); fs::create_directories(root);
        environment("TTRPG_DATA_ROOT", root.string()); environment("TTRPG_DB_PATH", (root / "TTRPG.db").string());
        environment("TTRPG_SETTINGS_PATH", (root / "settings.ini").string());
        auto *context = ImGui::CreateContext(); ImGui::GetIO().IniFilename = nullptr;
        gm::localization::LocalizationService::instance().setPreference(gm::localization::LocalePreference::English, false);
        auto &save = ProjectSaveService::getInstance(); save.setBackupSnapshotsEnabled(false); save.setRecoverySnapshotsEnabled(false);
        discardAndSameIdReload(); switchClearsDraftAndBindings(); saveContinueAndFailure(); pendingJobsRetireWithoutJoin();
        exportEpochPreventsSameIdPublication(root); recoveryOwnershipAndJournal(root);
        ImGui::DestroyContext(context);
        std::cout << "PASS map campaign boundary: 6 groups; isolated SQLite/files, no windows/rendering/audio\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
