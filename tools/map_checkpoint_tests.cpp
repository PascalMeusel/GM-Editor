#include "Models/Map/MapCheckpointService.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorFilePathPolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
bool require(bool condition, const std::string &message)
{
    if (condition) return true;
    std::cerr << message << std::endl;
    return false;
}

struct TemporaryDirectory
{
    std::filesystem::path path;
    TemporaryDirectory()
    {
        path = std::filesystem::temp_directory_path() / createEntityId("map-checkpoint-tests");
        std::filesystem::create_directories(path);
    }
    ~TemporaryDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

MapModel makeMap(std::uint64_t revision, const std::string &name)
{
    MapModel map;
    map.metadata.id = "checkpoint-map";
    map.metadata.createdAtUtc = "2026-09-05T07:00:00Z";
    map.metadata.updatedAtUtc = "2026-09-05T07:10:00Z";
    map.metadata.tags = {"world", "important"};
    map.metadata.customProperties = {{"owner", "gm"}, {"mood", "warm"}};
    map.name = name;
    map.description = "A complete editable checkpoint";
    map.creationIntent = MapCreationIntent::RegionExploration;
    map.atlasLevel = MapLevel::Region;
    map.mapKind = MapKind::Geographic;
    map.scope = MapScope::World;
    map.revision = revision;
    map.playerRelease = {true, false, true, false, true};
    map.width = 2;
    map.height = 2;
    map.cells.resize(4);
    map.cells[0].kind = MapTileKind::Forest;
    map.cells[1].kind = MapTileKind::Water;
    map.cells[1].fogged = true;
    map.cells[2].kind = MapTileKind::Mountain;
    map.cells[2].elevation = 7;
    map.cells[3].kind = MapTileKind::Bridge;
    map.cells[3].wayKind = MapWayKind::StoneRoad;
    map.backgroundImagePath = "assets/maps/underlay.png";
    map.backgroundImageOpacity = 0.4f;
    map.importedReference.enabled = true;
    map.importedReference.assetId = "reference-one";
    map.importedReference.projectRelativePath = "assets/maps/reference.png";
    map.importedReference.contentHash = "abc123";

    MapTokenModel token;
    token.metadata.id = "token-one";
    token.name = "Hidden scout";
    token.x = 1.25f;
    token.y = 0.75f;
    token.hiddenFromPlayers = true;
    map.tokens.push_back(token);

    MapGeneratedFeature river;
    river.id = "river-one";
    river.kind = MapGeneratedFeatureKind::River;
    river.name = "Silver Run";
    river.locked = true;
    river.points = {{0.0f, 0.5f}, {2.0f, 1.5f}};
    map.generation.enabled = true;
    map.generation.recipe.seed = 0x12345678u;
    map.generation.recipe.generatorVersion = 5;
    map.generation.recipe.recipeId = "v5-test";
    map.generation.recipe.styleSnapshotId = "fantasy-neutral-v3";
    map.generation.graph.features.push_back(river);
    return map;
}

std::filesystem::path findPackage(const std::filesystem::path &project,
                                  const std::string &checkpointId)
{
    const auto root = mapFileIoPath(project) / ".gm-editor" / "map-checkpoints";
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) return {};
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root, ec))
    {
        if (ec) return {};
        if (entry.is_regular_file() && entry.path().filename() == "map.ttrpgmappkg" &&
            entry.path().parent_path().filename() == checkpointId)
            return entry.path();
    }
    return {};
}

bool testNamedRoundTripAndExplicitRestore()
{
    TemporaryDirectory project;
    MapCheckpointService service(project.path);
    const MapModel original = makeMap(7, "Before edits");
    const MapCheckpointWriteResult written = service.createNamedCheckpoint(original, "Before the river edit");
    bool ok = require(written.success && written.checkpoint.valid &&
                          written.checkpoint.name == "Before the river edit",
                      "Named checkpoint should be written with its label");

    const MapCheckpointListResult listed = service.listNamedCheckpoints(original.metadata.id);
    ok &= require(listed.success && listed.checkpoints.size() == 1 && listed.checkpoints[0].valid &&
                      listed.checkpoints[0].id == written.checkpoint.id,
                  "Named checkpoint should be listed and validated");

    MapModel edited = makeMap(12, "After edits");
    edited.cells[0].kind = MapTileKind::Lava;
    edited.features.push_back(MapFeatureModel{});
    const MapCheckpointRestorePreview preview = service.previewRestore(edited, written.checkpoint.id);
    ok &= require(preview.valid && preview.targetRevision == 12 && preview.package.sourceMapId == original.metadata.id,
                  "Restore preview should expose the immutable source and target revision");

    const MapModel beforeRejectedRestore = edited;
    std::string error;
    ok &= require(!service.applyRestore(preview, edited, false, &error) && !error.empty() &&
                      edited.name == beforeRejectedRestore.name && edited.revision == beforeRejectedRestore.revision,
                  "Restore without confirmation must not mutate the map");
    ok &= require(service.applyRestore(preview, edited, true, &error),
                  "Confirmed restore should apply a valid checkpoint");
    ok &= require(edited.name == original.name && edited.revision == original.revision &&
                      edited.cells.size() == 4 && edited.cells[1].fogged &&
                      edited.tokens.size() == 1 && edited.tokens[0].hiddenFromPlayers &&
                      edited.generation.graph.features.size() == 1 &&
                      edited.playerRelease.politicalBorders &&
                      !edited.playerRelease.waysAndRoutes &&
                      edited.playerRelease.placesAndMarkers &&
                      !edited.playerRelease.labels &&
                      edited.playerRelease.tokensAndAnnotations &&
                      edited.metadata.customProperties == original.metadata.customProperties,
                  "Checkpoint restore should round-trip the full editable MapModel");

    ok &= require(service.removeNamedCheckpoint(original.metadata.id, written.checkpoint.id, &error),
                  "Named checkpoint should be removable");
    ok &= require(service.listNamedCheckpoints(original.metadata.id).checkpoints.empty(),
                  "Removed checkpoint should no longer be listed");
    return ok;
}

bool testDeepProjectCheckpointPaths()
{
    TemporaryDirectory temporary;
    const auto project = temporary.path / std::string(90, 'a') / std::string(90, 'b');
    std::filesystem::create_directories(mapFileIoPath(project));
    MapCheckpointService service(project);
    MapModel map = makeMap(3, "Deep project checkpoint");
    const auto written = service.createNamedCheckpoint(map, "Before editing");
    bool ok = require(written.success, "Deep project checkpoint write failed: " + written.error);
    if (!written.success) return false;
    const auto package = findPackage(project, written.checkpoint.id);
    ok &= require(!package.empty() && package.native().size() > 260,
                  "The long-path regression must exercise a package beyond Windows MAX_PATH");
    const auto listed = service.listNamedCheckpoints(map.metadata.id);
    ok &= require(listed.success && listed.checkpoints.size() == 1 && listed.checkpoints.front().valid,
                  "Deep project checkpoint metadata and package should remain readable");
    map.revision = 4;
    map.name = "Changed";
    const auto preview = service.previewRestore(map, written.checkpoint.id);
    std::string error;
    ok &= require(preview.valid && service.applyRestore(preview, map, true, &error) &&
                      map.name == "Deep project checkpoint",
                  "Deep project checkpoint should restore the complete map: " + error);
    return ok;
}

bool testLimitPrunesOldest()
{
    TemporaryDirectory project;
    MapCheckpointService service(project.path, 3);
    std::vector<MapCheckpointWriteResult> writes;
    for (int index = 0; index < 5; ++index)
    {
        MapModel map = makeMap(static_cast<std::uint64_t>(index + 1), "Revision " + std::to_string(index + 1));
        writes.push_back(service.createNamedCheckpoint(map, "Checkpoint " + std::to_string(index + 1)));
    }
    bool ok = true;
    for (const auto &write : writes) ok &= require(write.success, "Every checkpoint write should succeed");
    const auto listed = service.listNamedCheckpoints("checkpoint-map");
    ok &= require(listed.success && listed.checkpoints.size() == 3,
                  "Retention must keep no more than the configured checkpoint limit");
    std::set<std::string> names;
    for (const auto &checkpoint : listed.checkpoints) names.insert(checkpoint.name);
    ok &= require(names == std::set<std::string>({"Checkpoint 3", "Checkpoint 4", "Checkpoint 5"}),
                  "Retention should prune the oldest named checkpoints first");
    ok &= require(!writes[3].prunedCheckpointIds.empty() && !writes[4].prunedCheckpointIds.empty(),
                  "Pruning should be reported to callers");
    return ok;
}

bool testCorruptionAndTransactionalStaleness()
{
    TemporaryDirectory project;
    MapCheckpointService service(project.path);
    const auto written = service.createNamedCheckpoint(makeMap(4, "Good snapshot"), "Good snapshot");
    bool ok = require(written.success, "Corruption fixture should be created");

    MapModel current = makeMap(10, "Current");
    const MapCheckpointRestorePreview preview = service.previewRestore(current, written.checkpoint.id);
    ok &= require(preview.valid, "Valid checkpoint should produce a restore preview");

    MapModel changedAfterPreview = current;
    changedAfterPreview.revision = 11;
    changedAfterPreview.name = "Changed after preview";
    std::string error;
    ok &= require(!service.applyRestore(preview, changedAfterPreview, true, &error) &&
                      changedAfterPreview.revision == 11 && changedAfterPreview.name == "Changed after preview",
                  "A stale restore preview must not overwrite newer edits");

    const std::filesystem::path package = findPackage(project.path, written.checkpoint.id);
    ok &= require(!package.empty(), "Checkpoint package should be stored below the project");
    {
        std::fstream stream(package, std::ios::binary | std::ios::in | std::ios::out);
        char byte = 0;
        stream.read(&byte, 1);
        byte ^= 0x55;
        stream.seekp(0);
        stream.write(&byte, 1);
    }
    const MapModel beforeCorruptRestore = current;
    ok &= require(!service.applyRestore(preview, current, true, &error) &&
                      current.name == beforeCorruptRestore.name && current.revision == beforeCorruptRestore.revision,
                  "A checkpoint changed after preview must fail transactionally");
    const auto listed = service.listNamedCheckpoints("checkpoint-map");
    ok &= require(listed.success && listed.checkpoints.size() == 1 && !listed.checkpoints[0].valid,
                  "Corrupt checkpoints should remain visible but be marked invalid");
    ok &= require(!service.previewRestore(current, written.checkpoint.id).valid,
                  "Corrupt checkpoints must not produce an applicable preview");
    return ok;
}

bool testTraversalProtection()
{
    TemporaryDirectory project;
    const auto outside = project.path.parent_path() / (project.path.filename().u8string() + "-outside.txt");
    {
        std::ofstream sentinel(outside);
        sentinel << "keep";
    }
    MapCheckpointService service(project.path);
    MapModel malicious = makeMap(1, "Malicious identity");
    malicious.metadata.id = "../../outside-map";
    const auto written = service.createNamedCheckpoint(malicious, "../label is data, not a path");
    bool ok = require(written.success, "Unsafe map IDs must be safely encoded rather than resolved as paths");
    MapModel current = malicious;
    current.revision = 2;
    ok &= require(!service.previewRestore(current, "../../outside").valid,
                  "Traversal checkpoint IDs must be rejected");
    std::string error;
    ok &= require(!service.removeNamedCheckpoint(malicious.metadata.id, "../journal", &error) && !error.empty(),
                  "Traversal removal IDs must be rejected");
    std::ifstream sentinel(outside);
    std::string contents;
    sentinel >> contents;
    ok &= require(contents == "keep", "Traversal attempts must not touch files outside project storage");
    std::error_code ec;
    std::filesystem::remove(outside, ec);
    return ok;
}

bool testCrashJournalRecoveryPreview()
{
    TemporaryDirectory project;
    MapCheckpointService service(project.path);
    MapModel saved = makeMap(20, "Persisted map");
    MapModel afterTransaction = makeMap(21, "Unsaved transaction");
    afterTransaction.cells[0].kind = MapTileKind::Lava;
    const auto journal = service.writeCrashJournal(afterTransaction, "Paint terrain");
    bool ok = require(journal.success && journal.checkpoint.kind == MapCheckpointKind::CrashJournal &&
                          journal.checkpoint.transactionLabel == "Paint terrain",
                      "A completed transaction should create an atomic recovery journal snapshot");

    const auto preview = service.previewRecovery(saved);
    ok &= require(preview.valid && preview.recovery && preview.checkpoint.sourceRevision == 21,
                  "Recovery must first expose a preview of the newer journal snapshot");
    std::string error;
    ok &= require(!service.applyRestore(preview, saved, false, &error) && saved.revision == 20,
                  "Recovery preview must never overwrite without confirmation");
    ok &= require(service.applyRestore(preview, saved, true, &error) && saved.revision == 21 &&
                      saved.name == "Unsaved transaction" && saved.cells[0].kind == MapTileKind::Lava,
                  "Confirmed recovery should transactionally restore the journal snapshot");
    ok &= require(service.discardRecovery(saved.metadata.id, &error),
                  "Recovery journal should only be discarded by an explicit action");
    ok &= require(!service.previewRecovery(saved).valid,
                  "Discarded recovery journal should no longer be offered");
    return ok;
}

bool testFailedPersistentSaveRetainsCrashJournal()
{
    TemporaryDirectory project;
    MapCheckpointService service(project.path);
    MapModel edited = makeMap(31, "Unsaved after failed campaign save");
    const auto journal = service.writeCrashJournal(edited, "Move river control point");
    bool ok = require(journal.success, "Failed-save policy fixture should create a crash journal");

    bool cleanupCalled = false;
    std::string error;
    const MapCreatorRecoveryCleanupOutcome failedSaveOutcome =
        finalizeMapCreatorRecoveryAfterPersistentSave(false, [&]() {
            cleanupCalled = true;
            return service.discardRecovery(edited.metadata.id, &error);
        });
    ok &= require(failedSaveOutcome == MapCreatorRecoveryCleanupOutcome::RetainedBecauseSaveFailed &&
                      !cleanupCalled,
                  "A failed persistent campaign save must not attempt recovery-journal cleanup");
    ok &= require(service.previewRecovery(edited).valid,
                  "The recovery journal must remain available after a failed persistent campaign save");

    const MapCreatorRecoveryCleanupOutcome successfulSaveOutcome =
        finalizeMapCreatorRecoveryAfterPersistentSave(true, [&]() {
            cleanupCalled = true;
            return service.discardRecovery(edited.metadata.id, &error);
        });
    ok &= require(successfulSaveOutcome == MapCreatorRecoveryCleanupOutcome::Cleared && cleanupCalled &&
                      !service.previewRecovery(edited).valid,
                  "A successful persistent campaign save may clear the recovery journal");
    return ok;
}

bool testCrashJournalFlushRetriesWithoutLosingNewestRequest()
{
    bool pending = true;
    std::string label = "Move river control point";
    int writes = 0;
    const MapCreatorCrashJournalFlushOutcome failed =
        flushMapCreatorCrashJournalRequest(pending, label, [&](const std::string &writtenLabel) {
            ++writes;
            return writtenLabel == "Move river control point" && writes > 1;
        });
    bool ok = require(
        failed == MapCreatorCrashJournalFlushOutcome::RetryRequired &&
            pending && label == "Move river control point" && writes == 1,
        "A failed journal write must retain its pending request and transaction label");

    // A newer edit may arrive before the next safe flush. The retained slot
    // must carry that newest label instead of replaying or clearing the older
    // failed request.
    label = "Latest terrain edit";
    const MapCreatorCrashJournalFlushOutcome retried =
        flushMapCreatorCrashJournalRequest(pending, label, [&](const std::string &writtenLabel) {
            ++writes;
            return writtenLabel == "Latest terrain edit";
        });
    ok &= require(
        retried == MapCreatorCrashJournalFlushOutcome::Written &&
            !pending && label.empty() && writes == 2,
        "A retained journal request must clear only after a successful retry");

    pending = true;
    label = "Latest terrain edit";
    try
    {
        (void)flushMapCreatorCrashJournalRequest(
            pending, label, [](const std::string &) -> bool {
                throw std::runtime_error("injected journal failure");
            });
        ok &= require(false, "Injected journal exception did not propagate to the status boundary");
    }
    catch (const std::runtime_error &)
    {
        ok &= require(pending && label == "Latest terrain edit",
                      "A journal exception discarded the newest pending request");
    }
    return ok;
}

bool testMapCreatorSharingMergeAndSaveRollback()
{
    MapModel original;
    original.metadata.id = "sharing-merge-map";
    original.name = "Original terrain";
    original.width = 2;
    original.height = 2;
    resizeMapCells(original);
    original.visibility = MapVisibility::PlayersVisible;
    original.playerRelease = {true, true, true, true, true};
    auto baseline = captureMapCreatorPlayerSettings(original);
    MapModel draft = original;
    draft.name = "Edited terrain";
    draft.cells.front().kind = MapTileKind::Forest;
    draft.description = "Unsaved authoring notes";

    Campaign campaign;
    campaign.maps.push_back(original);
    campaign.maps.front().visibility = MapVisibility::GameMasterOnly;
    campaign.maps.front().playerRelease = {};
    campaign.partyLocation.mapId = original.metadata.id;
    campaign.partyLocation.x = 1;
    campaign.worldPlaces.emplace_back();
    campaign.worldPlaces.front().name = "Committed place";
    int editingIndex = 0;
    int selectedIndex = 0;
    const auto merged = mergeMapCreatorPlayerSettings(baseline, draft, campaign.maps.front());
    bool ok = require(merged.valid && merged.visibility == MapVisibility::GameMasterOnly,
                      "Saving an older map draft must preserve an external whole-map revocation");
    for (int category = 0; category < 5; ++category)
        ok &= require(!mapPlayerReleaseCategoryEnabled(merged.release, static_cast<MapPlayerReleaseCategory>(category)),
                      "Saving terrain must preserve every externally revoked player category");

    {
        MapCreatorCampaignSaveTransaction<Campaign> transaction(campaign, draft, editingIndex, selectedIndex);
        campaign.maps.front() = draft;
        applyMapCreatorPlayerSettings(campaign.maps.front(), merged);
        campaign.worldPlaces.front().name = "Uncommitted world-brief mutation";
        campaign.partyLocation.x = 9;
        ++draft.revision;
        editingIndex = 7;
        selectedIndex = 8;
        const bool saved = transaction.finish(false);
        ok &= require(!finalizeMapCreatorPlayerSettingsSave(saved, merged, draft, baseline),
                      "A failed persistence result must not advance draft sharing or its baseline");
    }
    ok &= require(campaign.maps.front().name == original.name &&
                      campaign.maps.front().cells.front().kind == original.cells.front().kind &&
                      campaign.maps.front().visibility == MapVisibility::GameMasterOnly &&
                      !campaign.maps.front().playerRelease.labels &&
                      campaign.worldPlaces.front().name == "Committed place" &&
                      campaign.partyLocation.x == 1 && editingIndex == 0 && selectedIndex == 0,
                  "Failed map save must restore canonical map, world links, party location and selections");
    ok &= require(draft.name == "Edited terrain" && draft.description == "Unsaved authoring notes" &&
                      draft.cells.front().kind == MapTileKind::Forest && draft.revision == original.revision &&
                      draft.visibility == original.visibility && draft.playerRelease.labels &&
                      baseline.visibility == original.visibility && baseline.release.labels,
                  "Failed map save must retain the edited draft and its last committed sharing baseline");

    const auto retryMerge = mergeMapCreatorPlayerSettings(baseline, draft, campaign.maps.front());
    {
        MapCreatorCampaignSaveTransaction<Campaign> transaction(campaign, draft, editingIndex, selectedIndex);
        campaign.maps.front() = draft;
        applyMapCreatorPlayerSettings(campaign.maps.front(), retryMerge);
        const bool saved = transaction.finish(true);
        ok &= require(finalizeMapCreatorPlayerSettingsSave(saved, retryMerge, draft, baseline),
                      "Retry must commit the same external revocation with the retained authoring draft");
    }
    ok &= require(campaign.maps.front().name == "Edited terrain" &&
                      campaign.maps.front().cells.front().kind == MapTileKind::Forest &&
                      draft.description == "Unsaved authoring notes" &&
                      draft.visibility == MapVisibility::GameMasterOnly && !draft.playerRelease.labels &&
                      baseline.visibility == draft.visibility && !baseline.release.labels,
                  "Successful save must retain geometry and notes while advancing only committed sharing state");
    MapModel oldUndoSnapshot = original;
    applyMapCreatorPlayerSettings(oldUndoSnapshot, baseline);
    ok &= require(oldUndoSnapshot.name == original.name &&
                      oldUndoSnapshot.cells.front().kind == original.cells.front().kind &&
                      oldUndoSnapshot.visibility == MapVisibility::GameMasterOnly && !oldUndoSnapshot.playerRelease.labels,
                  "Rebasing sharing in undo history must preserve geometry while preventing permission resurrection");

    MapModel authored = original;
    authored.playerRelease.labels = false;
    MapModel externallyEdited = original;
    externallyEdited.playerRelease.waysAndRoutes = false;
    const auto independent = mergeMapCreatorPlayerSettings(captureMapCreatorPlayerSettings(original), authored, externallyEdited);
    ok &= require(independent.valid && !independent.release.labels && !independent.release.waysAndRoutes &&
                      independent.release.placesAndMarkers,
                  "Independent authored and external sharing changes must merge field by field");
    MapModel unreleased = original;
    unreleased.playerRelease = {};
    MapModel newlyReleased = unreleased;
    newlyReleased.playerRelease.labels = true;
    newlyReleased.playerRelease.tokensAndAnnotations = true;
    const auto newGrant = mergeMapCreatorPlayerSettings(captureMapCreatorPlayerSettings(unreleased), unreleased, newlyReleased);
    ok &= require(newGrant.valid && newGrant.release.labels && newGrant.release.tokensAndAnnotations &&
                      !newGrant.release.waysAndRoutes,
                  "Saving an unchanged sharing draft must preserve newly granted external categories as well as revocations");
    original.visibility = MapVisibility::GameMasterOnly;
    authored.visibility = MapVisibility::PlayersKnown;
    externallyEdited.visibility = MapVisibility::PlayersVisible;
    ok &= require(!mergeMapCreatorPlayerSettings(captureMapCreatorPlayerSettings(original), authored, externallyEdited).valid &&
                      !mergeMapCreatorPlayerSettings({}, authored, externallyEdited).valid,
                  "Conflicting visibility decisions or an unknown baseline must require reload instead of overwriting");

    try
    {
        MapCreatorCampaignSaveTransaction<Campaign> transaction(campaign, draft, editingIndex, selectedIndex);
        campaign.maps.clear();
        draft.description = "Interrupted normalization";
        throw std::runtime_error("injected map save exception");
    }
    catch (const std::runtime_error &)
    {
        ok &= require(campaign.maps.size() == 1 && draft.description == "Unsaved authoring notes",
                      "An exception before save completion must roll back canonical state and retain the editable draft");
    }
    Campaign emptyCampaign;
    int newEditingIndex = -1;
    int newSelectedIndex = -1;
    {
        MapCreatorCampaignSaveTransaction<Campaign> transaction(emptyCampaign, draft, newEditingIndex, newSelectedIndex);
        emptyCampaign.maps.push_back(draft);
        emptyCampaign.partyLocation.mapId = draft.metadata.id;
        newEditingIndex = 0;
        newSelectedIndex = 0;
        transaction.finish(false);
    }
    ok &= require(emptyCampaign.maps.empty() && emptyCampaign.partyLocation.mapId.empty() &&
                      newEditingIndex == -1 && newSelectedIndex == -1 && draft.name == "Edited terrain",
                  "A failed first save must leave no canonical map or party reference and keep the new draft retryable");
    return ok;
}

bool testMapCreatorImportHistoryRetainsSharingBaselines()
{
    MapModel mapA;
    mapA.metadata.id = "history-map-a";
    mapA.width = 2;
    mapA.height = 2;
    resizeMapCells(mapA);
    mapA.visibility = MapVisibility::PlayersVisible;
    mapA.playerRelease = {true, true, true, true, true};
    std::vector<MapModel> committedMaps{mapA};
    MapModel draft = mapA;
    draft.description = "Retain A's unsaved notes across import";
    draft.cells.front().kind = MapTileKind::Forest;
    auto baseline = captureMapCreatorPlayerSettings(mapA);
    MapModel undoMapA = draft;
    auto undoBaselineA = baseline;
    int editingIndex = 0;
    int selectedIndex = 0;
    int selectedParentIndex = 0;

    // A package import creates an independent identity; its first successful
    // save replaces the controller's active baseline with B's baseline.
    MapModel importedB = mapA;
    importedB.metadata.id = "history-import-b";
    importedB.description = "Imported B";
    importedB.cells.front().kind = MapTileKind::Water;
    draft = importedB;
    editingIndex = -1;
    selectedIndex = -1;
    committedMaps.push_back(draft);
    bool ok = require(finalizeMapCreatorPlayerSettingsSave(
                          true, captureMapCreatorPlayerSettings(draft), draft, baseline),
                      "First save of an independent imported draft must capture its own sharing baseline");
    MapModel redoMapB = draft;
    auto redoBaselineB = baseline;
    ok &= require(!finalizeMapCreatorPlayerSettingsSave(true, baseline, undoMapA, undoBaselineA) &&
                      undoBaselineA.mapId == mapA.metadata.id && undoBaselineA.release.labels,
                  "Saving imported B must not replace the sharing baseline of A's undo snapshot");

    // The presentation panel revokes A while B is open, and campaign ordering
    // changes. Undo must use A's original baseline, not fresh revoked values.
    committedMaps.front().visibility = MapVisibility::GameMasterOnly;
    committedMaps.front().playerRelease = {};
    std::swap(committedMaps.front(), committedMaps.back());
    restoreMapCreatorDraftHistory(undoMapA, undoBaselineA, committedMaps,
                                  draft, baseline, editingIndex, selectedIndex, {}, selectedParentIndex);
    ok &= require(draft.metadata.id == mapA.metadata.id && editingIndex == 1 && selectedIndex == 1 &&
                      draft.cells.front().kind == MapTileKind::Forest &&
                      draft.description == undoMapA.description && baseline.mapId == mapA.metadata.id,
                  "Undoing a saved import must restore A's authored content, baseline and current identity-based indices");
    const auto mergedA = mergeMapCreatorPlayerSettings(baseline, draft, committedMaps[editingIndex]);
    ok &= require(mergedA.valid && mergedA.visibility == MapVisibility::GameMasterOnly &&
                      !mergedA.release.politicalBorders && !mergedA.release.waysAndRoutes &&
                      !mergedA.release.placesAndMarkers && !mergedA.release.labels && !mergedA.release.tokensAndAnnotations,
                  "Saving A after undoing a saved import must retain external whole-map and category revocations");
    ok &= require(!finalizeMapCreatorPlayerSettingsSave(false, mergedA, draft, baseline) && baseline.release.labels,
                  "A failed save after cross-map undo must retain A's retry baseline");
    committedMaps[editingIndex] = draft;
    applyMapCreatorPlayerSettings(committedMaps[editingIndex], mergedA);
    finalizeMapCreatorPlayerSettingsSave(true, mergedA, draft, baseline);
    finalizeMapCreatorPlayerSettingsSave(true, mergedA, undoMapA, undoBaselineA);
    ok &= require(!undoMapA.playerRelease.labels && !undoBaselineA.release.labels &&
                      undoMapA.cells.front().kind == MapTileKind::Forest && redoBaselineB.release.labels,
                  "Successful A save must rebase A history without changing geometry or B's independent baseline");

    // A later permission decision must survive an ordinary, rebased A undo.
    committedMaps[editingIndex].playerRelease.waysAndRoutes = true;
    restoreMapCreatorDraftHistory(undoMapA, undoBaselineA, committedMaps,
                                  draft, baseline, editingIndex, selectedIndex, {}, selectedParentIndex);
    const auto withinMapMerge = mergeMapCreatorPlayerSettings(baseline, draft, committedMaps[editingIndex]);
    ok &= require(withinMapMerge.valid && withinMapMerge.release.waysAndRoutes && !withinMapMerge.release.labels,
                  "Within-map undo after a successful save must retain its rebased baseline and later sharing decisions");

    committedMaps.front().visibility = MapVisibility::PlayersKnown;
    committedMaps.front().playerRelease.labels = false;
    committedMaps.front().playerRelease.tokensAndAnnotations = false;
    restoreMapCreatorDraftHistory(redoMapB, redoBaselineB, committedMaps,
                                  draft, baseline, editingIndex, selectedIndex, {}, selectedParentIndex);
    const auto mergedB = mergeMapCreatorPlayerSettings(baseline, draft, committedMaps[editingIndex]);
    ok &= require(draft.metadata.id == importedB.metadata.id && draft.description == "Imported B" &&
                      draft.cells.front().kind == MapTileKind::Water && editingIndex == 0 && selectedIndex == 0 &&
                      mergedB.valid && mergedB.visibility == MapVisibility::PlayersKnown &&
                      !mergedB.release.labels && !mergedB.release.tokensAndAnnotations && mergedB.release.waysAndRoutes,
                  "Redoing imported B must restore B's indices and baseline while retaining its later external revocations");

    std::vector<MapModel> noCommittedMaps;
    restoreMapCreatorDraftHistory(undoMapA, undoBaselineA, noCommittedMaps,
                                  draft, baseline, editingIndex, selectedIndex, {}, selectedParentIndex);
    ok &= require(editingIndex == -1 && selectedIndex == -1 && baseline.valid && baseline.mapId == mapA.metadata.id,
                  "History of a deleted saved map must retain proof it was previously saved rather than becoming a new draft");
    restoreMapCreatorDraftHistory(importedB, undoBaselineA, noCommittedMaps,
                                  draft, baseline, editingIndex, selectedIndex, {}, selectedParentIndex);
    ok &= require(!baseline.valid && editingIndex == -1 && selectedIndex == -1,
                  "An unsaved independent import must not inherit the unrelated source map's sharing baseline");
    return ok;
}

bool testMapCreatorHistoryRestoresParentSelectionByIdentity()
{
    MapModel parent;
    parent.metadata.id = "history-parent-p";
    MapModel child;
    child.metadata.id = "history-child-a";
    child.parentMapId = "stale-legacy-parent";
    child.atlasPlacement.parentId = parent.metadata.id;
    child.atlasPlacement.parentRect = {0.2f, 0.3f, 0.4f, 0.5f};
    child.atlasStatus = MapAtlasStatus::Linked;
    MapModel imported;
    imported.metadata.id = "history-import-b";
    const auto childBaseline = captureMapCreatorPlayerSettings(child);
    auto baseline = captureMapCreatorPlayerSettings(imported);
    MapModel draft = imported;
    // Parent P used to occupy the first picker entry. Import B has since been
    // saved and the campaign reordered from [P,A,B] to [B,A,P].
    std::vector<MapModel> committedMaps{imported, child, parent};
    int editingIndex = 0;
    int selectedIndex = 0;
    int selectedParentIndex = 1;
    restoreMapCreatorDraftHistory(child, childBaseline, committedMaps,
                                  draft, baseline, editingIndex, selectedIndex,
                                  child.atlasPlacement.parentId, selectedParentIndex);
    bool ok = require(editingIndex == 1 && selectedIndex == 1 && selectedParentIndex == 3 &&
                          committedMaps[static_cast<std::size_t>(selectedParentIndex - 1)].metadata.id == parent.metadata.id &&
                          draft.atlasPlacement.parentId == parent.metadata.id &&
                          draft.atlasPlacement.parentRect.x == 0.2f && draft.atlasPlacement.parentRect.width == 0.4f &&
                          mapCreatorParentSelectionIsResolved(selectedParentIndex, committedMaps.size()),
                      "Undo must resolve the effective placement parent by identity after import and campaign reordering");

    committedMaps.pop_back();
    restoreMapCreatorDraftHistory(child, childBaseline, committedMaps,
                                  draft, baseline, editingIndex, selectedIndex,
                                  child.atlasPlacement.parentId, selectedParentIndex);
    ok &= require(selectedParentIndex == -1 &&
                      !mapCreatorParentSelectionIsResolved(selectedParentIndex, committedMaps.size()) &&
                      draft.atlasPlacement.parentId == parent.metadata.id && draft.parentMapId == "stale-legacy-parent" &&
                      draft.atlasPlacement.parentRect.width == 0.4f && draft.atlasStatus == MapAtlasStatus::Linked,
                  "A missing restored parent must block save while retaining the link and placement, not relink to B or detach silently");
    selectedParentIndex = 0;
    ok &= require(mapCreatorParentSelectionIsResolved(selectedParentIndex, committedMaps.size()),
                  "Explicitly choosing No parent map must resolve the missing-parent save blocker");

    restoreMapCreatorDraftHistory(imported, captureMapCreatorPlayerSettings(imported), committedMaps,
                                  draft, baseline, editingIndex, selectedIndex, {}, selectedParentIndex);
    ok &= require(editingIndex == 0 && selectedIndex == 0 && selectedParentIndex == 0 &&
                      mapCreatorParentSelectionIsResolved(selectedParentIndex, committedMaps.size()),
                  "Redoing a standalone imported map must clear the previous map's parent picker selection");
    return ok;
}

bool testMapCreatorStepFiveRequiresCurrentPersistentSave()
{
    MapCreatorPersistentSaveReceipt receipt;
    bool ok = require(
        mapCreatorStepFivePrimaryAction(false) ==
            MapCreatorStepFivePrimaryAction::SaveMap,
        "Step five must lead with Save while no durable receipt exists");
    ok &= require(!mapCreatorPersistentSaveIsCurrent(
                      receipt, "map_a", 7, 3, true),
                  "An in-memory map without a successful commit must not be reviewable");

    receipt = {true, "map_a", 7, 3};
    ok &= require(mapCreatorPersistentSaveIsCurrent(
                      receipt, "map_a", 7, 3, true),
                  "A matching durable save receipt should permit player review");
    ok &= require(mapCreatorStepFivePrimaryAction(true) ==
                      MapCreatorStepFivePrimaryAction::ReviewPlayerView,
                  "A current durable save should change the primary action to player review");
    ok &= require(!mapCreatorPersistentSaveIsCurrent(
                      receipt, "map_a", 8, 3, true) &&
                      !mapCreatorPersistentSaveIsCurrent(
                          receipt, "map_a", 7, 4, true),
                  "A revision or authoring change after save must block review");
    ok &= require(!mapCreatorPersistentSaveIsCurrent(
                      receipt, "map_b", 7, 3, true) &&
                      !mapCreatorPersistentSaveIsCurrent(
                          receipt, "map_a", 7, 3, false),
                  "A different map or unapplied atlas-parent choice must block review");
    receipt.committed = false;
    ok &= require(!mapCreatorPersistentSaveIsCurrent(
                      receipt, "map_a", 7, 3, true),
                  "A failed re-save must revoke an older review receipt");

    LocalPlayerDisplayModel display;
    display.enabled = false;
    display.blackout = false;
    display.published = true;
    display.publishedPayloadFingerprint = 1234;
    display.publishedReviewFingerprint = 5678;
    display.showMap = false;
    ok &= require(stageMapCreatorPlayerReviewSelection(display, "map_a") &&
                      display.publicMapId == "map_a" && display.showMap &&
                      !display.enabled && display.blackout && !display.published &&
                      display.publishedPayloadFingerprint == 0 &&
                      display.publishedReviewFingerprint == 0,
                  "Staging from MapCreator must remain disabled, force blackout, and revoke every publication proof");
    const LocalPlayerDisplayModel staged = display;
    ok &= require(!stageMapCreatorPlayerReviewSelection(display, "") &&
                      display.publicMapId == staged.publicMapId &&
                      display.blackout == staged.blackout &&
                      display.enabled == staged.enabled,
                  "An empty map selection must fail without changing Player Display state");
    ok &= require(mapCreatorMapPackageCanImport(true, true) &&
                      !mapCreatorMapPackageCanImport(true, false) &&
                      !mapCreatorMapPackageCanImport(false, true) &&
                      !mapCreatorMapPackageCanImport(false, false),
                  "Map-package apply must require both structural validity and import eligibility");
    return ok;
}

bool testMapCreatorConcurrentFogChanges()
{
    MapModel original;
    original.width = 6;
    original.height = 4;
    original.visibility = MapVisibility::PlayersVisible;
    resizeMapCells(original);
    const auto baseline = captureMapCreatorPlayerSettings(original);
    MapModel draft = original;
    MapModel current = original;
    draft.cells[0].kind = MapTileKind::Forest;
    draft.cells[2].fogged = true;
    current.cells[1].fogged = true;
    const auto merged = mergeMapCreatorPlayerSettings(baseline, draft, current);
    bool ok = require(merged.valid && applyMapCreatorPlayerSettings(draft, merged) &&
                          draft.cells[0].kind == MapTileKind::Forest && draft.cells[1].fogged && draft.cells[2].fogged,
                      "Saving terrain must retain independent fog edits from both the editor and live presentation");
    const auto preview = makeMapCreatorPlayerOutputSnapshot(original, baseline, {current});
    ok &= require(preview.cells[1].fogged && !original.cells[1].fogged,
                  "A stale player preview/export must honor newly committed fog without changing its authoring draft");

    original.cells[0].fogged = true;
    const auto hiddenBaseline = captureMapCreatorPlayerSettings(original);
    current = original;
    current.cells[0].fogged = false;
    const auto revealed = mergeMapCreatorPlayerSettings(hiddenBaseline, original, current);
    ok &= require(revealed.valid && !revealed.fog[0],
                  "An unchanged authoring draft must retain a live reveal on save");

    draft = original;
    draft.width = 8;
    resizeMapCells(draft);
    ok &= require(!mergeMapCreatorPlayerSettings(hiddenBaseline, draft, current).valid,
                  "A resized design must not silently overwrite concurrent fog changes with unrelated coordinates");
    ok &= require(mergeMapCreatorPlayerSettings(hiddenBaseline, draft, original).valid,
                  "Resizing a map remains valid when its live fog has not changed");
    const auto resizedPreview = makeMapCreatorPlayerOutputSnapshot(draft, hiddenBaseline, {current});
    ok &= require(std::all_of(resizedPreview.cells.begin(), resizedPreview.cells.end(),
                              [](const MapCellModel &cell) { return cell.fogged; }),
                  "A player export of mismatched draft geometry must fail closed behind fog");
    return ok;
}

bool testMapCreatorIntentAndManualPathPolicies()
{
    bool ok = require(mapCreatorNormalCreationIntentCount == 6 &&
                          mapCreatorCreationIntentIsNormal(MapCreationIntent::WorldOverview) &&
                          mapCreatorCreationIntentIsNormal(MapCreationIntent::Cave) &&
                          !mapCreatorCreationIntentIsNormal(MapCreationIntent::TacticalEncounter) &&
                          !mapCreatorCreationIntentIsNormal(MapCreationIntent::ImportedReference),
                      "The normal map-type picker must not offer direct tactical creation");
    const std::string unicodePath = u8"München_日本/map";
    const std::filesystem::path nativePath =
        mapCreatorFilesystemPathFromUtf8(unicodePath);
    ok &= require(mapCreatorFilesystemPathToUtf8(nativePath) == unicodePath,
                  "Manual path state did not survive its UTF-8 filesystem round trip");
    const std::filesystem::path withExtension =
        mapCreatorPathWithDefaultExtension(nativePath, ".ttrpgmappkg");
    ok &= require(mapCreatorFilesystemPathToUtf8(withExtension).find(
                      ".ttrpgmappkg") != std::string::npos,
                  "Manual package path did not receive its missing default extension");
    ok &= require(!mapCreatorManualPathIsReady("") &&
                      mapCreatorManualPathIsReady(unicodePath) &&
                      mapCreatorManualExportMayWrite(true, false, false) &&
                      !mapCreatorManualExportMayWrite(true, true, false) &&
                      mapCreatorManualExportMayWrite(true, true, true),
                  "Manual export fallback does not preserve empty-path and overwrite safety gates");
    return ok;
}
}

int main()
{
    bool ok = true;
    ok &= testNamedRoundTripAndExplicitRestore();
    try
    {
        ok &= testDeepProjectCheckpointPaths();
    }
    catch (const std::exception &error)
    {
        std::cerr << "Deep checkpoint exception: " << error.what() << std::endl;
        ok = false;
    }
    ok &= testLimitPrunesOldest();
    ok &= testCorruptionAndTransactionalStaleness();
    ok &= testTraversalProtection();
    ok &= testCrashJournalRecoveryPreview();
    ok &= testFailedPersistentSaveRetainsCrashJournal();
    ok &= testCrashJournalFlushRetriesWithoutLosingNewestRequest();
    ok &= testMapCreatorStepFiveRequiresCurrentPersistentSave();
    ok &= testMapCreatorSharingMergeAndSaveRollback();
    ok &= testMapCreatorImportHistoryRetainsSharingBaselines();
    ok &= testMapCreatorHistoryRestoresParentSelectionByIdentity();
    ok &= testMapCreatorIntentAndManualPathPolicies();
    ok &= testMapCreatorConcurrentFogChanges();
    if (ok) std::cout << "Map checkpoint tests passed" << std::endl;
    return ok ? 0 : 1;
}
