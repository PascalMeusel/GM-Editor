#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Views/Editor/Widgets/Character/CharacterSheet/CharacterSheetView.hpp"
#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Character/CharacterAdvancement.hpp"
#include "Controllers/Editor/Widgets/CharacterSheet/CharacterSheetControllerState.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class CharacterSheetController : public IWidgetController
{
public:
    CharacterSheetController(Campaign &currentCampaign);

    void update(std::shared_ptr<GLFWwindow> window) override;
    void setOpenStoryThreadHandler(std::function<void(const EntityId &)> handler);
    bool selectCharacterById(const EntityId &characterId);
    bool hasUnsavedDraft() const { return _levelUpPending || selectedCharacterNotesDraftDirty(); }
    bool flushDraft() { return !_levelUpPending && (!selectedCharacterNotesDraftDirty() || saveSelectedCharacterNotesDraft()); }
    void resetDrafts();
    bool draftRecoveryReady() { return !_recoveryReady || _recoveryReady(); }
    void notesDraftEdited() { if(_recoveryChanged)_recoveryChanged(); }
    std::filesystem::path resolvePortraitPath(const CharacterModel &character) const;

private:
    friend class CharacterSheetView;
    friend class EditorDraftRecoveryController;
    friend struct EditorDraftRecoveryProbe;
    std::function<bool()> _recoveryReady, _recoveryDiscard;
    std::function<void()> _recoveryChanged, _recoveryCommitted;
    friend struct CharacterNotesWorkflowProbe;
    friend struct CharacterAdvancementProbe;
    friend struct SpellOverrideUiProbe;

    using CharacterDetailList = CharacterSheetDetailList;
    using RelationshipTargetOption = CharacterSheetRelationshipTargetOption;

    std::unique_ptr<CharacterSheetView> view;
    Campaign &_currentCampaign;
    std::function<void(const EntityId &)> _openStoryThreadHandler;
    CharacterSheetControllerState _state;
    int &_selectedCharacterIndex = _state.directory.selectedCharacterIndex;
    std::string &_statusMessage = _state.feedback.statusMessage;
    std::string &_errorMessage = _state.feedback.errorMessage;
    int &_selectedSpellToAddIndex = _state.assignment.selectedSpellIndex;
    std::string &_spellAssignmentSearch = _state.assignment.spellSearch;
    int &_selectedItemToAddIndex = _state.assignment.selectedItemIndex;
    std::string &_itemAssignmentSearch = _state.assignment.itemSearch;
    int &_itemAssignmentQuantity = _state.assignment.itemQuantity;
    bool &_itemAssignmentEquipped = _state.assignment.itemEquipped;
    int &_pendingCharacterRemovalIndex = _state.directory.pendingRemovalIndex;
    bool &_pendingCharacterRemovalIsArchive = _state.directory.pendingRemovalIsArchive;
    int &_levelUpTargetLevel = _state.advancement.targetLevel;
    int &_levelUpHitPointGain = _state.advancement.hitPointGain;
    bool &_levelUpRestoreHealth = _state.advancement.restoreHealth;
    bool &_levelUpManualHitPoints = _state.advancement.manualHitPoints;
    bool &_levelUpPending = _state.advancement.pending;
    EntityId &_levelUpCharacterId = _state.advancement.characterId;
    std::string &_levelUpRulesetId = _state.advancement.rulesetId;
    std::string &_levelUpRulesetVersion = _state.advancement.rulesetVersion;
    CharacterModel &_levelUpBase = _state.advancement.base;
    std::string &_levelUpFeatures = _state.advancement.features;
    CharacterAdvancement::Request &_levelUpChoices = _state.advancement.choices;
    int &_levelUpStep = _state.advancement.step;
    std::string &_levelUpSpellSearch = _state.advancement.spellSearch;
    CharacterAdvancement::Request advancementRequest() const;
    std::string &_newFeatEntry = _state.details.feat;
    std::string &_newFeatureEntry = _state.details.feature;
    std::string &_newLanguageEntry = _state.details.language;
    std::string &_newSenseEntry = _state.details.sense;
    std::string &_newSavingThrowEntry = _state.details.savingThrow;
    std::string &_newSkillEntry = _state.details.skill;
    std::string &_newToolEntry = _state.details.tool;
    MonsterActionModel &_featureActionDraft = _state.details.featureAction;
    ResourceTrackerModel &_resourceTrackerDraft = _state.details.resourceTracker;
    int &_relationshipTargetTypeIndex = _state.relationship.targetTypeIndex;
    int &_relationshipTargetIndex = _state.relationship.targetIndex;
    std::string &_relationshipLabel = _state.relationship.label;
    std::string &_relationshipNotes = _state.relationship.notes;
    EntityId &_notesDraftCharacterId = _state.notes.characterId;
    std::string &_backstoryDraft = _state.notes.backstory;
    std::string &_playerNotesDraft = _state.notes.playerNotes;
    std::string &_gmNotesDraft = _state.notes.gmNotes;
    std::string &_savedBackstory = _state.notes.savedBackstory;
    std::string &_savedPlayerNotes = _state.notes.savedPlayerNotes;
    std::string &_savedGmNotes = _state.notes.savedGmNotes;

    void openStoryThread(const EntityId &threadId);
    void duplicateSelectedCharacter();
    std::vector<std::string> getCharacterReferenceWarnings(int characterIndex) const;
    bool isCharacterArchived(const CharacterModel &character) const;
    void requestArchiveSelectedCharacter();
    void requestDeleteSelectedCharacter();
    void archivePendingCharacter();
    void deletePendingCharacter();
    void requestLevelUpSelectedCharacter();
    void applyLevelUpToSelectedCharacter();
    int suggestedLevelUpHitPointGain(const CharacterModel &character) const;
    void toggleInventoryEntryEquipped(int entryIndex);
    void setInventoryEntryQuantity(int entryIndex, int quantity);
    void removeInventoryEntry(int entryIndex);
    void addItemToSelectedCharacter(const EntityId &itemId, int quantity, bool equipped);
    void setSelectedCharacterName(const std::string &name);
    void setSelectedCharacterRace(const std::string &race);
    void setSelectedCharacterClass(const std::string &className);
    void setSelectedCharacterBackground(const std::string &background);
    void setSelectedCharacterSpeedFeet(int speedFeet);
    void setSelectedCharacterBackstory(const std::string &backstory);
    void setSelectedCharacterPlayerNotes(const std::string &notes);
    void setSelectedCharacterGmNotes(const std::string &notes);
    void ensureNotesDraftForSelectedCharacter();
    bool selectedCharacterNotesDraftDirty() const;
    bool saveSelectedCharacterNotesDraft();
    void revertSelectedCharacterNotesDraft();
    void addCharacterDetailListEntry(CharacterDetailList list, const std::string &value);
    void removeCharacterDetailListEntry(CharacterDetailList list, int index);
    void addFeatureActionToSelectedCharacter();
    void removeFeatureActionFromSelectedCharacter(int actionIndex);
    void addResourceTrackerToSelectedCharacter();
    void removeResourceTrackerFromSelectedCharacter(int trackerIndex);
    void setSelectedCharacterResourceUsed(int trackerIndex, int used);
    void resetSelectedCharacterResource(int trackerIndex);
    std::vector<std::string> getRelationshipTargetTypes() const;
    std::vector<RelationshipTargetOption> getRelationshipTargetOptions(const std::string &type) const;
    std::string describeRelationshipTarget(const CharacterRelationshipModel &relationship) const;
    std::vector<std::string> getSelectedCharacterValidationWarnings() const;
    void addSelectedCharacterRelationship(const std::string &targetType, const EntityId &targetId, const std::string &label, const std::string &notes);
    void removeSelectedCharacterRelationship(int relationshipIndex);
    void setSelectedCharacterLevel(int value);
    void setSelectedCharacterAttribute(const std::string &name, int value);
    void setSelectedCharacterRuleValue(const std::string &stableId, const std::string &displayName, int value);
    void setSelectedCharacterCurrentHitPoints(int value);
    void setSelectedCharacterMaxHitPoints(int value);
    void setSelectedCharacterTemporaryHitPoints(int value);
    void adjustSelectedCharacterHitPoints(int delta);
    void setSpellcastingEnabled(bool enabled);
    void setSpellcastingAbility(const std::string &ability);
    void setSpellAttackBonus(int value);
    void setSpellSaveDc(int value);
    void setSpellAttackMode(SpellValueMode mode);
    void setSpellSaveDcMode(SpellValueMode mode);
    void setSpellSlotMaximum(int spellLevel, int value);
    void setSpellSlotUsed(int spellLevel, int value);
    void resetSpellSlots();
    void addSpellToSelectedCharacter(const EntityId &spellId);
    void togglePreparedSpell(int spellEntryIndex);
    void removeSpellFromSelectedCharacter(int spellEntryIndex);
    void castSpellFromSelectedCharacter(int spellEntryIndex, int slotLevel, bool asRitual = false, bool usePactMagic = false);
    void browseForSelectedCharacterPortrait();
    void clearSelectedCharacterPortrait();
    void openPortraitDirectory();
    void exportSelectedCharacter();
    void importCharacterFromFile();
    bool saveCampaign();
    std::filesystem::path findProjectRoot() const;
    std::filesystem::path campaignPortraitDirectory() const;
    std::string copyPortraitFileToCampaign(const std::filesystem::path &sourcePath, const EntityId &characterId);
    bool isSupportedPortraitFile(const std::filesystem::path &sourcePath) const;
    std::vector<std::string> &mutableCharacterDetailList(CharacterModel &character, CharacterDetailList list) const;
    void removeDeletedCharacterReferences(const EntityId &characterId);
    void onShow() override;
};
