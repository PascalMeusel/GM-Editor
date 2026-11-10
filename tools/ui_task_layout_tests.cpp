#include "Views/Editor/Widgets/UiTaskLayout.hpp"
#include "Views/Editor/Widgets/UiTooltipPolicy.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapAuthoringWorkspaceContract.hpp"
#include "imgui_internal.h"
#include <iostream>

int main()
{
    int failures = 0;
    const auto check = [&](bool ok, const char *message) { if (!ok) { ++failures; std::cerr << message << '\n'; } };
    ImGui::CreateContext();
    auto &io = ImGui::GetIO(); io.IniFilename = nullptr; io.DeltaTime = 1.0f / 60.0f;
    unsigned char *pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    for (const float scale : {1.0f, 1.5f, 2.0f})
    for (const float windowWidth : {360.0f, 640.0f, 1280.0f})
    {
        io.DisplaySize = {windowWidth, 720.0f};
        const float mapRail=gm::mapui::mapCreatorEditingRailWidth(windowWidth,16.f*scale);
        check(mapRail<=windowWidth*.34f+.1f,"Map inspector takes more than one third of the workspace");
        check(mapRail<=300.f*scale+.1f,"Map inspector grows beyond its readable control width");
        for (int frame = 0; frame < 3; ++frame)
        {
            ImGui::NewFrame(); ImGui::SetNextWindowPos({0, 0}); ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Task layouts", nullptr, ImGuiWindowFlags_NoDecoration);
            ImGui::SetWindowFontScale(scale);
            const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            const float modePairMinimum = TaskUi::equalColumnButtonsMinimumWidth(
                "Vorbereiten", "Live leiten", 10.0f);
            check(TaskUi::equalColumnButtonsFit(
                      "Vorbereiten", "Live leiten", 10.0f, modePairMinimum),
                  "Navigation pair must stay inline when both localized labels fit");
            check(!TaskUi::equalColumnButtonsFit(
                      "Vorbereiten", "Live leiten", 10.0f, modePairMinimum - 1.0f),
                  "Navigation pair must stack before localized labels overflow");
            // German action strings, actual measured glyph widths, and a real
            // narrow child reproduce the source of the participant clipping.
            TaskUi::button("Spieler hinzufuegen");
            TaskUi::sameLineForButton("Gesamte Gruppe hinzufuegen");
            TaskUi::button("Gesamte Gruppe hinzufuegen");
            check(ImGui::GetItemRectMax().x <= right + 0.5f, "Wrapped German action exceeds content width");
            TaskUi::fieldLabel("A long translated field label must leave a usable full width input");
            char text[32] = "Draft retained"; ImGui::InputText("##Name", text, sizeof(text));
            check(ImGui::GetItemRectMax().x <= right + 0.5f, "Full width field exceeds content width");
            check(TaskUi::listHeight() < 400.0f, "List consumes the complete narrow page");
            ImGui::BeginChild("Inspector", {280, 120});
            check(!TaskUi::hasRoomForColumns(), "Narrow inspector must not use desktop two-column layout");
            ImGui::EndChild();
            if (frame == 0) ImGui::OpenPopup("Participant picker");
            if (ImGui::IsPopupOpen("Participant picker")) TaskUi::preparePickerWindow();
            if (ImGui::BeginPopup("Participant picker"))
            {
                ImGui::SetWindowFontScale(scale);
                for (const char *label : {"Spieler hinzufuegen", "Gesamte Gruppe hinzufuegen", "NSC hinzufuegen", "Monster hinzufuegen"})
                    TaskUi::button(label, {-1, 0});
                ImGui::TextWrapped("RK 12  TP 11  Bewegungsrate 30 Fuss  Herausforderungsgrad 1/8");
                if (frame > 0)
                {
                    auto *popup = ImGui::GetCurrentWindow();
                    check(popup->Size.x <= windowWidth * 0.9f + 1, "Picker exceeds viewport width");
                    check(popup->Size.y <= 720 * 0.85f + 1, "Picker exceeds viewport height");
                    check(popup->ScrollMax.x == 0, "Picker creates clipped horizontal content");
                }
                ImGui::EndPopup();
            }
            ImGui::End(); ImGui::Render();
        }
    }
    ImGui::DestroyContext();
    check(UiTooltipPolicy::shouldShow(false, true, true),
          "A hovered inactive item must show its tooltip");
    check(!UiTooltipPolicy::shouldShow(false, false, true),
          "A tooltip must close when the pointer leaves the application");
    check(!UiTooltipPolicy::shouldShow(false, true, false),
          "Mouse-acquired focus must not keep a tooltip open after hover ends");
    check(!UiTooltipPolicy::shouldShow(true, true, true),
          "An active editor control must not be obstructed by its tooltip");
    // A focused map editor must beat the campaign fallback, including when
    // its canvas child owns focus. A different window must retain global undo.
    for (const bool mapFocused : {true, false})
    for (const ImGuiKey key : {ImGuiKey_Z, ImGuiKey_Y})
    {
        ImGui::CreateContext();
        auto &input = ImGui::GetIO();
        input.IniFilename = nullptr; input.DisplaySize = {1280, 720};
        input.DeltaTime = 1.0f / 60.0f;
        input.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        bool globalTriggered = false, mapTriggered = false;
        for (int frame = 0; frame < 6; ++frame)
        {
            if (frame == 3) input.AddKeyEvent(ImGuiMod_Ctrl, true);
            if (frame == 4) input.AddKeyEvent(key, true);
            ImGui::NewFrame();
            globalTriggered |= ImGui::Shortcut(ImGuiMod_Ctrl | key, ImGuiInputFlags_RouteGlobal);
            ImGui::Begin("Map editor");
            mapTriggered |= ImGui::Shortcut(ImGuiMod_Ctrl | key);
            ImGui::BeginChild("Canvas", {400, 300});
            if (frame == 1 && mapFocused) ImGui::SetWindowFocus();
            ImGui::EndChild(); ImGui::End();
            ImGui::Begin("Campaign");
            if (frame == 1 && !mapFocused) ImGui::SetWindowFocus();
            ImGui::End(); ImGui::Render();
        }
        check(mapTriggered == mapFocused, "Undo/redo must route to focused map or its canvas child");
        check(globalTriggered != mapFocused, "Campaign and map histories must never consume the same shortcut");
        ImGui::DestroyContext();
    }
    std::cout << "Task layout failures: " << failures << '\n';
    return failures ? 1 : 0;
}
