// CPU ImGui only. These cases render the real editor canvas and feed ordinary
// mouse/key events; no Application, GLFW, graphics backend or clipboard use,
// and no audio assets or playback.
using CanvasProbe = WorkspaceInteractionProbe;
bool canvasNear(float a, float b) { return std::fabs(a - b) < .001f; }
bool canvasNear(ImVec2 a, ImVec2 b) { return canvasNear(a.x, b.x) && canvasNear(a.y, b.y); }
std::string canvasPoint(ImVec2 p) { return "(" + std::to_string(p.x) + "," + std::to_string(p.y) + ")"; }
std::string canvasObserved(EditorWindowController &editor)
{
    return " origin=" + canvasPoint(CanvasProbe::canvasOrigin(editor)) + " cell=" + std::to_string(CanvasProbe::canvasCell(editor)) +
        " mouse=" + canvasPoint(ImGui::GetIO().MousePos) + " viewport=" + canvasPoint(ImGui::GetIO().DisplaySize) +
        " font=" + std::to_string(ImGui::GetFontSize()) + " inspector=" + std::to_string(CanvasProbe::inspector(editor));
}

struct CanvasGestureFixture
{
    EntityId mapId, alternateId;
    CanvasGestureFixture(EditorWindowController &editor, Interaction &ui, bool german, bool compact, bool locked = false,
                         bool grid = false)
    {
        Campaign campaign; campaign.name = "Synthetic canvas gesture fixture";
        campaign.ruleSet = std::make_shared<DND5e>();
        MapModel map; map.name = "Canvas gesture map"; map.width = 24; map.height = 20;
        map.scope = MapScope::Local; map.atlasLevel = MapLevel::Tactical;
        map.creationIntent = MapCreationIntent::TacticalEncounter;
        map.generation.enabled = true; map.generation.continuousRendering = !grid;
        map.generation.recipe.styleVersion = 1; map.generation.recipe.styleSnapshotId.clear();
        map.importedReference.enabled = false; map.backgroundImagePath.clear();
        resizeMapCells(map);
        for (auto &cell : map.cells) { cell.kind = MapTileKind::Dirt; cell.fogged = false; }
        MapTokenModel token; token.name = "Gesture token"; token.x = 3; token.y = 2;
        map.tokens.push_back(token);
        MapGeneratedFeature road; road.id = "gesture-road"; road.name = "Gesture road";
        road.kind = MapGeneratedFeatureKind::Road; road.points = {{7, 3}, {12, 3}}; road.locked = locked;
        map.generation.graph.features.push_back(road);
        MapModel alternate = map; alternate.metadata = EntityMetadata{}; alternate.name = "Other canvas";
        alternate.tokens.clear(); alternate.generation.graph.features.clear();
        mapId = map.metadata.id; alternateId = alternate.metadata.id; campaign.maps = {map, alternate};
        require(ProjectSaveService::getInstance().saveCampaign(campaign, "isolated canvas fixture"), "Cannot save canvas fixture");
        editor.setCurrentCampaign(campaign);
        require(editor.editWorkspaceMap(mapId), "Cannot open production canvas fixture");
        require(CanvasProbe::mapDraft(editor).metadata.id == mapId, "Opening fixture entered replacement review instead of loading its map");
        ImGui::GetIO().AddMousePosEvent(-100, -100);
        ImGui::GetIO().ConfigInputTrickleEventQueue = true;
        ui.frame(24); // Settle fit/layout and expire the previous fixture's double-click interval.
        require(!CanvasProbe::inspector(editor) && !CanvasProbe::pendingInspector(editor), "New map retained an inspector request");
        ui.locate(german ? "Änderungen speichern" : "Save changes"); // Reserved even while clean.
        // Prove that these cases reached the production stacked layout. A CLI
        // 'compact' setting alone does not guarantee the map's width breakpoint.
        if (compact) ui.locate(german ? "Werkzeuge" : "Tools");
    }
};

struct CanvasScreenGesture
{
    ImVec2 origin, start, end;
    float cell;
    CanvasScreenGesture(EditorWindowController &editor, Interaction &ui, bool feature, bool move)
    {
        origin = CanvasProbe::canvasOrigin(editor); cell = CanvasProbe::canvasCell(editor);
        require(cell > ImGui::GetIO().MouseDragThreshold, "Canvas fixture is too small to exercise a one-cell drag");
        const ImVec2 point = feature ? ImVec2(7, 3) : ImVec2(3.5f, 2.5f);
        // ImGui floors queued mouse coordinates just like its native backend.
        // Keep both pixel endpoints fixed; do not recompute from a later layout.
        start = {std::floor(origin.x + point.x * cell), std::floor(origin.y + point.y * cell)};
        end = {std::floor(start.x + (move ? cell : 0)), start.y};
        const auto hitbox = ui.locate("MapCreatorCanvasHitbox");
        auto *window = ImGui::FindWindowByName(hitbox.window.c_str());
        require(window && window->InnerClipRect.Contains(start) && window->InnerClipRect.Contains(end),
            "Canvas gesture endpoints are outside the observed production canvas clip");
        ImGui::GetIO().AddMousePosEvent(start.x, start.y); ui.frame(2);
        require(canvasNear(ImGui::GetIO().MousePos, start), "Canvas press sample differs from its fixed pixel endpoint");
        require(canvasNear(origin, CanvasProbe::canvasOrigin(editor)) && canvasNear(cell, CanvasProbe::canvasCell(editor)),
            "Canvas fixture moved before press");
    }
    void stable(EditorWindowController &editor) const
    {
        require(canvasNear(origin, CanvasProbe::canvasOrigin(editor)) && canvasNear(cell, CanvasProbe::canvasCell(editor)),
            "Inspector reflow changed canvas coordinates before the gesture finished; start=" + canvasPoint(start) +
            " end=" + canvasPoint(end) + " frozenOrigin=" + canvasPoint(origin) + " frozenCell=" + std::to_string(cell) + canvasObserved(editor));
    }
};

void canvasPosition(EditorWindowController &editor, bool feature, bool reshape, float delta, float deltaY = 0)
{
    const auto &map = CanvasProbe::mapDraft(editor);
    if (!feature)
        require(canvasNear(map.tokens.front().x, 3 + delta) && canvasNear(map.tokens.front().y, 2 + deltaY),
            "Production token drag endpoint: expected=" + canvasPoint({3 + delta, 2 + deltaY}) +
            " actual=" + canvasPoint({map.tokens.front().x, map.tokens.front().y}) + canvasObserved(editor));
    else
    {
        const auto &points = map.generation.graph.features.front().points;
        require(points.size() == 2, "Production feature drag changed its control point count");
        require(canvasNear(points.front().x, 7 + delta) && canvasNear(points.front().y, 3 + deltaY) &&
            canvasNear(points.back().x, 12 + (reshape ? 0 : delta)) && canvasNear(points.back().y, 3 + (reshape ? 0 : deltaY)),
            "Production feature drag endpoint: expected first=" + canvasPoint({7 + delta, 3 + deltaY}) +
            " actual first=" + canvasPoint({points.front().x, points.front().y}) +
            " actual last=" + canvasPoint({points.back().x, points.back().y}) + canvasObserved(editor));
    }
}

void canvasGesture(EditorWindowController &editor, Interaction &ui, bool german, bool compact,
                   int object, int gesture, bool leaveOpen = false)
{
    CanvasGestureFixture fixture(editor, ui, german, compact);
    const bool feature = object != 0, reshape = object == 2, move = gesture != 2;
    if (reshape || leaveOpen)
    {
        CanvasScreenGesture select(editor, ui, feature, false);
        ImGui::GetIO().AddMouseButtonEvent(0, true); ui.frame();
        ImGui::GetIO().AddMouseButtonEvent(0, false); ui.frame(3);
        if (reshape) ui.click(german ? "Umformen" : "Reshape", "MapCreatorControlsScroll");
        if (compact && !leaveOpen) ui.click(german ? "Werkzeuge" : "Tools");
        ui.frame(3);
    }
    const bool initiallyOpen = CanvasProbe::inspector(editor);
    const std::size_t undoBefore = CanvasProbe::mapUndoDepth(editor);
    const auto editsBefore = CanvasProbe::mapEditSerial(editor);
    CanvasScreenGesture screen(editor, ui, feature, move);
    const auto saveBefore = ui.locate(german ? "Änderungen speichern" : "Save changes");
    const float deltaX = !move ? 0 : reshape ? (screen.end.x - screen.origin.x) / screen.cell - 7 :
        (screen.end.x - screen.start.x) / screen.cell;
    const float deltaY = move && reshape ? (screen.end.y - screen.origin.y) / screen.cell - 3 : 0;
    auto &io = ImGui::GetIO();
    io.AddMouseButtonEvent(0, true);
    if (gesture == 1) { io.AddMousePosEvent(screen.end.x, screen.end.y); io.AddMouseButtonEvent(0, false); }
    ui.frame(); screen.stable(editor);
    require(CanvasProbe::pendingInspector(editor) && CanvasProbe::inspector(editor) == initiallyOpen,
        "Canvas press revealed the inspector before release");
    canvasPosition(editor, feature, reshape, 0);
    if (gesture != 1)
    {
        io.AddMousePosEvent(screen.end.x, screen.end.y); ui.frame(3); screen.stable(editor);
        require(CanvasProbe::pendingInspector(editor) && CanvasProbe::inspector(editor) == initiallyOpen,
            "Selection-summary refresh revealed the inspector during a held gesture");
        io.AddMouseButtonEvent(0, false);
    }
    ui.frame(); screen.stable(editor); // Release must still use the original transform.
    const auto saveAfter = ui.locate(german ? "Änderungen speichern" : "Save changes");
    require(canvasNear(saveBefore.minimum, saveAfter.minimum) && canvasNear(saveBefore.maximum, saveAfter.maximum),
        "Embedded Save action moved as the gesture changed draft status");
    require(canvasNear(io.MousePos, screen.end), "Canvas release sample differs from its fixed pixel endpoint");
    require(!CanvasProbe::pendingInspector(editor) && CanvasProbe::inspector(editor), "Release did not reveal selected object controls");
    canvasPosition(editor, feature, reshape, deltaX, deltaY);
    require(CanvasProbe::mapUndoDepth(editor) == undoBefore + (move ? 1 : 0),
        "Canvas gesture did not make exactly one undo step, or a selection click created history");
    if (!move) require(CanvasProbe::mapEditSerial(editor) == editsBefore, "Selection click marked the map edited");
    ui.frame(3);
    if (move)
    {
        CanvasProbe::mapUndo(editor); canvasPosition(editor, feature, reshape, 0);
        CanvasProbe::mapRedo(editor); canvasPosition(editor, feature, reshape, deltaX, deltaY);
    }
    // A deliberate collapse remains collapsed with the same selected object.
    if (compact)
    {
        ui.click(german ? "Werkzeuge" : "Tools"); ui.frame(4);
        require(!CanvasProbe::inspector(editor), "Selection summary defeated the explicit Tools collapse");
    }
}

void canvasRevealCancellation(EditorWindowController &editor, Interaction &ui, bool german, bool compact, int mode)
{
    // Tokens already cancel on Escape/right click. A locked road gives the
    // remaining reset paths a selection without any geometry transaction.
    const bool feature = mode >= 2;
    CanvasGestureFixture fixture(editor, ui, german, compact, feature);
    CanvasScreenGesture screen(editor, ui, feature, !feature);
    auto &io = ImGui::GetIO(); io.AddMouseButtonEvent(0, true); ui.frame();
    require(CanvasProbe::pendingInspector(editor), "Cancellation fixture did not arm the production selection");
    if (!feature) io.AddMousePosEvent(screen.end.x, screen.end.y);
    if (mode == 0 || mode == 2) io.AddKeyEvent(ImGuiKey_Escape, true);
    else if (mode == 1) io.AddMouseButtonEvent(1, true);
    else if (mode == 3) CanvasProbe::mapStage(editor, 4);
    else if (mode == 4) require(editor.editWorkspaceMap(fixture.alternateId), "Cannot switch clean locked-selection map");
    else if (mode == 5) CanvasProbe::resetMapInteraction(editor);
    else CanvasProbe::mapTool(editor, MapCreatorController::Tool::PaintTile);
    ui.frame();
    require(!CanvasProbe::pendingInspector(editor) && !CanvasProbe::inspector(editor),
        "Cancel/reset left a delayed inspector reveal");
    io.AddKeyEvent(ImGuiKey_Escape, false); io.AddMouseButtonEvent(1, false); io.AddMouseButtonEvent(0, false); ui.frame(3);
    if (mode == 3) { CanvasProbe::mapStage(editor, 3); ui.frame(3); }
    require(!CanvasProbe::pendingInspector(editor) && !CanvasProbe::inspector(editor) && CanvasProbe::mapUndoDepth(editor) == 0,
        "Cancelled reveal reopened later or created an unexpected undo step");
    if (mode != 4) canvasPosition(editor, feature, false, 0);
    else require(CanvasProbe::mapDraft(editor).metadata.id == fixture.alternateId && CanvasProbe::mapDraft(editor).tokens.empty(),
        "Map reset retained an old selection target");
}

void mapCanvasGestureCases(UiInteractionTrace &trace, bool german, bool compact)
{
    const ImVec2 originalSize = ImGui::GetIO().DisplaySize;
    // At 1280px/150% the remaining map width can still exceed its 900px
    // breakpoint. Resize this CPU viewport; let production select the layout.
    if (compact) ImGui::GetIO().DisplaySize = ImVec2(1024, 720);
    std::cout << "Canvas fixture viewport=" << canvasPoint(ImGui::GetIO().DisplaySize)
              << " fontScale=" << ImGui::GetStyle().FontScaleMain
              << " observed Tools control required=" << compact << '\n';
    std::string currentCase;
    // Each case owns its authoring draft. setCurrentCampaign does not discard
    // a MapCreator draft, so reusing the preceding moved map
    // would open replacement review instead of this case's map.
    const auto run = [&](const auto &test) {
        EditorWindowController editor; editor.setWindowOpacity(true);
        Interaction ui(editor, trace); test(editor, ui);
    };
    try
    {
    for (int object = 0; object < 3; ++object)
        for (int gesture = 0; gesture < 3; ++gesture)
        {
            currentCase = "object=" + std::string(object == 0 ? "token" : object == 1 ? "move" : "reshape") +
                " gesture=" + (gesture == 0 ? "held" : gesture == 1 ? "fast" : "click");
            run([&](auto &editor, auto &ui) { canvasGesture(editor, ui, german, compact, object, gesture); });
        }
    currentCase = "token held with initially open inspector";
    run([&](auto &editor, auto &ui) { canvasGesture(editor, ui, german, compact, 0, 0, true); });
    for (int mode = 0; mode < 7; ++mode)
    {
        currentCase = "cancel/reset=" + std::to_string(mode);
        run([&](auto &editor, auto &ui) { canvasRevealCancellation(editor, ui, german, compact, mode); });
    }
    }
    catch (const std::exception &error)
    { throw std::runtime_error("Canvas case " + currentCase + " german=" + std::to_string(german) + " compact=" + std::to_string(compact) + ": " + error.what()); }
    ImGui::GetIO().DisplaySize = originalSize;
    std::cout << "Canvas gestures: 10 fixed-screen token/move/reshape groups and 7 reveal cancellation/reset groups passed.\n";
}
