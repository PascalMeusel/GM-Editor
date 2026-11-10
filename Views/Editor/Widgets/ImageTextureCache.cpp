#include "ImageTextureCache.hpp"
#include "ScopedImageTextureCache.hpp"
#include "ImageTextureMeasurements.hpp"
#include "Models/Media/RgbaImageLoader.hpp"
#include "Models/Map/MapFogTexture.hpp"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <map>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace
{
using ImageTextureMap = std::map<std::string, CachedImageTexture>;
using gm::image_texture::Origin;
using gm::image_texture::Stage;
using gm::image_texture::Measure;

ImageTextureMap &textureCache()
{
    static ImageTextureMap cache;
    return cache;
}

unsigned long long &accessCounter()
{
    static unsigned long long counter = 0;
    return counter;
}

void releaseTexture(CachedImageTexture &texture)
{
    if (texture.textureId)
    {
        const unsigned int textureId = static_cast<unsigned int>(texture.textureId);
        glDeleteTextures(1, &textureId);
    }
    texture = CachedImageTexture{};
}

bool uploadImageTexture(const LoadedRgbaImage &image,CachedImageTexture &texture,Origin origin)
{
    if(image.width<=0||image.height<=0||image.width>16384||image.height>16384||
        static_cast<std::uint64_t>(image.width)*image.height>100000000ull||
        image.pixels.size()!=static_cast<std::uint64_t>(image.width)*image.height*4)
        return false;
    texture.width=image.width;texture.height=image.height;
    unsigned int textureId = 0;
    {
        Measure measure(origin, Stage::UploadSubmit);
        glGenTextures(1, &textureId);
        if(!textureId)return false;
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.width, texture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&textureId);return false;}
    }

    texture.textureId = static_cast<ImTextureID>(textureId);
    texture.loaded = true;
    return true;
}

bool loadImageTexture(const std::filesystem::path &path, CachedImageTexture &texture, Origin origin)
{
    LoadedRgbaImage image;
    RgbaImageLoadOptions loadOptions;
    loadOptions.maximumSourceEdge = 16384;
    loadOptions.maximumSourcePixels = 100000000ull;
    {
        Measure measure(origin, Stage::DecodeOrientation);
        if (!loadRgbaImage(path, image, loadOptions))return false;
    }
    return uploadImageTexture(image,texture,origin);
}

std::string textureFileContentHash(const std::filesystem::path &path, Origin origin)
{
    Measure measure(origin, Stage::HashRead);
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};
    std::uint64_t hash = 14695981039346656037ull;
    char buffer[4096];
    while (file)
    {
        file.read(buffer, sizeof(buffer));
        const std::streamsize bytesRead = file.gcount();
        for (std::streamsize index = 0; index < bytesRead; ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= 1099511628211ull;
        }
    }
    std::ostringstream stream;
    stream << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

ScopedImageTextureCache *&existingScopedTextureCache()
{static ScopedImageTextureCache *cache=nullptr;return cache;}

std::shared_ptr<FrozenImageDecodeWorker> makeImageDecodeWorker()
{
    try {
        FrozenImageDecodeOptions options;options.measure=gm::image_texture::detail::observer()!=nullptr;
        return std::make_shared<FrozenImageDecodeWorker>(std::move(options));
    }catch(...){return {};}
}

ScopedImageTextureCache &scopedTextureCache()
{
    static ScopedImageTextureCache cache(
        [](const std::filesystem::path &path, const std::string &hash, CachedImageTexture &texture) {
            return textureFileContentHash(path, Origin::Frozen) == hash && loadImageTexture(path, texture, Origin::Frozen);
        }, releaseTexture,
        makeImageDecodeWorker(),
        [](const LoadedRgbaImage &image,CachedImageTexture &texture){return uploadImageTexture(image,texture,Origin::Frozen);},
        [](const FrozenImageDecodeMeasurements &measurement){
            // Merge owned worker values only on the renderer thread. A worker
            // never borrows a caller's scoped observer or its TLS pointer.
            if(auto *sink=gm::image_texture::detail::observer()){
                auto &hash=sink->values[std::size_t(Origin::FrozenWorker)][std::size_t(Stage::HashRead)];
                auto &decode=sink->values[std::size_t(Origin::FrozenWorker)][std::size_t(Stage::DecodeOrientation)];
                hash.count+=measurement.hashReads;hash.milliseconds+=measurement.hashMilliseconds;
                decode.count+=measurement.decodes;decode.milliseconds+=measurement.decodeMilliseconds;
            }
        });
    existingScopedTextureCache()=&cache;
    return cache;
}
}

const CachedImageTexture &getCachedImageTexture(const FrozenImageAsset &asset)
{
    return scopedTextureCache().get(asset);
}

const CachedImageTexture &getDeferredImageTexture(const FrozenImageAsset &asset,std::uint64_t frameSerial)
{
    return scopedTextureCache().getDeferred(asset,frameSerial);
}

const CachedImageTexture &getAsyncImageTexture(const FrozenImageAsset &asset,std::uint64_t frameSerial)
{return scopedTextureCache().getAsync(asset,frameSerial);}
const CachedImageTexture &findCachedImageTexture(const FrozenImageAsset &asset)
{
    static const CachedImageTexture empty;
    if(auto *cache=existingScopedTextureCache())return cache->find(asset);
    return empty;
}
void processAsyncImageTextures(std::uint64_t frameSerial)
{if(auto *cache=existingScopedTextureCache())cache->processAsync(frameSerial);}
void shutdownImageDecodeWorker()
{if(auto *cache=existingScopedTextureCache())cache->shutdownAsync();}

const CachedImageTexture &getCachedImageTexture(const std::filesystem::path &path)
{
    return getCachedImageTexture(path, {});
}

const CachedImageTexture &getCachedImageTexture(
    const std::filesystem::path &path,
    const std::string &expectedContentHash)
{
    ImageTextureMap &cache = textureCache();
    const std::string key = path.lexically_normal().generic_string();
    auto [it, inserted] = cache.try_emplace(key);
    it->second.lastAccessTick = ++accessCounter();
    std::error_code fileError;
    if (!std::filesystem::is_regular_file(path, fileError) || fileError)
    {
        const unsigned long long lastAccess = it->second.lastAccessTick;
        releaseTexture(it->second);
        it->second.loadAttempted = true;
        it->second.boundContentHash = expectedContentHash;
        it->second.lastAccessTick = lastAccess;
        return it->second;
    }

    const std::filesystem::file_time_type lastWriteTime =
        std::filesystem::last_write_time(path, fileError);
    if (fileError)
    {
        const unsigned long long lastAccess = it->second.lastAccessTick;
        releaseTexture(it->second);
        it->second.loadAttempted = true;
        it->second.boundContentHash = expectedContentHash;
        it->second.lastAccessTick = lastAccess;
        return it->second;
    }
    const std::uintmax_t fileSize = std::filesystem::file_size(path, fileError);
    if (fileError)
    {
        const unsigned long long lastAccess = it->second.lastAccessTick;
        releaseTexture(it->second);
        it->second.loadAttempted = true;
        it->second.boundContentHash = expectedContentHash;
        it->second.lastAccessTick = lastAccess;
        return it->second;
    }
    if (inserted || !it->second.loadAttempted ||
        it->second.lastWriteTime != lastWriteTime ||
        it->second.fileSize != fileSize ||
        it->second.boundContentHash != expectedContentHash)
    {
        const unsigned long long lastAccess = it->second.lastAccessTick;
        releaseTexture(it->second);
        it->second.lastWriteTime = lastWriteTime;
        it->second.fileSize = fileSize;
        it->second.boundContentHash = expectedContentHash;
        it->second.loadAttempted = true;
        it->second.lastAccessTick = lastAccess;
        if (expectedContentHash.empty() ||
            textureFileContentHash(path, Origin::Ordinary) == expectedContentHash)
            loadImageTexture(path, it->second, Origin::Ordinary);
        it->second.lastWriteTime = lastWriteTime;
        it->second.fileSize = fileSize;
        it->second.boundContentHash = expectedContentHash;
        it->second.loadAttempted = true;
    }
    return it->second;
}

const CachedImageTexture &getMapFogTexture(bool editor)
{
    auto &texture=textureCache()[editor ? ":builtin/editor-fog-v1" : ":builtin/player-fog-v1"];
    texture.lastAccessTick=++accessCounter();
    if(texture.loaded) return texture;
    const auto &pixels=gm::map_render::fogTexturePixels(editor);
    unsigned int id=0;glGenTextures(1,&id);
    if(!id)return texture;
    glBindTexture(GL_TEXTURE_2D,id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,gm::map_render::fogTextureSize,
        gm::map_render::fogTextureSize,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glBindTexture(GL_TEXTURE_2D,0);
    texture.textureId=static_cast<ImTextureID>(id);texture.loaded=true;
    texture.width=texture.height=gm::map_render::fogTextureSize;
    return texture;
}

ImageTextureCacheStats imageTextureCacheStats()
{
    ImageTextureCacheStats stats;
    const auto scoped = existingScopedTextureCache()?existingScopedTextureCache()->stats():ScopedImageTextureCacheStats{};
    stats.scopedEntries = stats.entries = scoped.entries;
    stats.scopedLoadedTextures = stats.loadedTextures = scoped.loadedTextures;
    for (const auto &entry : textureCache())
    {
        ++stats.entries;
        if (entry.second.loaded && entry.second.textureId)
            ++stats.loadedTextures;
    }
    return stats;
}

void pruneImageTextureCache(std::size_t maxEntries)
{
    ImageTextureMap &cache = textureCache();
    while (cache.size() > maxEntries)
    {
        auto oldest = cache.begin();
        for (auto iterator = cache.begin(); iterator != cache.end(); ++iterator)
        {
            if (iterator->second.lastAccessTick < oldest->second.lastAccessTick)
                oldest = iterator;
        }
        releaseTexture(oldest->second);
        cache.erase(oldest);
    }
}

void clearImageTextureCache()
{
    if(auto *cache=existingScopedTextureCache())cache->clear();
    ImageTextureMap &cache = textureCache();
    for (auto &entry : cache)
        releaseTexture(entry.second);
    cache.clear();
}

void collectExpiredImageTextures()
{
    if(auto *cache=existingScopedTextureCache())cache->collectExpired();
}
