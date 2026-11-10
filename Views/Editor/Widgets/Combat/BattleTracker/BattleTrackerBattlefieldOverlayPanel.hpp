
    if (_controller._combatStep == 1 && !encounter.participants.empty())
    {
        const EncounterParticipantModel &active = encounter.participants[encounter.activeParticipantIndex];
        const ImVec2 start(origin.x + (active.turnStartX + 0.5f) * cellSize, origin.y + (active.turnStartY + 0.5f) * cellSize);
        const ImVec2 end(origin.x + (active.x + 0.5f) * cellSize, origin.y + (active.y + 0.5f) * cellSize);
        drawList->AddCircleFilled(start, cellSize * 0.13f, IM_COL32(255, 245, 170, 235), 18);
        drawList->AddCircle(start, cellSize * 0.18f, IM_COL32(30, 34, 38, 255), 18, 2.0f);

        if (active.turnStartX != active.x || active.turnStartY != active.y)
        {
            drawList->AddLine(start, end, IM_COL32(255, 225, 96, 245), 4.0f);
            const float angle = std::atan2(end.y - start.y, end.x - start.x);
            const float arrowLength = std::max(8.0f, cellSize * 0.24f);
            const float arrowWidth = std::max(5.0f, cellSize * 0.16f);
            const ImVec2 tip(end.x - std::cos(angle) * cellSize * 0.28f, end.y - std::sin(angle) * cellSize * 0.28f);
            const ImVec2 back(tip.x - std::cos(angle) * arrowLength, tip.y - std::sin(angle) * arrowLength);
            const ImVec2 left(back.x + std::cos(angle + 1.5708f) * arrowWidth, back.y + std::sin(angle + 1.5708f) * arrowWidth);
            const ImVec2 right(back.x + std::cos(angle - 1.5708f) * arrowWidth, back.y + std::sin(angle - 1.5708f) * arrowWidth);
            drawList->AddTriangleFilled(tip, left, right, IM_COL32(255, 225, 96, 245));
        }

        if (_controller._selectedParticipantIndex >= 0 && _controller._selectedParticipantIndex < static_cast<int>(encounter.participants.size()))
        {
            const EncounterParticipantModel &attacker = encounter.participants[_controller._selectedParticipantIndex];
            const MonsterActionModel selectedAction = _controller.getSelectedAction();
            const int actionRangeFeet = std::max(selectedAction.reachFeet, selectedAction.rangeFeet);
            if (actionRangeFeet > 0)
            {
                const float rangeRadius = (static_cast<float>(actionRangeFeet) / static_cast<float>(std::max(1, encounter.cellSizeFeet))) * cellSize;
                const ImVec2 attackerCenter = cellCenter(origin, cellSize, attacker.x, attacker.y);
                const ImU32 rangeFill = selectedAction.sourceSpellId.empty() ? IM_COL32(96, 178, 255, 30) : IM_COL32(174, 126, 255, 34);
                const ImU32 rangeStroke = selectedAction.sourceSpellId.empty() ? IM_COL32(96, 178, 255, 150) : IM_COL32(174, 126, 255, 160);
                drawList->AddCircleFilled(attackerCenter, rangeRadius, rangeFill, 96);
                drawList->AddCircle(attackerCenter, rangeRadius, rangeStroke, 96, 2.0f);
                if (selectedAction.reachFeet > 0 && selectedAction.reachFeet < actionRangeFeet)
                {
                    const float reachRadius = (static_cast<float>(selectedAction.reachFeet) / static_cast<float>(std::max(1, encounter.cellSizeFeet))) * cellSize;
                    drawList->AddCircle(attackerCenter, reachRadius, IM_COL32(255, 245, 160, 180), 64, 2.0f);
                }

                if (_controller._rangeTargetIndex >= 0 && _controller._rangeTargetIndex < static_cast<int>(encounter.participants.size()) && _controller._rangeTargetIndex != _controller._selectedParticipantIndex)
                {
                    const EncounterParticipantModel &target = encounter.participants[_controller._rangeTargetIndex];
                    const ImVec2 targetCenter = cellCenter(origin, cellSize, target.x, target.y);
                    const bool targetInRange = _controller.distanceBetweenParticipants(_controller._selectedParticipantIndex, _controller._rangeTargetIndex) <= actionRangeFeet;
                    drawList->AddLine(attackerCenter, targetCenter, targetInRange ? IM_COL32(120, 235, 175, 235) : IM_COL32(235, 96, 88, 235), 3.0f);
                    drawList->AddCircle(targetCenter, cellSize * 0.46f, targetInRange ? IM_COL32(120, 235, 175, 220) : IM_COL32(235, 96, 88, 220), 32, 3.0f);
                }
            }

            RuleSpellResolutionSummary spellSummary;
            if (_controller.getSelectedSpellResolutionSummary(spellSummary) && !spellSummary.area.shape.empty())
            {
                const EncounterParticipantModel *target = nullptr;
                if (_controller._rangeTargetIndex >= 0 && _controller._rangeTargetIndex < static_cast<int>(encounter.participants.size()))
                    target = &encounter.participants[_controller._rangeTargetIndex];
                drawAreaTemplate(drawList, spellSummary.area, origin, cellSize, encounter.cellSizeFeet, attacker, target);
            }
        }
    }

    for (int i = 0; i < static_cast<int>(encounter.participants.size()); ++i)
    {
        const EncounterParticipantModel &participant = encounter.participants[i];
        const ImVec2 center(origin.x + (participant.x + 0.5f) * cellSize, origin.y + (participant.y + 0.5f) * cellSize);
        const float radius = cellSize * 0.34f;
        drawList->AddCircleFilled(center, radius, _controller.participantColor(participant), 28);
        const ImU32 ringColor = i == encounter.activeParticipantIndex ? IM_COL32(255, 234, 126, 255) : (_controller._selectedParticipantIndex == i ? IM_COL32(255, 255, 255, 255) : IM_COL32(42, 48, 54, 255));
        drawList->AddCircle(center, radius + std::clamp(cellSize * 0.06f, 0.4f, 2.0f), ringColor, 28,
            i == encounter.activeParticipantIndex ? std::clamp(cellSize * 0.10f, 0.8f, 3.0f) : std::clamp(cellSize * 0.075f, 0.6f, 2.0f));
        const std::string label = tokenLabel(participant.displayName);
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        if (textSize.x <= radius * 2.0f && textSize.y <= radius * 2.0f)
            drawList->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f), IM_COL32(12, 15, 18, 255), label.c_str());
    }

    ImGui::InvisibleButton("BattlefieldHitbox", ImVec2(cellSize * encounter.battlefieldWidth, cellSize * encounter.battlefieldHeight), ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mouse = ImGui::GetIO().MousePos;

    if (_controller._dragParticipantIndex >= 0 && _controller._dragParticipantIndex < static_cast<int>(encounter.participants.size()))
    {
        const int x = static_cast<int>((mouse.x - origin.x) / cellSize);
        const int y = static_cast<int>((mouse.y - origin.y) / cellSize);
        EncounterParticipantModel &moving = encounter.participants[_controller._dragParticipantIndex];
        const bool insideGrid = isEncounterCellInside(encounter, x, y);
        const bool canMove = insideGrid && _controller.canParticipantMoveTo(_controller._dragParticipantIndex, x, y);
        std::string moveText = battleFormat("moving: {0}", {moving.displayName});
        if (!insideGrid)
            moveText += " | " + battleText("outside battlefield");
        else if (_controller._combatStep != 1)
            moveText += " | " + battleText(encounter.combatPaused ? "paused: enable Free Placement to reposition" : "setup placement");
        else if (_controller._freePlacementMode)
            moveText += " | " + battleText("Free Placement override");
        else if (_controller._dragParticipantIndex != encounter.activeParticipantIndex)
            moveText += " | " + battleText("blocked: only the active combatant moves");
        else
            moveText += " | " + movementContextText(encounter, moving, x, y);
