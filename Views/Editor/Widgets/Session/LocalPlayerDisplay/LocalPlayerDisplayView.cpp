#include "Views/Editor/Widgets/Session/LocalPlayerDisplay/LocalPlayerDisplayViewSupport.hpp"

LocalPlayerDisplayView::LocalPlayerDisplayView(LocalPlayerDisplayController &controller)
    : _controller(controller)
{
}

void LocalPlayerDisplayView::update()
{
    if (!_isVisible)
        return;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImGuiPlatformIO &platform = ImGui::GetPlatformIO();
    const LocalPlayerDisplayModel &display = _controller._currentCampaign.localPlayerDisplay;
    const LocalPlayerDisplayWindowPlacement placement = resolveLocalPlayerDisplayWindowPlacement(
        display,
        platform.Monitors.Size,
        mainMonitorIndex(platform, *viewport));
    if (display.preferredMonitorIndex >= platform.Monitors.Size &&
        display.preferredMonitorIndex >= 0 && localPlayerDisplayIsPublished(display))
    {
        // A disconnected output must never move already-published content onto
        // the GM monitor. Fall back to a window, but keep it black until the GM
        // explicitly reviews and publishes again.
        _controller.blackout();
    }
    if (placement.monitorIndex != _lastResolvedMonitorIndex ||
        placement.fullscreen != _lastFullscreen ||
        placement.fallbackToMainWindow != _lastFallback)
        _placementRequested = true;

    const ImGuiPlatformMonitor *targetMonitor = placement.monitorIndex >= 0 &&
                                                       placement.monitorIndex < platform.Monitors.Size
                                                   ? &platform.Monitors[placement.monitorIndex]
                                                   : nullptr;
    if (placement.fullscreen && targetMonitor != nullptr)
    {
        ImGui::SetNextWindowPos(targetMonitor->MainPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(targetMonitor->MainSize, ImGuiCond_Always);
    }
    else if (_placementRequested)
    {
        const ImVec2 workPos = targetMonitor != nullptr ? targetMonitor->WorkPos : viewport->WorkPos;
        const ImVec2 workSize = targetMonitor != nullptr ? targetMonitor->WorkSize : viewport->WorkSize;
        const ImVec2 windowSize(std::min(1180.0f, std::max(640.0f, workSize.x * 0.88f)),
                                std::min(700.0f, std::max(420.0f, workSize.y * 0.82f)));
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x * 0.5f,
                                      workPos.y + workSize.y * 0.5f),
                                ImGuiCond_Always,
                                ImVec2(0.5f, 0.5f));
    }

    bool open = _isVisible;
    // Screen-sharing applications must be able to select this window even on
    // one monitor. Do not merge it back into the GM host when it overlaps it.
    ImGuiWindowClass playerWindowClass;
    playerWindowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
    // An owned Win32 window stays above its owner, obscuring the GM's review
    // dialog on one monitor. Keep the shareable display independently focusable.
    playerWindowClass.ParentViewportId = 0;
    playerWindowClass.ViewportFlagsOverrideClear = ImGuiViewportFlags_CanHostOtherWindows;
    ImGui::SetNextWindowClass(&playerWindowClass);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;
    if (placement.fullscreen)
        flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    // Keep the detached window identity stable when the UI language changes;
    // only its visible title is localized. Otherwise ImGui creates a second
    // window state and loses the user's monitor position/size.
    const std::string windowTitle = localizedUiLabel("Player Display") + "###LocalPlayerDisplay";
    const bool blackedOut = !localPlayerDisplayIsPublished(_controller._currentCampaign.localPlayerDisplay);
    if (blackedOut)
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.005f, 0.005f, 0.006f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.005f, 0.005f, 0.006f, 1.0f));
    }
    const bool expanded = ImGui::Begin(windowTitle.c_str(), &open, flags);
    _placementRequested = false;
    _lastResolvedMonitorIndex = placement.monitorIndex;
    _lastFullscreen = placement.fullscreen;
    _lastFallback = placement.fallbackToMainWindow;
    _isVisible = open;
    if (expanded)
    {
        const float textScale = std::isfinite(display.playerTextScale)
            ? std::clamp(display.playerTextScale, 1.0f, 1.8f) : 1.15f;
        ImGui::SetWindowFontScale(textScale);
        draw();
        ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::End();
    if (blackedOut)
        ImGui::PopStyleColor(2);
}

void LocalPlayerDisplayView::show()
{
    _isVisible = true;
}

void LocalPlayerDisplayView::hide()
{
    _isVisible = false;
}

bool LocalPlayerDisplayView::isVisible() const
{
    return _isVisible;
}

void LocalPlayerDisplayView::requestPlacement()
{
    _placementRequested = true;
}

void LocalPlayerDisplayView::draw()
{
    const Campaign &campaign = _controller._currentCampaign;
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    const PlayerDisplayReviewSnapshot &review = _controller._reviewCache;

    // Do not even resolve public source IDs before the GM explicitly
    // publishes. This makes startup, campaign switches, payload edits and the
    // B hotkey all fail closed through the same rendering path.
    if (!localPlayerDisplayIsPublished(display) || !review.eligible ||
        display.publishedReviewFingerprint == 0 ||
        display.publishedReviewFingerprint != review.contentFingerprint)
    {
        drawBlackoutSurface();
        return;
    }

    const MapModel *map = review.hasMapPreview && _controller._reviewedMapCacheValid
                              ? &_controller._reviewedMapCache
                              : nullptr;

    if (display.mapOnly)
    {
        if (display.showMap && map != nullptr)
            drawMapPanel(map, &review.playerMapPlan, true);
        else
            drawBlackoutSurface();
        return;
    }

    pushDisplayTitle();
    const std::string displayTitle = review.campaignName.empty() ? localizedUiLabel("Table Display") : review.campaignName;
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), displayTitle.c_str());
    ImGui::PopStyleColor();
    popDisplayTitle();
    ImGui::Separator();

    const bool hasMap = display.showMap && map != nullptr;
    const bool hasSharedArt = display.showSharedArt && map != nullptr &&
        !map->importedReference.enabled && review.playerMapPlan.backgroundVisible &&
        !map->backgroundImagePath.empty();
    const bool hasMainSurface = hasMap || hasSharedArt;
    const auto drawMainSurface = [&](bool fillAvailableHeight = false) {
        if (hasMap)
            drawMapPanel(map, review.hasMapPreview ? &review.playerMapPlan : nullptr, false,
                         fillAvailableHeight && !hasSharedArt);
        if (hasSharedArt)
            drawSharedArtPanel(map, review.hasMapPreview ? &review.playerMapPlan : nullptr);
    };
    const auto drawSidePanels = [&]() {
        drawScenePanel();
        if (display.showInitiative)
            drawInitiativePanel();
        drawStoryTrackingPanel();
        if (display.showParty)
            drawPartyPanel();
        if (display.showPartyInventory)
            drawInventoryPanel();
        if (display.showVisibleNpcs)
            drawVisibleNpcsPanel();
        if (display.showPublicHandouts)
            drawHandoutsPanel();
        drawSafetySummaryPanel();
    };

    // At the supported minimum width a fixed two-column layout makes both the
    // map and translated side-panel copy needlessly hard to read. Stack the
    // same content until both columns have useful working room.
    if (!hasMainSurface)
    {
        // A conversation scene should read like a shared handout, not a tiny
        // sidebar beside an empty map/art column. Keep a comfortable line length.
        const float available = ImGui::GetContentRegionAvail().x;
        const float textWidth = std::min(available, ImGui::GetFontSize() * 52.0f);
        const float inset = std::max(0.0f, (available - textWidth) * 0.5f);
        if (inset > 0.0f) ImGui::Indent(inset);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + textWidth);
        drawSidePanels();
        ImGui::PopTextWrapPos();
        if (inset > 0.0f) ImGui::Unindent(inset);
    }
    else if (ImGui::GetContentRegionAvail().x < 860.0f)
    {
        drawMainSurface();
        ImGui::Spacing();
        ImGui::Separator();
        drawSidePanels();
    }
    else if (ImGui::BeginTable("PlayerDisplayLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Main").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.64f);
        ImGui::TableSetupColumn(localizedUiLabel("Side").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.36f);
        ImGui::TableNextRow();

        // Keep the map in place while longer scene and journal text scrolls.
        // A short shared scene must not reserve half the map column for empty art.
        const float panelHeight = std::max(160.0f, ImGui::GetContentRegionAvail().y - 8.0f);
        ImGui::TableNextColumn();
        ImGui::BeginChild("PlayerMapAndArt", ImVec2(0.0f, panelHeight));
        drawMainSurface(true);
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("PlayerSceneAndJournal", ImVec2(0.0f, panelHeight));
        drawSidePanels();
        ImGui::EndChild();

        ImGui::EndTable();
    }
}
