#pragma once

#include "Controllers/Editor/Widgets/AudioManager/AudioManagerController.hpp"
#include "Controllers/Editor/Widgets/AudioQuickControls/AudioQuickControlsController.hpp"
#include "Controllers/Editor/Widgets/BattlefieldCreator/BattlefieldCreatorController.hpp"
#include "Controllers/Editor/Widgets/BattleTracker/BattleTrackerController.hpp"
#include "Controllers/Editor/Widgets/CharacterSheet/CharacterSheetController.hpp"
#include "Controllers/Editor/Widgets/EncounterBuilder/EncounterBuilderController.hpp"
#include "Controllers/Editor/Widgets/GMScreen/GMScreenController.hpp"
#include "Controllers/Editor/Widgets/InventoryManager/InventoryManagerController.hpp"
#include "Controllers/Editor/Widgets/ItemCreator/ItemCreatorController.hpp"
#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardController.hpp"
#include "Controllers/Editor/Widgets/LocalPlayerDisplay/LocalPlayerDisplayController.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorController.hpp"
#include "Controllers/Editor/Widgets/MapViewer/MapViewerController.hpp"
#include "Controllers/Editor/Widgets/NPCCreator/NPCCreatorController.hpp"
#include "Controllers/Editor/Widgets/NPCOverview/NPCOverviewController.hpp"
#include "Controllers/Editor/Widgets/PlayerCreator/PlayerCreatorController.hpp"
#include "Controllers/Editor/Widgets/PlayerOverview/PlayerOverviewController.hpp"
#include "Controllers/Editor/Widgets/SpellLibrary/SpellLibraryController.hpp"
#include "Controllers/Editor/Widgets/StoryWorkspace/StoryWorkspaceController.hpp"

#include <memory>

// Owns the editor's child-controller graph and the lifecycle operations that
// apply to the graph as a whole. EditorWindowController remains responsible
// for navigation and cross-tool coordination.
struct EditorChildControllers
{
    std::shared_ptr<AudioPlaybackService> audioPlaybackService;
    std::shared_ptr<CharacterSheetController> characterSheet;
    std::shared_ptr<ItemCreatorController> itemCreator;
    std::shared_ptr<InventoryManagerController> inventoryManager;
    std::shared_ptr<MapCreatorController> mapCreator;
    std::shared_ptr<MapViewerController> mapViewer;
    std::shared_ptr<BattlefieldCreatorController> battlefieldCreator;
    std::shared_ptr<EncounterBuilderController> encounterBuilder;
    std::shared_ptr<BattleTrackerController> battleTracker;
    std::shared_ptr<GMScreenController> gmScreen;
    std::shared_ptr<LiveSessionDashboardController> liveSessionDashboard;
    std::shared_ptr<LocalPlayerDisplayController> localPlayerDisplay;
    std::shared_ptr<AudioManagerController> audioManager;
    std::shared_ptr<AudioQuickControlsController> audioQuickControls;
    std::shared_ptr<PlayerCreatorController> playerCreator;
    std::shared_ptr<PlayerOverviewController> playerOverview;
    std::shared_ptr<NPCCreatorController> npcCreator;
    std::shared_ptr<NPCOverviewController> npcOverview;
    std::shared_ptr<SpellLibraryController> spellLibrary;
    std::shared_ptr<StoryWorkspaceController> storyWorkspace;

    void hideWorkspaceTools() const
    {
        characterSheet->hide();
        itemCreator->hide();
        inventoryManager->hide();
        mapCreator->hide();
        mapViewer->hide();
        battlefieldCreator->hide();
        encounterBuilder->hide();
        battleTracker->hide();
        gmScreen->hide();
        liveSessionDashboard->hide();
        audioManager->hide();
        audioQuickControls->hide();
        playerCreator->hide();
        playerOverview->hide();
        npcCreator->hide();
        npcOverview->hide();
        spellLibrary->hide();
        storyWorkspace->hide();
    }

    bool hasUnsavedDrafts() const
    {
        return liveSessionDashboard->hasUnsavedDrafts() || gmScreen->hasUnsavedDrafts() || mapCreator->hasUnsavedDraft() ||
               playerCreator->hasUnsavedDraft() || npcCreator->hasUnsavedDraft() || itemCreator->hasUnsavedDraft() ||
               encounterBuilder->hasUnsavedDrafts() || characterSheet->hasUnsavedDraft() || spellLibrary->hasUnsavedDrafts();
    }

    void resetDraftsAndSelection() const
    {
        npcOverview->resetReadingContext();
        liveSessionDashboard->resetDrafts();
        gmScreen->resetDrafts();
        characterSheet->resetDrafts();
        spellLibrary->resetDrafts();
        inventoryManager->resetSelection();
    }

    void configureCampaignEditors() const
    {
        encounterBuilder->resetDrafts();
        playerCreator->configure();
        npcCreator->configure();
        itemCreator->configure();
    }
};
