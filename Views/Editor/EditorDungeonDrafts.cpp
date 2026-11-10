#include "Views/Editor/EditorViewDetails.hpp"

void EditorWindowView::retainDungeonDraft()
{
    if (_worldDungeonDirty && !_worldDraftEntityId.empty())
        _retainedDungeonDrafts[_worldDraftEntityId] = _worldDungeonDraft;
}

WorldInspector::MutationResult EditorWindowView::applyDungeonDrafts(Campaign &campaign)
{
    retainDungeonDraft();
    for (const auto &[id, draft] : _retainedDungeonDrafts)
    {
        const auto result = WorldInspector::updateDungeon(campaign, id, draft);
        if (!result.applied()) return result;
    }
    return {WorldInspector::MutationStatus::Applied, "", {}};
}

void EditorWindowView::clearDungeonDrafts()
{
    _worldDungeonDirty = false;
    _retainedDungeonDrafts.clear();
}
