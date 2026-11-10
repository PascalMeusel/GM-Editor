#include "Models/Project/CampaignEntityDraft.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>

int main()
{
    int failures = 0;
    const auto check = [&](bool condition, const char *message) {
        if (!condition) { ++failures; std::cerr << message << '\n'; }
    };
    Campaign campaign;
    campaign.name = "Private draft fixture";
    campaign.description = "Other unsaved work must survive";
    SessionModel session;
    session.title = "Prepared session";
    campaign.sessions.push_back(session);
    const auto campaignId = campaign.metadata.id;
    NPCModel npc;
    npc.name = "Lantern keeper";
    npc.secrets = "Private draft secret";
    const auto npcId = npc.metadata.id;
    int attempts = 0;
    const auto failedSave = [&](Campaign &proposed) {
        ++attempts;
        check(proposed.npcs.size() == 1 && proposed.npcs.front().metadata.id == npcId,
              "The persistence attempt must contain the proposed NPC");
        proposed.description = "A failed backend may mutate its argument";
        return false;
    };
    check(!persistCampaignEntityDraft(campaign, npc, &Campaign::npcs, failedSave), "Failed save reported success");
    check(campaign.npcs.empty() && campaign.description == "Other unsaved work must survive",
          "Failed creation changed the live campaign");
    check(npc.name == "Lantern keeper" && npc.secrets == "Private draft secret" && npc.metadata.id == npcId,
          "Failed save destroyed the editable draft");
    const auto save = [&](Campaign &) { ++attempts; return true; };
    check(persistCampaignEntityDraft(campaign, npc, &Campaign::npcs, save), "Retry failed");
    check(campaign.npcs.size() == 1 && campaign.npcs.front().metadata.id == npcId,
          "Retry did not retain prepared entity identity");
    npc.name = "Renamed keeper";
    check(persistCampaignEntityDraft(campaign, npc, &Campaign::npcs, save), "Stable identity update failed");
    check(campaign.npcs.size() == 1 && campaign.npcs.front().name == "Renamed keeper",
          "Repeated Create duplicated the stable entity");
    npc.name = "Unsaved correction";
    check(!persistCampaignEntityDraft(campaign, npc, &Campaign::npcs, failedSave), "Failed replacement reported success");
    check(campaign.npcs.front().name == "Renamed keeper" && npc.name == "Unsaved correction",
          "Failed replacement lost committed data or draft correction");
    try
    {
        persistCampaignEntityDraft(campaign, npc, &Campaign::npcs, [](Campaign &proposed) -> bool {
            proposed.sessions.clear();
            throw std::runtime_error("Injected backend failure");
        });
        check(false, "Expected injected failure");
    }
    catch (const std::runtime_error &) {}
    check(campaign.sessions.size() == 1 && campaign.sessions.front().metadata.id == session.metadata.id && campaign.metadata.id == campaignId,
          "Creation altered the selected campaign/session context");
    MonsterDefinitionModel monster;
    monster.name = "Marsh guardian";
    check(!persistCampaignEntityDraft(campaign, monster, &Campaign::customMonsters, [](Campaign &) { return false; }) && campaign.customMonsters.empty(),
          "Failed monster save leaked its stat block");
    check(persistCampaignEntityDraft(campaign, monster, &Campaign::customMonsters, save) && campaign.customMonsters.size() == 1,
          "Monster retry did not commit");
    CharacterModel player;
    std::memcpy(player.nameBuffer, "Keeper player", sizeof("Keeper player"));
    check(!persistCampaignEntityDraft(campaign, player, &Campaign::characters, [](Campaign &) { return false; }) && campaign.characters.empty(),
          "Failed player save leaked its sheet");
    check(persistCampaignEntityDraft(campaign, player, &Campaign::characters, save) && campaign.characters.size() == 1,
          "Player retry did not commit");

    StoryThreadModel thread;
    StoryBeatModel scene;
    scene.title = "At the harbour";
    const auto sceneId = scene.metadata.id;
    thread.beats.push_back(scene);
    campaign.storyThreads.push_back(thread);
    SessionPlanEntry entry;
    entry.storyBeatId = sceneId;
    campaign.sessions.front().planEntries.push_back(entry);
    CampaignSceneDraftContext context{campaignId, session.metadata.id, sceneId};
    NPCModel linkedNpc;
    linkedNpc.name = "The harbourmaster";
    const auto saveWithLink = [&](Campaign &proposed, bool succeed) {
        check(linkCampaignEntityDraftToScene(proposed, context, linkedNpc.metadata.id, "NPC", linkedNpc.name),
              "Valid scene context was rejected");
        check(proposed.storyThreads.front().beats.front().links.size() == 1 && proposed.npcs.size() == 2,
              "Entity and link were not proposed together");
        return succeed;
    };
    check(!persistCampaignEntityDraft(campaign, linkedNpc, &Campaign::npcs, [&](Campaign &proposed) { return saveWithLink(proposed, false); }),
          "Failed linked creation reported success");
    check(campaign.npcs.size() == 1 && campaign.storyThreads.front().beats.front().links.empty(),
          "Failed linked creation committed an entity or scene link");
    check(persistCampaignEntityDraft(campaign, linkedNpc, &Campaign::npcs, [&](Campaign &proposed) { return saveWithLink(proposed, true); }),
          "Linked creation retry failed");
    const auto linkId = campaign.storyThreads.front().beats.front().links.front().metadata.id;
    linkedNpc.name = "Harbourmaster Mara";
    check(persistCampaignEntityDraft(campaign, linkedNpc, &Campaign::npcs, [&](Campaign &proposed) { return saveWithLink(proposed, true); }),
          "Linked creation stable identity retry failed");
    check(campaign.storyThreads.front().beats.front().links.front().metadata.id == linkId &&
          campaign.storyThreads.front().beats.front().links.front().label == linkedNpc.name,
          "Linked retry duplicated a link or retained a stale label");
    const auto staleSave = [&](Campaign &proposed) {
        return linkCampaignEntityDraftToScene(proposed, context, linkedNpc.metadata.id, "NPC", "Wrong update");
    };
    for (EntityMetadata *metadata : {&campaign.sessions.front().metadata,
            &campaign.storyThreads.front().metadata, &campaign.storyThreads.front().beats.front().metadata})
    {
        metadata->tags.push_back("archived");
        check(!persistCampaignEntityDraft(campaign, linkedNpc, &Campaign::npcs, staleSave),
            "Archived session, story or scene accepted a linked save");
        check(campaign.storyThreads.front().beats.front().links.front().metadata.id == linkId &&
            campaign.storyThreads.front().beats.front().links.front().label == linkedNpc.name,
            "Rejected archive context changed a committed link");
        metadata->tags.pop_back();
    }
    campaign.sessions.front().planEntries.clear();
    check(!persistCampaignEntityDraft(campaign, linkedNpc, &Campaign::npcs, staleSave), "Removed plan entry accepted a linked save");
    campaign.sessions.front().planEntries.push_back(entry);
    campaign.sessions.front().endedAtUtc = "2026-09-08T00:00:00Z";
    check(!persistCampaignEntityDraft(campaign, linkedNpc, &Campaign::npcs, staleSave), "Ended session accepted a linked save");
    campaign.sessions.front().endedAtUtc.clear();
    campaign.storyThreads.front().beats.clear();
    check(!persistCampaignEntityDraft(campaign, linkedNpc, &Campaign::npcs, staleSave), "Deleted scene accepted a linked save");
    campaign.storyThreads.front().beats.push_back(scene);
    context.campaignId = "another-campaign";
    check(!persistCampaignEntityDraft(campaign, linkedNpc, &Campaign::npcs, staleSave), "Changed campaign accepted the old creation context");
    check(linkCampaignEntityDraftToScene(campaign, {}, linkedNpc.metadata.id, "NPC", linkedNpc.name) &&
          campaign.storyThreads.front().beats.front().links.empty(), "Library creation added an unsolicited scene link");
    npc.metadata.id.clear();
    const int beforeInvalid = attempts;
    check(!persistCampaignEntityDraft(campaign, npc, &Campaign::npcs, save) && attempts == beforeInvalid,
          "Draft without stable identity reached persistence");
    return failures == 0 ? 0 : 1;
}
