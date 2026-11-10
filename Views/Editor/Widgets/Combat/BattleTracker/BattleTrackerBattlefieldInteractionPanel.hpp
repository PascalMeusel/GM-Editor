        if (insideGrid)
        {
            const ImVec2 min(origin.x + x * cellSize, origin.y + y * cellSize);
            const ImVec2 max(min.x + cellSize, min.y + cellSize);
            drawList->AddRectFilled(min, max, canMove ? IM_COL32(98, 204, 150, 48) : IM_COL32(224, 92, 82, 55));
            drawList->AddRect(min, max, canMove ? IM_COL32(98, 235, 170, 235) : IM_COL32(235, 96, 88, 235), 0.0f, 0, 3.0f);
            const ImVec2 start = cellCenter(origin, cellSize, moving.turnStartX, moving.turnStartY);
            const ImVec2 destination = cellCenter(origin, cellSize, x, y);
            drawList->AddLine(start, destination, canMove ? IM_COL32(120, 235, 175, 210) : IM_COL32(235, 96, 88, 210), 2.5f);
        }

        const ImVec2 textSize = ImGui::CalcTextSize(moveText.c_str());
        const ImVec2 textMin(origin.x + 10.0f, origin.y + 10.0f);
        const ImVec2 textMax(textMin.x + textSize.x + 16.0f, textMin.y + textSize.y + 10.0f);
        drawList->AddRectFilled(textMin, textMax, IM_COL32(20, 24, 28, 230), 5.0f);
        drawList->AddRect(textMin, textMax, canMove ? IM_COL32(98, 204, 150, 210) : IM_COL32(224, 92, 82, 220), 5.0f);
        drawList->AddText(ImVec2(textMin.x + 8.0f, textMin.y + 5.0f), IM_COL32(240, 244, 246, 255), moveText.c_str());
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        int hitParticipant = -1;
        for (int i = static_cast<int>(encounter.participants.size()) - 1; i >= 0; --i)
        {
            const EncounterParticipantModel &participant = encounter.participants[i];
            const ImVec2 center(origin.x + (participant.x + 0.5f) * cellSize, origin.y + (participant.y + 0.5f) * cellSize);
            if (pointInsideCircle(mouse, center, cellSize * 0.42f))
            {
                hitParticipant = i;
                break;
            }
        }
        if (hitParticipant >= 0)
            _controller.beginParticipantDrag(hitParticipant);
    }

    if (_controller._dragParticipantIndex >= 0 &&
        mapCanvasDragPositionAvailable())
    {
        const int x = static_cast<int>((mouse.x - origin.x) / cellSize);
        const int y = static_cast<int>((mouse.y - origin.y) / cellSize);
        _controller.moveParticipant(_controller._dragParticipantIndex, x, y);
    }

    if (_controller._dragParticipantIndex >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        _controller.endParticipantDrag();

    if (hovered)
    {
        const int x = static_cast<int>((mouse.x - origin.x) / cellSize);
        const int y = static_cast<int>((mouse.y - origin.y) / cellSize);
        if (isEncounterCellInside(encounter, x, y))
        {
            const ImVec2 min(origin.x + x * cellSize, origin.y + y * cellSize);
            drawList->AddRect(min, ImVec2(min.x + cellSize, min.y + cellSize), IM_COL32(255, 255, 255, 180), 0.0f, 0, 2.0f);
            ImGui::BeginTooltip();
            ImGui::Text("%d, %d", x, y);
            for (const auto &participant : encounter.participants)
                if (participant.x == x && participant.y == y)
                {
                    ImGui::TextUnformatted(participant.displayName.c_str());
                    ImGui::Text(localizedUiText("%d/%d HP"), participant.currentHitPoints, participant.maxHitPoints);
                }
            ImGui::EndTooltip();
        }
    }

    ImGui::EndChild();
