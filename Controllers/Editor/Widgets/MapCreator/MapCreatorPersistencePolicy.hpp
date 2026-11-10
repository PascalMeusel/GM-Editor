#pragma once

#include "Models/Session/LocalPlayerDisplayModel.hpp"
#include "Models/Map/MapModels.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Identity and user annotations belong to the current draft. These two
// generated cave annotations describe its replacement geometry and must not
// be lost when a preview is accepted, or retained after another design replaces it.
inline void retainMapAuthoredMetadata(MapModel &generated, const MapModel &previous)
{
    const auto generatedProperties = generated.metadata.customProperties;
    generated.metadata = previous.metadata;
    for (const char *key : {"cave.surface_entrances", "cave.composition_revision"})
    {
        generated.metadata.customProperties.erase(key);
        const auto found = generatedProperties.find(key);
        if (found != generatedProperties.end())
            generated.metadata.customProperties[key] = found->second;
    }
}

enum class MapCreatorRecoveryCleanupOutcome
{
    RetainedBecauseSaveFailed,
    Cleared,
    ClearFailed
};

enum class MapCreatorCrashJournalFlushOutcome
{
    NothingPending,
    Written,
    RetryRequired
};

// Sharing is also edited by the presentation panel. Keep the last loaded or
// successfully committed values separate from the authoring draft so saving
// terrain cannot restore an older grant or revoke a newer one.
struct MapCreatorPlayerSettingsBaseline
{
    bool valid = false;
    EntityId mapId;
    MapVisibility visibility = MapVisibility::GameMasterOnly;
    MapPlayerReleaseProfile release;
    int width = 0;
    int height = 0;
    std::vector<bool> fog;
};

#include "Controllers/Editor/Widgets/MapCreator/MapCreatorCampaignSaveTransaction.hpp"

inline MapCreatorPlayerSettingsBaseline captureMapCreatorPlayerSettings(const MapModel &map)
{
    MapCreatorPlayerSettingsBaseline result{true, map.metadata.id, map.visibility, map.playerRelease};
    result.width = map.width;
    result.height = map.height;
    result.fog.reserve(map.cells.size());
    for (const auto &cell : map.cells) result.fog.push_back(cell.fogged);
    return result;
}

inline MapCreatorPlayerSettingsBaseline mergeMapCreatorPlayerSettings(
    const MapCreatorPlayerSettingsBaseline &baseline,
    const MapModel &draft,
    const MapModel &current)
{
    if (!baseline.valid || baseline.mapId != draft.metadata.id ||
        current.metadata.id != draft.metadata.id)
        return {};
    auto merged = captureMapCreatorPlayerSettings(draft);
    const auto mergeField = [](auto original, auto authored, auto latest, auto &result) {
        if (authored == original)
            result = latest;
        else if (latest == original || authored == latest)
            result = authored;
        else
            return false;
        return true;
    };
    if (!mergeField(baseline.visibility, draft.visibility, current.visibility, merged.visibility))
        return {};
    for (auto field : {&MapPlayerReleaseProfile::politicalBorders,
                       &MapPlayerReleaseProfile::waysAndRoutes,
                       &MapPlayerReleaseProfile::placesAndMarkers,
                       &MapPlayerReleaseProfile::labels,
                       &MapPlayerReleaseProfile::tokensAndAnnotations})
        if (!mergeField(baseline.release.*field, draft.playerRelease.*field,
                        current.playerRelease.*field, merged.release.*field))
            return {};
    // Atlas and the presentation panel can change fog while this draft is
    // open. Merge each cell against its loaded value, never overwrite a live
    // reveal/hide merely because the GM saved unrelated terrain or notes.
    if (current.width != baseline.width || current.height != baseline.height ||
        current.cells.size() != baseline.fog.size())
        return {};
    const bool sameGeometry = draft.width == baseline.width && draft.height == baseline.height &&
                              draft.cells.size() == baseline.fog.size();
    for (std::size_t i = 0; i < baseline.fog.size(); ++i)
    {
        if (!sameGeometry)
        {
            // A replacement design is allowed if nobody changed live fog.
            // Otherwise there is no safe correspondence between the cells.
            if (current.cells[i].fogged != baseline.fog[i]) return {};
        }
        else
        {
            bool fogged = draft.cells[i].fogged;
            if (!mergeField(bool(baseline.fog[i]), draft.cells[i].fogged, current.cells[i].fogged, fogged))
                return {};
            merged.fog[i] = fogged;
        }
    }
    return merged;
}

inline bool applyMapCreatorPlayerSettings(MapModel &map,
                                          const MapCreatorPlayerSettingsBaseline &settings)
{
    if (!settings.valid || settings.mapId != map.metadata.id)
        return false;
    map.visibility = settings.visibility;
    map.playerRelease = settings.release;
    if (map.width == settings.width && map.height == settings.height && map.cells.size() == settings.fog.size())
        for (std::size_t i = 0; i < map.cells.size(); ++i) map.cells[i].fogged = settings.fog[i];
    return true;
}

inline bool finalizeMapCreatorPlayerSettingsSave(
    bool campaignSaved,
    const MapCreatorPlayerSettingsBaseline &committed,
    MapModel &draft,
    MapCreatorPlayerSettingsBaseline &baseline)
{
    if (!campaignSaved || !applyMapCreatorPlayerSettings(draft, committed))
        return false;
    baseline = committed;
    return true;
}

// Preview/export use authored content with current committed sharing. A field
// unchanged from the draft's baseline follows later presentation decisions;
// an intentional draft restriction can only narrow them. Unsaved grants never
// broaden canonical consent, and an unknown/deleted map stays private.
inline MapModel makeMapCreatorPlayerOutputSnapshot(
    const MapModel &draft,
    const MapCreatorPlayerSettingsBaseline &baseline,
    const std::vector<MapModel> &committedMaps)
{
    MapModel output = draft;
    output.visibility = MapVisibility::GameMasterOnly;
    output.playerRelease = {};
    const auto current = std::find_if(committedMaps.begin(), committedMaps.end(),
                                      [&](const MapModel &map) { return map.metadata.id == draft.metadata.id; });
    if (current == committedMaps.end())
        return output;

    // Preview and player exports honor live fog even before a stale clean
    // editor is reopened. Unsaved edits may hide more, never reveal more.
    if (output.width != current->width || output.height != current->height ||
        output.cells.size() != current->cells.size())
    {
        for (auto &cell : output.cells) cell.fogged = true;
    }
    else
        for (std::size_t i = 0; i < output.cells.size(); ++i)
            output.cells[i].fogged = output.cells[i].fogged || current->cells[i].fogged;

    output.visibility = current->visibility;
    output.playerRelease = current->playerRelease;
    const bool knownBaseline = baseline.valid && baseline.mapId == draft.metadata.id;
    if (!knownBaseline || draft.visibility != baseline.visibility)
        output.visibility = static_cast<MapVisibility>(std::min(
            static_cast<int>(output.visibility), static_cast<int>(draft.visibility)));
    for (auto field : {&MapPlayerReleaseProfile::politicalBorders,
                       &MapPlayerReleaseProfile::waysAndRoutes,
                       &MapPlayerReleaseProfile::placesAndMarkers,
                       &MapPlayerReleaseProfile::labels,
                       &MapPlayerReleaseProfile::tokensAndAnnotations})
        if (!knownBaseline || draft.playerRelease.*field != baseline.release.*field)
            output.playerRelease.*field = output.playerRelease.*field && draft.playerRelease.*field;
    return output;
}

// Import is undoable even after its independent copy has been saved. History
// therefore carries the sharing baseline of its own map, not the most recent
// map visited by the editor. Resolve selections by identity because campaign
// maps may have been inserted or reordered while this snapshot was in history.
inline void restoreMapCreatorDraftHistory(
    const MapModel &historyMap,
    const MapCreatorPlayerSettingsBaseline &historyBaseline,
    const std::vector<MapModel> &committedMaps,
    MapModel &draft,
    MapCreatorPlayerSettingsBaseline &baseline,
    int &editingIndex,
    int &selectedIndex,
    const EntityId &effectiveParentId,
    int &selectedParentIndex)
{
    draft = historyMap;
    baseline = historyBaseline.mapId == draft.metadata.id ? historyBaseline
                                                         : MapCreatorPlayerSettingsBaseline{};
    editingIndex = -1;
    selectedIndex = -1;
    // Zero means an explicit standalone map. An unresolved reference stays
    // distinct so a later Save cannot silently detach it or pick another map.
    selectedParentIndex = effectiveParentId.empty() ? 0 : -1;
    for (std::size_t index = 0; index < committedMaps.size(); ++index)
    {
        if (committedMaps[index].metadata.id == draft.metadata.id)
        {
            editingIndex = static_cast<int>(index);
            selectedIndex = editingIndex;
        }
        if (!effectiveParentId.empty() && committedMaps[index].metadata.id == effectiveParentId)
            selectedParentIndex = static_cast<int>(index) + 1;
    }
}

inline bool mapCreatorParentSelectionIsResolved(int selectedParentIndex, std::size_t mapCount)
{
    return selectedParentIndex >= 0 && static_cast<std::size_t>(selectedParentIndex) <= mapCount;
}

// A journal request is acknowledged only after the writer confirms durable
// success. A false return or exception leaves both the pending bit and the
// newest transaction label untouched, so the next safe controller tick can
// retry exactly the same recovery snapshot.
template <typename WriteJournal>
MapCreatorCrashJournalFlushOutcome flushMapCreatorCrashJournalRequest(
    bool &pending,
    std::string &transactionLabel,
    WriteJournal &&writeJournal)
{
    if (!pending)
        return MapCreatorCrashJournalFlushOutcome::NothingPending;
    if (!std::forward<WriteJournal>(writeJournal)(transactionLabel))
        return MapCreatorCrashJournalFlushOutcome::RetryRequired;
    pending = false;
    transactionLabel.clear();
    return MapCreatorCrashJournalFlushOutcome::Written;
}

constexpr int mapCreatorNormalCreationIntentCount =
    static_cast<int>(MapCreationIntent::Cave) + 1;

inline bool mapCreatorCreationIntentIsNormal(MapCreationIntent intent)
{
    return intent >= MapCreationIntent::WorldOverview &&
           intent <= MapCreationIntent::Cave;
}

// Step five must never infer persistence from the in-memory Campaign alone:
// saveDraftMap installs the draft there before the database commit is known.
// This receipt is issued only after that commit succeeds and is invalidated by
// every subsequent authoring transaction.  It deliberately contains no
// publication action; a matching receipt merely permits opening the existing
// fail-closed player review.
struct MapCreatorPersistentSaveReceipt
{
    bool committed = false;
    std::string mapId;
    std::uint64_t mapRevision = 0;
    std::uint64_t authoringSerial = 0;
};

// A clean loaded draft is an eligible input to the existing review callback,
// which must persist and blackout before opening review. This is deliberately
// separate from a durable-save receipt: loading an in-memory map is no commit.
struct MapCreatorLoadedDraftBaseline
{
    std::string mapId;
    std::uint64_t mapRevision = 0;
    std::uint64_t authoringSerial = 0;
};

inline bool mapCreatorLoadedDraftIsCurrent(
    const MapCreatorLoadedDraftBaseline &loaded,
    const std::string &draftMapId,
    std::uint64_t draftMapRevision,
    std::uint64_t authoringSerial,
    bool pendingAtlasSelectionMatchesDraft)
{
    return !draftMapId.empty() && loaded.mapId == draftMapId &&
           loaded.mapRevision == draftMapRevision && loaded.authoringSerial == authoringSerial &&
           pendingAtlasSelectionMatchesDraft;
}

inline bool mapCreatorPersistentSaveIsCurrent(
    const MapCreatorPersistentSaveReceipt &receipt,
    const std::string &draftMapId,
    std::uint64_t draftMapRevision,
    std::uint64_t authoringSerial,
    bool pendingAtlasSelectionMatchesDraft)
{
    return receipt.committed && !draftMapId.empty() &&
           receipt.mapId == draftMapId &&
           receipt.mapRevision == draftMapRevision &&
           receipt.authoringSerial == authoringSerial &&
           pendingAtlasSelectionMatchesDraft;
}

inline bool mapCreatorDraftReadyForPlayerReview(
    const MapCreatorPersistentSaveReceipt &receipt,
    const MapCreatorLoadedDraftBaseline &loaded,
    const MapModel &draft,
    std::uint64_t authoringSerial,
    bool pendingAtlasSelectionMatchesDraft,
    bool hasUnsavedDraft,
    const std::vector<MapModel> &campaignMaps)
{
    if (hasUnsavedDraft)
        return false;
    const auto current = std::find_if(campaignMaps.begin(), campaignMaps.end(),
        [&](const MapModel &map) { return map.metadata.id == draft.metadata.id; });
    return current != campaignMaps.end() && current->revision == draft.revision &&
        (mapCreatorPersistentSaveIsCurrent(receipt, draft.metadata.id, draft.revision,
            authoringSerial, pendingAtlasSelectionMatchesDraft) ||
         mapCreatorLoadedDraftIsCurrent(loaded, draft.metadata.id, draft.revision,
            authoringSerial, pendingAtlasSelectionMatchesDraft));
}

enum class MapCreatorStepFivePrimaryAction
{
    SaveMap,
    ReviewPlayerView
};

inline MapCreatorStepFivePrimaryAction mapCreatorStepFivePrimaryAction(
    bool persistentSaveIsCurrent)
{
    return persistentSaveIsCurrent
               ? MapCreatorStepFivePrimaryAction::ReviewPlayerView
               : MapCreatorStepFivePrimaryAction::SaveMap;
}

// Staging a saved map never arms or publishes the detached display.  The
// existing enabled preference is left alone, while any currently visible
// payload and its review proof are revoked immediately.
inline bool stageMapCreatorPlayerReviewSelection(
    LocalPlayerDisplayModel &display,
    const std::string &mapId)
{
    if (mapId.empty())
        return false;
    display.publicMapId = mapId;
    display.showMap = true;
    blackoutLocalPlayerDisplay(display);
    return true;
}

// Structural validity alone is insufficient: duplicate owned identities are
// discovered only after the package has been decoded and verified.  Both the
// modal action and the controller apply path use this same fail-closed gate.
inline bool mapCreatorMapPackageCanImport(bool valid, bool importEligible)
{
    return valid && importEligible;
}

// The crash journal is the last durable copy of a completed editor
// transaction until the campaign database commit succeeds. Keep the save
// result and cleanup call coupled here so a future controller refactor cannot
// accidentally delete recovery data after a failed persistent save.
template <typename DiscardRecovery>
MapCreatorRecoveryCleanupOutcome finalizeMapCreatorRecoveryAfterPersistentSave(
    bool campaignSaved,
    DiscardRecovery &&discardRecovery)
{
    if (!campaignSaved)
        return MapCreatorRecoveryCleanupOutcome::RetainedBecauseSaveFailed;
    return std::forward<DiscardRecovery>(discardRecovery)()
               ? MapCreatorRecoveryCleanupOutcome::Cleared
               : MapCreatorRecoveryCleanupOutcome::ClearFailed;
}
