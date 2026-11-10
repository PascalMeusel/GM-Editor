#pragma once

#include "Models/Localization/Locale.hpp"
#include "Models/Localization/TranslationCatalogTextIds.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace gm::localization
{
std::string_view translationKey(TextId id);
std::string_view translation(Locale locale, TextId id);
bool catalogIsComplete(Locale locale);

// Compatibility widgets still use literal labels while they are gradually
// consolidated into the stable workspaces. This bounded catalog translates
// application-owned chrome without rewriting persisted/user-authored content.
// Unknown literals intentionally pass through unchanged.
std::string_view uiLiteralTranslation(Locale locale, std::string_view english);
// System error text is never user-authored. German callers use this owned
// variant so dynamic suffixes can be retained and unknown English diagnostics
// fail closed to a localized message instead of leaking into the UI.
std::string uiSystemMessageTranslation(Locale locale, std::string_view english);
bool uiLiteralCatalogIsComplete();
std::size_t uiLiteralCatalogSize();
} // namespace gm::localization