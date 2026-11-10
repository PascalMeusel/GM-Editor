#pragma once
#include "NPCOverviewViewEntitySupport.hpp"
#include "Views/Editor/Widgets/Magic/SpellcastingValueUi.hpp"

namespace
{
std::string trimLowerCopy(const std::string &text)
{
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    const auto first = std::find_if(lowered.begin(), lowered.end(), [](unsigned char character) {
        return !std::isspace(character);
    });
    if (first == lowered.end())
        return {};

    const auto last = std::find_if(lowered.rbegin(), lowered.rend(), [](unsigned char character) {
        return !std::isspace(character);
    }).base();
    return std::string(first, last);
}

void appendSearchText(std::string &text, const std::string &part)
{
    if (part.empty())
        return;
    if (!text.empty())
        text += ' ';
    text += part;
}

const CharacterSpellEntryModel *findNpcSpellEntry(const NPCModel &npc, const EntityId &spellId)
{
    const auto found = std::find_if(npc.spellcasting.spells.begin(), npc.spellcasting.spells.end(), [&](const CharacterSpellEntryModel &entry) {
        return entry.spellId == spellId;
    });
    return found == npc.spellcasting.spells.end() ? nullptr : &*found;
}

std::string signedModifierLabel(int value)
{
    return std::string(value >= 0 ? "+" : "") + std::to_string(value);
}

std::string localizedSpellLevelLabel(int level)
{
    if (level <= 0)
        return localizedUiLabel("Cantrip");
    return localizedUiLabel("Level") + " " + std::to_string(std::clamp(level, 1, 9));
}

std::string localizedSpellResolutionLabel(SpellResolutionType resolution)
{
    return localizedUiLabel(spellResolutionLabel(resolution));
}

std::string spellSourceLabel(const Campaign &campaign, const SpellModel &spell)
{
    const auto customSpell = std::find_if(campaign.customSpells.begin(), campaign.customSpells.end(), [&](const SpellModel &candidate) {
        return candidate.metadata.id == spell.metadata.id;
    });
    if (customSpell != campaign.customSpells.end())
        return localizedUiLabel("Campaign");
    return campaign.ruleSet ? campaign.ruleSet->getName() : localizedUiLabel("Ruleset");
}

std::string spellAssignmentSearchText(const SpellModel &spell, const std::string &source)
{
    std::string text;
    appendSearchText(text, spell.name);
    appendSearchText(text, spellLevelLabel(spell.level));
    appendSearchText(text, spellSchoolLabel(spell.school));
    appendSearchText(text, spellResolutionLabel(spell.resolution));
    appendSearchText(text, source);
    appendSearchText(text, spell.classes);
    appendSearchText(text, spell.castingTime);
    appendSearchText(text, spell.range);
    appendSearchText(text, spell.duration);
    appendSearchText(text, spell.damage);
    appendSearchText(text, spell.healing);
    appendSearchText(text, spell.area);
    appendSearchText(text, spell.effectSummary);
    appendSearchText(text, spell.description);
    return text;
}

std::vector<int> filteredSpellAssignmentIndexes(const Campaign &campaign, const std::vector<SpellModel> &spells, const std::string &query)
{
    std::vector<int> indexes;
    const std::string normalizedQuery = trimLowerCopy(query);
    for (int i = 0; i < static_cast<int>(spells.size()); ++i)
    {
        const SpellModel &spell = spells[static_cast<std::size_t>(i)];
        const std::string source = spellSourceLabel(campaign, spell);
        if (normalizedQuery.empty() || trimLowerCopy(spellAssignmentSearchText(spell, source)).find(normalizedQuery) != std::string::npos)
            indexes.push_back(i);
    }
    return indexes;
}

bool containsSpellAssignmentIndex(const std::vector<int> &indexes, int index)
{
    return std::find(indexes.begin(), indexes.end(), index) != indexes.end();
}

std::string spellAssignmentOptionLabel(const SpellModel &spell, const std::string &source)
{
    return spell.name + " – " + localizedSpellLevelLabel(spell.level) + " – " +
           localizedSpellResolutionLabel(spell.resolution) + " – " + source;
}

std::string npcSpellPreparedContext(const NPCModel &npc, const SpellModel &spell)
{
    const CharacterSpellEntryModel *entry = findNpcSpellEntry(npc, spell.metadata.id);
    if (!entry)
        return localizedUiLabel("Not learned");
    if (spell.level == 0)
        return localizedUiLabel("Cantrip action");
    return localizedUiLabel(entry->prepared ? "Prepared action" : "Known, not prepared");
}

std::string npcSpellCastContext(const Campaign &campaign, const NPCModel &npc, const SpellModel &spell)
{
    if (!findNpcSpellEntry(npc, spell.metadata.id))
        return localizedUiLabel("Learn it before it can appear in combat.");
    SpellcastingValues::Reason calculationReason = SpellcastingValues::Reason::None;
    if (!SpellcastingValues::readyForSpell(campaign, npc.attributes, npc.spellcasting, 1, spell, &calculationReason))
        return SpellcastingValueUi::reasonText(calculationReason);
    if (!campaign.ruleSet)
        return localizedUiLabel(spell.level == 0 ? "Cantrip, slot-free." : "Slot use depends on the active ruleset.");

    std::string reason;
    campaign.ruleSet->canSpendSpellSlot(npc.spellcasting, spell.level, &reason);
    return localizedUiLabel(reason.c_str());
}

std::string npcSpellResolutionPreview(const Campaign &campaign, const NPCModel &npc, const SpellModel &spell)
{
    CharacterSpellcastingModel spellcasting = npc.spellcasting;
    normalizeSpellcasting(spellcasting);
    if (spell.resolution == SpellResolutionType::SpellAttack || spell.resolution == SpellResolutionType::SavingThrow)
        return SpellcastingValueUi::spellValueText(campaign, npc.attributes, spellcasting, 1, spell);
    SpellcastingValues::Reason calculationReason = SpellcastingValues::Reason::None;
    if (!SpellcastingValues::readyForSpell(campaign, npc.attributes, spellcasting, 1, spell, &calculationReason))
        return SpellcastingValueUi::reasonText(calculationReason);
    if (campaign.ruleSet)
    {
        const RuleSpellResolutionSummary summary = campaign.ruleSet->summarizeSpellResolution(spell, spellcasting, npc.attributes, 1, spell.level);
        if (!summary.damage.empty())
            return localizedUiLabel("Damage") + " " + summary.damage;
        if (!summary.healing.empty())
            return localizedUiLabel("Healing") + " " + summary.healing;
        if (!summary.area.shape.empty())
            return localizedSpellResolutionLabel(spell.resolution) + " " + localizedUiLabel("area") + " " +
                   localizedUiLabel(summary.area.shape.c_str()) + " " + std::to_string(summary.area.sizeFeet) + " " + localizedUiLabel("ft");
    }

    if (!spell.damage.empty())
        return localizedUiLabel("Damage") + " " + spell.damage;
    if (!spell.healing.empty())
        return localizedUiLabel("Healing") + " " + spell.healing;
    return localizedSpellResolutionLabel(spell.resolution);
}
}
