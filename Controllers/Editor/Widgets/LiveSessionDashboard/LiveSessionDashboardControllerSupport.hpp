#pragma once

#include "LiveSessionDashboardController.hpp"

#include "Models/Audio/AudioMixService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/Story/SceneLaunchService.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <utility>
#include <vector>

namespace
{
bool dashboardEncounterHasStarted(const EncounterModel *encounter)
{
    return encounter && (encounter->combatPaused ||
                         (encounter->combatPhase != CombatPhase::Setup && encounter->combatPhase != CombatPhase::Initiative) ||
                         encounter->round > 1 || encounter->activeParticipantIndex > 0 || !encounter->combatLog.empty());
}

std::string clockStateKey(const Campaign &campaign)
{
    CampaignClockModel clock = campaign.clock;
    InWorldTime::normalizeClock(clock);

    std::ostringstream key;
    key << campaign.metadata.id << "|"
        << gm::localization::LocalizationService::instance().revision() << "|"
        << clock.useSystemTime << "|"
        << clock.running << "|"
        << clock.multiplier << "|"
        << clock.anchorAbsoluteMinutes << "|"
        << clock.anchorUtc << "|"
        << clock.calendar.name << "|"
        << clock.calendar.hoursPerDay << "|"
        << clock.calendar.daylightHours << "|"
        << clock.calendar.nightHours << "|"
        << clock.calendar.dayStartsAtHour << "|"
        << clock.calendar.months.size() << "|"
        << clock.calendar.weekdays.size();
    for (const CalendarMonthModel &month : clock.calendar.months)
        key << "|" << month.name << ":" << month.days;
    for (const std::string &weekday : clock.calendar.weekdays)
        key << "|" << weekday;
    return key.str();
}
}
