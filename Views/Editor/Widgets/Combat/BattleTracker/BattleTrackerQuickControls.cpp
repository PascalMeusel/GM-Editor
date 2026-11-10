#include "BattleTrackerViewGeometrySupport.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include "Views/Editor/Widgets/UiPopupPlacement.hpp"

namespace
{
// Source getters and correction commands use the existing selection seam. Keep
// inspecting another combatant independent of the current action's selection.
struct ScopedCombatantSelection
{
    int &selection;
    int previous;
    ScopedCombatantSelection(int &value, int selected) : selection(value), previous(value) { selection = selected; }
    ~ScopedCombatantSelection() { selection = previous; }
};

bool drawCorrectionHistory(const std::function<bool(bool)> &perform,const std::function<std::string()> &error)
{
    auto &history = ProjectCommandManager::getInstance();
    ImGui::SeparatorText(uiTextForLocale("Project history", "Projektverlauf"));
    bool changed = false;
    if (ImGui::BeginTable("CombatCorrectionHistory", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextColumn();
        if (drawUiButton(uiTextForLocale("Undo", "Rückgängig"), history.canUndo(),
                         uiTextForLocale("No edit to undo.", "Keine Änderung zum Rückgängigmachen."), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            changed = perform?perform(false):history.undo();
        ImGui::TextWrapped("%s", history.undoLabel().empty() ? "—" : localizedUiText(history.undoLabel().c_str()));
        ImGui::TableNextColumn();
        if (drawUiButton(uiTextForLocale("Redo", "Wiederholen"), history.canRedo(),
                         uiTextForLocale("No edit to redo.", "Keine Änderung zum Wiederholen."), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            changed = (perform?perform(true):history.redo()) || changed;
        ImGui::TextWrapped("%s", history.redoLabel().empty() ? "—" : localizedUiText(history.redoLabel().c_str()));
        ImGui::EndTable();
    }
    if (!history.lastError().empty()) ImGui::TextWrapped("%s", history.lastError().c_str());
    if(error){const auto message=error();if(!message.empty())ImGui::TextWrapped("%s",message.c_str());}
    return changed;
}
}

void BattleTrackerView::drawPersistentCombatNavigation()
{
    const auto &encounter = _controller._draftEncounter;
    if (_controller._combatStep != 1 || encounter.participants.empty()) return;
    // Copy before any button can save, normalize, or replace participant storage.
    const auto active = encounter.participants[encounter.activeParticipantIndex];
    const auto actionIndex = std::clamp(_controller._selectedActionIndex, 0, std::max(0, static_cast<int>(active.actions.size()) - 1));
    const std::string action = active.actions.empty() ? _controller._manualActionName : active.actions[actionIndex].name;
    ImGui::TextWrapped(uiTextForLocale("%s · Round %d · HP %d/%d · Temporary HP %d",
                                      "%s · Runde %d · TP %d/%d · Temporäre TP %d"),
                       active.displayName.c_str(), encounter.round, active.currentHitPoints, active.maxHitPoints, active.temporaryHitPoints);
    ImGui::TextWrapped(uiTextForLocale("Current action: %s", "Aktuelle Aktion: %s"), action.empty() ? "—" : action.c_str());
    {
        ScopedCombatantSelection selection(_controller._selectedParticipantIndex, encounter.activeParticipantIndex);
        if (const auto *resources = _controller.getSelectedSourceResources(); resources && !resources->empty())
        {
            std::string summary;
            for (std::size_t index = 0; index < std::min<std::size_t>(resources->size(), 3); ++index)
            {
                const auto &resource = (*resources)[index];
                if (!summary.empty()) summary += " · ";
                summary += resource.name + " " + std::to_string(std::max(0, resource.maximum - resource.used)) + "/" + std::to_string(resource.maximum);
            }
            if (resources->size() > 3) summary += " …";
            ImGui::TextWrapped(uiTextForLocale("Resources remaining: %s", "Verbleibende Ressourcen: %s"), summary.c_str());
        }
    }
    if (encounter.combatPaused) ImGui::TextDisabled("%s", uiTextForLocale("Combat paused", "Kampf pausiert"));
    bool openCorrections=false;
    if (ImGui::BeginTable("CombatPersistentNavigation", battleActionColumns(4,
                         {"Action", "Effects", uiTextForLocale("HP / Resources / Undo", "TP / Ressourcen / Verlauf"), "Next Turn"}),
                         ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(encounter.combatPaused);
        if (drawUiButton("Action", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
            changeCombatPhaseAndFocus(CombatPhase::ActionResolution);
        ImGui::EndDisabled();
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(encounter.combatPaused);
        if (drawUiButton("Effects", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            changeCombatPhaseAndFocus(CombatPhase::ReactionsEffects);
        ImGui::EndDisabled();
        ImGui::TableNextColumn();
        if (drawUiButton(uiTextForLocale("HP / Resources / Undo", "TP / Ressourcen / Verlauf"), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
        {
            _quickCorrectionEncounterId = encounter.metadata.id;
            _quickCorrectionParticipantId = active.metadata.id;
            openCorrections=true;
        }
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(encounter.combatPaused);
        if (drawUiButton("Next Turn", encounter.combatPhase == CombatPhase::EndTurn ? UiButtonRole::Primary : UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            _controller.nextTurn();
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    // Open and Begin must share the parent ID scope, after the table ends.
    if(openCorrections)ImGui::OpenPopup("CombatQuickCorrections");
    drawQuickCorrections();
    const auto &history = ProjectCommandManager::getInstance();
    if (!history.lastError().empty()) ImGui::TextWrapped("%s", history.lastError().c_str());
    ImGui::Separator();
}

void BattleTrackerView::drawQuickCorrections()
{
    const float scale = std::max(1.0f, ImGui::GetFontSize() / 16.0f);
    PopupUi::prepareFixed("CombatQuickCorrections", ImVec2(420.0f * scale, 620.0f * scale));
    if (!ImGui::BeginPopup("CombatQuickCorrections")) return;
    if (_quickCorrectionEncounterId != _controller._draftEncounter.metadata.id || _controller._draftEncounter.participants.empty())
    { ImGui::CloseCurrentPopup(); ImGui::EndPopup(); return; }
    const char *closeLabel = uiTextForLocale("Close", "Schließen");
    const auto &style = ImGui::GetStyle();
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float footerHeight = ImGui::CalcTextSize(closeLabel, nullptr, false,
        std::max(1.0f, width - style.FramePadding.x * 2)).y + style.FramePadding.y * 2;
    const float bodyHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y - footerHeight - style.ItemSpacing.y);
    const auto drawBody = [&] {
        ImGui::SeparatorText(uiTextForLocale("Combat corrections", "Kampfwerte korrigieren"));
        // History may replace the campaign and draft. Return before acquiring
        // participant references; the caller only closes the child and draws Close.
        if (drawCorrectionHistory(_controller._historyHandler,_controller._historyError)) return;
        auto &encounter = _controller._draftEncounter;
        const auto found = std::find_if(encounter.participants.begin(), encounter.participants.end(), [&](const auto &participant) {
            return participant.metadata.id == _quickCorrectionParticipantId;
        });
        if (found == encounter.participants.end())
        { ImGui::TextWrapped("%s", uiTextForLocale("This combatant is no longer available.", "Dieser Beteiligte ist nicht mehr verfügbar.")); return; }
        ScopedCombatantSelection selection(_controller._selectedParticipantIndex, static_cast<int>(found - encounter.participants.begin()));
        std::vector<const char *> names;
        for (const auto &participant : encounter.participants) names.push_back(participant.displayName.c_str());
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::Combo(uiTextForLocale("Combatant", "Beteiligter"), &_controller._selectedParticipantIndex, names.data(), static_cast<int>(names.size())))
            _quickCorrectionParticipantId = encounter.participants[_controller._selectedParticipantIndex].metadata.id;
        const auto participant = encounter.participants[_controller._selectedParticipantIndex];
        ImGui::TextWrapped(uiTextForLocale("HP %d/%d · Temporary HP %d", "TP %d/%d · Temporäre TP %d"),
                           participant.currentHitPoints, participant.maxHitPoints, participant.temporaryHitPoints);
        const bool blocked = _controller._encounterSaveFailed || _controller._deferCombatSave ||
                             _controller._pendingTableBreakResume.has_value() || _controller._dragParticipantIndex >= 0;
        if (blocked) ImGui::TextWrapped("%s", uiTextForLocale("Finish moving or retry the pending save before correcting values.",
                                                           "Bewegung abschließen oder ausstehende Speicherung wiederholen, bevor Werte korrigiert werden."));
        ImGui::BeginDisabled(blocked);
        FormUi::InputInt(uiTextForLocale("Amount", "Betrag"), &_controller._pendingHitPointDelta, 0, 0);
        _controller._pendingHitPointDelta = std::clamp(_controller._pendingHitPointDelta, 0, 1000000);
        FormUi::InputText(uiTextForLocale("Damage type", "Schadensart"), &_controller._damageTypeDraft);
        int delta = 0;
        ImGui::BeginDisabled(_controller._pendingHitPointDelta == 0);
        if (ImGui::BeginTable("QuickHealthActions", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextColumn();
            if (drawUiButton(uiTextForLocale("Apply damage", "Schaden anwenden"), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f))) delta = -_controller._pendingHitPointDelta;
            ImGui::TableNextColumn();
            if (drawUiButton(uiTextForLocale("Heal", "Heilen"), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f))) delta = _controller._pendingHitPointDelta;
            ImGui::EndTable();
        }
        ImGui::EndDisabled();
        int temporaryHp = participant.temporaryHitPoints;
        const bool temporaryChanged = FormUi::InputInt(uiTextForLocale("Temporary HP", "Temporäre TP"), &temporaryHp, 0, 0);
        // Copy resources: the controller can replace their owning campaign on save.
        const auto *sourceResources = _controller.getSelectedSourceResources();
        const auto resources = sourceResources ? *sourceResources : std::vector<ResourceTrackerModel>{};
        int changedResource = -1, used = 0;
        ImGui::SeparatorText(uiTextForLocale("Resources", "Ressourcen"));
        if (resources.empty()) ImGui::TextDisabled("%s", uiTextForLocale("No linked resources.", "Keine verknüpften Ressourcen."));
        for (int index = 0; index < static_cast<int>(resources.size()); ++index)
        {
            const auto &resource = resources[index];
            ImGui::PushID(resource.metadata.id.c_str());
            ImGui::TextWrapped("%s · %d/%d", resource.name.c_str(), std::max(0, resource.maximum - resource.used), resource.maximum);
            int value = resource.used;
            if (FormUi::InputInt(uiTextForLocale("Used", "Verbraucht"), &value, 0, 0)) { changedResource = index; used = value; }
            ImGui::PopID();
        }
        ImGui::EndDisabled();
        // Use exactly the existing transactional correction paths and their guards.
        if (delta != 0) _controller.applyHitPointDeltaToSelected(delta);
        else if (temporaryChanged) _controller.setSelectedTemporaryHitPoints(std::clamp(temporaryHp, 0, 1000000));
        else if (changedResource >= 0) _controller.setSelectedSourceResourceUsed(changedResource, used);
        if (!_controller._combatErrorMessage.empty()) ImGui::TextWrapped("%s", _controller._combatErrorMessage.c_str());
    };
    if (ImGui::BeginChild("CombatCorrectionsBody", ImVec2(0, bodyHeight), ImGuiChildFlags_NavFlattened)) drawBody();
    ImGui::EndChild();
    if (drawUiButton(closeLabel, UiButtonRole::Secondary, ImVec2(-1, footerHeight))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
