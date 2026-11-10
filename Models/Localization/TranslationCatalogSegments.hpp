#pragma once
#include "Models/Localization/TranslationCatalogTextIds.hpp"

#include "Models/Localization/TranslationCatalogTextCore.hpp"
#include "Models/Localization/TranslationCatalogTextLiveSettings.hpp"
#include "Models/Localization/TranslationCatalogUiProgressionAtlas.hpp"
#include "Models/Localization/TranslationCatalogUiMapGenerationRecovery.hpp"
#include "Models/Localization/TranslationCatalogUiBattlefieldGeneration.hpp"
#include "Models/Localization/TranslationCatalogUiMapVocabulary.hpp"
#include "Models/Localization/TranslationCatalogUiMapEditor.hpp"
#include "Models/Localization/TranslationCatalogUiGeneratorResults.hpp"
#include "Models/Localization/TranslationCatalogUiAtlas.hpp"
#include "Models/Localization/TranslationCatalogUiTactical.hpp"
#include "Models/Localization/TranslationCatalogUiPlayerDisplay.hpp"
#include "Models/Localization/TranslationCatalogUiCompendiumLive.hpp"
#include "Models/Localization/TranslationCatalogSpecialistCoreEditors.hpp"
#include "Models/Localization/TranslationCatalogSpecialistMapWorld.hpp"
#include "Models/Localization/TranslationCatalogSpecialistCompendiumStory.hpp"
#include "Models/Localization/TranslationCatalogSpecialistLiveDashboard.hpp"
#include "Models/Localization/TranslationCatalogSpecialistBattleSetup.hpp"
#include "Models/Localization/TranslationCatalogSpecialistBattleResolution.hpp"
#include "Models/Localization/TranslationCatalogSpecialistReleaseMapsCast.hpp"
#include "Models/Localization/TranslationCatalogSpecialistActorCharacter.hpp"
#include "Models/Localization/TranslationCatalogSpecialistShellStory.hpp"
#include "Models/Localization/TranslationCatalogSpecialistLiveCompendium.hpp"
#include "Models/Localization/TranslationCatalogSpecialistRulesetSettings.hpp"
#include "Models/Localization/TranslationCatalogSpecialistPortableMaps.hpp"
#include "Models/Localization/TranslationCatalogSpecialistMapValidation.hpp"
#include "Models/Localization/TranslationCatalogSpecialistAtlasSpells.hpp"
#include "Models/Localization/TranslationCatalogSpecialistWorkspaceChrome.hpp"

#include <array>

namespace gm::localization::detail
{
inline constexpr std::array<CatalogSegment<TranslationEntry>, 2> kTranslationSegments = {{
    catalogSegment(TranslationCatalogTextCore),
    catalogSegment(TranslationCatalogTextLiveSettings),
}};

inline constexpr std::array<CatalogSegment<UiLiteralEntry>, 10> kUiLiteralSegments = {{
    catalogSegment(TranslationCatalogUiProgressionAtlas),
    catalogSegment(TranslationCatalogUiMapGenerationRecovery),
    catalogSegment(TranslationCatalogUiBattlefieldGeneration),
    catalogSegment(TranslationCatalogUiMapVocabulary),
    catalogSegment(TranslationCatalogUiMapEditor),
    catalogSegment(TranslationCatalogUiGeneratorResults),
    catalogSegment(TranslationCatalogUiAtlas),
    catalogSegment(TranslationCatalogUiTactical),
    catalogSegment(TranslationCatalogUiPlayerDisplay),
    catalogSegment(TranslationCatalogUiCompendiumLive),
}};

inline constexpr std::array<CatalogSegment<UiLiteralEntry>, 15> kSpecialistUiLiteralSegments = {{
    catalogSegment(TranslationCatalogSpecialistCoreEditors),
    catalogSegment(TranslationCatalogSpecialistMapWorld),
    catalogSegment(TranslationCatalogSpecialistCompendiumStory),
    catalogSegment(TranslationCatalogSpecialistLiveDashboard),
    catalogSegment(TranslationCatalogSpecialistBattleSetup),
    catalogSegment(TranslationCatalogSpecialistBattleResolution),
    catalogSegment(TranslationCatalogSpecialistReleaseMapsCast),
    catalogSegment(TranslationCatalogSpecialistActorCharacter),
    catalogSegment(TranslationCatalogSpecialistShellStory),
    catalogSegment(TranslationCatalogSpecialistLiveCompendium),
    catalogSegment(TranslationCatalogSpecialistRulesetSettings),
    catalogSegment(TranslationCatalogSpecialistPortableMaps),
    catalogSegment(TranslationCatalogSpecialistMapValidation),
    catalogSegment(TranslationCatalogSpecialistAtlasSpells),
    catalogSegment(TranslationCatalogSpecialistWorkspaceChrome),
}};

static_assert(catalogSize(kTranslationSegments) == static_cast<std::size_t>(TextId::Count));
} // namespace gm::localization::detail
