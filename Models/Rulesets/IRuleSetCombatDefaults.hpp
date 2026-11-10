#pragma once



#include "Models/Rulesets/IRuleSet.hpp"



inline RuleActionEconomyState IRuleSet::startTurnActionEconomy(int speedFeet, const RuleActionEconomyState& previousState) const
    {
        RuleActionEconomyState state = previousState;
        state.actionAvailable = true;
        state.bonusActionAvailable = true;
        state.freeObjectInteractionAvailable = true;
        state.movementAvailableFeet = std::max(0, speedFeet);
        state.movementUsedFeet = 0;
        return state;
    }

inline RuleActionEconomyState IRuleSet::startRoundActionEconomy(const RuleActionEconomyState& previousState) const
    {
        RuleActionEconomyState state = previousState;
        state.reactionAvailable = true;
        return state;
    }

inline bool IRuleSet::canSpendActionResource(const RuleActionEconomyState& state, RuleActionResource resource, int movementFeet, std::string* reason) const
    {
        bool available = false;
        switch (resource)
        {
        case RuleActionResource::Action:
            available = state.actionAvailable;
            if (!available && reason)
                *reason = "Action already used.";
            break;
        case RuleActionResource::BonusAction:
            available = state.bonusActionAvailable;
            if (!available && reason)
                *reason = "Bonus action already used.";
            break;
        case RuleActionResource::Reaction:
            available = state.reactionAvailable;
            if (!available && reason)
                *reason = "Reaction already used this round.";
            break;
        case RuleActionResource::FreeObjectInteraction:
            available = state.freeObjectInteractionAvailable;
            if (!available && reason)
                *reason = "Free object interaction already used.";
            break;
        case RuleActionResource::Movement:
            available = movementFeet >= 0 && state.movementUsedFeet + movementFeet <= state.movementAvailableFeet;
            if (!available && reason)
                *reason = "Not enough movement remains.";
            break;
        case RuleActionResource::Concentration:
            available = !state.concentrating;
            if (!available && reason)
                *reason = "Already concentrating.";
            break;
        }
        return available;
    }

inline RuleActionSpendResult IRuleSet::spendActionResource(RuleActionEconomyState state, RuleActionResource resource, int movementFeet, const std::string& concentrationSource) const
    {
        RuleActionSpendResult result;
        result.state = state;
        std::string reason;
        if (!canSpendActionResource(state, resource, movementFeet, &reason))
        {
            result.message = reason;
            return result;
        }

        result.success = true;
        switch (resource)
        {
        case RuleActionResource::Action:
            result.state.actionAvailable = false;
            result.message = "Action spent.";
            break;
        case RuleActionResource::BonusAction:
            result.state.bonusActionAvailable = false;
            result.message = "Bonus action spent.";
            break;
        case RuleActionResource::Reaction:
            result.state.reactionAvailable = false;
            result.message = "Reaction spent.";
            break;
        case RuleActionResource::FreeObjectInteraction:
            result.state.freeObjectInteractionAvailable = false;
            result.message = "Free object interaction spent.";
            break;
        case RuleActionResource::Movement:
            result.state.movementUsedFeet += std::max(0, movementFeet);
            result.message = "Movement spent.";
            break;
        case RuleActionResource::Concentration:
            result.state.concentrating = true;
            result.state.concentrationSource = concentrationSource;
            result.message = "Concentration started.";
            break;
        }
        return result;
    }

inline RuleActionEconomyState IRuleSet::endConcentration(RuleActionEconomyState state) const
    {
        state.concentrating = false;
        state.concentrationSource.clear();
        return state;
    }

inline RuleConcentrationCheck IRuleSet::createConcentrationCheck(int damageTaken, int casterLevel, RuleProficiencyTier constitutionSaveProficiency) const
    {
        RuleConcentrationCheck check;
        check.damageTaken = std::max(0, damageTaken);
        check.dc = std::max(10, check.damageTaken / 2);
        check.savingThrow.ability = "Constitution";
        check.savingThrow.level = casterLevel;
        check.savingThrow.proficiency = constitutionSaveProficiency;
        check.savingThrow.dc = check.dc;
        return check;
    }

inline RuleConcentrationResult IRuleSet::resolveConcentrationCheck(const std::map<std::string, int>& attributes, const RuleConcentrationCheck& check, int firstD20, int secondD20, RuleRollMode rollMode) const
    {
        RuleConcentrationResult result;
        result.check = check;
        RuleCheckRequest request = check.savingThrow;
        request.rollMode = rollMode;
        result.roll = resolveCheckRoll(attributes, request, firstD20, secondD20);
        result.concentrationMaintained = result.roll.valid && result.roll.success;
        result.concentrationBroken = result.roll.valid && !result.roll.success;
        return result;
    }

inline RuleRestResult IRuleSet::applyRest(const RuleRestRequest& request) const
    {
        RuleRestResult result;
        result.type = request.type;
        result.currentHitPoints = std::clamp(request.currentHitPoints, 0, std::max(0, request.maxHitPoints));
        result.hitDiceAvailable = std::max(0, request.hitDiceAvailable);
        result.spellcasting = request.spellcasting;
        normalizeSpellcasting(result.spellcasting);

        if (request.type == RuleRestType::LongRest)
        {
            result.currentHitPoints = std::max(0, request.maxHitPoints);
            std::fill(result.spellcasting.slotUsed.begin(), result.spellcasting.slotUsed.end(), 0);
            result.hitDiceAvailable = std::max(result.hitDiceAvailable, request.hitDiceAvailable + std::max(1, request.hitDiceSpent / 2));
            result.recoveredResources.push_back("Hit points");
            result.recoveredResources.push_back("Spell slots");
            result.recoveredResources.push_back("Long-rest features");
        }
        else
        {
            result.recoveredResources.push_back("Short-rest features");
            result.notes.push_back("Spend hit dice manually until hit dice are modeled per character.");
        }

        return result;
    }

inline RuleDamageResult IRuleSet::applyDamageOrHealing(const RuleDamageApplication& request) const
    {
        RuleDamageResult result;
        result.currentHitPoints = std::clamp(request.currentHitPoints, 0, std::max(0, request.maxHitPoints));
        result.temporaryHitPoints = std::max(0, request.temporaryHitPoints);
        const int amount = std::max(0, request.amount);

        if (request.healing)
        {
            result.effectiveAmount = amount;
            result.currentHitPoints = std::clamp(result.currentHitPoints + amount, 0, std::max(0, request.maxHitPoints));
            result.defeated = result.currentHitPoints == 0;
            result.notes.push_back("Healing applied.");
            return result;
        }

        const std::string damageType = request.damageType;
        if (!damageType.empty() && std::find(request.immunities.begin(), request.immunities.end(), damageType) != request.immunities.end())
        {
            result.effectiveAmount = 0;
            result.notes.push_back("Damage immunity applied.");
            return result;
        }

        int effectiveDamage = amount;
        if (!damageType.empty() && std::find(request.resistances.begin(), request.resistances.end(), damageType) != request.resistances.end())
        {
            effectiveDamage /= 2;
            result.notes.push_back("Damage resistance applied.");
        }
        if (!damageType.empty() && std::find(request.vulnerabilities.begin(), request.vulnerabilities.end(), damageType) != request.vulnerabilities.end())
        {
            effectiveDamage *= 2;
            result.notes.push_back("Damage vulnerability applied.");
        }

        const int absorbedByTemporaryHitPoints = std::min(result.temporaryHitPoints, effectiveDamage);
        result.temporaryHitPoints -= absorbedByTemporaryHitPoints;
        effectiveDamage -= absorbedByTemporaryHitPoints;
        result.currentHitPoints = std::clamp(result.currentHitPoints - effectiveDamage, 0, std::max(0, request.maxHitPoints));
        result.effectiveAmount = amount == 0 ? 0 : absorbedByTemporaryHitPoints + effectiveDamage;
        result.defeated = result.currentHitPoints == 0;
        result.massiveDamageRisk = request.currentHitPoints > 0 && result.currentHitPoints == 0 && effectiveDamage >= request.maxHitPoints;
        return result;
    }

inline RuleDeathSaveResult IRuleSet::resolveDeathSave(RuleDeathSaveState state, int d20Roll) const
    {
        RuleDeathSaveResult result;
        result.state = state;
        if (d20Roll < 1 || d20Roll > 20)
        {
            result.message = "Death save must be a d20 roll.";
            return result;
        }

        result.valid = true;
        if (d20Roll == 1)
        {
            result.state.failures += 2;
            result.message = "Natural 1: two death save failures.";
        }
        else if (d20Roll == 20)
        {
            result.state.successes = 0;
            result.state.failures = 0;
            result.state.stable = true;
            result.message = "Natural 20: stabilize and regain 1 HP manually.";
        }
        else if (d20Roll >= 10)
        {
            ++result.state.successes;
            result.message = "Death save success.";
        }
        else
        {
            ++result.state.failures;
            result.message = "Death save failure.";
        }

        if (result.state.successes >= 3)
        {
            result.state.stable = true;
            result.message = "Three successes: stable.";
        }
        if (result.state.failures >= 3)
        {
            result.state.dead = true;
            result.message = "Three failures: dead.";
        }
        return result;
    }

inline int IRuleSet::getExperienceForChallengeRating(const std::string& challengeRating) const
    {
        (void)challengeRating;
        return 0;
    }

inline RuleEncounterDifficultySummary IRuleSet::summarizeEncounterDifficulty(const std::vector<int>& partyLevels, const std::vector<std::string>& monsterChallengeRatings) const
    {
        RuleEncounterDifficultySummary summary;
        summary.partySize = static_cast<int>(partyLevels.size());
        for (int level : partyLevels)
            summary.partyLevelTotal += std::max(1, level);
        for (const std::string& challengeRating : monsterChallengeRatings)
            summary.adjustedExperience += getExperienceForChallengeRating(challengeRating);
        if (summary.adjustedExperience > 0)
            summary.difficulty = "Custom";
        return summary;
    }

inline RuleRewardSummary IRuleSet::summarizeRewards(int experience, bool milestoneAvailable, const std::string& milestoneLabel) const
    {
        RuleRewardSummary summary;
        summary.experience = std::max(0, experience);
        summary.milestoneAvailable = milestoneAvailable;
        summary.milestoneLabel = milestoneLabel;
        if (milestoneAvailable)
            summary.notes.push_back(milestoneLabel.empty() ? "Milestone advancement is available." : "Milestone: " + milestoneLabel);
        if (experience > 0)
            summary.notes.push_back("Award " + std::to_string(experience) + " XP.");
        return summary;
    }

inline RuleTreasureParcel IRuleSet::generateTreasureParcel(const RuleLootGenerationRequest& request) const
    {
        RuleTreasureParcel parcel;
        parcel.label = request.label.empty() ? "Treasure Parcel" : request.label;
        parcel.sourceId = request.sourceId;
        parcel.items = request.items;
        parcel.notes = request.notes;
        if (request.includeCoins && request.targetValueCopper > 0)
        {
            int remaining = request.targetValueCopper;
            parcel.platinumPieces = remaining / 1000;
            remaining %= 1000;
            parcel.goldPieces = remaining / 100;
            remaining %= 100;
            parcel.silverPieces = remaining / 10;
            parcel.copperPieces = remaining % 10;
        }
        return parcel;
    }
