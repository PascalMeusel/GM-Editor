#include "Views/Editor/Presentation/PlayerReviewContent.hpp"
#include "Views/Editor/Presentation/PresentationReviewUi.hpp"
#include "Views/Editor/WorkspaceUi.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Views/Editor/Widgets/FrozenImageUi.hpp"
namespace gm::player_review_ui
{
void drawHandoutImage(const FrozenImageAsset &asset,const HandoutTextureLookup &lookup)
{
    FrozenImageUi::slot(asset,220.f,uiTextForLocale("Loading reviewed image…","Geprüftes Bild wird geladen…"),
        uiTextForLocale("Handout image is unavailable.","Das Handout-Bild ist nicht verfügbar."),lookup);
}
void drawContent(const PlayerReviewSession &session)
{
    const auto &review=session.review();const auto &display=session.display();
    ImGui::SeparatorText(localizedUiText("Visible to players"));
    if (display.mapOnly)
        ImGui::TextWrapped("%s", uiTextForLocale(
            "Map only. Scene text, initiative, people and handouts are not shown in this layout.",
            "Nur Karte. Szenentexte, Initiative, Personen und Handouts werden in dieser Ansicht nicht angezeigt."));
    else if (review.eligible)
    {
        // Display the immutable public projection that this review approves.
        // A list of source names alone cannot tell a GM what they are sharing.
        const auto &scene = review.playerScene;
        if (scene.available)
        {
            ImGui::TextColored(WorkspaceUi::Teal, "%s", scene.title.c_str());
            if (!scene.playerSummary.empty()) ImGui::TextWrapped("%s", scene.playerSummary.c_str());
            if (!scene.readAloudText.empty())
            {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", scene.readAloudText.c_str());
            }
        }
        if (!review.playerInitiative.empty())
        {
            ImGui::SeparatorText(localizedUiLabel("Initiative").c_str());
            ImGui::TextWrapped("%s", review.playerEncounterName.c_str());
            ImGui::Text(localizedUiLabel("Round %d").c_str(), review.playerEncounterRound);
            for (const auto &entry : review.playerInitiative)
                ImGui::TextWrapped("%s%s (%d)", entry.active ? uiTextForLocale("Current turn: ", "Am Zug: ") : "",
                    entry.displayName.c_str(), entry.initiative);
        }
        if (!review.playerParty.empty()) ImGui::SeparatorText(uiTextForLocale("Party", "Gruppe"));
        for (const auto &character : review.playerParty)
        {
            ImGui::TextWrapped("%s · %s · %s %d", character.name.c_str(),
                localizedUiLabel(character.characterClass.c_str()).c_str(), uiTextForLocale("Level", "Stufe"), character.level);
            ImGui::Text(uiTextForLocale("HP %d / %d · Temporary %d · Speed %d ft", "TP %d / %d · Temporär %d · Bewegung %d Fuß"),
                character.currentHitPoints, character.maxHitPoints, character.temporaryHitPoints, character.speedFeet);
            for (const auto &slot : character.spellSlots)
                ImGui::TextWrapped(uiTextForLocale("Level %d spell slots: %d / %d remaining", "Zauberplätze Grad %d: %d / %d übrig"),
                    slot.level, slot.available, slot.maximum);
            for (const auto &condition : character.conditions)
                ImGui::TextWrapped("%s", localizedUiLabel(condition.c_str()).c_str());
            ImGui::Spacing();
        }
        if (!review.playerInventory.empty())
            ImGui::SeparatorText(uiTextForLocale("Shared inventory", "Geteiltes Inventar"));
        for (const PlayerSafeInventoryItemSummary &item : review.playerInventory)
            ImGui::TextWrapped("%s ×%d · %s", item.itemName.c_str(), item.quantity,
                item.partyOwned ? uiTextForLocale("Party Inventory", "Gruppeninventar") : item.ownerName.c_str());
        if (!review.playerObjectives.empty()) ImGui::SeparatorText(localizedUiLabel("Objectives").c_str());
        for (const auto &objective : review.playerObjectives)
        {
            gm::presentation_review_ui::drawWrappedBullet(objective.title);
            const char *status = objective.status == StoryObjectiveStatus::Active ? uiTextForLocale("Active", "Aktiv") :
                objective.status == StoryObjectiveStatus::Completed ? uiTextForLocale("Completed", "Abgeschlossen") :
                objective.status == StoryObjectiveStatus::Failed ? uiTextForLocale("Failed", "Gescheitert") :
                uiTextForLocale("Planned", "Geplant");
            ImGui::TextColored(WorkspaceUi::Teal, "%s", status);
            if (!objective.summary.empty()) ImGui::TextWrapped("%s", objective.summary.c_str());
        }
        if (!review.playerClues.empty()) ImGui::SeparatorText(localizedUiLabel("Discovered clues").c_str());
        for (const auto &clue : review.playerClues)
        {
            gm::presentation_review_ui::drawWrappedBullet(clue.title);
            if (!clue.text.empty()) ImGui::TextWrapped("%s", clue.text.c_str());
        }
        if (!review.playerNpcs.empty()) ImGui::SeparatorText(localizedUiLabel("Visible NPCs").c_str());
        for (const auto &npc : review.playerNpcs)
        {
            gm::presentation_review_ui::drawWrappedBullet(npc.name);
            if (!npc.role.empty()) ImGui::TextWrapped("%s", npc.role.c_str());
        }
        if (!review.playerHandouts.empty()) ImGui::SeparatorText(localizedUiLabel("Handouts").c_str());
        for (const auto &handout : review.playerHandouts)
        {
            gm::presentation_review_ui::drawWrappedBullet(handout.title);
            if (!handout.body.empty()) ImGui::TextWrapped("%s", handout.body.c_str());
            if (!handout.projectRelativePath.empty())
                drawHandoutImage(session.playerImage(handout.projectRelativePath,handout.contentHash),
                    [](const FrozenImageAsset &asset)->const CachedImageTexture & {
                        return getAsyncImageTexture(asset,static_cast<std::uint64_t>(ImGui::GetFrameCount()));});
        }
        if (!review.playerSafetyBoundaries.empty() || !review.playerContentWarnings.empty())
            ImGui::SeparatorText(uiTextForLocale("Table agreements", "Absprachen am Spieltisch"));
        for (const auto &boundary : review.playerSafetyBoundaries)
        {
            gm::presentation_review_ui::drawWrappedBullet(boundary.topic);
            if (!boundary.guidance.empty()) ImGui::TextWrapped("%s", boundary.guidance.c_str());
        }
        for (const auto &warning : review.playerContentWarnings)
            ImGui::TextWrapped("%s: %s", warning.targetLabel.c_str(), warning.warning.c_str());
    }
    ImGui::SeparatorText(localizedUiLabel("Hidden from players").c_str());
    gm::presentation_review_ui::drawWrappedBullet(
        localizedUiLabel("GM notes, secrets, private statistics, hidden tokens, and unrevealed story links."));
}
}
