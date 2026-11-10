#pragma once

#include "Models/Core/EntityMetadata.hpp"
#include <string>
#include <vector>

enum class StoryThreadType
{
    MainStory = 0,
    CharacterStory,
    WorldStory,
    FactionStory,
    LocationStory,
    Mystery,
    SideQuest,
    SessionThread,
    HistoricalEvent,
    DungeonOrEncounter
};

enum class StoryStatus
{
    Idea = 0,
    Planned,
    Seeded,
    Active,
    Revealed,
    Changed,
    Resolved,
    Failed,
    Abandoned
};

enum class StoryLinkType
{
    Related = 0,
    Contains,
    DependsOn,
    Causes,
    Foreshadows,
    Reveals,
    Blocks,
    Resolves,
    BranchesTo,
    Contradicts,
    Retcons
};

inline const char *storyThreadTypeLabel(StoryThreadType type)
{
    switch (type)
    {
    case StoryThreadType::MainStory:
        return "Main Story";
    case StoryThreadType::CharacterStory:
        return "Character Story";
    case StoryThreadType::WorldStory:
        return "World Story";
    case StoryThreadType::FactionStory:
        return "Faction Story";
    case StoryThreadType::LocationStory:
        return "Location Story";
    case StoryThreadType::Mystery:
        return "Mystery";
    case StoryThreadType::SideQuest:
        return "Side Quest";
    case StoryThreadType::SessionThread:
        return "Session Thread";
    case StoryThreadType::HistoricalEvent:
        return "Historical Event";
    case StoryThreadType::DungeonOrEncounter:
        return "Dungeon/Encounter";
    }
    return "Story";
}

inline const char *storyStatusLabel(StoryStatus status)
{
    switch (status)
    {
    case StoryStatus::Idea:
        return "Idea";
    case StoryStatus::Planned:
        return "Planned";
    case StoryStatus::Seeded:
        return "Seeded";
    case StoryStatus::Active:
        return "Active";
    case StoryStatus::Revealed:
        return "Revealed";
    case StoryStatus::Changed:
        return "Changed";
    case StoryStatus::Resolved:
        return "Resolved";
    case StoryStatus::Failed:
        return "Failed";
    case StoryStatus::Abandoned:
        return "Abandoned";
    }
    return "Idea";
}

inline const char *storyLinkTypeLabel(StoryLinkType type)
{
    switch (type)
    {
    case StoryLinkType::Related:
        return "Related";
    case StoryLinkType::Contains:
        return "Contains";
    case StoryLinkType::DependsOn:
        return "Depends On";
    case StoryLinkType::Causes:
        return "Causes";
    case StoryLinkType::Foreshadows:
        return "Foreshadows";
    case StoryLinkType::Reveals:
        return "Reveals";
    case StoryLinkType::Blocks:
        return "Blocks";
    case StoryLinkType::Resolves:
        return "Resolves";
    case StoryLinkType::BranchesTo:
        return "Branches To";
    case StoryLinkType::Contradicts:
        return "Contradicts";
    case StoryLinkType::Retcons:
        return "Retcons";
    }
    return "Related";
}

struct StoryLinkModel
{
    EntityMetadata metadata{"story_link"};
    std::string label;
    StoryLinkType type = StoryLinkType::Related;
    EntityId targetEntityId;
    std::string targetEntityType;
};

struct StoryBeatModel
{
    EntityMetadata metadata{"story_beat"};
    std::string title;
    std::string summary;
    std::string playerSummary;
    std::string readAloudText;
    std::string clues;
    std::string consequences;
    std::string gmNotes;
    StoryStatus status = StoryStatus::Idea;
    std::string inWorldDate;
    EntityId cueAudioAssetId;
    float cueVolume = 1.0f;
    bool cueLoop = true;
    std::vector<StoryLinkModel> links;
};

struct StoryThreadModel
{
    EntityMetadata metadata{"story_thread"};
    std::string title;
    StoryThreadType type = StoryThreadType::MainStory;
    StoryStatus status = StoryStatus::Idea;
    std::string summary;
    std::string stakes;
    std::string gmNotes;
    std::vector<StoryBeatModel> beats;
    std::vector<StoryLinkModel> links;
};

struct StoryDocumentModel
{
    EntityMetadata metadata{"story"};
    std::string title;
    std::string body;
    std::vector<StoryLinkModel> links;
};

struct TimelineEventModel
{
    EntityMetadata metadata{"timeline"};
    std::string title;
    std::string description;
    std::string inWorldDate;
    std::vector<EntityId> relatedEntityIds;
};
