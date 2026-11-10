#pragma once

#include "Models/Audio/AudioPlaybackService.hpp"
#include "Models/Campaign/Campaign.hpp"

#include <memory>
#include <string>

namespace AudioTriggers
{
int playMatchingTriggers(Campaign &campaign,
                         const std::shared_ptr<AudioPlaybackService> &playbackService,
                         const std::string &sourceType,
                         const EntityId &sourceEntityId,
                         const std::string &eventName,
                         std::string &errorMessage);
}
