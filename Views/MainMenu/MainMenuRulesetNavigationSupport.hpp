#pragma once
#include "MainMenuLauncherSupport.hpp"

namespace
{
constexpr int kRulesetLabStepCount = 6;

int rulesetLabStepIndex(RulesetLabStep step)
{
    return static_cast<int>(step);
}

RulesetLabStep rulesetLabStepFromIndex(int index)
{
    const int clampedIndex = std::clamp(index, 0, kRulesetLabStepCount - 1);
    return static_cast<RulesetLabStep>(clampedIndex);
}

const char *rulesetLabStepLabel(RulesetLabStep step)
{
    switch (step)
    {
    case RulesetLabStep::Basics:
        return rulesetStudioText("Basics", "Grundlagen");
    case RulesetLabStep::Template:
        return rulesetStudioText("Template", "Vorlage");
    case RulesetLabStep::StarterTerms:
        return rulesetStudioText("Core Lists", "Grundlisten");
    case RulesetLabStep::DerivedStats:
        return rulesetStudioText("Formula Lists", "Formellisten");
    case RulesetLabStep::AdvancedIds:
        return rulesetStudioText("Advanced IDs", "Erweiterte IDs");
    case RulesetLabStep::Review:
        return rulesetStudioText("Validation / Review", "Validierung / Prüfung");
    default:
        return rulesetStudioText("Ruleset Studio", "Regelwerk-Studio");
    }
}

const char *rulesetLabNextAction(RulesetLabStep step)
{
    switch (step)
    {
    case RulesetLabStep::Basics:
        return rulesetStudioText("Name the definition draft.", "Benenne den Regelwerksentwurf.");
    case RulesetLabStep::Template:
        return rulesetStudioText("Choose a starting point for your rules.", "Wähle einen Ausgangspunkt für deine Regeln.");
    case RulesetLabStep::StarterTerms:
        return rulesetStudioText("Edit abilities, skills, and other core ruleset lists.",
                                 "Bearbeite Attribute, Fertigkeiten und weitere Grundlisten.");
    case RulesetLabStep::DerivedStats:
        return rulesetStudioText("Add formula-backed definitions and preview their values.",
                                 "Ergänze formelbasierte Definitionen und prüfe ihre Werte.");
    case RulesetLabStep::AdvancedIds:
        return rulesetStudioText("Review stable IDs only if needed.", "Prüfe stabile IDs nur bei Bedarf.");
    case RulesetLabStep::Review:
        return rulesetStudioText("Validate and save the installed draft.",
                                 "Validiere und speichere den installierten Entwurf.");
    default:
        return rulesetStudioText("Complete ruleset definition setup.", "Schließe die Regelwerkseinrichtung ab.");
    }
}

const char *rulesetTemplateIntentName(int selectedTemplateIndex)
{
    switch (selectedTemplateIndex)
    {
    case 1:
        return rulesetStudioText("Blank ruleset draft", "Leerer Regelwerksentwurf");
    case 0:
    default:
        return rulesetStudioText("D&D-like baseline", "D&D-ähnliche Grundlage");
    }
}

const char *rulesetTemplateIntentSummary(int selectedTemplateIndex)
{
    switch (selectedTemplateIndex)
    {
    case 1:
        return rulesetStudioText("Start with empty lists and add the abilities, skills, and other rules your game needs.",
                                 "Ein Entwurf ohne vorbelegte Begriffe, den du von Grund auf aufbaust.");
    case 0:
    default:
        return rulesetStudioText(
            "Start with familiar abilities, skills, and resources, then adjust them in the next steps. The built-in D&D rules stay available.",
            "Beginne mit vertrauten Attributen, Fertigkeiten und Ressourcen und passe sie in den nächsten Schritten an. Die integrierten D&D-Regeln bleiben verfügbar.");
    }
}

void applyRulesetTemplateIntent(int selectedTemplateIndex,
                                std::string &baseId,
                                std::vector<RulesetAbilityScoreDefinition> &abilities,
                                std::vector<RulesetSkillDefinition> &skills,
                                std::vector<RulesetDerivedStatDefinition> &derivedStats,
                                std::vector<RulesetDefenseDefinition> &defenses,
                                std::vector<RulesetResourceDefinition> &resources,
                                std::vector<RulesetCurrencyDefinition> &currencies,
                                std::vector<RulesetSizeDefinition> &sizes,
                                std::vector<RulesetSpeedDefinition> &speeds,
                                std::vector<RulesetConditionDefinition> &conditions,
                                std::vector<RulesetClassDefinition> &classes,
                                std::vector<RulesetAncestryDefinition> &ancestries,
                                std::vector<RulesetBackgroundDefinition> &backgrounds,
                                std::vector<RulesetFeatureDefinition> &features,
                                std::vector<RulesetEquipmentCategoryDefinition> &equipmentCategories,
                                std::vector<RulesetSpellTraditionDefinition> &spellTraditions,
                                std::vector<RulesetMonsterTemplateDefinition> &monsterTemplates)
{
    abilities.clear();
    skills.clear();
    derivedStats.clear();
    defenses.clear();
    resources.clear();
    currencies.clear();
    sizes.clear();
    speeds.clear();
    conditions.clear();
    classes.clear();
    ancestries.clear();
    backgrounds.clear();
    features.clear();
    equipmentCategories.clear();
    spellTraditions.clear();
    monsterTemplates.clear();

    switch (selectedTemplateIndex)
    {
    case 1:
        baseId.clear();
        break;
    case 0:
    default:
        baseId = "dnd-2024-5.5e";
        abilities = {
            {"strength", "Strength", "STR", "Physical power and carrying force."},
            {"dexterity", "Dexterity", "DEX", "Agility, reflexes, and balance."},
            {"constitution", "Constitution", "CON", "Endurance and vitality."},
            {"intelligence", "Intelligence", "INT", "Reasoning and learned knowledge."},
            {"wisdom", "Wisdom", "WIS", "Perception, insight, and will."},
            {"charisma", "Charisma", "CHA", "Presence and force of personality."}};
        skills = {
            {"athletics", "Athletics", "strength", "Physical contests such as climbing, jumping, and grappling."},
            {"perception", "Perception", "wisdom", "Spotting hidden threats and details."}};
        derivedStats = {
            {"armor_class", "Armor Class", "10 + dexterity.mod", "Baseline defense target for attacks."}};
        defenses = {
            {"physical_defense", "Physical Defense", "10 + dexterity.mod + shield", "Generic attack defense preview."}};
        resources = {
            {"heroic_resource", "Heroic Resource", "level + proficiency", "proficiency", "Long Rest", "Example level-scaled resource."}};
        currencies = {
            {"gold", "Gold", "gp", 100, "Primary coin denomination."},
            {"silver", "Silver", "sp", 10, "Secondary coin denomination."},
            {"copper", "Copper", "cp", 1, "Base coin denomination."}};
        sizes = {{"medium", "Medium", 5, "One standard battlefield space."}};
        speeds = {{"walk", "Walking", "feet", "Default tactical movement."}};
        conditions = {{"prone", "Prone", "A creature is on the ground.", {}, "Stand up or otherwise end the effect."}};
        classes = {{"fighter", "Fighter", "d10", {"strength"}, "Martial baseline class example."}};
        ancestries = {{"human", "Human", "medium", "walk", "Flexible ancestry example."}};
        backgrounds = {{"soldier", "Soldier", {"athletics"}, "Military background example."}};
        features = {{"second_wind", "Second Wind", "class", 1, "Example class feature."}};
        equipmentCategories = {{"weapon", "Weapon", "Main Hand", "Offensive equipment category."}};
        spellTraditions = {{"arcane", "Arcane", "intelligence", "Learned magic tradition."}};
        monsterTemplates = {{"skirmisher", "Skirmisher", "medium", "party_level + proficiency", "Mobile combatant template."}};
        break;
    }
}

template <typename T>
void normalizeCommonDefinition(T &value)
{
    value.id = trimCopy(value.id);
    value.name = trimCopy(value.name);
    value.description = trimCopy(value.description);
}

template <typename T>
bool commonDefinitionHasContent(const T &value)
{
    return !value.id.empty() || !value.name.empty() || !value.description.empty();
}

std::string countSummary(std::size_t count, const char *label)
{
    return std::to_string(static_cast<int>(count)) + " " + label;
}

std::string rulesetDraftFingerprint(const RulesetDefinitionModel &definition, int templateIndex)
{
    std::ostringstream fingerprint;
    const auto append = [&](const std::string &value) {
        fingerprint << value.size() << ':' << value << ';';
    };
    append(definition.name);
    append(definition.version);
    append(definition.baseRulesetId);
    append(RulesetDefinitionSerialization::serializeDefinitionContent(definition));
    fingerprint << templateIndex;
    return fingerprint.str();
}

float rulesetFieldWidth(float preferredWidth = 0.0f)
{
    const float availableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    return preferredWidth > 0.0f ? std::min(preferredWidth, availableWidth) : availableWidth;
}

void drawRulesetFieldLabel(const char *label)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUiText(label));
    ImGui::PopStyleColor();
}

bool drawRulesetTextField(const char *id,
                          const char *label,
                          std::string &value,
                          const char *hint = "",
                          float preferredWidth = 0.0f)
{
    drawRulesetFieldLabel(label);
    ImGui::SetNextItemWidth(rulesetFieldWidth(preferredWidth));
    return FormUi::InputTextWithHint(id, localizedUiText(hint), &value);
}

bool drawRulesetIntegerField(const char *id,
                             const char *label,
                             int &value,
                             float preferredWidth = 0.0f)
{
    drawRulesetFieldLabel(label);
    ImGui::SetNextItemWidth(rulesetFieldWidth(preferredWidth));
    return FormUi::InputInt(id, &value);
}

bool drawRulesetMultilineField(const char *id,
                               const char *label,
                               std::string &value,
                               float height = 52.0f)
{
    drawRulesetFieldLabel(label);
    return FormUi::InputTextMultiline(id, &value, ImVec2(rulesetFieldWidth(), height));
}

float rulesetActionWidth(const char *label, float preferredWidth)
{
    const std::string localizedLabel = localizedUiLabel(label);
    return std::max(preferredWidth,
                    ImGui::CalcTextSize(localizedLabel.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f);
}

void continueRulesetActionRow(const char *nextLabel, float preferredWidth)
{
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x + 0.5f < rulesetActionWidth(nextLabel, preferredWidth))
        ImGui::NewLine();
}

std::vector<std::string> splitCommaSeparatedIds(const std::string &value)
{
    std::vector<std::string> results;
    std::stringstream stream(value);
    std::string field;
    while (std::getline(stream, field, ','))
    {
        const std::string trimmed = trimCopy(field);
        if (!trimmed.empty())
            results.push_back(trimmed);
    }
    return results;
}

std::string joinCommaSeparatedIds(const std::vector<std::string> &values)
{
    std::string result;
    for (const std::string &value : values)
    {
        if (!result.empty())
            result += ", ";
        result += value;
    }
    return result;
}
}
