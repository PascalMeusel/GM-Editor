#pragma once

#include "LocalPlayerDisplayView.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "Controllers/Editor/Widgets/LocalPlayerDisplay/LocalPlayerDisplayController.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Session/PlayerSafeDataFilter.hpp"
#include "Views/Editor/ApprovedUi/ApprovedToolkitChrome.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Views/Editor/Widgets/Map/MapFogRendering.hpp"
#include "Views/Editor/Widgets/Map/MapTileVisuals.hpp"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <string>

namespace
{
const ImVec4 kAccent = gm::approved_ui::Verdigris;
const ImVec4 kMutedText = gm::approved_ui::Muted;

const EncounterModel *findEncounter(const Campaign &campaign, const EntityId &encounterId)
{
    const auto found = std::find_if(campaign.encounters.begin(), campaign.encounters.end(), [&](const EncounterModel &encounter) {
        return encounter.metadata.id == encounterId;
    });
    return found == campaign.encounters.end() ? nullptr : &*found;
}

std::string safeDisplayName(const std::string &value, const char *fallback)
{
    return value.empty() ? localizedUiLabel(fallback) : value;
}

void pushDisplayTitle()
{
    ImFontAtlas *fonts = ImGui::GetIO().Fonts;
    if (fonts->Fonts.Size > 1)
        ImGui::PushFont(fonts->Fonts[1]);
}

void popDisplayTitle()
{
    if (ImGui::GetIO().Fonts->Fonts.Size > 1)
        ImGui::PopFont();
}

int mainMonitorIndex(const ImGuiPlatformIO &platform, const ImGuiViewport &mainViewport)
{
    const ImVec2 center(mainViewport.Pos.x + mainViewport.Size.x * 0.5f,
                        mainViewport.Pos.y + mainViewport.Size.y * 0.5f);
    for (int monitorIndex = 0; monitorIndex < platform.Monitors.Size; ++monitorIndex)
    {
        const ImGuiPlatformMonitor &monitor = platform.Monitors[monitorIndex];
        if (center.x >= monitor.MainPos.x && center.y >= monitor.MainPos.y &&
            center.x < monitor.MainPos.x + monitor.MainSize.x &&
            center.y < monitor.MainPos.y + monitor.MainSize.y)
            return monitorIndex;
    }
    return platform.Monitors.empty() ? -1 : 0;
}
}
