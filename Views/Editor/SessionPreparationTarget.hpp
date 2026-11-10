#pragma once

#include "Models/Session/SessionOutcomeDraftKeys.hpp"
#include <functional>
#include <string>

// Navigation intent copied from a successful carry receipt. It never owns a
// campaign, a view, or a persistence callback, and cannot replay carry-forward.
struct PreparationTargetRequest
{
    EntityId campaignId, sourceSessionId, targetSessionId, carryRequestId;
    SessionOutcomeDraftKeys::Surface sourceSurface = SessionOutcomeDraftKeys::Surface::Preparation;
};

enum class PreparationTargetStatus { Opened, NeedsReview, Unavailable, CheckpointFailed, StaleContext };
struct PreparationTargetResult
{
    PreparationTargetStatus status = PreparationTargetStatus::Unavailable;
    std::string message;
    bool opened() const { return status == PreparationTargetStatus::Opened; }
};
using PreparationTargetSink = std::function<void(PreparationTargetRequest)>;
