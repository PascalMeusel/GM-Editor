#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Session/TabletopToolkit/TabletopToolkitModels.hpp"

#include <random>
#include <string>
#include <vector>

namespace TabletopToolkit
{
bool validDiceMacro(const Campaign &campaign, const CampaignDiceMacroModel &macro, std::string *error = nullptr);
TabletopDiceRollResult roll(Campaign &campaign,
                            const std::string &expression,
                            const std::string &label,
                            bool addToSessionNotes,
                            std::mt19937 &generator);

bool setObjectiveStatus(StoryObjectiveModel &objective, StoryObjectiveStatus status);
bool revealClue(StoryClueModel &clue, bool shareWithPlayers);
bool hideClue(StoryClueModel &clue);

bool requestSafetyPause(Campaign &campaign);
bool clearSafetyPause(Campaign &campaign);
bool startSafetyCheckIn(Campaign &campaign, const std::string &prompt);
bool clearSafetyCheckIn(Campaign &campaign);

std::vector<PlayerSafeObjective> playerSafeObjectives(const Campaign &campaign);
std::vector<PlayerSafeClue> playerSafeClues(const Campaign &campaign);
std::vector<PlayerSafeSafetyBoundary> playerSafeBoundaries(const Campaign &campaign);
std::vector<PlayerSafeContentWarning> playerSafeWarnings(const Campaign &campaign, const EntityId &targetEntityId = {});
} // namespace TabletopToolkit
