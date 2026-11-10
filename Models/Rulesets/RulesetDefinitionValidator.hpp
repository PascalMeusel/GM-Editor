#pragma once

#include "Models/Rulesets/RulesetDefinitionModel.hpp"

#include <string>
#include <vector>

enum class RulesetDiagnosticSeverity
{
    Warning,
    Error
};

// Identifies application-owned validator prose without coupling the domain
// model to a display language.  `message` remains the English diagnostic used
// by logs and older callers; UI code renders `messageKey` and `arguments`.
enum class RulesetDiagnosticMessageKey
{
    Legacy,
    MissingId,
    DuplicateId,
    MissingReference,
    UnknownFormulaSymbol,
    BrokenFormula,
    CircularDependency,
    InvalidCheckDiceFormula,
    MissingPublishIdentity,
    MissingRecoveryTrigger,
    InvalidDiceString
};

struct RulesetValidationDiagnostic
{
    RulesetDiagnosticSeverity severity = RulesetDiagnosticSeverity::Error;
    std::string code;
    std::string path;
    std::string message;
    RulesetDiagnosticMessageKey messageKey = RulesetDiagnosticMessageKey::Legacy;
    std::vector<std::string> arguments;
};

std::vector<RulesetValidationDiagnostic> validateRulesetDefinition(const RulesetDefinitionModel &definition);
