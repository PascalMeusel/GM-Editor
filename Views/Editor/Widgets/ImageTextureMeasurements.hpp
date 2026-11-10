#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace gm::image_texture
{
enum class Origin { Ordinary, Frozen, FrozenWorker, Count };
// FrozenWorker samples are background durations delivered on a later pump
// frame. Receipt-frame attribution is not renderer-thread execution time.
enum class Stage { HashRead, DecodeOrientation, UploadSubmit, Count };
inline const char *originName(Origin value)
{ constexpr const char *names[]={"ordinary","frozen","frozen_worker_receipt"}; return names[static_cast<std::size_t>(value)]; }
inline const char *stageName(Stage value)
{ constexpr const char *names[]={"encoded_hash_read","decode_orientation","gl_upload_submission"}; return names[static_cast<std::size_t>(value)]; }
struct Measurements
{
    struct Sample { std::uint64_t count=0; double milliseconds=0; };
    std::array<std::array<Sample,static_cast<std::size_t>(Stage::Count)>,static_cast<std::size_t>(Origin::Count)> values{};
};
namespace detail
{
inline Measurements *&observer() { static thread_local Measurements *value=nullptr; return value; }
}
// Opt-in, caller-owned instrumentation for synchronous renderer-thread calls.
// The sink must outlive this scope. Nested scopes restore the previous observer;
// never transfer a scope between threads. There are no clocks/allocations on
// unobserved loads, no GL synchronization and no change to cache ownership.
class ScopedMeasurements
{
    Measurements *previous_;
public:
    explicit ScopedMeasurements(Measurements *sink) noexcept : previous_(detail::observer()) { detail::observer()=sink; }
    ~ScopedMeasurements() { detail::observer()=previous_; }
    ScopedMeasurements(const ScopedMeasurements &)=delete;
    ScopedMeasurements &operator=(const ScopedMeasurements &)=delete;
};
class Measure
{
    Measurements::Sample *sample_=nullptr;
    std::chrono::steady_clock::time_point started_{};
public:
    Measure(Origin origin, Stage stage)
    {
        if(auto *sink=detail::observer())
        {
            sample_=&sink->values[static_cast<std::size_t>(origin)][static_cast<std::size_t>(stage)];
            started_=std::chrono::steady_clock::now();
        }
    }
    ~Measure()
    {
        if(sample_)
        {
            ++sample_->count;
            sample_->milliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started_).count();
        }
    }
    Measure(const Measure &)=delete;
    Measure &operator=(const Measure &)=delete;
};
}
