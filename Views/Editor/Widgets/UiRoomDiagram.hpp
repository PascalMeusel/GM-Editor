#pragma once
#include "Models/Campaign/Campaign.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "imgui.h"
#include <algorithm>
#include <vector>

namespace RoomDiagramUi
{
// Edits the caller's draft only. The owning inspector commits it through its
// normal save/retry boundary; selecting a node never writes campaign data.
inline bool draw(DungeonModel &dungeon, const Campaign &campaign, EntityId &selected, bool &placing)
{
    const float width = std::max(1.f, ImGui::GetContentRegionAvail().x);
    const int gridColumns = std::max(1, std::min(4, int(width / 110.f)));
    const int gridRows = std::max(1, (int(dungeon.rooms.size()) + gridColumns - 1) / gridColumns);
    const float height = std::max(180.f, gridRows * 55.f);
    int columns = 24, rows = 18;
    for (const auto &map : campaign.maps)
        if (map.metadata.id == dungeon.parentMapId)
        {
            columns = std::max(1, map.width);
            rows = std::max(1, map.height);
        }
    const bool arranged = std::all_of(dungeon.rooms.begin(), dungeon.rooms.end(),
                                      [](const auto &r) { return r.mapX == 0 && r.mapY == 0; });
    const auto origin = ImGui::GetCursorScreenPos();
    auto *draw = ImGui::GetWindowDrawList();
    std::vector<ImVec2> centres;
    for (std::size_t i = 0; i < dungeon.rooms.size(); ++i)
    {
        const auto &r = dungeon.rooms[i];
        const float x =
            arranged ? (i % gridColumns + .5f) / gridColumns : std::clamp((r.mapX + .5f) / columns, .1f, .9f);
        const float y = arranged ? (i / gridColumns + .5f) / gridRows : std::clamp((r.mapY + .5f) / rows, .13f, .87f);
        centres.push_back({origin.x + x * width, origin.y + y * height});
    }
    draw->AddRectFilled(origin, {origin.x + width, origin.y + height}, ImGui::GetColorU32(ImGuiCol_FrameBg), 4.f);
    draw->PushClipRect(origin, {origin.x + width, origin.y + height}, true);
    for (std::size_t i = 0; i < dungeon.rooms.size(); ++i)
        for (const auto &connection : dungeon.rooms[i].connections)
            for (std::size_t j = 0; j < dungeon.rooms.size(); ++j)
                if (connection.targetRoomId == dungeon.rooms[j].metadata.id)
                    draw->AddLine(centres[i], centres[j],
                                  ImGui::GetColorU32(connection.secret ? ImGuiCol_TextDisabled : ImGuiCol_Border), 2.f);
    const float half = std::min(52.f, width * .18f);
    for (std::size_t i = 0; i < centres.size(); ++i)
    {
        const auto c = centres[i];
        const bool active = dungeon.rooms[i].metadata.id == selected;
        draw->AddRectFilled({c.x - half, c.y - 19}, {c.x + half, c.y + 19},
                            ImGui::GetColorU32(active ? ImGuiCol_HeaderActive : ImGuiCol_Button), 4.f);
        draw->AddRect({c.x - half, c.y - 19}, {c.x + half, c.y + 19},
                      ImGui::GetColorU32(active ? ImGuiCol_NavCursor : ImGuiCol_Border), 4.f, 0, 1.5f);
        draw->PushClipRect({c.x - half + 4, c.y - 16}, {c.x + half - 4, c.y + 16}, true);
        draw->AddText({c.x - half + 5, c.y - ImGui::GetFontSize() * .5f}, ImGui::GetColorU32(ImGuiCol_Text),
                      dungeon.rooms[i].name.c_str());
        draw->PopClipRect();
    }
    draw->PopClipRect();
    draw->AddRect(origin, {origin.x + width, origin.y + height}, ImGui::GetColorU32(ImGuiCol_Border), 4.f);
    ImGui::InvisibleButton("RoomDiagramCanvas", {width, height});
    bool modified = false;
    if (ImGui::IsItemClicked())
    {
        const auto mouse = ImGui::GetMousePos();
        if (placing)
        {
            const auto chosen = std::find_if(dungeon.rooms.begin(), dungeon.rooms.end(),
                                             [&](const auto &room) { return room.metadata.id == selected; });
            if (chosen != dungeon.rooms.end())
            {
                if (arranged)
                    for (std::size_t i = 0; i < dungeon.rooms.size(); ++i)
                    {
                        dungeon.rooms[i].mapX =
                            std::clamp(int((centres[i].x - origin.x) / width * columns), 0, columns - 1);
                        dungeon.rooms[i].mapY = std::clamp(int((centres[i].y - origin.y) / height * rows), 0, rows - 1);
                    }
                chosen->mapX = std::clamp(int((mouse.x - origin.x) / width * columns), 0, columns - 1);
                chosen->mapY = std::clamp(int((mouse.y - origin.y) / height * rows), 0, rows - 1);
                placing = false;
                modified = true;
            }
        }
        else
            for (std::size_t i = 0; i < centres.size(); ++i)
                if (std::abs(mouse.x - centres[i].x) <= half && std::abs(mouse.y - centres[i].y) <= 19)
                    selected = dungeon.rooms[i].metadata.id;
    }
    if (ImGui::Button(placing ? uiTextForLocale("Cancel placement", "Platzieren abbrechen")
                              : uiTextForLocale("Place selected room", "Gewählten Raum platzieren")))
        placing = !placing;
    if (placing)
        ImGui::TextWrapped("%s", uiTextForLocale("Click the diagram, then save the dungeon layout.",
                                                 "Klicke in die Übersicht und speichere dann den Dungeon-Grundriss."));
    return modified;
}
} // namespace RoomDiagramUi
