#pragma once
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cstdint>
#include <map>

#include "Views/IWindowView.hpp"
#include "Models/Core/EntityMetadata.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Session/PlayerReviewSession.hpp"
#include "Models/Session/LocalPlayerDisplayModel.hpp"
#include "Models/Shop/ShopModel.hpp"
#include "Models/Dungeon/DungeonModel.hpp"
#include "Models/Project/WorldInspectorService.hpp"
#include "Models/Project/WorldInspectorDrafts.hpp"
#include "Views/Editor/EditorWorldRecoveryState.hpp"
#include "Views/Editor/WorkspaceUi.hpp"
#include "Views/Editor/SessionWorkspace.hpp"
#include "Views/Editor/ExchangePanel.hpp"
#include "Views/Editor/Widgets/TagEditor.hpp"

class EditorWindowController;

class EditorWindowView : public IWindowView
{
public:
    EditorWindowView() = delete;
    EditorWindowView(EditorWindowController &controller);
    ~EditorWindowView() = default;
    // Function to update the view
    void update(std::shared_ptr<GLFWwindow> window) override;

private:
    enum class PresentationReviewAction
    {
        None,
        Arm,
        Publish
    };

    struct ToastMessage
    {
        std::string title;
        std::string body;
        ImVec4 color;
        double createdAt = 0.0;
    };

    EditorWindowController &_controller;
    SessionWorkspace _sessionWorkspace;
    ExchangePanel _exchange;
    std::string _diagnosticPreview, _diagnosticStatus;
    void renderProjectTools();
    void renderEditorDraftRecoveryLauncher();
    void renderEditorDraftRecovery();
    bool _editorDraftRecoveryCopied = false;
    bool _campaignNavigationOpen = true;
    EditorWorkspace _activeWorkspace = EditorWorkspace::Home;
    bool _returnToLiveContext = false;
    std::string _workspaceFilter;
    int _compendiumCategoryFilter = 0;
    int _worldCategoryFilter = 0;
    EntityId _workspaceSelectionId;
    EntityId _pendingWorkspaceArchiveId;
    std::string _pendingWorkspaceArchiveType;
    std::string _pendingWorkspaceArchiveName;
    std::string _workspaceArchiveStatus;
    EntityId _worldDraftEntityId;
    enum class WorldDraftKind { None, Place, Nation, Shop };
    WorldDraftKind _worldDraftKind = WorldDraftKind::None;
    bool _worldFormEdited = false, _worldDraftReloadRequested = false;
    WorldInspectorDrafts::Store _worldDrafts;
    EditorWorldRecoveryState _worldRecovery;
    EntityId _worldDraftPopupCampaignId;
    WorldPlaceModel _worldPlaceBase;
    NationModel _worldNationBase;
    ShopModel _worldShopBase;
    void retainWorldDraft();
    bool hasPendingWorldDrafts() const;
    void acknowledgeWorldDrafts(const WorldInspectorDrafts::Scope &);
    void discardWorldDrafts(const WorldInspectorDrafts::Scope &);
    void clearWorldDrafts();
    void renderWorldDraftStatus();
    void resetWorldRecovery();
    void setWorldRecoveryError(std::string message);
    bool worldRecoveryBound() const;
    bool worldRecoveryReady();
    bool checkpointWorldRecovery(bool force=false,EditorWorldRecoveryState::Clock::time_point now=EditorWorldRecoveryState::Clock::now());
    bool discardWorldDraftsChecked(const WorldInspectorDrafts::Scope &);
    void cleanupWorldRecoveryAfterCommit();
    bool renderWorldRecoveredDrafts();
    void renderWorldRecoveryNotice();
    const char *worldRecoveryAttentionTooltip() const;
    WorldInspectorDrafts::StockTransfer _worldStockTransfer;
    bool _worldStockTransferReviewed=false;
    void drawWorldShopTransactions(EntityId shopId,EntityId itemId,std::string itemName);
    ShopModel _worldShopDraft;
    DungeonModel _worldDungeonDraft;
    bool _worldDungeonDirty = false;
    std::map<EntityId, DungeonModel> _retainedDungeonDrafts;
    void retainDungeonDraft();
    WorldInspector::MutationResult applyDungeonDrafts(Campaign &campaign);
    void clearDungeonDrafts();
    NationModel _worldNationDraft;
    WorldPlaceModel _worldPlaceDraft;
    int _worldSelectedTerritoryIndex = -1;
    EntityId _worldSelectedRoomId;
    bool _worldPlaceRoom = false;
    std::string _worldNewShopName;
    std::string _worldNewMerchantName;
    bool _createHandoutRequested = false;
    HandoutModel _handoutCreationDraft;
    std::string _handoutCreationError;
    bool _handoutCreationDirty = false;
    EntityId _randomTableDraftId;
    RandomTableModel _randomTableDraft;
    int _randomTableEntryIndex = -1;
    bool _randomTableDraftDirty = false;
    std::string _randomTableStatus;
    std::vector<RandomTableRollResult> _randomTableRollHistory;
    int _worldSelectedStockIndex = -1;
    int _worldAddStockItemIndex = 0;
    ImGuiTextFilter _worldStockFilter;
    int _worldTransferQuantity = 1;
    int _worldConnectionTargetIndex = 0;
    std::string _worldInspectorStatus;
    EntityId _pendingWorldDeleteId;
    std::string _pendingWorldDeleteKind;
    bool _pendingWorkspaceArchiveState = false;
    std::string _localizationStatus;
    std::string _accessibilityStatus;
    int _selectedRulesetUpgradeIndex = 0;
    bool _rulesetUpgradePreviewOpen = false;
    bool _requestRulesetUpgradePreview = false;
    bool _rulesetUpgradeConfirmed = false;
    std::string _rulesetUpgradeStatus;
    std::string _campaignNameDraft;
    std::string _descriptionDraft;
    std::string _campaignDetailsStatus;
    std::string _activeSidebarWorkflowLabel = "Campaign Home";
    std::string _sidebarSearchQuery;
    std::string _globalSearchQuery;
    std::string _newCollectionName;
    std::string _commandPaletteQuery;
    std::string _selectedTagEntityId;
    std::string _selectedTagEntityLabel;
    std::vector<std::string> _selectedTagEntityTags;
    EntityId _selectedKnowledgeGraphNodeId;
    std::string _selectedKnowledgeGraphEdgeKey;
    std::vector<std::string> _ignoredHealthDiagnosticKeys;
    std::vector<std::string> _resolvedHealthDiagnosticKeys;
    TagEditorState _tagEditorState;
    EntityId _descriptionCampaignId;
    bool _globalSearchOpen = false;
    bool _knowledgeGraphOpen = false;
    bool _commandPaletteOpen = false;
    bool _universalSearchOpen = false;
    bool _focusUniversalSearchNextFrame = false;
    bool _presentationPanelOpen = false;
    bool _openPresentationArmReviewNextFrame = false;
    bool _hasPresentationRestoreSnapshot = false;
    PresentationReviewAction _presentationReviewAction = PresentationReviewAction::None;
    PlayerReviewSession _playerReviewSession;
    std::uint64_t _playerReviewContextEpoch=0;
    std::string _presentationReviewError;
    bool _focusCampaignHomeNextFrame = false;
    bool _focusCampaignNotesNextFrame = false;
    bool _liveUtilityRailExplicitlyOpened = false;
    bool _sidebarHomeOpen = true;
    bool _sidebarCreateOpen = false;
    bool _sidebarPrepOpen = false;
    bool _sidebarRunOpen = false;
    bool _sidebarManageOpen = false;
    bool _sidebarPresentOpen = false;
    bool _sidebarAdvancedOpen = false;
    bool _sidebarSnapshotOpen = false;
    LocalPlayerDisplayModel _presentationRestoreSnapshot;
    std::uint64_t _lastToastEventSequence = 0;
    std::uint64_t _lastToastLogSequence = 0;
    std::vector<ToastMessage> _toasts;

    void renderWindowSelector();
    void renderWorkspaceLanding();
    void renderUniversalSearch();
    bool activateWorkspace(EditorWorkspace workspace, bool rememberReturn = false);
    void renderPersistentSessionClock();
    void renderPersistentAudioMini();
    void renderPersistentMiniMap();
    void renderPresentationPanel();
    void preparePresentationPlayerReview(PresentationReviewAction action);
    void pollPresentationPlayerReview();
    void updatePresentationPlayerReviewError();
    void renderPresentationPlayerReviewModal(const char *popupId);
    void renderCampaignHome();
    void renderGlobalSearch();
    void renderKnowledgeGraph();
    void renderCommandPalette();
    void renderToasts();
    void renderWorldShopInspector(const EntityId &shopId);
    void renderWorldDungeonInspector(const EntityId &dungeonId);
    void renderHandoutCreation();
    bool saveHandoutCreationDraft();
    void reloadRandomTableDraft(const EntityId &tableId);
    void renderRandomTableInspector(const EntityId &tableId);
    bool saveRandomTableDraft();
    void renderWorldNationInspector(const EntityId &nationId);
    void renderWorldPlaceInspector(const EntityId &placeId);
    void renderContextLinks(const EntityId &sourceId);
    EntityId _pendingContextSource, _pendingContextTarget;
    int _pendingContextKind = -1;
    void reloadWorldInspectorDraft(const EntityId &entityId);
    void reportWorldInspectorResult(const WorldInspector::MutationResult &result, const char *successEnglish, const char *successGerman);
    void collectToastMessages();
    void pushToast(const std::string &title, const std::string &body, const ImVec4 &color);

    friend class EditorWindowController;
    friend class EditorWorldHistory;
    friend class EditorMarkdownExchange;
    friend struct WorkspaceInteractionProbe;
    friend struct EditorDraftRecoveryUiProbe;
    friend struct EditorDraftRecoveryProbe;
    friend struct PlayerReviewUiProbe;
    friend struct NotificationHistoryProbe;
};
