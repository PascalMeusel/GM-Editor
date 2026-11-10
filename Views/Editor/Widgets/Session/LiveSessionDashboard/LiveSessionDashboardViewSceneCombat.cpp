#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardViewSupport.hpp"
#include "Models/Session/TravelSessionIntegration.hpp"

void LiveSessionDashboardView::drawSceneMapEncounter()
{
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const StoryBeatModel *beat = _controller.activeStoryBeat();
    EntityRegistry::getInstance().rebuild(_controller._currentCampaign);

    // Scene text is already shown in the scene launcher. Keep this area for
    // actionable references, and own link IDs across navigation/save callbacks.
    if (beat && !_controller._currentCampaign.presentationModeActive)
    {
        const auto links = beat->links;
        for (std::size_t i = 0; i < links.size(); ++i)
        {
            const auto link = links[i];
            const auto *entity = EntityRegistry::getInstance().find(link.targetEntityId);
            std::string name = entity ? entity->displayName : link.label;
            const HandoutModel *handout = nullptr;
            if (link.targetEntityType == "Handout")
                for (const auto &candidate : _controller._currentCampaign.handouts)
                    if (candidate.metadata.id == link.targetEntityId) { handout = &candidate; name = candidate.title; break; }
            ImGui::PushID(static_cast<int>(i));
            ImGui::TextWrapped("%s", name.empty() ? (german ? "Fehlender Inhalt" : "Missing material") : name.c_str());
            ImGui::BeginDisabled((!entity && !handout) || (!handout && !_controller._openEntityHandler));
            if (drawUiButton(german ? "Öffnen" : "Open", UiButtonRole::Secondary))
            {
                if (handout) ImGui::OpenPopup("LiveHandout");
                else
                {
                    _controller._openEntityHandler(link.targetEntityType, link.targetEntityId);
                    ImGui::EndDisabled();
                    ImGui::PopID();
                    return;
                }
            }
            ImGui::EndDisabled();
            if (ImGui::BeginPopup("LiveHandout"))
            {
                if (handout)
                {
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 340.0f);
                    ImGui::TextUnformatted(handout->title.c_str());
                    ImGui::Separator();
                    ImGui::TextUnformatted(handout->body.c_str());
                    if (!handout->gmNotes.empty())
                    {
                        ImGui::Spacing();
                        ImGui::TextDisabled("%s", german ? "SL-Notizen" : "GM notes");
                        ImGui::TextUnformatted(handout->gmNotes.c_str());
                    }
                    ImGui::PopTextWrapPos();
                }
                ImGui::EndPopup();
            }
            if (!entity && !handout) ImGui::TextDisabled("%s", german ? "Verknüpfung fehlt" : "Link missing");
            ImGui::PopID();
        }
    }
    const MapModel *map = _controller.activeMap();
    if (map && (!beat || std::none_of(beat->links.begin(), beat->links.end(), [&](const StoryLinkModel &link) { return link.targetEntityId == map->metadata.id; })))
    {
        ImGui::TextWrapped("%s", map->name.c_str());
        if (_controller._openEntityHandler && drawUiButton(german ? "Karte öffnen" : "Open map", UiButtonRole::Secondary))
        {
            _controller._openEntityHandler("Map", map->metadata.id);
            return;
        }
    }

    if (!_controller._currentCampaign.presentationModeActive)
    {
        const EntityId source = TravelSession::geographicSource(_controller._currentCampaign, map ? map->metadata.id : EntityId{});
        ImGui::BeginDisabled(source.empty() || !_controller._openMapHandler);
        if (drawUiButton(german ? "Von diesem Ort / dieser Route improvisieren" : "Improvise from this place / route", UiButtonRole::Secondary))
        {
            _controller._openMapHandler(source, true);
            ImGui::EndDisabled();
            return;
        }
        ImGui::EndDisabled();
        if (source.empty()) ImGui::TextDisabled("%s", german ? "Wähle zuerst eine geografische Karte im Atlas." : "Choose a geographic map in Atlas first.");
        if (map && _controller._openMapHandler && drawUiButton(german ? "Vorhandene Karten durchsuchen" : "Browse existing maps", UiButtonRole::Secondary))
        {
            const auto id = map->metadata.id;
            _controller._openMapHandler(id, false);
            return;
        }
    }

    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveActiveEncounter));
    const EncounterModel *encounter = _controller.activeEncounter();
    if (encounter)
    {
        const std::string encounterLabel = encounterSelectionLabel(encounter);
        drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveEncounter), encounterLabel.c_str());
        ImGui::Text(localizedUiLabel("%s: %d").c_str(), gm::localization::tr(gm::localization::TextId::LiveRound), std::max(1, encounter->round));
        ImGui::Text(localizedUiLabel("%s: %d").c_str(), gm::localization::tr(gm::localization::TextId::LiveCombatants), static_cast<int>(encounter->participants.size()));
        ImGui::Text(localizedUiLabel("%s: %s").c_str(),
                    gm::localization::tr(gm::localization::TextId::LiveState),
                    encounter->combatPaused ? gm::localization::tr(gm::localization::TextId::LivePaused)
                                            : gm::localization::tr(gm::localization::TextId::CommonReady));
        const char *combatAction = encounterHasStarted(encounter) || encounter->combatPaused ? "Continue Combat" : "Set Up Combat";
        if (drawUiButton(combatAction, UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Move this prepared encounter into the Live combat phase and show its complete turn controls."))
            _controller.openActiveEncounter();
    }
    else
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveNoEncounter));
        if (drawUiButton("Prepare Encounter", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Open Encounters to prepare a roster and battlefield before running combat."))
            _controller.openActiveEncounter();
    }
}

void LiveSessionDashboardView::drawCombatCommandCenter()
{
    const EncounterModel *encounter = _controller.activeEncounter();
    if (!encounter)
    {
        ImGui::TextWrapped(localizedUiLabel("No encounter is staged. Prepare an encounter first; it will return here with initiative, turns, actions, effects, and the battlefield in one place.").c_str());
        if (drawUiButton("Prepare Encounter", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Open Encounters to prepare a roster and battlefield before running combat."))
            _controller.openActiveEncounter();
        return;
    }

    ImGui::TextUnformatted(encounter->name.c_str());
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    ImGui::SameLine();
    if (drawSmallUiButton(german ? "Begegnung bearbeiten" : "Edit encounter", UiButtonRole::Secondary,
        "Return to Encounters to change roster, waves, terrain, or starting positions."))
    {
        _controller.openEncounterPreparation();
        return;
    }

    ImGui::PushID("EmbeddedLiveCombat");
    _controller.renderActiveCombatWorkspace();
    ImGui::PopID();
}

void LiveSessionDashboardView::drawEffectsWatch()
{
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveEffects));
    const EncounterModel *encounter = _controller.activeEncounter();
    const std::vector<EffectsWatchRow> rows = buildEffectsWatchRows(_controller._currentCampaign, encounter);
    int concentrationCount = 0;
    int expiringCount = 0;
    for (const EffectsWatchRow &row : rows)
    {
        if (row.concentration)
            ++concentrationCount;
        if (row.expiring)
            ++expiringCount;
    }

    UiStatusActionStrip strip;
    const std::string watchedCount = std::to_string(rows.size());
    const std::string concentration = std::to_string(concentrationCount);
    const std::string expiring = std::to_string(expiringCount);
    const std::string mode = rows.empty()
                                 ? gm::localization::tr(gm::localization::TextId::LiveEffectsClear)
                                 : gm::localization::LocalizationService::instance().format(gm::localization::TextId::LiveEffectsWatched, {watchedCount});
    const std::string selection = gm::localization::LocalizationService::instance().format(
        gm::localization::TextId::LiveEffectsCounts,
        {concentration, expiring});
    const std::string next = gm::localization::tr(rows.empty() ? gm::localization::TextId::LiveEffectsNoneNext
                                                               : gm::localization::TextId::LiveEffectsResolveNext);
    strip.mode = mode.c_str();
    strip.selection = selection.c_str();
    strip.nextAction = next.c_str();
    strip.help = "Effects Watch gathers party/NPC conditions and active encounter effects so the GM can see concentration, durations, and reminders without opening every actor detail panel.";
    drawStatusActionStrip("LiveSessionEffectsWatchStatus", strip);

    if (rows.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveNoEffects));
        return;
    }

    if (ImGui::BeginTable("LiveSessionEffectsWatchTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, 220.0f)))
    {
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveOwner), ImGuiTableColumnFlags_WidthStretch, 0.22f);
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::CommonType), ImGuiTableColumnFlags_WidthStretch, 0.18f);
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveEffectCondition), ImGuiTableColumnFlags_WidthStretch, 0.28f);
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveDuration), ImGuiTableColumnFlags_WidthStretch, 0.14f);
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveReminder), ImGuiTableColumnFlags_WidthStretch, 0.18f);
        ImGui::TableHeadersRow();
        for (const EffectsWatchRow &row : rows)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(row.owner);
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(row.type);
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(row.name);
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(row.duration);
            ImGui::TableSetColumnIndex(4);
            tableTextWrapped(row.reminder);
        }
        ImGui::EndTable();
    }
}
