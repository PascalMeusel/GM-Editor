#pragma once
// Included inside the existing production-controller friend probe. This uses
// the isolated workflow database and real SQLite failures; no UI or audio calls.
static void exerciseFailedSaveCorrections(Campaign &campaign, BattleTrackerController &tracker)
{
    auto &history = ProjectCommandManager::getInstance();
    const auto characterId = tracker._draftEncounter.participants[0].sourceEntityId;
    const auto *resources = tracker.getSelectedSourceResources();
    require(resources && !resources->empty() && resources->front().used < resources->front().maximum,
            "Correction fixture needs an available linked player resource");
    const auto resourceId = resources->front().metadata.id;
    SQLite::Database control(ApplicationPaths::databasePath().u8string(), SQLite::OPEN_READWRITE);
    const auto failSaves = [&] {
        control.exec("CREATE TRIGGER reject_initial_combat_correction BEFORE INSERT ON characters "
                     "BEGIN SELECT RAISE(ABORT, 'injected original combat save failure'); END;");
    };
    const auto allowSaves = [&] { control.exec("DROP TRIGGER reject_initial_combat_correction;"); };
    const auto committedValue = [&](bool resource) {
        const auto saved = reload(campaign.metadata.id);
        const auto player = std::find_if(saved.characters.begin(), saved.characters.end(),
            [&](const auto &item) { return item.metadata.id == characterId; });
        require(player != saved.characters.end(), "Correction fixture lost its canonical character");
        if (!resource) return player->temporaryHitPoints;
        const auto entry = std::find_if(player->resourceTrackers.begin(), player->resourceTrackers.end(),
            [&](const auto &item) { return item.metadata.id == resourceId; });
        require(entry != player->resourceTrackers.end(), "Correction fixture lost its linked resource");
        return entry->used;
    };
    for (bool resource : {false, true})
    {
        history.clear();
        const int before = committedValue(resource);
        failSaves();
        if (resource) tracker.setSelectedSourceResourceUsed(0, before + 1);
        else tracker.setSelectedTemporaryHitPoints(before + 1);
        require(tracker._encounterSaveFailed && history.canUndo() && committedValue(resource) == before,
                "Original failed combat edit lost history or modified the committed database");
        require(!history.undo() && history.canUndo() && committedValue(resource) == before,
                "Combat undo was allowed before the original save succeeded");
        allowSaves();
        tracker.saveEncounter();
        require(!tracker._encounterSaveFailed && committedValue(resource) == before + 1,
                "Retry did not commit the retained combat edit");
        require(history.undo() && committedValue(resource) == before,
                "Successfully retried combat edit could not undo canonical health/resource data");
        require(history.redo() && committedValue(resource) == before + 1,
                "Successfully retried combat edit could not redo canonical health/resource data");
        require(history.undo() && committedValue(resource) == before,
                "Correction fixture could not return to its original values");
    }
    history.clear();
    const int before = committedValue(false);
    failSaves();
    tracker.setSelectedTemporaryHitPoints(before + 2);
    // Older detail controls can edit the retained draft while saving is failed.
    // captureCorrection must not create another command for this later edit.
    tracker.setSelectedTemporaryHitPoints(before + 3);
    require(tracker._encounterSaveFailed && !history.undo() && committedValue(false) == before,
            "A later pending edit bypassed the failed-save correction guard");
    allowSaves();
    tracker.saveEncounter();
    require(!tracker._encounterSaveFailed && committedValue(false) == before + 3,
            "Retry lost the later pending combat edit");
    require(!history.undo() && committedValue(false) == before + 3 &&
                tracker._draftEncounter.participants[0].temporaryHitPoints == before + 3,
            "Stale correction overwrote later same-field work after retry");
    history.clear();
    tracker.setSelectedTemporaryHitPoints(before);
    require(!tracker._encounterSaveFailed && committedValue(false) == before,
            "Correction fixture could not restore its baseline");
    history.clear();
}
