#pragma once

#include <iostream>
#include <utility>
#include "EditorWindowController.hpp"
#include "EditorWorldHistory.hpp"
#include "EditorMarkdownExchange.hpp"
#include "Controllers/Editor/EditorDraftRecoveryController.hpp"
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Audio/AudioMixService.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Project/CampaignBookmarkService.hpp"
#include "Models/Project/CampaignCollectionService.hpp"
#include "Models/Project/EntityRegistry.hpp"
#include "Models/Project/CampaignTagService.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Models/Rulesets/RulesetStudioLibrary.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Session/PlayerSafeDataFilter.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/Story/SceneLaunchService.hpp"
#include "Models/UX/LegacySurfaceRegistry.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include "Views/Editor/EditorWindowView.hpp"
#include "imgui.h"
#include <algorithm>
#include <random>

namespace
{
std::string uiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}
}
