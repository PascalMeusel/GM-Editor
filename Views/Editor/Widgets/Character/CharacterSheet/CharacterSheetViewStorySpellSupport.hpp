#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include "CharacterSheetView.hpp"
#include "Controllers/Editor/Widgets/CharacterSheet/CharacterSheetController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Rulesets/CharacterRulesService.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Models/Rulesets/RulesetPresentationFormatter.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Views/Editor/Widgets/Inventory/InventoryDisplayHelpers.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"
#include "Views/Editor/Widgets/UiPortrait.hpp"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>


namespace
{
struct CharacterStoryReference
{
    int threadIndex = -1;
    std::string beatTitle;
    std::string linkLabel;
};

bool linkTargetsCharacter(const StoryLinkModel &link, const EntityId &characterId)
{
    return link.targetEntityType == "Player" && link.targetEntityId == characterId;
}

std::vector<CharacterStoryReference> findCharacterStoryReferences(const Campaign &campaign, const CharacterModel &character)
{
    std::vector<CharacterStoryReference> references;
    for (int threadIndex = 0; threadIndex < static_cast<int>(campaign.storyThreads.size()); ++threadIndex)
    {
        const StoryThreadModel &thread = campaign.storyThreads[threadIndex];
        for (const StoryLinkModel &link : thread.links)
        {
            if (linkTargetsCharacter(link, character.metadata.id))
                references.push_back({threadIndex, "", link.label});
        }

        for (const StoryBeatModel &beat : thread.beats)
        {
            for (const StoryLinkModel &link : beat.links)
            {
                if (linkTargetsCharacter(link, character.metadata.id))
                    references.push_back({threadIndex, beat.title, link.label});
            }
        }
    }
    return references;
}

std::string characterConditionsLabel(const CharacterModel &character)
{
    if (character.conditions.empty())
        return localizedUiLabel("None");

    std::string label;
    for (const std::string &condition : character.conditions)
    {
        if (!label.empty())
            label += ", ";
        label += localizedUiLabel(condition.c_str());
    }
    return label;
}

std::string characterSpellSlotsLabel(CharacterSpellcastingModel spellcasting)
{
    if (!spellcasting.enabled)
        return localizedUiLabel("No magic");

    normalizeSpellcasting(spellcasting);
    int maximumTotal = 0;
    int usedTotal = 0;
    for (int level = 1; level <= 9; ++level)
    {
        maximumTotal += spellcasting.slotMaximums[static_cast<std::size_t>(level)];
        usedTotal += spellcasting.slotUsed[static_cast<std::size_t>(level)];
    }

    if (maximumTotal <= 0)
        return localizedUiLabel("Magic, no slots");
    return std::to_string(std::max(0, maximumTotal - usedTotal)) + "/" + std::to_string(maximumTotal) + " " + localizedUiLabel("slots left");
}

std::string lowerText(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsText(const std::string &value, const std::string &query)
{
    if (query.empty())
        return true;
    return lowerText(value).find(lowerText(query)) != std::string::npos;
}

std::string spellSourceLabel(const Campaign &campaign, const SpellModel &spell)
{
    const bool campaignCustom = std::any_of(campaign.customSpells.begin(), campaign.customSpells.end(), [&](const SpellModel &customSpell) {
        return customSpell.metadata.id == spell.metadata.id;
    });
    if (campaignCustom)
        return localizedUiLabel("Campaign");
    return campaign.ruleSet ? campaign.ruleSet->getName() : localizedUiLabel("Ruleset");
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

std::string spellSummaryLine(const SpellModel &spell)
{
    if (!spell.damage.empty())
        return spell.damage;
    if (!spell.healing.empty())
        return spell.healing;
    if (!spell.effectSummary.empty())
        return spell.effectSummary;
    if (!spell.description.empty())
        return spell.description;
    return localizedUiLabel("No effect summary.");
}

const CharacterSpellEntryModel *findCharacterSpellEntry(const CharacterSpellcastingModel &spellcasting, const EntityId &spellId)
{
    const auto found = std::find_if(spellcasting.spells.begin(), spellcasting.spells.end(), [&](const CharacterSpellEntryModel &entry) {
        return entry.spellId == spellId;
    });
    return found == spellcasting.spells.end() ? nullptr : &(*found);
}

std::string spellAssignmentContext(const CharacterModel &character, const SpellModel &spell)
{
    const CharacterSpellEntryModel *entry = findCharacterSpellEntry(character.spellcasting, spell.metadata.id);
    if (!entry)
        return localizedUiLabel("Not learned");
    if (spell.level <= 0)
        return localizedUiLabel(entry->prepared ? "Learned, prepared cantrip" : "Learned cantrip");
    return localizedUiLabel(entry->prepared ? "Learned, prepared" : "Learned, not prepared");
}

std::string spellCastContext(const Campaign &campaign, const CharacterModel &character, const SpellModel &spell)
{
    const CharacterSpellEntryModel *entry = findCharacterSpellEntry(character.spellcasting, spell.metadata.id);
    if (!entry)
        return localizedUiLabel("Preview only");
    if (spell.level <= 0)
        return localizedUiLabel("Cast-ready cantrip");
    if (!entry->prepared)
        return localizedUiLabel("Prepare before casting");
    if (!campaign.ruleSet)
        return localizedUiLabel("Prepared; slot rules unavailable");

    CharacterSpellcastingModel spellcasting = character.spellcasting;
    normalizeSpellcasting(spellcasting);
    std::string reason;
    if (campaign.ruleSet->canSpendSpellSlot(spellcasting, spell.level, &reason))
        return localizedUiLabel("Prepared; slot available");
    return reason.empty() ? localizedUiLabel("No slot available") : localizedUiLabel(reason.c_str());
}

bool spellAssignmentSearchMatches(const Campaign &campaign, const SpellModel &spell, const std::string &query)
{
    return containsText(spell.name, query) ||
           containsText(spellLevelLabel(spell.level), query) ||
           containsText(spellSchoolLabel(spell.school), query) ||
           containsText(spellResolutionLabel(spell.resolution), query) ||
           containsText(spell.classes, query) ||
           containsText(spell.effectSummary, query) ||
           containsText(spell.description, query) ||
           containsText(spellSourceLabel(campaign, spell), query);
}
}
