#pragma once

#include "Models/Localization/LocalizationService.hpp"
#include "Models/Map/MapImportedReferencePolicy.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/MapStructuredCreation.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Session/PlayerSafeDataFilter.hpp"
#include "Models/Session/TabletopToolkit/TabletopToolkitService.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

enum class PlayerDisplayReviewIssue
{
    None,
    NoPublicPayload,
    MapOnlyNeedsPublicMap,
    MapUnavailable,
    SceneUnavailable,
    InitiativeUnavailable,
    NpcUnavailable,
    PartyUnavailable,
    HandoutUnavailable
};

struct PlayerDisplayReviewSnapshot
{
    bool eligible = false;
    bool hasMapPreview = false;
    PlayerDisplayReviewIssue issue = PlayerDisplayReviewIssue::NoPublicPayload;
    std::string locale = "de";
    std::string campaignName;
    std::uint64_t contentFingerprint = 0;
    MapRenderPlan gmMapPlan;
    MapRenderPlan playerMapPlan;
    // Every non-map channel is copied from its player-safe projection during
    // review. Detached player rendering must never re-read live campaign
    // entities after the GM approves this snapshot.
    PlayerSafeStoryScene playerScene;
    std::vector<PlayerSafeInitiativeEntry> playerInitiative;
    std::string playerEncounterName;
    int playerEncounterRound = 0;
    std::vector<PlayerSafeNpcSummary> playerNpcs;
    std::vector<PlayerSafeCharacterSummary> playerParty;
    std::vector<PlayerSafeInventoryItemSummary> playerInventory;
    // Immutable, content-bound handouts approved by the same review. The
    // detached view must consume these instead of projecting campaign data a
    // second time after publication.
    std::vector<PlayerSafeHandout> playerHandouts;
    std::vector<PlayerSafeObjective> playerObjectives;
    std::vector<PlayerSafeClue> playerClues;
    std::vector<PlayerSafeSafetyBoundary> playerSafetyBoundaries;
    std::vector<PlayerSafeContentWarning> playerContentWarnings;
};
