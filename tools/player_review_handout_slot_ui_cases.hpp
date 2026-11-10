// The exact production handout-slot renderer with its real scoped cache and
// fake renderer callbacks. No file reads, image decoding, GL, or native input.
void playerReviewHandoutSlotUi(bool de)
{
    HeadlessPanel ui(de,ImVec2(900,700),1.5f,18.f);
    ui.context(de?"Deferred reviewed handout slot DE":"Deferred reviewed handout slot EN");
    std::size_t lookups=0,loads=0,deletes=0;ImTextureID lastTexture=0;
    ScopedImageTextureCache cache([&](const auto &path,const auto &hash,CachedImageTexture &texture) {
        require(!path.empty()&&!hash.empty(),"Invalid reviewed asset reached fake renderer");++loads;
        texture.width=400;texture.height=800;texture.textureId=static_cast<ImTextureID>(0xF120+loads);
        lastTexture=texture.textureId;return hash!="failed";
    },[&](CachedImageTexture &texture){if(texture.textureId)++deletes;texture={};});
    auto generation=std::make_shared<const FrozenImageAssetGeneration>();
    FrozenImageAsset asset("synthetic-never-read.png","reviewed",generation);
    ImRect slot,clip;float contentTop=0,contentBottom=0;bool visible=false,submitted=false;
    ui.draw=[&] {
        ImGui::BeginChild("ReviewedHandoutSlot",ImVec2(480,300));
        auto *body=ImGui::GetCurrentWindow();clip=body->InnerClipRect;
        ImGui::Dummy(ImVec2(1,600));
        gm::player_review_ui::drawHandoutImage(asset,[&](const FrozenImageAsset &value)->const CachedImageTexture & {
            ++lookups;return cache.getDeferred(value,static_cast<std::uint64_t>(ImGui::GetFrameCount()));
        });
        slot=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());visible=ImGui::IsItemVisible();
        contentTop=slot.Min.y-body->Pos.y+body->Scroll.y;
        contentBottom=ImGui::GetCursorScreenPos().y-body->Pos.y+body->Scroll.y;
        submitted=false;const auto *draw=body->DrawList;
        for(const auto &command:draw->CmdBuffer)if(lastTexture&&command.GetTexID()==lastTexture&&command.ElemCount) {
            submitted=true;ImRect quad(ImVec2(FLT_MAX,FLT_MAX),ImVec2(-FLT_MAX,-FLT_MAX));
            for(unsigned n=0;n<command.ElemCount;++n)
                quad.Add(draw->VtxBuffer[draw->IdxBuffer[command.IdxOffset+n]+command.VtxOffset].pos);
            require(slot.Contains(quad)&&std::abs(quad.GetWidth()/quad.GetHeight()-.5f)<.001f,
                "Production handout quad escaped its reserved slot or distorted aspect ratio");
            require(std::abs(quad.GetCenter().x-slot.GetCenter().x)<.01f&&
                std::abs(quad.GetCenter().y-slot.GetCenter().y)<.01f,"Production handout quad is not centered");
        }
        ImGui::TextUnformatted("AFTER_HANDOUT_SLOT");ImGui::EndChild();
    };
    ui.frame(4);
    require(!visible&&lookups==0&&loads==0&&cache.stats().entries==0,
        "Offscreen handout performed a lookup/load despite actual clipping");
    const float originalTop=contentTop,originalBottom=contentBottom,originalWidth=slot.GetWidth();
    const auto stable=[&] {
        require(std::abs(slot.GetHeight()-220.f)<.01f&&std::abs(slot.GetWidth()-originalWidth)<.01f&&
            std::abs(contentTop-originalTop)<.01f&&std::abs(contentBottom-originalBottom)<.01f,
            "Deferred, loaded, or unavailable handout changed reserved layout");
    };
    stable();ui.frame(5);require(lookups==0&&loads==0,"Idle offscreen frames attempted image work");
    // Reach the actual child through queued pointer/wheel input; never assign
    // scroll offsets or invoke a production loader directly from the test.
    const auto hover=clip.GetCenter();ImGui::GetIO().AddMousePosEvent(hover.x,hover.y);ui.frame();
    int firstVisible=-1;
    for(int step=0;step<20&&!visible;++step) {
        ImGui::GetIO().AddMouseWheelEvent(0,-1.f);ui.frame();stable();
        if(visible)firstVisible=ImGui::GetFrameCount();
        else require(lookups==0&&loads==0,"Fully clipped slot reached the deferred cache");
    }
    require(firstVisible>=0&&lookups==1&&loads==0&&!submitted&&cache.stats().entries==1,
        "First actually visible frame did not reserve exactly one unloaded entry");
    ui.frame();stable();require(loads==1&&submitted&&cache.stats().loadedTextures==1,
        "Next visible frame failed to load and submit the production handout quad");
    ui.frame(12);stable();require(loads==1&&submitted,"Visible idle frames repeated decode or lost image submission");
    const auto beforeExpiry=lookups;generation.reset();ui.frame(2);stable();
    require(lookups==beforeExpiry&&loads==1&&!submitted,
        "Expired handout descriptor looked up or displayed a retained texture");
    cache.collectExpired();require(cache.stats().entries==0&&deletes==1,"Expired handout did not retire after frame boundary");
    generation=std::make_shared<const FrozenImageAssetGeneration>();asset=FrozenImageAsset("synthetic-never-read.png","failed",generation);
    ui.frame();stable();require(loads==1&&!submitted,"New failed-image generation skipped deferral");
    ui.frame();stable();require(loads==2&&!submitted,"Visible failed-image attempt was not represented as unavailable");
    ui.frame(8);stable();require(loads==2&&!submitted,"Failed image retried on idle UI frames");
    generation.reset();cache.collectExpired();
    generation=std::make_shared<const FrozenImageAssetGeneration>();asset=FrozenImageAsset("synthetic-never-read.png","revoked-pending",generation);
    ui.frame();require(loads==2&&!submitted,"Pending revocation fixture loaded in adoption frame");
    const auto pendingLookups=lookups;generation.reset();ui.frame(2);stable();
    require(lookups==pendingLookups&&loads==2&&!submitted,"Revoked pending handout decoded on the following frame");
    cache.collectExpired();require(cache.stats().entries==0&&deletes==2,"Handout fixture retained renderer resources");
}
