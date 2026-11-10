#pragma once
#include "BattleTrackerControllerParticipantSupport.hpp"

namespace
{
std::vector<MonsterActionModel> getCharacterFeatureActions(const CharacterModel &character)
{
    std::vector<MonsterActionModel> actions = character.featureActions;
    for (MonsterActionModel &action : actions)
    {
        if (action.actionKind.empty() || action.actionKind == "Attack")
            action.actionKind = "Feature";
        action.sourceLabel = std::string(character.nameBuffer);
        action.sourceEntityId = character.metadata.id;
    }
    return actions;
}

bool addUniqueCondition(std::vector<std::string> &conditions, const std::string &condition)
{
    if (condition.empty() || std::find(conditions.begin(), conditions.end(), condition) != conditions.end())
        return false;
    conditions.push_back(condition);
    return true;
}

int estimateDurationRounds(const std::string &duration)
{
    const std::string lowerDuration = lowerCopy(duration);
    if (lowerDuration.empty() || lowerDuration == "instantaneous")
        return -1;

    int amount = 1;
    for (size_t i = 0; i < lowerDuration.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(lowerDuration[i])))
            continue;
        size_t cursor = i;
        while (cursor < lowerDuration.size() && std::isdigit(static_cast<unsigned char>(lowerDuration[cursor])))
            ++cursor;
        amount = std::max(1, std::stoi(lowerDuration.substr(i, cursor - i)));
        break;
    }

    if (lowerDuration.find("round") != std::string::npos)
        return amount;
    if (lowerDuration.find("minute") != std::string::npos)
        return amount * 10;
    if (lowerDuration.find("hour") != std::string::npos)
        return amount * 600;
    return -1;
}

ActiveEffectKind effectKindForSpellSummary(const RuleSpellResolutionSummary &summary)
{
    if (!summary.area.shape.empty() || summary.resolution == SpellResolutionType::SummonOrZone)
        return ActiveEffectKind::SummonedZone;
    if (summary.resolution == SpellResolutionType::Healing)
        return ActiveEffectKind::Buff;
    if (summary.resolution == SpellResolutionType::SavingThrow || summary.resolution == SpellResolutionType::SpellAttack)
        return ActiveEffectKind::Debuff;
    return ActiveEffectKind::Buff;
}

bool addOrRefreshActiveEffect(std::vector<ActiveEffectModel> &effects, const ActiveEffectModel &effect)
{
    const auto found = std::find_if(effects.begin(), effects.end(), [&](const ActiveEffectModel &existing) {
        return existing.name == effect.name && existing.sourceEntityId == effect.sourceEntityId && existing.sourceSpellId == effect.sourceSpellId;
    });
    if (found != effects.end())
    {
        found->kind = effect.kind;
        found->sourceName = effect.sourceName;
        found->notes = effect.notes;
        found->remainingRounds = effect.remainingRounds;
        found->concentration = effect.concentration;
        found->active = true;
        found->metadata.touch();
        return false;
    }

    effects.push_back(effect);
    return true;
}

bool rechargeRuleMatchesTurn(const std::string &rule)
{
    const std::string lowerRule = lowerCopy(rule);
    return lowerRule.find("turn") != std::string::npos || lowerRule.find("round") != std::string::npos;
}

int participantThreatRangeFeet(const EncounterParticipantModel &participant)
{
    int range = 5;
    for (const MonsterActionModel &action : participant.actions)
    {
        const std::string kind = lowerCopy(action.actionKind);
        if (kind.find("attack") != std::string::npos && action.rangeFeet <= std::max(10, action.reachFeet))
            range = std::max(range, std::max(action.reachFeet, 5));
    }
    return range;
}

void syncParticipantConditionsToSource(Campaign &campaign, const EncounterParticipantModel &participant)
{
    if (participant.kind == CombatantKind::Player)
    {
        auto characterIt = std::find_if(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        if (characterIt != campaign.characters.end())
        {
            characterIt->conditions = participant.conditions;
            characterIt->metadata.touch();
            campaign.metadata.touch();
        }
    }
    else if (participant.kind == CombatantKind::NPC)
    {
        auto npcIt = std::find_if(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        if (npcIt != campaign.npcs.end())
        {
            npcIt->conditions = participant.conditions;
            npcIt->metadata.touch();
            campaign.metadata.touch();
        }
    }
}

void touchParticipantSpellcaster(Campaign &campaign, const EncounterParticipantModel &participant)
{
    if (participant.kind == CombatantKind::Player)
    {
        auto characterIt = std::find_if(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        if (characterIt != campaign.characters.end())
            characterIt->metadata.touch();
    }
    else if (participant.kind == CombatantKind::NPC)
    {
        auto npcIt = std::find_if(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        if (npcIt != campaign.npcs.end())
            npcIt->metadata.touch();
    }
}

int initiativeModifierForParticipant(const IRuleSet &ruleset, const Campaign &campaign, const EncounterParticipantModel &participant, const std::vector<MonsterDefinitionModel> &monsters)
{
    if (participant.kind == CombatantKind::Player)
    {
        const auto characterIt = std::find_if(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        return characterIt == campaign.characters.end() ? 0 : ruleset.getAbilityModifier(characterIt->attributes, "Dexterity");
    }

    if (participant.kind == CombatantKind::NPC)
    {
        const auto npcIt = std::find_if(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        return npcIt == campaign.npcs.end() ? 0 : ruleset.getAbilityModifier(npcIt->attributes, "Dexterity");
    }

    const auto monsterIt = std::find_if(monsters.begin(), monsters.end(), [&](const MonsterDefinitionModel &monster) {
        return monster.metadata.id == participant.sourceEntityId || monster.metadata.id == participant.monsterDefinitionId;
    });
    return monsterIt == monsters.end() ? 0 : ruleset.getAbilityModifier(monsterIt->attributes, "Dexterity");
}
}
