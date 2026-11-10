#pragma once
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Models/Media/FrozenImageDecodeWorker.hpp"
#include <functional>
#include <map>
#include <optional>
#include <cstdint>

struct ScopedImageTextureCacheStats
{
    std::size_t entries = 0;
    std::size_t loadedTextures = 0;
};

// Renderer-thread cache for immutable reviewed files. Call collectExpired only
// after every viewport using queued texture IDs has finished rendering/swapping.
// Destruction deliberately invokes no graphics callbacks: clear explicitly
// while the graphics context exists, before renderer/platform shutdown.
class ScopedImageTextureCache
{
public:
    using Loader = std::function<bool(const std::filesystem::path &, const std::string &, CachedImageTexture &)>;
    using Deleter = std::function<void(CachedImageTexture &)>;
    using Uploader = std::function<bool(const LoadedRgbaImage &, CachedImageTexture &)>;
    using DecodeObserver = std::function<void(const FrozenImageDecodeMeasurements &)>;
    ScopedImageTextureCache(Loader loader, Deleter deleter,
        std::shared_ptr<FrozenImageDecodeWorker> worker = {}, Uploader uploader = {}, DecodeObserver observer = {});
    ScopedImageTextureCache(const ScopedImageTextureCache &) = delete;
    ScopedImageTextureCache &operator=(const ScopedImageTextureCache &) = delete;
    const CachedImageTexture &get(const FrozenImageAsset &asset);
    // The first request only reserves the exact immutable key. A later frame
    // may load it; repeated callers in the first frame cannot combine decode
    // with review adoption. Existing textures and failed attempts return now.
    const CachedImageTexture &getDeferred(const FrozenImageAsset &asset,std::uint64_t frameSerial);
    // Call only for a visible image slot. This registers visibility, never I/O
    // or graphics work. The post-UI pump owns requests and completion upload.
    const CachedImageTexture &getAsync(const FrozenImageAsset &,std::uint64_t frameSerial);
    const CachedImageTexture &find(const FrozenImageAsset &) const;
    void processAsync(std::uint64_t frameSerial);
    void shutdownAsync();
    void collectExpired();
    void clear();
    ScopedImageTextureCacheStats stats() const;
private:
    struct Key
    {
        std::weak_ptr<const FrozenImageAssetGeneration> lifetime;
        std::string path, hash;
    };
    struct Less { bool operator()(const Key &, const Key &) const; };
    struct Entry
    {
        CachedImageTexture texture;
        std::optional<std::uint64_t> firstFrame;
        FrozenImageAsset asset;
        std::optional<std::uint64_t> visibleFrame;
        std::uint64_t ticket=0;
    };
    const CachedImageTexture &getImpl(const FrozenImageAsset &,std::optional<std::uint64_t> frameSerial);
    std::map<Key, Entry, Less> entries_;
    Loader loader_;
    Deleter deleter_;
    std::shared_ptr<FrozenImageDecodeWorker> worker_;
    Uploader uploader_;
    DecodeObserver observer_;
    bool stopped_=false;
    std::optional<std::uint64_t> pumpedFrame_;
};
