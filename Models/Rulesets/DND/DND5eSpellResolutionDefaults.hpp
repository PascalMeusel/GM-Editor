#pragma once

#include "Models/Rulesets/DND/DND5e.hpp"

#include <cctype>
#include <string_view>

namespace dnd5e_detail
{
constexpr std::string_view kSpellcastingModifierMarker = "spellcasting modifier";
constexpr std::string_view kTrailingSpellcastingModifier = " + spellcasting modifier";

inline std::string signedModifier(int modifier)
{
    return modifier > 0 ? "+" + std::to_string(modifier) :
           modifier < 0 ? std::to_string(modifier) : std::string{};
}

inline std::string resolveSpellcastingModifierFormula(const std::string &formula, int modifier)
{
    // The catalog owns this exact trailing grammar. Do not partially rewrite a
    // custom expression containing the words in another position: preserving it
    // lets normal dice validation report the unsupported authored formula.
    if (formula.size() <= kTrailingSpellcastingModifier.size() ||
        formula.compare(formula.size() - kTrailingSpellcastingModifier.size(),
                        kTrailingSpellcastingModifier.size(), kTrailingSpellcastingModifier) != 0)
        return formula;

    const std::string dice = formula.substr(0, formula.size() - kTrailingSpellcastingModifier.size());
    return dice + signedModifier(modifier);
}

inline bool hasLegacyBuiltInHealingFormula(const SpellModel &spell)
{
    return (spell.metadata.id == "dnd.srd.spell.cure-wounds" && spell.healing == "2d8") ||
           (spell.metadata.id == "dnd.srd.spell.healing-word" && spell.healing == "2d4");
}
}

inline RuleSpellResolutionSummary DND5e::summarizeSpellResolution(
    const SpellModel &spell,
    const CharacterSpellcastingModel &spellcasting,
    const std::map<std::string, int> &attributes,
    int casterLevel,
    int castAtLevel) const
{
    RuleSpellResolutionSummary summary = IRuleSet::summarizeSpellResolution(
        spell, spellcasting, attributes, casterLevel, castAtLevel);
    if (summary.resolution != SpellResolutionType::Healing)
        return summary;

    const int modifier = getAbilityModifier(attributes, spellcasting.ability);
    // New catalog entries state the rule in their formula, so copying them to a
    // campaign preserves it. The ID fallback only supports older direct catalog
    // entries; it never guesses from a custom spell's name.
    if (spell.healing.find(dnd5e_detail::kSpellcastingModifierMarker) != std::string::npos)
        summary.healing = dnd5e_detail::resolveSpellcastingModifierFormula(spell.healing, modifier);
    else if (dnd5e_detail::hasLegacyBuiltInHealingFormula(spell))
        summary.healing += dnd5e_detail::signedModifier(modifier);
    return summary;
}
