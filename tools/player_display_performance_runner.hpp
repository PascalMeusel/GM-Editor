#pragma once
#include "player_display_performance_render.hpp"
#include "player_display_performance_popup.hpp"
#include "player_display_performance_durability.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Localization/LocalizationService.hpp"

namespace player_perf
{
enum class Stage { Boot, Reviews, Arm, Armed, Publish, Cold, Recap, RecapRetire, ReviewOwner, ReviewOwnerRetire, Warm, Steady, Blackout, Retire,
                   RevocationWarm, Replace, RevocationWait, RevocationRetire, Final, Done };
inline const char *label(Stage stage)
{
    switch(stage)
    {
    case Stage::Reviews:return "actual_review_popups";
    case Stage::Boot:return "initial_warmup"; case Stage::Arm:return "review_arm"; case Stage::Armed:return "armed_blackout_control";
    case Stage::Publish:return "cold_publication"; case Stage::Cold:return "cold_render_control";
    case Stage::Recap:return "recap_owner_probe"; case Stage::RecapRetire:return "recap_retirement_control";
    case Stage::ReviewOwner:return "review_owner_overlap_probe";case Stage::ReviewOwnerRetire:return "review_owner_retirement_control";
    case Stage::Warm:return "recorded_warmup"; case Stage::Steady:return "steady";
    case Stage::Blackout:return "blackout_control"; case Stage::Retire:return "display_retirement_control";
    case Stage::RevocationWarm:return "revocation_warmup"; case Stage::Replace:return "same_metadata_replacement";
    case Stage::RevocationWait:return "revocation_wait_control"; case Stage::RevocationRetire:return "revocation_retirement_control";
    case Stage::Final:return "final_cleanup"; case Stage::Done:return "done";
    }
    return "unknown";
}
struct Runner
{
    Fixture &fixture; Metrics &metrics; bool diagnostic=false, noSnapshots=false, complete=false;
    bool reviewOwned=false;
    Stage stage=Stage::Boot; int cycle=0, stageFrames=0;
    ImTextureID displayTexture=0, recapTexture=0, reviewTexture=0, sentinel=0;
    std::unique_ptr<PlayerRecapPackage> recap;
    std::unique_ptr<PopupExercise> popup;
    std::unique_ptr<PlayerReviewSession> overlappingReview;
    PlayerDisplayReviewSnapshot review;
    Clock::time_point began=Clock::now(), frameStarted{}, changedAt{};
    Clock::time_point coldStarted{};
    std::optional<Frame> sample;
    gm::image_texture::Measurements priorImageStages;
    std::optional<DurabilityReceipt> durability;
    bool persistedEnabled=false;
    void next(Stage value) { stage=value; stageFrames=0; }
    std::string context() const { return "phase="+std::string(label(stage))+" cycle="+std::to_string(cycle)+
        " display_texture="+std::to_string(static_cast<unsigned long long>(displayTexture)); }
    void initialized(Application &app)
    {
        auto &save=ProjectSaveService::getInstance();const auto defaults=save.config();
        require(defaults.backupSnapshotsEnabled&&defaults.recoverySnapshotsEnabled&&defaults.maxBackupSnapshots==10&&defaults.maxRecoverySnapshots==5,
            "Fresh isolated configuration does not match default backup/recovery retention10/5");
        if(noSnapshots){save.setBackupSnapshotsEnabled(false);save.setRecoverySnapshotsEnabled(false);}
        metrics.facts["initial_snapshot_defaults"]="asserted enabled/enabled; backup retention10, recovery retention5; no retention overrides";
        metrics.facts["durability_readback"]="Every measured successful default save: a separate control frame opens both snapshots read-only before the next save/pruning. Actual-button readback waits until black or first-image pixels are observed; API cycles use the next control frame. Integrity/FK/isolated campaign/exact pre-post description and raw enabled/map checked. JSONL hashes are FNV1a64; save API timings exclude verification, control frame timings include it.";
        metrics.facts["durability"]=noSnapshots?"diagnostic: pre-save backups and recovery snapshots disabled":"default: pre-save backups and recovery snapshots enabled";
        metrics.facts["review_stage_timings"]="review-stages.csv: caller-owned counts/totals; inclusive stages overlap, never sum inclusive and child stages";
        metrics.facts["image_stage_timings"]="image-texture-stages.csv totals and image-texture-frames.csv deltas. Ordinary and synchronous Frozen calls measure nonoverlapping renderer-thread hash/read, path-loader decode (including encoded reread/header/EXIF), and GL upload submission. frozen_worker_receipt instead reports background immutable-buffer hash/read and memory decode, received later on a pump frame; those durations are not UI-frame execution and must not be added to its wall time. Excludes publication capture, procedural fog/atlases and retirement. No added GL synchronization; upload is CPU submission latency, not GPU completion. Totals include initialization; frame deltas begin at first beforeFrame.";
        metrics.facts["popup_scope"]="Actual Editor panel plus production legacy GM controls in a QA host; each has separate300-frame map-visible and image-visible arrangements. Positive map GL_BACK samples, private plan/filter contrast, four image quadrants; held same-metadata replacement, actual refused arm, explicit refresh/changed quadrants/cancel. After Editor Cancel, additional actual Editor Open/Show to players/Blackout/Close controls verify changed detached pixels and durable saves; legacy retains its original cancellation-only scope. Popup/cold performance separate, no acceptance claim.";
        metrics.facts["button_observation_scope"]="button-controls.csv records exact observed control ID, queued release frame and response on that frame. Timings include application work, not isolated callback time. button-player-pixels.csv separately records first verified detached GL_FRONT pixel frame; timestamps precede diagnostic readbacks, afterRender follows secondary swap. No compositor/human-visible latency claim. Snapshot verification is deferred until after black/first-image observation and before the next save. Existing API cycles stay separately labelled.";
        metrics.facts["button_review_stage_scope"]="button-review-stages.csv subtracts owned copies of all review measurements immediately before queued release and at its afterRender response. Inclusive stages overlap; never sum them. Confirmation ownership measures compact data copies; controller confirmation includes local destruction, while UI confirmation also includes successful review-owner clearing. Neither includes preceding review rendering or subsequent rendering/swap. These stages are distinct from complete input-frame/render latency.";
        metrics.facts["frozen_worker_timing_scope"]="frozen_worker_receipt image samples are owned background hash/decode durations merged on the receiving pump frame, not work executed by that UI frame. Cancelled work with no delivered result may omit partial cost. Frozen origin remains actual renderer-thread synchronous API probes and upload submission. Pending frames are separate from the300fully-loaded visible image samples.";
        metrics.facts["review_overlap_scope"]="Production PlayerReviewSession clear + real scoped GL retirement while approved detached output stays visible; owner/cache probe, not a second popup navigation walkthrough over published output.";
        gm::localization::LocalizationService::instance().setPreference(gm::localization::LocalePreference::English,false);
        CampaignRepository::saveCampaign(fixture.campaign); ApplicationQaHarness::load(app,fixture.campaign);
        PlayerReviewUiProbe::runtimeMeasurements(ApplicationQaHarness::editor(app),&metrics.reviewStages);
        ImGui::GetIO().IniFilename=nullptr;
        ImGui::GetPlatformIO().Platform_ShowWindow=[](ImGuiViewport *){};
        ImGui::GetPlatformIO().Platform_SetWindowFocus=[](ImGuiViewport *){};
        GLint maximum=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum); checkGl("initial GL capability");
        require(maximum>=4096,"Required 4096-square workload unsupported: GL_MAX_TEXTURE_SIZE="+std::to_string(maximum));
        metrics.facts["gl_max_texture_size"]=std::to_string(maximum);
        for(const auto &[name,query]:std::array<std::pair<const char *,GLenum>,3>{{{"gl_vendor",GL_VENDOR},{"gl_renderer",GL_RENDERER},{"gl_version",GL_VERSION}}})
            metrics.facts[name]=reinterpret_cast<const char *>(glGetString(query));
        metrics.time("ordinary_sentinel_load",-1,[&] {
            const auto &texture=getCachedImageTexture(fixture.sentinel); require(texture.loaded,"Ordinary sentinel did not load"); sentinel=texture.textureId;
        });
        checkGl("sentinel upload"); requireCounts(0,"initial blackout");
        require(WorkspaceInteractionProbe::silent(ApplicationQaHarness::editor(app)),"Fixture unexpectedly started audio");
        metrics.facts["swap_intervals_source_settings"]="main1; secondary0; actual compositor presentation unverified";
        metrics.facts["popup_locale_text_scale"]="English; application default UI font scale; playerTextScale=1";
        metrics.facts["timing_definition"]="beforeFrame entry to next beforeFrame entry; includes UI, viewport renders/swaps, collection, backend input and harness checks";
        metrics.facts["gpu_synchronization"]=diagnostic ? "diagnostic glFinish on every owned viewport; separate from default run" : "ordinary application behavior; no GPU completion claim";
        metrics.facts["cold_measurements"]="All control operations and first submitted-image frame reported separately; no cold responsiveness gate passed";
        metrics.facts["cache_order"]="Popup exercises precede publication. Cold means first submission for each new scoped owner; filesystem/decoder/driver caches may already be warm, not a cold-machine comparison.";
        metrics.facts["first_submitted_image_interval"]="beforeFrame to afterRender; includes main-thread callback review/publication, rendering and upload submission on that frame, if any; background hash/decode is reported separately through worker receipts and may precede this frame. Excludes main swap/collection.";
        metrics.facts["review_owner_mode"]=reviewOwned?"private-session-owner":"external-review-snapshot";
        metrics.facts["successful_confirmation_scope"]=reviewOwned?
            "Subsequent API cycles: production private PlayerReviewSession preparation and Editor confirmation API with that exact owner's review reference. Owner is cleared after the call. Actual button results are separately recorded in button-controls.csv.":
            "Subsequent API cycles: production Editor review builder and confirmation API with an external snapshot. No private review seed; comparable to original baseline. Actual button results are separately recorded in button-controls.csv.";
        metrics.facts["blackout_pixel_scope"]="Single interior center-pixel smoke paired with prior handout/map positive controls; not whole-frame privacy proof";
        began=Clock::now();
    }
    void before(Application &app, std::uint64_t frame)
    {
        const auto now=Clock::now();
        if(sample)
        {
            sample->wall=milliseconds(now-frameStarted); const auto stats=imageTextureCacheStats();
            sample->scopedEntries=stats.scopedEntries; sample->scopedLoaded=stats.scopedLoadedTextures;
            for(std::size_t origin=0;origin<metrics.imageStages.values.size();++origin)
                for(std::size_t stage=0;stage<metrics.imageStages.values[origin].size();++stage)
                {
                    const auto &total=metrics.imageStages.values[origin][stage];
                    const auto &prior=priorImageStages.values[origin][stage];
                    sample->imageStages.values[origin][stage]={total.count-prior.count,total.milliseconds-prior.milliseconds};
                    if(sample->phase=="steady"||sample->phase.find("_popup_map_idle")!=std::string::npos||sample->phase.find("_popup_image_idle")!=std::string::npos)
                        require(total.count==prior.count,"Immutable idle cache unexpectedly performed image cold work");
                }
            metrics.frames.push_back(*sample);
        }
        priorImageStages=metrics.imageStages;
        require(now-began<std::chrono::seconds(240),"240-second run watchdog expired before all phases completed");
        sample=Frame{frame,cycle,popup?popup->label():label(stage)}; frameStarted=now;
        auto &editor=ApplicationQaHarness::editor(app);
        if(durability){sample->phase="snapshot_readback_control";
            metrics.time("snapshot_readback_outside_save",cycle,[&]{durability->verify(fixture,metrics);});durability.reset();}
        if(stage==Stage::Reviews){popup->before(frame);return;}
        if(stage==Stage::Arm || stage==Stage::Publish)
        {
            const std::string operation=stage==Stage::Arm?"arm":"publish";
            const auto beforeDescription=editor.currentCampaign().description;
            auto &campaign=PlayerReviewUiProbe::campaign(editor);
            campaign.description="DURABILITY_"+std::to_string(cycle)+"_"+operation;
            auto &privateOwner=PlayerReviewUiProbe::owner(editor);
            metrics.time("review_construction",cycle,[&] {
                if(reviewOwned) {
                    privateOwner.setMeasurements(&metrics.reviewStages);
                    require(privateOwner.prepare(campaign,PlayerDisplayReview::currentLocaleCode()),
                        "Production private owner could not prepare confirmation");
                } else review=editor.buildLocalPlayerDisplayReview(&metrics.reviewStages);
            });
            const auto &confirmationReview=reviewOwned?privateOwner.review():review;
            const auto heldMemo=reviewOwned?privateOwner.compileMemo():nullptr;
            const auto beforeMemo=heldMemo?heldMemo->stats():gm::map_render::MapRenderCompileMemoStats{};
            if(reviewOwned) require(heldMemo && !heldMemo->empty(),"Prepared owner did not retain a map compilation");
            require(confirmationReview.eligible && confirmationReview.hasMapPreview && confirmationReview.playerHandouts.size()==1 &&
                confirmationReview.playerHandouts.front().assetId==fixture.handoutId,"Real review lacks expected eligible map/one handout");
            if(stage==Stage::Arm)
            {
                metrics.time("arm_and_save",cycle,[&] { require(editor.armLocalPlayerDisplayFromReview(confirmationReview,&metrics.reviewStages),"Actual Editor arm/save failed"); });
                const auto &display=editor.currentCampaign().localPlayerDisplay;
                require(display.enabled && display.blackout && !display.published,"Arming did not retain blackout"); next(Stage::Armed);
            }
            else
            {
                metrics.time("publish_save_capture",cycle,[&] { require(editor.publishLocalPlayerDisplay(confirmationReview,&metrics.reviewStages),"Actual Editor publish/save/capture failed"); });
                coldStarted=Clock::now();next(Stage::Cold);
            }
            if(reviewOwned) {
                const auto afterMemo=heldMemo->stats();
                require(afterMemo.hits>beforeMemo.hits,"Private-owner confirmation did not reuse exact map geometry");
                const auto key="memo_cycle"+std::to_string(cycle)+"_"+operation;
                metrics.facts[key]="before hits="+std::to_string(beforeMemo.hits)+" misses="+std::to_string(beforeMemo.misses)+
                    " bypasses="+std::to_string(beforeMemo.bypasses)+"; after hits="+std::to_string(afterMemo.hits)+
                    " misses="+std::to_string(afterMemo.misses)+" bypasses="+std::to_string(afterMemo.bypasses);
                privateOwner.clear();
            }
            if(!noSnapshots)durability=DurabilityReceipt{ProjectSaveService::getInstance().state(),beforeDescription,
                editor.currentCampaign().description,operation,persistedEnabled,cycle};
            persistedEnabled=true;
        }
        else if(stage==Stage::Recap)
        {
            recap=std::make_unique<PlayerRecapPackage>();
            metrics.time("recap_review_capture",cycle,[&] {
                const bool reviewed=recap->review(editor.currentCampaign(),fixture.sessionId,{fixture.handoutId});
                require(reviewed,"Recap owner review: "+recap->error());
            });
            metrics.time("recap_first_hash_decode_upload",cycle,[&] {
                const auto &texture=getCachedImageTexture(recap->image(recap->attachments().front()));
                require(texture.loaded && texture.width==4096 && texture.height==4096,"Recap cache-owner upload failed"); recapTexture=texture.textureId;
            });
            checkGl("recap upload");
        }
        else if(stage==Stage::RecapRetire)
        {
            requireCounts(1,"recap post-collection");
            require(!textureAlive(recapTexture) && textureAlive(displayTexture) && textureAlive(sentinel),"Recap retirement deleted wrong texture or retained old handle");
            metrics.checks.push_back("cycle"+std::to_string(cycle)+": recap owner retired, display and ordinary sentinel alive"); next(cycle==0?Stage::ReviewOwner:Stage::Warm);
        }
        else if(stage==Stage::ReviewOwner)
        {
            overlappingReview=std::make_unique<PlayerReviewSession>();
            metrics.time("review_owner_overlap_capture_upload",cycle,[&]{
                require(overlappingReview->prepare(editor.currentCampaign(),"en"),"Overlapping review could not freeze approved content");
                const auto &h=overlappingReview->review().playerHandouts.at(0);
                const auto &texture=getCachedImageTexture(overlappingReview->playerImage(h.projectRelativePath,h.contentHash));
                require(texture.loaded&&texture.width==4096&&texture.height==4096,"Overlapping review large texture missing");reviewTexture=texture.textureId;});
        }
        else if(stage==Stage::ReviewOwnerRetire)
        {
            requireCounts(1,"review retired while approved display lives");
            require(!textureAlive(reviewTexture)&&textureAlive(displayTexture)&&textureAlive(sentinel)&&
                localPlayerDisplayIsPublished(editor.currentCampaign().localPlayerDisplay),"Review reset retired approved display or ordinary texture");
            metrics.checks.push_back("production review owner reset retired only its texture after collection; approved display and ordinary sentinel retained");
            overlappingReview.reset();next(Stage::Warm);
        }
        else if(stage==Stage::Retire || stage==Stage::RevocationRetire)
        {
            requireCounts(0,"display post-collection");
            require(!textureAlive(displayTexture) && textureAlive(sentinel),"Display retirement retained old handle or deleted ordinary sentinel");
            require(!localPlayerDisplayIsPublished(editor.currentCampaign().localPlayerDisplay),"Retired output is still published");
            if(stage==Stage::RevocationRetire)
            {
                metrics.operations.push_back({"same_metadata_to_texture_retirement",cycle,milliseconds(now-changedAt)});
                metrics.checks.push_back("same-metadata source revocation and post-render texture retirement"); next(Stage::Final);
            }
        }
        else if(stage==Stage::Replace)
        {
            metrics.time("atomic_same_metadata_source_replacement",cycle,[&] { replaceSource(fixture); });
            metrics.facts["source_replacement"]="Prevalidated equal-size sibling with old mtime, atomically renamed; no truncated source/transient new mtime";
            changedAt=Clock::now(); next(Stage::RevocationWait);
        }
    }
    void after(Application &app, std::uint64_t frame)
    {
        sample->beforeToAfterRender=milliseconds(Clock::now()-frameStarted);
        if(diagnostic) metrics.time("diagnostic_finish_all_viewports",cycle,[] { finishAllViewports(); });
        auto &editor=ApplicationQaHarness::editor(app);
        if(stage==Stage::Reviews){popup->after(frame,sample->beforeToAfterRender);if(popup->complete){
            require(!editor.currentCampaign().localPlayerDisplay.enabled&&!persistedEnabled,"Popup walkthrough did not restore disabled API-cycle baseline");
            popup.reset();next(Stage::Arm);}return;}
        if(stage==Stage::Boot)
        { if(++stageFrames==12) { requireCounts(0,"warm initial blackout");popup=std::make_unique<PopupExercise>(editor,metrics,fixture,noSnapshots,sentinel);next(Stage::Reviews); } }
        else if(stage==Stage::Armed)
        {
            if(++stageFrames<4) return;
            blackoutPixels(); const auto *vp=detachedPlayer(); int w=0,h=0;
            glfwGetFramebufferSize(static_cast<GLFWwindow *>(vp->PlatformHandle),&w,&h);
            metrics.facts["player_framebuffer"]=std::to_string(w)+"x"+std::to_string(h);
            metrics.facts["player_dpi_scale"]=std::to_string(vp->DpiScale);
            glfwGetFramebufferSize(ApplicationQaHarness::window(app),&w,&h);
            metrics.facts["main_framebuffer"]=std::to_string(w)+"x"+std::to_string(h);
            next(Stage::Publish);
        }
        else if(stage==Stage::Cold)
        {
            checkGl("first display upload/main rendering");
            const auto draw=imageDraw();
            if(!draw.texture) { ++stageFrames;require(Clock::now()-coldStarted<std::chrono::seconds(10),
                "Published handout never completed async loading/submitted its actual4096texture"); return; }
            displayTexture=draw.texture; positivePixels(draw); mapPixels(metrics); requireCounts(1,"cold display");
            require(textureAlive(displayTexture) && textureAlive(sentinel),"Cold display or sentinel GL handle invalid");
            metrics.operations.push_back({"first_submitted_handout_before_to_after_render",cycle,sample->beforeToAfterRender});
            metrics.checks.push_back("cycle"+std::to_string(cycle)+": actual detached draw + four approved color quadrants +4096 GL texture");
            next(cycle<3 ? Stage::Recap : Stage::RevocationWarm);
        }
        else if(stage==Stage::Recap)
        {
            requireCounts(2,"overlapping recap/display");
            require(recapTexture!=displayTexture && textureAlive(recapTexture) && textureAlive(displayTexture) && textureAlive(sentinel),"Overlapping owners did not retain independent GL textures");
            positivePixels(imageDraw(displayTexture));
            metrics.time("recap_owner_release",cycle,[&] { recap.reset(); });
            require(textureAlive(recapTexture),"Recap texture deleted before post-render boundary"); next(Stage::RecapRetire);
        }
        else if(stage==Stage::ReviewOwner)
        {
            // RecapRetire changes phase in before(); capture starts in the
            // following before(), so this first after() has no new owner yet.
            if(!overlappingReview)return;
            requireCounts(2,"overlapping review and approved display");
            require(reviewTexture!=displayTexture&&textureAlive(reviewTexture)&&textureAlive(displayTexture),"Review/display scopes share a texture handle");
            positivePixels(imageDraw(displayTexture));overlappingReview->clear();
            require(textureAlive(reviewTexture),"Review clear deleted GL texture before frame collector");next(Stage::ReviewOwnerRetire);
        }
        else if(stage==Stage::Warm)
        { if(++stageFrames==20) next(Stage::Steady); }
        else if(stage==Stage::Steady)
        {
            require(imageDraw(displayTexture).texture==displayTexture,"Steady frame did not submit large handout");
            requireCounts(1,"steady display"); if(++stageFrames==300) next(Stage::Blackout);
        }
        else if(stage==Stage::Blackout)
        {
            metrics.time("blackout_after_render",cycle,[&] { editor.blackoutLocalPlayerDisplayOutput(); });
            require(textureAlive(displayTexture) && textureAlive(sentinel),"Blackout deleted texture before frame collector");
            requireCounts(1,"expired display before collector"); next(Stage::Retire);
        }
        else if(stage==Stage::Retire)
        {
            blackoutPixels(); metrics.checks.push_back("cycle"+std::to_string(cycle)+": black pixels, expired display retired after swap, ordinary sentinel retained");
            ++cycle; next(Stage::Arm);
        }
        else if(stage==Stage::RevocationWarm)
        { if(++stageFrames==20) next(Stage::Replace); }
        else if(stage==Stage::RevocationWait)
        {
            require(Clock::now()-changedAt<std::chrono::seconds(5),"Same-metadata automatic revocation exceeded five-second deadline");
            if(localPlayerDisplayIsPublished(editor.currentCampaign().localPlayerDisplay)) positivePixels(imageDraw(displayTexture));
            else
            {
                blackoutPixels(); require(!imageDraw(displayTexture).texture,"Unpublished handout still submitted");
                require(textureAlive(displayTexture),"Revocation deleted texture before collector");
                metrics.operations.push_back({"same_metadata_to_automatic_blackout",cycle,milliseconds(Clock::now()-changedAt)});
                next(Stage::RevocationRetire);
            }
        }
        else if(stage==Stage::Final)
        {
            const auto &frozen=metrics.imageStages.values[static_cast<std::size_t>(gm::image_texture::Origin::Frozen)];
            for(const auto &timing:frozen)require(timing.count>0,"Production frozen-image stage was not observed");
            const auto &worker=metrics.imageStages.values[static_cast<std::size_t>(gm::image_texture::Origin::FrozenWorker)];
            require(worker[std::size_t(gm::image_texture::Stage::HashRead)].count>0&&
                worker[std::size_t(gm::image_texture::Stage::DecodeOrientation)].count>0&&
                worker[std::size_t(gm::image_texture::Stage::UploadSubmit)].count==0,
                "Background decode receipts were absent or falsely included a worker GL upload");
            if(!noSnapshots) {
                const auto &state=ProjectSaveService::getInstance().state();
                require(state.lastBackupError.empty()&&state.lastRecoveryError.empty()&&
                    std::filesystem::is_regular_file(std::filesystem::u8path(state.lastBackupPath))&&
                    std::filesystem::is_regular_file(std::filesystem::u8path(state.lastRecoverySnapshotPath)),
                    "Default durability mode did not leave successful backup and recovery files");
                metrics.checks.push_back("default durability: backup and recovery enabled, successful snapshot files exist");
            }
            blackoutPixels(); requireCounts(0,"final cleanup"); require(textureAlive(sentinel),"Final cleanup deleted ordinary sentinel");
            require(WorkspaceInteractionProbe::silent(editor),"Audio became active during fixture"); checkGl("final validation");
            metrics.checks.push_back("final zero scoped textures; ordinary sentinel retained; no audio");
            complete=true; next(Stage::Done); glfwSetWindowShouldClose(ApplicationQaHarness::window(app),GLFW_TRUE);
        }
    }
};
}
