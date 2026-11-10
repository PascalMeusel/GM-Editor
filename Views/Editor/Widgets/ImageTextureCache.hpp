#pragma once

#include "imgui.h"
#include "Models/Session/FrozenImageAsset.hpp"

#include <filesystem>
#include <cstddef>
#include <cstdint>
#include <string>

struct CachedImageTexture
{
    ImTextureID textureId = 0;
    int width = 0;
    int height = 0;
    bool loaded = false;
    std::filesystem::file_time_type lastWriteTime{};
    std::uintmax_t fileSize = 0;
    std::string boundContentHash;
    bool loadAttempted = false;
    unsigned long long lastAccessTick = 0;
};

struct ImageTextureCacheStats
{
    std::size_t entries = 0;
    std::size_t loadedTextures = 0;
    std::size_t scopedEntries = 0;
    std::size_t scopedLoadedTextures = 0;
};

const CachedImageTexture &getCachedImageTexture(const std::filesystem::path &path);
const CachedImageTexture &getCachedImageTexture(const FrozenImageAsset &asset);
// Renderer-thread caller supplies its frame serial. First cold lookup is
// pending; a later frame uses the same verified loader. No wall-time promise.
const CachedImageTexture &getDeferredImageTexture(const FrozenImageAsset &asset,std::uint64_t frameSerial);
// Frozen-only background decode. A visible call registers the current frame.
// With loaded=false, loadAttempted distinguishes pending(false) from failure(true).
const CachedImageTexture &getAsyncImageTexture(const FrozenImageAsset &,std::uint64_t frameSerial);
// Inspection only: does not queue, mark visible, retry, hash, decode or upload.
const CachedImageTexture &findCachedImageTexture(const FrozenImageAsset &);
// Main GL context, after all UI and before rendering. At most one upload/frame.
void processAsyncImageTextures(std::uint64_t frameSerial);
// Join CPU decoding before graphics teardown; no graphics callbacks here.
void shutdownImageDecodeWorker();
// A non-empty expected hash binds imported-map textures to the exact bytes
// approved by their MapRenderPlan/review snapshot. Ordinary portraits and
// other GM-only images may continue to use the path-only overload.
const CachedImageTexture &getCachedImageTexture(const std::filesystem::path &path,
                                                const std::string &expectedContentHash);
ImageTextureCacheStats imageTextureCacheStats();
const CachedImageTexture &getMapFogTexture(bool editor);
void pruneImageTextureCache(std::size_t maxEntries);
void clearImageTextureCache();
// Main GL context only, after every viewport has rendered and swapped.
void collectExpiredImageTextures();
