#include "Views/Editor/Widgets/Session/LocalPlayerDisplay/LocalPlayerDisplayViewSupport.hpp"
#include "Views/Editor/Widgets/FrozenImageUi.hpp"

void LocalPlayerDisplayView::drawInitiativePanel()
{
    const PlayerDisplayReviewSnapshot &review = _controller._reviewCache;
    if (review.playerInitiative.empty())
        return;
    ImGui::SeparatorText(localizedUiLabel("Initiative").c_str());
    const std::string encounterName = review.playerEncounterName.empty() ? localizedUiLabel("Encounter") : review.playerEncounterName;
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), encounterName.c_str());
    ImGui::TextDisabled(localizedUiLabel("Round %d").c_str(), review.playerEncounterRound);
    for (std::size_t index = 0; index < review.playerInitiative.size(); ++index)
    {
        if (!review.playerInitiative[index].active) continue;
        ImGui::TextColored(kAccent, uiTextForLocale("Current turn: %s", "Aktueller Zug: %s"),
                           review.playerInitiative[index].displayName.c_str());
        if (review.playerInitiative.size() > 1)
            ImGui::TextWrapped(uiTextForLocale("Next: %s", "Danach: %s"),
                review.playerInitiative[(index + 1) % review.playerInitiative.size()].displayName.c_str());
        break;
    }
    if (ImGui::BeginTable("PlayerDisplayInitiative", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Turn").c_str(), ImGuiTableColumnFlags_WidthFixed, 42.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Name").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Init").c_str(), ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableHeadersRow();
        for (const PlayerSafeInitiativeEntry &entry : review.playerInitiative)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entry.active ? ">>" : "");
            ImGui::TableSetColumnIndex(1);
            const std::string combatantName = entry.displayName.empty() ? localizedUiLabel("Combatant") : entry.displayName;
            tableTextWrapped(combatantName);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text(localizedUiLabel("%d").c_str(), entry.initiative);
        }
        ImGui::EndTable();
    }
}

void LocalPlayerDisplayView::drawPartyPanel()
{
    const auto &party = _controller._reviewCache.playerParty;
    if (party.empty()) return;
    gm::approved_ui::publicSection(uiTextForLocale("Party", "Gruppe"));
    const int columns = std::min(static_cast<int>(party.size()),
        std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x / (ImGui::GetFontSize() * 18.0f)), 1, 3));
    if (!ImGui::BeginTable("PublicParty", columns, ImGuiTableFlags_SizingStretchSame)) return;
    for (const auto &character : party)
    {
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
        ImGui::TextWrapped("%s", character.name.c_str());
        ImGui::PopStyleColor();
        ImGui::TextWrapped("%s · %s %d", localizedUiLabel(character.characterClass.c_str()).c_str(),
            uiTextForLocale("Level", "Stufe"), character.level);
        if (character.speedFeet > 0)
            ImGui::TextWrapped(uiTextForLocale("HP %d / %d · Speed %d ft", "TP %d / %d · Bewegung %d Fuß"),
                character.currentHitPoints, character.maxHitPoints, character.speedFeet);
        else
            ImGui::Text(uiTextForLocale("HP %d / %d", "TP %d / %d"), character.currentHitPoints, character.maxHitPoints);
        if (character.temporaryHitPoints > 0)
        {
            ImGui::TextWrapped(uiTextForLocale("+%d temporary HP", "+%d temporäre TP"), character.temporaryHitPoints);
        }
        for (const auto &slot : character.spellSlots)
            ImGui::TextWrapped(uiTextForLocale("Level %d spell slots: %d / %d remaining", "Zauberplätze Grad %d: %d / %d übrig"),
                slot.level, slot.available, slot.maximum);
        for (const auto &condition : character.conditions)
            ImGui::TextWrapped("%s", localizedUiLabel(condition.c_str()).c_str());
        ImGui::Spacing();
    }
    ImGui::EndTable();
}

void LocalPlayerDisplayView::drawInventoryPanel()
{
    const auto &inventory = _controller._reviewCache.playerInventory;
    if (inventory.empty()) return;
    gm::approved_ui::publicSection(uiTextForLocale("Shared inventory", "Geteiltes Inventar"));
    for (const PlayerSafeInventoryItemSummary &item : inventory)
    {
        ImGui::Bullet();
        ImGui::SameLine();
        ImGui::TextWrapped("%s ×%d · %s", item.itemName.c_str(), item.quantity,
            item.partyOwned ? uiTextForLocale("Party Inventory", "Gruppeninventar") : item.ownerName.c_str());
    }
}

void LocalPlayerDisplayView::drawVisibleNpcsPanel()
{
    const std::vector<PlayerSafeNpcSummary> &npcs = _controller._reviewCache.playerNpcs;
    if (npcs.empty()) return;
    ImGui::SeparatorText(localizedUiLabel("Visible NPCs").c_str());
    bool anyVisible = false;
    for (const PlayerSafeNpcSummary &npc : npcs)
    {
        anyVisible = true;
        const std::string npcName = npc.name.empty() ? localizedUiLabel("NPC") : npc.name;
        ImGui::Bullet();
        ImGui::SameLine();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), npcName.c_str());
        if (!npc.role.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), npc.role.c_str());
            ImGui::PopStyleColor();
        }
    }

    if (!anyVisible)
        ImGui::TextDisabled(localizedUiLabel("No NPCs currently revealed.").c_str());
}

void LocalPlayerDisplayView::drawHandoutsPanel()
{
    // Use the immutable, byte-bound projection approved by the GM. Re-reading
    // campaign handouts here would create a post-review TOCTOU channel.
    const std::vector<PlayerSafeHandout> &handouts = _controller._reviewCache.playerHandouts;
    if (handouts.empty())
        return;
    ImGui::SeparatorText(localizedUiLabel("Handouts").c_str());

    for (const PlayerSafeHandout &handout : handouts)
    {
        const std::string handoutTitle = handout.title.empty() ? localizedUiLabel("Handout") : handout.title;
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), handoutTitle.c_str());
        if (!handout.body.empty())
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), handout.body.c_str());
        if (!handout.projectRelativePath.empty())
        {
            FrozenImageUi::slot(_controller._publicationAssets.image(handout.projectRelativePath,handout.contentHash),220.f,
                uiTextForLocale("Loading shared image…","Geteiltes Bild wird geladen…"),
                uiTextForLocale("Shared image is unavailable.","Das geteilte Bild ist nicht verfügbar."),FrozenImageUi::lookup);
        }
        ImGui::Separator();
    }
}

void LocalPlayerDisplayView::drawStoryTrackingPanel()
{
    const PlayerDisplayReviewSnapshot &review = _controller._reviewCache;
    if (review.playerObjectives.empty() && review.playerClues.empty()) return;

    if (!review.playerObjectives.empty())
    {
        gm::approved_ui::publicSection(localizedUiLabel("Objectives").c_str());
        // Keep the next decision ahead of finished goals, without altering
        // the reviewed projection or hiding past progress from players.
        for (const StoryObjectiveStatus group : {StoryObjectiveStatus::Active,
                 StoryObjectiveStatus::Planned, StoryObjectiveStatus::Completed, StoryObjectiveStatus::Failed})
        for (const PlayerSafeObjective &objective : review.playerObjectives)
        {
            if (objective.status != group) continue;
            ImGui::Bullet(); ImGui::SameLine(); ImGui::TextWrapped("%s", objective.title.c_str());
            const char *status = objective.status == StoryObjectiveStatus::Active ? uiTextForLocale("Active", "Aktiv") :
                objective.status == StoryObjectiveStatus::Completed ? uiTextForLocale("Completed", "Abgeschlossen") :
                objective.status == StoryObjectiveStatus::Failed ? uiTextForLocale("Failed", "Gescheitert") :
                uiTextForLocale("Planned", "Geplant");
            ImGui::TextColored(kAccent, "%s", status);
            if (!objective.summary.empty()) ImGui::TextWrapped("%s", objective.summary.c_str());
        }
    }
    if (!review.playerClues.empty())
    {
        gm::approved_ui::publicSection(localizedUiLabel("Discovered clues").c_str());
        for (const PlayerSafeClue &clue : review.playerClues)
        {
            ImGui::Bullet(); ImGui::SameLine(); ImGui::TextWrapped("%s", clue.title.c_str());
            if (!clue.text.empty()) ImGui::TextWrapped("%s", clue.text.c_str());
        }
    }
}

void LocalPlayerDisplayView::drawSafetySummaryPanel()
{
    const PlayerDisplayReviewSnapshot &review = _controller._reviewCache;
    if (review.playerSafetyBoundaries.empty() && review.playerContentWarnings.empty()) return;

    gm::approved_ui::publicSection(localizedUiLabel("Table safety").c_str());
    for (const PlayerSafeSafetyBoundary &boundary : review.playerSafetyBoundaries)
    {
        const char *kind = boundary.kind == SafetyBoundaryKind::Line ? "Line" : "Veil";
        ImGui::TextWrapped("%s: %s", kind, boundary.topic.c_str());
        if (!boundary.guidance.empty()) ImGui::TextDisabled("%s", boundary.guidance.c_str());
    }
    for (const PlayerSafeContentWarning &warning : review.playerContentWarnings)
        ImGui::TextWrapped("%s: %s", warning.targetLabel.c_str(), warning.warning.c_str());
}

void LocalPlayerDisplayView::drawSharedArtPanel(const MapModel *map, const MapRenderPlan *plan)
{
    ImGui::SeparatorText(localizedUiLabel("Shared Art").c_str());
    // Managed imported map art is already rendered by the reviewed, cropped,
    // rotated and fog-aware map surface. Repeating the raw proxy here would
    // expose pixels outside the calibrated crop and bypass map fog entirely.
    if (map != nullptr && map->importedReference.enabled)
    {
        ImGui::TextDisabled(localizedUiLabel("No shared art selected.").c_str());
        return;
    }
    if (map == nullptr || plan == nullptr || plan->audience != MapRenderAudience::Player ||
        !mapRenderPlanMatchesMap(*plan, *map) || !plan->backgroundVisible ||
        map->backgroundImagePath.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No shared art selected.").c_str());
        return;
    }

    FrozenImageUi::slot(_controller._publicationAssets.image(plan->backgroundInteractiveProjectRelativePath,
        plan->backgroundInteractiveContentHash),260.f,uiTextForLocale("Loading shared image…","Geteiltes Bild wird geladen…"),
        uiTextForLocale("Shared image is unavailable.","Das geteilte Bild ist nicht verfügbar."),FrozenImageUi::lookup);
}
