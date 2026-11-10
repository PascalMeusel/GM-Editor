#include "Models/Rulesets/RulesetPresentationFormatter.hpp"

#include <array>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace gm::rulesets
{
namespace
{
bool extractBetween(std::string_view value,
                    std::string_view prefix,
                    std::string_view suffix,
                    std::string &middle)
{
    if (value.size() < prefix.size() + suffix.size() || value.substr(0, prefix.size()) != prefix)
        return false;
    if (!suffix.empty() && value.substr(value.size() - suffix.size()) != suffix)
        return false;
    middle.assign(value.substr(prefix.size(), value.size() - prefix.size() - suffix.size()));
    return !middle.empty();
}

const char *englishDefinitionName(RulesetDefinitionKind kind)
{
    switch (kind)
    {
    case RulesetDefinitionKind::Ability:
        return "ability";
    case RulesetDefinitionKind::DerivedStat:
        return "derived stat";
    case RulesetDefinitionKind::SkillOrCheck:
        return "skill/check";
    case RulesetDefinitionKind::Defense:
        return "defense";
    case RulesetDefinitionKind::Resource:
        return "resource";
    case RulesetDefinitionKind::DiceFormula:
        return "dice formula";
    case RulesetDefinitionKind::Currency:
        return "currency";
    case RulesetDefinitionKind::Size:
        return "size";
    case RulesetDefinitionKind::Speed:
        return "speed";
    case RulesetDefinitionKind::Condition:
        return "condition";
    case RulesetDefinitionKind::Class:
        return "class";
    case RulesetDefinitionKind::Subclass:
        return "subclass";
    case RulesetDefinitionKind::Ancestry:
        return "ancestry";
    case RulesetDefinitionKind::Background:
        return "background";
    case RulesetDefinitionKind::Feature:
        return "feature";
    case RulesetDefinitionKind::EquipmentCategory:
        return "equipment category";
    case RulesetDefinitionKind::SpellTradition:
        return "spell tradition";
    case RulesetDefinitionKind::MonsterTemplate:
        return "monster template";
    case RulesetDefinitionKind::None:
    default:
        return "definition";
    }
}

const char *germanDefinitionName(RulesetDefinitionKind kind)
{
    switch (kind)
    {
    case RulesetDefinitionKind::Ability:
        return "Attribut";
    case RulesetDefinitionKind::DerivedStat:
        return "Abgeleiteter Wert";
    case RulesetDefinitionKind::SkillOrCheck:
        return "Fertigkeit/Probe";
    case RulesetDefinitionKind::Defense:
        return "Verteidigung";
    case RulesetDefinitionKind::Resource:
        return "Ressource";
    case RulesetDefinitionKind::DiceFormula:
        return "Würfelformel";
    case RulesetDefinitionKind::Currency:
        return "Währung";
    case RulesetDefinitionKind::Size:
        return "Größe";
    case RulesetDefinitionKind::Speed:
        return "Bewegungsart";
    case RulesetDefinitionKind::Condition:
        return "Zustand";
    case RulesetDefinitionKind::Class:
        return "Klasse";
    case RulesetDefinitionKind::Subclass:
        return "Unterklasse";
    case RulesetDefinitionKind::Ancestry:
        return "Abstammung";
    case RulesetDefinitionKind::Background:
        return "Hintergrund";
    case RulesetDefinitionKind::Feature:
        return "Merkmal";
    case RulesetDefinitionKind::EquipmentCategory:
        return "Ausrüstungskategorie";
    case RulesetDefinitionKind::SpellTradition:
        return "Magietradition";
    case RulesetDefinitionKind::MonsterTemplate:
        return "Monstervorlage";
    case RulesetDefinitionKind::None:
    default:
        return "Definition";
    }
}

std::optional<RulesetDefinitionKind> definitionKindFromLegacyLabel(std::string_view label)
{
    constexpr std::array<std::pair<std::string_view, RulesetDefinitionKind>, 18> labels{{
        {"ability", RulesetDefinitionKind::Ability},
        {"derived stat", RulesetDefinitionKind::DerivedStat},
        {"skill/check", RulesetDefinitionKind::SkillOrCheck},
        {"defense", RulesetDefinitionKind::Defense},
        {"resource", RulesetDefinitionKind::Resource},
        {"dice formula", RulesetDefinitionKind::DiceFormula},
        {"currency", RulesetDefinitionKind::Currency},
        {"size", RulesetDefinitionKind::Size},
        {"speed", RulesetDefinitionKind::Speed},
        {"condition", RulesetDefinitionKind::Condition},
        {"class", RulesetDefinitionKind::Class},
        {"subclass", RulesetDefinitionKind::Subclass},
        {"ancestry", RulesetDefinitionKind::Ancestry},
        {"background", RulesetDefinitionKind::Background},
        {"feature", RulesetDefinitionKind::Feature},
        {"equipment category", RulesetDefinitionKind::EquipmentCategory},
        {"spell tradition", RulesetDefinitionKind::SpellTradition},
        {"monster template", RulesetDefinitionKind::MonsterTemplate},
    }};
    for (const auto &[candidate, kind] : labels)
    {
        if (candidate == label)
            return kind;
    }
    return std::nullopt;
}
} // namespace

std::string formatUpgradeChange(const RulesetUpgradeChange &change,
                                localization::Locale locale)
{
    const bool german = locale == localization::Locale::German;
    switch (change.kind)
    {
    case RulesetUpgradeChangeKind::Version:
        return "Version: " + change.previousValue + " → " + change.nextValue;
    case RulesetUpgradeChangeKind::Added:
        return german
                   ? std::string(germanDefinitionName(change.definitionKind)) + " hinzugefügt: " + change.stableId
                   : "Added " + std::string(englishDefinitionName(change.definitionKind)) + ": " + change.stableId;
    case RulesetUpgradeChangeKind::Removed:
        return german
                   ? std::string(germanDefinitionName(change.definitionKind)) + " entfernt: " + change.stableId
                   : "Removed " + std::string(englishDefinitionName(change.definitionKind)) + ": " + change.stableId;
    case RulesetUpgradeChangeKind::Changed:
        return german
                   ? std::string(germanDefinitionName(change.definitionKind)) + " geändert: " + change.stableId
                   : "Changed " + std::string(englishDefinitionName(change.definitionKind)) + ": " + change.stableId;
    case RulesetUpgradeChangeKind::AbilityModifierFormula:
        return german ? "Formel für Attributsmodifikatoren geändert." : "Changed ability modifier formula.";
    case RulesetUpgradeChangeKind::ProficiencyFormula:
        return german ? "Formel für die Übungsbonus-Entwicklung geändert." : "Changed proficiency progression formula.";
    case RulesetUpgradeChangeKind::DefaultCheckDiceFormula:
        return german ? "Standard-Würfelformel für Proben geändert." : "Changed default check dice formula.";
    case RulesetUpgradeChangeKind::PassiveScoreBase:
        return german ? "Basiswert für passive Proben geändert." : "Changed passive score base.";
    }
    return {};
}

std::string formatLegacyUpgradeChange(std::string_view message,
                                      localization::Locale locale)
{
    if (locale != localization::Locale::German)
        return std::string(message);

    std::string versionBody;
    if (extractBetween(message, "Version: ", "", versionBody))
    {
        const std::size_t separator = versionBody.find(" -> ");
        if (separator != std::string::npos && separator > 0 && separator + 4U < versionBody.size())
        {
            RulesetUpgradeChange change;
            change.kind = RulesetUpgradeChangeKind::Version;
            change.previousValue = versionBody.substr(0, separator);
            change.nextValue = versionBody.substr(separator + 4U);
            return formatUpgradeChange(change, locale);
        }
    }

    struct Prefix
    {
        std::string_view value;
        RulesetUpgradeChangeKind kind;
    };
    constexpr std::array<Prefix, 3> prefixes{{
        {"Added ", RulesetUpgradeChangeKind::Added},
        {"Removed ", RulesetUpgradeChangeKind::Removed},
        {"Changed ", RulesetUpgradeChangeKind::Changed},
    }};
    for (const Prefix &prefix : prefixes)
    {
        if (message.substr(0, prefix.value.size()) != prefix.value)
            continue;
        const std::string_view body = message.substr(prefix.value.size());
        const std::size_t delimiter = body.find(": ");
        if (delimiter == std::string_view::npos || delimiter == 0 || delimiter + 2U >= body.size())
            continue;
        const std::optional<RulesetDefinitionKind> kind = definitionKindFromLegacyLabel(body.substr(0, delimiter));
        if (!kind)
            continue;
        return formatUpgradeChange(
            RulesetUpgradeChange{prefix.kind, *kind, std::string(body.substr(delimiter + 2U)), {}, {}},
            locale);
    }

    if (message == "Changed ability modifier formula")
        return formatUpgradeChange({RulesetUpgradeChangeKind::AbilityModifierFormula}, locale);
    if (message == "Changed proficiency progression formula")
        return formatUpgradeChange({RulesetUpgradeChangeKind::ProficiencyFormula}, locale);
    if (message == "Changed default check dice formula")
        return formatUpgradeChange({RulesetUpgradeChangeKind::DefaultCheckDiceFormula}, locale);
    if (message == "Changed passive score base")
        return formatUpgradeChange({RulesetUpgradeChangeKind::PassiveScoreBase}, locale);
    return std::string(message);
}

std::string formatFormulaError(std::string_view message,
                               localization::Locale locale)
{
    if (locale != localization::Locale::German)
        return std::string(message);
    if (message == "Formula is empty.")
        return "Die Formel ist leer.";
    if (message == "Division by zero.")
        return "Division durch null.";
    if (message == "Missing closing parenthesis.")
        return "Schließende Klammer fehlt.";

    std::string value;
    if (extractBetween(message, "Unexpected token at position ", ".", value))
        return "Unerwartetes Zeichen an Position " + value + ".";
    if (extractBetween(message, "Expected number, variable, or parenthesized expression at position ", ".", value))
        return "Zahl, Variable oder geklammerter Ausdruck an Position " + value + " erwartet.";
    if (extractBetween(message, "Unknown formula variable: ", ".", value))
        return "Unbekannte Formelvariable: " + value + ".";
    return std::string(message);
}
} // namespace gm::rulesets
