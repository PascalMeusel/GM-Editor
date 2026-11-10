#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardViewSupport.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"

void LiveSessionDashboardView::drawLiveCoreSummary()
{
    const Campaign &campaign = _controller._currentCampaign;
    const StoryThreadModel *thread = _controller.activeStoryThread();
    const StoryBeatModel *beat = _controller.activeStoryBeat();
    const MapModel *map = _controller.activeMap();
    const EncounterModel *encounter = _controller.activeEncounter();
    const std::vector<EntityId> activeAudio = _controller._playbackService ? _controller._playbackService->activeAssetIds() : std::vector<EntityId>{};
    int pausedAudio = 0;
    for (const EntityId &assetId : activeAudio)
    {
        if (_controller._playbackService && _controller._playbackService->isPaused(assetId))
            ++pausedAudio;
    }

    int woundedCount = 0;
    int conditionCount = 0;
    for (CharacterModel character : campaign.characters)
    {
        normalizeCharacterHitPoints(character);
        if (character.currentHitPoints < character.maxHitPoints)
            ++woundedCount;
        conditionCount += static_cast<int>(character.conditions.size());
    }

    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const std::string partyLine = std::to_string(campaign.characters.size()) + (german ? " Figuren" : " PCs") +
                                  (woundedCount > 0 ? ", " + std::to_string(woundedCount) + (german ? " verwundet" : " wounded") : "") +
                                  (conditionCount > 0 ? ", " + std::to_string(conditionCount) + (german ? " Zustände" : " conditions") : "");
    const std::string sceneLine = sceneSelectionLabel(thread, beat) + "\n" +
                                  (german ? "Karte: " : "Map: ") + mapSelectionLabel(map);
    const std::string encounterLine = encounterSelectionLabel(encounter) +
                                      (encounter && encounterHasStarted(encounter)
                                           ? std::string("\n") + (german ? "Runde " : "Round ") + std::to_string(encounter->round)
                                           : std::string("\n") + (german ? "Vorbereitet" : "Prepared"));
    const std::string gameTimeLabel = _controller.currentInWorldClockLabel();
    const std::string clockLine = _controller.activeSession()
                                      ? (_controller.isSessionBreakActive() ? (german ? "Pause, Uhr angehalten\n" : "Break paused\n") : (german ? "Läuft\n" : "Running\n")) + (gameTimeLabel == "-" ? (german ? "Spielzeit nicht gesetzt" : "Game time not set") : gameTimeLabel)
                                      : (german ? std::string("Vorbereitung\nKeine Live-Sitzung") : std::string("Prep\nNo live session"));
    const std::string audioLine = activeAudio.empty()
                                      ? (german ? "Inaktiv" : "Idle")
                                      : std::to_string(static_cast<int>(activeAudio.size())) + (german ? " aktiv, " : " active, ") + std::to_string(pausedAudio) + (german ? " pausiert" : " paused");

    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveCore));
    showHelpTooltip(gm::localization::trUi("The live core keeps party, scene/map, encounter, clock, and audio visible regardless of the selected phase."));
    if (ImGui::BeginTable("LiveSessionCoreSummary", 5, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveParty));
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveSceneMap));
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveEncounter));
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveGameTime));
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveAudio));
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        tableTextWrapped(partyLine);
        ImGui::TableSetColumnIndex(1);
        tableTextWrapped(sceneLine);
        ImGui::TableSetColumnIndex(2);
        tableTextWrapped(encounterLine);
        ImGui::TableSetColumnIndex(3);
        tableTextWrapped(clockLine);
        ImGui::TableSetColumnIndex(4);
        tableTextWrapped(audioLine);
        ImGui::EndTable();
    }
}

void LiveSessionDashboardView::drawPhaseSelector()
{
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LivePhaseFocus));
    const LiveSessionDashboardPhase selectedPhase = _controller.dashboardPhase();
    const int phaseCount = static_cast<int>(sizeof(kDashboardPhases) / sizeof(kDashboardPhases[0]));
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columnCount = availableWidth >= 900.0f ? phaseCount : (availableWidth >= 480.0f ? 3 : 2);
    if (ImGui::BeginTable("LiveSessionPhaseSelector", columnCount,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        for (int i = 0; i < phaseCount; ++i)
        {
            const LiveSessionDashboardPhase phase = kDashboardPhases[i];
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            const UiButtonRole role = phase == selectedPhase ? UiButtonRole::Primary : UiButtonRole::Secondary;
            const std::string label = phaseLabel(phase);
            if (drawUiButton(label.c_str(), role, ImVec2(-1.0f, 0.0f), phaseHelp(phase)))
                _controller.selectDashboardPhase(phase);
            ImGui::PopID();
        }
        if (_controller._dashboardPhaseManuallySelected)
        {
            ImGui::TableNextColumn();
            if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveUseCurrent), UiButtonRole::Advanced,
                             ImVec2(-1.0f, 0.0f), "Return the dashboard to the phase inferred from session state."))
                _controller.useCurrentRecap();
        }
        ImGui::EndTable();
    }
}

void LiveSessionDashboardView::drawPhaseWorkspace(LiveSessionDashboardPhase phase)
{
    const std::string header = phaseHeaderLabel(phase);
    ImGui::SeparatorText(header.c_str());
    ImGui::TextWrapped("%s", phaseHelp(phase));
    UiGuidance::drawDetails(UiGuidance::Topic::SessionPhases);
    drawPhaseControls(phase);
}

void LiveSessionDashboardView::drawPhaseAdvancedSections(LiveSessionDashboardPhase currentPhase)
{
    ImGui::Separator();
    if (!ImGui::CollapsingHeader(gm::localization::tr(gm::localization::TextId::LiveAdvancedSections)))
        return;

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveAdvancedHint));
    ImGui::PopStyleColor();
    for (LiveSessionDashboardPhase phase : kDashboardPhases)
    {
        if (phase == currentPhase)
            continue;

        ImGui::PushID(static_cast<int>(phase));
        const std::string header = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LivePhaseTools,
            {phaseLabel(phase)});
        if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            drawPhaseControls(phase);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void LiveSessionDashboardView::drawPhaseControls(LiveSessionDashboardPhase phase)
{
    switch (phase)
    {
    case LiveSessionDashboardPhase::Prep:
        if (ImGui::BeginTable("LiveSessionPrepLayout", responsivePhaseColumnCount(2), ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawSessionLifecycleControls();
            drawCompletedRecapEntry();
            if (ImGui::CollapsingHeader(uiTextForLocale("Party and preparation tools", "Gruppe und Vorbereitung")))
            { drawPartyState(); drawGmPrepWrapTools(); }
            ImGui::TableNextColumn();
            drawSceneLauncher();
            if (ImGui::CollapsingHeader(uiTextForLocale("Linked material", "Verknüpfte Inhalte"))) drawSceneMapEncounter();
            ImGui::EndTable();
        }
        break;
    case LiveSessionDashboardPhase::RunningScene:
        if (ImGui::BeginTable("LiveSessionRunningSceneLayout", responsivePhaseColumnCount(2), ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawSceneLauncher();
            if (ImGui::CollapsingHeader(uiTextForLocale("Linked material", "Verknüpfte Inhalte"))) drawSceneMapEncounter();
            ImGui::TableNextColumn();
            drawSessionLifecycleControls();
            drawSessionSupport();
            ImGui::EndTable();
        }
        break;
    case LiveSessionDashboardPhase::ExplorationTravel:
        if (ImGui::BeginTable("LiveSessionExplorationLayout", responsivePhaseColumnCount(2), ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawSceneMapEncounter();
            if (ImGui::CollapsingHeader(uiTextForLocale("Party", "Gruppe"))) drawPartyState();
            ImGui::TableNextColumn();
            drawSessionLifecycleControls();
            drawSessionSupport();
            ImGui::EndTable();
        }
        break;
    case LiveSessionDashboardPhase::Combat:
        drawCombatCommandCenter();
        if (ImGui::CollapsingHeader(gm::localization::trUi("Live support during combat")))
        {
            if (ImGui::BeginTable("LiveSessionCombatSupportLayout", responsivePhaseColumnCount(2), ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                drawEffectsWatch();
                drawGmSessionNotes();
                ImGui::TableNextColumn();
                drawSessionLifecycleControls();
                drawAudioControls();
                ImGui::EndTable();
            }
        }
        break;
    case LiveSessionDashboardPhase::Break:
        if (ImGui::BeginTable("LiveSessionBreakLayout", responsivePhaseColumnCount(2), ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawSessionLifecycleControls();
            if (ImGui::CollapsingHeader(uiTextForLocale("Audio", "Audio"))) drawAudioControls();
            ImGui::TableNextColumn();
            drawGmSessionNotes();
            if (ImGui::CollapsingHeader(uiTextForLocale("Quick rolls", "Schnellwürfe"))) drawQuickRolls();
            ImGui::EndTable();
        }
        break;
    case LiveSessionDashboardPhase::WrapUp:
    {
        const int columns = responsivePhaseColumnCount(2);
        if (ImGui::BeginTable("LiveSessionWrapUpLayout", columns, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("##WrapUpRecap", ImGuiTableColumnFlags_WidthStretch, 0.64f);
            if (columns > 1)
                ImGui::TableSetupColumn("##WrapUpSupport", ImGuiTableColumnFlags_WidthStretch, 0.36f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawGmPrepWrapTools();
            ImGui::TableNextColumn();
            drawSessionLifecycleControls();
            if (ImGui::CollapsingHeader(uiTextForLocale("Session notes", "Sitzungsnotizen"))) drawGmSessionNotes();
            if (ImGui::CollapsingHeader(uiTextForLocale("Timeline", "Zeitverlauf"))) drawTimelineControls();
            if (ImGui::CollapsingHeader(uiTextForLocale("Party", "Gruppe"))) drawPartyState();
            ImGui::EndTable();
        }
        break;
    }
    }
}
