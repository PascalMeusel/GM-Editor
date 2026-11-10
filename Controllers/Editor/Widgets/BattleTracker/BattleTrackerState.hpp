#pragma once

#include "Models/Audio/AudioPlaybackService.hpp"
#include "Models/Campaign/Campaign.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

class BattleTrackerContext final
{
public:
    BattleTrackerContext(Campaign &campaign, std::shared_ptr<AudioPlaybackService> &playbackService)
        : campaign(campaign), playbackService(playbackService)
    {
    }

    Campaign &campaign;
    std::shared_ptr<AudioPlaybackService> &playbackService;
};

struct BattleTrackerSelectionState
{
    int encounterIndex = -1;
    int mapIndex = 0;
    int characterIndex = 0;
    int npcIndex = 0;
    int monsterIndex = 0;
    int participantIndex = -1;
    int rangeTargetIndex = -1;
    int actionIndex = 0;
    int spellSlotEditorLevel = 1;
    int diceRollPresetIndex = 0;
    std::vector<int> targetIndices;
};

struct BattleTrackerBoardInteractionState
{
    int dragParticipantIndex = -1;
    int dragStartX = 0;
    int dragStartY = 0;
    float zoom = 1.0f;
    bool freePlacementMode = false;
};

struct BattleTrackerAttackState
{
    int pendingHitPointDelta = 0;
    int manualRangeFeet = 5;
    int attackRoll = 10;
    int secondAttackRoll = 10;
    int savingThrowRoll = 10;
    int savingThrowModifier = 0;
    std::vector<int> targetSavingThrowRolls;
    std::vector<int> targetSavingThrowModifiers;
    int damageRoll = 0;
    bool healingDiceEntered = false;
    bool lastResolved = false;
    bool lastHit = false;
    bool adjudicationReviewed = false;
    bool hasAdvantage = false;
    bool hasDisadvantage = false;
    bool targetHasCover = false;
    bool targetProne = false;
    bool targetUnseen = false;
    bool targetHidden = false;
    bool castAsRitual = false;
    std::string lastMessage;
    std::string adjudicationNotes;
    std::string manualActionName = "Manual Attack";
    std::string manualDamageFormula;
    std::string damageTypeDraft;
};

struct BattleTrackerTableBreakState
{
    std::optional<bool> pendingResume;
    EntityId sessionId;
    std::string detail;
    std::string clock;
};

struct BattleTrackerDiceState
{
    std::string presetName = "Quick Roll";
    std::string expression = "1d20";
    std::string presetNotes;
    std::string lastMessage;
};

struct BattleTrackerCombatState
{
    EncounterModel draftEncounter;
    int step = 0;
    bool initiativeLocked = false;
    bool deferSave = false;
    bool saveFailed = false;
    std::string statusMessage;
    std::string errorMessage;
    std::string conditionDraft;
    std::string combatLogNote;
};

struct BattleTrackerState
{
    BattleTrackerCombatState combat;
    BattleTrackerSelectionState selection;
    BattleTrackerBoardInteractionState board;
    BattleTrackerAttackState attack;
    BattleTrackerTableBreakState tableBreak;
    BattleTrackerDiceState dice;
};
