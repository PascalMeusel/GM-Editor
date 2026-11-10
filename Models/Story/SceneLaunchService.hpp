#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <string>
#include <vector>

namespace SceneLaunch
{
inline constexpr const char *kSceneLaunchNoteTag = "scene-launch";

struct ScenePresentationUpdate
{
    bool publicScene = false;
    bool publicMap = false;
    bool publicEncounter = false;
    bool displayEnabled = false;
    int visibleNpcCount = 0;
    int publicHandoutCount = 0;
};

struct SceneLiveStateUpdate
{
    ScenePresentationUpdate presentation;
    int mapLinkCount = 0;
    int encounterLinkCount = 0;
    int npcLinkCount = 0;
    int itemLinkCount = 0;
    int audioLinkCount = 0;
    EntityId firstMapId;
    EntityId firstEncounterId;
    bool partyMapUpdated = false;
    bool sessionStoryLinked = false;
    bool sessionEncounterLinked = false;
    bool hasNpcLink = false;
    bool hasItemLink = false;
};

std::vector<StoryLinkModel> collectSceneLinks(const StoryThreadModel &thread, const StoryBeatModel &beat);
ScenePresentationUpdate applyPresentationStateForScene(Campaign &campaign, const StoryBeatModel &beat);
SceneLiveStateUpdate applyLiveStateForScene(Campaign &campaign, StoryThreadModel &thread, StoryBeatModel &beat);
std::string describePresentationUpdate(const ScenePresentationUpdate &update);
std::string describeLiveStateUpdate(const SceneLiveStateUpdate &update);
void appendLaunchReceipt(Campaign &campaign, const StoryBeatModel &beat, const std::string &receipt);
}
