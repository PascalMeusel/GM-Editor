#include "Models/Project/StarterCampaignTemplate.hpp"
#include "Models/Combat/EncounterMapRenderSource.hpp"

#include <iostream>
#include <set>

int main()
{
    Campaign campaign;
    campaign.name = "Test Campaign";

    StarterCampaignTemplate::apply(campaign, StarterCampaignTemplate::Language::German);
    if (campaign.ruleSet || campaign.storyThreads.size() != 1 ||
        campaign.storyThreads.front().beats.size() != 3 || campaign.npcs.size() != 1 ||
        campaign.worldPlaces.size() != 1 || campaign.description.empty() || campaign.sessions.size() != 1 || campaign.sessions.front().planEntries.size() != 3)
    {
        std::cerr << "Guided starter template did not create the expected workspace.\n";
        return 1;
    }
    if (campaign.storyThreads.front().title.find("Erste Sitzung") == std::string::npos ||
        campaign.metadata.customProperties["starter_template"] != "guided_v1")
    {
        std::cerr << "German starter content or template metadata is missing.\n";
        return 1;
    }

    Campaign existing;
    existing.description = "Keep this";
    StarterCampaignTemplate::apply(existing, StarterCampaignTemplate::Language::English);
    if (existing.description != "Keep this" || existing.storyThreads.front().title.find("First session") == std::string::npos)
    {
        std::cerr << "Starter template overwrote user content or ignored the selected language.\n";
        return 1;
    }
    for (std::size_t i = 0; i < campaign.sessions.front().planEntries.size(); ++i)
    {
        const auto &scene = campaign.storyThreads.front().beats[i];
        if (campaign.sessions.front().planEntries[i].storyBeatId != scene.metadata.id || scene.links.size() < 2 || scene.gmNotes.empty())
        { std::cerr << "Starter scene links or runnable content are missing.\n"; return 1; }
    }

    std::set<std::string> storyLinkIds;
    for (const StoryThreadModel &thread : campaign.storyThreads)
    {
        for (const StoryLinkModel &link : thread.links)
            if (link.metadata.id.empty() || !storyLinkIds.insert(link.metadata.id).second)
            { std::cerr << "Starter thread links must have unique non-empty identities.\n"; return 1; }
        for (const StoryBeatModel &scene : thread.beats)
            for (const StoryLinkModel &link : scene.links)
                if (link.metadata.id.empty() || !storyLinkIds.insert(link.metadata.id).second)
                { std::cerr << "Starter scene links must have unique non-empty identities.\n"; return 1; }
    }

    for (Campaign *sample : {&campaign, &existing})
    {
        if (sample->maps.size() != 1 || sample->encounters.size() != 1)
        { std::cerr << "Starter map or encounter is missing.\n"; return 1; }
        auto &encounter = sample->encounters.front();
        normalizeEncounter(encounter);
        const MapModel *map = encounterMapRenderSource(encounter, sample->maps);
        if (!map || map != &sample->maps.front())
        { std::cerr << "Starter encounter terrain differs from its linked map after normalization.\n"; return 1; }
        const auto *wall = encounterCellAt(static_cast<const EncounterModel &>(encounter), 0, 5);
        const auto *door = encounterCellAt(static_cast<const EncounterModel &>(encounter), 8, 11);
        const auto *pillar = encounterCellAt(static_cast<const EncounterModel &>(encounter), 5, 5);
        if (!wall || wall->kind != MapTileKind::Wall || !wall->blocksLineOfSight || wall->movementBlockedMask == 0 ||
            !door || door->kind != MapTileKind::Door || door->blocksLineOfSight || door->movementBlockedMask != 0 ||
            !pillar || pillar->kind != MapTileKind::Cover || !pillar->difficultTerrain || pillar->movementCost != 2)
        { std::cerr << "Starter combat lost its walls, open entrance or cover semantics.\n"; return 1; }
        for (const auto &participant : encounter.participants)
        {
            const auto *cell = encounterCellAt(static_cast<const EncounterModel &>(encounter), participant.x, participant.y);
            if (!cell || cell->movementBlockedMask != 0)
            { std::cerr << "Starter combatant starts outside the room or inside a blocker.\n"; return 1; }
        }
    }

    std::cout << "Starter campaign template tests passed.\n";
    return 0;
}
