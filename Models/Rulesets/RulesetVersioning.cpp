#include "RulesetVersioning.hpp"

#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Models/Rulesets/RulesetDefinitionLayering.hpp"
#include "Models/Rulesets/RulesetDefinitionValidator.hpp"
#include "Models/Rulesets/RulesetResolution.hpp"

#include <algorithm>
#include <set>
#include <utility>

namespace
{
template <typename T>
std::set<std::string> idsOf(const std::vector<T> &values)
{
    std::set<std::string> ids;
    for (const T &value : values)
        ids.insert(value.id);
    return ids;
}

void appendChange(RulesetUpgradePreview &preview,
                  RulesetUpgradeChange change,
                  std::string legacyText)
{
    preview.structuredChanges.push_back(std::move(change));
    preview.changes.push_back(std::move(legacyText));
}

void compareIds(const char *label,
                RulesetDefinitionKind definitionKind,
                const std::set<std::string> &current,
                const std::set<std::string> &next,
                RulesetUpgradePreview &preview)
{
    for (const std::string &id : current)
    {
        if (next.find(id) == next.end())
        {
            appendChange(preview,
                         RulesetUpgradeChange{RulesetUpgradeChangeKind::Removed, definitionKind, id},
                         std::string("Removed ") + label + ": " + id);
            preview.affectedStableIds.push_back(id);
        }
    }
    for (const std::string &id : next)
    {
        if (current.find(id) == current.end())
        {
            appendChange(preview,
                         RulesetUpgradeChange{RulesetUpgradeChangeKind::Added, definitionKind, id},
                         std::string("Added ") + label + ": " + id);
            preview.affectedStableIds.push_back(id);
        }
    }
}

template <typename T, typename Fingerprint>
void compareChanged(const char *label,
                    RulesetDefinitionKind definitionKind,
                    const std::vector<T> &current,
                    const std::vector<T> &next,
                    Fingerprint fingerprint,
                    RulesetUpgradePreview &preview)
{
    for (const T &before : current)
    {
        const auto after = std::find_if(next.begin(), next.end(), [&](const T &candidate) { return candidate.id == before.id; });
        if (after == next.end() || fingerprint(before) == fingerprint(*after))
            continue;
        appendChange(preview,
                     RulesetUpgradeChange{RulesetUpgradeChangeKind::Changed, definitionKind, before.id},
                     std::string("Changed ") + label + ": " + before.id);
        preview.affectedStableIds.push_back(before.id);
    }
}

std::string joined(const std::vector<std::string> &values)
{
    std::string result;
    for (const std::string &value : values)
        result += value + "\n";
    return result;
}
}

RulesetPublishResult publishRulesetVersion(const RulesetDefinitionModel &draft,
                                           const std::string &publishedAtUtc)
{
    RulesetPublishResult result;
    result.version = draft;
    if (result.version.rulesetId.empty())
        result.errors.push_back("Ruleset ID is required.");
    if (result.version.version.empty())
        result.errors.push_back("Version is required.");

    for (const RulesetValidationDiagnostic &diagnostic : validateRulesetDefinition(result.version))
    {
        if (diagnostic.severity == RulesetDiagnosticSeverity::Error)
        {
            result.validationDiagnostics.push_back(diagnostic);
            result.errors.push_back(diagnostic.path + ": " + diagnostic.message);
        }
    }
    if (!result.errors.empty())
        return result;

    result.version.published = true;
    result.version.publishedAtUtc = publishedAtUtc;
    result.version.metadata.id = result.version.rulesetId + "@" + result.version.version;
    result.success = true;
    return result;
}

RulesetUpgradePreview createRulesetUpgradePreview(const RulesetDefinitionModel &current,
                                                  const RulesetDefinitionModel &next,
                                                  const RulesetDefinitionModel *overlay)
{
    RulesetUpgradePreview preview;
    const RulesetDefinitionModel effectiveCurrent = overlay ? createLayeredRulesetDefinition(current, *overlay) : current;
    const RulesetDefinitionModel effectiveNext = overlay ? createLayeredRulesetDefinition(next, *overlay) : next;
    if (overlay)
    {
        preview.overlayApplied = true;
        preview.overlayId = overlay->rulesetId.empty() ? overlay->metadata.id : overlay->rulesetId;
        preview.overlayVersion = overlay->version;
    }
    if (current.version != next.version)
        appendChange(preview,
                     RulesetUpgradeChange{RulesetUpgradeChangeKind::Version, RulesetDefinitionKind::None, {}, current.version, next.version},
                     "Version: " + current.version + " -> " + next.version);
    compareIds("ability", RulesetDefinitionKind::Ability, idsOf(effectiveCurrent.abilityScores), idsOf(effectiveNext.abilityScores), preview);
    compareIds("derived stat", RulesetDefinitionKind::DerivedStat, idsOf(effectiveCurrent.derivedStats), idsOf(effectiveNext.derivedStats), preview);
    compareIds("skill/check", RulesetDefinitionKind::SkillOrCheck, idsOf(effectiveCurrent.skills), idsOf(effectiveNext.skills), preview);
    compareIds("defense", RulesetDefinitionKind::Defense, idsOf(effectiveCurrent.defenses), idsOf(effectiveNext.defenses), preview);
    compareIds("resource", RulesetDefinitionKind::Resource, idsOf(effectiveCurrent.resources), idsOf(effectiveNext.resources), preview);
    compareIds("dice formula", RulesetDefinitionKind::DiceFormula, idsOf(effectiveCurrent.diceFormulas), idsOf(effectiveNext.diceFormulas), preview);
    compareIds("currency", RulesetDefinitionKind::Currency, idsOf(effectiveCurrent.currencies), idsOf(effectiveNext.currencies), preview);
    compareIds("size", RulesetDefinitionKind::Size, idsOf(effectiveCurrent.sizes), idsOf(effectiveNext.sizes), preview);
    compareIds("speed", RulesetDefinitionKind::Speed, idsOf(effectiveCurrent.speeds), idsOf(effectiveNext.speeds), preview);
    compareIds("condition", RulesetDefinitionKind::Condition, idsOf(effectiveCurrent.conditions), idsOf(effectiveNext.conditions), preview);
    compareIds("class", RulesetDefinitionKind::Class, idsOf(effectiveCurrent.classes), idsOf(effectiveNext.classes), preview);
    compareIds("subclass", RulesetDefinitionKind::Subclass, idsOf(effectiveCurrent.subclasses), idsOf(effectiveNext.subclasses), preview);
    compareIds("ancestry", RulesetDefinitionKind::Ancestry, idsOf(effectiveCurrent.ancestries), idsOf(effectiveNext.ancestries), preview);
    compareIds("background", RulesetDefinitionKind::Background, idsOf(effectiveCurrent.backgrounds), idsOf(effectiveNext.backgrounds), preview);
    compareIds("feature", RulesetDefinitionKind::Feature, idsOf(effectiveCurrent.features), idsOf(effectiveNext.features), preview);
    compareIds("equipment category", RulesetDefinitionKind::EquipmentCategory, idsOf(effectiveCurrent.equipmentCategories), idsOf(effectiveNext.equipmentCategories), preview);
    compareIds("spell tradition", RulesetDefinitionKind::SpellTradition, idsOf(effectiveCurrent.spellTraditions), idsOf(effectiveNext.spellTraditions), preview);
    compareIds("monster template", RulesetDefinitionKind::MonsterTemplate, idsOf(effectiveCurrent.monsterTemplates), idsOf(effectiveNext.monsterTemplates), preview);

    if (effectiveCurrent.abilityModifierFormula != effectiveNext.abilityModifierFormula)
        appendChange(preview, {RulesetUpgradeChangeKind::AbilityModifierFormula}, "Changed ability modifier formula");
    if (effectiveCurrent.proficiencyFormula != effectiveNext.proficiencyFormula)
        appendChange(preview, {RulesetUpgradeChangeKind::ProficiencyFormula}, "Changed proficiency progression formula");
    if (effectiveCurrent.checkDiceFormula != effectiveNext.checkDiceFormula)
        appendChange(preview, {RulesetUpgradeChangeKind::DefaultCheckDiceFormula}, "Changed default check dice formula");
    if (effectiveCurrent.passiveScoreBase != effectiveNext.passiveScoreBase)
        appendChange(preview, {RulesetUpgradeChangeKind::PassiveScoreBase}, "Changed passive score base");

    compareChanged("derived stat", RulesetDefinitionKind::DerivedStat, effectiveCurrent.derivedStats, effectiveNext.derivedStats,
                   [](const RulesetDerivedStatDefinition &value) { return value.name + "\n" + value.formula + "\n" + value.description; }, preview);
    compareChanged("ability", RulesetDefinitionKind::Ability, effectiveCurrent.abilityScores, effectiveNext.abilityScores,
                   [](const RulesetAbilityScoreDefinition &value) { return value.name + "\n" + value.abbreviation + "\n" + value.description; }, preview);
    compareChanged("skill/check", RulesetDefinitionKind::SkillOrCheck, effectiveCurrent.skills, effectiveNext.skills,
                   [](const RulesetSkillDefinition &value) { return value.name + "\n" + value.abilityScoreId + "\n" + value.description; }, preview);
    compareChanged("defense", RulesetDefinitionKind::Defense, effectiveCurrent.defenses, effectiveNext.defenses,
                   [](const RulesetDefenseDefinition &value) { return value.name + "\n" + value.formula + "\n" + value.description; }, preview);
    compareChanged("resource", RulesetDefinitionKind::Resource, effectiveCurrent.resources, effectiveNext.resources,
                   [](const RulesetResourceDefinition &value) { return value.name + "\n" + value.maximumFormula + "\n" + value.recoveryFormula + "\n" + value.recoveryTrigger + "\n" + value.description; }, preview);
    compareChanged("dice formula", RulesetDefinitionKind::DiceFormula, effectiveCurrent.diceFormulas, effectiveNext.diceFormulas,
                   [](const RulesetDiceFormulaDefinition &value) { return value.name + "\n" + value.formula + "\n" + value.description; }, preview);
    compareChanged("currency", RulesetDefinitionKind::Currency, effectiveCurrent.currencies, effectiveNext.currencies,
                   [](const RulesetCurrencyDefinition &value) { return value.name + "\n" + value.code + "\n" + std::to_string(value.baseUnitValue) + "\n" + value.description; }, preview);
    compareChanged("condition", RulesetDefinitionKind::Condition, effectiveCurrent.conditions, effectiveNext.conditions,
                   [](const RulesetConditionDefinition &value) { return value.name + "\n" + value.description + "\n" + joined(value.mechanicalEffects) + value.endCondition; }, preview);
    compareChanged("size", RulesetDefinitionKind::Size, effectiveCurrent.sizes, effectiveNext.sizes,
                   [](const RulesetSizeDefinition &value) { return value.name + "\n" + std::to_string(value.spaceFeet) + "\n" + value.description; }, preview);
    compareChanged("speed", RulesetDefinitionKind::Speed, effectiveCurrent.speeds, effectiveNext.speeds,
                   [](const RulesetSpeedDefinition &value) { return value.name + "\n" + value.unit + "\n" + value.description; }, preview);
    compareChanged("class", RulesetDefinitionKind::Class, effectiveCurrent.classes, effectiveNext.classes,
                   [](const RulesetClassDefinition &value) { return value.name + "\n" + value.hitDie + "\n" + joined(value.primaryAbilityScoreIds) + value.description; }, preview);
    compareChanged("subclass", RulesetDefinitionKind::Subclass, effectiveCurrent.subclasses, effectiveNext.subclasses,
                   [](const RulesetSubclassDefinition &value) { return value.name + "\n" + value.parentClassId + "\n" + value.description; }, preview);
    compareChanged("ancestry", RulesetDefinitionKind::Ancestry, effectiveCurrent.ancestries, effectiveNext.ancestries,
                   [](const RulesetAncestryDefinition &value) { return value.name + "\n" + value.sizeId + "\n" + value.speedId + "\n" + value.description; }, preview);
    compareChanged("background", RulesetDefinitionKind::Background, effectiveCurrent.backgrounds, effectiveNext.backgrounds,
                   [](const RulesetBackgroundDefinition &value) { return value.name + "\n" + joined(value.skillIds) + value.description; }, preview);
    compareChanged("feature", RulesetDefinitionKind::Feature, effectiveCurrent.features, effectiveNext.features,
                   [](const RulesetFeatureDefinition &value) { return value.name + "\n" + value.featureType + "\n" + std::to_string(value.level) + "\n" + value.description; }, preview);
    compareChanged("equipment category", RulesetDefinitionKind::EquipmentCategory, effectiveCurrent.equipmentCategories, effectiveNext.equipmentCategories,
                   [](const RulesetEquipmentCategoryDefinition &value) { return value.name + "\n" + value.slotType + "\n" + value.description; }, preview);
    compareChanged("spell tradition", RulesetDefinitionKind::SpellTradition, effectiveCurrent.spellTraditions, effectiveNext.spellTraditions,
                   [](const RulesetSpellTraditionDefinition &value) { return value.name + "\n" + value.abilityScoreId + "\n" + value.description; }, preview);
    compareChanged("monster template", RulesetDefinitionKind::MonsterTemplate, effectiveCurrent.monsterTemplates, effectiveNext.monsterTemplates,
                   [](const RulesetMonsterTemplateDefinition &value) { return value.name + "\n" + value.sizeId + "\n" + value.challengeFormula + "\n" + value.description; }, preview);

    for (const RulesetValidationDiagnostic &diagnostic : validateRulesetDefinition(effectiveNext))
    {
        if (diagnostic.severity != RulesetDiagnosticSeverity::Error)
            continue;
        preview.valid = false;
        preview.validationDiagnostics.push_back(diagnostic);
        preview.errors.push_back(diagnostic.path + ": " + diagnostic.message);
    }
    std::sort(preview.affectedStableIds.begin(), preview.affectedStableIds.end());
    preview.affectedStableIds.erase(std::unique(preview.affectedStableIds.begin(), preview.affectedStableIds.end()),
                                    preview.affectedStableIds.end());
    return preview;
}

bool migrateCampaignToRuleset(Campaign &campaign,
                              const RulesetDefinitionModel &next,
                              const RulesetDefinitionModel *overlay,
                              std::vector<std::string> &errors)
{
    if ((!campaign.rulesetReference.overlayId.empty() || !campaign.rulesetReference.overlayVersion.empty()) &&
        (!overlay || (overlay->rulesetId.empty() ? overlay->metadata.id : overlay->rulesetId) != campaign.rulesetReference.overlayId ||
         (overlay->version != campaign.rulesetReference.overlayVersion &&
          campaign.rulesetReference.overlayId + "@" + overlay->version != campaign.rulesetReference.overlayVersion)))
    {
        errors = {"The pinned overlay is unavailable or mismatched. Restore its exact version before upgrading."};
        return false;
    }
    const RulesetUpgradePreview preview = createRulesetUpgradePreview(
        dynamic_cast<const DefinitionBackedRuleSet *>(campaign.ruleSet.get())
            ? dynamic_cast<const DefinitionBackedRuleSet *>(campaign.ruleSet.get())->definition()
            : RulesetDefinitionModel{},
        next,
        overlay);
    if (!preview.valid || !next.published)
    {
        errors = preview.errors;
        if (!next.published)
            errors.push_back("Campaigns can only pin published ruleset versions.");
        return false;
    }

    // All mutation is deferred until validation succeeds, so a failed migration
    // leaves the campaign and its legacy values untouched.
    std::vector<CharacterModel> migratedCharacters = campaign.characters;
    const RulesetDefinitionModel effectiveNext = overlay ? createLayeredRulesetDefinition(next, *overlay) : next;
    for (CharacterModel &character : migratedCharacters)
        migrateLegacyCharacterRuleValues(character, effectiveNext);

    campaign.characters = std::move(migratedCharacters);
    campaign.rulesetReference.rulesetId = next.rulesetId.empty() ? next.metadata.id : next.rulesetId;
    campaign.rulesetReference.version = next.version;
    if (campaign.rulesetReference.overlayId.empty() && campaign.rulesetReference.overlayVersion.empty())
    {
        campaign.rulesetReference.overlayId = overlay ? (overlay->rulesetId.empty() ? overlay->metadata.id : overlay->rulesetId) : std::string{};
        campaign.rulesetReference.overlayVersion = overlay ? overlay->version : std::string{};
    }
    campaign.ruleSet = overlay
                           ? std::static_pointer_cast<IRuleSet>(std::make_shared<DefinitionBackedRuleSet>(next, *overlay))
                           : std::static_pointer_cast<IRuleSet>(std::make_shared<DefinitionBackedRuleSet>(next));
    campaign.rulesetResolution = {true, campaign.rulesetReference, campaign.ruleSet, SpellcastingValues::Reason::None};
    errors.clear();
    return true;
}
