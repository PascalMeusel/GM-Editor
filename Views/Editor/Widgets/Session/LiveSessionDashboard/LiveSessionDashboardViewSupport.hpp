#pragma once

#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "LiveSessionDashboardView.hpp"

#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardController.hpp"
#include "Models/Localization/LocaleFormatting.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

namespace
{
std::string joinStrings(const std::vector<std::string> &values)
{
    std::string result;
    for (const std::string &value : values)
    {
        if (!result.empty())
            result += ", ";
        result += value;
    }
    return result.empty() ? "-" : result;
}

std::string lowerCopy(const std::string &value)
{
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return lowered;
}

std::string formatClockSpeed(double multiplier)
{
    return "×" + gm::localization::formatNumber(
                     multiplier,
                     multiplier < 10.0 ? 2 : 1,
                     gm::localization::LocalizationService::instance().locale());
}

bool noteMatchesSearch(const SessionNoteModel &note, const std::string &query)
{
    if (query.empty())
        return true;

    const std::string loweredQuery = lowerCopy(query);
    return lowerCopy(note.body).find(loweredQuery) != std::string::npos ||
           lowerCopy(SessionControl::systemTextForDisplay(note.body)).find(loweredQuery) != std::string::npos ||
           lowerCopy(note.linkedEntityLabel).find(loweredQuery) != std::string::npos ||
           lowerCopy(note.linkedEntityType).find(loweredQuery) != std::string::npos ||
           lowerCopy(note.timestampUtc).find(loweredQuery) != std::string::npos;
}

std::string noteLinkLabel(const SessionNoteModel &note)
{
    if (note.linkedEntityType == kSessionClockNoteLinkType && !note.linkedEntityLabel.empty())
        return std::string(gm::localization::trUi("Clock")) + ": " + InWorldTime::localizeDateTimeText(note.linkedEntityLabel);
    return note.linkedEntityLabel.empty() ? "-" : note.linkedEntityLabel;
}

std::string localizedRegistryFallback(const std::string &displayName)
{
    static constexpr const char *kSystemFallbacks[] = {
        "Campaign", "Unnamed character", "Unnamed NPC", "Unnamed faction", "Unnamed deity",
        "Unnamed nation", "Unnamed territory", "Unnamed place", "Unnamed item", "Unnamed spell",
        "Unnamed monster", "Unnamed map", "Unnamed dungeon", "Unnamed room", "Untitled session",
        "Timeline event", "Unnamed audio", "Unnamed shop", "Unnamed encounter", "Unnamed combatant",
        "Combat log entry", "Untitled story", "Untitled beat", "Story link"};
    for (const char *fallback : kSystemFallbacks)
    {
        if (displayName == fallback)
            return gm::localization::trUi(fallback);
    }
    return displayName;
}

struct AudioBreakCounts
{
    int active = 0;
    int playing = 0;
    int paused = 0;
};

struct EffectsWatchRow
{
    std::string owner;
    std::string type;
    std::string name;
    std::string duration;
    std::string reminder;
    int urgency = 3;
    bool concentration = false;
    bool expiring = false;
};

AudioBreakCounts audioBreakCounts(const std::shared_ptr<AudioPlaybackService> &playbackService)
{
    AudioBreakCounts counts;
    if (!playbackService)
        return counts;

    const std::vector<EntityId> activeIds = playbackService->activeAssetIds();
    counts.active = static_cast<int>(activeIds.size());
    for (const EntityId &assetId : activeIds)
    {
        if (playbackService->isPaused(assetId))
            ++counts.paused;
        else
            ++counts.playing;
    }
    return counts;
}

std::string characterName(const CharacterModel &character)
{
    return character.nameBuffer[0] == '\0' ? gm::localization::trUi("Unnamed player") : std::string(character.nameBuffer);
}

std::string npcName(const NPCModel &npc)
{
    return npc.name.empty() ? gm::localization::trUi("Unnamed NPC") : npc.name;
}

std::string effectDurationLabel(const ActiveEffectModel &effect)
{
    if (effect.remainingRounds < 0)
        return gm::localization::trUi(effect.concentration ? "Concentration" : "Manual");
    if (effect.remainingRounds == 0)
        return gm::localization::trUi("Expires now");
    if (effect.remainingRounds == 1)
        return std::string("1 ") + gm::localization::trUi("round");
    return std::to_string(effect.remainingRounds) + " " + gm::localization::trUi("rounds");
}

std::string effectReminderLabel(const ActiveEffectModel &effect)
{
    if (!effect.active)
        return gm::localization::trUi("Inactive");
    if (effect.remainingRounds == 0)
        return gm::localization::trUi("Resolve now");
    if (effect.remainingRounds == 1)
        return gm::localization::trUi("Expires this round");
    if (effect.concentration)
        return gm::localization::trUi("Check concentration on damage");
    if (!effect.notes.empty())
        return effect.notes;
    return gm::localization::trUi("Monitor");
}

void appendConditionRows(std::vector<EffectsWatchRow> &rows, const std::string &owner, const std::vector<std::string> &conditions, const std::string &typePrefix)
{
    for (const std::string &condition : conditions)
    {
        if (condition.empty())
            continue;
        const std::string loweredCondition = lowerCopy(condition);
        const bool concentration = loweredCondition.find("concentrat") != std::string::npos ||
                                   loweredCondition.find("konzentrat") != std::string::npos;
        rows.push_back({owner,
                        typePrefix + " " + gm::localization::trUi(concentration ? "Concentration" : "Condition"),
                        condition,
                        gm::localization::trUi(concentration ? "Concentration" : "Until cleared"),
                        gm::localization::trUi(concentration ? "Check on damage" : "Track during scene"),
                        concentration ? 2 : 3,
                        concentration,
                        false});
    }
}

std::vector<EffectsWatchRow> buildEffectsWatchRows(const Campaign &campaign, const EncounterModel *encounter)
{
    std::vector<EffectsWatchRow> rows;
    for (const CharacterModel &character : campaign.characters)
        appendConditionRows(rows, characterName(character), character.conditions, gm::localization::trUi("PC"));

    for (const NPCModel &npc : campaign.npcs)
        appendConditionRows(rows, npcName(npc), npc.conditions, gm::localization::trUi("NPC"));

    if (encounter)
    {
        for (const EncounterParticipantModel &participant : encounter->participants)
        {
            const std::string owner = participant.displayName.empty() ? gm::localization::trUi("Unnamed combatant") : participant.displayName;
            appendConditionRows(rows, owner, participant.conditions, gm::localization::trUi("Combat"));
            for (const ActiveEffectModel &effect : participant.activeEffects)
            {
                if (effect.name.empty() && effect.notes.empty())
                    continue;
                const int urgency = !effect.active ? 4 : (effect.remainingRounds == 0 ? 0 : (effect.remainingRounds == 1 ? 1 : (effect.concentration ? 2 : 3)));
                rows.push_back({owner,
                                std::string(gm::localization::trUi(activeEffectKindLabel(effect.kind))) + (effect.concentration ? std::string(" / ") + gm::localization::trUi("Concentration") : ""),
                                effect.name.empty() ? effect.notes : effect.name,
                                effectDurationLabel(effect),
                                effectReminderLabel(effect),
                                urgency,
                                effect.concentration,
                                effect.active && (effect.remainingRounds == 0 || effect.remainingRounds == 1)});
            }
        }
    }

    std::sort(rows.begin(), rows.end(), [](const EffectsWatchRow &left, const EffectsWatchRow &right) {
        if (left.urgency != right.urgency)
            return left.urgency < right.urgency;
        if (left.owner != right.owner)
            return left.owner < right.owner;
        return left.name < right.name;
    });
    return rows;
}

bool encounterHasStarted(const EncounterModel *encounter)
{
    return encounter && (encounter->combatPaused ||
                         (encounter->combatPhase != CombatPhase::Setup && encounter->combatPhase != CombatPhase::Initiative) ||
                         encounter->round > 1 || encounter->activeParticipantIndex > 0 || !encounter->combatLog.empty());
}

int responsivePhaseColumnCount(int preferredColumns)
{
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    // Live cells contain their own tables and prose. Giving each cell roughly
    // 500 logical pixels is substantially more robust than merely fitting the
    // outer headings, especially at 1280x720 and increased text sizes.
    return gm::ux::responsiveColumnCount(availableWidth, spacing, 500.0f, preferredColumns);
}

std::string sceneSelectionLabel(const StoryThreadModel *thread, const StoryBeatModel *beat)
{
    if (!thread && !beat)
        return gm::localization::trUi("None");
    if (!beat)
        return thread->title.empty() ? gm::localization::trUi("Untitled story") : thread->title;
    if (!thread)
        return beat->title.empty() ? gm::localization::trUi("Untitled scene") : beat->title;
    return (thread->title.empty() ? std::string(gm::localization::trUi("Untitled story")) : thread->title) + " / " +
           (beat->title.empty() ? std::string(gm::localization::trUi("Untitled scene")) : beat->title);
}

std::string mapSelectionLabel(const MapModel *map)
{
    if (!map)
        return gm::localization::trUi("None");
    return map->name.empty() ? std::string(gm::localization::trUi("Untitled map")) : map->name;
}

std::string encounterSelectionLabel(const EncounterModel *encounter)
{
    if (!encounter)
        return gm::localization::trUi("None");
    return encounter->name.empty() ? std::string(gm::localization::trUi("Untitled encounter")) : encounter->name;
}

const char *phaseLabel(LiveSessionDashboardPhase phase)
{
    using gm::localization::TextId;
    switch (phase)
    {
    case LiveSessionDashboardPhase::Prep:
        return gm::localization::tr(TextId::LiveSetup);
    case LiveSessionDashboardPhase::RunningScene:
        return gm::localization::tr(TextId::LiveScene);
    case LiveSessionDashboardPhase::ExplorationTravel:
        return gm::localization::tr(TextId::LiveExploration);
    case LiveSessionDashboardPhase::Combat:
        return gm::localization::tr(TextId::LiveCombat);
    case LiveSessionDashboardPhase::Break:
        return gm::localization::tr(TextId::LiveBreak);
    case LiveSessionDashboardPhase::WrapUp:
        return gm::localization::tr(TextId::LiveWrapUp);
    default:
        return gm::localization::tr(TextId::LiveSetup);
    }
}

const char *phaseHelp(LiveSessionDashboardPhase phase)
{
    switch (phase)
    {
    case LiveSessionDashboardPhase::Prep:
        return gm::localization::trUi("Prepare the party, choose opening scenes, and start the live session.");
    case LiveSessionDashboardPhase::RunningScene:
        return gm::localization::trUi("Run the active scene with notes, scene links, audio, clock, and rolls close at hand.");
    case LiveSessionDashboardPhase::ExplorationTravel:
        return gm::localization::trUi("Keep party location, travel clock, route notes, timeline, and table notes visible.");
    case LiveSessionDashboardPhase::Combat:
        return gm::localization::trUi("Run the prepared encounter here while keeping notes, effects, audio, and the player display in the same Live workspace.");
    case LiveSessionDashboardPhase::Break:
        return gm::localization::trUi("Keep resume/end controls and audio visible while the table is paused.");
    case LiveSessionDashboardPhase::WrapUp:
        return gm::localization::trUi("Capture recap, rewards, follow-ups, and the post-session report before ending or leaving the session.");
    default:
        return "";
    }
}

std::string phaseNextAction(LiveSessionDashboardPhase phase,
                            const SessionModel *session,
                            const EncounterModel *encounter,
                            bool breakActive,
                            bool hasSceneOptions)
{
    switch (phase)
    {
    case LiveSessionDashboardPhase::Prep:
        return gm::localization::tr(session ? gm::localization::TextId::LiveNextPrepActive
                                            : gm::localization::TextId::LiveNextPrepInactive);
    case LiveSessionDashboardPhase::RunningScene:
        return gm::localization::tr(hasSceneOptions ? gm::localization::TextId::LiveNextSceneReady
                                                    : gm::localization::TextId::LiveNextSceneMissing);
    case LiveSessionDashboardPhase::ExplorationTravel:
        return gm::localization::tr(gm::localization::TextId::LiveNextExploration);
    case LiveSessionDashboardPhase::Combat:
        return gm::localization::tr(encounterHasStarted(encounter) || (encounter && encounter->combatPaused)
                                        ? gm::localization::TextId::LiveNextCombatActive
                                        : gm::localization::TextId::LiveNextCombatInactive);
    case LiveSessionDashboardPhase::Break:
        return gm::localization::tr(breakActive ? gm::localization::TextId::LiveNextBreakActive
                                                : gm::localization::TextId::LiveNextBreakInactive);
    case LiveSessionDashboardPhase::WrapUp:
        return gm::localization::tr(gm::localization::TextId::LiveNextWrapUp);
    default:
        return gm::localization::tr(gm::localization::TextId::LiveNextPrepInactive);
    }
}

std::string phaseHeaderLabel(LiveSessionDashboardPhase phase)
{
    return gm::localization::LocalizationService::instance().format(
        gm::localization::TextId::LiveCurrentPhase,
        {phaseLabel(phase)});
}

const LiveSessionDashboardPhase kDashboardPhases[] = {
    LiveSessionDashboardPhase::Prep,
    LiveSessionDashboardPhase::RunningScene,
    LiveSessionDashboardPhase::ExplorationTravel,
    LiveSessionDashboardPhase::Combat,
    LiveSessionDashboardPhase::Break,
    LiveSessionDashboardPhase::WrapUp};
}
