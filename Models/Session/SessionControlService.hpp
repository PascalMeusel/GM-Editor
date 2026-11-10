#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Session/InWorldTimeModel.hpp"

#include <string>

namespace SessionControl
{
SessionModel *activeSession(Campaign &campaign);
const SessionModel *activeSession(const Campaign &campaign);
std::string defaultSessionTitle(const Campaign &campaign);
bool isBreakActive(const SessionModel &session);
bool isLiveSessionBreakActive(const Campaign &campaign);
std::string currentInWorldTime(const SessionModel &session);

// Session-state and clock notes keep their canonical English markers in
// persistence because break detection and old projects depend on them. This
// translates only those known system templates when they are presented.
std::string systemTextForDisplay(const std::string &text);
SessionModel *startLiveSession(Campaign &campaign, const std::string &title = "");
bool endLiveSession(Campaign &campaign, EntityId *endedSessionId = nullptr, std::string *endedSessionTitle = nullptr);
bool startBreak(Campaign &campaign, const std::string &detail = "", const std::string &inWorldTime = "");
bool resumeFromBreak(Campaign &campaign, const std::string &detail = "", const std::string &inWorldTime = "");
bool advanceInWorldTime(Campaign &campaign,
                        const std::string &amount,
                        const std::string &reason,
                        const std::string &fallbackInWorldTime,
                        std::string *newInWorldTime = nullptr);
bool adjustInWorldTime(Campaign &campaign,
                       const std::string &inWorldTime,
                       const std::string &reason,
                       std::string *newInWorldTime = nullptr);
bool setInWorldDateTime(Campaign &campaign,
                        const InWorldDateTime &dateTime,
                        const std::string &reason,
                        std::string *newInWorldTime = nullptr);
}
