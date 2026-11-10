// Actual production Edit workspace at the observed 1282x752 native window's
// approximately 1280x720 client area, with
// application geometry and 18px base text at 150%; no graphics backend.
void mapCanvasEnlargedLayoutCase(UiInteractionTrace &trace, bool german)
{
    const ImGuiStyle previousStyle = ImGui::GetStyle();
    const ImVec2 previousSize = ImGui::GetIO().DisplaySize;
    struct Restore {
        ImGuiStyle style; ImVec2 size;
        ~Restore() { ImGui::GetStyle() = style; ImGui::GetIO().DisplaySize = size; }
    } restore{previousStyle, previousSize};
    gm::approved_ui::applyGeometry(false);
    ImGui::GetStyle().FontSizeBase = 18.0f;
    ImGui::GetStyle().FontScaleMain = 1.5f;
    ImGui::GetIO().DisplaySize = ImVec2(1280, 720);
    EditorWindowController editor; editor.setWindowOpacity(true);
    Interaction ui(editor, trace);
    CanvasGestureFixture fixture(editor, ui, german, false, false, true);
    const auto geometry = [&] {
        const auto hitbox = ui.locate("MapCreatorCanvasHitbox");
        auto *canvas = ImGui::FindWindowByName(hitbox.window.c_str());
        ImGuiWindow *rail = nullptr;
        for (auto *window : ImGui::GetCurrentContext()->Windows)
            if (window->Active && std::string(window->Name).find("MapCreatorControlsScroll") != std::string::npos)
                rail = window;
        require(canvas && rail, "Enlarged Edit workspace did not render its canvas and inspector");
        const auto c = canvas->InnerClipRect, r = rail->InnerClipRect;
        const std::string diagnostic = " canvas=" + canvasPoint(c.Min) + ".." + canvasPoint(c.Max) +
            " rail=" + canvasPoint(r.Min) + ".." + canvasPoint(r.Max) +
            " renderedFonts=" + std::to_string(canvas->FontRefSize) + "," + std::to_string(rail->FontRefSize) +
            canvasObserved(editor);
        require(canvasNear(canvas->FontRefSize, 27.0f) && canvasNear(rail->FontRefSize, 27.0f),
                "Fixture did not reach actual 150% application text" + diagnostic);
        require(r.Max.x <= c.Min.x && r.GetWidth() >= 240 && r.GetHeight() >= 240 &&
                    c.GetWidth() >= 420 && c.GetHeight() >= 200,
                "Enlarged Edit workspace must retain useful side-by-side inspector and map areas" + diagnostic);
        require(c.Max.y <= ImGui::GetMainViewport()->WorkPos.y + ImGui::GetMainViewport()->WorkSize.y &&
                    r.Max.y <= ImGui::GetMainViewport()->WorkPos.y + ImGui::GetMainViewport()->WorkSize.y,
                "Enlarged Edit workspace extends below its viewport" + diagnostic);
        return c;
    };
    const ImRect initial = geometry();
    // Use the independent inspector scroll route to reach its page controls;
    // long enlarged labels must not make those controls merely present in data.
    // At 27px four wheel notches can jump more than a viewport and miss the
    // whole nested page table. Fine wheel input searches overlapping regions.
    ui.click(german ? "Karteninhalt" : "Map contents", "MapCreatorControlsScroll", false, 0.5f);
    ui.click(german ? "Werkzeug" : "Tool settings", "MapCreatorControlsScroll", false, 0.5f);
    geometry();
    CanvasScreenGesture screen(editor, ui, false, true);
    auto &io = ImGui::GetIO();
    io.AddMouseButtonEvent(0, true); ui.frame(); screen.stable(editor);
    io.AddMousePosEvent(screen.end.x, screen.end.y); ui.frame(2); screen.stable(editor);
    io.AddMouseButtonEvent(0, false); ui.frame(); screen.stable(editor);
    const float delta = (screen.end.x - screen.start.x) / screen.cell;
    canvasPosition(editor, false, false, delta);
    require(CanvasProbe::mapUndoDepth(editor) == 1 && CanvasProbe::inspector(editor),
            "Enlarged Edit drag must select the object and create exactly one undo step");
    ui.frame(3);
    const ImRect selected = geometry();
    require(canvasNear(initial.Min, selected.Min) && canvasNear(initial.Max, selected.Max),
            "Opening the enlarged Edit inspector displaced the canvas");
    const auto text = [](const char *label) { return std::string(gm::localization::trUi(label)); };
    ui.click(text("Undo")); canvasPosition(editor, false, false, 0);
    ui.click(text("Redo")); canvasPosition(editor, false, false, delta);
    ui.click(text("Save Map"));
    const auto saved = readCampaign(CanvasProbe::liveCampaign(editor).metadata.id);
    require(!CanvasProbe::map(editor).hasUnsavedDraft() && saved.maps.front().metadata.id == fixture.mapId &&
                canvasNear(saved.maps.front().tokens.front().x, 3 + delta) &&
                canvasNear(saved.maps.front().tokens.front().y, 2),
            "Enlarged Edit Save Map did not persist the actual dragged position");
    geometry();
    ui.click(german ? "Spielervorschau" : "Player preview");
    ui.click(german ? "Bearbeiten" : "Edit"); ui.frame(3);
    geometry();
    require(CanvasProbe::mapDraft(editor).metadata.id == fixture.mapId,
            "Preview/Edit navigation changed the current map");
    std::cout << "Enlarged map layout:1280x720 client,27px text, rail+canvas geometry, drag/undo/redo/save/navigation passed\n";
}
