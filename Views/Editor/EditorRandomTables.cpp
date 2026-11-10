#include "Views/Editor/EditorViewDetails.hpp"

#include "Models/Project/EntityRegistry.hpp"
#include "Models/Random/RandomTableService.hpp"

#include <algorithm>

using namespace gm::editor_view_detail;

namespace
{
const char *uiText(const char *english, const char *german)
{
    return uiTextForLocale(english, german);
}
}

void EditorWindowView::reloadRandomTableDraft(const EntityId &tableId)
{
    const Campaign &campaign = _controller.currentCampaign();
    const auto found = std::find_if(campaign.randomTables.begin(), campaign.randomTables.end(),
                                    [&](const auto &table) { return table.metadata.id == tableId; });
    if (found == campaign.randomTables.end())
    {
        _randomTableDraftId.clear();
        _randomTableEntryIndex = -1;
        _randomTableDraftDirty = false;
        return;
    }
    _randomTableDraftId = tableId;
    _randomTableDraft = *found;
    _randomTableEntryIndex = _randomTableDraft.entries.empty() ? -1 : 0;
    _randomTableDraftDirty = false;
    _randomTableStatus.clear();
}

bool EditorWindowView::saveRandomTableDraft()
{
    if (!_randomTableDraftDirty)
        return true;
    std::string error;
    if (!_controller.saveRandomTable(_randomTableDraft, error))
    {
        _randomTableStatus = error;
        return false;
    }
    _randomTableDraftDirty = false;
    _randomTableStatus = uiText("Random table saved.", "Zufallstabelle gespeichert.");
    reloadRandomTableDraft(_randomTableDraft.metadata.id);
    _randomTableStatus = uiText("Random table saved.", "Zufallstabelle gespeichert.");
    return true;
}

void EditorWindowView::renderRandomTableInspector(const EntityId &tableId)
{
    if (_randomTableDraftId != tableId)
    {
        if (_randomTableDraftDirty && !saveRandomTableDraft())
            return;
        reloadRandomTableDraft(tableId);
    }
    if (_randomTableDraftId.empty())
        return;

    RandomTableModel &table = _randomTableDraft;
    ImGui::PushID(table.metadata.id.c_str());
    ImGui::SeparatorText(uiText("Table setup", "Tabelleneinstellungen"));
    _randomTableDraftDirty |= FormUi::InputText(uiText("Name", "Name"), &table.name);
    _randomTableDraftDirty |= FormUi::InputTextMultiline(uiText("Description", "Beschreibung"), &table.description,
                                                         ImVec2(-1.0f, 70.0f), ImGuiInputTextFlags_WordWrap);
    int mode = static_cast<int>(table.selectionMode);
    const char *modes[] = {uiText("Weighted entries", "Gewichtete Einträge"),
                           uiText("Dice ranges", "Würfelbereiche")};
    if (FormUi::Combo(uiText("Selection", "Auswahl"), &mode, modes, 2))
    {
        table.selectionMode = mode == 1 ? RandomTableSelectionMode::DiceRange : RandomTableSelectionMode::Weighted;
        _randomTableDraftDirty = true;
    }
    if (table.selectionMode == RandomTableSelectionMode::DiceRange)
        _randomTableDraftDirty |= FormUi::InputText(uiText("Dice expression", "Würfelausdruck"), &table.diceExpression);

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    ImGui::SeparatorText(uiText("Results", "Ergebnisse"));
    if (drawSmallUiButton(uiText("Add result", "Ergebnis hinzufügen"), UiButtonRole::Secondary))
    {
        RandomTableEntryModel entry;
        entry.result = uiText("New result", "Neues Ergebnis");
        if (table.selectionMode == RandomTableSelectionMode::DiceRange)
        {
            int next = 1;
            for (const auto &existing : table.entries)
                next = std::max(next, existing.maximumRoll + 1);
            entry.minimumRoll = next;
            entry.maximumRoll = next;
        }
        table.entries.push_back(std::move(entry));
        _randomTableEntryIndex = static_cast<int>(table.entries.size()) - 1;
        _randomTableDraftDirty = true;
    }

    if (table.entries.empty())
    {
        WorkspaceUi::emptyState(uiText("No results yet", "Noch keine Ergebnisse"),
                                uiText("Add at least one result before saving or rolling this table.",
                                       "Füge vor dem Speichern oder Würfeln mindestens ein Ergebnis hinzu."));
    }
    else
    {
        _randomTableEntryIndex = std::clamp(_randomTableEntryIndex, 0, static_cast<int>(table.entries.size()) - 1);
        if (ImGui::BeginTable("RandomTableEntryEditor", 2,
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn(uiText("Entries", "Einträge"), ImGuiTableColumnFlags_WidthFixed, 185.0f);
            ImGui::TableSetupColumn(uiText("Selected result", "Gewähltes Ergebnis"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            for (int index = 0; index < static_cast<int>(table.entries.size()); ++index)
            {
                const RandomTableEntryModel &entry = table.entries[static_cast<std::size_t>(index)];
                std::string label;
                if (table.selectionMode == RandomTableSelectionMode::DiceRange)
                    label = std::to_string(entry.minimumRoll) + "–" + std::to_string(entry.maximumRoll) + "  ";
                else
                    label = std::to_string(entry.weight) + "×  ";
                label += entry.result.empty() ? uiText("Untitled result", "Unbenanntes Ergebnis") : entry.result;
                label += "##entry" + std::to_string(index);
                if (ImGui::Selectable(label.c_str(), _randomTableEntryIndex == index))
                    _randomTableEntryIndex = index;
            }

            ImGui::TableSetColumnIndex(1);
            RandomTableEntryModel &entry = table.entries[static_cast<std::size_t>(_randomTableEntryIndex)];
            _randomTableDraftDirty |= FormUi::InputTextMultiline(uiText("Result", "Ergebnis"), &entry.result,
                                                                 ImVec2(-1.0f, 72.0f), ImGuiInputTextFlags_WordWrap);
            _randomTableDraftDirty |= FormUi::InputTextMultiline(uiText("GM notes", "SL-Notizen"), &entry.gmNotes,
                                                                 ImVec2(-1.0f, 58.0f), ImGuiInputTextFlags_WordWrap);
            if (table.selectionMode == RandomTableSelectionMode::Weighted)
                _randomTableDraftDirty |= FormUi::InputInt(uiText("Weight", "Gewichtung"), &entry.weight, 1, 5);
            else
            {
                _randomTableDraftDirty |= FormUi::InputInt(uiText("Minimum roll", "Mindestwurf"), &entry.minimumRoll);
                _randomTableDraftDirty |= FormUi::InputInt(uiText("Maximum roll", "Höchstwurf"), &entry.maximumRoll);
            }
            _randomTableDraftDirty |= FormUi::Checkbox(uiText("Player-safe result", "Spielersicheres Ergebnis"),
                                                        &entry.playerSafe);

            const std::vector<EntityRegistryEntry> entities = EntityRegistry::getInstance().entries();
            const char *preview = entry.linkedEntityLabel.empty() ? uiText("None", "Keine") : entry.linkedEntityLabel.c_str();
            if (FormUi::BeginCombo(uiText("Linked campaign entity", "Verknüpfter Kampagneneintrag"), preview))
            {
                if (ImGui::Selectable(uiText("None", "Keine"), entry.linkedEntityId.empty()))
                {
                    entry.linkedEntityId.clear();
                    entry.linkedEntityType.clear();
                    entry.linkedEntityLabel.clear();
                    _randomTableDraftDirty = true;
                }
                for (const EntityRegistryEntry &candidate : entities)
                {
                    if (candidate.type == "random_table" || candidate.type == "random_table_entry" ||
                        candidate.id == table.metadata.id)
                        continue;
                    const std::string label = candidate.displayName + " (" + candidate.type + ")##" + candidate.id;
                    if (ImGui::Selectable(label.c_str(), entry.linkedEntityId == candidate.id))
                    {
                        entry.linkedEntityId = candidate.id;
                        entry.linkedEntityType = candidate.type;
                        entry.linkedEntityLabel = candidate.displayName;
                        _randomTableDraftDirty = true;
                    }
                }
                ImGui::EndCombo();
            }

            const int selectedIndex = _randomTableEntryIndex;
            ImGui::BeginDisabled(selectedIndex <= 0);
            if (drawSmallUiButton(uiText("Move up", "Nach oben"), UiButtonRole::Secondary))
            {
                std::swap(table.entries[static_cast<std::size_t>(selectedIndex)],
                          table.entries[static_cast<std::size_t>(selectedIndex - 1)]);
                --_randomTableEntryIndex;
                _randomTableDraftDirty = true;
            }
            ImGui::EndDisabled();
            TaskUi::sameLineIfFits();
            ImGui::BeginDisabled(selectedIndex + 1 >= static_cast<int>(table.entries.size()));
            if (drawSmallUiButton(uiText("Move down", "Nach unten"), UiButtonRole::Secondary))
            {
                std::swap(table.entries[static_cast<std::size_t>(selectedIndex)],
                          table.entries[static_cast<std::size_t>(selectedIndex + 1)]);
                ++_randomTableEntryIndex;
                _randomTableDraftDirty = true;
            }
            ImGui::EndDisabled();
            TaskUi::sameLineIfFits();
            if (drawSmallUiButton(uiText("Remove result", "Ergebnis entfernen"), UiButtonRole::Destructive))
            {
                table.entries.erase(table.entries.begin() + selectedIndex);
                _randomTableEntryIndex = table.entries.empty() ? -1 : std::min(selectedIndex, static_cast<int>(table.entries.size()) - 1);
                _randomTableDraftDirty = true;
            }
            ImGui::EndTable();
        }
    }

    const std::vector<std::string> validationErrors = RandomTables::validate(table);
    if (!validationErrors.empty())
        ImGui::TextWrapped(localizedUiLabel("%s: %s").c_str(), uiText("Cannot save", "Speichern nicht möglich"),
                           validationErrors.front().c_str());
    ImGui::BeginDisabled(!_randomTableDraftDirty || !validationErrors.empty());
    if (drawUiButton(uiText("Save table", "Tabelle speichern"), UiButtonRole::Primary))
        saveRandomTableDraft();
    ImGui::EndDisabled();
    TaskUi::sameLineIfFits();
    ImGui::BeginDisabled(_randomTableDraftDirty || !validationErrors.empty());
    if (drawUiButton(uiText("Roll", "Würfeln"), UiButtonRole::Secondary))
    {
        RandomTableRollResult roll = _controller.rollRandomTable(table.metadata.id);
        if (roll.rolled)
        {
            _randomTableRollHistory.insert(_randomTableRollHistory.begin(), roll);
            if (_randomTableRollHistory.size() > 10)
                _randomTableRollHistory.resize(10);
            _randomTableStatus = RandomTables::formatRoll(roll);
        }
        else
            _randomTableStatus = roll.error;
    }
    ImGui::EndDisabled();
    TaskUi::sameLineIfFits();
    if (drawUiButton(uiText("Discard changes", "Änderungen verwerfen"), UiButtonRole::Advanced))
        reloadRandomTableDraft(table.metadata.id);

    if (!_randomTableStatus.empty())
    {
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _randomTableStatus.c_str());
    }
    if (!_randomTableRollHistory.empty())
    {
        const RandomTableRollResult &last = _randomTableRollHistory.front();
        ImGui::SeparatorText(uiText("Latest roll", "Letzter Wurf"));
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), RandomTables::formatRoll(last).c_str());
        if (!last.gmNotes.empty())
            drawInlineLabelValue(uiText("GM notes", "SL-Notizen"), last.gmNotes.c_str());
        if (drawSmallUiButton(uiText("Add to session notes", "Zu Sitzungsnotizen hinzufügen"), UiButtonRole::Secondary))
            _controller.addRandomTableRollToSessionNotes(last, _randomTableStatus);
        TaskUi::sameLineIfFits();
        ImGui::BeginDisabled(!last.playerSafe);
        if (drawSmallUiButton(uiText("Create player handout", "Spieler-Handout erstellen"), UiButtonRole::Secondary))
            _controller.createRandomTableRollHandout(last, _randomTableStatus);
        ImGui::EndDisabled();
        if (!last.playerSafe)
            ImGui::SetItemTooltip("%s", uiText("Mark this result as player-safe before creating a handout.",
                                               "Markiere dieses Ergebnis als spielersicher, bevor du ein Handout erstellst."));
        if (_randomTableRollHistory.size() > 1 &&
            ImGui::CollapsingHeader(uiText("Recent rolls", "Letzte Würfe")))
        {
            for (std::size_t index = 1; index < _randomTableRollHistory.size(); ++index)
                ImGui::BulletText("%s", RandomTables::formatRoll(_randomTableRollHistory[index]).c_str());
        }
    }
    ImGui::PopID();
}
