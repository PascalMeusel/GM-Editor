#pragma once
#include "RulesetResolutionState.hpp"
#include "Models/Magic/SpellModels.hpp"
#include "Models/Combat/EncounterTypes.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>

struct Campaign;
namespace SpellcastingValues
{
enum class Field { Attack, SaveDc };
struct ResolvedValue
{
    SpellValueMode mode = SpellValueMode::Legacy;
    bool legacyUsesOverride = false;
    std::optional<int> effective, calculated, abilityModifier, proficiency;
    int base = 0;
    Reason reason = Reason::None;
    std::string abilityFormula, proficiencyFormula;
};
struct Provenance
{
    RulesetReference requested;
    std::string providerId, providerName, providerVersion;
    Reason reason = Reason::None;
};
ResolvedValue resolve(const Campaign &, const std::map<std::string,int> &attributes,
                      const CharacterSpellcastingModel &, int level, Field);
Provenance provenance(const Campaign &);
bool readyForSpell(const Campaign &, const std::map<std::string,int> &attributes,
                   const CharacterSpellcastingModel &, int level, const SpellModel &, Reason *reason = nullptr);
std::vector<MonsterActionModel> preparedActions(const Campaign &, const CharacterSpellcastingModel &,
    const std::vector<SpellModel> &, const std::map<std::string,int> &attributes, int level);
}
