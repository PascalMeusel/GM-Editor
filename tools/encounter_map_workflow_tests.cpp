#include "workflow_test_database.hpp"
#include "Models/Map/CurrentMapArtRecipe.hpp"
#include "Controllers/Editor/Widgets/EncounterBuilder/EncounterBuilderController.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

struct EncounterWorkflowProbe
{
    static void setName(EncounterBuilderController &editor, const std::string &name) { editor._draftEncounter.name = name; }
    static const std::string &name(const EncounterBuilderController &editor) { return editor._draftEncounter.name; }
    static std::vector<TacticalMapCandidate> generateBattlefields(EncounterBuilderController &editor)
    {
        editor._keepTacticalSeed = true;
        editor._tacticalSeed = 612345;
        editor._tacticalWidth = 24;
        editor._tacticalHeight = 18;
        editor.generateTacticalCandidates();
        if (!editor._tacticalGeneration.valid() ||
            editor._tacticalGeneration.wait_for(std::chrono::seconds(30)) != std::future_status::ready)
            throw std::runtime_error("Encounter battlefield generation did not finish");
        editor.pollTacticalGeneration();
        return editor._tacticalCandidates;
    }
};

namespace
{
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
Campaign readCampaign(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns())
        if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Missing campaign on disk");
}
}

int main()
{
    try
    {
        isolateWorkflowDatabase();
        auto &save = ProjectSaveService::getInstance();
        save.setBackupSnapshotsEnabled(false);
        save.setRecoverySnapshotsEnabled(false);
        Campaign campaign;
        campaign.name = "Generated encounter map workflow";
        campaign.ruleSet = std::make_shared<DND5e>();
        MapModel map;
        map.name = "Generated tactical grid";
        map.atlasLevel = MapLevel::Tactical;
        map.width = 8; map.height = 8; map.cells.resize(64);
        campaign.maps.push_back(map);
        CampaignRepository::saveCampaign(campaign);
        EncounterBuilderController editor(campaign);
        EntityId destination, editedMap;
        int opens = 0;
        editor.setMapAuthoringHandler([&](EntityId encounterId, EntityId mapId) {
            destination = encounterId; editedMap = mapId; ++opens; return true;
        });
        require(editor.openSharedMapAuthoring(true), "Create-map route did not save the encounter first");
        require(opens == 1 && !destination.empty() && editedMap.empty(), "Create route lost encounter identity");
        require(readCampaign(campaign.metadata.id).encounters[0].metadata.id == destination,
            "Route opened before encounter existed on disk");
        EncounterModel second;
        second.name = "Second encounter";
        campaign.encounters.push_back(second);
        CampaignRepository::saveCampaign(campaign);
        EncounterWorkflowProbe::setName(editor, "Retained preparation notes");
        require(editor.selectEncounterById(second.metadata.id) && editor.selectEncounterById(destination) &&
            EncounterWorkflowProbe::name(editor) == "Retained preparation notes", "Encounter browse visit lost its draft");
        SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
        control.exec("CREATE TRIGGER reject_encounter BEFORE INSERT ON encounters "
                     "BEGIN SELECT RAISE(ABORT, 'injected encounter commit failure'); END;");
        require(!editor.assignSavedMap(destination, map.metadata.id), "Failed assignment reported success");
        require(!campaign.encounters[0].tacticalMapId.has_value() &&
            !readCampaign(campaign.metadata.id).encounters[0].tacticalMapId.has_value(),
            "Failed assignment published draft map changes into memory or disk");
        require(!editor.openSharedMapAuthoring(false) && opens == 1,
            "Failed encounter save left the retry context");
        require(!editor.selectEncounterById(second.metadata.id), "Failed save allowed selection to discard its retry context");
        control.exec("DROP TRIGGER reject_encounter;");
        require(editor.assignSavedMap(destination, map.metadata.id), "Assignment retry failed");
        const auto persisted = readCampaign(campaign.metadata.id);
        require(persisted.encounters.size() == 2 && persisted.encounters[0].metadata.id == destination &&
            persisted.encounters[0].tacticalMapId == map.metadata.id && persisted.encounters[0].battlefieldCells.size() == 64,
            "Retry changed identity, duplicated the encounter or lost the map grid");
        require(editor.openSharedMapAuthoring(false) && editedMap == map.metadata.id,
            "Edit route did not use the committed tactical map ID");
        require(!editor.assignSavedMap("another-encounter", map.metadata.id), "Late result assigned another encounter");
        const auto battlefields = EncounterWorkflowProbe::generateBattlefields(editor);
        require(!battlefields.empty(), "Encounter route returned no battlefield designs");
        for (const auto &candidate : battlefields)
            require(candidate.map.generation.recipe.recipeVersion == kCurrentMapIllustrationRecipe &&
                candidate.map.generation.recipe.generatorVersion == 12,
                "Encounter route selected legacy artwork for a new battlefield");
        require(campaign.maps.size() == 1 && readCampaign(campaign.metadata.id).maps.size() == 1,
            "Previewing new battlefield artwork published an unselected map");
        campaign.encounters[0].metadata.tags.push_back("archived");
        require(!editor.assignSavedMap(destination, map.metadata.id), "Archived encounter accepted a map");
        campaign.encounters.clear();
        require(!editor.assignSavedMap(destination, map.metadata.id) && !editor.openSharedMapAuthoring(false),
            "Deleted encounter was resurrected by retained preparation");
        require(campaign.encounters.empty(), "Deletion recovery modified the campaign");
        std::cout << "Actual encounter service: create/edit route, assignment commit failure, retry, stable IDs, archive/delete passed.\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
