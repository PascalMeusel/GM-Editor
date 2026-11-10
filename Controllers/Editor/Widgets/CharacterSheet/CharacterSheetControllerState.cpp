#include "Controllers/Editor/Widgets/CharacterSheet/CharacterSheetControllerState.hpp"

CharacterAdvancement::Request CharacterSheetAdvancementState::request() const
{
    CharacterAdvancement::Request value = choices;
    value.targetLevel = targetLevel;
    value.manualHitPoints = manualHitPoints;
    value.hitPointGain = hitPointGain;
    value.restoreHealth = restoreHealth;
    value.newFeatures = features;
    return value;
}

bool CharacterSheetNotesDraftState::dirty() const
{
    return backstory != savedBackstory || playerNotes != savedPlayerNotes ||
           gmNotes != savedGmNotes;
}

void CharacterSheetNotesDraftState::clear()
{
    characterId.clear();
    backstory.clear();
    playerNotes.clear();
    gmNotes.clear();
    savedBackstory.clear();
    savedPlayerNotes.clear();
    savedGmNotes.clear();
}

void CharacterSheetControllerState::resetDrafts()
{
    advancement.pending = false;
    advancement.characterId.clear();
    advancement.features.clear();
    notes.clear();
    directory.selectedCharacterIndex = -1;
}
