#pragma once
#include "Models/Session/PlayerReviewSession.hpp"
#include <filesystem>

struct PlayerReviewSession::State
{
    EntityId campaignId;
    const Campaign *campaign=nullptr;
    std::filesystem::path root;
    PlayerDisplayReviewSnapshot review;
    LocalPlayerDisplayModel display;
    MapModel gmMap,playerMap;
    PlayerDisplayPublicationAssets playerAssets,gmAssets;
    std::shared_ptr<gm::map_render::MapRenderCompileMemo> compileMemo;
    std::uint64_t sourceKey=0;
    bool maps=false,gmArtOmitted=false;
};
struct PlayerReviewSession::Preparation
{
    // Context can survive an unavailable worker so the user may explicitly
    // choose the existing foreground preparation path.
    std::unique_ptr<PlayerReviewCompileWorker> worker;
    const Campaign *campaign=nullptr;
    EntityId campaignId;
    std::string locale;
    std::filesystem::path root;
    std::uint64_t contextEpoch=0,serial=0,sourceKey=0;
    PlayerReviewPreparationStatus status=PlayerReviewPreparationStatus::Idle;
    std::optional<std::vector<std::uint8_t>> environment;
    std::unique_ptr<PlayerReviewCompileResult> fallback;
    PlayerReviewAcceptedCompileStats accepted;
};
