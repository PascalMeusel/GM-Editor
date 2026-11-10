#pragma once

#include "Models/Project/CampaignIndexRebuildService.hpp"
#include "Models/Project/CampaignSearchIndex.hpp"
#include "Models/UX/WorkspaceNavigation.hpp"
#include "Views/Editor/SessionPreparationTarget.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

struct Campaign;
class EditorWorldHistory;
class EditorMarkdownExchange;
enum class PlayerReviewSource { Npc, Item, Inventory };
struct EditorPlayerReviewEnvelope
{
    PlayerReviewSource source = PlayerReviewSource::Npc;
    EntityId sourceId, ownerId, campaignId;
    const Campaign *campaign = nullptr; // Compared only; never dereferenced.
    std::filesystem::path root;
    std::string locale, editorRoute;
    std::uint64_t epoch = 0;
    gm::ux::WorkspaceTarget workspace = gm::ux::WorkspaceTarget::Home;
};
struct EditorPreparationTargetEnvelope
{
    PreparationTargetRequest request;
    const Campaign *campaign = nullptr; // Compared only; never dereferenced.
    std::filesystem::path root;
    std::uint64_t epoch = 0;
    gm::ux::WorkspaceTarget workspace = gm::ux::WorkspaceTarget::Home;
};

struct EditorWorkspaceState
{
    gm::ux::WorkspaceNavigation navigation;
    bool openingEntity = false;
    CampaignIndexRebuildResult lastIndexRebuildResult;
    mutable CampaignSearchIndex cachedSearchIndex;
    mutable bool searchIndexDirty = true;
    std::uint64_t preparationEpoch = 0;
    std::optional<EditorPreparationTargetEnvelope> preparationTarget;
    std::optional<EditorPlayerReviewEnvelope> playerReviewTarget;
    std::string playerReviewError; // Generic overlay feedback; never source content.
    std::shared_ptr<EditorWorldHistory> worldHistory;
    std::shared_ptr<EditorMarkdownExchange> markdownExchange;
};

struct EditorShortcutState
{
    std::string statusMessage;
    float preMuteMasterVolume = 1.0f;
    bool audioMuted = false;
};
