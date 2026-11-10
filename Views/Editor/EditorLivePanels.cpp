#include "Views/Editor/EditorViewDetails.hpp"

using namespace gm::editor_view_detail;

void EditorWindowView::renderPersistentSessionClock()
{
    const CompactUiDensityScope uiDensity;
    const SessionModel *session = _controller.activeSession();
    const bool breakActive = _controller.isSessionBreakActive();
    std::string inWorld = _controller.currentInWorldClockLabel();
    if (inWorld.empty())
        inWorld = localizedUiText("Clock not set");

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    if (!ImGui::Begin(localizedUiLabel("Game Time").c_str(), nullptr, flags))
    {
        ImGui::End();
        return;
    }

    const std::string compactInWorld = compactWidgetText(inWorld);
    ImGui::TextDisabled(localizedUiLabel("Game time").c_str());
    pushBoldFont();
    ImGui::TextUnformatted(compactInWorld.c_str());
    popBoldFont();
    if (compactInWorld != inWorld)
        showHelpTooltip(inWorld.c_str());
    const char *clockState =
        _controller._currentCampaign.clock.useSystemTime
            ? localizedUiText("System time")
            : (_controller._currentCampaign.clock.running ? localizedUiText("Running") : localizedUiText("Paused"));
    ImGui::TextDisabled(localizedUiLabel("%s | %s | x%.2f").c_str(),
                        _controller.currentInWorldClockPhaseLabel().c_str(), clockState,
                        _controller._currentCampaign.clock.multiplier);

    const bool clockAdvancing =
        _controller._currentCampaign.clock.useSystemTime || _controller._currentCampaign.clock.running;
    const char *buttonLabel = clockAdvancing ? "Pause Clock" : "Run Clock";
    if (drawUiButton(buttonLabel, UiButtonRole::Primary, ImVec2(120.0f, 0.0f),
                     "Start or stop in-world time progression without changing session state."))
        _controller.setCampaignClockRunning(!clockAdvancing, "toggle pinned game clock");
    ImGui::SameLine();
    if (drawUiButton("Live", session ? UiButtonRole::Secondary : UiButtonRole::Primary, ImVec2(64.0f, 0.0f),
                     session ? "Open Live Dashboard." : "Open Live Dashboard to start a session."))
        _controller.showLiveSessionDashboard();

    const char *sessionButtonLabel =
        !session ? "Start in Dashboard" : (breakActive ? "Resume in Dashboard" : "Break in Dashboard");
    const char *buttonTooltip = !session ? "Open Live Dashboard to review and start a live session."
                                         : (breakActive ? "Open Live Dashboard to review and resume the table."
                                                        : "Open Live Dashboard to review break options.");
    if (drawUiButton(sessionButtonLabel, UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), buttonTooltip))
        _controller.showLiveSessionDashboard();

    ImGui::End();
}

void EditorWindowView::renderPersistentAudioMini()
{
    if (!_controller._audioPlaybackService)
        return;

    Campaign &campaign = _controller._currentCampaign;
    AudioPlaybackService &playback = *_controller._audioPlaybackService;
    const std::vector<EntityId> activeIds = playback.activeAssetIds();
    const AudioPlaylistPlaybackState playlistState = playback.playlistPlaybackState();

    EntityId focusedAudioId = playlistState.currentAssetId;
    if (focusedAudioId.empty() && !activeIds.empty())
        focusedAudioId = activeIds.front();

    const std::string currentLabel =
        focusedAudioId.empty() ? "No active audio" : audioChannelDisplayName(campaign, focusedAudioId);
    const bool focusedActive = !focusedAudioId.empty() && playback.isActive(focusedAudioId);
    const bool focusedPaused = focusedActive && playback.isPaused(focusedAudioId);
    const bool hasPlayableFocus = !focusedAudioId.empty() && audioAssetById(campaign, focusedAudioId) != nullptr;
    const bool canStartPauseResume = focusedActive || hasPlayableFocus;
    const bool canReset = focusedActive;
    const bool canPlayNext = playlistState.active && !playlistState.nextAssetId.empty() &&
                             audioAssetById(campaign, playlistState.nextAssetId) != nullptr;

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    if (!ImGui::Begin(localizedUiLabel("Audio Mini Player").c_str(), nullptr, flags))
    {
        ImGui::End();
        return;
    }

    const std::string compactCurrentLabel = compactWidgetText(currentLabel);
    ImGui::TextDisabled(localizedUiLabel("Current audio").c_str());
    pushBoldFont();
    ImGui::TextUnformatted(compactCurrentLabel.c_str());
    popBoldFont();
    if (compactCurrentLabel != currentLabel)
        showHelpTooltip(currentLabel.c_str());

    if (drawTransportIconButton("##AudioMiniPlayPause",
                                focusedActive && !focusedPaused ? TransportIconKind::Pause : TransportIconKind::Play,
                                canStartPauseResume, "No current audio or playlist item is available.",
                                focusedPaused || !focusedActive ? UiButtonRole::Primary : UiButtonRole::Secondary,
                                focusedActive ? (focusedPaused ? "Resume current audio." : "Pause current audio.")
                                              : "Start the current audio from the beginning."))
    {
        std::string errorMessage;
        bool changed = false;
        if (!focusedActive)
        {
            changed = playAudioAsset(campaign, playback, focusedAudioId, playlistState.oneShot, errorMessage);
            if (changed)
            {
                publishMiniPlaylistState(playback, playlistState, focusedAudioId);
                _controller.saveCurrentCampaign("audio mini start");
            }
        }
        else if (focusedPaused)
            changed = playback.resume(focusedAudioId, errorMessage);
        else
            changed = playback.pause(focusedAudioId, errorMessage);

        if (!changed && !errorMessage.empty())
            pushToast("Audio", errorMessage, ImVec4(0.95f, 0.36f, 0.30f, 1.0f));
    }
    ImGui::SameLine();
    if (drawTransportIconButton("##AudioMiniReset", TransportIconKind::Stop, canReset, "No current audio is playing.",
                                UiButtonRole::Secondary, "Stop and reset the current audio."))
    {
        std::string errorMessage;
        if (!playback.stop(focusedAudioId, errorMessage) && !errorMessage.empty())
            pushToast("Audio", errorMessage, ImVec4(0.95f, 0.36f, 0.30f, 1.0f));
    }
    ImGui::SameLine();
    if (drawTransportIconButton("##AudioMiniNext", TransportIconKind::Next, canPlayNext,
                                "No next playlist item is available.", UiButtonRole::Primary,
                                "Jump to the next song in the active playlist."))
    {
        std::string errorMessage;
        std::string ignored;
        if (focusedActive)
            playback.stop(focusedAudioId, ignored);
        if (playAudioAsset(campaign, playback, playlistState.nextAssetId, playlistState.oneShot, errorMessage))
        {
            publishMiniPlaylistState(playback, playlistState, playlistState.nextAssetId);
            _controller.saveCurrentCampaign("audio mini next");
        }
        else if (!errorMessage.empty())
        {
            pushToast("Audio", errorMessage, ImVec4(0.95f, 0.36f, 0.30f, 1.0f));
        }
    }

    ImGui::End();
}

void EditorWindowView::renderPersistentMiniMap()
{
    const CompactUiDensityScope uiDensity;
    const Campaign &campaign = _controller.currentCampaign();
    const MapModel *map = hubActiveMap(campaign);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    if (!ImGui::Begin(localizedUiLabel("Mini Map").c_str(), nullptr, flags))
    {
        ImGui::End();
        return;
    }

    if (!map || map->width <= 0 || map->height <= 0)
    {
        ImGui::TextDisabled(localizedUiLabel("No map selected.").c_str());
        ImGui::End();
        return;
    }

    const std::string mapTitle = compactWidgetText(map->name.empty() ? std::string("Untitled map") : map->name);
    pushBoldFont();
    ImGui::TextUnformatted(mapTitle.c_str());
    popBoldFont();
    if (mapTitle != map->name && !map->name.empty())
        showHelpTooltip(map->name.c_str());

    ImVec2 available = ImGui::GetContentRegionAvail();
    const float canvasHeight = std::max(96.0f, available.y - ImGui::GetTextLineHeightWithSpacing());
    const float cellSize = std::max(
        2.0f, std::min(available.x / static_cast<float>(map->width), canvasHeight / static_cast<float>(map->height)));
    const ImVec2 mapSize(cellSize * static_cast<float>(map->width), cellSize * static_cast<float>(map->height));
    const float xOffset = std::max(0.0f, (available.x - mapSize.x) * 0.5f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xOffset);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(mapSize);

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin, ImVec2(origin.x + mapSize.x, origin.y + mapSize.y), IM_COL32(22, 24, 26, 255));
    for (int y = 0; y < map->height; ++y)
    {
        for (int x = 0; x < map->width; ++x)
        {
            const MapCellModel *cell = mapCellAt(*map, x, y);
            const MapTileKind kind = cell ? cell->kind : MapTileKind::Empty;
            const ImVec2 cellMin(origin.x + static_cast<float>(x) * cellSize,
                                 origin.y + static_cast<float>(y) * cellSize);
            const ImVec2 cellMax(cellMin.x + cellSize, cellMin.y + cellSize);
            drawList->AddRectFilled(cellMin, cellMax, mapTileColor(kind));
        }
    }

    const auto drawMarker = [&](float x, float y, ImU32 color, float radius) {
        const ImVec2 center(origin.x + (x + 0.5f) * cellSize, origin.y + (y + 0.5f) * cellSize);
        drawList->AddCircleFilled(center, std::max(radius, cellSize * 0.45f), color);
        drawList->AddCircle(center, std::max(radius, cellSize * 0.45f), IM_COL32(12, 14, 16, 240), 12, 1.0f);
    };

    for (const MapFeatureModel &feature : map->features)
    {
        if (!isMapCellInside(*map, feature.x, feature.y))
            continue;
        drawMarker(static_cast<float>(feature.x), static_cast<float>(feature.y), miniMapFeatureColor(feature.kind),
                   2.2f);
    }

    for (const MapTokenModel &token : map->tokens)
    {
        if (token.hiddenFromPlayers)
            continue;
        drawMarker(token.x, token.y, IM_COL32(216, 216, 226, 230), 2.0f);
    }

    const bool partyOnMap =
        campaign.partyLocation.mapId == map->metadata.id ||
        (campaign.partyLocation.mapId.empty() && campaign.partyTravel.currentMapId == map->metadata.id);
    if (partyOnMap && isMapCellInside(*map, campaign.partyLocation.x, campaign.partyLocation.y))
        drawMarker(static_cast<float>(campaign.partyLocation.x), static_cast<float>(campaign.partyLocation.y),
                   IM_COL32(118, 211, 199, 255), 3.0f);

    drawList->AddRect(origin, ImVec2(origin.x + mapSize.x, origin.y + mapSize.y), IM_COL32(210, 220, 226, 150), 0.0f, 0,
                      1.0f);
    ImGui::TextDisabled(localizedUiLabel("%dx%d | %s").c_str(), map->width, map->height, mapScopeLabel(map->scope));

    ImGui::End();
}
