struct AsyncRenderer : Renderer
{
    std::shared_ptr<frozen_decode_test::Decoder> decoder=std::make_shared<frozen_decode_test::Decoder>();
    std::shared_ptr<FrozenImageDecodeWorker> worker;
    std::thread::id mainThread=std::this_thread::get_id();std::size_t uploads=0,observations=0;
    std::uint64_t lastCompletion=0;
    explicit AsyncRenderer(const std::shared_ptr<frozen_decode_test::Gate> &gate={})
        :worker(std::make_shared<FrozenImageDecodeWorker>(frozen_decode_test::options(decoder,gate))){}
    ScopedImageTextureCache cache()
    {
        return ScopedImageTextureCache([this](const auto &path,const auto &hash,auto &texture){return load(path,hash,texture);},
            [this](auto &texture){require(std::this_thread::get_id()==mainThread,"Delete callback ran on worker");destroy(texture);},worker,
            [this](const LoadedRgbaImage &image,CachedImageTexture &texture){
                require(std::this_thread::get_id()==mainThread&&image.pixels.size()==16,"Upload was off-thread or received invalid pixels");
                ++uploads;texture.textureId=static_cast<ImTextureID>(next++);texture.width=image.width;texture.height=image.height;return true;},
            [this](const auto &){require(std::this_thread::get_id()==mainThread,"Decode receipt observer ran on worker");++observations;});
    }
    void ready(){frozen_decode_test::until([&]{const auto s=worker->stats();if(s.completed<=lastCompletion||!s.completionOutstanding)return false;
        lastCompletion=s.completed;return true;},"Async cache decode did not complete");}
};
void asyncVisiblePumpAndFairness()
{
    using namespace frozen_decode_test;Fixture first,second;AsyncRenderer renderer;auto cache=renderer.cache();
    const auto a=first.asset(),b=second.asset();
    for(int i=0;i<20;++i)require(!cache.getAsync(a,10).loadAttempted,"Visible registration attempted image work");
    cache.getAsync(b,10);require(renderer.worker->stats().started==0&&renderer.loads==0&&renderer.uploads==0,"Registration launched worker or renderer");
    cache.processAsync(10);renderer.ready();cache.getAsync(a,10);cache.getAsync(b,10);cache.processAsync(10);
    require(renderer.uploads==0,"Same-frame pump uploaded a newly visible image");
    cache.getAsync(a,11);cache.getAsync(b,11);cache.processAsync(11);
    require(renderer.uploads==1&&cache.stats().loadedTextures==1,"First later pump did not upload exactly one image");
    cache.processAsync(11);require(renderer.uploads==1,"Multiple callers exceeded one upload per frame");renderer.ready();
    cache.getAsync(a,12);cache.getAsync(b,12);cache.processAsync(12);
    require(renderer.uploads==2&&cache.stats().loadedTextures==2&&renderer.loads==0,"FIFO peer starved or used synchronous loader");
    const auto original=cache.getAsync(a,13).textureId;auto independent=generation();
    FrozenImageAsset replacement(first.path,a.contentHash(),independent,first.storage);
    cache.getAsync(replacement,13);cache.processAsync(13);renderer.ready();cache.getAsync(replacement,14);cache.processAsync(14);
    require(renderer.uploads==3&&cache.getAsync(replacement,14).textureId!=original,"New generation reused old async texture");
    for(std::uint64_t frame=15;frame<35;++frame){cache.getAsync(a,frame);cache.getAsync(b,frame);cache.getAsync(replacement,frame);cache.processAsync(frame);}
    require(renderer.uploads==3&&renderer.decoder->calls==3&&renderer.observations==3,"Immutable async hits repeated decode/upload");
    first.generation.reset();cache.collectExpired();require(renderer.deleted.count(original)==1&&cache.stats().loadedTextures==2,"Expiry deleted wrong async generation");
    cache.clear();require(cache.stats().entries==0&&renderer.deleted.size()==3,"Explicit async cache clear leaked textures");
}
void asyncVisibilityRevocationAndFailure()
{
    using namespace frozen_decode_test;Fixture f;AsyncRenderer renderer;auto cache=renderer.cache();const auto asset=f.asset();
    cache.getAsync(asset,1);cache.processAsync(1);renderer.ready();cache.processAsync(2);
    require(renderer.uploads==0&&!cache.getAsync(asset,3).loadAttempted,"Offscreen result uploaded or became terminal");
    cache.processAsync(3);renderer.ready();cache.getAsync(asset,4);cache.processAsync(4);
    require(renderer.uploads==1&&renderer.decoder->calls==2,"Visible return revived old result or failed to requeue");cache.clear();
    auto gate=std::make_shared<Gate>();AsyncRenderer revoked(gate);auto cancelled=revoked.cache();Release release{gate};Fixture old;
    const auto stale=old.asset();cancelled.getAsync(stale,1);cancelled.processAsync(1);entered(gate);
    old.generation.reset();cancelled.collectExpired();cancelled.processAsync(2);
    require(!gate->opened&&revoked.uploads==0&&cancelled.stats().entries==0,"Expired pending owner joined/loaded/retained cache entry");
    gate->open();until([&]{return !revoked.worker->stats().running;},"Expired decode failed to unwind");cancelled.processAsync(3);
    require(revoked.uploads==0&&revoked.decoder->calls==0,"Expired completion reached fake renderer");
    AsyncRenderer failed;failed.decoder->fail=true;auto failure=failed.cache();failure.getAsync(asset,10);failure.processAsync(10);failed.ready();
    failure.getAsync(asset,11);failure.processAsync(11);
    require(failure.getAsync(asset,11).loadAttempted&&!failure.getAsync(asset,11).loaded,"Async failure stayed pending or became loaded");
    for(std::uint64_t frame=12;frame<25;++frame){failure.getAsync(asset,frame);failure.processAsync(frame);}
    require(failed.decoder->calls==1&&failed.uploads==0&&failed.loads==0,"Failed immutable image retried or fell back synchronously");
    failure.shutdownAsync();failure.clear();
}
void asyncImmediateAndTeardownBoundary()
{
    using namespace frozen_decode_test;Fixture f;auto gate=std::make_shared<Gate>();AsyncRenderer renderer(gate);
    auto cache=renderer.cache();Release release{gate};const auto asset=f.asset();cache.getAsync(asset,1);cache.processAsync(1);entered(gate);
    const auto id=cache.get(asset).textureId;
    require(id&&renderer.loads==1&&!gate->opened&&cache.getDeferred(asset,1).textureId==id,
        "Immediate/deferred compatibility waited for async worker or lost loaded hit");
    gate->open();until([&]{return !renderer.worker->stats().running;},"Superseded decode did not retire");cache.getAsync(asset,2);cache.processAsync(2);
    require(renderer.uploads==0&&cache.getAsync(asset,2).textureId==id,"Superseded async completion overwrote immediate texture");
    cache.shutdownAsync();Fixture newAsset;require(cache.getAsync(newAsset.asset(),3).loadAttempted,"Shutdown continued advertising pending work");
    cache.processAsync(3);require(renderer.uploads==0,"Shutdown invoked an uploader");cache.clear();require(renderer.deleted.count(id)==1,"Shutdown lost explicit renderer cleanup");
    auto compatible=renderer.cache();const FrozenImageAsset legacy(f.path,asset.contentHash(),f.generation);
    require(compatible.getAsync(legacy,4).loadAttempted&&compatible.stats().entries==0,"Unsupported async descriptor inserted a poisoned entry");
    require(compatible.get(legacy).loaded,"Unsupported async lookup broke the existing synchronous descriptor contract");compatible.clear();
    const auto destroyed=renderer.deleted.size();{auto late=renderer.cache();(void)late.get(asset);}
    require(renderer.deleted.size()==destroyed,"Async-capable cache destructor invoked graphics callbacks");
}
