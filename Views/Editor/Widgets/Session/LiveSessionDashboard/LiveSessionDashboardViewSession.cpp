#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardViewSupport.hpp"

void LiveSessionDashboardView::drawSessionSupport()
{
    if (!ImGui::BeginTabBar("LiveSessionSupport")) return;
    if (ImGui::BeginTabItem(uiTextForLocale("Notes", "Notizen")))
    { drawGmSessionNotes(); ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem(uiTextForLocale("Audio", "Audio")))
    { drawAudioControls(); ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem(uiTextForLocale("Tools", "Werkzeuge")))
    {
        if (ImGui::CollapsingHeader(uiTextForLocale("Effects", "Effekte"))) drawEffectsWatch();
        if (ImGui::CollapsingHeader(uiTextForLocale("Clock and calendar", "Uhr und Kalender"))) drawClockCalendarControls();
        if (ImGui::CollapsingHeader(uiTextForLocale("Timeline", "Zeitverlauf"))) drawTimelineControls();
        if (ImGui::CollapsingHeader(uiTextForLocale("Quick rolls", "Schnellwürfe"))) drawQuickRolls();
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

void LiveSessionDashboardView::drawGmOnlyNotice(const char *sectionLabel)
{
    ImGui::SeparatorText(gm::localization::trUi(sectionLabel));
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveGmOnlyNotice));
}

void LiveSessionDashboardView::drawGmSessionNotes()
{
    if (_controller._currentCampaign.presentationModeActive)
    {
        drawGmOnlyNotice("Notes");
        return;
    }
    drawSessionNotes();
}

void LiveSessionDashboardView::drawGmPrepWrapTools()
{
    if (_controller._currentCampaign.presentationModeActive)
    {
        drawGmOnlyNotice("Prep / Wrap-up Tools");
        return;
    }

    drawSessionRecap();
    if (_controller.activeSession() && _controller._sessionReview.completedId.empty() &&
        ImGui::CollapsingHeader(uiTextForLocale("Session templates", "Sitzungsvorlagen"))) drawSessionTemplates();
    if (ImGui::CollapsingHeader(uiTextForLocale("Detailed report", "Ausführlicher Bericht"))) drawPostSessionReport();
}

void LiveSessionDashboardView::drawSceneLauncher()
{
    const auto *plannedSession = _controller.activeSession();
    if (plannedSession && !plannedSession->planEntries.empty())
    {
        const bool de = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
        ImGui::SeparatorText(de ? "Sitzungsplan" : "Session plan");
        // Keep a value snapshot because a successful command replaces campaign data.
        const SessionModel plan = *plannedSession;
        const auto *selected = SessionPlan::findEntry(plan, _controller._selectedPlanEntryId);
        if (!selected)
        {
            selected = SessionPlan::findEntry(plan, plan.liveState.activePlanEntryId);
            if (!selected) selected = &plan.planEntries.front();
            _controller._selectedPlanEntryId = selected->metadata.id;
        }
        const auto sceneLabel = [&](const SessionPlanEntry &entry, std::size_t index) {
            const auto *scene = SessionPlan::findScene(_controller._currentCampaign, entry.storyBeatId);
            const char *progress = entry.state == SessionEntryState::Completed ? (de ? "Erledigt" : "Done")
                : entry.state == SessionEntryState::Skipped ? (de ? "Übersprungen" : "Skipped") : "";
            const bool active = entry.metadata.id == plan.liveState.activePlanEntryId;
            std::string label = std::to_string(index + 1) + ". " +
                (scene ? scene->title : (de ? "Szene fehlt" : "Missing scene"));
            // Progress is deliberately shown before the live cursor: a scene can
            // remain the current context after it has been completed or skipped.
            if (*progress) label += std::string(" · ") + progress;
            if (active) label += std::string(" · ") + (de ? "Jetzt" : "Now");
            return label;
        };
        const auto selectedIndex = static_cast<std::size_t>(selected - plan.planEntries.data());
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::BeginCombo("##LivePlannedScene", sceneLabel(*selected, selectedIndex).c_str()))
        {
            for (std::size_t index = 0; index < plan.planEntries.size(); ++index)
            {
                const auto &entry = plan.planEntries[index];
                if (ImGui::Selectable((sceneLabel(entry, index) + "##" + entry.metadata.id).c_str(), selected == &entry))
                {
                    _controller._selectedPlanEntryId = entry.metadata.id;
                    selected = &entry;
                }
            }
            ImGui::EndCombo();
        }
        const auto *scene = SessionPlan::findScene(_controller._currentCampaign, selected->storyBeatId);
        if (scene)
        {
            if (!_controller._currentCampaign.presentationModeActive && !scene->summary.empty()) ImGui::TextWrapped("%s", scene->summary.c_str());
            if (!scene->readAloudText.empty()) { ImGui::SeparatorText(de ? "Vorlesen" : "Read aloud"); ImGui::TextWrapped("%s", scene->readAloudText.c_str()); }
            if (!_controller._currentCampaign.presentationModeActive)
            {
                if (!selected->prepNote.empty()) { ImGui::SeparatorText(de ? "Notizen für diese Sitzung" : "Notes for this session"); ImGui::TextWrapped("%s", selected->prepNote.c_str()); }
                if ((!scene->gmNotes.empty() || !scene->clues.empty() || !scene->consequences.empty()) &&
                    ImGui::CollapsingHeader(de ? "SL-Details" : "GM details"))
                {
                    if (!scene->gmNotes.empty()) ImGui::TextWrapped("%s", scene->gmNotes.c_str());
                    if (!scene->clues.empty()) { ImGui::TextDisabled("%s", de ? "Hinweise" : "Clues"); ImGui::TextWrapped("%s", scene->clues.c_str()); }
                    if (!scene->consequences.empty()) { ImGui::TextDisabled("%s", de ? "Folgen" : "Consequences"); ImGui::TextWrapped("%s", scene->consequences.c_str()); }
                }
            }
            const EntityId entryId = selected->metadata.id;
            const bool active = entryId == plan.liveState.activePlanEntryId;
            const bool final = selected->state == SessionEntryState::Completed || selected->state == SessionEntryState::Skipped;
            if (final)
            {
                ImGui::TextDisabled("%s", selected->state == SessionEntryState::Completed
                    ? (de ? "Diese Szene ist als erledigt markiert." : "This scene is marked complete.")
                    : (de ? "Diese Szene ist für diese Sitzung übersprungen." : "This scene is skipped for this session."));
                // updateEntry supports returning a completed or skipped entry to
                // Planned. For the active entry that resumes the current scene;
                // otherwise it makes the explicit Begin action available again.
                if (drawUiButton(active ? (de ? "Diese Szene fortsetzen" : "Resume current scene")
                                       : (de ? "Als geplant wieder öffnen" : "Reopen as planned"),
                                 UiButtonRole::Primary, ImVec2(-1, 0)))
                    _controller.markPlanEntry(entryId, SessionEntryState::Planned);
            }
            else
            {
                if (!active && drawUiButton(de ? "Diese Szene beginnen" : "Begin this scene", UiButtonRole::Primary, ImVec2(-1, 0)))
                    _controller.launchPlanEntry(entryId);
                if (drawUiButton(de ? "Als erledigt markieren" : "Mark completed", UiButtonRole::Secondary, ImVec2(-1, 0)))
                    _controller.markPlanEntry(entryId, SessionEntryState::Completed);
                if (drawUiButton(de ? "Für heute überspringen" : "Skip for this session", UiButtonRole::Secondary, ImVec2(-1, 0)))
                    _controller.markPlanEntry(entryId, SessionEntryState::Skipped);
            }
        }
        else ImGui::TextWrapped("%s", de ? "Repariere die Verknüpfung im Sitzungsplan oder entferne den Eintrag. Die übrigen Szenen bleiben verfügbar." : "Repair the link in the session plan or remove this entry. The other scenes remain available.");
        if (!_controller._sceneLaunchMessage.empty()) ImGui::TextWrapped("%s", _controller._sceneLaunchMessage.c_str());
        if (!_controller._sessionPlanMessage.empty()) ImGui::TextWrapped("%s", _controller._sessionPlanMessage.c_str());
        return;
    }
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveSceneLauncher));
    const std::vector<const StoryBeatModel *> scenes = _controller.sceneOptions();
    if (scenes.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveNoScenes));
        return;
    }

    if (_controller._selectedSceneIndex < 0 || _controller._selectedSceneIndex >= static_cast<int>(scenes.size()))
        _controller._selectedSceneIndex = 0;
    const StoryBeatModel *selectedBeat = scenes[static_cast<std::size_t>(_controller._selectedSceneIndex)];
    const std::string selectedSceneLabel = selectedBeat
                                               ? (selectedBeat->title.empty() ? gm::localization::trUi("Untitled scene") : selectedBeat->title)
                                               : gm::localization::tr(gm::localization::TextId::LiveSelectScene);
    const char *selectedLabel = selectedSceneLabel.c_str();
    if (FormUi::BeginCombo(gm::localization::tr(gm::localization::TextId::LiveScene), selectedLabel))
    {
        for (int i = 0; i < static_cast<int>(scenes.size()); ++i)
        {
            const StoryBeatModel *beat = scenes[static_cast<std::size_t>(i)];
            const std::string beatLabel = beat->title.empty() ? gm::localization::trUi("Untitled scene") : beat->title;
            if (ImGui::Selectable(beatLabel.c_str(), i == _controller._selectedSceneIndex))
                _controller._selectedSceneIndex = i;
        }
        ImGui::EndCombo();
    }

    if (selectedBeat)
    {
        int mapLinks = 0;
        int encounterLinks = 0;
        int npcLinks = 0;
        int itemLinks = 0;
        int audioLinks = selectedBeat->cueAudioAssetId.empty() ? 0 : 1;
        for (const StoryThreadModel &thread : _controller._currentCampaign.storyThreads)
        {
            for (const StoryBeatModel &beat : thread.beats)
            {
                if (beat.metadata.id != selectedBeat->metadata.id)
                    continue;
                std::vector<StoryLinkModel> links = thread.links;
                links.insert(links.end(), beat.links.begin(), beat.links.end());
                for (const StoryLinkModel &link : links)
                {
                    if (link.targetEntityType == "Map")
                        ++mapLinks;
                    else if (link.targetEntityType == "Encounter")
                        ++encounterLinks;
                    else if (link.targetEntityType == "NPC" || link.targetEntityType == "Monster")
                        ++npcLinks;
                    else if (link.targetEntityType == "Item")
                        ++itemLinks;
                    else if (link.targetEntityType == "Audio")
                        ++audioLinks;
                }
            }
        }
        ImGui::TextDisabled(localizedUiLabel("Links: maps %d, encounters %d, NPCs %d, items %d, audio %d").c_str(), mapLinks, encounterLinks, npcLinks, itemLinks, audioLinks);
    }

    if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveLaunchScene), UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Marks the scene active, opens linked workspaces, moves the party to linked maps, plays scene audio, and attaches linked encounters to the live session."))
        _controller.launchSelectedScene();
    if (!_controller._sceneLaunchMessage.empty())
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _controller._sceneLaunchMessage.c_str());
}

void LiveSessionDashboardView::drawPartyState()
{
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LivePartyState));
    if (_controller._currentCampaign.characters.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveNoPlayerCharacters));
        return;
    }

    if (ImGui::BeginTable("LivePartyState", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LivePlayer));
        ImGui::TableSetupColumn(localizedUiLabel("HP").c_str(), ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveLevel), ImGuiTableColumnFlags_WidthFixed, 54.0f);
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveClass));
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveConditions));
        ImGui::TableHeadersRow();
        for (CharacterModel character : _controller._currentCampaign.characters)
        {
            normalizeCharacterHitPoints(character);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(characterName(character));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(localizedUiLabel("%d/%d").c_str(), character.currentHitPoints, character.maxHitPoints);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text(localizedUiLabel("%d").c_str(), character.level);
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(localizedUiLabel(character.selectedClass.c_str()));
            ImGui::TableSetColumnIndex(4);
            tableTextWrapped(joinStrings(character.conditions));
        }
        ImGui::EndTable();
    }
}
