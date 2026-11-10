#pragma once
#include "player_review_ui_probe.hpp"
#include "Views/Editor/Widgets/UiInteractionTrace.hpp"
#include "player_display_performance_confirmation.hpp"

namespace player_perf
{
// Actual production popup controls driven by observed ImGui item rectangles.
// The legacy panel uses a QA host window; Editor uses its normal panel path.
struct PopupExercise
{
    EditorWindowController &editor;Metrics &metrics;Fixture &fixture;
    UiInteractionTrace trace{ImGui::GetCurrentContext()};
    ImGuiID hook=0;bool legacy=false,complete=false;
    int phase=0,clickStep=0,frames=0;
    int preparingFrames=0,readyFrames=0;
    std::uint64_t imageDecodesAtRequest=0,imageUploadsAtRequest=0;
    std::size_t mapVisibleImageCount=0;
    bool refreshRetirementObserved=false;
    Clock::time_point preparationStarted{};
    std::uint64_t generation=0,builds=0,filters=0;
    const MapModel *gmMap=nullptr,*playerMap=nullptr;
    ImTextureID image=0;ImGuiID clickId=0;
    bool noSnapshots=false;ImTextureID sentinel=0;
    std::optional<ButtonConfirmation> confirmation;
    explicit PopupExercise(EditorWindowController &e,Metrics &m,Fixture &f,bool snapshotsDisabled,ImTextureID ordinary)
        :editor(e),metrics(m),fixture(f),noSnapshots(snapshotsDisabled),sentinel(ordinary)
    {
        PlayerReviewUiProbe::owner(e).setMeasurements(&m.reviewStages);
        PlayerReviewUiProbe::owner(PlayerReviewUiProbe::gm(e)).setMeasurements(&m.reviewStages);
        PlayerReviewUiProbe::open(e);
        ImGuiContextHook h;h.Type=ImGuiContextHookType_EndFramePre;h.UserData=this;
        h.Callback=[](ImGuiContext *,ImGuiContextHook *h){auto &self=*static_cast<PopupExercise *>(h->UserData);
            if(!self.legacy)return;
            const auto *viewport=ImGui::GetMainViewport();
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x+16,viewport->WorkPos.y+24),ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(std::min(1200.f,viewport->WorkSize.x-32),
                std::min(680.f,viewport->WorkSize.y-48)),ImGuiCond_Always);
            if(ImGui::Begin("Legacy player review benchmark",nullptr,ImGuiWindowFlags_NoSavedSettings))
                PlayerReviewUiProbe::draw(PlayerReviewUiProbe::gm(self.editor));
            ImGui::End();};
        hook=ImGui::AddContextHook(ImGui::GetCurrentContext(),&h);
    }
    ~PopupExercise(){ImGui::GetCurrentContext()->TestEngineHookItems=true;ImGui::RemoveContextHook(ImGui::GetCurrentContext(),hook);}
    PlayerReviewSession &owner(){return legacy?PlayerReviewUiProbe::owner(PlayerReviewUiProbe::gm(editor)):PlayerReviewUiProbe::owner(editor);}
    std::uint64_t count(PlayerDisplayReview::Stage stage)const
    {std::uint64_t n=0;for(const auto &row:metrics.reviewStages.values)n+=row[std::size_t(stage)].count;return n;}
    const char *label()
    {if(phase==17)return confirmation->label();
     if((phase==1||phase==4)&&!owner().preparing())return legacy?"legacy_popup_image_pending":"editor_popup_image_pending";
     return (phase==1||phase==4)&&owner().preparing()?(legacy?"legacy_popup_preparing":"editor_popup_preparing"):
        phase==2?(legacy?"legacy_popup_map_idle":"editor_popup_map_idle"):
        phase==9?(legacy?"legacy_popup_image_idle":"editor_popup_image_idle"):(legacy?"legacy_popup_control":"editor_popup_control");}
    bool click(const char *label,bool disabled=false)
    {
        if(clickStep==3){clickStep=0;return true;}
        auto items=trace.find(label);auto *popup=ImGui::GetTopMostPopupModal();
        items.erase(std::remove_if(items.begin(),items.end(),[&](const auto &item){
            auto *w=ImGui::FindWindowByName(item.window.c_str());if(w&&w->ID==item.id)return true;
            if(!popup)return false;while(w&&w!=popup)w=w->ParentWindow;return w!=popup;}),items.end());
        if(items.size()!=1){const auto *v=ImGui::GetMainViewport();
            throw std::runtime_error(std::string("Native review control not unique: ")+label+" count="+std::to_string(items.size())+
                " owner="+(legacy?"legacy":"editor")+" work_origin="+std::to_string(v->WorkPos.x)+","+std::to_string(v->WorkPos.y));}
        const auto &item=items.front();auto *w=ImGui::FindWindowByName(item.window.c_str());
        require(w&&w->InnerClipRect.Contains(ImRect(item.minimum,item.maximum)),std::string("Native review control clipped: ")+label);
        const auto center=ImRect(item.minimum,item.maximum).GetCenter();auto &io=ImGui::GetIO();
        if(clickStep==0){clickId=item.id;io.AddFocusEvent(true);io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
            io.AddMousePosEvent(center.x,center.y);++clickStep;return false;}
        require(clickId==item.id,"Native review control identity changed during input");
        if(clickStep==1){const auto *context=ImGui::GetCurrentContext();
            require(context->HoveredId==clickId&&context->HoveredIdIsDisabled==disabled,
            disabled?"Known-stale confirm is still enabled/hoverable":"Native review target not hovered");io.AddMouseButtonEvent(0,true);++clickStep;return false;}
        if(clickStep==2){
            if(phase==0||phase==3){preparationStarted=Clock::now();preparingFrames=readyFrames=0;refreshRetirementObserved=false;
                imageDecodesAtRequest=metrics.imageStages.values[std::size_t(gm::image_texture::Origin::Frozen)]
                    [std::size_t(gm::image_texture::Stage::DecodeOrientation)].count;
                imageUploadsAtRequest=metrics.imageStages.values[std::size_t(gm::image_texture::Origin::Frozen)]
                    [std::size_t(gm::image_texture::Stage::UploadSubmit)].count;}
            io.AddMouseButtonEvent(0,false);++clickStep;return false;}
        clickStep=0;return true;
    }
    void before(std::uint64_t frame)
    {
        if(phase==17){ImGui::GetCurrentContext()->TestEngineHookItems=true;confirmation->before(*this,frame);trace.beginFrame();return;}
        if(phase==4&&readyFrames==1){
            // Observe retirement after the adoption frame's collector, before
            // the deferred replacement upload can reuse the deleted GL name.
            const auto stats=imageTextureCacheStats();
            require(!textureAlive(image)&&stats.scopedEntries<=1&&stats.scopedLoadedTextures==0,
                "Refresh did not retire its old image before the replacement load");
            refreshRetirementObserved=true;
            metrics.facts[std::string(legacy?"legacy":"editor")+"_refresh_retired_before_new_load"]="true";
        }
        ImGui::GetCurrentContext()->TestEngineHookItems=phase!=2&&phase!=9;
        if(phase==0&&frames>=4&&click("Open player window…")){phase=1;frames=0;}
        else if(phase==3&&frames>=2&&click("Refresh review")){
            if(owner().preparing())require(textureAlive(image),"Pending refresh retired the earlier frozen image");
            phase=4;frames=0;}
        else if(phase==5&&click("Cancel")){phase=6;frames=0;}
        else if(phase==10&&frames==0){replaceReviewFile(true);}
        else if(phase==11&&click("Open player window")){phase=12;frames=0;}
        else if(phase==15&&click("Open player window",true)){phase=16;frames=0;}
        else if(phase==7||phase==13){auto *w=body();const auto position=w->InnerClipRect.GetCenter();
            ImGui::GetIO().AddFocusEvent(true);ImGui::GetIO().AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
            ImGui::GetIO().AddMousePosEvent(position.x,position.y);
            if(frames%3==2)ImGui::GetIO().AddMouseWheelEvent(0,-4);}
        trace.beginFrame();
    }
    void replaceReviewFile(bool changed)
    {
        const auto root=std::filesystem::u8path(metrics.facts.at("run_root"));
        const auto sibling=fixture.source.parent_path()/(std::string(legacy?"legacy":"editor")+(changed?"-review-change.png":"-review-restore.png"));
        std::filesystem::copy_file(changed?fixture.replacement:root/"original.png",sibling);
        std::filesystem::last_write_time(sibling,fixture.originalTime);replaceSource(fixture,sibling);
        require(ProjectAssetImport::computeContentHash(fixture.source)==(changed?fixture.replacementHash:fixture.originalHash),
            "Held-review same-metadata replacement bytes differ from validated pair");
    }
    ImGuiWindow *body()const
    {
        for(auto *w:ImGui::GetCurrentContext()->Windows)
            if(w->Active&&std::string(w->Name).find(legacy?"PlayerDisplayReviewBody":"PresentationReviewBody")!=std::string::npos)return w;
        throw std::runtime_error("Observed review scrolling body missing");
    }
    ImageDraw imageDraw(bool whollyVisible=true)const
    {
        auto *w=body();const auto *draw=w->DrawList;
        for(const auto &command:draw->CmdBuffer)if(!command.UserCallback&&command.ElemCount&&command.GetTexID()&&(!image||command.GetTexID()==image)){
            const auto id=command.GetTexID();
            if(!image){
                if(!textureAlive(id))continue;
                GLint prior=0,width=0,height=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&prior);
                glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(id));
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
                glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(prior));checkGl("popup texture discovery");
                if(width!=4096||height!=4096)continue;
            }
            ImRect bounds(ImVec2(FLT_MAX,FLT_MAX),ImVec2(-FLT_MAX,-FLT_MAX));
            for(unsigned n=0;n<command.ElemCount;++n)bounds.Add(draw->VtxBuffer[draw->IdxBuffer[command.IdxOffset+n]+command.VtxOffset].pos);
            const ImRect clip(ImVec2(command.ClipRect.x,command.ClipRect.y),ImVec2(command.ClipRect.z,command.ClipRect.w));
            const bool visible=whollyVisible?(clip.Contains(bounds)&&w->InnerClipRect.Contains(bounds)):
                (clip.Overlaps(bounds)&&w->InnerClipRect.Overlaps(bounds));
            if(bounds.GetWidth()>32&&bounds.GetHeight()>32&&visible)return {id,bounds,w->Viewport};
        }
        return {};
    }
    void checkMaps(bool pixels=false)
    {
        for(const char *name:{"ReviewGmMap","ReviewPlayerMap"})
        {
            ImGuiWindow *found=nullptr;
            for(auto *window:ImGui::GetCurrentContext()->Windows)
                if(window->Active&&std::string(window->Name).find(name)!=std::string::npos)found=window;
            require(found&&!found->Hidden&&!found->SkipItems&&found->DrawList->VtxBuffer.Size>100,
                std::string("Actual popup has no substantive map preview: ")+name);
            auto bounds=found->InnerRect;bounds.ClipWith(found->InnerClipRect);
            require(bounds.GetWidth()>100&&bounds.GetHeight()>100&&found->Viewport==ImGui::GetMainViewport(),
                "Private map comparison is clipped or not on the GM viewport");
            if(pixels){std::set<unsigned> colors;
                for(int y=1;y<=5;++y)for(int x=1;x<=5;++x){const auto c=readPixel(found->Viewport,
                    ImVec2(bounds.Min.x+bounds.GetWidth()*x/6,bounds.Min.y+bounds.GetHeight()*y/6),GL_BACK);
                    colors.insert((unsigned(c[0]/16)<<8)|(unsigned(c[1]/16)<<4)|unsigned(c[2]/16));}
                require(colors.size()>=3,"Actual popup map back-buffer samples are blank/uniform");}
        }
    }
    bool preparationReady()
    {
        require(frames<600,"Actual popup preparation exceeded its bounded frame budget");
        if(owner().preparing()) {
            ++preparingFrames;readyFrames=0;
            if(phase==4)require(textureAlive(image),"Pending refresh destroyed earlier reviewed pixels");
            return false;
        }
        require(owner().prepared()&&!owner().stale(),"Actual popup preparation failed: "+owner().error());
        if(++readyFrames==1) {
            require(metrics.imageStages.values[std::size_t(gm::image_texture::Origin::Frozen)]
                [std::size_t(gm::image_texture::Stage::DecodeOrientation)].count==imageDecodesAtRequest&&
                metrics.imageStages.values[std::size_t(gm::image_texture::Origin::Frozen)]
                [std::size_t(gm::image_texture::Stage::UploadSubmit)].count==imageUploadsAtRequest,
                "Review adoption also decoded/uploaded a handout instead of deferring first visible load");
            const auto key=std::string(legacy?"legacy":"editor")+(phase==1?"_open":"_refresh");
            metrics.facts[key+"_to_ready_wall_ms"]=std::to_string(milliseconds(Clock::now()-preparationStarted));
            metrics.facts[key+"_preparing_frames_observed"]=std::to_string(preparingFrames);
            metrics.facts[key+"_adoption_image_decode_count"]="0";
            metrics.facts[key+"_adoption_image_upload_count"]="0";
            metrics.facts["popup_preparation_timing_scope"]="Queued actual button release to first observed ready result; asynchronous elapsed time, not input blocking. *_popup_preparing frame rows measure frame wall time during work. Map/asset/render finalization costs remain in frame rows. Stable geometry/retirement checks wait three ready frames.";
        }
        return readyFrames>=3;
    }
    void after(std::uint64_t frame,double renderMs)
    {
        if(phase==17){confirmation->after(*this,frame,renderMs);
            if(confirmation->complete()){replaceReviewFile(false);confirmation.reset();PlayerReviewUiProbe::close(editor);legacy=true;phase=0;frames=0;}
            return;}
        ++frames;
        if(phase==1)
        {
            if(!preparationReady()||frames<4)return;
            // The post-UI pump may upload after this frame's draw list was
            // built. Start loaded idle samples only once the actual quad is
            // submitted too; no forced lookup or synchronous load here.
            // A prior popup's deleted GL name may have been reused. Initial
            // discovery must validate dimensions, not bind to that old name.
            image=0;const auto visible=imageDraw(false);const auto readyStats=imageTextureCacheStats();
            if(readyStats.scopedEntries!=readyStats.scopedLoadedTextures||
                readyStats.scopedLoadedTextures!=(visible.texture?1u:0u))return;
            auto &o=owner();require(o.prepared()&&o.gmMap()&&o.playerMap()&&o.review().playerHandouts.size()==1,"Actual popup did not capture complete review");
            const auto containsPrivate=[](const auto &features){return std::any_of(features.begin(),features.end(),[](const auto &f){return f.id=="native-private-review-road";});};
            const auto planPrivate=[](const auto &plan){return std::any_of(plan.primitives.begin(),plan.primitives.end(),[](const auto &p){return p.sourceId=="native-private-review-road";});};
            require(containsPrivate(o.gmMap()->generation.graph.features)&&!containsPrivate(o.playerMap()->generation.graph.features)&&
                planPrivate(o.review().gmMapPlan)&&!planPrivate(o.review().playerMapPlan),"GM/player comparison lost its explicit private geometry contrast");
            auto *modal=ImGui::GetTopMostPopupModal();require(modal&&modal->Viewport==ImGui::GetMainViewport(),"Review escaped GM viewport");
            metrics.facts[std::string(legacy?"legacy":"editor")+"_popup_bounds"]=
                std::to_string(modal->Pos.x)+","+std::to_string(modal->Pos.y)+" "+std::to_string(modal->Size.x)+"x"+std::to_string(modal->Size.y);
            metrics.facts["popup_actual_font_scale"]=std::to_string(ImGui::GetStyle().FontScaleMain);
            checkMaps(true);generation=o.generation();gmMap=o.gmMap();playerMap=o.playerMap();
            builds=count(PlayerDisplayReview::Stage::Build);filters=count(PlayerDisplayReview::Stage::PreviewFilter);
            mapVisibleImageCount=visible.texture?1:0;
            requireCounts(mapVisibleImageCount,"map-visible review matches actual handout visibility");
            image=visible.texture;
            metrics.facts[std::string(legacy?"legacy":"editor")+"_map_phase_handout_visibility"]=visible.texture?
                "Handout quad intersects the actual body clip; deferred visible load is expected.":
                "No handout quad intersects the body clip and no scoped texture is loaded.";
            if(visible.texture){
                const auto rect=[](const ImRect &r){return std::to_string(r.Min.x)+","+std::to_string(r.Min.y)+" -> "+
                    std::to_string(r.Max.x)+","+std::to_string(r.Max.y);};
                metrics.facts[std::string(legacy?"legacy":"editor")+"_map_phase_handout_bounds"]=
                    "quad="+rect(visible.bounds)+"; body_clip="+rect(body()->InnerClipRect)+
                    "; wholly_visible="+(imageDraw().texture?"true":"false");}
            phase=2;frames=0;
        }
        else if(phase==2)
        {
            require(owner().generation()==generation&&owner().gmMap()==gmMap&&owner().playerMap()==playerMap&&
                count(PlayerDisplayReview::Stage::Build)==builds&&count(PlayerDisplayReview::Stage::PreviewFilter)==filters,
                "Idle actual popup rebuilt or replaced its reviewed inputs");
            requireCounts(mapVisibleImageCount,"map-visible review retained its observed image count");
            checkMaps();if(frames==300){metrics.checks.push_back(std::string(legacy?"legacy":"editor")+": 300 map-visible popup frames; positive back-buffer map samples, observed handout visibility, no repeated image work or extra builds/filters");phase=7;frames=0;}
        }
        else if(phase==7)
        {
            require(frames<600,"Could not scroll/load actual review image fully into view");
            if(const auto submitted=imageDraw();submitted.texture){image=submitted.texture;requireCounts(1,"visible review image");phase=8;frames=0;}
        }
        else if(phase==8&&frames>=3)
        {
            const auto imageQuad=imageDraw();require(imageQuad.texture==image,"Review handout did not remain wholly visible");
            positivePixels(imageQuad,GL_BACK);phase=9;frames=0;
        }
        else if(phase==9)
        {
            require(imageDraw().texture==image&&owner().generation()==generation&&count(PlayerDisplayReview::Stage::Build)==builds&&
                count(PlayerDisplayReview::Stage::PreviewFilter)==filters,"Visible image idle changed owner, rebuilt review or stopped submitting the exact quad");
            if(frames==300){metrics.checks.push_back(std::string(legacy?"legacy":"editor")+": 300 wholly-visible4096 handout popup frames; four GL_BACK quadrant colors verified before timing");phase=10;frames=0;}
        }
        else if(phase==10&&frames>=3){positivePixels(imageDraw(),GL_BACK);
            require(owner().review().playerHandouts[0].contentHash==fixture.originalHash,"Held review silently changed its approved hash");phase=11;frames=0;}
        else if(phase==12&&frames>=4){
            const auto &display=editor.currentCampaign().localPlayerDisplay;
            require(owner().prepared()&&owner().stale()&&ImGui::GetTopMostPopupModal()&&!display.enabled&&!display.published,
                "Actual confirmation accepted same-size/mtime changed bytes or dismissed the frozen review");
            require(textureAlive(image),"Failed confirmation destroyed the frozen review image");
            builds=count(PlayerDisplayReview::Stage::Build);filters=count(PlayerDisplayReview::Stage::PreviewFilter);
            metrics.checks.push_back(std::string(legacy?"legacy":"editor")+": held review retained original GL_BACK pixels; actual arm confirmation refused same-size/mtime source replacement");
            phase=15;frames=0;}
        else if(phase==16&&frames>=3){require(owner().stale()&&!editor.currentCampaign().localPlayerDisplay.enabled&&
            count(PlayerDisplayReview::Stage::Build)==builds,"Known-stale disabled confirm retried or changed output");phase=3;frames=0;}
        else if(phase==4)
        {
            if(!preparationReady()||frames<4)return;
            require(refreshRetirementObserved,"Refresh retirement was not observed before the replacement load");
            require(owner().generation()==generation+1&&count(PlayerDisplayReview::Stage::Build)==builds+1&&
                count(PlayerDisplayReview::Stage::PreviewFilter)==filters+1,"Actual Refresh did not capture exactly once");
            require(owner().review().playerHandouts[0].contentHash==fixture.replacementHash,"Explicit Refresh did not bind changed source bytes");
            const auto oldName=image;image=0;const auto replacement=imageDraw(false);
            if(replacement.texture)metrics.facts[std::string(legacy?"legacy":"editor")+"_refresh_reused_deleted_gl_name"]=
                replacement.texture==oldName?"true":"false";
            phase=13;frames=0;
        }
        else if(phase==13){require(frames<600,"Refreshed review image never became wholly visible and loaded");
            if(const auto submitted=imageDraw();submitted.texture){image=submitted.texture;requireCounts(1,"refreshed visible review image");phase=14;frames=0;}}
        else if(phase==14&&frames>=3){positivePixels(imageDraw(),GL_BACK,1);metrics.checks.push_back(std::string(legacy?"legacy":"editor")+": explicit refresh displayed four changed image quadrants");phase=5;frames=0;}
        else if(phase==6&&frames>=4)
        {
            require(!owner().prepared()&&!ImGui::GetTopMostPopupModal(),"Actual Cancel retained popup owner");requireCounts(0,"review popup retirement");
            if(!legacy){confirmation.emplace(*this,noSnapshots,sentinel);phase=17;frames=0;}
            else {replaceReviewFile(false);legacy=false;complete=true;}
        }
    }
};
}
