#pragma once

#include "imgui.h"

namespace gm::mapui
{
inline bool mapCreatorShouldUpdateGeneratedFeatureDrag(bool editActive)
{
    const ImGuiIO &io = ImGui::GetIO();
    const bool released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    if ((!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !released) ||
        !ImGui::IsMousePosValid(&io.MousePos)) return false;

    if (editActive || ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) return true;
    if (!released || !ImGui::IsMousePosValid(&io.MouseClickedPos[ImGuiMouseButton_Left])) return false;

    // ImGui may deliver the final position and button-up in the same frame.
    // IsMouseDragging and its maximum-distance accumulator only update while
    // held, so a fast drag needs the release position checked explicitly.
    const float dx = io.MousePos.x - io.MouseClickedPos[ImGuiMouseButton_Left].x;
    const float dy = io.MousePos.y - io.MouseClickedPos[ImGuiMouseButton_Left].y;
    return dx * dx + dy * dy >= 1.0f;
}
}
