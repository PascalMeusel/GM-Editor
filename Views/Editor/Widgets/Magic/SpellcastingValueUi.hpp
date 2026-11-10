#pragma once
#include "Models/Rulesets/SpellcastingValues.hpp"

namespace SpellcastingValueUi
{
struct Edit
{
    SpellValueMode mode;
    int value;
    bool valueEdited = false;
};
const char *reasonText(SpellcastingValues::Reason);
std::optional<Edit> drawField(const Campaign &, const std::map<std::string,int> &,
    const CharacterSpellcastingModel &, int level, SpellcastingValues::Field);
void drawDraftFields(const Campaign &, const std::map<std::string,int> &,
    CharacterSpellcastingModel &, int level);
void drawSourceDetails(const Campaign &, const std::map<std::string,int> &,
    const CharacterSpellcastingModel &, int level);
std::string spellValueText(const Campaign &, const std::map<std::string,int> &,
    const CharacterSpellcastingModel &, int level, const SpellModel &);
}
