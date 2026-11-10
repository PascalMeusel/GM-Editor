#pragma once

#include "Models/Map/MapPackageService.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

enum class MapCheckpointKind
{
    Named,
    CrashJournal
};

struct MapCheckpointInfo
{
    std::string id;
    std::string name;
    std::string createdAtUtc;
    std::uint64_t createdUnixNanos = 0;
    EntityId mapId;
    std::uint64_t sourceRevision = 0;
    MapCheckpointKind kind = MapCheckpointKind::Named;
    std::string transactionLabel;
    std::string packageFingerprint;
    std::uint64_t packageBytes = 0;
    bool valid = false;
    std::string error;
};

struct MapCheckpointListResult
{
    bool success = false;
    std::vector<MapCheckpointInfo> checkpoints;
    std::string error;
};

struct MapCheckpointWriteResult
{
    bool success = false;
    MapCheckpointInfo checkpoint;
    std::vector<std::string> prunedCheckpointIds;
    std::string error;
    bool cancelled = false;
};

// Execute publication only while the caller's revision is still eligible.
// Expensive serialization finishes before this guard is entered.
using MapCheckpointPublicationGate = std::function<bool(const std::function<bool()> &)>;

// Restore is deliberately a two-step operation. The preview records both the
// package fingerprint and the current map revision. applyRestore refuses a
// stale preview and also requires an explicit confirmation flag.
struct MapCheckpointRestorePreview
{
    bool valid = false;
    bool recovery = false;
    MapCheckpointInfo checkpoint;
    MapPackagePreview package;
    EntityId targetMapId;
    std::uint64_t targetRevision = 0;
    std::string error;
};

class MapCheckpointService
{
public:
    static constexpr std::size_t MaximumNamedCheckpoints = 20;

    // projectDirectory must identify the explicit campaign/project directory.
    // Checkpoints are kept below .gm-editor/map-checkpoints and never resolve
    // user-provided map or checkpoint IDs as path segments.
    explicit MapCheckpointService(std::filesystem::path projectDirectory,
                                  std::size_t namedCheckpointLimit = MaximumNamedCheckpoints);

    MapCheckpointWriteResult createNamedCheckpoint(const MapModel &map, const std::string &name) const;
    MapCheckpointListResult listNamedCheckpoints(const EntityId &mapId) const;

    MapCheckpointRestorePreview previewRestore(const MapModel &currentMap,
                                                const std::string &checkpointId) const;
    bool applyRestore(const MapCheckpointRestorePreview &preview,
                      MapModel &targetMap,
                      bool confirmed,
                      std::string *error = nullptr) const;

    bool removeNamedCheckpoint(const EntityId &mapId,
                               const std::string &checkpointId,
                               std::string *error = nullptr) const;

    // Call after committing one editor transaction. Only the newest journal
    // snapshot is retained, and replacement uses an atomic directory swap.
    MapCheckpointWriteResult writeCrashJournal(const MapModel &map,
                                                const std::string &transactionLabel,
                                                const MapCheckpointPublicationGate &publish = {}) const;
    MapCheckpointRestorePreview previewRecovery(const MapModel &currentMap) const;
    bool discardRecovery(const EntityId &mapId, std::string *error = nullptr) const;

    const std::filesystem::path &projectDirectory() const { return projectDirectory_; }
    std::size_t namedCheckpointLimit() const { return namedCheckpointLimit_; }

private:
    std::filesystem::path projectDirectory_;
    std::size_t namedCheckpointLimit_ = MaximumNamedCheckpoints;
};
