#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "Views/Editor/Widgets/Map/MapCanvasInteraction.hpp"
#include "Models/UX/DraftHistoryState.hpp"
#include "Views/Editor/Widgets/UiModalLayout.hpp"
#include <iostream>
#include <string>

namespace {
int failures = 0;
void check(bool value, const char *message) { if (!value) { ++failures; std::cerr << message << '\n'; } }
std::string text = "Mara reaches Havenfall with an unusual lantern and asks the ferryman about the old tower.\nGrüße aus dem Hafen. 🕯 Keep every space and newline.";
std::string clipboard;
ImGuiID inputId = 0;
ImVec2 origin;
float fieldWidth = 240;
float textScale = 1.0f;
void frame(bool focus = false, bool wrap = true) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({620,420});
    ImGui::Begin("Prose test", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowFontScale(textScale);
    origin = ImGui::GetCursorScreenPos();
    if (focus) ImGui::SetKeyboardFocusHere();
    ImGui::InputTextMultiline("##Notes", &text, {fieldWidth, 220}, wrap ? ImGuiInputTextFlags_WordWrap : 0);
    inputId = ImGui::GetItemID();
    ImGui::End(); ImGui::Render();
}
void key(ImGuiKey key, bool ctrl = false) {
    auto &io = ImGui::GetIO(); io.AddKeyEvent(ImGuiMod_Ctrl, ctrl); io.AddKeyEvent(key, true); frame();
    io.AddKeyEvent(key, false); io.AddKeyEvent(ImGuiMod_Ctrl, false); frame();
}
}
int main() {
    ImGui::CreateContext(); auto &io = ImGui::GetIO();
    ImGui::GetPlatformIO().Platform_GetClipboardTextFn = [](ImGuiContext *) { return clipboard.c_str(); };
    ImGui::GetPlatformIO().Platform_SetClipboardTextFn = [](ImGuiContext *, const char *value) { clipboard = value; };
    io.IniFilename = nullptr; io.DisplaySize = {640,480}; io.DeltaTime = 1.0f/60;
    io.ConfigInputTrickleEventQueue = false;
    unsigned char *pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    const auto original = text;
    frame(); frame(true); frame();
    auto *state = ImGui::GetInputTextState(inputId);
    check(state && state->WrapWidth > 0, "Prose input did not activate wrapped row layout");
    if (state) {
        state->ReloadUserBufAndMoveToEnd(); frame(); key(ImGuiKey_Home, true);
        key(ImGuiKey_DownArrow);
        check(state->GetCursorPos() > 0 && state->GetCursorPos() < static_cast<int>(original.find('\n')),
              "Down must move by a visual row inside the first paragraph");
        key(ImGuiKey_UpArrow);
        check(state->GetCursorPos() == 0, "Up must return to the previous wrapped row");
        io.AddMousePosEvent(origin.x+5, origin.y+ImGui::GetStyle().FramePadding.y+io.Fonts->Fonts[0]->LegacySize*1.5f);
        io.AddMouseButtonEvent(0,true); frame(); io.AddMouseButtonEvent(0,false); frame();
        check(state->GetCursorPos() > 0 && state->GetCursorPos() < static_cast<int>(original.find('\n')),
              "Clicking the second visual row must address that row in the original text");
        key(ImGuiKey_A, true);
        check(state->GetSelectionStart() == 0 && state->GetSelectionEnd() == static_cast<int>(original.size()), "Select all must include the unchanged UTF-8 buffer");
        key(ImGuiKey_C, true);
        check(clipboard == original, "Copying a wrapped selection must retain the original text without soft newlines");
        clipboard = "Neue Szene 🕯"; key(ImGuiKey_V, true);
        check(text == "Neue Szene 🕯", "Replacing wrapped selection corrupted UTF-8 text");
        key(ImGuiKey_Z, true);
        // Upstream STB records replacing a selection as delete + paste. Keep
        // that behavior and verify both undo records preserve the source bytes.
        check(text.empty(), "The first paste undo must remove the inserted text");
        key(ImGuiKey_Z, true);
        check(text == original, "Undo must restore exact spaces, UTF-8 and hard newlines");
        fieldWidth = 150; frame(); fieldWidth = 410; frame();
        check(text == original && state->Scroll.x == 0, "Resizing wrapped input changed text or retained horizontal scroll");
    }
    text = "A line\n\n"; if(state)state->ReloadUserBufAndMoveToEnd(); frame();
    check(text == "A line\n\n", "Trailing blank lines were changed by visual layout");
    frame(false,false);
    check(!state || state->WrapWidth == 0, "Unwrapped inputs must retain upstream row behavior");
    textScale = 1.5f; text = original; if(state)state->ReloadUserBufAndMoveToEnd(); frame();
    check(text == original && state && state->WrapWidth > 0, "Large-text reflow changed the stored text");
    float zoom = 1;
    float outerScroll = 0;
    const auto canvasFrame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({10,10}); ImGui::SetNextWindowSize({500,350});
        ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::BeginChild("Map", {450,280}, false, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_HorizontalScrollbar);
        if (io.KeyCtrl) applyMapCanvasWheelZoom(zoom,.2f,3);
        ImGui::InvisibleButton("MapHitbox", {1000,1000});
        ImGui::EndChild();
        ImGui::Dummy({20,1000});
        outerScroll = ImGui::GetScrollY();
        ImGui::End(); ImGui::Render();
    };
    io.AddMousePosEvent(100,100); canvasFrame(); canvasFrame();
    io.AddKeyEvent(ImGuiMod_Ctrl, true); canvasFrame();
    io.AddMouseWheelEvent(0,1); canvasFrame(); canvasFrame();
    check(zoom > 1, "Ctrl+wheel over the map did not zoom it");
    check(outerScroll == 0, "Ctrl+wheel zoom also scrolled the editor page");
    io.AddKeyEvent(ImGuiMod_Ctrl, false); canvasFrame();
    const float zoomBeforePlainWheel = zoom;
    io.AddMouseWheelEvent(0,-1); canvasFrame();
    canvasFrame();
    check(zoom == zoomBeforePlainWheel, "Plain wheel over the map unexpectedly changed its zoom");
    check(outerScroll > 0, "Plain wheel over the map did not scroll the editor page");
    const float hoveredZoom = zoom;
    io.AddMousePosEvent(100,330); canvasFrame(); canvasFrame();
    io.AddMouseWheelEvent(0,1); canvasFrame();
    check(zoom == hoveredZoom, "Wheel outside the canvas changed its zoom");
    bool dragging = false, sawReleasePosition = false; ImVec2 token(100,100);
    const auto dragFrame = [&] {
        ImGui::NewFrame();
        if (ImGui::IsMouseClicked(0)) dragging = true;
        if (dragging && mapCanvasDragPositionAvailable()) { token = io.MousePos; sawReleasePosition |= ImGui::IsMouseReleased(0); }
        if (ImGui::IsMouseReleased(0)) dragging = false;
        ImGui::EndFrame();
    };
    io.ConfigInputTrickleEventQueue = true;
    io.AddMousePosEvent(100,100); dragFrame();
    io.AddMouseButtonEvent(0,true); io.AddMousePosEvent(180,140); io.AddMouseButtonEvent(0,false);
    dragFrame(); dragFrame();
    check(sawReleasePosition && token.x == 180 && token.y == 140 && !dragging, "Combat drag dropped the position delivered with button release");
    DraftHistoryState history;
    history.edited(); const auto first = history.current(); history.saved();
    history.edited(); const auto second = history.current(); history.saved();
    history.edited(); history.restore(second);
    check(!history.dirty(), "Undo to a saved node must be clean");
    history.restore(first); check(history.dirty(), "Undo past the last save must be dirty");
    history.restore(second); check(!history.dirty(), "Redo to the saved node must be clean");
    history.restore(first); history.edited(); check(history.dirty() && history.current() != second, "A new history branch must not reuse a saved identity");
    // Exercise the production modal helper with German prose at both target
    // viewport sizes; pure width arithmetic would miss ImGui auto-fit behavior.
    for (const ImVec2 display : {ImVec2(2560,1440), ImVec2(1280,720), ImVec2(480,320)})
    {
        io.DisplaySize = display;
        ImVec2 modalPos, modalSize;
        float expectedWidth = 0.0f;
        for (int pass = 0; pass < 4; ++pass)
        {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(display);
            ImGui::Begin("Modal host", nullptr, ImGuiWindowFlags_NoDecoration);
            ImGui::SetWindowFontScale(1.5f);
            expectedWidth = std::min(ImGui::GetFontSize()*38.0f, display.x*0.90f);
            if (pass == 0) ImGui::OpenPopup("Kampfpause prüfen");
            if (beginBoundedConfirmationModal("Kampfpause prüfen"))
            {
                ImGui::TextWrapped("Kampf pausieren und den Sitzungsfortschritt speichern. Die Hintergrundwiedergabe bleibt unverändert, bis die Spielleitung ausdrücklich eine Audiopause auswählt.");
                ImGui::Button("Kampf pausieren");
                modalPos = ImGui::GetWindowPos(); modalSize = ImGui::GetWindowSize();
                if (pass == 3) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            ImGui::End(); ImGui::Render();
        }
        check(std::abs(modalSize.x-expectedWidth)<2.0f, "Wrapped confirmation lost its font-scaled width during auto-fit");
        check(modalPos.x >= 0 && modalPos.y >= 0 && modalPos.x+modalSize.x <= display.x+1 && modalPos.y+modalSize.y <= display.y+1,
              "Confirmation escaped the available viewport");
        check(std::abs(modalPos.x+modalSize.x*.5f-display.x*.5f)<2 && std::abs(modalPos.y+modalSize.y*.5f-display.y*.5f)<2,
              "Confirmation was not centered after auto-fit");
    }
    io.DisplaySize = {640,480};
    float visibleActionY = 0, hiddenActionY = 0;
    for (int pass = 0; pass < 2; ++pass)
    {
        ImGui::NewFrame(); ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({400,300});
        ImGui::Begin("Stable combat status");
        drawStableTransientStatus("status", "Gespeichert. Ein langer Hinweis darf die Schaltflächen beim Ausblenden nicht verschieben.", pass == 0);
        const float actionY = ImGui::GetCursorScreenPos().y;
        if (pass == 0) visibleActionY = actionY; else hiddenActionY = actionY;
        ImGui::Button("Kampf fortsetzen"); ImGui::End(); ImGui::Render();
    }
    check(visibleActionY == hiddenActionY, "Expiring status moved the combat action under the pointer");
    ImGui::DestroyContext();
    return failures ? 1 : 0;
}
