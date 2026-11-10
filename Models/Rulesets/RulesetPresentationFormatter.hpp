#pragma once

#include "Models/Localization/Locale.hpp"
#include "Models/Rulesets/RulesetDefinitionValidator.hpp"
#include "Models/Rulesets/RulesetMessageTypes.hpp"

#include <string>
#include <string_view>

namespace gm::rulesets
{
// Formats structured ruleset output at the UI boundary. Stable IDs and formula
// fragments are deliberately preserved verbatim; only application-owned prose
// is localized.
std::string formatUpgradeChange(const RulesetUpgradeChange &change,
                                localization::Locale locale);

// Compatibility path for upgrade previews created by older data/code. Only
// the finite set of strings emitted by RulesetVersioning is recognized.
// Unknown/user-authored text passes through unchanged.
std::string formatLegacyUpgradeChange(std::string_view message,
                                      localization::Locale locale);

std::string formatFormulaError(std::string_view message,
                               localization::Locale locale);
std::string formatValidationDiagnostic(const RulesetValidationDiagnostic &diagnostic,
                                       localization::Locale locale);
std::string formatDiagnosticCode(std::string_view code,
                                 localization::Locale locale);
std::string formatDiagnosticPath(std::string_view path,
                                 localization::Locale locale);

// Formats the bounded set of errors emitted by publish/migration/versioning,
// including legacy "path: validator message" composites. Unknown details are
// returned unchanged so filesystem/database errors are never misrepresented.
std::string formatBackendError(std::string_view message,
                               localization::Locale locale);
} // namespace gm::rulesets
