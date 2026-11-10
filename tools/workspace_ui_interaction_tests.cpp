#include "workflow_test_database.hpp"
#include "Models/Map/CurrentMapArtRecipe.hpp"
#include "Controllers/Editor/EditorWindowController.hpp"
#include "Views/Editor/EditorWindowView.hpp"
#include "Views/Editor/Widgets/UiInteractionTrace.hpp"
#include "Views/Editor/ApprovedUi/ApprovedUiTheme.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Session/SessionOutcomeDraftKeys.hpp"
#include "Models/Project/TextDraftRecovery.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Controllers/Editor/EditorWorldHistory.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "session_target_ui_probe.hpp"
#include "imgui_internal.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <iostream>
#include <fstream>
#include <stdexcept>

struct WorkspaceInteractionProbe
{
    static SessionWorkspace &preparation(EditorWindowController &e){return e.view->_sessionWorkspace;}
    static auto workspace(EditorWindowController &e){return e.view->_activeWorkspace;}
    static void processPreparation(EditorWindowController &e){e.processPreparationTarget();}
    static bool preparationQueued(const EditorWindowController &e){return e._workspace.preparationTarget.has_value();}
    static LiveSessionDashboardController &liveSessions(EditorWindowController &e){return *e._children.liveSessionDashboard;}
    static GMScreenController &gmSessions(EditorWindowController &e){return *e._children.gmScreen;}
    static const WorldPlaceModel &placeDraft(EditorWindowController &e){return e.view->_worldPlaceDraft;}
    static const NationModel &nationDraft(EditorWindowController &e){return e.view->_worldNationDraft;}
    static const ShopModel &shopDraft(EditorWindowController &e){return e.view->_worldShopDraft;}
    static const DungeonModel &dungeonDraft(EditorWindowController &e){return e.view->_worldDungeonDraft;}
    static std::shared_ptr<EditorWorldHistory> worldHistory(EditorWindowController &e){return e._workspace.worldHistory;}
    static void lateWorldDescription(EditorWindowController &e,const std::string &text){e.view->_worldPlaceDraft.description=text;e.view->_worldFormEdited=true;}
    static void lateWorldNotes(EditorWindowController &e,const std::string &text){e.view->_worldPlaceDraft.notes=text;e.view->_worldFormEdited=true;}
    static EntityId worldSelection(EditorWindowController &e){return e.view->_workspaceSelectionId;}
    static const std::string &worldFilter(EditorWindowController &e){return e.view->_workspaceFilter;}
    static const std::string &newShopName(EditorWindowController &e){return e.view->_worldNewShopName;}
    static const RandomTableModel &tableDraft(EditorWindowController &e){return e.view->_randomTableDraft;}
    static Campaign &liveCampaign(EditorWindowController &e){return e._currentCampaign;}
    static const EditorWorldRecoveryState &worldRecovery(EditorWindowController &e){return e.view->_worldRecovery;}
    static bool worldTick(EditorWindowController &e,int seconds,bool force=false){return e.view->checkpointWorldRecovery(force,e.view->_worldRecovery.attempted+std::chrono::seconds(seconds));}
    static CharacterSheetController &character(EditorWindowController &editor) { return *editor._characterSheet; }
    static ItemCreatorController &item(EditorWindowController &editor) { return *editor._itemCreator; }
    static bool createEncounterMap(EditorWindowController &editor) { return editor._encounterBuilder->openSharedMapAuthoring(true); }
    static MapLevel mapLevel(EditorWindowController &editor) { return editor._mapCreator->_draftMap.atlasLevel; }
    static int artworkVersion(EditorWindowController &editor) { return editor._mapCreator->_draftMap.generation.recipe.recipeVersion; }
    static MapGenerationOptions nextMapDesign(EditorWindowController &editor) { return editor._mapCreator->buildGenerationOptions(); }
    static MapCreatorController &map(EditorWindowController &editor) { return *editor._mapCreator; }
    static const MapModel &mapDraft(EditorWindowController &editor) { return map(editor)._draftMap; }
    static ImVec2 canvasOrigin(EditorWindowController &editor) { return map(editor)._view->_interactionCanvasOrigin; }
    static float canvasCell(EditorWindowController &editor) { return map(editor)._view->_interactionCanvasCellSize; }
    static bool inspector(EditorWindowController &editor) { return map(editor)._view->_showCompactInspector; }
    static bool pendingInspector(EditorWindowController &editor) { return map(editor)._view->_pendingCanvasInspectorReveal; }
    static std::size_t mapUndoDepth(EditorWindowController &editor) { return map(editor)._history.undo.size(); }
    static std::uint64_t mapEditSerial(EditorWindowController &editor) { return map(editor)._editing.authoringSerial; }
    static void mapUndo(EditorWindowController &editor) { map(editor).undoLastAction(); }
    static void mapRedo(EditorWindowController &editor) { map(editor).redoLastAction(); }
    static void mapStage(EditorWindowController &editor, int step) { map(editor)._view->setWorkflowStep(step); }
    static void mapTool(EditorWindowController &editor, MapCreatorController::Tool tool) { map(editor).setActiveTool(tool); }
    static void resetMapInteraction(EditorWindowController &editor) { map(editor)._view->resetGeneratedFeatureGeometryInteraction(); }
    static bool preservesLoadedArtwork()
    {
        Campaign fixture;
        MapModel legacy;
        legacy.width = 16; legacy.height = 12; legacy.cells.resize(192);
        legacy.atlasLevel = MapLevel::World;
        legacy.generation.enabled = true;
        legacy.generation.recipe.recipeVersion = 9;
        legacy.generation.recipe.generatorVersion = 11;
        legacy.generation.recipe.styleId = "illustrated_atlas";
        legacy.generation.recipe.styleVersion = 5;
        legacy.generation.recipe.styleSnapshotId = "illustrated_atlas_v5";
        legacy.cells[7].kind = MapTileKind::Forest;
        legacy.cells[8].wayKind = MapWayKind::DirtRoad;
        legacy.cells[8].movementCost = 4;
        legacy.cells[8].fogged = true;
        MapGeneratedFeature secret;
        secret.id = "private-artwork-fixture";
        secret.gmOnly = true; secret.points = {{5.f, 6.f}};
        legacy.generation.graph.features.push_back(secret);
        fixture.maps.push_back(legacy);
        MapCreatorController editor(fixture);
        if (!editor.openMapForEditing(legacy.metadata.id)) return false;
        const auto requested = editor.buildGenerationOptions();
        const bool loadedUnchanged = requested.recipeVersion == kCurrentMapIllustrationRecipe &&
            editor._draftMap.generation.recipe.recipeVersion == 9 &&
            fixture.maps.front().generation.recipe.recipeVersion == 9 &&
            editor._draftMap.generation.recipe.styleSnapshotId == "illustrated_atlas_v5";
        if (!loadedUnchanged) return false;
        const auto preserved = [&] {
            const auto &map = editor._draftMap;
            return map.width == 16 && map.height == 12 && map.cells.size() == 192 &&
                map.cells[7].kind == MapTileKind::Forest && map.cells[8].wayKind == MapWayKind::DirtRoad &&
                map.cells[8].movementCost == 4 && map.cells[8].fogged &&
                map.generation.graph.features.size() == 1 && map.generation.graph.features[0].gmOnly &&
                map.generation.graph.features[0].points[0].x == 5.f &&
                !map.playerRelease.waysAndRoutes && !map.playerRelease.labels;
        };
        editor.updateDraftArtwork("Artwork test");
        if (editor._draftMap.generation.recipe.recipeVersion != kCurrentMapIllustrationRecipe || !editor.hasUnsavedDraft() || !preserved())
            return false;
        editor.undoLastAction();
        if (editor._draftMap.generation.recipe.recipeVersion != 9 || !preserved()) return false;
        editor.redoLastAction();
        return editor._draftMap.generation.recipe.recipeVersion == kCurrentMapIllustrationRecipe && preserved() &&
            fixture.maps.front().generation.recipe.recipeVersion == 9;
    }
};

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}
Campaign readCampaign(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns()) if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Campaign missing from SQLite");
}
bool submittedLabelContains(const UiInteractionTrace &trace, const std::string &text)
{
    return std::any_of(trace.items().begin(), trace.items().end(), [&](const auto &entry) {
        return entry.second.label.find(text) != std::string::npos;
    });
}
class Interaction
{
    EditorWindowController &editor_;
    UiInteractionTrace &trace_;
    bool captureText_=false;
    std::string capturedText_;
public:
    Interaction(EditorWindowController &editor, UiInteractionTrace &trace) : editor_(editor), trace_(trace) {}
    void frame(int count = 1)
    {
        while (count--)
        {
            trace_.beginFrame();
            ImGui::NewFrame();
            const ImGuiID dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
            if (!DockableWidget::defaultDockId())
            {
                ImGui::DockBuilderRemoveNode(dock);
                ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dock, ImGui::GetIO().DisplaySize);
                ImGuiID sidebar = 0, workspace = 0;
                ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, .17f, &sidebar, &workspace);
                DockableWidget::setSidebarDockId(sidebar);
                DockableWidget::setDefaultDockId(workspace);
                ImGui::DockBuilderFinish(dock);
            }
            if(captureText_)ImGui::LogToBuffer();
            editor_.update({});
            if(captureText_){capturedText_=ImGui::GetCurrentContext()->LogBuffer.c_str();ImGui::LogFinish();}
            ImGui::Render();
        }
    }
    std::string renderedText(){captureText_=true;frame();captureText_=false;return capturedText_;}
    void dumpCompactLayout(const char *context) const
    {
        std::cerr << "UI layout at " << context << ":\n";
        for (const auto &entry : trace_.items())
        {
            const auto &item = entry.second;
            if (item.label.find("Discard") == std::string::npos &&
                item.label.find("verwerfen") == std::string::npos &&
                item.label.find("Macro") == std::string::npos &&
                item.label.find("Makro") == std::string::npos)
                continue;
            std::cerr << "  item '" << item.label << "' in " << item.window
                      << " rect=" << item.minimum.x << ',' << item.minimum.y << "-"
                      << item.maximum.x << ',' << item.maximum.y << '\n';
        }
        for (auto *window : ImGui::GetCurrentContext()->Windows)
            if (window->WasActive && (std::string(window->Name).find("Tabletop") != std::string::npos ||
                                      std::string(window->Name).find("Workspace") != std::string::npos))
                std::cerr << "  window " << window->Name << " pos=" << window->Pos.x << ',' << window->Pos.y
                          << " size=" << window->Size.x << 'x' << window->Size.y
                          << " scroll=" << window->Scroll.x << ',' << window->Scroll.y
                          << " max=" << window->ScrollMax.x << ',' << window->ScrollMax.y
                          << " clip=" << window->InnerClipRect.Min.x << ',' << window->InnerClipRect.Min.y
                          << '-' << window->InnerClipRect.Max.x << ',' << window->InnerClipRect.Max.y << '\n';
    }
    UiInteractionTrace::Item locate(const std::string &label)
    {
        auto candidates = trace_.find(label);
        if (auto *modal = ImGui::GetTopMostPopupModal())
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](const auto &item) {
                auto *window = ImGui::FindWindowByName(item.window.c_str());
                while (window && window != modal) window = window->ParentWindow;
                return window != modal;
            }), candidates.end());
        if (candidates.size() != 1)
            for (const auto &entry : trace_.items())
                if (!entry.second.label.empty()) std::cerr << entry.second.window << ": " << entry.second.label << '\n';
        require(candidates.size() == 1, "Expected one production widget: " + label + "; found " + std::to_string(candidates.size()));
        return candidates.front();
    }
    std::string observedScrollWindow(const std::string &anchor)
    {
        const auto item = locate(anchor);
        auto *window = ImGui::FindWindowByName(item.window.c_str());
        require(window != nullptr, "Observed production anchor window disappeared");
        for (auto *ancestor = window; ancestor; ancestor = ancestor->ParentWindow)
            if (ancestor->ScrollMax.y > 0.0f) return ancestor->Name;
        return window->Name;
    }
    void click(const std::string &label, const char *scrollWindow = nullptr, bool directorySelectable = false,
               float wheelNotches = 4.0f)
    {
        const auto findScrollTarget = [&]() -> ImGuiWindow * {
            if (!scrollWindow)
                return nullptr;
            ImGuiWindow *target = nullptr;
            for (auto *window : ImGui::GetCurrentContext()->Windows)
                if (window->WasActive && std::string(window->Name).find(scrollWindow) != std::string::npos &&
                    (!target || window->Size.x * window->Size.y > target->Size.x * target->Size.y))
                    target = window;
            return target;
        };
        const auto logicalBounds=[&](const UiInteractionTrace::Item &item,ImGuiWindow *owner) {
            ImRect bounds(item.minimum,item.maximum);
            if(directorySelectable) {
                require(std::string(owner->Name).find("/WorkspaceDirectoryList_")!=std::string::npos,
                    "Directory selectable is not owned by the observed directory child");
                // Selectable(size.x=0) expands ItemAdd's hit area by half the
                // spacing. Restore its logical row, not an arbitrary tolerance.
                const auto spacing=ImGui::GetStyle().ItemSpacing;
                const float left=std::floor(spacing.x*.5f),top=std::floor(spacing.y*.5f);
                bounds.Min.x+=left;bounds.Max.x-=spacing.x-left;
                bounds.Min.y+=top;bounds.Max.y-=spacing.y-top;
                // Renderer-less frames scale the existing legacy bake while
                // the atlas is locked. Measure that same bake: requesting a
                // new size after Render would dirty the unlocked font atlas.
                auto *font=ImGui::GetIO().Fonts->Fonts[0];
                const auto legacy=font->CalcTextSizeA(font->LegacySize,FLT_MAX,0,label.c_str());
                const float scale=owner->FontRefSize/font->LegacySize;
                const ImVec2 text(legacy.x*scale,legacy.y*scale);
                require(ImGui::GetIO().Fonts->TexIsBuilt,"Directory measurement invalidated the CPU font atlas");
                require(bounds.Min.x>=owner->WorkRect.Min.x-1.f&&bounds.Max.x<=owner->WorkRect.Max.x+1.f&&
                    text.x<=bounds.GetWidth()+1.f&&text.y<=bounds.GetHeight()+1.f,
                    "Directory row caption exceeds its logical content: "+label+" font="+std::to_string(owner->FontRefSize));
            }
            return bounds;
        };
        const auto scrollTargetBy = [&](ImGuiWindow *target, float wheelDelta) {
            if (!target) return;
            const float before = target->Scroll.y;
            const auto hover = target->InnerClipRect.GetCenter();
            ImGui::GetIO().AddMousePosEvent(hover.x, hover.y); frame();
            ImGui::GetIO().AddMouseWheelEvent(0, wheelDelta); frame(2);
            // The headless test backend records the embedded child's scroll
            // range but does not always route a queued wheel event to it. Keep
            // the test on that observed production child rather than clicking
            // an off-clip rectangle or an unrelated parent window.
            if (target->Scroll.y == before && target->ScrollMax.y > 0.0f)
            {
                target->Scroll.y = ImClamp(before - wheelDelta * ImGui::GetTextLineHeightWithSpacing() * 2.0f,
                                            0.0f, target->ScrollMax.y);
                frame(2);
            }
        };
        ImGuiWindow *preferredScrollTarget = findScrollTarget();
        if (scrollWindow && trace_.find(label).empty()) {
            ImGuiWindow *target = preferredScrollTarget;
            if (!target) for (const auto &entry : trace_.items())
                if (!entry.second.label.empty()) std::cerr << entry.second.window << ": " << entry.second.label << '\n';
            require(target != nullptr, "Scrollable production panel unavailable for " + label);
            // Search both directions using ordinary wheel input. Clipped widgets
            // need not be submitted to the observation hook until scrolled into view.
            for (int direction : {-1, 1}) {
                for (int step = 0; step < 32 && trace_.find(label).empty(); ++step) {
                    scrollTargetBy(target, direction * wheelNotches);
                }
                if (!trace_.find(label).empty()) break;
            }
            if (trace_.find(label).empty())
                std::cerr << "Scroll search missed label=" << label << " owner=" << target->Name
                          << " scroll=" << target->Scroll.y << '/' << target->ScrollMax.y
                          << " clipHeight=" << target->InnerClipRect.GetHeight()
                          << " renderedFont=" << target->FontRefSize << '\n';
        }
        for (int attempt = 0; attempt < 24; ++attempt)
        {
            const auto item = locate(label);
            auto *window = ImGui::FindWindowByName(item.window.c_str());
            require(window != nullptr, "Widget window disappeared");
            ImGuiWindow *interactionWindow = preferredScrollTarget ? preferredScrollTarget : window;
            if(directorySelectable)require(interactionWindow==window,"Directory row resolved to a different scrolling owner");
            const auto bounds=logicalBounds(item,window);const auto center=bounds.GetCenter();
            if(bounds.Min.x < interactionWindow->InnerClipRect.Min.x-1.f || bounds.Max.x > interactionWindow->InnerClipRect.Max.x+1.f) {
                const auto rect=[](const ImVec2 &a,const ImVec2 &b){return "("+std::to_string(a.x)+","+std::to_string(a.y)+")-("+
                    std::to_string(b.x)+","+std::to_string(b.y)+")";};
                std::cerr<<"Horizontal control diagnostic: id="<<item.id<<" label="<<label<<" owner="<<window->Name
                    <<" target="<<interactionWindow->Name<<" item="<<rect(item.minimum,item.maximum)
                    <<" logical="<<rect(bounds.Min,bounds.Max)
                    <<" ownerClip="<<rect(window->InnerClipRect.Min,window->InnerClipRect.Max)
                    <<" targetClip="<<rect(interactionWindow->InnerClipRect.Min,interactionWindow->InnerClipRect.Max)
                    <<" work="<<rect(window->WorkRect.Min,window->WorkRect.Max)
                    <<" padding="<<window->WindowPadding.x<<","<<window->WindowPadding.y
                    <<" spacing="<<ImGui::GetStyle().ItemSpacing.x<<","<<ImGui::GetStyle().ItemSpacing.y
                    <<" ownerFontSize="<<window->FontRefSize<<'\n';
            }
            require(bounds.Min.x >= interactionWindow->InnerClipRect.Min.x - 1.f &&
                    bounds.Max.x <= interactionWindow->InnerClipRect.Max.x + 1.f,
                    "Production control is horizontally clipped: " + label);
            if (interactionWindow->InnerClipRect.Contains(center))
            {
                // A focused multiline field may request a final child-scroll
                // adjustment on this frame. Re-read the observed production
                // rectangle until the pointer and item agree before pressing.
                UiInteractionTrace::Item current = item;
                ImVec2 clickCenter = center;
                for (int settle = 0; settle < 4; ++settle)
                {
                    ImGui::GetIO().AddMousePosEvent(clickCenter.x, clickCenter.y);
                    frame();
                    current = locate(label);
                    clickCenter = logicalBounds(current,window).GetCenter();
                }
                if(directorySelectable) {
                    require(current.id==item.id&&current.window==item.window,"Directory row identity changed while settling input");
                    const auto currentBounds=logicalBounds(current,window);
                    require(currentBounds.Min.x>=window->InnerClipRect.Min.x-1.f&&currentBounds.Max.x<=window->InnerClipRect.Max.x+1.f&&
                        window->InnerClipRect.Contains(clickCenter),"Directory row moved outside its visible clip before press");
                    if(ImGui::GetCurrentContext()->HoveredId!=current.id)continue;
                }
                // Queue the final position and press together. A final
                // standalone frame could again reflow a just-updated header
                // between the observed rectangle and the press.
                ImGui::GetIO().AddMousePosEvent(clickCenter.x, clickCenter.y);
                ImGui::GetIO().AddMouseButtonEvent(0, true); frame();
                ImGui::GetIO().AddMouseButtonEvent(0, false); frame(2);
                return;
            }
            scrollTargetBy(interactionWindow, center.y < interactionWindow->InnerClipRect.GetCenter().y ? wheelNotches : -wheelNotches);
        }
        throw std::runtime_error("Could not scroll to production widget: " + label);
    }
    void clickDirectoryRow(const std::string &label){click(label,"WorkspaceDirectoryList",true);}
    void replaceText(const std::string &label, const char *text, const char *scrollWindow = nullptr)
    {
        click(label, scrollWindow);
        require(ImGui::GetCurrentContext()->ActiveId != 0, "Input did not receive keyboard focus");
        auto &io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl, true); io.AddKeyEvent(ImGuiKey_A, true); frame();
        io.AddKeyEvent(ImGuiKey_A, false); io.AddKeyEvent(ImGuiMod_Ctrl, false); frame();
        io.AddInputCharactersUTF8(text); frame(2);
    }
    void clickModalButton(const std::string &label)
    {
        require(ImGui::GetTopMostPopupModal() != nullptr, "Expected an open production confirmation");
        // Use the ordinary production click path: it follows the popup parent chain,
        // refreshes normal layout, and scrolls a compact modal when required.
        click(label);
    }
};
#include "session_outcome_leave_ui_cases.hpp"
#include "session_target_reset_ui_cases.hpp"
#include "session_preparation_navigation_ui_cases.hpp"
#include "map_canvas_gesture_ui_cases.hpp"
#include "map_canvas_layout_ui_cases.hpp"
#include "world_inspector_draft_ui_cases.hpp"
#include "world_draft_recovery_ui_cases.hpp"
#include "world_history_ui_cases.hpp"
}

int main(int argc, char **argv)
{
    ImGuiContext *context = ImGui::CreateContext();
    try
    {
        isolateWorkflowDatabase();
        const auto dataRoot = ApplicationPaths::databasePath().parent_path().string();
#ifdef _WIN32
        require(_putenv_s("TTRPG_DATA_ROOT", dataRoot.c_str()) == 0, "Cannot isolate outcome recovery fixtures");
#else
        require(setenv("TTRPG_DATA_ROOT", dataRoot.c_str(), 1) == 0, "Cannot isolate outcome recovery fixtures");
#endif
        bool german = false, compact = false;
        for (int i = 1; i < argc; ++i) {
            const std::string argument(argv[i]);
            if (argument == "--german") german = true;
            else if (argument == "--compact") compact = true;
            else throw std::runtime_error("Unknown interaction option: " + argument);
        }
        gm::localization::LocalizationService::instance().setPreference(german ?
            gm::localization::LocalePreference::German : gm::localization::LocalePreference::English, false);
        const auto translated = [](const char *label) { return std::string(gm::localization::trUi(label)); };
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = compact ? ImVec2(1280, 720) : ImVec2(1600, 1000);
        if (compact) ImGui::GetStyle().FontScaleMain = 1.5f;
        io.DeltaTime = 1.f / 60.f;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        unsigned char *pixels = nullptr; int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        require(pixels && width > 0 && height > 0, "Font atlas did not build");
        auto &save = ProjectSaveService::getInstance();
        save.setBackupSnapshotsEnabled(false);
        save.setRecoverySnapshotsEnabled(false);
        require(WorkspaceInteractionProbe::preservesLoadedArtwork(),
            "Loading a saved map changed its artwork before explicit regeneration");
        Campaign campaign;
        campaign.name = "Generated scripted UI fixture";
        MapModel legacyTactical;
        legacyTactical.name="Legacy artwork review";legacyTactical.width=16;legacyTactical.height=12;
        legacyTactical.scope=MapScope::Local;legacyTactical.atlasLevel=MapLevel::Tactical;
        legacyTactical.visibility=MapVisibility::GameMasterOnly;
        legacyTactical.creationIntent=MapCreationIntent::TacticalEncounter;
        legacyTactical.generation.enabled=true;legacyTactical.generation.recipe.generatorVersion=12;
        legacyTactical.generation.recipe.recipeVersion=9;legacyTactical.generation.recipe.scale=MapLevel::Tactical;
        legacyTactical.generation.recipe.styleId="illustrated_atlas";
        legacyTactical.generation.recipe.styleVersion=6;legacyTactical.generation.recipe.styleSnapshotId="illustrated_atlas_v6";
        resizeMapCells(legacyTactical);
        for(auto &cell:legacyTactical.cells){cell.kind=MapTileKind::Dirt;cell.fogged=false;}
        campaign.maps.push_back(legacyTactical);
        CharacterModel advancingCharacter;
        advancingCharacter.selectedClass = "Fighter";
        advancingCharacter.maxHitPoints = 12;
        advancingCharacter.currentHitPoints = 8;
        advancingCharacter.attributes["Constitution"] = 14;
        campaign.characters.push_back(advancingCharacter);
        campaign.ruleSet = std::make_shared<DND5e>();
        ItemModel item; item.name = "Original blade";
        campaign.customItems.push_back(item);
        CharacterModel player;
        campaign.characters.push_back(player);
        DungeonModel dungeon; dungeon.name = "First dungeon";
        DungeonRoomModel room; room.name = "Original room"; dungeon.rooms.push_back(room);
        DungeonModel otherDungeon; otherDungeon.name = "Second dungeon";
        campaign.dungeons = {dungeon, otherDungeon};
        StoryObjectiveModel objective;
        objective.title = "Find the missing lamplighter";
        objective.playerSummary = "Follow the last lit street.";
        objective.gmNotes = "The lamplighter is held beneath the old bridge.";
        campaign.storyObjectives.push_back(objective);
        StoryClueModel clue;
        clue.title = "Lamplighter's brass key";
        clue.playerText = "A brass key bears the bridge keeper's mark.";
        clue.gmTruth = "It opens the holding room beneath the old bridge.";
        campaign.storyClues.push_back(clue);
        CampaignRepository::saveCampaign(campaign);
        {
            UiInteractionTrace trace(context);
            EditorWindowController editor;
            editor.setCurrentCampaign(campaign);
            editor.setWindowOpacity(true);
            Interaction ui(editor, trace);
            editor.showCharacterSheet();
            ui.frame(4); // A fresh sheet has index -1; opening must select a valid player.
            require(editor.openWorkspaceEntity("Item", item.metadata.id), "Could not open owning item workspace");
            ui.frame(4);
            ui.replaceText("##Name", "Retained blade");
            require(editor.hasUnsavedChanges(), "Typed item edit did not participate in global dirty state");
            require(WorkspaceInteractionProbe::item(editor).getModel().name == "Retained blade", "Typing did not edit production draft");
            editor.showSpellLibrary(); ui.frame(3);
            ui.click(german ? "Zurück: Kompendium##PreviousTask" : "Back: Compendium##PreviousTask"); ui.frame(3);
            require(editor.navigationContext().current().editorRoute == "show:item_creator" &&
                WorkspaceInteractionProbe::item(editor).getModel().name == "Retained blade",
                "Same-workspace return lost the edited item");
            SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
            control.exec("CREATE TRIGGER reject_ui_item BEFORE INSERT ON custom_items BEGIN SELECT RAISE(ABORT, 'injected UI save failure'); END;");
            ui.click(translated("Save Item"));
            require(editor.hasUnsavedChanges() && readCampaign(campaign.metadata.id).customItems[0].name == "Original blade",
                "Failed visible save cleared dirty state or wrote the draft");
            control.exec("DROP TRIGGER reject_ui_item;");
            ui.click(german ? "Speicherung wiederholen" : "Retry saving");
            require(readCampaign(campaign.metadata.id).customItems[0].name == "Retained blade" && !editor.hasUnsavedChanges(),
                "Visible Retry did not submit the retained draft and clear dirty state");
            ui.click(translated("Archive Item"), "ItemDraftFields"); ui.frame(2); ui.click(translated("Archive"));
            const auto archived = readCampaign(campaign.metadata.id).customItems[0];
            require(std::find(archived.metadata.tags.begin(), archived.metadata.tags.end(), "archived") != archived.metadata.tags.end(),
                "Visible archive action did not persist");
            editor.showPlayerOverview(); ui.frame(3); ui.click(german ? "Zurück: Kompendium##PreviousTask" : "Back: Compendium##PreviousTask");
            require(editor.navigationContext().current().selectedEntityId.empty(), "Archived return retained an active entity selection");
            editor.showItemCreator(); ui.frame(3); ui.click(translated("New Item"), "ItemDraftFields");
            ui.replaceText("##Name", "Created through input"); ui.click(translated("Create Item"));
            const auto finalCampaign = readCampaign(campaign.metadata.id);
            require(finalCampaign.customItems.size() == 2 && finalCampaign.customItems[1].metadata.id != item.metadata.id &&
                finalCampaign.customItems[1].name == "Created through input", "Visible create action lost content or identity");
            editor.showPlayerCreator(); ui.frame(3);
            ui.click(german ? "Identität vorschlagen..." : "Suggest identity...");
            auto *identityModal = ImGui::GetTopMostPopupModal();
            require(identityModal && identityModal->Size.x >= 300.0f &&
                        identityModal->Size.x <= io.DisplaySize.x * 0.90f + 1.0f,
                    "Identity suggestion confirmation collapsed or exceeded the viewport");
            ui.click(translated("Cancel"));
            require(!ImGui::GetTopMostPopupModal(), "Identity suggestion confirmation did not cancel");
            editor.showHandoutCreator();ui.frame(3);
            ui.replaceText(german?"Titel":"Title","A sealed invitation");
            ui.replaceText("##HandoutBody","Meet at the old bridge.");
            require(editor.hasUnsavedChanges(),"Handout draft omitted from global dirty state");
            control.exec("CREATE TRIGGER reject_ui_handout BEFORE INSERT ON handouts BEGIN SELECT RAISE(ABORT, 'injected handout save failure'); END;");
            ui.click(german?"Verborgenes Handout speichern":"Save hidden handout");
            require(editor.hasUnsavedChanges() && readCampaign(campaign.metadata.id).handouts.empty(),"Failed handout UI save lost the draft or committed data");
            control.exec("DROP TRIGGER reject_ui_handout;");
            ui.click(german?"Verborgenes Handout speichern":"Save hidden handout");
            const auto handouts=readCampaign(campaign.metadata.id);
            require(handouts.handouts.size()==1 && handouts.handouts.front().body=="Meet at the old bridge." &&
                handouts.localPlayerDisplay.publicHandoutIds.empty() && !editor.hasUnsavedChanges(),"Handout retry did not save hidden content cleanly");
            require(editor.openWorkspaceEntity("Objective", objective.metadata.id), "Saved objective did not open its table-tools workspace");
            ui.frame(4);
            require(editor.navigationContext().current().workspace == gm::ux::WorkspaceTarget::Live &&
                submittedLabelContains(trace, objective.title), "Objective navigation did not focus the saved objective");
            require(editor.openWorkspaceEntity("Clue", clue.metadata.id), "Saved clue did not open its table-tools workspace");
            ui.frame(4);
            require(editor.navigationContext().current().workspace == gm::ux::WorkspaceTarget::Live &&
                submittedLabelContains(trace, clue.title), "Clue navigation did not focus the saved clue");
            require(!editor.openWorkspaceEntity("Objective", "missing-objective-id"),
                "Unknown objective stable id navigated to an unrelated record");
            require(readCampaign(campaign.metadata.id).storyObjectives.size() == 1 &&
                readCampaign(campaign.metadata.id).storyClues.size() == 1,
                "Saved toolkit navigation changed persisted records");

            require(editor.openWorkspaceEntity("Objective", objective.metadata.id), "Objective could not be reopened for draft protection");
            ui.frame(4);
            ui.click(german ? "Ziel hinzufügen" : "Add objective", "Workspace");
            ui.replaceText(german ? "Titel##NewObjective" : "Title##NewObjective", "Pending objective", "Workspace");
            require(editor.hasUnsavedChanges() && !editor.saveCurrentCampaign("pending toolkit objective"),
                "Global save accepted an unfinished objective draft");
            require(readCampaign(campaign.metadata.id).storyObjectives.size() == 1 &&
                submittedLabelContains(trace, german ? "Titel##NewObjective" : "Title##NewObjective"),
                "Rejected objective save created or discarded the unfinished draft");

            require(editor.openWorkspaceEntity("Clue", clue.metadata.id),
                "Clue could not be focused for independent draft protection");
            ui.frame(4);
            ui.click(german ? "Hinweis hinzufügen" : "Add clue", "Workspace");
            ui.replaceText(german ? "Titel##NewClue" : "Title##NewClue", "Pending clue", "Workspace");
            require(!editor.saveCurrentCampaign("pending toolkit drafts"), "Global save accepted independent pending drafts");
            require(editor.openWorkspaceEntity("Objective", objective.metadata.id),
                "Objective could not be refocused before discarding its draft");
            ui.frame(4);
            ui.click(german ? "Entwurf verwerfen" : "Discard draft", "Workspace"); ui.frame(2);
            require(ImGui::GetTopMostPopupModal(), "Objective discard did not require confirmation");
            ui.clickModalButton(std::string(german ? "Entwurf verwerfen" : "Discard draft") +
                                "##TabletopToolkitDiscardConfirm1");
            require(!ImGui::GetTopMostPopupModal(), "Objective discard confirmation stayed open");
            require(!editor.saveCurrentCampaign("remaining clue draft") &&
                readCampaign(campaign.metadata.id).storyObjectives.size() == 1,
                "Discarding one pending draft discarded another draft or created an objective");
            require(editor.openWorkspaceEntity("Clue", clue.metadata.id),
                "Clue could not be refocused before discarding its draft");
            ui.frame(4);
            ui.click(german ? "Entwurf verwerfen" : "Discard draft", "Workspace"); ui.frame(2);
            require(ImGui::GetTopMostPopupModal(), "Clue discard did not require confirmation");
            ui.clickModalButton(std::string(german ? "Entwurf verwerfen" : "Discard draft") +
                                "##TabletopToolkitDiscardConfirm2");
            require(!ImGui::GetTopMostPopupModal(), "Clue discard confirmation stayed open");
            // The compact profiles exercise the independently retained macro draft
            // through the same embedded table-tools surface as the compact map modal.
            if (compact)
            {
                // Live now contains the table tools in their own scrolling child.
                // Target that child rather than the outer Workspace window: after
                // the compact header changes height, the macro controls may be
                // clipped while the outer workspace itself has no scroll range.
                constexpr const char *tableToolsContent = "TabletopToolkitEmbeddedContent";
                ui.click(german ? "Würfel" : "Dice", tableToolsContent); ui.frame(3);
                ui.click(german ? "Makro hinzufügen" : "Add macro", tableToolsContent);
                ui.replaceText("Name##NewMacro", "Pending macro", tableToolsContent);
                require(!editor.saveCurrentCampaign("pending toolkit macro"),
                    "Global save accepted an unfinished macro draft after focused navigation");
                ui.click(german ? "Entwurf verwerfen" : "Discard draft", tableToolsContent); ui.frame(2);
                if (!ImGui::GetTopMostPopupModal()) ui.dumpCompactLayout("macro discard did not open confirmation");
                require(ImGui::GetTopMostPopupModal(), "Macro discard did not require confirmation");
                ui.clickModalButton(std::string(german ? "Entwurf verwerfen" : "Discard draft") +
                                    "##TabletopToolkitDiscardConfirm0");
                require(!ImGui::GetTopMostPopupModal(), "Macro discard confirmation stayed open");
            }
            require(editor.saveCurrentCampaign("discarded toolkit drafts") && !editor.hasUnsavedChanges() &&
                readCampaign(campaign.metadata.id).storyObjectives.size() == 1 &&
                readCampaign(campaign.metadata.id).storyClues.size() == 1 &&
                readCampaign(campaign.metadata.id).diceMacros.empty(),
                "Confirmed discards did not leave the campaign clean without creating records");

            require(editor.openWorkspaceEntity("Objective", objective.metadata.id), "Saved objective unavailable for normal campaign edit");
            ui.frame(4);
            ui.replaceText(german ? "Spielertext" : "Player summary", "Revised public trail", "Workspace");
            require(editor.hasUnsavedChanges() && editor.saveCurrentCampaign("saved toolkit objective"),
                "Editing a saved objective did not use normal campaign saving");
            require(readCampaign(campaign.metadata.id).storyObjectives.front().playerSummary == "Revised public trail",
                "Normal campaign save did not retain the saved objective edit");
            editor.showEncounterBuilder();
            require(WorkspaceInteractionProbe::createEncounterMap(editor) &&
                editor.navigationContext().current().workspace == gm::ux::WorkspaceTarget::World &&
                WorkspaceInteractionProbe::mapLevel(editor) == MapLevel::Tactical,
                "Encounter map creation did not open World with tactical intent");
            const auto nextMap = WorkspaceInteractionProbe::nextMapDesign(editor);
            require(nextMap.recipeVersion == kCurrentMapIllustrationRecipe && nextMap.generatorVersion == 12 &&
                nextMap.styleId == "illustrated_atlas" && nextMap.styleVersion == 6,
                "Shared map authoring did not select the current tactical artwork");
            require(editor.editWorkspaceMap(legacyTactical.metadata.id),"Saved tactical map could not open for artwork review");
            ui.frame(4);
            if(ImGui::GetTopMostPopupModal()) {ui.click(translated("Load Map"));ui.frame(3);}
            require(WorkspaceInteractionProbe::artworkVersion(editor)==9,"Opening tactical authoring silently changed artwork");
            if (compact)
            {
                ui.click(german ? "Spielervorschau" : "Player preview", "Workspace"); ui.frame(2);
                ui.click(german ? "Spielersichtbarkeit…" : "Player visibility…", "Workspace"); ui.frame(2);
                require(ImGui::GetTopMostPopupModal(), "Compact player-visibility controls did not open a modal");
                ui.click(german ? "Karte für Spieler verfügbar" : "Map available to players");
                ui.click(german ? "Speichern und Vorschau" : "Save and preview"); ui.frame(3);
                const auto savedVisibility = readCampaign(campaign.metadata.id);
                require(savedVisibility.maps.front().visibility == MapVisibility::PlayersVisible &&
                    !savedVisibility.localPlayerDisplay.published,
                    "Compact player-visibility save did not persist safely without publishing the player display");
                ui.click(german ? "Bearbeiten" : "Edit", "Workspace"); ui.frame(2);
            }
            ui.click(german?"Kartenmenü":"Map menu");ui.frame(2);
            ui.click(german?"Kartendarstellung aktualisieren":"Update map artwork");ui.frame(2);
            require(WorkspaceInteractionProbe::artworkVersion(editor)==kCurrentMapIllustrationRecipe,"Visible local artwork update did not change the draft");
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,true);ui.frame();
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,false);ui.frame();
            ui.click(translated("Undo"));ui.frame(2);
            require(WorkspaceInteractionProbe::artworkVersion(editor)==9 &&
                readCampaign(campaign.metadata.id).maps.front().generation.recipe.recipeVersion==9,
                "Visible artwork Undo changed saved data or failed to restore the prior appearance");
            editor.showDungeonBuilder();ui.frame(3);
            require(editor.navigationContext().current().editorRoute.empty(),"Dungeon shortcut left map authoring open");
            ui.replaceText(german ? "Raumname" : "Room name", "Retained room", "Workspace");
            require(editor.hasUnsavedChanges(), "Room edits omitted from global dirty state");
            editor.showHandoutCreator();ui.frame(3);
            ui.replaceText(german?"Titel":"Title","A separate handout");
            ui.click(german?"Verborgenes Handout speichern":"Save hidden handout");
            require(editor.hasUnsavedChanges() &&
                readCampaign(campaign.metadata.id).dungeons.front().rooms.front().name == "Original room",
                "Saving a handout committed or discarded an independent room draft");
            require(editor.openWorkspaceEntity("Dungeon", otherDungeon.metadata.id), "Second dungeon unavailable");
            ui.frame(3);
            require(editor.openWorkspaceEntity("Dungeon", dungeon.metadata.id), "First dungeon unavailable");
            ui.frame(3);
            control.exec("CREATE TRIGGER reject_ui_dungeon BEFORE INSERT ON dungeons BEGIN SELECT RAISE(ABORT, 'injected dungeon failure'); END;");
            require(!editor.saveCurrentCampaign("dungeon draft retry") && editor.hasUnsavedChanges() &&
                readCampaign(campaign.metadata.id).dungeons.front().rooms.front().name == "Original room",
                "Failed room save lost dirty state or wrote the draft");
            control.exec("DROP TRIGGER reject_ui_dungeon;");
            require(editor.saveCurrentCampaign("dungeon draft retry") && !editor.hasUnsavedChanges(), "Room retry failed");
            require(readCampaign(campaign.metadata.id).dungeons.front().rooms.front().name == "Retained room",
                "Switching dungeons or failed commit discarded the edited room");
            editor.showMapCreator();editor.showShopManager();ui.frame(3);
            require(editor.navigationContext().current().editorRoute.empty(),"Shop shortcut left map authoring open");
            editor.showCharacterSheet();
            require(WorkspaceInteractionProbe::character(editor).selectCharacterById(advancingCharacter.metadata.id), "Advancing character unavailable");
            ui.frame(3);
            ui.click(translated("Profile"));
            ui.click(translated("Level Up"), "EmbeddedEditorContent");
            ui.click(translated("Next"));
            ui.replaceText(german ? "Neue Merkmale" : "New features", "Chosen technique", translated("Level Up Character").c_str());
            ui.click(translated("Next"));
            ui.click(translated("Add resource"), translated("Level Up Character").c_str());
            ui.replaceText(translated("Resource name"), "Generated resolve", translated("Level Up Character").c_str());
            ui.replaceText(translated("Maximum uses"), "3", translated("Level Up Character").c_str());
            ui.replaceText(translated("Recharge rule"), "Short Rest", translated("Level Up Character").c_str());
            ui.click(translated("Next"));
            ui.click(translated("I reviewed these changes and feature prerequisites"));
            ui.click(translated("Back")); ui.click(translated("Back"));
            ui.replaceText(translated("New features"), "Revised technique", translated("Level Up Character").c_str());
            ui.click(translated("Next")); ui.click(translated("Next"));
            ui.click(translated("Apply Level Up"));
            require(ImGui::GetTopMostPopupModal() && readCampaign(campaign.metadata.id).characters.front().level == 1,
                "Editing choices did not invalidate the previous review");
            ui.click(translated("I reviewed these changes and feature prerequisites"));
            control.exec("CREATE TRIGGER reject_ui_advance BEFORE INSERT ON characters BEGIN SELECT RAISE(ABORT, 'injected advancement UI failure'); END;");
            ui.click(translated("Apply Level Up"));
            require(ImGui::GetTopMostPopupModal() && editor.hasUnsavedChanges() && readCampaign(campaign.metadata.id).characters.front().level == 1,
                "Failed advancement closed the dialog, changed disk or lost the draft");
            control.exec("DROP TRIGGER reject_ui_advance;");
            ui.click(translated("Apply Level Up"));
            const auto advanced = readCampaign(campaign.metadata.id).characters.front();
            require(!ImGui::GetTopMostPopupModal() && advanced.level == 2 && advanced.maxHitPoints == 20 &&
                advanced.traitsAndFeatures.size() == 1 && advanced.traitsAndFeatures.front() == "Revised technique" && advanced.resourceTrackers.size() == 1 &&
                advanced.resourceTrackers.front().name == "Generated resolve" && advanced.resourceTrackers.front().maximum == 3,
                "Advancement dialog retry lost choices or applied HP twice");
            outcomeLeaveRecovery(editor, ui, german, false);
            outcomeLeaveRecovery(editor, ui, german, true);
            sessionTargetCampaignReset(editor, ui, german);
            sessionPreparationNavigation(editor,ui,german);
            std::cout << "Production UI input: create/edit/archive, keyboard focus, retained draft, visible save failure/retry, same-workspace return and private session draft leave recovery passed.\n";
        }
        {
            UiInteractionTrace trace(context);
            worldInspectorDraftCases(trace,german);
            worldDraftRecoveryCases(trace,german);
            worldHistoryUiCases(trace,german);
            mapCanvasGestureCases(trace, german, compact);
            if (compact) mapCanvasEnlargedLayoutCase(trace, german);
        }
        ImGui::DestroyContext(context);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        ImGui::DestroyContext(context);
        return 1;
    }
}
