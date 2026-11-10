#pragma once



#include "Models/Rulesets/DND/DND5e.hpp"



inline int DND5e::getExperienceForChallengeRating(const std::string &challengeRating) const
{
        static const std::map<std::string, int> xpByChallengeRating{
            {"0", 10}, {"1/8", 25}, {"1/4", 50}, {"1/2", 100}, {"1", 200}, {"2", 450}, {"3", 700}, {"4", 1100}, {"5", 1800},
            {"6", 2300}, {"7", 2900}, {"8", 3900}, {"9", 5000}, {"10", 5900}, {"11", 7200}, {"12", 8400}, {"13", 10000},
            {"14", 11500}, {"15", 13000}, {"16", 15000}, {"17", 18000}, {"18", 20000}, {"19", 22000}, {"20", 25000},
            {"21", 33000}, {"22", 41000}, {"23", 50000}, {"24", 62000}, {"25", 75000}, {"26", 90000}, {"27", 105000},
            {"28", 120000}, {"29", 135000}, {"30", 155000}};
        const auto found = xpByChallengeRating.find(challengeRating);
        return found == xpByChallengeRating.end() ? 0 : found->second;
    }

inline RuleEncounterDifficultySummary DND5e::summarizeEncounterDifficulty(const std::vector<int> &partyLevels, const std::vector<std::string> &monsterChallengeRatings) const
{
        static const std::map<int, std::array<int, 4>> thresholdsByLevel{
            {1, {25, 50, 75, 100}},       {2, {50, 100, 150, 200}},     {3, {75, 150, 225, 400}},
            {4, {125, 250, 375, 500}},    {5, {250, 500, 750, 1100}},   {6, {300, 600, 900, 1400}},
            {7, {350, 750, 1100, 1700}},  {8, {450, 900, 1400, 2100}},  {9, {550, 1100, 1600, 2400}},
            {10, {600, 1200, 1900, 2800}}, {11, {800, 1600, 2400, 3600}}, {12, {1000, 2000, 3000, 4500}},
            {13, {1100, 2200, 3400, 5100}}, {14, {1250, 2500, 3800, 5700}}, {15, {1400, 2800, 4300, 6400}},
            {16, {1600, 3200, 4800, 7200}}, {17, {2000, 3900, 5900, 8800}}, {18, {2100, 4200, 6300, 9500}},
            {19, {2400, 4900, 7300, 10900}}, {20, {2800, 5700, 8500, 12700}}};

        RuleEncounterDifficultySummary summary;
        summary.partySize = static_cast<int>(partyLevels.size());
        for (int level : partyLevels)
        {
            const int clampedLevel = std::clamp(level, 1, 20);
            summary.partyLevelTotal += clampedLevel;
            const auto thresholds = thresholdsByLevel.at(clampedLevel);
            summary.easyThreshold += thresholds[0];
            summary.mediumThreshold += thresholds[1];
            summary.hardThreshold += thresholds[2];
            summary.deadlyThreshold += thresholds[3];
        }
        for (const std::string &challengeRating : monsterChallengeRatings)
            summary.adjustedExperience += getExperienceForChallengeRating(challengeRating);

        if (summary.adjustedExperience >= summary.deadlyThreshold && summary.deadlyThreshold > 0)
            summary.difficulty = "Deadly";
        else if (summary.adjustedExperience >= summary.hardThreshold && summary.hardThreshold > 0)
            summary.difficulty = "Hard";
        else if (summary.adjustedExperience >= summary.mediumThreshold && summary.mediumThreshold > 0)
            summary.difficulty = "Medium";
        else if (summary.adjustedExperience >= summary.easyThreshold && summary.easyThreshold > 0)
            summary.difficulty = "Easy";
        else
            summary.difficulty = "Trivial";
        return summary;
    }

inline RuleTreasureParcel DND5e::generateTreasureParcel(const RuleLootGenerationRequest &request) const
{
        RuleTreasureParcel parcel = IRuleSet::generateTreasureParcel(request);
        if (!request.includeCoins || parcel.totalCoinValueCopper() > 0)
        {
            parcel.notes.push_back("Treasure uses GM-authored items only; no official random treasure table text is bundled.");
            return parcel;
        }

        int budgetCopper = request.targetValueCopper;
        if (budgetCopper <= 0)
        {
            int experienceBudget = 0;
            for (const std::string &challengeRating : request.sourceChallengeRatings)
                experienceBudget += getExperienceForChallengeRating(challengeRating);
            for (int level : request.partyLevels)
                experienceBudget += std::clamp(level, 1, 20) * 25;
            budgetCopper = std::max(0, experienceBudget * 10);
        }

        parcel.platinumPieces = budgetCopper / 1000;
        budgetCopper %= 1000;
        parcel.goldPieces = budgetCopper / 100;
        budgetCopper %= 100;
        parcel.silverPieces = budgetCopper / 10;
        parcel.copperPieces = budgetCopper % 10;
        parcel.notes.push_back("Treasure uses GM-authored items only; no official random treasure table text is bundled.");
        if (!request.sourceChallengeRatings.empty())
            parcel.notes.push_back("Coin budget derived from challenge-rating XP as a deterministic planning aid.");
        return parcel;
    }
