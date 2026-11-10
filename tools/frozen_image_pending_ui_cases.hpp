void frozenPendingSlotUi(bool de)
{
    using namespace frozen_decode_test;HeadlessPanel ui(de,ImVec2(900,700),1.5f,18.f);ui.context("Frozen async pending slot");
    Fixture fixture;auto asset=fixture.asset();auto gate=std::make_shared<Gate>();auto decoder=std::make_shared<Decoder>();
    auto worker=std::make_shared<FrozenImageDecodeWorker>(options(decoder,gate));Release release{gate};
    std::size_t lookups=0,uploads=0;ImTextureID image=0;ImRect slot,clip;bool visible=false,submitted=false;int glyphVertices=0;
    ScopedImageTextureCache cache([](const auto &,const auto &,auto &)->bool{throw std::runtime_error("Async slot called synchronous loader");},
        [](auto &texture){texture={};},worker,[&](const LoadedRgbaImage &pixels,CachedImageTexture &texture){
            ++uploads;texture.width=pixels.width;texture.height=pixels.height;texture.textureId=image=static_cast<ImTextureID>(0xA510);return true;});
    float top=0,bottom=0;ui.draw=[&]{
        ImGui::BeginChild("AsyncReviewedSlot",ImVec2(480,320));auto *w=ImGui::GetCurrentWindow();clip=w->InnerClipRect;
        ImGui::Dummy(ImVec2(1,600));const int firstVertex=w->DrawList->VtxBuffer.Size;
        gm::player_review_ui::drawHandoutImage(asset,[&](const auto &a)->const CachedImageTexture &{++lookups;return cache.getAsync(a,ImGui::GetFrameCount());});
        slot=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());visible=ImGui::IsItemVisible();glyphVertices=w->DrawList->VtxBuffer.Size-firstVertex;
        top=slot.Min.y-w->Pos.y+w->Scroll.y;bottom=ImGui::GetCursorScreenPos().y-w->Pos.y+w->Scroll.y;submitted=false;
        for(const auto &cmd:w->DrawList->CmdBuffer)if(image&&cmd.GetTexID()==image&&cmd.ElemCount)submitted=true;
        ImGui::Button("AFTER_ASYNC_SLOT");ImGui::EndChild();cache.processAsync(ImGui::GetFrameCount());};
    ui.frame(4);const auto initialTop=top,initialBottom=bottom;
    const auto stable=[&]{require(std::abs(slot.GetHeight()-220)<.01f&&std::abs(top-initialTop)<.01f&&std::abs(bottom-initialBottom)<.01f,
        "Pending/ready frozen slot moved subsequent controls");};
    require(!visible&&!lookups&&!uploads&&worker->stats().started==0,"Clipped async slot admitted worker/renderer work");
    ImGui::GetIO().AddMousePosEvent(clip.GetCenter().x,clip.GetCenter().y);ui.frame();
    for(int step=0;step<20&&!visible;++step){ImGui::GetIO().AddMouseWheelEvent(0,-1);ui.frame();stable();}
    require(visible&&lookups==1&&!submitted&&glyphVertices>0&&uploads==0,"Visible pending slot lacked status glyphs or submitted an image");entered(gate);
    ui.frame(4);stable();require(glyphVertices>0&&!submitted&&uploads==0&&!gate->opened,"Pending slot blocked UI or lost its rendered loading status");
    gate->open();until([&]{ui.frame();return submitted;},"Actual visible slot never submitted the uploaded image");stable();
    require(uploads==1&&decoder->calls==1,"Async visible slot repeated worker/upload work");
    ui.compactClick("AFTER_ASYNC_SLOT");stable();
    const auto previousLookups=lookups;fixture.generation.reset();ui.frame(2);stable();
    require(lookups==previousLookups&&!submitted&&glyphVertices>0,"Expired slot loaded pixels or lost unavailable status");
    cache.shutdownAsync();cache.collectExpired();require(cache.stats().entries==0,"Async UI fixture retained expired texture entries");
}
// Real recap controls, lookup-only global cache and NO upload pump. This is
// deliberately a final one-off case: shutdown precedes ImGui destruction and
// later tests must not expect to restart the process-global graphics worker.
void frozenUnseenRecapUi()
{
    HeadlessPanel ui(false,ImVec2(1280,720),1.5f,18.f);ui.context("Actual recap pending images block export");
    struct Shutdown{~Shutdown(){shutdownImageDecodeWorker();}} shutdown;
    SessionMenuFixture f;SessionOutcomesPanel panel;int saves=0;
    auto &session=f.campaign.sessions.front();SessionOutcomes::Ledger ledger;ledger.publicTitle="Frozen recap";ledger.publicIntro="Reviewed public introduction";
    SessionOutcomes::write(session,ledger);
    const auto folder=ProjectPackageLayout::layoutForCampaign(f.campaign).assetsDirectory/"Handouts";
    std::filesystem::create_directories(folder);const auto path=folder/"pending.png";
    {std::ofstream out(path,std::ios::binary);out<<"UNSEEN_IMAGE_BYTES";require(bool(out),"Recap image fixture write failed");}
    HandoutModel handout;handout.title="Pending selected image";handout.filePath=std::filesystem::relative(path,ApplicationPaths::writableRoot()).generic_u8string();
    f.campaign.handouts.push_back(handout);auto &display=f.campaign.localPlayerDisplay;display.mapOnly=false;display.showPublicHandouts=true;display.publicHandoutIds={handout.metadata.id};
    const auto counts=imageTextureCacheStats();ui.draw=[&]{panel.draw(f.campaign,f.session,[&](Campaign &){++saves;return true;});};ui.frame(3);
    ui.compactClick("Player recap");ui.compactClick("Choose public handouts (optional)");
    ui.compactClick((handout.title+"##"+handout.metadata.id).c_str());ui.compactClick("Review player recap");
    const auto afterReview=imageTextureCacheStats();require(afterReview.scopedLoadedTextures==counts.scopedLoadedTextures,
        "Recap review synchronously loaded an unseen image");
    ui.compactReach("Export reviewed Markdown and HTML");ui.reviewState("Export reviewed Markdown and HTML",true);
    require(contains(ui.rendered,"View every selected image and wait for it to load before exporting.")&&saves==0&&
        imageTextureCacheStats().scopedLoadedTextures==counts.scopedLoadedTextures,"Unseen/pending recap image authorized export or invoked a renderer");
    require(!std::filesystem::exists(ProjectPackageLayout::layoutForCampaign(f.campaign).exportsDirectory/"PlayerRecaps"),"Disabled recap export wrote output");
}
