#pragma once
#include "Models/Project/WorldDraftRecovery.hpp"
#include <chrono>
#include <filesystem>

struct EditorWorldRecoveryState
{
    using Clock=std::chrono::steady_clock;
    EntityId campaignId;
    std::filesystem::path root;
    bool initialized=false,pending=false,dirty=false,cleanup=false;
    Clock::time_point attempted=Clock::now();
    WorldDraftRecovery::ReadResult recovered;
    std::string lastPayload,error;
};
