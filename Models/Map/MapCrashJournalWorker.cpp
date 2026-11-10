#include "Models/Map/MapCrashJournalWorker.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

struct MapCrashJournalWorker::Impl
{
    struct Job
    {
        std::filesystem::path root;
        MapModel map;
        std::string label;
        std::uint64_t serial, epoch;
        bool discard;
    };
    explicit Impl(BeforePublish hook) : beforePublish(std::move(hook)), worker([this] { run(); })
    {
    }
    ~Impl()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            closing = true;
        }
        changed.notify_one();
        worker.join();
        if (completion && !completion->success)
            std::cerr << "Map recovery snapshot failed during shutdown: " << completion->error << '\n';
    }
    void run()
    {
        for (;;)
        {
            std::optional<Job> job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock, [&] { return closing || pending.has_value(); });
                if (!pending)
                    return;
                job = std::move(pending);
                pending.reset();
            }
            const auto started = std::chrono::steady_clock::now();
            MapCrashJournalCompletion result{job->root, job->map.metadata.id, job->serial};
            try
            {
                std::filesystem::create_directories(mapFileIoPath(job->root));
                MapCheckpointService service(job->root);
                auto publish = [&](const std::function<bool()> &operation) {
                    if (beforePublish)
                        beforePublish();
                    std::lock_guard<std::mutex> lock(mutex);
                    return job->epoch == epoch && operation();
                };
                if (job->discard)
                    result.success = publish([&] { return service.discardRecovery(result.mapId, &result.error); });
                else
                {
                    const auto written = service.writeCrashJournal(job->map, job->label, publish);
                    result.success = written.success;
                    result.error = written.error;
                }
            }
            catch (const std::exception &error)
            {
                result.error = error.what();
            }
            catch (...)
            {
                result.error = "Unknown recovery snapshot failure.";
            }
            result.milliseconds =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            std::lock_guard<std::mutex> lock(mutex);
            if (job->epoch == epoch)
                completion = std::move(result);
        }
    }
    BeforePublish beforePublish;
    std::mutex mutex;
    std::condition_variable changed;
    bool closing = false;
    std::uint64_t epoch = 0;
    std::optional<Job> pending;
    std::optional<MapCrashJournalCompletion> completion;
    std::thread worker;
};

MapCrashJournalWorker::MapCrashJournalWorker(BeforePublish hook) : impl_(std::make_unique<Impl>(std::move(hook)))
{
}
MapCrashJournalWorker::~MapCrashJournalWorker() = default;

void MapCrashJournalWorker::submit(const std::filesystem::path &root, const MapModel &map, const std::string &label,
                                   std::uint64_t serial, bool discard)
{
    Impl::Job job{root, map, label, serial, 0, discard};
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        job.epoch = ++impl_->epoch;
        impl_->pending = std::move(job);
        impl_->completion.reset();
    }
    impl_->changed.notify_one();
}
std::optional<MapCrashJournalCompletion> MapCrashJournalWorker::takeCompletion()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto result = std::move(impl_->completion);
    impl_->completion.reset();
    return result;
}
void MapCrashJournalWorker::cancel()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    ++impl_->epoch;
    impl_->pending.reset();
    impl_->completion.reset();
}
