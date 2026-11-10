#pragma once
#include "MainMenuRulesetNavigationSupport.hpp"

namespace
{
RulesetDefinitionModel buildRulesetDraftDefinition(const std::string &name,
                                                   const std::string &version,
                                                   const std::string &baseId,
                                                   const std::vector<RulesetAbilityScoreDefinition> &abilities,
                                                   const std::vector<RulesetSkillDefinition> &skills,
                                                   const std::vector<RulesetDerivedStatDefinition> &derivedStats,
                                                   const std::vector<RulesetDefenseDefinition> &defenses,
                                                   const std::vector<RulesetResourceDefinition> &resources,
                                                   const std::vector<RulesetCurrencyDefinition> &currencies,
                                                   const std::vector<RulesetSizeDefinition> &sizes,
                                                   const std::vector<RulesetSpeedDefinition> &speeds,
                                                   const std::vector<RulesetConditionDefinition> &conditions,
                                                   const std::vector<RulesetClassDefinition> &classes,
                                                   const std::vector<RulesetAncestryDefinition> &ancestries,
                                                   const std::vector<RulesetBackgroundDefinition> &backgrounds,
                                                   const std::vector<RulesetFeatureDefinition> &features,
                                                   const std::vector<RulesetEquipmentCategoryDefinition> &equipmentCategories,
                                                   const std::vector<RulesetSpellTraditionDefinition> &spellTraditions,
                                                   const std::vector<RulesetMonsterTemplateDefinition> &monsterTemplates,
                                                   const std::string &existingRulesetId)
{
    RulesetDefinitionModel definition;
    definition.rulesetId = existingRulesetId.empty() ? rulesetIdentifier(name) : existingRulesetId;
    definition.metadata.id = definition.rulesetId + "@draft";
    definition.name = trimCopy(name);
    definition.version = trimCopy(version);
    definition.baseRulesetId = trimCopy(baseId);
    definition.installedGlobally = true;

    for (RulesetAbilityScoreDefinition ability : abilities)
    {
        normalizeCommonDefinition(ability);
        ability.abbreviation = trimCopy(ability.abbreviation);
        if (ability.abbreviation.empty() && !ability.name.empty())
            ability.abbreviation = ability.name.substr(0, std::min<std::size_t>(3, ability.name.size()));
        if (commonDefinitionHasContent(ability) || !ability.abbreviation.empty())
            definition.abilityScores.push_back(ability);
    }

    for (RulesetSkillDefinition skill : skills)
    {
        normalizeCommonDefinition(skill);
        skill.abilityScoreId = trimCopy(skill.abilityScoreId);
        if (commonDefinitionHasContent(skill) || !skill.abilityScoreId.empty())
            definition.skills.push_back(skill);
    }

    for (RulesetDerivedStatDefinition stat : derivedStats)
    {
        normalizeCommonDefinition(stat);
        stat.formula = trimCopy(stat.formula);
        if (commonDefinitionHasContent(stat) || !stat.formula.empty())
            definition.derivedStats.push_back(stat);
    }

    for (RulesetDefenseDefinition defense : defenses)
    {
        normalizeCommonDefinition(defense);
        defense.formula = trimCopy(defense.formula);
        if (commonDefinitionHasContent(defense) || !defense.formula.empty())
            definition.defenses.push_back(defense);
    }

    for (RulesetResourceDefinition resource : resources)
    {
        normalizeCommonDefinition(resource);
        resource.maximumFormula = trimCopy(resource.maximumFormula);
        resource.recoveryFormula = trimCopy(resource.recoveryFormula);
        if (commonDefinitionHasContent(resource) || !resource.maximumFormula.empty() || !resource.recoveryFormula.empty())
            definition.resources.push_back(resource);
    }

    for (RulesetCurrencyDefinition currency : currencies)
    {
        normalizeCommonDefinition(currency);
        currency.code = trimCopy(currency.code);
        if (commonDefinitionHasContent(currency) || !currency.code.empty())
            definition.currencies.push_back(currency);
    }

    for (RulesetSizeDefinition size : sizes)
    {
        normalizeCommonDefinition(size);
        if (commonDefinitionHasContent(size))
            definition.sizes.push_back(size);
    }

    for (RulesetSpeedDefinition speed : speeds)
    {
        normalizeCommonDefinition(speed);
        speed.unit = trimCopy(speed.unit);
        if (commonDefinitionHasContent(speed) || !speed.unit.empty())
            definition.speeds.push_back(speed);
    }

    for (RulesetConditionDefinition condition : conditions)
    {
        normalizeCommonDefinition(condition);
        condition.endCondition = trimCopy(condition.endCondition);
        if (commonDefinitionHasContent(condition) || !condition.endCondition.empty() || !condition.mechanicalEffects.empty())
            definition.conditions.push_back(condition);
    }

    for (RulesetClassDefinition classDefinition : classes)
    {
        normalizeCommonDefinition(classDefinition);
        classDefinition.hitDie = trimCopy(classDefinition.hitDie);
        if (commonDefinitionHasContent(classDefinition) || !classDefinition.hitDie.empty() || !classDefinition.primaryAbilityScoreIds.empty())
            definition.classes.push_back(classDefinition);
    }

    for (RulesetAncestryDefinition ancestry : ancestries)
    {
        normalizeCommonDefinition(ancestry);
        ancestry.sizeId = trimCopy(ancestry.sizeId);
        ancestry.speedId = trimCopy(ancestry.speedId);
        if (commonDefinitionHasContent(ancestry) || !ancestry.sizeId.empty() || !ancestry.speedId.empty())
            definition.ancestries.push_back(ancestry);
    }

    for (RulesetBackgroundDefinition background : backgrounds)
    {
        normalizeCommonDefinition(background);
        if (commonDefinitionHasContent(background) || !background.skillIds.empty())
            definition.backgrounds.push_back(background);
    }

    for (RulesetFeatureDefinition feature : features)
    {
        normalizeCommonDefinition(feature);
        feature.featureType = trimCopy(feature.featureType);
        if (commonDefinitionHasContent(feature) || !feature.featureType.empty())
            definition.features.push_back(feature);
    }

    for (RulesetEquipmentCategoryDefinition category : equipmentCategories)
    {
        normalizeCommonDefinition(category);
        category.slotType = trimCopy(category.slotType);
        if (commonDefinitionHasContent(category) || !category.slotType.empty())
            definition.equipmentCategories.push_back(category);
    }

    for (RulesetSpellTraditionDefinition tradition : spellTraditions)
    {
        normalizeCommonDefinition(tradition);
        tradition.abilityScoreId = trimCopy(tradition.abilityScoreId);
        if (commonDefinitionHasContent(tradition) || !tradition.abilityScoreId.empty())
            definition.spellTraditions.push_back(tradition);
    }

    for (RulesetMonsterTemplateDefinition monsterTemplate : monsterTemplates)
    {
        normalizeCommonDefinition(monsterTemplate);
        monsterTemplate.sizeId = trimCopy(monsterTemplate.sizeId);
        monsterTemplate.challengeFormula = trimCopy(monsterTemplate.challengeFormula);
        if (commonDefinitionHasContent(monsterTemplate) || !monsterTemplate.sizeId.empty() || !monsterTemplate.challengeFormula.empty())
            definition.monsterTemplates.push_back(monsterTemplate);
    }

    return definition;
}

void drawRulesetLabProgress(RulesetLabStep currentStep)
{
    float minimumStepWidth = 0.0f;
    for (int index = 0; index < kRulesetLabStepCount; ++index)
    {
        const std::string label = std::to_string(index + 1) + ". " + rulesetLabStepLabel(rulesetLabStepFromIndex(index));
        minimumStepWidth = std::max(minimumStepWidth, ImGui::CalcTextSize(label.c_str()).x + ImGui::GetStyle().CellPadding.x * 2.0f);
    }
    const int progressColumns = gm::ux::responsiveColumnCount(
        ImGui::GetContentRegionAvail().x,
        ImGui::GetStyle().ItemSpacing.x,
        std::max(150.0f, minimumStepWidth),
        kRulesetLabStepCount);

    if (ImGui::BeginTable("RulesetLabProgress", progressColumns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        for (int index = 0; index < kRulesetLabStepCount; ++index)
        {
            ImGui::TableNextColumn();
            const RulesetLabStep step = rulesetLabStepFromIndex(index);
            const bool active = index == rulesetLabStepIndex(currentStep);
            if (active)
                ImGui::TextColored(ImVec4(0.45f, 0.82f, 0.78f, 1.0f), localizedUiLabel("%d. %s").c_str(), index + 1, rulesetLabStepLabel(step));
            else
                ImGui::TextDisabled(localizedUiLabel("%d. %s").c_str(), index + 1, rulesetLabStepLabel(step));
        }
        ImGui::EndTable();
    }
}

bool pathStartsWith(const std::string &path, const char *prefix)
{
    return path.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

RulesetLabStep rulesetLabStepForDiagnosticPath(const std::string &path)
{
    if (pathStartsWith(path, "derivedStats") ||
        pathStartsWith(path, "defenses") ||
        pathStartsWith(path, "resources") ||
        pathStartsWith(path, "monsterTemplates"))
        return RulesetLabStep::DerivedStats;
    if (pathStartsWith(path, "abilityScores") ||
        pathStartsWith(path, "skills") ||
        pathStartsWith(path, "currencies") ||
        pathStartsWith(path, "sizes") ||
        pathStartsWith(path, "speeds") ||
        pathStartsWith(path, "conditions") ||
        pathStartsWith(path, "classes") ||
        pathStartsWith(path, "ancestries") ||
        pathStartsWith(path, "backgrounds") ||
        pathStartsWith(path, "features") ||
        pathStartsWith(path, "equipmentCategories") ||
        pathStartsWith(path, "spellTraditions"))
        return RulesetLabStep::StarterTerms;
    if (pathStartsWith(path, "ruleset") || pathStartsWith(path, "name"))
        return RulesetLabStep::Basics;
    return RulesetLabStep::AdvancedIds;
}

template <typename T>
int definitionIndexForDiagnosticPath(const std::string &path, const char *prefix, const std::vector<T> &definitions)
{
    if (!pathStartsWith(path, prefix))
        return -1;

    const std::size_t idStart = std::string(prefix).size();
    const std::size_t idEnd = path.find('.', idStart);
    const std::string id = path.substr(idStart, idEnd == std::string::npos ? std::string::npos : idEnd - idStart);
    for (int index = 0; index < static_cast<int>(definitions.size()); ++index)
    {
        if (definitions[static_cast<std::size_t>(index)].id == id)
            return index;
    }
    return -1;
}

void drawRulesetFormulaPreview(const RulesetDefinitionModel &definition, const std::string &formula)
{
    ImGui::SeparatorText(localizedUiLabel("Formula Preview").c_str());
    if (trimCopy(formula).empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No formula entered yet.").c_str());
        return;
    }

    const RulesetFormulaEvaluationResult preview = previewRulesetFormula(definition, formula);
    if (preview.valid)
    {
        ImGui::TextColored(ImVec4(0.45f, 0.82f, 0.78f, 1.0f), localizedUiLabel("Example value: %.2f").c_str(), preview.value);
        ImGui::TextDisabled(localizedUiLabel("Preview variables: level=5, proficiency=3, ability scores=10, ability modifiers=0.").c_str());
        return;
    }

    for (const std::string &error : preview.errors)
    {
        const std::string localizedError = gm::rulesets::formatFormulaError(
            error, gm::localization::LocalizationService::instance().locale());
        ImGui::BulletText(localizedUiLabel("%s").c_str(), localizedError.c_str());
    }
}
}
