#include "Views/Editor/EditorWorkspacesSupport.hpp"

void EditorWindowView::reloadWorldInspectorDraft(const EntityId &entityId)
{
    if(!worldRecoveryReady())return;
    if(_worldDraftEntityId!=entityId&&!checkpointWorldRecovery(true)) {
        // Refuse this transition once; a render-time mismatch must not force a
        // failed filesystem retry on every following frame.
        _workspaceSelectionId=_worldDraftEntityId;return;
    }
    retainDungeonDraft();
    retainWorldDraft();
    const bool sameShop=_worldDraftKind==WorldDraftKind::Shop && _worldDraftEntityId==entityId;
    _worldDraftKind=WorldDraftKind::None; _worldFormEdited=false; _worldDraftReloadRequested=false;
    _worldDungeonDirty = false;
    const int preferredStockIndex = _worldSelectedStockIndex;
    const EntityId preferredRoomId = _worldSelectedRoomId;
    _worldDraftEntityId = entityId;
    if(!sameShop)_worldTransferQuantity = 1;
    const Campaign &campaign = _controller.currentCampaign();
    if (const ShopModel *shop = WorldInspector::findShop(campaign, entityId))
    {
        WorldInspectorDrafts::Draft<ShopModel> draft{*shop,*shop};
        if(const auto old=_worldDrafts.shops.find(entityId);old!=_worldDrafts.shops.end())draft=old->second;
        WorldInspectorDrafts::refresh(draft,*shop);
        _worldShopBase=std::move(draft.base);_worldShopDraft=std::move(draft.edited);
        _worldDraftKind=WorldDraftKind::Shop;_worldFormEdited=false;
        _worldSelectedRoomId.clear();
        _worldSelectedStockIndex =
            _worldShopDraft.stock.empty()
                ? -1
                : std::clamp(preferredStockIndex, 0, static_cast<int>(_worldShopDraft.stock.size()) - 1);
        return;
    }
    if (const DungeonModel *dungeon = WorldInspector::findDungeon(campaign, entityId))
    {
        const auto retained = _retainedDungeonDrafts.find(entityId);
        _worldDungeonDraft = retained == _retainedDungeonDrafts.end() ? *dungeon : retained->second;
        _worldDungeonDirty = retained != _retainedDungeonDrafts.end();
        _worldSelectedStockIndex = -1;
        if (!preferredRoomId.empty() && WorldInspector::findRoom(_worldDungeonDraft, preferredRoomId))
            _worldSelectedRoomId = preferredRoomId;
        else if (!_worldDungeonDraft.activeRoomId.empty() &&
                 WorldInspector::findRoom(_worldDungeonDraft, _worldDungeonDraft.activeRoomId))
            _worldSelectedRoomId = _worldDungeonDraft.activeRoomId;
        else if (!_worldDungeonDraft.rooms.empty())
            _worldSelectedRoomId = _worldDungeonDraft.rooms.front().metadata.id;
        else
            _worldSelectedRoomId.clear();
        return;
    }
    if (const NationModel *nation = WorldInspector::findNation(campaign, entityId))
    {
        WorldInspectorDrafts::Draft<NationModel> draft{*nation,*nation};
        if(const auto old=_worldDrafts.nations.find(entityId);old!=_worldDrafts.nations.end())draft=old->second;
        WorldInspectorDrafts::refresh(draft,*nation);
        _worldNationBase=std::move(draft.base);_worldNationDraft=std::move(draft.edited);
        _worldDraftKind=WorldDraftKind::Nation;_worldFormEdited=false;
        _worldSelectedTerritoryIndex = _worldNationDraft.territories.empty() ? -1 : 0;
        return;
    }
    if (const WorldPlaceModel *place = WorldInspector::findWorldPlace(campaign, entityId))
    {
        WorldInspectorDrafts::Draft<WorldPlaceModel> draft{*place,*place};
        if(const auto old=_worldDrafts.places.find(entityId);old!=_worldDrafts.places.end())draft=old->second;
        WorldInspectorDrafts::refresh(draft,*place);
        _worldPlaceBase=std::move(draft.base);_worldPlaceDraft=std::move(draft.edited);
        _worldDraftKind=WorldDraftKind::Place;_worldFormEdited=false;
        _worldSelectedTerritoryIndex = -1;
    }
}

void EditorWindowView::reportWorldInspectorResult(const WorldInspector::MutationResult &result,
                                                  const char *successEnglish, const char *successGerman)
{
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    if (result.status == WorldInspector::MutationStatus::CommittedContextChanged)
    {
        _worldInspectorStatus = german
            ? "Die Änderungen wurden gespeichert, aber Kampagne oder Entwurf wurden während des Speicherns geändert. Prüfe den Eintrag, bevor du fortfährst."
            : "Changes were saved, but the campaign or draft changed during the save. Review the entry before continuing.";
        // The durable save succeeded. Do not acknowledge, retain or reload a
        // replacement draft, and do not imply that the commit was rolled back.
        return;
    }
    if (result.applied())
    {
        _worldInspectorStatus = german ? successGerman : successEnglish;
        // Invalidate only. Reload on the next frame so controls currently holding
        // references into a draft never observe a vector reassignment mid-render.
        retainWorldDraft();
        _worldDraftReloadRequested=true;
        if(_worldDraftKind==WorldDraftKind::None)_worldDraftEntityId.clear();
        return;
    }
    if (!german)
    {
        _worldInspectorStatus = result.message;
        // A failed commit must leave the authored inspector draft available for retry.
        return;
    }
    switch (result.status)
    {
    case WorldInspector::MutationStatus::PersistenceFailed:
        _worldInspectorStatus =
            "Die Änderung konnte nicht gespeichert werden. Der vorherige Kampagnenzustand wurde im Arbeitsspeicher wiederhergestellt.";
        break;
    case WorldInspector::MutationStatus::NotFound:
        _worldInspectorStatus = "Der ausgewählte Eintrag oder ein benötigter Verweis ist nicht mehr vorhanden.";
        break;
    case WorldInspector::MutationStatus::Conflict:
        _worldInspectorStatus = "Die Änderung steht im Konflikt mit dem aktuellen Eintrag. Prüfe geänderte, fehlende oder mehrdeutige Werte; dein Entwurf bleibt erhalten.";
        break;
    default:
        _worldInspectorStatus = "Die Änderung ist ungültig. Prüfe Namen, Menge und Ziel.";
        break;
    }
}
