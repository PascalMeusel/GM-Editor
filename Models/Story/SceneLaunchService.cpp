#include "Models/Story/SceneLaunchService.hpp"
#include "Models/Localization/LocalizationService.hpp"

#include <algorithm>

namespace
{
bool addUniqueId(std::vector<EntityId> &ids, const EntityId &id)
{
    if (id.empty() || std::find(ids.begin(), ids.end(), id) != ids.end())
        return false;
    ids.push_back(id);
    return true;
}

const EncounterModel *findEncounterById(const Campaign &campaign, const EntityId &encounterId)
{
    const auto found = std::find_if(campaign.encounters.begin(), campaign.encounters.end(), [&](const EncounterModel &encounter) {
        return encounter.metadata.id == encounterId;
    });
    return found == campaign.encounters.end() ? nullptr : &(*found);
}

EntityId firstLinkedTarget(const StoryBeatModel &beat, const std::string &targetType)
{
    const auto found = std::find_if(beat.links.begin(), beat.links.end(), [&](const StoryLinkModel &link) {
        return link.targetEntityType == targetType && !link.targetEntityId.empty();
    });
    return found == beat.links.end() ? EntityId{} : found->targetEntityId;
}

EntityId firstLinkedTarget(const std::vector<StoryLinkModel> &links, const std::string &targetType)
{
    const auto found = std::find_if(links.begin(), links.end(), [&](const StoryLinkModel &link) {
        return link.targetEntityType == targetType && !link.targetEntityId.empty();
    });
    return found == links.end() ? EntityId{} : found->targetEntityId;
}

SessionModel *activeSession(Campaign &campaign)
{
    const auto found = std::find_if(campaign.sessions.begin(), campaign.sessions.end(), [](const SessionModel &session) {
        return session.liveModeActive;
    });
    return found == campaign.sessions.end() ? nullptr : &(*found);
}
}

namespace SceneLaunch
{
std::vector<StoryLinkModel> collectSceneLinks(const StoryThreadModel &thread, const StoryBeatModel &beat)
{
    std::vector<StoryLinkModel> links = thread.links;
    links.insert(links.end(), beat.links.begin(), beat.links.end());
    return links;
}

ScenePresentationUpdate applyPresentationStateForScene(Campaign &campaign, const StoryBeatModel &beat)
{
    ScenePresentationUpdate update;
    LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    display.publicSceneId = beat.metadata.id;
    // Scene-specific selections never inherit material from the previous scene.
    display.publicMapId.clear();
    display.showMap = false;
    display.publicEncounterId.clear();
    display.showInitiative = false;
    display.metadata.touch();
    update.publicScene = !beat.metadata.id.empty();
    update.displayEnabled = display.enabled;

    EntityId encounterId = firstLinkedTarget(beat, "Encounter");
    EntityId mapId = firstLinkedTarget(beat, "Map");
    if (!encounterId.empty())
    {
        display.publicEncounterId = encounterId;
        display.showInitiative = true;
        update.publicEncounter = true;

        const EncounterModel *encounter = findEncounterById(campaign, encounterId);
        if (mapId.empty() && encounter && !encounter->mapId.empty())
            mapId = encounter->mapId;
    }

    if (!mapId.empty())
    {
        display.publicMapId = mapId;
        display.showMap = true;
        update.publicMap = true;
        // Staging preserves map privacy; the player-review flow handles publication.
    }

    for (const StoryLinkModel &link : beat.links)
    {
        if (link.type != StoryLinkType::Reveals)
            continue;

        if (link.targetEntityType == "NPC" && addUniqueId(display.visibleNpcIds, link.targetEntityId))
        {
            display.showVisibleNpcs = true;
            ++update.visibleNpcCount;
        }
        else if (link.targetEntityType == "Handout" && addUniqueId(display.publicHandoutIds, link.targetEntityId))
        {
            display.showPublicHandouts = true;
            ++update.publicHandoutCount;
        }
    }

    return update;
}

SceneLiveStateUpdate applyLiveStateForScene(Campaign &campaign, StoryThreadModel &thread, StoryBeatModel &beat)
{
    SceneLiveStateUpdate update;
    thread.status = StoryStatus::Active;
    beat.status = StoryStatus::Active;
    thread.metadata.touch();
    beat.metadata.touch();
    const std::vector<StoryLinkModel> links = collectSceneLinks(thread, beat);
    StoryBeatModel presentation = beat;
    // A specific scene's map/encounter overrides its arc's shared defaults.
    presentation.links.insert(presentation.links.end(), thread.links.begin(), thread.links.end());
    update.firstMapId = firstLinkedTarget(presentation.links, "Map");
    update.firstEncounterId = firstLinkedTarget(presentation.links, "Encounter");
    update.presentation = applyPresentationStateForScene(campaign, presentation);
    SessionModel *session = activeSession(campaign);
    for (const StoryLinkModel &link : links)
    {
        if (link.targetEntityType == "Map")
        {
            ++update.mapLinkCount;
        }
        else if (link.targetEntityType == "Encounter")
        {
            ++update.encounterLinkCount;
            if (session && addUniqueId(session->encounterIds, link.targetEntityId))
                update.sessionEncounterLinked = true;
        }
        else if (link.targetEntityType == "NPC" || link.targetEntityType == "Monster")
        {
            ++update.npcLinkCount;
            update.hasNpcLink = true;
        }
        else if (link.targetEntityType == "Item" || link.targetEntityType == "Loot")
        {
            ++update.itemLinkCount;
            update.hasItemLink = true;
        }
        else if (link.targetEntityType == "Audio")
        {
            ++update.audioLinkCount;
        }
    }

    if (update.firstMapId.empty() && !update.firstEncounterId.empty())
    {
        if (const EncounterModel *encounter = findEncounterById(campaign, update.firstEncounterId))
            update.firstMapId = encounter->mapId;
    }

    if (!update.firstMapId.empty())
    {
        campaign.partyLocation.mapId = update.firstMapId;
        campaign.partyTravel.currentMapId = update.firstMapId;
        update.partyMapUpdated = true;
    }

    if (session)
    {
        if (addUniqueId(session->storyDocumentIds, beat.metadata.id))
            update.sessionStoryLinked = true;
        if (update.sessionStoryLinked || update.sessionEncounterLinked)
            session->metadata.touch();
    }

    return update;
}

std::string describePresentationUpdate(const ScenePresentationUpdate &update)
{
    std::string summary = gm::localization::trUi(" Player Display state: scene staged");
    if (update.publicMap)
        summary += gm::localization::trUi(", map staged");
    if (update.publicEncounter)
        summary += gm::localization::trUi(", initiative staged");
    if (update.visibleNpcCount > 0)
    {
        summary += ", " + std::to_string(update.visibleNpcCount) + " " +
                   gm::localization::trUi(update.visibleNpcCount == 1 ? "NPC reveal" : "NPC reveals");
    }
    if (update.publicHandoutCount > 0)
    {
        summary += ", " + std::to_string(update.publicHandoutCount) + " " +
                   gm::localization::trUi(update.publicHandoutCount == 1 ? "handout reveal" : "handout reveals");
    }
    summary += update.displayEnabled ? "." : gm::localization::trUi(". Arm the Player Display in Presentation Panel when ready.");
    return summary;
}

std::string describeLiveStateUpdate(const SceneLiveStateUpdate &update)
{
    std::string summary = std::string(gm::localization::trUi(" Scene links:")) + " " +
                          gm::localization::trUi("maps") + " " + std::to_string(update.mapLinkCount) + ", " +
                          gm::localization::trUi("encounters") + " " + std::to_string(update.encounterLinkCount) + ", " +
                          gm::localization::trUi("NPCs/monsters") + " " + std::to_string(update.npcLinkCount) + ", " +
                          gm::localization::trUi("items/loot") + " " + std::to_string(update.itemLinkCount) + ", " +
                          gm::localization::trUi("audio") + " " + std::to_string(update.audioLinkCount) + ".";
    if (update.partyMapUpdated)
        summary += gm::localization::trUi(" Active map updated.");
    if (update.sessionStoryLinked)
        summary += gm::localization::trUi(" Session story link recorded.");
    if (update.sessionEncounterLinked)
        summary += gm::localization::trUi(" Session encounter link recorded.");
    return summary;
}

void appendLaunchReceipt(Campaign &campaign, const StoryBeatModel &beat, const std::string &receipt)
{
    SessionModel *session = activeSession(campaign);
    if (!session || receipt.empty())
        return;

    SessionNoteModel note;
    note.timestampUtc = currentUtcTimestamp();
    note.body = receipt;
    note.linkedEntityId = beat.metadata.id;
    note.linkedEntityType = "story_beat";
    note.linkedEntityLabel = beat.title.empty() ? "Untitled scene" : beat.title;
    note.metadata.tags.push_back(kSceneLaunchNoteTag);
    session->notes.push_back(note);
    session->metadata.touch();
}
}
