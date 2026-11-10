void markdownUiWrite(const std::filesystem::path &path,const std::string &text)
{std::filesystem::create_directories(path.parent_path());AtomicProjectFile::write(path,text);}
std::filesystem::path markdownUiEdit(const std::filesystem::path &root,const EntityId &id)
{std::string hex;for(unsigned char ch:id){hex+="0123456789abcdef"[ch>>4];hex+="0123456789abcdef"[ch&15];}return root/"editable"/(hex+".md");}
void markdownUiWait(HeadlessPanel &ui,EditorMarkdownExchange &exchange)
{
    markdown_test::until([&]{ui.frame();return !exchange.busy();},"Actual Markdown panel job did not settle");ui.frame(3);
}
struct MarkdownUiFixture
{
    Campaign initial;EditorWindowController editor;EntityId first,second;
    std::filesystem::path root=ApplicationPaths::writableRoot()/createEntityId("markdown-ui");
    MarkdownUiFixture()
    {
        require(std::filesystem::create_directory(root),"Cannot create fresh Markdown UI directory");
        initial.name="Markdown UI "+initial.metadata.id;initial.ruleSet=std::make_shared<DND5e>();
        StoryThreadModel a,b;a.title="Eligible note";b.title="Conflicting note";a.gmNotes="ORIGINAL_FIRST";b.gmNotes="ORIGINAL_SECOND";
        first=a.metadata.id;second=b.metadata.id;initial.storyThreads={a,b};
        require(ProjectSaveService::getInstance().saveCampaign(initial,"Markdown UI fixture"),"Cannot save Markdown UI fixture");
        markdown_test::exported(initial,root/"export");
        markdownUiWrite(markdownUiEdit(root/"export",first),"EXTERNAL_FIRST");markdownUiWrite(markdownUiEdit(root/"export",second),"EXTERNAL_SECOND");
        initial.storyThreads[1].gmNotes="CONCURRENT_SECOND";
        require(ProjectSaveService::getInstance().saveCampaign(initial,"Markdown conflict fixture"),"Cannot save Markdown conflict fixture");
        require(editor.setCurrentCampaign(initial),"Cannot bind Markdown UI fixture");
    }
    Campaign &campaign(){return PlayerReviewUiProbe::campaign(editor);}
    std::string saved(const EntityId &id)const
    {const auto c=menuReload(initial.metadata.id);for(const auto &n:c.storyThreads)if(n.metadata.id==id)return n.gmNotes;throw std::runtime_error("Saved Markdown note missing");}
};
ImGuiID markdownUiRow(HeadlessPanel &ui,const char *label,const EntityId &id,const std::string &pane)
{
    for(float direction:{-1.f,1.f})for(int step=0;step<48;++step) {
        auto *body=ImGui::FindWindowByName(pane.c_str());require(body&&body->Active,"Observed Markdown body disappeared");
        for(const auto &item:ui.activeControls(label)) {
            const auto *owner=ImGui::FindWindowByName(item.window.c_str());auto *ancestor=owner;
            while(ancestor&&ancestor!=body)ancestor=ancestor->ParentWindow;
            require(ancestor==body,"Markdown row appeared outside observed body");
            if(owner&&item.id==ImHashStr(label,0,ImHashStr(id.c_str(),0,owner->ID)))return item.id;
        }
        if(body->ScrollMax.y<=0)break;
        const auto p=body->InnerClipRect.GetCenter();ImGui::GetIO().AddMouseViewportEvent(body->ViewportId);
        ImGui::GetIO().AddMousePosEvent(p.x,p.y);ui.frame();const auto before=body->Scroll.y;
        ImGui::GetIO().AddMouseWheelEvent(0,direction);ui.frame(3);if(body->Scroll.y==before)break;
    }
    ui.controlFailure("Exact stable-ID Markdown row was not submitted",label);
}
void markdownUiDisabled(HeadlessPanel &ui,const char *label,ImGuiID id)
{
    ui.compactReach(label,id);
    for(int i=0;i<3;++i){const auto item=ui.compactItem(label,id);ui.reviewVisible(item);
        const auto *owner=ImGui::FindWindowByName(item.window.c_str());ImGui::GetIO().AddMouseViewportEvent(owner->ViewportId);
        const auto center=ImRect(item.minimum,item.maximum).GetCenter();ImGui::GetIO().AddMousePosEvent(center.x,center.y);ui.frame();}
    require(GImGui->HoveredId==id&&GImGui->HoveredIdIsDisabled,"Conflict row was not actually disabled under observed hover");
}
void markdownUiRaw(HeadlessPanel &ui,const std::string &pane,const char *childId,const char *text)
{
    for(int step=0;step<48;++step) {
        auto *body=ImGui::FindWindowByName(pane.c_str());require(body&&body->Active,"Observed Markdown text body disappeared");
        ImGuiWindow *raw=nullptr;
        for(auto *window:GImGui->Windows)if(window->Active&&window->ParentWindow==body&&std::string(window->Name).find(childId)!=std::string::npos){
            require(!raw,"Ambiguous actual Markdown raw-text child");raw=window;}
        require(raw!=nullptr,"Actual Markdown raw-text child was not created");
        const ImRect line(raw->DC.CursorStartPos,ImVec2(raw->DC.CursorStartPos.x+1,raw->DC.CursorStartPos.y+raw->FontRefSize));
        if(!raw->SkipItems&&raw->InnerClipRect.Contains(line)&&body->InnerClipRect.Contains(line)) {
            require(contains(ui.rendered,text),"Visible actual raw-text child omitted expected reviewed text");return;
        }
        require(body->ScrollMax.y>0,"Raw-text child is clipped without an outer scroll range");
        const auto center=body->InnerClipRect.GetCenter();ImGui::GetIO().AddMouseViewportEvent(body->ViewportId);
        ImGui::GetIO().AddMousePosEvent(center.x,center.y);ui.frame();
        ImGui::GetIO().AddMouseWheelEvent(0,line.Min.y<body->InnerClipRect.Min.y?1.f:-1.f);ui.frame(3);
    }
    ui.controlFailure("Queued wheel did not reveal reviewed raw text",childId,0,pane.c_str());
}
