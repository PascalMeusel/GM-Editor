#include "Models/Map/MapCrashJournalWorker.hpp"
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
struct Gate
{
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false, released = false;
    void block()
    {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        changed.notify_all();
        require(changed.wait_for(lock, std::chrono::seconds(10), [&] { return released; }), "Gate timeout");
    }
    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        require(changed.wait_for(lock, std::chrono::seconds(10), [&] { return entered; }), "Worker timeout");
    }
    void release()
    {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        changed.notify_all();
    }
};
MapCrashJournalCompletion completion(MapCrashJournalWorker &worker)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (;;)
    {
        if (auto result = worker.takeCompletion())
            return *result;
        require(std::chrono::steady_clock::now() < deadline, "Completion timeout");
        std::this_thread::yield();
    }
}
MapModel fixture(int revision)
{
    MapModel map;
    map.metadata.id = "journal-worker-map";
    map.name = "Journal fixture " + std::to_string(revision);
    map.width = map.height = 8;
    map.cells.resize(64);
    map.revision = revision;
    return map;
}
}
int main()
{
    const auto root = std::filesystem::temp_directory_path() / createEntityId("journal-worker-tests");
    try
    {
        std::filesystem::create_directories(root);
        MapCheckpointService service(root);
        require(service.writeCrashJournal(fixture(1), "initial").success, "Initial journal failed");
        {
            Gate gate;
            MapCrashJournalWorker worker([&] { gate.block(); });
            worker.submit(root, fixture(2), "obsolete", 2, false);
            gate.wait();
            auto latest = fixture(3);
            worker.submit(root, latest, "latest", 3, false);
            latest.revision = 99;
            gate.release();
            const auto done = completion(worker);
            require(done.success && done.authoringSerial == 3, "Latest immutable revision not completed");
            const auto recovered = service.previewRecovery(fixture(1));
            require(recovered.valid && recovered.checkpoint.sourceRevision == 3, "Obsolete snapshot published");
        }
        {
            Gate gate;
            MapCrashJournalWorker worker([&] { gate.block(); });
            worker.submit(root, fixture(4), "cancelled", 4, false);
            gate.wait();
            worker.cancel();
            require(service.discardRecovery(fixture(1).metadata.id), "Discard failed");
            gate.release();
        }
        require(!service.previewRecovery(fixture(1)).valid, "Cancelled write resurrected discarded recovery");
        {
            MapCrashJournalWorker worker;
            worker.submit(root, fixture(5), "shutdown", 5, false);
        }
        require(service.previewRecovery(fixture(1)).checkpoint.sourceRevision == 5, "Shutdown lost latest edit");
        {
            MapCrashJournalWorker worker([] { throw std::runtime_error("Injected publication failure"); });
            worker.submit(root, fixture(6), "failure", 6, false);
            const auto done = completion(worker);
            require(!done.success && !done.error.empty(), "Failure hidden");
            require(service.previewRecovery(fixture(1)).checkpoint.sourceRevision == 5,
                    "Failure replaced valid journal");
        }
        {
            MapCrashJournalWorker worker;
            worker.submit(root, fixture(6), "retry", 6, false);
            require(completion(worker).success, "Retry failed");
            worker.submit(root, fixture(6), "undo to saved", 7, true);
            require(completion(worker).success, "Async discard failed");
        }
        require(!service.previewRecovery(fixture(1)).valid, "Clean state retains recovery");
        std::filesystem::remove_all(root);
        std::cout << "Recovery worker publication, cancellation, retry and shutdown controls passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\nEvidence retained at " << root << '\n';
        return 1;
    }
}
