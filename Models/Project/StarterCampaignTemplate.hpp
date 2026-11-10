#pragma once

#include "Models/Campaign/Campaign.hpp"

namespace StarterCampaignTemplate
{
enum class Language
{
    English,
    German
};

// Adds a small, system-neutral guided workspace. The chosen ruleset and every
// field the user already entered remain untouched.
void apply(Campaign &campaign, Language language);
} // namespace StarterCampaignTemplate
