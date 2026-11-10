#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Controllers/Editor/Widgets/NPCOverview/NPCOverviewState.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Views/Editor/Widgets/Npc/NPCOverview/NPCOverviewView.hpp"
#include <functional>

class NPCOverviewController : public IWidgetController
{
public:
    explicit NPCOverviewController(Campaign &currentCampaign);

    void update(std::shared_ptr<GLFWwindow> window) override;
    bool selectEntityById(const EntityId &entityId, const std::string &entityType);
    void setOpenStoryThreadHandler(std::function<void(const EntityId &)> handler);
    void resetReadingContext();
    EntityId currentSelectedNpcId() const;
    void setPlayerReviewHandler(std::function<void(EntityId)> handler) { _playerReviewHandler = std::move(handler); }

private:
    friend class NPCOverviewView;
    friend struct ContextPreviewUiProbe;
    friend struct SecondaryEditorPersistenceProbe;

    std::unique_ptr<NPCOverviewView> _view;
    Campaign &_currentCampaign;
    std::function<void(EntityId)> _playerReviewHandler;
    std::function<void(const EntityId &)> _openStoryThreadHandler;
    NPCOverviewContext _context{_currentCampaign, _openStoryThreadHandler};
    NPCOverviewState _state;

    // Compatibility references preserve the existing view/controller seam;
    // cohesive state objects own persistence, selection, filters, and drafts.
    bool &_saveFailed = _state.persistence.saveFailed;
    EntityId &_saveFailureCampaignId = _state.persistence.failureCampaignId;
    std::string &_saveErrorMessage = _state.persistence.errorMessage;
    bool &_resourceDraftPendingSave = _state.persistence.resourceDraftPendingSave;
    bool &_actionDraftPendingSave = _state.persistence.actionDraftPendingSave;
    int &_selectedNpcIndex = _state.selection.npcIndex;
    int &_selectedMonsterIndex = _state.selection.monsterIndex;
    int &_selectedMonsterActionIndex = _state.selection.monsterActionIndex;
    int &_selectedFactionIndex = _state.selection.factionIndex;
    int &_selectedDeityIndex = _state.selection.deityIndex;
    int &_selectedCatalogMonsterVariantIndex = _state.selection.catalogMonsterVariantIndex;
    int &_selectedNpcSpellToAddIndex = _state.selection.npcSpellToAddIndex;
    std::string &_npcSpellAssignmentSearch = _state.directory.npcSpellAssignmentSearch;
    std::string &_npcDirectorySearch = _state.directory.npcSearch;
    int &_npcDirectoryCategoryFilter = _state.directory.npcCategoryFilter;
    int &_npcDirectoryThreatFilter = _state.directory.npcThreatFilter;
    bool &_npcDirectoryShowArchived = _state.directory.npcShowArchived;
    bool &_npcDirectoryPlayerVisibleOnly = _state.directory.npcPlayerVisibleOnly;
    bool &_npcDirectoryLiveRelevantOnly = _state.directory.npcLiveRelevantOnly;
    bool &_npcDirectoryStoryLinkedOnly = _state.directory.npcStoryLinkedOnly;
    bool &_npcDirectoryCombatLinkedOnly = _state.directory.npcCombatLinkedOnly;
    std::string &_monsterDirectorySearch = _state.directory.monsterSearch;
    bool &_monsterDirectoryShowArchived = _state.directory.monsterShowArchived;
    MonsterActionModel &_monsterActionDraft = _state.drafts.monsterAction;
    ResourceTrackerModel &_npcResourceDraft = _state.drafts.npcResource;
    int &_pendingNpcRemovalIndex = _state.pendingRemoval.npcIndex;
    bool &_pendingNpcRemovalIsArchive = _state.pendingRemoval.npcIsArchive;
    int &_pendingMonsterRemovalIndex = _state.pendingRemoval.monsterIndex;
    bool &_pendingMonsterRemovalIsArchive = _state.pendingRemoval.monsterIsArchive;

    void openStoryThread(const EntityId &threadId);
    void duplicateSelectedNpc();
    void duplicateSelectedMonster();
    void clampSelectedMonsterActionIndex();
    int importRulesetMonsterCatalog();
    void createVariantFromSelectedCatalogMonster();
    std::vector<std::string> getNpcReferenceWarnings(int npcIndex) const;
    std::vector<std::string> getMonsterReferenceWarnings(int monsterIndex) const;
    bool isNpcArchived(const NPCModel &npc) const;
    bool isMonsterArchived(const MonsterDefinitionModel &monster) const;
    void requestArchiveSelectedNpc();
    void requestDeleteSelectedNpc();
    void requestArchiveSelectedMonster();
    void requestDeleteSelectedMonster();
    void archivePendingNpc();
    void deletePendingNpc();
    void archivePendingMonster();
    void deletePendingMonster();
    void setSelectedNpcCategory(int categoryIndex);
    void setSelectedNpcFactionId(const EntityId &factionId);
    void setSelectedNpcDeityId(const EntityId &deityId);
    void createOrLinkFactionFromSelectedNpcText();
    void createOrLinkDeityFromSelectedNpcName();
    void setSelectedNpcName(const std::string &name);
    void setSelectedNpcFaction(const std::string &faction);
    void setSelectedNpcRole(const std::string &role);
    void setSelectedNpcAttitude(const std::string &attitude);
    void setSelectedNpcSecrets(const std::string &secrets);
    void setSelectedNpcGoals(const std::string &goals);
    void setSelectedNpcResources(const std::string &resources);
    void setSelectedNpcLocation(const std::string &location);
    void setSelectedNpcRelationships(const std::string &relationships);
    void setSelectedNpcThreatLevel(int value);
    void setSelectedNpcDescription(const std::string &description);
    void setSelectedNpcDialogueNotes(const std::string &dialogueNotes);
    void setSelectedNpcAttribute(const std::string &name, int value);
    void setSelectedNpcSpellcastingEnabled(bool enabled);
    void setSelectedNpcSpellcastingAbility(const std::string &ability);
    void setSelectedNpcSpellAttackBonus(int value);
    void setSelectedNpcSpellSaveDc(int value);
    void setSelectedNpcSpellAttackMode(SpellValueMode mode);
    void setSelectedNpcSpellSaveDcMode(SpellValueMode mode);
    void setSelectedNpcSpellSlotMaximum(int spellLevel, int value);
    void setSelectedNpcSpellSlotUsed(int spellLevel, int value);
    void addResourceTrackerToSelectedNpc();
    void removeResourceTrackerFromSelectedNpc(int trackerIndex);
    void setSelectedNpcResourceUsed(int trackerIndex, int used);
    void resetSelectedNpcResource(int trackerIndex);
    void addSpellToSelectedNpc(const EntityId &spellId);
    void toggleSelectedNpcPreparedSpell(int spellEntryIndex);
    void removeSpellFromSelectedNpc(int spellEntryIndex);
    void setNpcPublicVisibility(const EntityId &npcId, bool visible);
    void setSelectedMonsterName(const std::string &name);
    void setSelectedMonsterSize(const std::string &size);
    void setSelectedMonsterType(const std::string &type);
    void setSelectedMonsterChallengeRating(const std::string &challengeRating);
    void setSelectedMonsterArmorClass(int value);
    void setSelectedMonsterArmorDamageReduction(int value);
    void setSelectedMonsterHitPoints(int value);
    void setSelectedMonsterSpeedFeet(int value);
    void setSelectedMonsterExperience(int value);
    void setSelectedMonsterAttribute(const std::string &name, int value);
    void setSelectedMonsterActionName(int actionIndex, const std::string &name);
    void setSelectedMonsterActionReachFeet(int actionIndex, int value);
    void setSelectedMonsterActionRangeFeet(int actionIndex, int value);
    void setSelectedMonsterActionAttackBonus(int actionIndex, const std::string &attackBonus);
    void setSelectedMonsterActionDamage(int actionIndex, const std::string &damage);
    void setSelectedMonsterActionNotes(int actionIndex, const std::string &notes);
    void addActionToSelectedMonster();
    void removeSelectedMonsterAction(int actionIndex);
    void createFaction();
    void setSelectedFactionName(const std::string &name);
    void setSelectedFactionType(const std::string &type);
    void setSelectedFactionSummary(const std::string &summary);
    void setSelectedFactionHeadquarters(const std::string &headquarters);
    void setSelectedFactionLeaderNpcId(const EntityId &leaderNpcId);
    void setSelectedFactionAttitude(const std::string &attitude);
    void setSelectedFactionGoals(const std::string &goals);
    void setSelectedFactionResources(const std::string &resources);
    void setSelectedFactionGmNotes(const std::string &notes);
    void setSelectedFactionInfluence(int value);
    void setSelectedFactionThreatLevel(int value);
    void createDeity();
    void setSelectedDeityName(const std::string &name);
    void setSelectedDeityPowerType(const std::string &powerType);
    void setSelectedDeityPantheon(const std::string &pantheon);
    void setSelectedDeityDomains(const std::string &domains);
    void setSelectedDeitySymbol(const std::string &symbol);
    void setSelectedDeityAlignment(const std::string &alignment);
    void setSelectedDeityTenets(const std::string &tenets);
    void setSelectedDeityWorshipers(const std::string &worshipers);
    void setSelectedDeityRelationships(const std::string &relationships);
    void setSelectedDeityGmNotes(const std::string &notes);
    void setSelectedDeityInfluence(int value);
    void setSelectedDeityThreatLevel(int value);
    void toggleInventoryEntryEquipped(int entryIndex);
    void setInventoryEntryQuantity(int entryIndex, int quantity);
    void removeInventoryEntry(int entryIndex);
    bool saveCampaign();
    void retrySave();
    bool retryPendingSave();
    void refreshSaveContext();
    void removeDeletedNpcReferences(const EntityId &npcId);
    void removeDeletedMonsterReferences(const EntityId &monsterId);
    void onShow() override;
};
