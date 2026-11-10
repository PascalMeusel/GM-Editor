#pragma once

#include "Models/Map/MapTypography.hpp"
#include "imgui.h"
#include <string_view>

inline constexpr const char *kMapAtlasFontName = "GM Atlas Serif";

inline ImFont *mapLabelFont(const MapModel &map)
{
    if(gm::map_render::usesAtlasSerifTypography(map))
        for(ImFont *font:ImGui::GetIO().Fonts->Fonts)
            if(font && std::string_view(font->GetDebugName())==kMapAtlasFontName)
                return font;
    // Minimal test contexts and incomplete installations remain readable.
    return ImGui::GetFont();
}
