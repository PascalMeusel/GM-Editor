#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>

inline bool mapCanvasDragPositionAvailable()
{
    return (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) &&
        ImGui::IsMousePosValid(&ImGui::GetIO().MousePos);
}

inline void applyMapCanvasWheelZoom(float &zoom, float minZoom, float maxZoom, float step = 0.12f)
{
    const ImGuiIO &io = ImGui::GetIO();
    if (io.MouseWheel == 0.0f || !ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) return;
    const float previous = std::clamp(zoom, std::max(0.001f, minZoom), maxZoom);
    const float next = std::clamp(previous * std::pow(1.0f + step, io.MouseWheel), minZoom, maxZoom);
    if (next == previous) return;
    const ImVec2 mouse = ImGui::GetMousePos(), window = ImGui::GetWindowPos();
    const ImVec2 pointer(mouse.x-window.x, mouse.y-window.y);
    const float ratio = next / previous;
    zoom = next;
    ImGui::SetScrollX(std::max(0.0f, (pointer.x + ImGui::GetScrollX()) * ratio - pointer.x));
    ImGui::SetScrollY(std::max(0.0f, (pointer.y + ImGui::GetScrollY()) * ratio - pointer.y));
}
