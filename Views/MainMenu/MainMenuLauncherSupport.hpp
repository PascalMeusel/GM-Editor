#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "MainMenuWindowView.hpp"
#include "Controllers/MainMenu/MainMenuWindowController.hpp"
#include "Models/Rulesets/RulesetFormulaEvaluator.hpp"
#include "Models/Rulesets/RulesetDefinitionSerialization.hpp"
#include "Models/Rulesets/RulesetDefinitionValidator.hpp"
#include "Models/Rulesets/RulesetPresentationFormatter.hpp"
#include "Models/Rulesets/RulesetVersioning.hpp"
#include "Models/Rulesets/RulesetStudioLibrary.hpp"
#include "Models/Project/StarterCampaignTemplate.hpp"
#include "Models/Core/ProductInfo.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Localization/LocaleFormatting.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Settings/AccessibilityRuntime.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/UX/EncounterResumeState.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "Views/Editor/ApprovedUi/ApprovedUiTheme.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <exception>
#include <iomanip>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif


namespace
{
const char *launcherText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
               ? german
               : english;
}

const char *rulesetStudioText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
               ? german
               : english;
}

std::string trimCopy(const std::string &value)
{
    auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    if (begin >= end)
        return "";
    return std::string(begin, end);
}

std::string rulesetIdentifier(const std::string &name)
{
    std::string id;
    for (unsigned char character : name)
    {
        if (std::isalnum(character))
            id.push_back(static_cast<char>(std::tolower(character)));
        else if (!id.empty() && id.back() != '-')
            id.push_back('-');
    }
    while (!id.empty() && id.back() == '-')
        id.pop_back();
    return id.empty() ? "custom-ruleset" : id;
}

ImFont *getBoldFont()
{
    ImFontAtlas *fonts = ImGui::GetIO().Fonts;
    return fonts->Fonts.Size > 1 ? fonts->Fonts[1] : nullptr;
}

void drawCenteredText(const char *text, float contentWidth, bool bold = false)
{
    const float textWidth = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (contentWidth - textWidth) * 0.5f);
    if (bold && getBoldFont())
        ImGui::PushFont(getBoldFont());
    ImGui::TextUnformatted(text);
    if (bold && getBoldFont())
        ImGui::PopFont();
}

void drawPanelHeading(const char *text)
{
    if (getBoldFont())
        ImGui::PushFont(getBoldFont());
    ImGui::TextUnformatted(text);
    if (getBoldFont())
        ImGui::PopFont();
}

std::string rulesetName(const Campaign &campaign)
{
    return campaign.ruleSet ? campaign.ruleSet->getName() : launcherText("No ruleset assigned", "Kein Regelwerk zugewiesen");
}

std::string pluralCount(int count, const char *singular, const char *plural)
{
    return std::to_string(count) + " " + (count == 1 ? singular : plural);
}

std::string campaignSummary(const Campaign &campaign)
{
    std::vector<std::string> parts;
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    auto addPart = [&](int count,
                       const char *englishSingular,
                       const char *englishPlural,
                       const char *germanSingular,
                       const char *germanPlural) {
        if (count > 0)
            parts.push_back(pluralCount(count,
                                        german ? germanSingular : englishSingular,
                                        german ? germanPlural : englishPlural));
    };

    addPart(static_cast<int>(campaign.characters.size()), "player", "players", "Spielfigur", "Spielfiguren");
    addPart(static_cast<int>(campaign.npcs.size() + campaign.customMonsters.size()), "NPC/monster", "NPCs/monsters", "NSC/Monster", "NSC/Monster");
    addPart(static_cast<int>(campaign.maps.size()), "map", "maps", "Karte", "Karten");
    addPart(static_cast<int>(campaign.storyThreads.size()), "story arc", "story arcs", "Handlungsbogen", "Handlungsbögen");
    addPart(static_cast<int>(campaign.encounters.size()), "encounter", "encounters", "Begegnung", "Begegnungen");
    addPart(static_cast<int>(campaign.sessions.size()), "session", "sessions", "Sitzung", "Sitzungen");

    if (parts.empty())
        return launcherText("Empty campaign workspace.", "Leerer Kampagnenarbeitsbereich.");

    std::string summary;
    for (const std::string &part : parts)
    {
        if (!summary.empty())
            summary += " | ";
        summary += part;
    }
    return summary;
}

std::string localizedCampaignTimestamp(const std::string &timestamp, bool includeTime)
{
    if (timestamp.size() < 19)
        return timestamp;

    std::tm utc{};
    std::istringstream input(timestamp.substr(0, 19));
    input >> std::get_time(&utc, "%Y-%m-%dT%H:%M:%S");
    if (input.fail())
        return timestamp;

#if defined(_WIN32)
    const std::time_t raw = _mkgmtime(&utc);
#else
    const std::time_t raw = timegm(&utc);
#endif
    if (raw == static_cast<std::time_t>(-1))
        return timestamp;

    const auto value = std::chrono::system_clock::from_time_t(raw);
    const gm::localization::Locale locale = gm::localization::LocalizationService::instance().locale();
    std::string formatted = gm::localization::formatDate(value, locale);
    if (includeTime)
        formatted += ", " + gm::localization::formatTime(value, locale);
    return formatted;
}

std::string campaignUpdatedLabel(const Campaign &campaign)
{
    if (!campaign.metadata.updatedAtUtc.empty())
        return std::string(launcherText("Updated: ", "Aktualisiert: ")) + localizedCampaignTimestamp(campaign.metadata.updatedAtUtc, true);
    if (!campaign.metadata.createdAtUtc.empty())
        return std::string(launcherText("Created: ", "Erstellt: ")) + localizedCampaignTimestamp(campaign.metadata.createdAtUtc, true);
    return launcherText("No saved timestamp", "Noch kein Speicherzeitpunkt");
}

std::string campaignCompactDateLabel(const Campaign &campaign)
{
    const std::string &timestamp = !campaign.metadata.updatedAtUtc.empty()
                                       ? campaign.metadata.updatedAtUtc
                                       : campaign.metadata.createdAtUtc;
    if (timestamp.size() < 10)
        return launcherText("Not saved yet", "Noch nicht gespeichert");

    return std::string(launcherText("Last changed: ", "Zuletzt geändert: ")) +
           localizedCampaignTimestamp(timestamp, false);
}

struct ResumeStateSummary
{
    bool available = false;
    std::string mode = "Launcher";
    std::string selection = "No saved live state";
    std::string nextAction = "Continue opens Campaign Home.";
};

std::string fallbackLabel(const std::string &value, const char *fallback)
{
    return value.empty() ? std::string(fallback) : value;
}

ResumeStateSummary resumeStateForCampaign(const Campaign &campaign)
{
    ResumeStateSummary state;
    state.mode = launcherText("Launcher", "Kampagnenauswahl");
    state.selection = launcherText("No saved live state", "Kein gespeicherter Live-Stand");
    state.nextAction = launcherText("Continue opens Campaign Home.", "Fortsetzen öffnet den Kampagnenstart.");
    const SessionModel *session = SessionControl::activeSession(campaign);
    const EncounterModel *encounter = gm::ux::resumableEncounter(campaign.encounters);

    if (session)
    {
        const bool breakActive = SessionControl::isBreakActive(*session);
        state.available = true;
        if (breakActive)
            state.mode = launcherText("Session paused", "Sitzung pausiert");
        else if (encounter && encounter->combatPaused)
            state.mode = launcherText("Combat paused", "Kampf pausiert");
        else if (encounter && gm::ux::encounterHasResumableState(*encounter))
            state.mode = launcherText("Combat in progress", "Kampf läuft");
        else
            state.mode = launcherText("Live session", "Live-Sitzung");
        state.selection = fallbackLabel(session->title, launcherText("Active live session", "Aktive Live-Sitzung"));
        if (encounter && gm::ux::encounterHasResumableState(*encounter))
            state.selection += " | " + fallbackLabel(encounter->name, launcherText("Active encounter", "Aktive Begegnung"));
        state.nextAction = launcherText("Loads Campaign Home with saved live state.", "Lädt den Kampagnenstart mit dem gespeicherten Live-Stand.");
        return state;
    }

    if (encounter)
    {
        state.available = true;
        state.mode = encounter->combatPaused ? launcherText("Combat paused", "Kampf pausiert")
                                             : launcherText("Combat in progress", "Kampf läuft");
        state.selection = fallbackLabel(encounter->name, launcherText("Active encounter", "Aktive Begegnung"));
        state.nextAction = launcherText("Loads Campaign Home with saved combat state.", "Lädt den Kampagnenstart mit dem gespeicherten Kampfstand.");
    }

    return state;
}

std::string suggestedNextAction(const Campaign &campaign)
{
    if (resumeStateForCampaign(campaign).available)
        return launcherText("Resume the saved table state from Campaign Home.", "Setze den gespeicherten Spieltischstand am Kampagnenstart fort.");
    if (campaign.characters.empty())
        return launcherText("Create the first player character.", "Erstelle die erste Spielfigur.");
    if (campaign.maps.empty())
        return launcherText("Add or open a campaign map.", "Erstelle oder öffne eine Kampagnenkarte.");
    if (campaign.storyThreads.empty())
        return launcherText("Plan the first story scene.", "Plane die erste Szene.");
    if (!campaign.sessions.empty())
        return launcherText("Continue prep or open the live session dashboard.", "Setze die Vorbereitung fort oder öffne die Live-Sitzung.");
    return launcherText("Open the campaign home.", "Öffne den Kampagnenstart.");
}

bool drawCampaignListEntry(const Campaign &campaign, int index, bool selected, bool &doubleClicked)
{
    const ResumeStateSummary resumeState = resumeStateForCampaign(campaign);
    std::string supportingText = rulesetName(campaign);
    supportingText += "  ·  ";
    supportingText += resumeState.available ? resumeState.mode : campaignCompactDateLabel(campaign);

    const float availableWidth = std::max(160.0f, ImGui::GetContentRegionAvail().x);
    const float textWidth = std::max(120.0f, availableWidth - 20.0f);
    const float nameHeight = ImGui::CalcTextSize(campaign.name.c_str(), nullptr, false, textWidth).y;
    const float supportingHeight = ImGui::CalcTextSize(supportingText.c_str(), nullptr, false, textWidth).y;
    const float rowHeight = std::max(58.0f, nameHeight + supportingHeight + 17.0f);
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();

    ImGui::PushID(index);
    const bool activated = ImGui::Selectable("##CampaignCard",
                                             selected,
                                             ImGuiSelectableFlags_AllowDoubleClick,
                                             ImVec2(availableWidth, rowHeight));
    const bool hovered = ImGui::IsItemHovered();
    doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

    const ImVec2 rowEnd = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(rowStart.x + 10.0f, rowStart.y + 7.0f));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + textWidth);
    if (getBoldFont())
        ImGui::PushFont(getBoldFont());
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), campaign.name.c_str());
    if (getBoldFont())
        ImGui::PopFont();
    if (resumeState.available)
        ImGui::TextColored(ImVec4(0.45f, 0.82f, 0.78f, 1.0f), localizedUiLabel("%s").c_str(), supportingText.c_str());
    else
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), supportingText.c_str());
    ImGui::PopTextWrapPos();
    ImGui::SetCursorScreenPos(rowEnd);
    ImGui::PopID();
    return activated;
}

struct LauncherButtonPairResult
{
    bool first = false;
    bool second = false;
};

LauncherButtonPairResult drawLauncherButtonPair(const char *firstLabel,
                                                UiButtonRole firstRole,
                                                const char *firstTooltip,
                                                const char *secondLabel,
                                                UiButtonRole secondRole,
                                                const char *secondTooltip)
{
    LauncherButtonPairResult result;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float firstMinimum = ImGui::CalcTextSize(localizedUiLabel(firstLabel).c_str()).x +
                               ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
    const float secondMinimum = ImGui::CalcTextSize(localizedUiLabel(secondLabel).c_str()).x +
                                ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
    const float columnWidth = (availableWidth - spacing) * 0.5f;
    const bool sideBySide = availableWidth > 0.0f &&
                            columnWidth >= std::max(firstMinimum, secondMinimum);

    if (sideBySide)
    {
        result.first = drawUiButton(firstLabel, firstRole, ImVec2(columnWidth, 36.0f), firstTooltip);
        ImGui::SameLine(0.0f, spacing);
        result.second = drawUiButton(secondLabel, secondRole, ImVec2(columnWidth, 36.0f), secondTooltip);
    }
    else
    {
        const float buttonWidth = std::min(availableWidth, std::max(240.0f, std::max(firstMinimum, secondMinimum)));
        result.first = drawUiButton(firstLabel, firstRole, ImVec2(buttonWidth, 36.0f), firstTooltip);
        result.second = drawUiButton(secondLabel, secondRole, ImVec2(buttonWidth, 36.0f), secondTooltip);
    }
    return result;
}

void drawLauncherCapability(const char *title, const char *description)
{
    if (getBoldFont())
        ImGui::PushFont(getBoldFont());
    ImGui::TextUnformatted(title);
    if (getBoldFont())
        ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), description);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
}

std::string selectedRulesetName(const std::vector<std::shared_ptr<IRuleSet>> &ruleSets, int selectedRuleIndex)
{
    if (ruleSets.empty())
        return launcherText("No rulesets available", "Keine Regelwerke verfügbar");
    if (selectedRuleIndex < 0 || selectedRuleIndex >= static_cast<int>(ruleSets.size()))
        return launcherText("No ruleset selected", "Kein Regelwerk ausgewählt");
    return ruleSets[static_cast<std::size_t>(selectedRuleIndex)]->getName();
}

const char *selectedTemplateName(int selectedTemplateIndex)
{
    if (selectedTemplateIndex == 1)
        return launcherText("Guided starter campaign", "Geführte Startkampagne");
    return launcherText("Blank campaign workspace", "Leerer Kampagnenarbeitsbereich");
}
}
