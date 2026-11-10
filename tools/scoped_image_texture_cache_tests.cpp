#include "Views/Editor/Widgets/ScopedImageTextureCache.hpp"
#include "Views/Editor/Widgets/ImageTextureMeasurements.hpp"
#include "frozen_image_decode_test_support.hpp"
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace
{
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
using Generation = std::shared_ptr<const FrozenImageAssetGeneration>;
Generation generation() { return std::make_shared<const FrozenImageAssetGeneration>(); }
struct Renderer
{
    unsigned long long next = 1;
    std::size_t loads = 0;
    std::set<ImTextureID> deleted;
    bool load(const std::filesystem::path &path, const std::string &hash, CachedImageTexture &texture)
    {
        ++loads;
        require(!path.empty() && !hash.empty(), "Invalid descriptor reached loader");
        texture.textureId = static_cast<ImTextureID>(next++);
        texture.width = 32; texture.height = 16;
        return hash != "failed";
    }
    void destroy(CachedImageTexture &texture)
    {
        if (texture.textureId) require(deleted.insert(texture.textureId).second, "Texture deleted twice");
        texture = {};
    }
    ScopedImageTextureCache cache()
    {
        return ScopedImageTextureCache(
            [this](const auto &path, const auto &hash, auto &texture) { return load(path, hash, texture); },
            [this](auto &texture) { destroy(texture); });
    }
};

void missingExpiredAndDeferredDelete()
{
    Renderer renderer; auto cache = renderer.cache();
    auto owner = generation();
    FrozenImageAsset asset("never-opened-by-test.png", "reviewed", owner);
    require(!cache.get({}).loaded && !cache.get(FrozenImageAsset({}, "reviewed", owner)).loaded &&
        !cache.get(FrozenImageAsset("image.png", {}, owner)).loaded, "Missing descriptor loaded");
    require(renderer.loads == 0 && cache.stats().entries == 0, "Missing descriptor inserted or touched loader");
    const auto &texture = cache.get(asset); const auto queuedTextureId = texture.textureId;
    for (int frame = 0; frame != 200; ++frame)
        require(cache.get(asset).textureId == queuedTextureId, "Immutable hit replaced texture");
    auto copiedDescriptor = asset;
    require(renderer.loads == 1 && asset.lifetime().use_count() == 1, "Cache/descriptor retained strong ownership or reloaded");
    owner.reset();
    require(!copiedDescriptor.valid() && !cache.get(copiedDescriptor).loaded && renderer.loads == 1,
        "Expired descriptor invoked loader or returned stale texture");
    require(renderer.deleted.empty() && texture.textureId == queuedTextureId && cache.stats().loadedTextures == 1,
        "Expiry/lookup deleted a texture still referenced by queued draws");
    cache.collectExpired();
    require(renderer.deleted.count(queuedTextureId) == 1 && cache.stats().entries == 0, "Boundary did not retire expired texture");
    cache.collectExpired(); cache.clear();
    require(renderer.deleted.size() == 1, "Repeated collection double deleted");
    require(!cache.get(asset).loaded && cache.stats().entries == 0 && renderer.loads == 1, "Expired lookup reinserted entry");
}

void collisionsAndFailedLoads()
{
    Renderer renderer; auto cache = renderer.cache(); auto first = generation(), second = generation();
    FrozenImageAsset a("same.png", "approved", first), b("same.png", "approved", second);
    const auto old = cache.get(a).textureId, current = cache.get(b).textureId;
    require(old != current && cache.stats().entries == 2, "Same path reused an earlier generation's texture");
    FrozenImageAsset otherHash("same.png", "other-approved", second);
    require(cache.get(otherHash).textureId != current && renderer.loads == 3, "Content binding collided");
    first.reset(); cache.collectExpired();
    require(renderer.deleted.count(old) == 1 && !renderer.deleted.count(current) && cache.stats().entries == 2,
        "Expired collection touched active generation");
    FrozenImageAsset failure("failed.png", "failed", second);
    require(!cache.get(failure).loaded && cache.get(failure).loadAttempted, "Failed load was advertised as loaded");
    const auto loads = renderer.loads;
    require(!cache.get(failure).loaded && renderer.loads == loads && renderer.deleted.size() == 1,
        "Failed immutable load retried or deleted before boundary");
    cache.clear();
    require(cache.stats().entries == 0 && renderer.deleted.size() == renderer.loads, "Explicit shutdown clear leaked texture IDs");
    second.reset(); cache.collectExpired();
    require(renderer.deleted.size() == renderer.loads, "Shutdown clear was deleted twice");
}

void boundedReviewCyclesAndDestructor()
{
    Renderer renderer; auto cache = renderer.cache(); Generation owner;
    for (int review = 0; review != 200; ++review)
    {
        owner = generation();
        require(cache.get(FrozenImageAsset("map.png", "map", owner)).loaded, "Map capture did not load");
        require(cache.get(FrozenImageAsset("handout.png", "handout", owner)).loaded, "Handout capture did not load");
        require(cache.stats().entries <= 4, "More than current/prior review retained before frame boundary");
        cache.collectExpired();
        require(cache.stats().entries == 2 && cache.stats().loadedTextures == 2, "Repeated review retained expired generations");
    }
    owner.reset(); cache.collectExpired();
    require(cache.stats().entries == 0 && renderer.loads == 400 && renderer.deleted.size() == 400,
        "Repeated review/blackout cycles leaked textures");
    // A static production cache may outlive GLFW. Only explicit collection or
    // clear calls the renderer; object/static destruction must be GL-free.
    const auto before = renderer.deleted.size();
    {
        auto lateCache = renderer.cache(); auto lateOwner = generation();
        (void)lateCache.get(FrozenImageAsset("late.png", "late", lateOwner));
    }
    require(renderer.deleted.size() == before, "Cache destructor invoked renderer after context lifetime");
}

void deferredVisibleFrames()
{
    Renderer renderer;auto cache=renderer.cache();auto first=generation();
    FrozenImageAsset asset("deferred.png","reviewed",first);
    for(int caller=0;caller<20;++caller) {
        const auto &pending=cache.getDeferred(asset,100);
        require(!pending.loaded&&!pending.loadAttempted&&pending.textureId==0,
            "First visible frame decoded or advertised a deferred texture");
    }
    require(renderer.loads==0&&cache.stats().entries==1&&cache.stats().loadedTextures==0,
        "Same-frame callers bypassed deferral or duplicated the entry");
    const auto &loaded=cache.getDeferred(asset,101);const auto id=loaded.textureId;
    require(loaded.loaded&&loaded.loadAttempted&&id&&renderer.loads==1,
        "Next visible frame did not make exactly one load attempt");
    for(int frame=101;frame<130;++frame)
        require(cache.getDeferred(asset,frame).textureId==id,"Deferred hit replaced an immutable texture");
    require(renderer.loads==1,"Loaded deferred hit repeated the loader");
    auto second=generation();FrozenImageAsset replacement("deferred.png","reviewed",second);
    require(!cache.getDeferred(replacement,130).loadAttempted&&cache.getDeferred(asset,130).textureId==id&&
        renderer.loads==1&&cache.stats().entries==2,"New generation reused old texture or skipped deferral");
    const auto fresh=cache.getDeferred(replacement,131).textureId;
    require(fresh&&fresh!=id&&renderer.loads==2,"Independent deferred generation failed to load");
    first.reset();require(!cache.getDeferred(asset,132).loaded&&renderer.loads==2,
        "Expired loaded descriptor returned its cached texture");
    require(renderer.deleted.empty(),"Deferred expiry deleted before the renderer boundary");
    cache.collectExpired();require(renderer.deleted.count(id)==1&&!renderer.deleted.count(fresh),
        "Deferred collection retired the wrong owner");
    second.reset();cache.collectExpired();require(cache.stats().entries==0,"Deferred owners leaked after collection");
}

void deferredExpiryFailureAndImmediateMix()
{
    Renderer renderer;auto cache=renderer.cache();auto revoked=generation();
    FrozenImageAsset cancelled("cancelled.png","reviewed",revoked);
    require(!cache.getDeferred(cancelled,10).loadAttempted,"Revocable pending texture loaded immediately");
    revoked.reset();require(!cache.getDeferred(cancelled,11).loaded&&renderer.loads==0,
        "Revocation before the deferred frame still invoked the loader");
    cache.collectExpired();require(cache.stats().entries==0&&renderer.deleted.empty(),
        "Unloaded revoked entry was retained or deleted a nonexistent texture");
    auto owner=generation();FrozenImageAsset failed("failed.png","failed",owner);
    require(!cache.getDeferred(failed,20).loadAttempted&&renderer.loads==0,"Failure fixture was not deferred");
    require(!cache.getDeferred(failed,21).loaded&&cache.getDeferred(failed,21).loadAttempted&&renderer.loads==1,
        "Deferred failure did not make one real attempt");
    for(int frame=22;frame<30;++frame)require(!cache.getDeferred(failed,frame).loaded,"Failed load became available");
    require(!cache.get(failed).loaded&&renderer.loads==1,"Immediate/deferred failed hit retried the loader");
    FrozenImageAsset eager("eager.png","reviewed",owner);
    const auto eagerId=cache.get(eager).textureId;
    require(eagerId&&cache.getDeferred(eager,30).textureId==eagerId&&renderer.loads==2,
        "Deferred lookup delayed or reloaded an existing immediate texture");
    FrozenImageAsset mixed("mixed.png","reviewed",owner);
    require(!cache.getDeferred(mixed,30).loadAttempted&&renderer.loads==2,"Mixed fixture was not pending");
    const auto mixedId=cache.get(mixed).textureId;
    require(mixedId&&cache.getDeferred(mixed,30).textureId==mixedId&&renderer.loads==3,
        "Immediate lookup did not preserve its same-frame loading contract");
    require(!cache.getDeferred({},31).loaded&&renderer.loads==3,"Empty deferred descriptor loaded");
    owner.reset();cache.collectExpired();require(cache.stats().entries==0&&renderer.deleted.size()==3,
        "Mixed deferred/immediate cleanup lost or duplicated a resource");
}

void optionalMeasurementOwnership()
{
    using namespace gm::image_texture;
    Measurements outer, inner;
    {
        ScopedMeasurements observed(&outer);
        { Measure value(Origin::Ordinary, Stage::HashRead); }
        { ScopedMeasurements nested(&inner); Measure value(Origin::Frozen, Stage::DecodeOrientation); }
        { ScopedMeasurements disabled(nullptr); Measure value(Origin::Frozen, Stage::UploadSubmit); }
        try
        {
            ScopedMeasurements nested(&inner); Measure value(Origin::Frozen, Stage::HashRead);
            throw std::runtime_error("synthetic observer unwind");
        }
        catch(const std::runtime_error &) {}
        { Measure value(Origin::Ordinary, Stage::UploadSubmit); }
    }
    { Measure unobserved(Origin::Ordinary, Stage::DecodeOrientation); }
    require(outer.values[0][0].count==1 && outer.values[0][1].count==0 && outer.values[0][2].count==1,
        "Scoped measurement did not restore its caller-owned observer");
    require(inner.values[1][0].count==1 && inner.values[1][1].count==1 && inner.values[1][2].count==0,
        "Nested, disabled or exceptional measurement lifetime differs");
    for(const auto *sink:{&outer,&inner})for(const auto &origin:sink->values)for(const auto &value:origin)
        require(std::isfinite(value.milliseconds)&&value.milliseconds>=0,"Invalid measured duration");
    for(const auto &value:outer.values[1])require(value.count==0,"Nested/disabled frozen timing leaked into outer sink");
    for(const auto &value:inner.values[0])require(value.count==0,"Ordinary timing leaked into nested sink");
}
#include "scoped_image_texture_async_cases.hpp"
}

int main()
{
    try
    {
        missingExpiredAndDeferredDelete(); collisionsAndFailedLoads(); boundedReviewCyclesAndDestructor(); optionalMeasurementOwnership();
        deferredVisibleFrames();deferredExpiryFailureAndImmediateMix();
        asyncVisiblePumpAndFairness();asyncVisibilityRevocationAndFailure();asyncImmediateAndTeardownBoundary();
        std::cout << "Scoped texture cache: weak ownership, no hit I/O, deferred retirement, generation isolation, bounded cycles passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
