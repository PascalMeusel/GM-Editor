#include "workflow_test_database.hpp"
#include "Controllers/Editor/Widgets/CharacterSheet/CharacterSheetController.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>

struct CharacterNotesWorkflowProbe
{
    static void edit(CharacterSheetController &editor, const std::string &text) { editor._gmNotesDraft = text; }
    static std::string draft(const CharacterSheetController &editor) { return editor._gmNotesDraft; }
};
namespace
{
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
Campaign read(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns()) if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Missing campaign");
}
std::string notes(const Campaign &campaign, const EntityId &id)
{
    for (const auto &character : campaign.characters) if (character.metadata.id == id) return character.gmNotes;
    throw std::runtime_error("Missing character");
}
}
int main()
{
    try
    {
        isolateWorkflowDatabase();
        auto &save = ProjectSaveService::getInstance();
        save.setBackupSnapshotsEnabled(false); save.setRecoverySnapshotsEnabled(false);
        Campaign campaign; campaign.name = "Generated notes workflow"; campaign.ruleSet = std::make_shared<DND5e>();
        CharacterModel first, second; first.gmNotes = "Original secret"; second.gmNotes = "Other secret";
        campaign.characters = {first, second}; CampaignRepository::saveCampaign(campaign);
        CharacterSheetController editor(campaign);
        require(editor.selectCharacterById(first.metadata.id) && !editor.hasUnsavedDraft(), "Initial notes selection is dirty");
        CharacterNotesWorkflowProbe::edit(editor, "Retained secret");
        require(!editor.selectCharacterById(second.metadata.id) && editor.hasUnsavedDraft(), "Switch silently discarded notes");
        std::reverse(campaign.characters.begin(), campaign.characters.end());
        SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
        control.exec("CREATE TRIGGER reject_notes BEFORE INSERT ON characters BEGIN SELECT RAISE(ABORT, 'injected notes failure'); END;");
        require(!editor.flushDraft() && editor.hasUnsavedDraft() && notes(campaign, first.metadata.id) == "Original secret" &&
            notes(read(campaign.metadata.id), first.metadata.id) == "Original secret", "Failed notes commit changed memory/disk or dirty state");
        control.exec("DROP TRIGGER reject_notes;");
        require(editor.flushDraft() && !editor.hasUnsavedDraft() && notes(read(campaign.metadata.id), first.metadata.id) == "Retained secret" &&
            notes(read(campaign.metadata.id), second.metadata.id) == "Other secret", "Reordered notes retry changed wrong character");
        CharacterNotesWorkflowProbe::edit(editor, "Conflicting secret");
        for (auto &character : campaign.characters) if (character.metadata.id == first.metadata.id) character.gmNotes = "External secret";
        CampaignRepository::saveCampaign(campaign);
        require(!editor.flushDraft() && CharacterNotesWorkflowProbe::draft(editor) == "Conflicting secret" &&
            notes(read(campaign.metadata.id), first.metadata.id) == "External secret", "Conflicting save overwrote external notes");
        campaign.characters.erase(std::remove_if(campaign.characters.begin(), campaign.characters.end(),
            [&](const auto &character) { return character.metadata.id == first.metadata.id; }), campaign.characters.end());
        CampaignRepository::saveCampaign(campaign);
        require(!editor.flushDraft() && editor.hasUnsavedDraft() && read(campaign.metadata.id).characters.size() == 1,
            "Deleted character was resurrected or draft lost");
        std::cout << "Character notes: selection guard, reordered identity, commit failure/retry, conflict and deletion retention passed.\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
