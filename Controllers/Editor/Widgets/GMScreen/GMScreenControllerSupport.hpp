#pragma once

#include "GMScreenController.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/NativeAssetPicker.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Session/PlayerSafeDataFilter.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Localization/LocalizationService.hpp"

#include <algorithm>
#include <utility>
namespace
{
SessionPlan::Persist gmSessionPersist(const char *reason)
{ return [reason](Campaign &value) { return ProjectSaveService::getInstance().saveCampaign(value, reason); }; }
bool gmSessionResult(const SessionPlan::Result &result, std::string &message)
{
    message.clear();
    if (result.applied()) return true;
    message = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
        ? "Die Aktion wurde nicht gespeichert. Prüfe den Sitzungsstand und versuche es erneut." : result.message;
    return false;
}
}
