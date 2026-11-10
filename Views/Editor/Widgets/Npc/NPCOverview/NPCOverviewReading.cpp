#include "NPCOverviewViewMonsterSupport.hpp"
#include "Views/Editor/Presentation/SelectedPlayerPreview.hpp"
#include "Models/Project/ApplicationPaths.hpp"

void NPCOverviewView::resetReadingContext()
{
    _readingNpcId.clear();
    _readingCampaignId.clear();
    _readingRoot.clear();
    _reading = _secretsRevealed = false;
    _detailSection = 0;
}

void NPCOverviewView::reconcileReadingContext()
{
    const auto id = _controller.currentSelectedNpcId();
    const auto &campaign = _controller._currentCampaign;
    const auto root = ApplicationPaths::writableRoot();
    if (id.empty() || id != _readingNpcId || campaign.metadata.id != _readingCampaignId || root != _readingRoot)
    {
        resetReadingContext();
        _readingNpcId = id;
        _readingCampaignId = campaign.metadata.id;
        _readingRoot = root;
    }
}

void NPCOverviewView::drawNpcReading()
{
    auto &campaign = _controller._currentCampaign;
    const auto id = _controller.currentSelectedNpcId();
    if (campaign.presentationModeActive || id.empty() || id != _readingNpcId) { resetReadingContext(); return; }
    const auto &npc = campaign.npcs[static_cast<std::size_t>(_controller._selectedNpcIndex)];
    ImGui::TextWrapped("%s", npc.name.empty() ? uiTextForLocale("Unnamed NPC", "Unbenannter NSC") : npc.name.c_str());
    ImGui::TextWrapped("%s", uiTextForLocale("Private reference · reading does not change or share this NPC.",
        "Private Referenz · Lesen ändert oder teilt diesen NSC nicht."));
    if (drawUiButton(uiTextForLocale("Back to summary", "Zur Zusammenfassung"), UiButtonRole::Secondary))
    { _reading = _secretsRevealed = false; _detailSection = 0; }
    TaskUi::sameLineIfFits();
    if (drawUiButton(uiTextForLocale("Edit profile", "Profil bearbeiten"), UiButtonRole::Primary))
    { _reading = _secretsRevealed = false; _detailSection = 1; }
    SelectedPlayerPreview::npc(campaign, id, _controller._playerReviewHandler);
    const auto section = [](const char *label, const std::string &value) {
        if (value.empty()) return;
        ImGui::SeparatorText(label);
        ImGui::TextWrapped("%s", value.c_str());
    };
    section(uiTextForLocale("Role", "Rolle"), npc.role);
    section(uiTextForLocale("Description", "Beschreibung"), npc.description);
    section(uiTextForLocale("Attitude to the party", "Haltung zur Gruppe"), npc.attitudeTowardParty);
    section(uiTextForLocale("Dialogue notes", "Gesprächsnotizen"), npc.dialogueNotes);
    section(uiTextForLocale("Location", "Ort"), npc.threatLocation);
    if (!npc.factionId.empty() || !npc.faction.empty()) section(localizedUiText("Faction"), factionDisplayName(campaign, npc));
    section(uiTextForLocale("Goals", "Ziele"), npc.goals);
    section(uiTextForLocale("Resources", "Ressourcen"), npc.resources);
    section(uiTextForLocale("Relationships", "Beziehungen"), npc.relationships);
    if (!npc.conditions.empty()) section(localizedUiText("Conditions"), npcConditionsLabel(npc));
    if (npc.threatLevel > 0) ImGui::Text("%s: %d / 10", localizedUiText("Threat"), npc.threatLevel);
    if (!npc.secrets.empty())
    {
        ImGui::SeparatorText(uiTextForLocale("Private secrets", "Private Geheimnisse"));
        if (drawUiButton(_secretsRevealed ? uiTextForLocale("Hide secrets", "Geheimnisse verbergen") :
            uiTextForLocale("Reveal secrets", "Geheimnisse anzeigen"), UiButtonRole::Secondary))
            _secretsRevealed = !_secretsRevealed;
        if (_secretsRevealed) ImGui::TextWrapped("%s", npc.secrets.c_str());
    }
}
