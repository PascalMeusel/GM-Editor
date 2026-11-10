#include "Models/Map/MapPreviewCompiler.hpp"
#include "Models/Map/MapRenderPlanCache.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

struct MapPreviewCompiler::Impl
{
    struct Job
    {
        std::uint64_t epoch;
        MapRenderPlanCacheKey key;
        std::shared_ptr<const MapModel> map;
        MapRenderPlanRequest request;
    };
    explicit Impl(Compiler function) : compiler(std::move(function)), worker([this] { run(); })
    {
    }
    ~Impl()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
            ++epoch;
            pending.reset();
        }
        changed.notify_one();
        worker.join();
    }
    void run()
    {
        for (;;)
        {
            std::optional<Job> job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock, [&] { return stopping || pending.has_value(); });
                if (stopping)
                    return;
                job = std::move(pending);
                pending.reset();
            }
            const auto started = std::chrono::steady_clock::now();
            std::shared_ptr<const MapRenderPlan> plan;
            std::string failure;
            try
            {
                plan = std::make_shared<MapRenderPlan>(compiler(*job->map, job->request));
                if (!mapRenderPlanMatchesMap(*plan, *job->map))
                    throw std::runtime_error("Compiled preview does not match its source map.");
            }
            catch (const std::exception &error)
            {
                failure = error.what();
            }
            catch (...)
            {
                failure = "Unknown map preview compilation failure.";
            }
            const double milliseconds =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            std::lock_guard<std::mutex> lock(mutex);
            if (stopping || job->epoch != epoch || !wanted || !(job->key == *wanted))
            {
                ++discarded;
                continue;
            }
            error = std::move(failure);
            if (error.empty())
            {
                ready = std::move(job);
                readyPlan = std::move(plan);
                duration = milliseconds;
            }
        }
    }
    Compiler compiler;
    mutable std::mutex mutex;
    std::condition_variable changed;
    bool stopping = false;
    std::uint64_t epoch = 0, discarded = 0;
    std::optional<MapRenderPlanCacheKey> wanted;
    std::optional<Job> pending, ready;
    std::shared_ptr<const MapRenderPlan> readyPlan;
    std::string error;
    double duration = 0;
    // Last member: every object observed by run() exists before it starts.
    std::thread worker;
};

MapPreviewCompiler::MapPreviewCompiler(Compiler compiler) : impl_(std::make_unique<Impl>(std::move(compiler)))
{
}
MapPreviewCompiler::~MapPreviewCompiler() = default;

void MapPreviewCompiler::request(const MapModel &map, const MapRenderPlanRequest &request,
                                 std::uint64_t authoringSerial)
{
    if (request.audience != MapRenderAudience::GameMaster || request.additionalGeneratedFeatureVisibility)
        throw std::invalid_argument("Background authoring previews require a self-contained GM request.");
    // Compilation does not depend on cell size; zoom only changes how the
    // finished immutable geometry is drawn. Do not rebuild on LOD crossings.
    const auto key = makeMapRenderPlanCacheKey(map, request, authoringSerial, 0);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->wanted && *impl_->wanted == key)
            return;
    }
    auto snapshot = std::make_shared<const MapModel>(map);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->wanted = key;
        impl_->error.clear();
        impl_->pending = Impl::Job{++impl_->epoch, key, std::move(snapshot), request};
    }
    impl_->changed.notify_one();
}

MapPreviewCompilation MapPreviewCompiler::state() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    MapPreviewCompilation result;
    result.error = impl_->error;
    result.discardedResults = impl_->discarded;
    if (!impl_->wanted)
        return result;
    if (impl_->ready)
    {
        auto previous = impl_->ready->key;
        auto current = *impl_->wanted;
        result.current = previous == current;
        previous.mapRevision = current.mapRevision = 0;
        previous.contentVersion = current.contentVersion = 0;
        if (previous == current)
        {
            result.source = impl_->ready->map;
            result.plan = impl_->readyPlan;
            result.compilationMilliseconds = impl_->duration;
        }
    }
    result.refreshing = !result.current && result.error.empty();
    return result;
}

void MapPreviewCompiler::retry()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    ++impl_->epoch;
    impl_->pending.reset();
    impl_->wanted.reset();
    impl_->error.clear();
}

void MapPreviewCompiler::clear()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    ++impl_->epoch;
    impl_->wanted.reset();
    impl_->pending.reset();
    impl_->ready.reset();
    impl_->readyPlan.reset();
    impl_->error.clear();
}
