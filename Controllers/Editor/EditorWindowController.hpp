#pragma once
#include "Models/Campaign/Campaign.hpp"
#include "Controllers/IWindowController.hpp"
#include "Controllers/Editor/EditorChildControllers.hpp"
#include "Controllers/Editor/EditorWindowControllerState.hpp"
#include "WindowTypes.hpp"
#include "Models/Project/CampaignReferenceService.hpp"
#include "Models/Project/CampaignGraphService.hpp"
#include "Models/Project/CampaignCommandPaletteService.hpp"
#include "Models/Rulesets/RulesetVersioning.hpp"
#include "Models/Project/WorldInspectorService.hpp"
#include "Models/Project/WorldInspectorDrafts.hpp"
#include "Models/Random/RandomTableService.hpp"
#include <iostream>
#include <memory>
#include <random>
#include <string>

class IProjectCommand;
class ProjectSaveService;
class EditorWindowView;
class EditorDraftRecoveryController;
struct PlayerDisplayReviewSnapshot;
namespace PlayerDisplayReview { struct Measurements; }
class EditorWindowController : public IWindowController
{
    friend struct WorkspaceInteractionProbe;
    friend struct PlayerReviewUiProbe;
    friend struct TabletopHistoryProbe;
    friend struct MapCampaignBoundaryProbe;
    friend struct EditorDraftRecoveryProbe;
    friend struct EditorDraftRecoveryUiProbe;
    friend class EditorWorldHistory;
    friend class EditorMarkdownExchange;
public:
    EditorWindowController();
    ~EditorWindowController();
    virtual void update(std::shared_ptr<GLFWwindow> window) override;
    virtual void setWindowOpacity(bool showView) override;

    virtual WindowTypes getWindowType() override;
    virtual bool getWindowActive() override;
    bool setCurrentCampaign(Campaign currentCampaign);
    const Campaign &getCurrentCampaign() const { return _currentCampaign; }
    const Campaign &currentCampaign() const { return _currentCampaign; }
    bool saveCurrentCampaign(const std::string &reason = "manual save");
    bool discardUnsavedChanges(const std::string &reason);
    void pollRetiredMapWork();
    void queuePreparationTarget(PreparationTargetRequest);
    void showLiveSessionRunTab();
    void requestCurrentPlayerReview(PlayerReviewSource source, const EntityId &sourceId);
    bool hasUnsavedChanges() const;
    // Preserve private session text before application/campaign teardown;
    // this never records unfinished outcomes in canonical campaign data.
    bool checkpointOutcomeRecoveryForLeave();
    bool checkpointWorldRecoveryForLeave();
    bool checkpointEditorDraftRecoveryForLeave();
    EditorDraftRecoveryController &editorDraftRecovery() { return *_editorDraftRecovery; }
    const std::string &draftDiscardError() const { return _partialDraftDiscardError; }
    const std::string &worldRecoveryError() const;
    const std::string &outcomeRecoveryError() const { return _outcomeRecoveryError; }
    const std::string &worldDraftSaveError() const { return _worldDraftSaveError; }
    const gm::ux::WorkspaceNavigation &navigationContext() const { return _workspace.navigation; }
    void executeCommand(std::unique_ptr<IProjectCommand> command);
    bool undoCommand();
    bool redoCommand();
    std::vector<CampaignSearchResult> searchCurrentCampaign(const std::string &query, std::size_t limit = 50) const;
    std::vector<std::string> tagsForEntity(const EntityId &entityId) const;
    std::vector<std::string> allCampaignTags() const;
    bool setEntityTags(const EntityId &entityId, const std::vector<std::string> &tags, const std::string &saveReason = "edit entity tags");
    std::vector<CampaignBacklink> backlinksToEntity(const EntityId &entityId) const;
    std::vector<CampaignBrokenReference> brokenReferences() const;
    CampaignKnowledgeGraph knowledgeGraph() const;
    std::vector<CampaignCommandPaletteItem> commandPaletteItems(const std::string &query) const;
    void executePaletteCommand(const CampaignCommandPaletteItem &item);
    bool openWorkspaceEntity(std::string entityType, EntityId entityId);
    bool editWorkspaceMap(EntityId mapId);
    bool workspaceEntityArchived(const std::string &entityType, const EntityId &entityId) const;
    bool setWorkspaceEntityArchived(const std::string &entityType, const EntityId &entityId, bool archived);
    bool stageSceneForLive(EntityId sceneId);
    bool stageEncounterForLive(EntityId encounterId);
    WorldInspector::MutationResult applyWorldInspectorMutation(const WorldInspector::Mutation &mutation,
                                                               const std::string &saveReason,
                                                               WorldInspectorDrafts::Scope drafts = {},
                                                               WorldInspectorDrafts::Scope discarded = {});
    WorldInspector::MutationResult saveWorldDraft(WorldInspectorDrafts::Scope, const std::string &reason);
    std::vector<ItemModel> availableWorldItems() const;
    void activateWorkspaceByIndex(int workspaceIndex);
    std::vector<CampaignEntityBookmarkModel> favoriteItems() const;
    std::vector<CampaignEntityBookmarkModel> recentItems() const;
    bool isFavoriteEntity(const EntityId &entityId) const;
    void setFavoriteEntity(const EntityId &entityId, const std::string &entityType, const std::string &label, bool favorite);
    CampaignIndexRebuildResult rebuildCampaignIndex();
    const CampaignIndexRebuildResult &lastIndexRebuildResult() const { return _workspace.lastIndexRebuildResult; }
    const std::vector<CampaignSavedFilterModel> &savedFilters() const { return _currentCampaign.savedFilters; }
    std::vector<CampaignSearchResult> evaluateSavedFilter(const CampaignSavedFilterModel &filter, std::size_t limit = 100) const;
    void saveCurrentSearchAsFilter(const std::string &name, const std::string &query);
    void deleteSavedFilter(const EntityId &filterId);
    // Details are staged by the Settings view and committed as one durable campaign update.
    // A failed save deliberately leaves the active campaign untouched so its draft can be retried.
    bool updateCampaignDetails(const std::string &name, const std::string &description, std::string *error = nullptr);
    void setCampaignDescription(const std::string &description, const std::string &saveReason);
    bool setCampaignContentLanguage(const std::string &language);
    const SessionModel *activeSession() const;
    bool isSessionBreakActive() const;
    void startLiveSessionFromShell();
    void startSessionBreakFromShell();
    void resumeSessionFromBreakFromShell();
    void endLiveSessionFromShell();
    std::string currentInWorldClockLabel() const;
    std::string currentInWorldClockPhaseLabel() const;
    void setCampaignClockRunning(bool running, const std::string &reason = "change campaign clock running state");
    void setCampaignClockMultiplier(double multiplier, const std::string &reason = "change campaign clock multiplier");
    void resetCampaignClockToSystemTime();
    void setPresentationModeActive(bool active);
    PlayerDisplayReviewSnapshot buildLocalPlayerDisplayReview(PlayerDisplayReview::Measurements *measurements = nullptr) const;
    bool localPlayerDisplayReviewIsCurrent(const PlayerDisplayReviewSnapshot &review) const;
    bool armLocalPlayerDisplayFromReview(const PlayerDisplayReviewSnapshot &review, PlayerDisplayReview::Measurements *measurements = nullptr);
    bool publishLocalPlayerDisplay(const PlayerDisplayReviewSnapshot &review, PlayerDisplayReview::Measurements *measurements = nullptr);
    void disableLocalPlayerDisplay();
    void blackoutLocalPlayerDisplayOutput();
    bool localPlayerDisplayCombatStatsUpdatePending() const;
    bool updateLocalPlayerDisplayCombatStats();
    bool pingLocalPlayerDisplay();
    void setLocalPlayerDisplayMapOnly(bool mapOnly);
    void setLocalPlayerDisplayFullscreen(bool fullscreen);
    void setLocalPlayerDisplayMonitor(int monitorIndex);
    void setLocalPlayerDisplayCameraFollow(bool follow);
    void setLocalPlayerDisplayCamera(float normalizedCenterX, float normalizedCenterY, float zoom);
    bool revealEntirePublicMap();
    bool hideEntirePublicMap();
    void restoreLocalPlayerDisplayState(const LocalPlayerDisplayModel &display);
    void clearLocalPlayerDisplayState();
    void setPublicScene(const EntityId &sceneId);
    void setPublicMap(const EntityId &mapId);
    void setPublicMapReleaseCategory(MapPlayerReleaseCategory category, bool visible);
    void setPublicEncounter(const EntityId &encounterId);
    void setShowPublicMap(bool show);
    void setShowPublicInitiative(bool show);
    void setShowPublicNpcs(bool show);
    void setShowPublicHandouts(bool show);
    void setShowSharedArt(bool show);
    void setShowPublicParty(bool show);
    void setShowPublicPartySpellSlots(bool show);
    void setShowPublicPartyInventory(bool show);
    void setPublicCharacterVisibility(const EntityId &characterId, bool visible);
    void setPlayerTextScale(float scale);
    void setPublicNpcVisibility(const EntityId &npcId, bool visible);
    void setPublicHandoutVisibility(const EntityId &handoutId, bool visible);
    void quickRollD20();
    void advanceCombatTurn();
    void openCampaignNotes();
    void toggleAudioMute();
    const std::string &shortcutStatusMessage() const;
    bool shouldShowLiveUtilityRail() const;

    void showPlayerCreator();
    void showNPCCreator();
    void showPlayerOverview();
    void showCharacterSheet();
    void showNPCOverview();
    void showItemCreator();
    void showItemCatalog();
    void showInventoryManager();
    void showMapCreator(bool newMap = false);
    bool createMapFromWorldPlace(const EntityId &placeId);
    void showMapViewer();
    void showDungeonBuilder();
    void showBattlefieldCreator();
    void showEncounterBuilder();
    void showNewEncounterBuilder();
    void showBattleTracker();
    void showGMScreen();
    void showHandoutCreator();
    void showLiveSessionDashboard();
    void showTabletopToolkit();
    void showTabletopObjectives();
    void showTabletopClues();
    LiveSessionDashboardPhase liveSessionPhase() const;
    void setLiveSessionPhase(LiveSessionDashboardPhase phase);
    void renderLiveWorkspace();
    std::vector<RulesetDefinitionModel> availableRulesetUpgrades() const;
    RulesetUpgradePreview previewRulesetUpgrade(const RulesetDefinitionModel &next) const;
    bool applyRulesetUpgrade(const RulesetDefinitionModel &next, std::vector<std::string> &errors);
    void showLocalPlayerDisplay();
    void showPresentationPanel();
    void showAudioManager();
    void showAudioQuickControls();
    void showShopManager();
    void showSpellLibrary();
    void showStoryWorkspace();
    bool saveRandomTable(const RandomTableModel &table, std::string &error);
    RandomTableRollResult rollRandomTable(const EntityId &tableId);
    bool addRandomTableRollToSessionNotes(const RandomTableRollResult &roll, std::string &error);
    bool createRandomTableRollHandout(const RandomTableRollResult &roll, std::string &error);
    void showGlobalSearch();
    void showKnowledgeGraph();
    void showCommandPalette();
private:
    friend class EditorWindowView;
    bool flushPreparationDrafts(const std::string &reason);
    bool persistCurrentCampaign(const std::string &reason);
    bool updateCampaignAutosave(ProjectSaveService &service);
    void processPreparationTarget();
    void revokePreparationTarget();
    bool consumeCurrentPlayerReview();
    EditorWorkspaceState _workspace;
    void captureWorkspaceLocation();
    void restoreWorkspaceLocation();
    std::shared_ptr<IWidgetController> workspaceTool(const std::string &route) const;
    bool openWorkspaceTool(const std::string &route, gm::ux::WorkspaceTarget workspace);
    bool returnToPreviousWorkspace();
    bool restoreWorkspaceToolSelection(const gm::ux::WorkspaceLocation &location);
    bool renderEmbeddedWorkspaceTool();
    Campaign _currentCampaign;
    std::unique_ptr<EditorWindowView> view;
    EditorChildControllers _children;
    std::unique_ptr<EditorDraftRecoveryController> _editorDraftRecovery;
    EditorShortcutState _shortcuts;
    std::string _outcomeRecoveryError;
    std::string _worldDraftSaveError;
    std::string _editorDraftRecoveryStatus, _partialDraftDiscardError;
    // Used by EditorRandomTables.cpp, which owns the random-table workflow.
    std::mt19937 _randomTableGenerator{std::random_device{}()};

    // Non-owning aliases for the existing workspace navigation implementation.
    gm::ux::WorkspaceNavigation &_navigation = _workspace.navigation;
    bool &_openingWorkspaceEntity = _workspace.openingEntity;
    std::shared_ptr<CharacterSheetController> &_characterSheet = _children.characterSheet;
    std::shared_ptr<PlayerCreatorController> &_playerCreator = _children.playerCreator;
    std::shared_ptr<PlayerOverviewController> &_playerOverview = _children.playerOverview;
    std::shared_ptr<NPCCreatorController> &_npcCreator = _children.npcCreator;
    std::shared_ptr<NPCOverviewController> &_npcOverview = _children.npcOverview;
    std::shared_ptr<ItemCreatorController> &_itemCreator = _children.itemCreator;
    std::shared_ptr<InventoryManagerController> &_inventoryManager = _children.inventoryManager;
    std::shared_ptr<SpellLibraryController> &_spellLibrary = _children.spellLibrary;
    std::shared_ptr<AudioManagerController> &_audioManager = _children.audioManager;
    std::shared_ptr<MapCreatorController> &_mapCreator = _children.mapCreator;
    std::shared_ptr<MapViewerController> &_mapViewer = _children.mapViewer;
    std::shared_ptr<EncounterBuilderController> &_encounterBuilder = _children.encounterBuilder;
    std::shared_ptr<StoryWorkspaceController> &_storyWorkspace = _children.storyWorkspace;
    std::shared_ptr<AudioPlaybackService> &_audioPlaybackService = _children.audioPlaybackService;
    std::shared_ptr<LiveSessionDashboardController> &_liveSessionDashboard = _children.liveSessionDashboard;

    void touchLocalPlayerDisplayAndSave(const std::string &reason);
    bool prepareSavedMapForPlayerReview(const EntityId &mapId);
    void hideAllWorkspaceTools();
    void markSearchIndexDirty();
    void ensureSearchIndex() const;
};
