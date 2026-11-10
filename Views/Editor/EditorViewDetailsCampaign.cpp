#include "Views/Editor/EditorViewDetails.hpp"

namespace gm::editor_view_detail
{
const SessionModel *activeSession(const Campaign &campaign)
{
    const auto found = std::find_if(campaign.sessions.begin(), campaign.sessions.end(),
                                    [](const SessionModel &session) { return session.liveModeActive; });
    return found == campaign.sessions.end() ? nullptr : &(*found);
}

const StoryThreadModel *activeStoryThread(const Campaign &campaign)
{
    const auto active =
        std::find_if(campaign.storyThreads.begin(), campaign.storyThreads.end(),
                     [](const StoryThreadModel &thread) { return thread.status == StoryStatus::Active; });
    if (active != campaign.storyThreads.end())
        return &(*active);
    return nullptr;
}

const StoryBeatModel *activeStoryBeat(const StoryThreadModel *thread)
{
    if (!thread)
        return nullptr;

    const auto active = std::find_if(thread->beats.begin(), thread->beats.end(),
                                     [](const StoryBeatModel &beat) { return beat.status == StoryStatus::Active; });
    return active == thread->beats.end() ? nullptr : &(*active);
}

const StoryBeatModel *storyBeatById(const Campaign &campaign, const EntityId &beatId)
{
    if (beatId.empty())
        return nullptr;

    for (const StoryThreadModel &thread : campaign.storyThreads)
    {
        const auto found = std::find_if(thread.beats.begin(), thread.beats.end(),
                                        [&](const StoryBeatModel &beat) { return beat.metadata.id == beatId; });
        if (found != thread.beats.end())
            return &(*found);
    }
    return nullptr;
}

bool metadataHasTag(const EntityMetadata &metadata, const std::string &tag)
{
    return std::find(metadata.tags.begin(), metadata.tags.end(), tag) != metadata.tags.end();
}

const MapModel *hubActiveMap(const Campaign &campaign)
{
    const EntityId mapId =
        !campaign.partyLocation.mapId.empty() ? campaign.partyLocation.mapId : campaign.partyTravel.currentMapId;
    const auto selectedMap = std::find_if(campaign.maps.begin(), campaign.maps.end(),
                                          [&](const MapModel &map) { return map.metadata.id == mapId; });
    if (selectedMap != campaign.maps.end())
        return &(*selectedMap);

    const auto mainMap =
        std::find_if(campaign.maps.begin(), campaign.maps.end(), [](const MapModel &map) { return map.isMainMap; });
    if (mainMap != campaign.maps.end())
        return &(*mainMap);
    return campaign.maps.empty() ? nullptr : &campaign.maps.front();
}

bool encounterHasLiveState(const EncounterModel &encounter)
{
    return encounter.round > 1 || encounter.activeParticipantIndex > 0 || !encounter.combatLog.empty() ||
           encounter.combatPaused;
}

const EncounterModel *hubActiveEncounter(const Campaign &campaign)
{
    const auto liveEncounter =
        std::find_if(campaign.encounters.begin(), campaign.encounters.end(), encounterHasLiveState);
    if (liveEncounter != campaign.encounters.end())
        return &(*liveEncounter);
    return campaign.encounters.empty() ? nullptr : &campaign.encounters.front();
}

std::string emptyFallback(const std::string &value, const char *fallback)
{
    return value.empty() ? std::string(fallback) : value;
}

std::string compactWidgetText(const std::string &value, std::size_t maxLength)
{
    if (value.size() <= maxLength || maxLength <= 3)
        return value;
    return value.substr(0, maxLength - 3) + "...";
}

std::string audioAssetDisplayName(const AudioAssetModel &asset)
{
    if (!asset.name.empty())
        return asset.name;
    if (!asset.originalFileName.empty())
        return asset.originalFileName;
    return "Untitled audio";
}

const AudioAssetModel *audioAssetById(const Campaign &campaign, const EntityId &assetId)
{
    const auto found = std::find_if(campaign.audioAssets.begin(), campaign.audioAssets.end(),
                                    [&](const AudioAssetModel &asset) { return asset.metadata.id == assetId; });
    return found == campaign.audioAssets.end() ? nullptr : &(*found);
}

AudioAssetModel *audioAssetById(Campaign &campaign, const EntityId &assetId)
{
    const auto found = std::find_if(campaign.audioAssets.begin(), campaign.audioAssets.end(),
                                    [&](const AudioAssetModel &asset) { return asset.metadata.id == assetId; });
    return found == campaign.audioAssets.end() ? nullptr : &(*found);
}

const AudioAmbienceLayerModel *ambienceLayerById(const Campaign &campaign, const EntityId &layerId)
{
    const auto found = std::find_if(campaign.audioAmbienceLayers.begin(), campaign.audioAmbienceLayers.end(),
                                    [&](const AudioAmbienceLayerModel &layer) { return layer.metadata.id == layerId; });
    return found == campaign.audioAmbienceLayers.end() ? nullptr : &(*found);
}

std::string audioChannelDisplayName(const Campaign &campaign, const EntityId &channelId)
{
    if (const AudioAssetModel *asset = audioAssetById(campaign, channelId))
        return audioAssetDisplayName(*asset);
    if (const AudioAmbienceLayerModel *layer = ambienceLayerById(campaign, channelId))
        return layer->name.empty() ? std::string("Ambience layer") : layer->name;
    return channelId.empty() ? std::string("None") : channelId;
}

int playlistIndexOf(const std::vector<EntityId> &queue, const EntityId &assetId)
{
    const auto found = std::find(queue.begin(), queue.end(), assetId);
    return found == queue.end() ? -1 : static_cast<int>(std::distance(queue.begin(), found));
}

EntityId playlistNextId(const AudioPlaylistPlaybackState &state, int currentIndex)
{
    if (state.queueAssetIds.empty() || currentIndex < 0)
        return {};

    const int nextIndex = currentIndex + 1;
    if (nextIndex < static_cast<int>(state.queueAssetIds.size()))
        return state.queueAssetIds[static_cast<std::size_t>(nextIndex)];
    return state.repeat ? state.queueAssetIds.front() : EntityId{};
}

bool playAudioAsset(Campaign &campaign, AudioPlaybackService &playback, const EntityId &assetId, bool oneShot,
                    std::string &errorMessage)
{
    AudioAssetModel *asset = audioAssetById(campaign, assetId);
    if (!asset)
    {
        errorMessage = localizedUiText("Playlist item is missing from campaign audio.");
        return false;
    }

    const float volume = AudioMix::effectiveVolume(campaign, *asset, asset->defaultVolume);
    return playback.play(*asset, ProjectAssets::resolveAudioPath(*asset), volume, oneShot ? false : asset->loop,
                         errorMessage);
}

void publishMiniPlaylistState(AudioPlaybackService &playback, AudioPlaylistPlaybackState state,
                              const EntityId &currentId)
{
    if (!state.active)
        return;

    const int currentIndex = playlistIndexOf(state.queueAssetIds, currentId);
    state.previousAssetId = state.currentAssetId == currentId ? state.previousAssetId : state.currentAssetId;
    state.currentAssetId = currentId;
    state.lastAssetId = currentId;
    state.currentIndex = currentIndex;
    state.nextAssetId = playlistNextId(state, currentIndex);
    state.transitionActive = false;
    state.transitionLabel = localizedUiText("Mini player");
    playback.setPlaylistPlaybackState(std::move(state));
}

std::string campaignModeLabel(const Campaign &campaign)
{
    if (activeSession(campaign))
        return "Live Session";
    if (campaign.presentationModeActive || campaign.localPlayerDisplay.enabled)
        return "Presentation Prep";
    return "Prep";
}

std::string campaignSelectionLabel(const Campaign &campaign)
{
    if (const SessionModel *session = activeSession(campaign))
        return session->title.empty() ? "Active live session" : session->title;

    if (!campaign.partyLocation.mapId.empty())
    {
        const auto foundMap = std::find_if(campaign.maps.begin(), campaign.maps.end(), [&](const MapModel &map) {
            return map.metadata.id == campaign.partyLocation.mapId;
        });
        if (foundMap != campaign.maps.end())
            return "Party on " + foundMap->name;
    }

    if (!campaign.name.empty())
        return campaign.name;
    return "Campaign";
}

std::string storySceneLabel(const Campaign &campaign, const EntityId &sceneId)
{
    if (sceneId.empty())
        return localizedUiLabel("None");

    for (const StoryThreadModel &thread : campaign.storyThreads)
    {
        for (const StoryBeatModel &beat : thread.beats)
        {
            if (beat.metadata.id == sceneId)
                return (thread.title.empty() ? localizedUiLabel("Untitled story") : thread.title) + " / " +
                       (beat.title.empty() ? localizedUiLabel("Untitled scene") : beat.title);
        }
    }
    return localizedUiLabel("Missing scene");
}

std::string mapLabel(const Campaign &campaign, const EntityId &mapId)
{
    if (mapId.empty())
        return localizedUiLabel("None");

    const auto found = std::find_if(campaign.maps.begin(), campaign.maps.end(),
                                    [&](const MapModel &map) { return map.metadata.id == mapId; });
    if (found == campaign.maps.end())
        return localizedUiLabel("Missing map");
    return found->name.empty() ? localizedUiLabel("Untitled map") : found->name;
}

std::string encounterLabel(const Campaign &campaign, const EntityId &encounterId)
{
    if (encounterId.empty())
        return localizedUiLabel("None");

    const auto found =
        std::find_if(campaign.encounters.begin(), campaign.encounters.end(),
                     [&](const EncounterModel &encounter) { return encounter.metadata.id == encounterId; });
    if (found == campaign.encounters.end())
        return localizedUiLabel("Missing encounter");
    return found->name.empty() ? localizedUiLabel("Untitled encounter") : found->name;
}

std::string presentationSelectionLabel(const Campaign &campaign)
{
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    const std::string scene = storySceneLabel(campaign, display.publicSceneId);
    const std::string map = mapLabel(campaign, display.publicMapId);
    const std::string encounter = encounterLabel(campaign, display.publicEncounterId);
    return formatLocalizedUi("Scene: %s | Map: %s | Initiative: %s", scene.c_str(), map.c_str(), encounter.c_str());
}

std::string campaignNextActionLabel(const Campaign &campaign, const ProjectSaveState &saveState)
{
    if (saveState.dirty)
        return "Save changes or choose a task mode.";
    if (activeSession(campaign))
        return "Open Live Dashboard or Combat.";
    if (campaign.characters.empty())
        return "Create or import the first player.";
    if (campaign.storyThreads.empty())
        return "Plan a scene on the story board.";
    if (campaign.maps.empty())
        return "Create or open a map.";
    return "Choose Prep Session, Run Table, or Advanced/System.";
}

std::string sessionInWorldClockLabel(const Campaign &campaign, const SessionModel *session)
{
    const std::string campaignClock = InWorldTime::formatDateTime(campaign.clock);
    if (!campaignClock.empty())
        return campaignClock;

    if (session)
    {
        const std::string sessionClock = SessionControl::currentInWorldTime(*session);
        if (!sessionClock.empty())
            return sessionClock;

        for (auto eventIt = campaign.timelineEvents.rbegin(); eventIt != campaign.timelineEvents.rend(); ++eventIt)
        {
            if (!eventIt->inWorldDate.empty() && metadataHasTag(eventIt->metadata, kSessionClockNoteTag) &&
                std::find(eventIt->relatedEntityIds.begin(), eventIt->relatedEntityIds.end(), session->metadata.id) !=
                    eventIt->relatedEntityIds.end())
            {
                return eventIt->inWorldDate;
            }
        }

        for (const EntityId &storyId : session->storyDocumentIds)
        {
            const StoryBeatModel *beat = storyBeatById(campaign, storyId);
            if (beat && !beat->inWorldDate.empty())
                return beat->inWorldDate;
        }
    }

    if (const StoryBeatModel *beat = activeStoryBeat(activeStoryThread(campaign)))
    {
        if (!beat->inWorldDate.empty())
            return beat->inWorldDate;
    }

    return {};
}

std::string inWorldClockLabel(const Campaign &campaign)
{
    const std::string clockLabel = sessionInWorldClockLabel(campaign, activeSession(campaign));
    return clockLabel.empty() ? std::string("-") : clockLabel;
}

ImU32 miniMapFeatureColor(MapFeatureKind kind)
{
    switch (kind)
    {
    case MapFeatureKind::Trap:
    case MapFeatureKind::Hazard:
        return IM_COL32(238, 112, 92, 235);
    case MapFeatureKind::Door:
    case MapFeatureKind::SecretDoor:
        return IM_COL32(226, 185, 92, 235);
    case MapFeatureKind::NamedLocation:
    case MapFeatureKind::Pin:
    default:
        return IM_COL32(132, 205, 196, 235);
    }
}

std::string sessionClockModeLabel(const SessionModel *session, bool breakActive)
{
    if (!session)
        return "Prep / no live session";
    return breakActive ? "Break / clock paused" : "Live session running";
}

std::string sessionClockNextActionLabel(const SessionModel *session, bool breakActive)
{
    if (!session)
        return "Start the session when the table is ready.";
    if (breakActive)
        return "Resume table when the break ends.";
    return "Start break, open dashboard, or end session.";
}

}
