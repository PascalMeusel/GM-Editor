// Real shared guidance widgets, observed popup stack and queued CPU ImGui input.
// The optional viewport adapter is memory-only; no platform/renderer backend.
ImGuiWindow *guidanceBounds(HeadlessPanel &ui, int depth)
{
    const auto closing=ui.compactItem("GuidanceClose");ui.reviewVisible(closing);
    auto *popup=ImGui::FindWindowByName(closing.window.c_str());auto &g=*ImGui::GetCurrentContext();
    require(g.OpenPopupStack.Size==depth&&popup&&(popup->Flags&ImGuiWindowFlags_Popup),
        "Guidance footer is outside the expected real popup stack");
    const auto &entry=g.OpenPopupStack.back();const auto *vp=ImGui::GetMainViewport();
    const ImRect work(vp->WorkPos,ImVec2(vp->WorkPos.x+vp->WorkSize.x,vp->WorkPos.y+vp->WorkSize.y));
    require(entry.Window==popup&&entry.PopupId==popup->PopupId&&
        entry.PopupId==ImHashStr("##GuidancePopup",0,entry.OpenParentId),"Guidance topic popup identity changed");
    if(popup->Viewport!=vp||!work.Contains(popup->Rect())||std::abs(popup->FontRefSize-27.f)>.01f)
        ui.controlFailure("Guidance escaped its actual owner work area or 27px font","GuidanceClose",closing.id);
    ImGuiWindow *body=nullptr;
    for(auto *candidate:g.Windows)if(candidate->Active&&candidate->ParentWindow==popup&&
        std::string(candidate->Name).find("GuidanceBody")!=std::string::npos){require(!body,"Ambiguous guidance body");body=candidate;}
    require(body&&!(body->ChildFlags&ImGuiChildFlags_NavFlattened)&&body->InnerClipRect.GetHeight()>27,
        "Guidance lacks a usable keyboard-enterable scrolling body");
    require(body->InnerClipRect.Max.y<=closing.minimum.y,"Guidance body overlaps persistent Close");
    return body;
}
void guidanceKeyboardRead(HeadlessPanel &ui)
{
    auto *body=guidanceBounds(ui,1);auto &g=*ImGui::GetCurrentContext();
    require(body->ScrollMax.y>100,"Long guidance no longer exercises keyboard scrolling");
    bool focused=false;
    for(int n=0;n<8;++n){
        if(g.NavId==body->ChildId&&g.NavCursorVisible&&g.NavWindow==body->ParentWindow){focused=true;break;}
        ui.reviewKey(ImGuiKey_Tab);
    }
    if(!focused)ui.controlFailure("Real Tab did not focus the scroll-only guidance body","GuidanceClose");
    ui.reviewKey(ImGuiKey_Enter);require(g.NavWindow==body,"Enter did not enter actual guidance scrolling child");
    const float before=body->Scroll.y;ui.reviewKey(ImGuiKey_PageDown);
    require(body->Scroll.y>before,"PageDown did not scroll long guidance through native keyboard navigation");
    ui.reviewKey(ImGuiKey_End);require(body->Scroll.y>=body->ScrollMax.y-1,"End did not expose final guidance example");
    // Escape leaves the scroll-only child first; it must not close the modal.
    ui.reviewKey(ImGuiKey_Escape);require(g.OpenPopupStack.Size==1&&g.NavWindow==body->ParentWindow,
        "Escape from guidance body lost its parent popup");
}
void guidanceReadExample(HeadlessPanel &ui, const char *example, int depth)
{
    auto *body=guidanceBounds(ui,depth);bool visible=false;
    for(int n=0;n<40;++n){
        const ImRect last(ImVec2(body->WorkRect.Min.x,body->DC.CursorPosPrevLine.y),
            ImVec2(body->DC.CursorPosPrevLine.x,body->DC.CursorPosPrevLine.y+body->DC.PrevLineSize.y));
        if(last.GetHeight()>0&&body->InnerClipRect.Contains(last.Min)&&
            body->InnerClipRect.Contains(ImVec2(last.Max.x-1,last.Max.y-1))){visible=true;break;}
        auto &io=ImGui::GetIO();const auto p=body->InnerClipRect.GetCenter();
        io.AddMouseViewportEvent(body->ViewportId);io.AddMousePosEvent(p.x,p.y);ui.frame();
        io.AddMouseWheelEvent(0,-1);ui.frame(3);
    }
    require(visible&&contains(ui.rendered,example),"Guidance example is not reachable inside actual body clip");
    guidanceBounds(ui,depth);
}
void guidanceSharedTopics(bool de)
{
    std::vector<ImGuiID> ids;
    for(int n=0;n<8;++n){
        // Each topic is an independent source window. Replacing the topic of
        // a currently focused source would remove its NavId without a real
        // user navigation action, unlike the production fixed-topic callers.
        HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
        ui.context(std::string("Guidance topic ")+std::to_string(n)+" identity and keyboard 27px "+(de?"DE":"EN"));
        const auto topic=static_cast<UiGuidance::Topic>(n);
        char retained[80]="UNCHANGED_AUTHORED_SOURCE";int sourceActions=0;
        ui.draw=[&]{ImGui::InputText("Source draft",retained,sizeof(retained));
            if(ImGui::Button("Source action"))++sourceActions;
            UiGuidance::drawPurpose(topic);const auto *vp=ImGui::GetMainViewport();
            ImGui::SetCursorScreenPos(ImVec2(vp->WorkPos.x+24,vp->WorkPos.y+vp->WorkSize.y-90));
            UiGuidance::drawDetails(topic);};
        ui.frame(4);
        const auto source=ui.compactItem("GuidanceDetails");
        require(std::find(ids.begin(),ids.end(),source.id)==ids.end(),"Two guidance topics share a control ID");ids.push_back(source.id);
        if(n%2){ui.reviewTabTo("GuidanceDetails");ui.reviewKey(ImGuiKey_Enter);}else ui.compactClick("GuidanceDetails");
        auto *body=guidanceBounds(ui,1);const auto popupId=body->ParentWindow->PopupId;
        const auto closeId=ui.compactItem("GuidanceClose").id;
        if(topic==UiGuidance::Topic::DataSafety)guidanceKeyboardRead(ui);
        guidanceReadExample(ui,UiGuidance::text(topic).example,1);
        // A locale change must change prose, not the topic/control or open popup.
        gm::localization::LocalizationService::instance().setPreference(de?
            gm::localization::LocalePreference::English:gm::localization::LocalePreference::German,false);ui.frame(3);
        require(ui.controlId("GuidanceDetails")==source.id&&ui.compactItem("GuidanceClose").id==closeId&&
            guidanceBounds(ui,1)->ParentWindow->PopupId==popupId,"Locale change replaced guidance identity");
        guidanceReadExample(ui,UiGuidance::text(topic).example,1);
        ui.reviewTabTo("GuidanceClose");ui.reviewKey(ImGuiKey_Tab,true);ui.reviewTabTo("GuidanceClose");
        ui.reviewKey(n%2?ImGuiKey_Escape:ImGuiKey_Space);
        auto &g=*ImGui::GetCurrentContext();
        require(!ui.popupOpen()&&g.NavId==source.id&&g.NavWindow&&source.window==g.NavWindow->Name,
            "Closing guidance did not return keyboard focus to its real source");
        require(sourceActions==0&&std::string(retained)=="UNCHANGED_AUTHORED_SOURCE","Guidance changed source draft or activated business action");
        gm::localization::LocalizationService::instance().setPreference(de?
            gm::localization::LocalePreference::German:gm::localization::LocalePreference::English,false);
    }
}
void guidanceNestedModal(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Nested modal guidance 27px ")+(de?"DE":"EN"));
    char draft[64]="RETAINED_PARENT_MODAL_DRAFT";int commits=0;
    ui.draw=[&]{if(ImGui::Button("Open source modal"))ImGui::OpenPopup("Source modal");
        const auto *vp=ImGui::GetMainViewport();ImGui::SetNextWindowViewport(vp->ID);
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x+25,vp->WorkPos.y+80),ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(900,600));
        if(ImGui::BeginPopupModal("Source modal",nullptr,ImGuiWindowFlags_NoSavedSettings)){
            ImGui::InputText("Source value",draft,sizeof(draft));
            UiGuidance::drawDetails(UiGuidance::Topic::DataSafety);
            if(ImGui::Button("Commit source"))++commits;
            if(ImGui::Button("Close source"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}};
    ui.frame(3);ui.compactClick("Open source modal");const auto id=ui.compactItem("GuidanceDetails").id;
    ui.reviewTabTo("GuidanceDetails");ui.reviewKey(ImGuiKey_Space);guidanceReadExample(ui,UiGuidance::text(UiGuidance::Topic::DataSafety).example,2);
    ui.reviewTabTo("GuidanceClose");ui.reviewKey(ImGuiKey_Enter);
    require(ImGui::GetCurrentContext()->OpenPopupStack.Size==1&&ImGui::GetCurrentContext()->NavId==id&&
        commits==0&&std::string(draft)=="RETAINED_PARENT_MODAL_DRAFT","Nested help closed/committed/changed parent modal");
    ui.compactClick("Close source");require(!ui.popupOpen(),"Parent modal no longer independently closes");
}
