#pragma once
#include "Models/Character/CharacterModel.hpp"
#include "Models/Rulesets/IRuleSet.hpp"
#include <string>

namespace CharacterAdvancement
{
struct Request
{
    int targetLevel = 2;
    bool manualHitPoints = false;
    int hitPointGain = 0;
    bool restoreHealth = true;
    std::string newFeatures;
    bool guided = false;
    std::string advancingClass;
    std::map<std::string, int> abilityIncreases;
    std::string newFeats;
    bool automaticSlots = true, automaticResources = true;
    std::vector<int> slotMaximums;
    std::vector<CharacterSpellEntryModel> spellsToAdd;
    std::vector<EntityId> spellsToRemove;
    std::vector<ResourceTrackerModel> resourceChanges;
    std::string overrideReason;
    std::string reviewToken;
};
struct Change
{
    std::string label, before, after;
    bool calculated = false;
};
struct Preview
{
    bool valid = false;
    bool calculatedHitPoints = false;
    int gainedLevels = 0;
    int hitPointGain = 0;
    int maximumHitPoints = 0;
    int proficiencyBefore = 0, proficiencyAfter = 0;
    std::string error;
    int featChoices = 0;
    CharacterModel proposed;
    std::vector<Change> changes;
};
Preview preview(const CharacterModel &character, const IRuleSet &rules, const Request &request,
                const std::vector<SpellModel> *spells = nullptr);
bool apply(CharacterModel &character, const IRuleSet &rules, const Request &request, std::string &error,
           const std::vector<SpellModel> *spells = nullptr);
std::string reviewKey(const Request &request);
std::map<std::string, int> classLevels(const CharacterModel &character);
std::vector<int> standardSlots(const CharacterModel &character);
int pactSlotLevel(const CharacterModel &character);
CharacterSpellcastingModel spellcastingForEntry(const CharacterModel &character, const IRuleSet &rules,
                                                const CharacterSpellEntryModel &entry);
} // namespace CharacterAdvancement
