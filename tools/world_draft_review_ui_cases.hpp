// Real production popup and lower-field input. Runs under both locales and
// the existing normal/1280x720 scaled CPU-ImGui workspace variants.
std::string worldReviewPopup(Interaction &ui,bool de)
{
    ui.click(de?"Weltentwürfe prüfen":"Review World drafts","Workspace");
    const auto item=ui.locate(de?"Erhaltene Werte":"Retained values");
    auto *window=ImGui::FindWindowByName(item.window.c_str());
    require(window&&(window->Flags&ImGuiWindowFlags_Popup),"World review is not an explicit popup");
    const auto *viewport=ImGui::GetMainViewport();
    require(window->Size.x<=viewport->WorkSize.x*.9f+1.f&&window->Size.y<=viewport->WorkSize.y*.85f+1.f&&
        window->Pos.x>=viewport->WorkPos.x-1.f&&window->Pos.y>=viewport->WorkPos.y-1.f&&
        window->Pos.x+window->Size.x<=viewport->WorkPos.x+viewport->WorkSize.x+1.f&&
        window->Pos.y+window->Size.y<=viewport->WorkPos.y+viewport->WorkSize.y+1.f,
        "World draft review exceeds the observed viewport: "+std::to_string(window->Size.x)+"x"+std::to_string(window->Size.y));
    return window->Name;
}
void worldDraftReviewLayout(UiInteractionTrace &trace,bool de)
{
    EditorWindowController editor;Interaction ui(editor,trace);auto c=worldUiFixture();
    const auto id=c.worldPlaces[0].metadata.id;
    require(ProjectSaveService::getInstance().saveCampaign(c,"World review layout fixture"),"World review fixture save failed");
    editor.setCurrentCampaign(c);worldOpenPlace(editor,ui,id);
    ui.replaceText(de?"Beschreibung":"Description","EXPANDED_REVIEW_DRAFT","Workspace");
    auto popup=worldReviewPopup(ui,de);
    ui.click(de?"Erhaltene Werte":"Retained values",popup.c_str());
    require(ui.renderedText().find("EXPANDED_REVIEW_DRAFT")!=std::string::npos,"Explicit review omitted retained values");
    ui.click(de?"Eintrag öffnen":"Open entry",popup.c_str());
    require(ImGui::GetCurrentContext()->OpenPopupStack.empty()&&WorkspaceInteractionProbe::worldSelection(editor)==id,
        "Open entry did not close review and retain the exact target");
    ui.click(de?"Ort speichern":"Save place","Workspace");
    require(!editor.hasUnsavedChanges()&&ImGui::GetCurrentContext()->OpenPopupStack.empty(),"Saving left review or a World draft open");
    ui.click(de?"Sitzung":"Session");worldOpenPlace(editor,ui,id);
    // The entry stays submitted while empty; normal input must not open it.
    ui.click(de?"Weltentwürfe prüfen":"Review World drafts","Workspace");
    require(ImGui::GetCurrentContext()->OpenPopupStack.empty()&&!editor.hasUnsavedChanges(),"Empty review entry was actionable");
    const char *notes=de?"SL-Notizen":"GM notes";
    ui.click(notes,"Workspace");ui.frame(4);
    const auto before=ui.locate(notes);const auto active=ImGui::GetCurrentContext()->ActiveId;
    auto *owner=ImGui::FindWindowByName(before.window.c_str());
    require(owner&&active==before.id&&owner->InnerClipRect.Contains(ImRect(before.minimum,before.maximum).GetCenter()),
        "Lower World field lacks visible, exact keyboard focus");
    require(WorkspaceInteractionProbe::placeDraft(editor).notes.empty(),"First-edit fixture notes are not pristine");
    const auto scroll=owner->Scroll;
    ImGui::GetIO().AddInputCharactersUTF8("F");
    for(int frame=0;frame<6;++frame) {
        ui.frame();const auto after=ui.locate(notes);
        const auto close=[](float a,float b){return std::abs(a-b)<.01f;};
        require(after.id==before.id&&after.window==before.window&&ImGui::GetCurrentContext()->ActiveId==active&&
            close(after.minimum.x,before.minimum.x)&&close(after.minimum.y,before.minimum.y)&&
            close(after.maximum.x,before.maximum.x)&&close(after.maximum.y,before.maximum.y)&&
            close(owner->Scroll.x,scroll.x)&&close(owner->Scroll.y,scroll.y),
            "First World edit moved/deactivated the lower field at frame "+std::to_string(frame)+" before=("+
            std::to_string(before.minimum.x)+","+std::to_string(before.minimum.y)+") after=("+
            std::to_string(after.minimum.x)+","+std::to_string(after.minimum.y)+") active="+
            std::to_string(ImGui::GetCurrentContext()->ActiveId)+" expected="+std::to_string(active));
        require(ImGui::GetCurrentContext()->OpenPopupStack.empty()&&WorkspaceInteractionProbe::placeDraft(editor).notes=="F",
            "First edit reopened review or changed typed text");
    }
    ImGui::GetIO().AddInputCharactersUTF8("_LOWER_FIELD_CONTINUES");ui.frame(3);
    require(WorkspaceInteractionProbe::placeDraft(editor).notes=="F_LOWER_FIELD_CONTINUES"&&
        ImGui::GetCurrentContext()->ActiveId==active&&editor.hasUnsavedChanges()&&worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes.empty(),
        "Stable lower-field typing lost focus/text or implicitly committed the draft");
    // Global Save can consume the last draft while review is open (e.g. shortcut).
    popup=worldReviewPopup(ui,de);
    require(editor.saveCurrentCampaign("save from open World review"),"Open-review save failed");ui.frame(3);
    require(ImGui::GetCurrentContext()->OpenPopupStack.empty()&&!editor.hasUnsavedChanges()&&
        worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes=="F_LOWER_FIELD_CONTINUES","Save did not close consumed review or preserve exact text");
    ui.replaceText(notes,"PRIVATE_REVIEW_PRESENTATION_CANARY","Workspace");worldReviewPopup(ui,de);
    WorkspaceInteractionProbe::liveCampaign(editor).presentationModeActive=true;ui.frame(3);
    require(ImGui::GetCurrentContext()->OpenPopupStack.empty()&&trace.find(de?"Erhaltene Werte":"Retained values").empty()&&
        WorkspaceInteractionProbe::placeDraft(editor).notes=="PRIVATE_REVIEW_PRESENTATION_CANARY",
        "Presentation did not close private review or discarded its draft");
    WorkspaceInteractionProbe::liveCampaign(editor).presentationModeActive=false;ui.frame();worldReviewPopup(ui,de);
    editor.setCurrentCampaign(readCampaign(c.metadata.id));ui.frame(3);
    require(ImGui::GetCurrentContext()->OpenPopupStack.empty()&&WorkspaceInteractionProbe::worldRecovery(editor).pending,
        "Same-ID reset retained the private popup or failed to require explicit recovery");
    // Campaign replacement deliberately returns to Session. Reach the actual
    // World surface before locating its private recovery entry point.
    ui.click(de?"Welt":"World");ui.frame(3);
    ui.click(de?"Weltentwürfe prüfen":"Review World drafts","Workspace");
    ui.click(de?"Gesicherte Entwürfe verwerfen":"Discard saved drafts");
    require(!editor.hasUnsavedChanges(),"Explicit saved-draft discard did not clear recovery on reset");
    std::cout<<"World review: explicit bounded recovery, stable first-edit geometry/focus, and save/presentation/reset closure passed\n";
}
