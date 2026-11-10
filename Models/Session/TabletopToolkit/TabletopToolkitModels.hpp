#pragma once

#include "Models/Core/EntityMetadata.hpp"

#include <string>
#include <vector>

enum class StoryObjectiveStatus
{
    Planned,
    Active,
    Completed,
    Failed
};

enum class StoryClueStatus
{
    Hidden,
    Discovered,
    Shared
};

enum class SafetyBoundaryKind
{
    Line,
    Veil
};

struct CampaignDiceMacroModel
{
    EntityMetadata metadata{"dice_macro"};
    std::string name = "Roll";
    std::string expression = "1d20";
    std::string notes;
    bool addToSessionNotes = true;
};

struct StoryObjectiveModel
{
    EntityMetadata metadata{"story_objective"};
    std::string title = "New objective";
    std::string playerSummary;
    std::string gmNotes;
    StoryObjectiveStatus status = StoryObjectiveStatus::Planned;
    bool playerVisible = false;
    EntityId linkedStoryThreadId;
    EntityId linkedStoryBeatId;
    std::vector<EntityId> assigneeCharacterIds;
    std::vector<EntityId> prerequisiteObjectiveIds;
};

struct StoryClueModel
{
    EntityMetadata metadata{"story_clue"};
    std::string title = "New clue";
    std::string playerText;
    std::string gmTruth;
    StoryClueStatus status = StoryClueStatus::Hidden;
    bool playerVisible = false;
    EntityId sourceEntityId;
    std::string sourceEntityType;
    std::string discoveredAtUtc;
};

struct SafetyBoundaryModel
{
    EntityMetadata metadata{"safety_boundary"};
    SafetyBoundaryKind kind = SafetyBoundaryKind::Line;
    std::string topic;
    std::string guidance;
    bool shareWithPlayers = false;
    bool active = true;
};

struct SafetyContentWarningModel
{
    EntityMetadata metadata{"safety_warning"};
    EntityId targetEntityId;
    std::string targetEntityType;
    std::string targetLabel;
    std::string warning;
    bool shareWithPlayers = true;
};

struct CampaignSafetyProfileModel
{
    bool enabled = true;
    std::string pauseLabel = "Pause";
    std::string pauseProtocol = "Stop the scene, check in, and resume only when the table is ready.";
    bool pauseActive = false;
    std::string pauseRequestedAtUtc;
    bool checkInActive = false;
    std::string checkInPrompt = "Is everyone comfortable continuing?";
    std::string checkInStartedAtUtc;
    std::vector<SafetyBoundaryModel> boundaries;
    std::vector<SafetyContentWarningModel> warnings;
};

struct TabletopDiceRollResult
{
    bool valid = false;
    int total = 0;
    std::vector<int> rolls;
    std::string expression;
    std::string label;
    std::string message;
    std::string error;
    std::string rolledAtUtc;
};

struct PlayerSafeObjective
{
    EntityId id;
    std::string title;
    std::string summary;
    StoryObjectiveStatus status = StoryObjectiveStatus::Planned;
};

struct PlayerSafeClue
{
    EntityId id;
    std::string title;
    std::string text;
    std::string discoveredAtUtc;
};

struct PlayerSafeSafetyBoundary
{
    SafetyBoundaryKind kind = SafetyBoundaryKind::Line;
    std::string topic;
    std::string guidance;
};

struct PlayerSafeContentWarning
{
    EntityId targetEntityId;
    std::string targetEntityType;
    std::string targetLabel;
    std::string warning;
};
