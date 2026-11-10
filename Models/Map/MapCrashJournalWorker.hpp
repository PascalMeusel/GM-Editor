#pragma once

#include "Models/Map/MapCheckpointService.hpp"
#include <memory>
#include <optional>

struct MapCrashJournalCompletion
{
    std::filesystem::path projectDirectory;
    EntityId mapId;
    std::uint64_t authoringSerial = 0;
    bool success = false;
    std::string error;
    double milliseconds = 0;
};

// UI-thread submission/polling; one running immutable snapshot and at most
// one latest pending snapshot. Shutdown drains the last completed edit.
class MapCrashJournalWorker
{
  public:
    using BeforePublish = std::function<void()>;
    explicit MapCrashJournalWorker(BeforePublish beforePublish = {});
    ~MapCrashJournalWorker();
    MapCrashJournalWorker(const MapCrashJournalWorker &) = delete;
    MapCrashJournalWorker &operator=(const MapCrashJournalWorker &) = delete;
    void submit(const std::filesystem::path &projectDirectory, const MapModel &map, const std::string &label,
                std::uint64_t authoringSerial, bool discard);
    std::optional<MapCrashJournalCompletion> takeCompletion();
    // After return no earlier job can publish. Call before synchronous save,
    // discard or import so recovery cannot reappear after those operations.
    void cancel();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
