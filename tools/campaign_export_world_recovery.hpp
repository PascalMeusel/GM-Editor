// Included inside the export fixture namespace after its file helpers.
const std::string worldDraftSecret = "UNCOMMITTED_WORLD_RECOVERY_EXPORT_CANARY_481927";
const std::string editorDraftSecret = "UNCOMMITTED_ITEM_NOTES_RECOVERY_EXPORT_CANARY_219684";
void seedWorldRecoveryCanary(Campaign &campaign)
{
    WorldPlaceModel place;
    place.metadata.id = "export_world_recovery_place";
    place.name = "Saved place";
    place.notes = "Canonical place note";
    campaign.worldPlaces.push_back(place);
    WorldInspectorDrafts::Draft<WorldPlaceModel> draft{place, place};
    draft.edited.notes = worldDraftSecret;
    WorldInspectorDrafts::Store store;
    store.retain(draft);
    const auto saved = WorldDraftRecovery::save(campaign, store);
    require(saved.success, "Could not create real World recovery checkpoint: " + saved.error);
    const auto restored = WorldDraftRecovery::read(campaign);
    require(restored.status == WorldDraftRecovery::ReadStatus::Ready &&
        restored.drafts.places.at(place.metadata.id).edited.notes == worldDraftSecret &&
        campaign.worldPlaces.back().notes == place.notes,
        "World recovery fixture failed round trip or modified canonical data");
    const auto checkpoint = ProjectPackageLayout::layoutForCampaign(campaign).recoveryDirectory /
        "Drafts/world-inspector-v1.draft";
    require(readFile(checkpoint).find(worldDraftSecret) != std::string::npos,
        "Actual private recovery file lacks export canary");
    EditorDraftRecovery::Store editorDrafts;
    EditorDraftRecovery::ItemDraft item;item.item.name="Uncommitted item";item.item.description=editorDraftSecret;
    item.powerDirty=true;item.power.notes=editorDraftSecret;editorDrafts.items[item.item.metadata.id]=item;
    editorDrafts.notes=EditorDraftRecovery::Notes{"recovery-character","",editorDraftSecret,editorDraftSecret,"","",""};
    require(EditorDraftRecovery::save(campaign,editorDrafts).empty(),"Cannot create real item/notes recovery canary");
    const auto editorRead=EditorDraftRecovery::read(campaign);
    require(editorRead.status==EditorDraftRecovery::ReadStatus::Ready&&editorRead.rawBytes.find(editorDraftSecret)!=std::string::npos&&
        editorRead.drafts.notes->gmNotes==editorDraftSecret,"Item/notes recovery canary failed its independent read");
}
