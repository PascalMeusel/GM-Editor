#include "Models/Media/FrozenImageDecodeInput.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace
{
bool same(const FrozenImageAsset &a,const FrozenImageAsset &b)
{
    std::owner_less<std::weak_ptr<const FrozenImageAssetGeneration>> less;
    return !less(a.lifetime(),b.lifetime()) && !less(b.lifetime(),a.lifetime()) &&
        a.path().lexically_normal()==b.path().lexically_normal() && a.contentHash()==b.contentHash();
}
}
struct FrozenImageDecodeWorker::Impl
{
    struct Job
    {
        std::uint64_t ticket=0;
        FrozenImageAsset asset;
        std::shared_ptr<const FrozenImageStorage> storage;
        std::atomic<bool> cancelled{false};
        std::unique_ptr<FrozenImageDecodeResult> result;
    };
    FrozenImageDecodeOptions options;
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::unique_ptr<Job>> queue;
    Job *active=nullptr;
    std::unique_ptr<Job> completion;
    FrozenImageDecodeStats totals;
    std::uint64_t next=0;
    bool stopping=false;
    std::thread worker;

    explicit Impl(FrozenImageDecodeOptions value):options(std::move(value)),worker([this]{run();}) {}
    bool retired(const Job &job) const { return job.cancelled.load() || !job.asset.valid(); }
    auto cancelledQueued()
    { return std::find_if(queue.begin(),queue.end(),[this](const auto &job){return retired(*job);}); }
    void run()
    {
        for (;;)
        {
            std::unique_ptr<Job> job;
            bool execute=false;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock,[&]{return stopping || cancelledQueued()!=queue.end() ||
                    (completion && completion->result && retired(*completion)) || (!completion && !queue.empty());});
                const auto cancelled=cancelledQueued();
                if (cancelled!=queue.end())
                { job=std::move(*cancelled);queue.erase(cancelled);++totals.cancelled; }
                else if (completion && completion->result && (stopping || retired(*completion)))
                { job=std::move(completion);++totals.cancelled; }
                else if (stopping)
                {
                    if (queue.empty()) {completion.reset();return;}
                    job=std::move(queue.front());queue.pop_front();++totals.cancelled;
                }
                else if (!completion && !queue.empty())
                {
                    job=std::move(queue.front());queue.pop_front();active=job.get();
                    ++totals.started;execute=true;
                }
            }
            if (!execute) continue; // All retired storage/pixels are destroyed here, on this worker.
            try
            {
                gm::frozen_decode::decode(job->asset,*job->storage,job->cancelled,options,*job->result);
                job->storage.reset(); // Completion never prolongs file storage or consent.
                if (options.phaseHook) options.phaseHook(job->ticket,FrozenImageDecodePhase::Ready);
            }
            catch (const std::exception &error)
            {
                job->result->success=false;job->result->image={};
                try {job->result->error=error.what();} catch (...) {}
            }
            catch (...) {job->result->success=false;job->result->image={};}
            job->storage.reset();
            {
                std::lock_guard<std::mutex> lock(mutex);
                active=nullptr;
                if (!stopping && !retired(*job)) {completion=std::move(job);++totals.completed;}
                else ++totals.cancelled;
            }
            wake.notify_all();
        }
    }
};

FrozenImageDecodeWorker::FrozenImageDecodeWorker(FrozenImageDecodeOptions options)
    :impl_(std::make_unique<Impl>(std::move(options))) {}
FrozenImageDecodeWorker::~FrozenImageDecodeWorker() {shutdown();}

std::uint64_t FrozenImageDecodeWorker::request(const FrozenImageAsset &asset)
{
    if (!asset.valid()) return 0;
    auto job=std::make_unique<Impl::Job>();
    job->asset=asset;job->result=std::make_unique<FrozenImageDecodeResult>();
    job->result->asset=asset;job->storage=asset.leaseStorage();
    if (!job->storage) return 0;
    // Declare the lock last: rejected/failed admission releases storage only
    // after unlocking. The queue owns every admitted lease from this point.
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->stopping) return 0;
    const auto match=[&](const auto &job){return job && !impl_->retired(*job) && same(job->asset,asset);};
    if (match(impl_->active)) return impl_->active->ticket;
    if (match(impl_->completion)) return impl_->completion->ticket;
    for (const auto &job:impl_->queue) if (match(job)) return job->ticket;
    if (impl_->queue.size()>=impl_->options.maxQueued) return 0;
    job->ticket=++impl_->next;
    job->result->ticket=job->ticket;
    const auto ticket=job->ticket;
    impl_->queue.push_back(std::move(job));
    impl_->wake.notify_one();
    return ticket;
}
void FrozenImageDecodeWorker::cancel(std::uint64_t ticket)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->active && impl_->active->ticket==ticket) impl_->active->cancelled=true;
    if (impl_->completion && impl_->completion->ticket==ticket) impl_->completion->cancelled=true;
    for (const auto &job:impl_->queue) if (job->ticket==ticket) job->cancelled=true;
    impl_->wake.notify_one();
}
void FrozenImageDecodeWorker::cancelAll()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->active) impl_->active->cancelled=true;
    if (impl_->completion) impl_->completion->cancelled=true;
    for (const auto &job:impl_->queue) job->cancelled=true;
    impl_->wake.notify_one();
}
std::unique_ptr<FrozenImageDecodeResult> FrozenImageDecodeWorker::takeCompletion()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->completion || impl_->retired(*impl_->completion)) return {};
    return std::move(impl_->completion->result);
}
void FrozenImageDecodeWorker::acknowledge(std::uint64_t ticket)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->completion && impl_->completion->ticket==ticket && !impl_->completion->result)
        impl_->completion.reset(); // No storage or pixel buffer remains in this sentinel.
    impl_->wake.notify_one();
}
FrozenImageDecodeStats FrozenImageDecodeWorker::stats() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto stats=impl_->totals;
    stats.queued=impl_->queue.size();stats.running=impl_->active!=nullptr;
    stats.completionOutstanding=impl_->completion!=nullptr;
    return stats;
}
void FrozenImageDecodeWorker::shutdown()
{
    if (!impl_) return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->stopping=true;
        if (impl_->active) impl_->active->cancelled=true;
        for (const auto &job:impl_->queue) job->cancelled=true;
        impl_->wake.notify_one();
    }
    if (impl_->worker.joinable()) impl_->worker.join();
}
