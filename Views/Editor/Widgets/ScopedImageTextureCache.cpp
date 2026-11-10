#include "Views/Editor/Widgets/ScopedImageTextureCache.hpp"
#include <utility>

ScopedImageTextureCache::ScopedImageTextureCache(Loader loader, Deleter deleter,
    std::shared_ptr<FrozenImageDecodeWorker> worker, Uploader uploader, DecodeObserver observer)
    : loader_(std::move(loader)), deleter_(std::move(deleter)),worker_(std::move(worker)),
      uploader_(std::move(uploader)),observer_(std::move(observer)) {}

bool ScopedImageTextureCache::Less::operator()(const Key &a, const Key &b) const
{
    const std::owner_less<std::weak_ptr<const FrozenImageAssetGeneration>> owner;
    if (owner(a.lifetime, b.lifetime)) return true;
    if (owner(b.lifetime, a.lifetime)) return false;
    if (a.path != b.path) return a.path < b.path;
    return a.hash < b.hash;
}

const CachedImageTexture &ScopedImageTextureCache::get(const FrozenImageAsset &asset)
{return getImpl(asset,std::nullopt);}

const CachedImageTexture &ScopedImageTextureCache::getDeferred(const FrozenImageAsset &asset,std::uint64_t frameSerial)
{return getImpl(asset,frameSerial);}

const CachedImageTexture &ScopedImageTextureCache::getImpl(const FrozenImageAsset &asset,
    std::optional<std::uint64_t> frameSerial)
{
    static const CachedImageTexture unloaded;
    if (!asset.valid()) return unloaded;
    Key key{asset.lifetime(), asset.path().lexically_normal().generic_u8string(), asset.contentHash()};
    auto [entry, inserted] = entries_.try_emplace(std::move(key));
    if (inserted)
    {
        entry->second.texture.boundContentHash=asset.contentHash();
        entry->second.firstFrame=frameSerial;
    }
    auto &value=entry->second;
    if(!value.texture.loadAttempted&&
       (!frameSerial||!value.firstFrame||*frameSerial!=*value.firstFrame))
    {
        if(value.ticket&&worker_){worker_->cancel(value.ticket);value.ticket=0;}
        // Immediate lookup deliberately preserves its old contract, even if
        // this key was first requested by a deferred caller in this frame.
        // Hits never stat/hash files or release queued texture IDs.
        value.texture.loadAttempted=true;
        value.texture.loaded=loader_(asset.path(),asset.contentHash(),value.texture);
        value.firstFrame.reset();
    }
    return value.texture;
}

const CachedImageTexture &ScopedImageTextureCache::getAsync(const FrozenImageAsset &asset,std::uint64_t frame)
{
    static const CachedImageTexture empty;
    static const CachedImageTexture unavailable=[] {CachedImageTexture value;value.loadAttempted=true;return value;}();
    if(!asset.valid())return empty;
    Key key{asset.lifetime(),asset.path().lexically_normal().generic_u8string(),asset.contentHash()};
    auto entry=entries_.find(key);
    if(entry!=entries_.end()&&entry->second.texture.loadAttempted){
        entry->second.visibleFrame=frame;return entry->second.texture;
    }
    // Unsupported async descriptors do not poison the compatible synchronous
    // lookup. Production descriptors always carry a separate storage lease.
    if(!worker_||!uploader_||stopped_||!asset.leaseStorage())return unavailable;
    if(entry==entries_.end()){
        entry=entries_.try_emplace(std::move(key)).first;
        entry->second.texture.boundContentHash=asset.contentHash();entry->second.firstFrame=frame;
    }
    auto &value=entry->second;
    value.asset=asset;value.visibleFrame=frame;
    return value.texture;
}

const CachedImageTexture &ScopedImageTextureCache::find(const FrozenImageAsset &asset) const
{
    static const CachedImageTexture unavailable;
    if(!asset.valid())return unavailable;
    const Key key{asset.lifetime(),asset.path().lexically_normal().generic_u8string(),asset.contentHash()};
    const auto entry=entries_.find(key);
    return entry==entries_.end()?unavailable:entry->second.texture;
}

void ScopedImageTextureCache::processAsync(std::uint64_t frame)
{
    if(!worker_||stopped_||pumpedFrame_==frame)return;
    pumpedFrame_=frame;
    for(auto &[key,value]:entries_)
        if(value.ticket&&(key.lifetime.expired()||value.visibleFrame!=frame)){
            worker_->cancel(value.ticket);value.ticket=0;
        }
    if(auto result=worker_->takeCompletion())
    {
        // Release the RGBA buffer before releasing the worker's one-result
        // credit, including observer/uploader exceptions and stale results.
        struct Acknowledge {
            FrozenImageDecodeWorker &worker;std::unique_ptr<FrozenImageDecodeResult> &result;std::uint64_t ticket;
            ~Acknowledge(){result.reset();worker.acknowledge(ticket);}
        } acknowledge{*worker_,result,result->ticket};
        if(observer_)observer_(result->measurements);
        for(auto &[key,value]:entries_)if(value.ticket==result->ticket){
            value.ticket=0;
            const Key returned{result->asset.lifetime(),result->asset.path().lexically_normal().generic_u8string(),result->asset.contentHash()};
            const Less less;
            if(!less(key,returned)&&!less(returned,key)&&result->asset.valid()&&value.asset.valid()&&
                value.visibleFrame==frame&&value.firstFrame&&*value.firstFrame<frame&&!value.texture.loadAttempted){
                value.texture.loadAttempted=true;
                value.texture.loaded=result->success&&uploader_(result->image,value.texture);
            }
            break;
        }
    }
    // FIFO worker tickets stay intact while visible, including when the queue
    // is full. An offscreen result is discarded, then a later visible request
    // may queue again without reviving the discarded completion.
    for(auto &[key,value]:entries_)
        if(value.visibleFrame==frame&&value.asset.valid()&&!value.texture.loadAttempted&&!value.ticket){
            try {value.ticket=worker_->request(value.asset);}
            catch(...){value.ticket=0;} // Admission may retry while visible; never decode synchronously.
        }
}

void ScopedImageTextureCache::shutdownAsync()
{
    stopped_=true;
    if(worker_)worker_->shutdown();
    for(auto &[key,value]:entries_)value.ticket=0;
}

void ScopedImageTextureCache::collectExpired()
{
    for (auto entry = entries_.begin(); entry != entries_.end();)
    {
        if (!entry->first.lifetime.expired()) { ++entry; continue; }
        if(entry->second.ticket&&worker_)worker_->cancel(entry->second.ticket);
        deleter_(entry->second.texture);
        entry = entries_.erase(entry);
    }
}

void ScopedImageTextureCache::clear()
{
    if(worker_)worker_->cancelAll();
    for (auto &entry : entries_) deleter_(entry.second.texture);
    entries_.clear();
}

ScopedImageTextureCacheStats ScopedImageTextureCache::stats() const
{
    ScopedImageTextureCacheStats result;
    result.entries = entries_.size();
    for (const auto &entry : entries_)
        if (entry.second.texture.loaded && entry.second.texture.textureId) ++result.loadedTextures;
    return result;
}
