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

std::string localizedReferenceKind(std::string_view english, localization::Locale locale)
{
    if (locale != localization::Locale::German)
        return std::string(english);
    if (english == "ability score")
        return "Attribut";
    if (english == "class")
        return "Klasse";
    if (english == "size")
        return "Größe";
    if (english == "speed")
        return "Bewegungsart";
    if (english == "skill")
        return "Fertigkeit";
    return std::string(english);
}

std::optional<std::string> formatKnownValidationMessage(std::string_view message,
                                                        localization::Locale locale)
{
    if (locale != localization::Locale::German)
        return std::string(message);

    if (message == "Definition is missing an id.")
        return "Der Definition fehlt eine ID.";
    if (message == "Derived stat participates in a circular formula dependency.")
        return "Der abgeleitete Wert ist Teil einer zirkulären Formelabhängigkeit.";
    if (message == "Published versions require a ruleset ID and version.")
        return "Veröffentlichte Versionen benötigen eine Regelwerk-ID und eine Version.";
    if (message == "Choose a recovery trigger.")
        return "Wähle einen Erholungsauslöser.";

    std::string value;
    if (extractBetween(message, "Duplicate id: ", ".", value))
        return "Doppelte ID: " + value + ".";
    if (extractBetween(message, "Unknown formula symbol: ", ".", value))
        return "Unbekanntes Formelsymbol: " + value + ".";
    if (extractBetween(message, "Invalid check dice formula: ", ".", value))
        return "Ungültige Würfelformel für Proben: " + value + ".";
    if (extractBetween(message, "Invalid dice string: ", ".", value))
        return "Ungültige Würfelformel: " + value + ".";

    constexpr std::string_view missingPrefix = "Missing ";
    constexpr std::string_view referenceDelimiter = " reference: ";
    if (message.size() > missingPrefix.size() + 1U &&
        message.substr(0, missingPrefix.size()) == missingPrefix &&
        message.back() == '.')
    {
        const std::string_view body = message.substr(missingPrefix.size(), message.size() - missingPrefix.size() - 1U);
        const std::size_t delimiter = body.find(referenceDelimiter);
        if (delimiter != std::string_view::npos && delimiter > 0 &&
            delimiter + referenceDelimiter.size() < body.size())
        {
            const std::string_view kind = body.substr(0, delimiter);
            const std::string_view reference = body.substr(delimiter + referenceDelimiter.size());
            return "Fehlende Referenz auf " + localizedReferenceKind(kind, locale) + ": " + std::string(reference) + ".";
        }
    }

    const std::string formula = formatFormulaError(message, locale);
    if (formula != message)
        return formula;
    return std::nullopt;
}

std::string argument(const RulesetValidationDiagnostic &diagnostic, std::size_t index)
{
    return index < diagnostic.arguments.size() ? diagnostic.arguments[index] : std::string{};
}

std::vector<std::string_view> splitPath(std::string_view path)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= path.size())
    {
        const std::size_t separator = path.find('.', start);
        parts.push_back(path.substr(start, separator == std::string_view::npos ? path.size() - start : separator - start));
        if (separator == std::string_view::npos)
            break;
        start = separator + 1U;
    }
    return parts;
}

std::string localizedPathToken(std::string_view token)
{
    constexpr std::array<std::pair<std::string_view, std::string_view>, 26> translations{{
        {"runtime", "Laufzeit"},
        {"overview", "Übersicht"},
        {"abilityScores", "Attribute"},
        {"derivedStats", "Abgeleitete Werte"},
        {"skills", "Fertigkeiten"},
        {"defenses", "Verteidigungen"},
        {"resources", "Ressourcen"},
        {"diceFormulas", "Würfelformeln"},
        {"currencies", "Währungen"},
        {"sizes", "Größen"},
        {"speeds", "Bewegungsarten"},
        {"conditions", "Zustände"},
        {"classes", "Klassen"},
        {"subclasses", "Unterklassen"},
        {"ancestries", "Abstammungen"},
        {"backgrounds", "Hintergründe"},
        {"features", "Merkmale"},
        {"equipmentCategories", "Ausrüstungskategorien"},
        {"spellTraditions", "Magietraditionen"},
        {"monsterTemplates", "Monstervorlagen"},
        {"abilityModifierFormula", "Attributsmodifikator-Formel"},
        {"proficiencyFormula", "Übungsbonus-Formel"},
        {"checkDiceFormula", "Standard-Probenformel"},
        {"formula", "Formel"},
        {"maximumFormula", "Maximumsformel"},
        {"recoveryFormula", "Erholungsformel"},
    }};
    for (const auto &[source, target] : translations)
    {
        if (source == token)
            return std::string(target);
    }
    if (token == "recoveryTrigger")
        return "Erholungsauslöser";
    return std::string(token);
}
} // namespace

std::string formatValidationDiagnostic(const RulesetValidationDiagnostic &diagnostic,
                                       localization::Locale locale)
{
    if (locale != localization::Locale::German)
        return diagnostic.message;

    switch (diagnostic.messageKey)
    {
    case RulesetDiagnosticMessageKey::MissingId:
        return "Der Definition fehlt eine ID.";
    case RulesetDiagnosticMessageKey::DuplicateId:
        return "Doppelte ID: " + argument(diagnostic, 0) + ".";
    case RulesetDiagnosticMessageKey::MissingReference:
        return "Fehlende Referenz auf " + localizedReferenceKind(argument(diagnostic, 0), locale) + ": " + argument(diagnostic, 1) + ".";
    case RulesetDiagnosticMessageKey::UnknownFormulaSymbol:
        return "Unbekanntes Formelsymbol: " + argument(diagnostic, 0) + ".";
    case RulesetDiagnosticMessageKey::BrokenFormula:
        return formatFormulaError(argument(diagnostic, 0).empty() ? diagnostic.message : argument(diagnostic, 0), locale);
    case RulesetDiagnosticMessageKey::CircularDependency:
        return "Der abgeleitete Wert ist Teil einer zirkulären Formelabhängigkeit.";
    case RulesetDiagnosticMessageKey::InvalidCheckDiceFormula:
        return "Ungültige Würfelformel für Proben: " + argument(diagnostic, 0) + ".";
    case RulesetDiagnosticMessageKey::MissingPublishIdentity:
        return "Veröffentlichte Versionen benötigen eine Regelwerk-ID und eine Version.";
    case RulesetDiagnosticMessageKey::MissingRecoveryTrigger:
        return "Wähle einen Erholungsauslöser.";
    case RulesetDiagnosticMessageKey::InvalidDiceString:
        return "Ungültige Würfelformel: " + argument(diagnostic, 0) + ".";
    case RulesetDiagnosticMessageKey::Legacy:
    default:
        break;
    }

    const std::optional<std::string> known = formatKnownValidationMessage(diagnostic.message, locale);
    return known ? *known : diagnostic.message;
}

std::string formatDiagnosticPath(std::string_view path,
                                 localization::Locale locale)
{
    if (locale != localization::Locale::German || path.empty())
        return std::string(path);

    std::ostringstream output;
    const std::vector<std::string_view> parts = splitPath(path);
    for (std::size_t index = 0; index < parts.size(); ++index)
    {
        if (index > 0)
            output << " › ";
        output << localizedPathToken(parts[index]);
    }
    return output.str();
}

std::string formatDiagnosticCode(std::string_view code,
                                 localization::Locale locale)
{
    const bool german = locale == localization::Locale::German;
    if (code == "missing_id")
        return german ? "Fehlende ID" : "Missing ID";
    if (code == "duplicate_id")
        return german ? "Doppelte ID" : "Duplicate ID";
    if (code == "missing_reference")
        return german ? "Fehlende Referenz" : "Missing reference";
    if (code == "broken_formula")
        return german ? "Fehlerhafte Formel" : "Broken formula";
    if (code == "circular_dependency")
        return german ? "Zirkuläre Abhängigkeit" : "Circular dependency";
    if (code == "invalid_dice")
        return german ? "Ungültige Würfelformel" : "Invalid dice formula";
    if (code == "missing_publish_identity")
        return german ? "Fehlende Veröffentlichungsdaten" : "Missing publish identity";
    if (code == "missing_recovery_trigger")
        return german ? "Fehlender Erholungsauslöser" : "Missing recovery trigger";
    return std::string(code);
}

std::string formatBackendError(std::string_view message,
                               localization::Locale locale)
{
    if (locale != localization::Locale::German)
        return std::string(message);
    if (message == "Ruleset ID is required.")
        return "Eine Regelwerk-ID ist erforderlich.";
    if (message == "Version is required.")
        return "Eine Version ist erforderlich.";
    if (message == "Campaigns can only pin published ruleset versions.")
        return "Kampagnen können nur veröffentlichte Regelwerk-Versionen verwenden.";

    if (const std::optional<std::string> known = formatKnownValidationMessage(message, locale))
        return *known;

    // RulesetVersioning serializes validator failures as "path: message" for
    // backwards-compatible service APIs. Translate only when the complete
    // suffix is one of the validator's known messages.
    const std::size_t delimiter = message.find(": ");
    if (delimiter != std::string_view::npos && delimiter > 0 && delimiter + 2U < message.size())
    {
        const std::string_view path = message.substr(0, delimiter);
        const std::string_view detail = message.substr(delimiter + 2U);
        if (const std::optional<std::string> known = formatKnownValidationMessage(detail, locale))
            return formatDiagnosticPath(path, locale) + ": " + *known;
    }
    return std::string(message);
}
} // namespace gm::rulesets

