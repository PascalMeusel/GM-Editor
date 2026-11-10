#include "Views/Editor/Widgets/Session/GMScreen/GMScreenViewSupport.hpp"

GMScreenView::GMScreenView(GMScreenController &controller)
    : DockableWidget("GM Screen", 0, ImVec2(1120.0f, 720.0f)), _controller(controller)
{
}

void GMScreenView::draw()
{
    if (!_controller._sessionOperationError.empty())
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _controller._sessionOperationError.c_str());
    CompactUiDensityScope density;
    const std::vector<ItemModel> items = _controller.getAvailableItems();
    drawHelpText("GM Screen", "Configurable live-table dashboard for quick references, public display staging, handouts, effects, and session controls.");

    if (ImGui::BeginTabBar("GMScreenTabs"))
    {
        if (ImGui::BeginTabItem(gm::localization::trUi("Table Core")))
        {
            drawTableCoreDashboard(items);
            ImGui::EndTabItem();
        }
        const bool germanToolkitLabel = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
        if (ImGui::BeginTabItem(germanToolkitLabel ? "Tischwerkzeuge" : "Table tools"))
        {
            _controller._tabletopToolkit->drawEmbedded();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(gm::localization::trUi("Public Display")))
        {
            drawPublicDisplayTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(gm::localization::trUi("Magic/Effects")))
        {
            drawMagicEffectsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(gm::localization::trUi("Handouts")))
        {
            drawHandoutsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(gm::localization::trUi("Random/Rules")))
        {
            drawRandomRulesTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(gm::localization::trUi("Advanced")))
        {
            drawAdvancedTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void GMScreenView::drawTableCoreDashboard(const std::vector<ItemModel> &items)
{
    drawSessionStatusStrip("GMScreenCoreSessionStatus");
    drawPublicDisplayStatusStrip("GMScreenCorePublicStatus");

    if (ImGui::BeginTable("GMScreenCoreLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("TableCoreLeft").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.56f);
        ImGui::TableSetupColumn(localizedUiLabel("TableCoreRight").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.44f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        drawPartyPassives(items);
        drawCommonDcs();
        drawConditionsSnapshot();
        ImGui::TableNextColumn();
        drawQuickNpcs();
        drawActiveEffects();
        drawRandomTools();
        drawRulesBookmarks();
        ImGui::EndTable();
    }
}

void GMScreenView::drawPublicDisplayTab()
{
    drawLocalPlayerDisplayControls();
}

void GMScreenView::drawMagicEffectsTab()
{
    const int trackerCount = spellSlotTrackerCount(_controller._currentCampaign);
    const std::string selection = std::to_string(trackerCount) + " " + localizedUiLabel("Live Spell Slots") + " | " +
                                  std::to_string(activeEffectCount(_controller._currentCampaign)) + " " + localizedUiLabel("Active Effects");
    UiStatusActionStrip strip;
    strip.mode = gm::localization::trUi(trackerCount > 0 ? "Magic resources available" : "No tracked spell slots");
    strip.selection = selection.c_str();
    strip.nextAction = gm::localization::trUi(trackerCount > 0 ? "Open a caster row only when updating slots." : "Use character or NPC sheets to enable trackers.");
    strip.help = "Spell slot edits here write to the same player and NPC spellcasting state used elsewhere.";
    drawStatusActionStrip("GMScreenMagicEffectsStatus", strip);

    drawActiveEffects();
    if (beginContextHeader("Live Spell Slots", trackerCount > 0, "Open only when slot totals need checking or adjusting."))
        drawLiveSpellSlots();
    if (beginAdvancedHeader("Condition Reference / Full Rules", "Dense rules reference kept out of the main magic workflow."))
        drawConditionsReference();
}

void GMScreenView::drawHandoutsTab()
{
    const LocalPlayerDisplayModel &display = _controller._currentCampaign.localPlayerDisplay;
    const std::string selection = handoutStatusLabel(_controller._currentCampaign, display);
    UiStatusActionStrip strip;
    strip.mode = gm::localization::trUi(display.showPublicHandouts ? "Handouts can be shown" : "Handout panel hidden");
    strip.selection = selection.c_str();
    strip.nextAction = gm::localization::trUi(display.showPublicHandouts ? "Mark only player-safe handouts public." : "Enable Show handouts in Public Display when ready.");
    strip.help = "Handout drafts and reveal toggles are kept separate so the default GM Screen stays quick-reference focused.";
    drawStatusActionStrip("GMScreenHandoutStatus", strip);
    drawHandoutRevealControls();
}

void GMScreenView::drawRandomRulesTab()
{
    UiStatusActionStrip strip;
    strip.mode = gm::localization::trUi("Rules quick reference");
    strip.selection = gm::localization::trUi(_controller._currentCampaign.ruleSet ? "Campaign ruleset loaded" : "No ruleset loaded");
    strip.nextAction = gm::localization::trUi("Roll, bookmark, or open a dense reference only when needed.");
    strip.help = "This tab keeps roll tools, bookmarks, DCs, and full condition rules together without crowding the default dashboard.";
    drawStatusActionStrip("GMScreenRandomRulesStatus", strip);

    if (ImGui::BeginTable("GMScreenRandomRulesLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("RandomRulesLeft").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn(localizedUiLabel("RandomRulesRight").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.55f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        drawRandomTools();
        drawRulesBookmarks();
        ImGui::TableNextColumn();
        drawCommonDcs();
        if (beginAdvancedHeader("Full Conditions Reference", "Open the complete condition text only when a quick DC or roll tool is not enough."))
            drawConditionsReference();
        ImGui::EndTable();
    }
}

void GMScreenView::drawAdvancedTab()
{
    drawSessionFlow();
}

void GMScreenView::drawSessionStatusStrip(const char *id) const
{
    const SessionModel *activeSession = _controller.activeSession();
    const bool breakActive = _controller.isSessionBreakActive();
    const std::string mode = gm::localization::trUi(activeSession ? (breakActive ? "Break / clock paused" : "Live session running") : "Prep / no live session");
    const std::string selection = activeSession ? activeSession->title : gm::localization::trUi("No active session");
    const std::string nextAction = gm::localization::trUi(activeSession ? (breakActive ? "Resume table before continuing." : "Use quick references, start break, or end from Advanced.") : "Start the session when the table is ready.");
    UiStatusActionStrip strip;
    strip.mode = mode.c_str();
    strip.selection = selection.c_str();
    strip.nextAction = nextAction.c_str();
    strip.help = "Break and resume markers are written to the current session notes.";
    drawStatusActionStrip(id, strip);
}

void GMScreenView::drawPublicDisplayStatusStrip(const char *id) const
{
    const LocalPlayerDisplayModel &display = _controller._currentCampaign.localPlayerDisplay;
    const char *modeKey = !display.enabled ? "Local display off" :
                          (localPlayerDisplayIsPublished(display) ? "Visible to players" : "Blackout active");
    const std::string mode = gm::localization::trUi(modeKey);
    const std::string selection = publicDisplaySelectionLabel(_controller._currentCampaign, display);
    const char *nextKey = !display.enabled ? "Open Public Display when ready to stage player-safe content." :
                          (localPlayerDisplayIsPublished(display) ? "Use Blackout to hide the player view immediately." : "Review the staged content, then show it to players.");
    const std::string nextAction = gm::localization::trUi(nextKey);
    UiStatusActionStrip strip;
    strip.mode = mode.c_str();
    strip.selection = selection.c_str();
    strip.nextAction = nextAction.c_str();
    strip.help = "Only selected public scene, map, initiative, NPCs, handouts, and shared art are sent to the local player display.";
    drawStatusActionStrip(id, strip);
}
