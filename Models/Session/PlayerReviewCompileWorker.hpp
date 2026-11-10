#pragma once

#include "Models/Map/MapRenderCompileMemo.hpp"
#include "Models/Map/MapRenderPlanMeasurements.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

enum class PlayerReviewCompilePhase { BeforeGm, BeforePlayer, Ready };
struct PlayerReviewCompileOptions
{
    gm::map_render::MapRenderCompileMemoLimits limits;
    // Optional owned deterministic barrier/failure seam. Production leaves it
    // empty. Hooks must never capture campaign, UI or service references.
    std::function<void(std::uint64_t, PlayerReviewCompilePhase)> phaseHook;
};
struct PlayerReviewCompileStats
{
    std::uint64_t started = 0, completed = 0, discarded = 0;
    bool running = false, pending = false;
};
struct PlayerReviewAcceptedCompileStats
{
    gm::map_render::MapRenderCompileMemoStats gm, player;
};
struct PlayerReviewCompileInput
{
    std::shared_ptr<const MapModel> gmMap, playerMap;
    // Incoming callbacks are refused. Worker callbacks below use only owned
    // per-occurrence decisions, including duplicate feature IDs.
    MapRenderPlanRequest gmRequest, playerRequest;
    std::vector<unsigned char> playerPolicy;
    bool measure=false;
};
struct PlayerReviewCompileResult
{
    std::uint64_t serial = 0;
    bool foregroundRequired = false;
    std::string error;
    std::shared_ptr<gm::map_render::MapRenderCompileMemo> gm, player;
    MapRenderPlanMeasurements gmMeasurements, playerMeasurements;
    std::optional<std::vector<std::uint8_t>> environment;
};
std::optional<std::vector<std::uint8_t>> playerReviewCompileEnvironment() noexcept;

// Persistent across popup close/reset. One CPU worker and one latest pending
// immutable job. cancel() never joins; destruction joins only at owner teardown.
class PlayerReviewCompileWorker
{
  public:
    explicit PlayerReviewCompileWorker(PlayerReviewCompileOptions options = {});
    ~PlayerReviewCompileWorker();
    PlayerReviewCompileWorker(const PlayerReviewCompileWorker &) = delete;
    PlayerReviewCompileWorker &operator=(const PlayerReviewCompileWorker &) = delete;
    std::uint64_t request(PlayerReviewCompileInput input);
    void cancel();
    std::unique_ptr<PlayerReviewCompileResult> takeCompletion();
    PlayerReviewCompileStats stats() const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
