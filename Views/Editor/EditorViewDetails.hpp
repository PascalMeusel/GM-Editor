#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorOutputPreviewRendering.hpp"
#include "Views/Editor/EditorWindowView.hpp"
#include "Controllers/Editor/EditorWindowController.hpp"
#include "Models/Audio/AudioMixService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Project/EntityRegistry.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/ProjectLogger.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Session/PlayerSafeDataFilter.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/Rulesets/RulesetPresentationFormatter.hpp"
#include "Models/Settings/AccessibilityRuntime.hpp"
#include "Views/Editor/EditorLayoutProfileService.hpp"
#include "Views/Editor/Widgets/DockableWidget.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Views/Editor/Widgets/Map/MapTileVisuals.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

namespace gm::editor_view_detail
{

const char *saveUiText(const char *english, const char *german);
inline const ImVec4 kAccent = ImVec4(0.52f, 0.80f, 0.76f, 1.0f);
inline const ImVec4 kWarmAccent = ImVec4(0.88f, 0.68f, 0.36f, 1.0f);
inline const ImVec4 kMutedText = ImVec4(0.68f, 0.73f, 0.78f, 1.0f);
inline const ImVec4 kPanelBg = ImVec4(0.15f, 0.16f, 0.17f, 1.0f);

enum class TransportIconKind
{
    Play,
    Pause,
    Stop,
    Next
};

struct SidebarNavEntry
{
    const char *group = "";
    const char *label = "";
    const char *keywords = "";
    UiButtonRole role = UiButtonRole::Secondary;
    std::function<void()> open;
    bool enabled = true;
    const char *disabledReason = "";
};

std::string lowerCopy(std::string value);

bool containsText(const std::string &haystack, const std::string &needle);

template <typename... Arguments> std::string formatLocalizedUi(const char *englishFormat, Arguments... arguments)
{
    const std::string format = localizedUiLabel(englishFormat);
    const int required = std::snprintf(nullptr, 0, format.c_str(), arguments...);
    if (required <= 0)
        return format;

    std::vector<char> buffer(static_cast<std::size_t>(required) + 1U, '\0');
    std::snprintf(buffer.data(), buffer.size(), format.c_str(), arguments...);
    return std::string(buffer.data(), static_cast<std::size_t>(required));
}

int mainPlatformMonitorIndex();

bool sidebarEntryMatches(const SidebarNavEntry &entry, const std::string &query);

ImFont *getBoldFont();

void pushBoldFont();

void popBoldFont();

void drawTransportIcon(TransportIconKind kind, const ImVec2 &min, const ImVec2 &max, ImU32 color);

bool drawTransportIconButton(const char *id, TransportIconKind kind, bool enabled, const char *disabledReason,
                             UiButtonRole role, const char *tooltip);

void renderCenteredTextColored(const ImVec4 &color, const std::string &text);

void renderSectionLabel(const char *label);

bool renderLauncherButton(const char *label);

bool renderLauncherButton(const char *label, const char *tooltip);

void renderMetric(const char *label, int value);

void renderLabeledCount(const char *label, int value, const ImVec4 &valueColor = kAccent);

void renderHubLine(const char *label, const std::string &value);

void renderHubPanel(const char *id, const char *title, const std::vector<std::pair<std::string, std::string>> &lines);

void renderSaveChangeSummary(const char *label, const std::vector<std::string> &changes);

std::string knowledgeGraphEdgeKey(const CampaignGraphEdge &edge);

const CampaignGraphNode *findGraphNode(const CampaignKnowledgeGraph &graph, const EntityId &id);

const CampaignGraphEdge *findGraphEdge(const CampaignKnowledgeGraph &graph, const std::string &key);

bool edgeTouchesNode(const CampaignGraphEdge &edge, const EntityId &nodeId);

std::string graphEdgeSummary(const CampaignGraphEdge &edge);

enum class CampaignHealthSeverity
{
    Info,
    Warning,
    Error
};

enum class CampaignHealthAction
{
    None,
    SaveCampaign,
    RebuildIndex,
    OpenSearch,
    OpenAudioLibrary
};

struct CampaignHealthDiagnosticRow
{
    std::string key;
    CampaignHealthSeverity severity = CampaignHealthSeverity::Info;
    std::string affectedEntity;
    std::string diagnostic;
    CampaignHealthAction action = CampaignHealthAction::None;
};

const char *healthSeverityLabel(CampaignHealthSeverity severity);

ImVec4 healthSeverityColor(CampaignHealthSeverity severity);

const char *healthActionLabel(CampaignHealthAction action);

bool containsDiagnosticKey(const std::vector<std::string> &keys, const std::string &key);

void removeDiagnosticKey(std::vector<std::string> &keys, const std::string &key);

void addDiagnosticKey(std::vector<std::string> &keys, const std::string &key);

std::string campaignHealthStateLabel(const std::vector<std::string> &ignoredKeys,
                                     const std::vector<std::string> &resolvedKeys, const std::string &key);

std::vector<CampaignHealthDiagnosticRow> buildCampaignHealthRows(const Campaign &campaign,
                                                                 const ProjectSaveState &saveState,
                                                                 const std::vector<CampaignBrokenReference> &brokenRefs,
                                                                 const std::vector<ProjectLogEntry> &recentProblems);

const SessionModel *activeSession(const Campaign &campaign);

const StoryThreadModel *activeStoryThread(const Campaign &campaign);

const StoryBeatModel *activeStoryBeat(const StoryThreadModel *thread);

const StoryBeatModel *storyBeatById(const Campaign &campaign, const EntityId &beatId);

bool metadataHasTag(const EntityMetadata &metadata, const std::string &tag);

const MapModel *hubActiveMap(const Campaign &campaign);

bool encounterHasLiveState(const EncounterModel &encounter);

const EncounterModel *hubActiveEncounter(const Campaign &campaign);

std::string emptyFallback(const std::string &value, const char *fallback);

std::string compactWidgetText(const std::string &value, std::size_t maxLength = 42);

std::string audioAssetDisplayName(const AudioAssetModel &asset);

const AudioAssetModel *audioAssetById(const Campaign &campaign, const EntityId &assetId);

AudioAssetModel *audioAssetById(Campaign &campaign, const EntityId &assetId);

const AudioAmbienceLayerModel *ambienceLayerById(const Campaign &campaign, const EntityId &layerId);

std::string audioChannelDisplayName(const Campaign &campaign, const EntityId &channelId);

int playlistIndexOf(const std::vector<EntityId> &queue, const EntityId &assetId);

EntityId playlistNextId(const AudioPlaylistPlaybackState &state, int currentIndex);

bool playAudioAsset(Campaign &campaign, AudioPlaybackService &playback, const EntityId &assetId, bool oneShot,
                    std::string &errorMessage);

void publishMiniPlaylistState(AudioPlaybackService &playback, AudioPlaylistPlaybackState state,
                              const EntityId &currentId);

std::string campaignModeLabel(const Campaign &campaign);

std::string campaignSelectionLabel(const Campaign &campaign);

std::string storySceneLabel(const Campaign &campaign, const EntityId &sceneId);

std::string mapLabel(const Campaign &campaign, const EntityId &mapId);

std::string encounterLabel(const Campaign &campaign, const EntityId &encounterId);

std::string presentationSelectionLabel(const Campaign &campaign);

std::string campaignNextActionLabel(const Campaign &campaign, const ProjectSaveState &saveState);

std::string sessionInWorldClockLabel(const Campaign &campaign, const SessionModel *session);

std::string inWorldClockLabel(const Campaign &campaign);

ImU32 miniMapFeatureColor(MapFeatureKind kind);

std::string sessionClockModeLabel(const SessionModel *session, bool breakActive);

std::string sessionClockNextActionLabel(const SessionModel *session, bool breakActive);

bool renderTaskButton(const char *label, const char *tooltip, UiButtonRole role = UiButtonRole::Secondary,
                      bool selected = false);

bool renderWorkflowButton(const char *label, const char *tooltip, UiButtonRole role = UiButtonRole::Secondary,
                          bool selected = false);

bool renderWorkflowButton(const char *label, bool enabled, const char *disabledReason, const char *tooltip,
                          UiButtonRole role = UiButtonRole::Secondary, bool selected = false);

bool renderSidebarGroupButton(const char *label, bool &open, bool containsActive);

struct SidebarSubmenuFrame
{
    const char *id = "";
    bool containsActive = false;
};

SidebarSubmenuFrame beginSidebarSubmenuFrame(const char *id, bool containsActive);

void endSidebarSubmenuFrame(const SidebarSubmenuFrame &frame);

void renderSidebarEntryButton(const SidebarNavEntry &entry, const std::string &activeWorkflowLabel);

bool groupContainsActiveWorkflow(const std::vector<SidebarNavEntry> &entries, const char *group,
                                 const std::string &activeWorkflowLabel);

void renderSidebarGroup(const char *label, bool &open, const std::vector<SidebarNavEntry> &entries,
                        const std::string &activeWorkflowLabel);

const SidebarNavEntry *findSidebarEntry(const std::vector<SidebarNavEntry> &entries, const char *label);

void addRecommendedSidebarEntry(std::vector<const SidebarNavEntry *> &recommendations,
                                const std::vector<SidebarNavEntry> &entries, const char *label);

void renderSidebarRecommendedActions(const Campaign &campaign, const ProjectSaveState &saveState,
                                     const std::vector<SidebarNavEntry> &entries,
                                     const std::string &activeWorkflowLabel);

void renderSidebarWorkflowHeader(const Campaign &campaign, const std::string &modeLabel,
                                 const std::string &nextActionLabel);

void renderSidebarSearchResults(const std::string &query, const std::vector<SidebarNavEntry> &entries,
                                const std::string &activeWorkflowLabel);

void renderBookmarkLauncherList(EditorWindowController &controller,
                                const std::vector<CampaignEntityBookmarkModel> &bookmarks, const char *emptyText,
                                const char *prefix, UiButtonRole role, std::size_t limit);

}
