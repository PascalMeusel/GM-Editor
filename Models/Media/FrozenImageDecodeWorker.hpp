#pragma once
#include "Models/Media/RgbaImageLoader.hpp"
#include "Models/Session/FrozenImageAsset.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

enum class FrozenImageDecodePhase { BeforeRead,BeforeDecode,Ready };
struct FrozenImageDecodeMeasurements
{
    std::uint64_t hashReads=0,decodes=0;
    double hashMilliseconds=0,decodeMilliseconds=0;
};
struct FrozenImageDecodeOptions
{
    std::size_t maxQueued=16;
    bool measure=false;
    // Owned test hooks only. Production supplies neither and uses the normal
    // bounded memory decoder. No renderer, campaign or service may be captured.
    std::function<void(std::uint64_t,FrozenImageDecodePhase)> phaseHook;
    std::function<bool(const std::vector<unsigned char> &,const std::string &,LoadedRgbaImage &,
                       const RgbaImageLoadOptions &,std::string *)> decoder;
};
struct FrozenImageDecodeResult
{
    std::uint64_t ticket=0;
    FrozenImageAsset asset; // Weak generation/storage references only.
    LoadedRgbaImage image;
    bool success=false;
    std::string error;
    FrozenImageDecodeMeasurements measurements;
};
struct FrozenImageDecodeStats
{
    std::size_t queued=0;
    bool running=false,completionOutstanding=false;
    std::uint64_t started=0,completed=0,cancelled=0;
};
// One CPU worker, bounded FIFO and one decode/completion credit. request and
// cancellation never wait for I/O. Only explicit shutdown/destruction joins.
class FrozenImageDecodeWorker final
{
public:
    explicit FrozenImageDecodeWorker(FrozenImageDecodeOptions = {});
    ~FrozenImageDecodeWorker();
    FrozenImageDecodeWorker(const FrozenImageDecodeWorker &)=delete;
    FrozenImageDecodeWorker &operator=(const FrozenImageDecodeWorker &)=delete;
    // Zero means unavailable/full/stopped; no synchronous fallback occurs.
    // An exact outstanding generation/path/hash returns its existing ticket.
    std::uint64_t request(const FrozenImageAsset &);
    void cancel(std::uint64_t ticket);
    void cancelAll();
    std::unique_ptr<FrozenImageDecodeResult> takeCompletion();
    // Required after consuming OR discarding a taken completion. Destroy its
    // pixels before acknowledging, so the next decode cannot overlap buffers.
    void acknowledge(std::uint64_t ticket);
    FrozenImageDecodeStats stats() const;
    void shutdown();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
