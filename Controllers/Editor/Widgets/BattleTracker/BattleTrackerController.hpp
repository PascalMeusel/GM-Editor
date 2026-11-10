#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Controllers/Editor/Widgets/BattleTracker/BattleTrackerState.hpp"
#include "Models/Audio/AudioPlaybackService.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/Combat/CombatEditHistory.hpp"
#include "Views/Editor/Widgets/Combat/BattleTracker/BattleTrackerView.hpp"
#include "imgui.h"
#include <memory>
#include <functional>
#include <vector>

class BattleTrackerController : public IWidgetController
{
public:
    BattleTrackerController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService = nullptr);

    void update(std::shared_ptr<GLFWwindow> window) override;
    bool advanceTurnFromShortcut();
    bool selectEncounterById(const EntityId &encounterId);
    bool refreshSavedPreparationById(const EntityId &encounterId);
    void onPreparationSaved(const EntityId &encounterId);
    bool canLeaveForPreparation();
    void renderEmbedded();
    void setHistoryHandler(std::function<bool(bool)> handler,std::function<std::string()> error={})
    { _historyHandler=std::move(handler);_historyError=std::move(error); }

private:
    friend class BattleTrackerView;
    friend struct BattleTrackerPreparationProbe;
    friend struct BattleTrackerWrapUpProbe;
    friend struct BattleTrackerTurnsProbe;
    friend struct BattleTrackerTableBreakProbe;
    std::function<bool(bool)> _historyHandler;
    std::function<std::string()> _historyError;

    std::unique_ptr<BattleTrackerView> _view;
    Campaign &_currentCampaign;
    std::shared_ptr<AudioPlaybackService> _playbackService;
    BattleTrackerContext _context{_currentCampaign, _playbackService};
    BattleTrackerState _state;
    std::size_t _correctionGeneration = 0;
    std::optional<CombatEditHistory::Snapshot> _dragCorrection;
    std::optional<CombatEditHistory::Snapshot> captureCorrection(CombatEditHistory::Kind, const EntityId &resource = {});
    void recordCorrection(const std::optional<CombatEditHistory::Snapshot> &, const std::string &label);

    // Compatibility references keep the established view/controller seam
    // stable while cohesive state objects own the underlying data.
    EncounterModel &_draftEncounter = _state.combat.draftEncounter;
    int &_selectedEncounterIndex = _state.selection.encounterIndex;
    int &_selectedMapIndex = _state.selection.mapIndex;
    int &_selectedCharacterIndex = _state.selection.characterIndex;
    int &_selectedNPCIndex = _state.selection.npcIndex;
    int &_selectedMonsterIndex = _state.selection.monsterIndex;
    int &_selectedParticipantIndex = _state.selection.participantIndex;
    int &_rangeTargetIndex = _state.selection.rangeTargetIndex;
    int &_selectedActionIndex = _state.selection.actionIndex;
    int &_spellSlotEditorLevel = _state.selection.spellSlotEditorLevel;
    int &_selectedDiceRollPresetIndex = _state.selection.diceRollPresetIndex;
    std::vector<int> &_selectedTargetIndices = _state.selection.targetIndices;
    int &_dragParticipantIndex = _state.board.dragParticipantIndex;
    int &_dragStartX = _state.board.dragStartX;
    int &_dragStartY = _state.board.dragStartY;
    float &_zoom = _state.board.zoom;
    bool &_freePlacementMode = _state.board.freePlacementMode;
    int &_pendingHitPointDelta = _state.attack.pendingHitPointDelta;
    int &_manualAttackRangeFeet = _state.attack.manualRangeFeet;
    int &_attackRollResult = _state.attack.attackRoll;
    int &_secondAttackRollResult = _state.attack.secondAttackRoll;
    int &_savingThrowRollResult = _state.attack.savingThrowRoll;
    int &_savingThrowModifier = _state.attack.savingThrowModifier;
    std::vector<int> &_targetSavingThrowRollResults = _state.attack.targetSavingThrowRolls;
    std::vector<int> &_targetSavingThrowModifiers = _state.attack.targetSavingThrowModifiers;
    int &_damageRollResult = _state.attack.damageRoll;
    bool &_healingDiceEntered = _state.attack.healingDiceEntered;
    bool &_lastAttackResolved = _state.attack.lastResolved;
    bool &_lastAttackHit = _state.attack.lastHit;
    bool &_castSelectedSpellAsRitual = _state.attack.castAsRitual;
    bool &_actionAdjudicationReviewed = _state.attack.adjudicationReviewed;
    bool &_attackHasAdvantage = _state.attack.hasAdvantage;
    bool &_attackHasDisadvantage = _state.attack.hasDisadvantage;
    bool &_targetHasCover = _state.attack.targetHasCover;
    bool &_targetProne = _state.attack.targetProne;
    bool &_targetUnseen = _state.attack.targetUnseen;
    bool &_targetHidden = _state.attack.targetHidden;
    std::string &_lastAttackMessage = _state.attack.lastMessage;
    std::string &_gmAdjudicationNotes = _state.attack.adjudicationNotes;
    std::string &_manualActionName = _state.attack.manualActionName;
    std::string &_manualDamageFormula = _state.attack.manualDamageFormula;
    std::string &_damageTypeDraft = _state.attack.damageTypeDraft;
    std::optional<bool> &_pendingTableBreakResume = _state.tableBreak.pendingResume;
    EntityId &_pendingTableBreakSessionId = _state.tableBreak.sessionId;
    std::string &_pendingTableBreakDetail = _state.tableBreak.detail;
    std::string &_pendingTableBreakClock = _state.tableBreak.clock;
    std::string &_diceRollPresetName = _state.dice.presetName;
    std::string &_diceRollExpression = _state.dice.expression;
    std::string &_diceRollPresetNotes = _state.dice.presetNotes;
    std::string &_lastDiceRollMessage = _state.dice.lastMessage;
    int &_combatStep = _state.combat.step;
    bool &_initiativeLocked = _state.combat.initiativeLocked;
    bool &_deferCombatSave = _state.combat.deferSave;
    bool &_encounterSaveFailed = _state.combat.saveFailed;
    std::string &_combatStatusMessage = _state.combat.statusMessage;
    std::string &_combatErrorMessage = _state.combat.errorMessage;
    std::string &_conditionDraft = _state.combat.conditionDraft;
    std::string &_combatLogNote = _state.combat.combatLogNote;

    void startNewEncounter();
    void loadSelectedEncounter();
    void saveEncounter();
    void persistEncounterDraft();
    void setSelectedTemporaryHitPoints(int value);
    void setCombatPhase(CombatPhase phase);
    void changeCombatPhase(CombatPhase phase);
    bool isActiveCombatPhase() const;
    static bool combatPhaseRunsActiveTurn(CombatPhase phase);
    void synchronizeParticipantsWithSources();
    void addSelectedPlayer();
    void addAllPlayers();
    void addSelectedNPC();
    void addSelectedMonster();
    void removeSelectedParticipant();
    void rollInitiativeForAll();
    void sortInitiative();
    void beginCombat();
    void pauseCombat();
    void resumeCombat();
    void startCombatTableBreak(bool pauseAudio);
    void changeCombatTableBreak(bool resume, bool changeAudio);
    void persistPendingTableBreak();
    void resumeCombatFromTableBreak(bool resumeAudio);
    bool hasActiveSession() const;
    bool isSessionBreakActive() const;
    int playingAudioChannelCount() const;
    int pausedAudioChannelCount() const;
    void moveParticipantInOrder(int participantIndex, int direction);
    void startActiveTurn();
    void selectActiveParticipant();
    void nextTurn();
    void advanceCombatTurn(bool skipped);
    void skipTurn();
    void resolveAttackRoll();
    void applyDamageRoll();
    MonsterActionModel getSelectedAction() const;
    bool selectedActionIsSpell() const;
    bool canCastSelectedSpell(std::string *reason = nullptr) const;
    bool selectedSpellCanBeCastAsRitual() const;
    std::string selectedSpellSlotStatus() const;
    void castSelectedSpell();
    void resolveSelectedSpellCast();
    bool getSelectedSpellResolutionSummary(RuleSpellResolutionSummary &summary) const;
    int getSavingThrowTotal() const;
    int getSavingThrowTotalForTarget(int targetIndex) const;
    void ensureSavingThrowInputs();
    RuleDamageResult previewSpellDamageOrHealingForTarget(int targetIndex, int amount, bool healing) const;
    std::string previewSpellOutcomeForTarget(int targetIndex, const RuleSpellResolutionSummary &summary, bool targetFailedOrHit) const;
    void setSelectedSpellSlotMaximum(int spellLevel, int value);
    void setSelectedSpellSlotUsed(int spellLevel, int value);
    CharacterSpellcastingModel *getSelectedSourceSpellcasting();
    const CharacterSpellcastingModel *getSelectedSourceSpellcasting() const;
    std::vector<ResourceTrackerModel> *getSelectedSourceResources();
    const std::vector<ResourceTrackerModel> *getSelectedSourceResources() const;
    void setSelectedSourceResourceUsed(int trackerIndex, int used);
    void resetSelectedSourceResource(int trackerIndex);
    std::vector<int> getSelectedTargetIndices() const;
    void addConditionToSelectedParticipant();
    void removeConditionFromSelectedParticipant(int conditionIndex);
    void removeActiveEffectFromSelectedParticipant(int effectIndex);
    void syncSelectedParticipantConditionsToSource();
    void addCombatLogEntry(const std::string &eventType, const std::string &summary, const std::string &actorName = "");
    void addCombatLogNote();
    void completeEffectsReview();
    void setNarrativePosition(int participantIndex, const std::string &position);
    void setNarrativeRangeBand(int participantIndex, const std::string &rangeBand);
    void setNarrativeSpotlight(int participantIndex);
    void setNarrativeRelativeTo(int participantIndex, int anchorIndex, const std::string &rangeBand, const std::string &relationship);
    void rollDiceExpression();
    bool rollSelectedSpellHealing();
    void addDiceRollPreset();
    void updateSelectedDiceRollPreset();
    void removeSelectedDiceRollPreset();
    void loadSelectedDiceRollPreset();
    void generateAfterCombatSummary();
    int getSelectedAttackModifier() const;
    int getAttackTotal() const;
    int getSelectedDamageModifier() const;
    int getDamageTotal() const;
    int getArmorAdjustedDamageTotal() const;
    int getSpellHealingTotal() const;
    bool spendSelectedSpellSlot(const MonsterActionModel &action, EncounterParticipantModel &participant, std::string *reason = nullptr);
    bool spendSelectedActionResource(const MonsterActionModel &action, std::string *reason = nullptr);
    std::string reactionResourceStatus(int participantIndex, int actionIndex) const;
    bool reactionResourceAvailable(int participantIndex, int actionIndex, std::string *reason = nullptr) const;
    bool spendReactionActionResource(int participantIndex, int actionIndex, std::string *reason = nullptr);
    void applySpellDamageToTarget(int targetIndex, int damageAmount, const std::string &actorName);
    void applySpellHealingToTarget(int targetIndex, int healingAmount, const std::string &actorName);
    void applySpellEffectToTarget(int targetIndex, const RuleSpellResolutionSummary &summary, const std::string &actorName);
    void beginSelectedCasterConcentration(const RuleSpellResolutionSummary &summary);
    void advanceTimedEffectsForNewRound();
    void processTurnBoundaryEffects(int participantIndex, const std::string &boundary);
    void applyHitPointDeltaToSelected(int delta);
    bool moveParticipant(int participantIndex, int x, int y);
    void beginParticipantDrag(int participantIndex);
    void endParticipantDrag();
    bool canParticipantMoveTo(int participantIndex, int x, int y) const;
    int distanceBetweenParticipants(int attackerIndex, int targetIndex) const;
    std::vector<MonsterDefinitionModel> getMonsterCatalog() const;
    std::vector<ItemModel> getAvailableItems() const;
    MapModel *getSelectedMap();
    const MapModel *getSelectedMap() const;
    ImU32 participantColor(const EncounterParticipantModel &participant) const;
    int findOpenX() const;
    int findOpenY() const;
    void saveCampaign();
    void onShow() override;
};
