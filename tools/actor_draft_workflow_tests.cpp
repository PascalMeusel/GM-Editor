#include "workflow_test_database.hpp"
#include "Controllers/Editor/Widgets/NPCCreator/NPCCreatorController.hpp"
#include "Controllers/Editor/Widgets/PlayerCreator/PlayerCreatorController.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <stdexcept>

struct ActorDraftWorkflowProbe
{
    static void select(NPCCreatorController &editor, int type) { editor._selectedEntryType = type; }
    static bool saveNpc(NPCCreatorController &editor) { return editor.saveNPC(); }
    static void editAction(NPCCreatorController &editor)
    { editor._monsterAction.damage = "1d6"; editor._monsterActionEdited = true; editor.markDraftEdited(); }
};
namespace
{
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
Campaign read(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns()) if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Missing campaign");
}
}
int main()
{
    try
    {
        isolateWorkflowDatabase();
        auto &save = ProjectSaveService::getInstance(); save.setBackupSnapshotsEnabled(false); save.setRecoverySnapshotsEnabled(false);
        Campaign campaign; campaign.name = "Generated independent actor drafts"; campaign.ruleSet = std::make_shared<DND5e>();
        CampaignRepository::saveCampaign(campaign);
        NPCCreatorController editor(campaign); editor.configure();
        editor.getModel().name = "Retained person"; editor.markDraftEdited();
        ActorDraftWorkflowProbe::select(editor, 1);
        editor.getMonsterModel().name = "Retained creature"; editor.markDraftEdited();
        ActorDraftWorkflowProbe::editAction(editor);
        const EntityId personId = editor.getModel().metadata.id, creatureId = editor.getMonsterModel().metadata.id;
        require(ActorDraftWorkflowProbe::saveNpc(editor) && editor.hasUnsavedDraft(), "Saving NPC cleared the other actor draft");
        editor.hide(); editor.resumeDraft();
        require(editor.getMonsterModel().metadata.id == creatureId && editor.getMonsterModel().name == "Retained creature",
            "Reopening creator discarded the other unsaved actor");
        SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
        control.exec("CREATE TRIGGER reject_actor BEFORE INSERT ON custom_monsters BEGIN SELECT RAISE(ABORT, 'injected creature failure'); END;");
        require(!editor.flushDraft() && editor.hasUnsavedDraft() && campaign.customMonsters.empty() &&
            read(campaign.metadata.id).customMonsters.empty(), "Failed creature save lost draft or changed committed campaign");
        control.exec("DROP TRIGGER reject_actor;");
        require(editor.flushDraft() && !editor.hasUnsavedDraft(), "Creator retry failed");
        const auto loaded = read(campaign.metadata.id);
        require(loaded.npcs.size() == 1 && loaded.npcs[0].metadata.id == personId && loaded.npcs[0].name == "Retained person" &&
            loaded.customMonsters.size() == 1 && loaded.customMonsters[0].metadata.id == creatureId &&
            loaded.customMonsters[0].name == "Retained creature" && loaded.customMonsters[0].actions.size() == 1 &&
            loaded.customMonsters[0].actions[0].damage == "1d6", "Independent actor drafts or pending action lost identity/content or duplicated on retry");

        Campaign playerCampaign;
        playerCampaign.name = "Explicit player creation";
        playerCampaign.ruleSet = std::make_shared<DND5e>();
        PlayerCreatorController playerCreator(playerCampaign);
        playerCreator.configure();
        std::snprintf(playerCreator.getModel().nameBuffer,
                      sizeof(playerCreator.getModel().nameBuffer), "%s", "Unfinished player");
        playerCreator.markDraftEdited();
        require(!playerCreator.flushDraft() && playerCreator.hasUnsavedDraft() && playerCampaign.characters.empty(),
                "Campaign save implicitly committed an unfinished player draft");
        playerCreator.discardDraft();
        require(!playerCreator.hasUnsavedDraft() && playerCampaign.characters.empty(),
                "Discarding an unfinished player draft changed the campaign");
        std::cout << "Independent NPC/monster drafts and pending action survive save, library return and failed commit retry.\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
