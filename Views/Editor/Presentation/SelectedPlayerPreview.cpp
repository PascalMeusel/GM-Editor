#include "SelectedPlayerPreview.hpp"
#include "Models/Session/PlayerSafeSelectionPreview.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "Views/Editor/Widgets/UiPopupPlacement.hpp"
#include <algorithm>

namespace SelectedPlayerPreview
{
namespace
{
const char *stateText(PlayerSafeSelection::State state)
{
    using State = PlayerSafeSelection::State;
    switch (state)
    {
    case State::Staged: return uiTextForLocale("Selected for the next player review. Review and explicitly show this content through Player Display.",
        "Für die nächste Spielerprüfung ausgewählt. Prüfe diese Inhalte und zeige sie ausdrücklich über die Spieleranzeige.");
    case State::MapOnly: return uiTextForLocale("Not staged here: the player display is set to map only.", "Hier nicht bereitgestellt: Die Spieleranzeige zeigt nur die Karte.");
    case State::ChannelHidden: return uiTextForLocale("Not staged here: the relevant player display section is off.", "Hier nicht bereitgestellt: Der entsprechende Bereich der Spieleranzeige ist ausgeschaltet.");
    case State::NotSelected: return uiTextForLocale("Not included in the current staged selection.", "In der aktuellen Auswahl nicht bereitgestellt.");
    case State::Ambiguous: return uiTextForLocale("The source identity is ambiguous. Review the source records first.", "Die Quellidentität ist mehrdeutig. Prüfe zuerst die Quelldatensätze.");
    default: return uiTextForLocale("This selection has no matching campaign source.", "Für diese Auswahl gibt es keine passende Quelle in der Kampagne.");
    }
}

template<class Body>
void popup(const Campaign &campaign, const EntityId &id, const std::function<void(EntityId)> &review, Body body)
{
    if (campaign.presentationModeActive) return;
    ImGui::PushID(id.c_str());
    if (drawUiButton(uiTextForLocale("Preview player fields", "Spielerfelder ansehen"), UiButtonRole::Secondary))
        ImGui::OpenPopup("##SelectedPlayerFields");
    PopupUi::prepareFixed("##SelectedPlayerFields", ImVec2(660, 510));
    bool requested = false;
    if (ImGui::BeginPopup("##SelectedPlayerFields"))
    {
        const auto &style = ImGui::GetStyle();
        const char *reviewLabel = uiTextForLocale("Review staged content", "Bereitgestellte Inhalte prüfen");
        const char *closeLabel = uiTextForLocale("Close", "Schließen");
        const float width = std::max(1.f, ImGui::GetContentRegionAvail().x);
        const auto height = [&](const char *label) { return ImGui::CalcTextSize(label, nullptr, false,
            std::max(1.f, width - 2 * style.FramePadding.x)).y + 2 * style.FramePadding.y; };
        const float reviewHeight = height(reviewLabel), closeHeight = height(closeLabel);
        const float bodyHeight = std::max(1.f, ImGui::GetContentRegionAvail().y - reviewHeight - closeHeight - 2 * style.ItemSpacing.y);
        if (ImGui::BeginChild("SelectedPlayerFieldsBody", ImVec2(0, bodyHeight)))
        {
            ImGui::TextWrapped("%s", uiTextForLocale("Selected player fields", "Spielerfelder der Auswahl"));
            body();
            ImGui::Separator();
            ImGui::TextWrapped("%s", uiTextForLocale("Review opens the existing staged-content review. It does not save, select, or publish this source.",
                "Die Prüfung öffnet die vorhandenen bereitgestellten Inhalte. Sie speichert, wählt oder veröffentlicht diese Quelle nicht."));
        }
        ImGui::EndChild();
        if (drawUiButton(reviewLabel, static_cast<bool>(review) && !id.empty(), nullptr,
            UiButtonRole::Primary, ImVec2(-1, reviewHeight)))
        { requested = true; ImGui::CloseCurrentPopup(); }
        if (drawUiButton(closeLabel, UiButtonRole::Secondary, ImVec2(-1, closeHeight))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    if (requested) review(id);
}
}

void npc(const Campaign &campaign, const EntityId &id, const std::function<void(EntityId)> &review)
{
    popup(campaign, id, review, [&] {
        const auto preview = PlayerSafeSelection::npc(campaign, id);
        ImGui::TextWrapped("%s", stateText(preview.state));
        if (preview.candidate)
        {
            ImGui::Separator();
            ImGui::TextWrapped("%s", uiTextForLocale("Allowed NPC fields", "Erlaubte NSC-Felder"));
            ImGui::TextWrapped("%s: %s", uiTextForLocale("Name", "Name"), preview.candidate->name.c_str());
            ImGui::TextWrapped("%s: %s", uiTextForLocale("Role", "Rolle"), preview.candidate->role.c_str());
        }
        ImGui::TextWrapped("%s", uiTextForLocale("Description, goals, secrets, dialogue and NPC inventory remain private.",
            "Beschreibung, Ziele, Geheimnisse, Gesprächsnotizen und NSC-Inventar bleiben privat."));
    });
}

void item(const Campaign &campaign, const EntityId &id, const std::function<void(EntityId)> &review, const ItemModel *privateDraft)
{
    popup(campaign, id, review, [&] {
        if (privateDraft)
        {
            ImGui::Separator();
            ImGui::TextWrapped("%s", uiTextForLocale("Private editor candidate", "Privater Bearbeitungsentwurf"));
            ImGui::TextWrapped("%s: %s", uiTextForLocale("Name", "Name"), privateDraft->name.c_str());
            ImGui::TextWrapped("%s", uiTextForLocale("Name in the editor; it may have unsaved changes. Current campaign inventory entries are shown below.",
                "Name im Editor; er kann ungespeicherte Änderungen enthalten. Die aktuellen Inventareinträge der Kampagne stehen unten."));
        }
        const auto preview = PlayerSafeSelection::inventory(campaign, id);
        ImGui::Separator();
        ImGui::TextWrapped("%s", uiTextForLocale("Inventory entries selected for player review", "Für die Spielerprüfung ausgewählte Inventareinträge"));
        ImGui::TextWrapped("%s", uiTextForLocale("All eligible inventory uses of this item are listed here, not only the selected inventory row.",
            "Hier stehen alle geeigneten Inventarverwendungen dieses Gegenstands, nicht nur die ausgewählte Inventarzeile."));
        ImGui::TextWrapped("%s", stateText(preview.state));
        for (const auto &row : preview.rows)
            ImGui::TextWrapped("%s — %s × %d", row.partyOwned ? uiTextForLocale("Party", "Gruppe") : row.ownerName.c_str(), row.itemName.c_str(), row.quantity);
        ImGui::TextWrapped("%s", uiTextForLocale("Only owner, item name and quantity are shared for selected characters and the party inventory. NPC inventories, descriptions, powers and state notes are excluded.",
            "Nur Besitzer, Gegenstandsname und Anzahl werden für ausgewählte Charaktere und das Gruppeninventar geteilt. NSC-Inventare, Beschreibungen, Kräfte und Zustandsnotizen sind ausgeschlossen."));
    });
}
}
