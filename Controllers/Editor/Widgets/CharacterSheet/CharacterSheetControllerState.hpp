#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Character/CharacterAdvancement.hpp"

#include <string>

enum class CharacterSheetDetailList
{
    Feats,
    TraitsAndFeatures,
    Languages,
    Senses,
    SavingThrows,
    Skills,
    Tools
};

struct CharacterSheetRelationshipTargetOption
{
    std::string type;
    EntityId id;
    std::string label;
};

struct CharacterSheetFeedbackState final
{
    std::string statusMessage;
    std::string errorMessage;
};

struct CharacterSheetAssignmentState final
{
    int selectedSpellIndex = 0;
    std::string spellSearch;
    int selectedItemIndex = 0;
    std::string itemSearch;
    int itemQuantity = 1;
    bool itemEquipped = false;
};

struct CharacterSheetDirectoryState final
{
    int selectedCharacterIndex = 0;
    int pendingRemovalIndex = -1;
    bool pendingRemovalIsArchive = false;
};

struct CharacterSheetAdvancementState final
{
    int targetLevel = 2;
    int hitPointGain = 1;
    bool restoreHealth = true;
    bool manualHitPoints = false;
    bool pending = false;
    EntityId characterId;
    std::string rulesetId;
    std::string rulesetVersion;
    CharacterModel base;
    std::string features;
    CharacterAdvancement::Request choices;
    int step = 0;
    std::string spellSearch;

    CharacterAdvancement::Request request() const;
};

struct CharacterSheetDetailDraftState final
{
    std::string feat;
    std::string feature;
    std::string language;
    std::string sense;
    std::string savingThrow;
    std::string skill;
    std::string tool;
    MonsterActionModel featureAction;
    ResourceTrackerModel resourceTracker;
};

struct CharacterSheetRelationshipDraftState final
{
    int targetTypeIndex = 0;
    int targetIndex = 0;
    std::string label;
    std::string notes;
};

struct CharacterSheetNotesDraftState final
{
    EntityId characterId;
    std::string backstory;
    std::string playerNotes;
    std::string gmNotes;
    std::string savedBackstory;
    std::string savedPlayerNotes;
    std::string savedGmNotes;

    bool dirty() const;
    void clear();
};

class CharacterSheetControllerState final
{
  public:
    CharacterSheetFeedbackState feedback;
    CharacterSheetAssignmentState assignment;
    CharacterSheetDirectoryState directory;
    CharacterSheetAdvancementState advancement;
    CharacterSheetDetailDraftState details;
    CharacterSheetRelationshipDraftState relationship;
    CharacterSheetNotesDraftState notes;

    void resetDrafts();
};
