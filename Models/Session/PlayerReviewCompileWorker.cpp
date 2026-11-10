#include "Models/Session/PlayerReviewCompileWorker.hpp"
#include "Models/Map/MapRenderCompileMemoAmbient.hpp"
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

std::optional<std::vector<std::uint8_t>> playerReviewCompileEnvironment() noexcept
{
    try
    {
        MapPackageServiceInternal::Writer writer(nullptr, 8192);
        gm::map_render::detail::encodeCompileAmbientState(writer);
        return writer.take();
    }
    catch (...) { return std::nullopt; }
}

struct PlayerReviewCompileWorker::Impl
{
    struct Job
    {
        std::uint64_t serial;
        std::shared_ptr<const PlayerReviewCompileInput> input;
        std::unique_ptr<PlayerReviewCompileResult> result;
    };
    explicit Impl(PlayerReviewCompileOptions settings)
        : options(std::move(settings)), worker([this] { run(); }) {}
    ~Impl()
    {
        { std::lock_guard<std::mutex> lock(mutex); stopping = true; ++serial; pending.reset(); }
        changed.notify_one(); worker.join();
    }
    void run()
    {
        for (;;)
        {
            std::optional<Job> job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock, [&] { return stopping || pending.has_value(); });
                if (stopping) return;
                job = std::move(pending); pending.reset(); counts.pending = false;
                counts.running = true; ++counts.started;
            }
            auto result = std::move(job->result);
            result->serial = job->serial;
            try
            {
                const auto &input = job->input;
                if (input->gmMap)
                {
                    if (options.phaseHook) options.phaseHook(job->serial, PlayerReviewCompilePhase::BeforeGm);
                    result->gm = std::make_shared<gm::map_render::MapRenderCompileMemo>(options.limits);
                    compileMapRenderPlanWithMemo(*input->gmMap, input->gmRequest, *result->gm,
                                                input->measure?&result->gmMeasurements:nullptr);
                    if (options.phaseHook) options.phaseHook(job->serial, PlayerReviewCompilePhase::BeforePlayer);
                    result->player = std::make_shared<gm::map_render::MapRenderCompileMemo>(options.limits);
                    auto request = input->playerRequest;
                    request.additionalGeneratedFeatureVisibility = [input](const MapGeneratedFeature &feature) {
                        // The compiler passes references from this exact owned
                        // source vector, preserving occurrence/order semantics.
                        const auto index = static_cast<std::size_t>(&feature - input->playerMap->generation.graph.features.data());
                        return index < input->playerPolicy.size() && input->playerPolicy[index] != 0;
                    };
                    compileMapRenderPlanWithMemo(*input->playerMap, request, *result->player,
                                                input->measure?&result->playerMeasurements:nullptr);
                    result->foregroundRequired = result->gm->empty() || result->player->empty();
                }
                if (options.phaseHook) options.phaseHook(job->serial, PlayerReviewCompilePhase::Ready);
                result->environment = playerReviewCompileEnvironment();
                result->foregroundRequired=result->foregroundRequired||!result->environment;
            }
            catch (const std::exception &error)
            {result->foregroundRequired=true;try{result->error=error.what();}catch(...) {}}
            catch (...)
            {result->foregroundRequired=true;try{result->error="Map review compilation failed.";}catch(...) {}}
            {
                std::lock_guard<std::mutex> lock(mutex);
                counts.running = false; ++counts.completed;
                if (stopping || job->serial != serial) { ++counts.discarded; continue; }
                ready = std::move(result);
            }
        }
    }
    PlayerReviewCompileOptions options;
    mutable std::mutex mutex;
    std::condition_variable changed;
    bool stopping = false;
    std::uint64_t serial = 0;
    PlayerReviewCompileStats counts;
    std::optional<Job> pending;
    std::unique_ptr<PlayerReviewCompileResult> ready;
    std::thread worker;
};

PlayerReviewCompileWorker::PlayerReviewCompileWorker(PlayerReviewCompileOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}
PlayerReviewCompileWorker::~PlayerReviewCompileWorker() = default;
std::uint64_t PlayerReviewCompileWorker::request(PlayerReviewCompileInput input)
{
    if (input.gmRequest.additionalGeneratedFeatureVisibility || input.playerRequest.additionalGeneratedFeatureVisibility ||
        bool(input.gmMap) != bool(input.playerMap) ||
        (input.playerMap && input.playerPolicy.size() != input.playerMap->generation.graph.features.size()))
        throw std::invalid_argument("Review worker requires owned matching map/policy inputs.");
    auto owned = std::make_shared<const PlayerReviewCompileInput>(std::move(input));
    // Allocate the completion on the caller; allocation failure must never
    // escape a background thread or lose the ability to report its outcome.
    auto result=std::make_unique<PlayerReviewCompileResult>();
    std::uint64_t serial;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        serial = ++impl_->serial; impl_->ready.reset();
        impl_->pending = Impl::Job{serial, std::move(owned),std::move(result)}; impl_->counts.pending = true;
    }
    impl_->changed.notify_one(); return serial;
}
void PlayerReviewCompileWorker::cancel()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    ++impl_->serial; impl_->pending.reset(); impl_->ready.reset(); impl_->counts.pending = false;
}
std::unique_ptr<PlayerReviewCompileResult> PlayerReviewCompileWorker::takeCompletion()
{
    std::lock_guard<std::mutex> lock(impl_->mutex); return std::move(impl_->ready);
}
PlayerReviewCompileStats PlayerReviewCompileWorker::stats() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex); return impl_->counts;
}
