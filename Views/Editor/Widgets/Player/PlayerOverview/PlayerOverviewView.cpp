#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "PlayerOverviewView.hpp"
#include "Controllers/Editor/Widgets/PlayerOverview/PlayerOverviewController.hpp"
#include "Models/Rulesets/CharacterRulesService.hpp"
#include "Views/Editor/Widgets/Inventory/InventoryDisplayHelpers.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"

#include <algorithm>
#include <numeric>
#include <string>

namespace
{
std::string conditionsLabel(const CharacterModel &character)
{
    if (character.conditions.empty())
        return "-";

    std::string label;
    for (const std::string &condition : character.conditions)
    {
        if (!label.empty())
            label += ", ";
        label += condition;
    }
    return label;
}

std::string spellSlotsLabel(CharacterSpellcastingModel spellcasting)
{
    if (!spellcasting.enabled)
        return "-";

    normalizeSpellcasting(spellcasting);
    int maximumTotal = 0;
    int usedTotal = 0;
    for (int level = 1; level <= 9; ++level)
    {
        maximumTotal += spellcasting.slotMaximums[static_cast<std::size_t>(level)];
        usedTotal += spellcasting.slotUsed[static_cast<std::size_t>(level)];
    }

    if (maximumTotal <= 0)
        return localizedUiLabel("Magic, no slots");
    return std::to_string(std::max(0, maximumTotal - usedTotal)) + "/" + std::to_string(maximumTotal) +
           " " + localizedUiLabel("spell slots left");
}

bool drawRowActionButton(
    const char *label,
    bool enabled,
    const char *disabledReason,
    UiButtonRole role,
    const char *tooltip)
{
    if (!enabled)
    {
        UiButtonStyleScope style(role);
        ImGui::BeginDisabled(true);
        const std::string localizedLabel = localizedUiLabel(label);
        ImGui::SmallButton(localizedLabel.c_str());
        ImGui::EndDisabled();
        showHelpTooltip(localizedUiText(disabledReason), ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled);
        return false;
    }

    return drawSmallUiButton(label, role, tooltip);
}

}

PlayerOverviewView::PlayerOverviewView(PlayerOverviewController &controller)
    : DockableWidget("Player Overview", 0, ImVec2(1120.0f, 640.0f)), _controller(controller)
{
}

void PlayerOverviewView::draw()
{
    Campaign &campaign = _controller._currentCampaign;
    if (campaign.characters.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No players exist in this campaign yet.").c_str());
        return;
    }

    const std::vector<ItemModel> items = _controller.getAvailableItems();
    int currentHpTotal = 0;
    int maxHpTotal = 0;
    int woundedCount = 0;
    int spellcasterCount = 0;
    double partyWeight = 0.0;
    for (CharacterModel character : campaign.characters)
    {
        normalizeCharacterHitPoints(character);
        currentHpTotal += character.currentHitPoints;
        maxHpTotal += character.maxHitPoints;
        woundedCount += character.currentHitPoints < character.maxHitPoints ? 1 : 0;
        spellcasterCount += character.spellcasting.enabled ? 1 : 0;
        partyWeight += calculateInventoryWeight(character.inventory, items);
    }

    const std::string overviewSelection = std::to_string(campaign.characters.size()) + " " + localizedUiLabel("players") + ", " +
                                          std::to_string(woundedCount) + " " + localizedUiLabel("wounded") + ", " +
                                          std::to_string(spellcasterCount) + " " + localizedUiLabel("spellcasters");
    UiStatusActionStrip overviewStrip;
    overviewStrip.mode = "Party Scan";
    overviewStrip.selection = overviewSelection.c_str();
    overviewStrip.nextAction = "Use row actions for sheets, inventory, party map, or live session context.";
    overviewStrip.help = "Rows open the relevant editor surface when the shell provides a handler. Character and inventory links open their owning surfaces; map links select the party map when a party position exists.";
    drawStatusActionStrip("##PlayerOverviewStatus", overviewStrip);

    drawHelpText("Party Dashboard", "A compact table view for live-session scanning: health, defense, passive awareness, conditions, spell slots, inventory weight, and party map position.");
    if (ImGui::BeginTable("PlayerOverviewMetrics", 5, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();
        ImGui::Text(localizedUiLabel("Players").c_str());
        ImGui::Text(localizedUiLabel("%d").c_str(), static_cast<int>(campaign.characters.size()));
        ImGui::TableNextColumn();
        ImGui::Text(localizedUiLabel("Party Health").c_str());
        ImGui::Text(localizedUiLabel("%d / %d").c_str(), currentHpTotal, maxHpTotal);
        ImGui::TableNextColumn();
        ImGui::Text(localizedUiLabel("Wounded").c_str());
        ImGui::Text(localizedUiLabel("%d").c_str(), woundedCount);
        ImGui::TableNextColumn();
        ImGui::Text(localizedUiLabel("Spellcasters").c_str());
        ImGui::Text(localizedUiLabel("%d").c_str(), spellcasterCount);
        ImGui::TableNextColumn();
        ImGui::Text(localizedUiLabel("Carried Weight").c_str());
        ImGui::Text(localizedUiLabel("%.1f lb").c_str(), partyWeight);
        ImGui::EndTable();
    }
    ImGui::TextDisabled(localizedUiLabel("Party Map: %s").c_str(), _controller.getPartyLocationLabel().c_str());

    ImGui::SeparatorText(localizedUiLabel("Party Resources").c_str());
    InventoryModel &partyInventory = campaign.partyInventory;
    if (partyInventory.ownerEntityId.empty())
        partyInventory.ownerEntityId = campaign.metadata.id;
    if (partyInventory.name.empty())
        partyInventory.name = "Party Inventory";
    if (ImGui::BeginTable("PartyResourceSummary", 4, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();
        int copper = partyInventory.copper;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputInt(localizedUiLabel("Copper").c_str(), &copper))
            _controller.setPartyCoin("Copper", copper);
        ImGui::TableNextColumn();
        int silver = partyInventory.silver;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputInt(localizedUiLabel("Silver").c_str(), &silver))
            _controller.setPartyCoin("Silver", silver);
        ImGui::TableNextColumn();
        int gold = partyInventory.gold;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputInt(localizedUiLabel("Gold").c_str(), &gold))
            _controller.setPartyCoin("Gold", gold);
        ImGui::TableNextColumn();
        int platinum = partyInventory.platinum;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputInt(localizedUiLabel("Platinum").c_str(), &platinum))
            _controller.setPartyCoin("Platinum", platinum);
        ImGui::EndTable();
    }
    int renown = campaign.partyRenown;
    ImGui::SetNextItemWidth(120.0f);
    if (FormUi::InputInt(localizedUiLabel("Renown").c_str(), &renown))
        _controller.setPartyRenown(renown);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(360.0f);
    std::string reputation = campaign.partyReputation;
    if (FormUi::InputText(localizedUiLabel("Reputation").c_str(), &reputation))
        _controller.setPartyReputation(reputation);

    auto renderResourceList = [](const char *label, std::vector<std::string> &entries, std::string &draft, auto addCallback, auto removeCallback) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(localizedUiText(label));
        int pendingRemoval = -1;
        for (int i = 0; i < static_cast<int>(entries.size()); ++i)
        {
            ImGui::BulletText("%s", entries[static_cast<std::size_t>(i)].c_str());
            ImGui::SameLine();
            ImGui::PushID(i);
            if (ImGui::SmallButton(localizedUiText("Remove")))
                pendingRemoval = i;
            ImGui::PopID();
        }
        ImGui::SetNextItemWidth(220.0f);
        FormUi::InputText(localizedUiLabel("##NewResource").c_str(), &draft);
        ImGui::SameLine();
        if (ImGui::Button(localizedUiLabel("Add").c_str()))
        {
            addCallback(draft);
            draft.clear();
        }
        if (pendingRemoval >= 0)
            removeCallback(pendingRemoval);
        ImGui::PopID();
    };
    if (ImGui::BeginTable("PartyResourceLists", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        renderResourceList("Vehicles", campaign.partyVehicles, _controller._newPartyVehicle,
                           [&](const std::string &value) { _controller.addPartyVehicle(value); },
                           [&](int index) { _controller.removePartyVehicle(index); });
        ImGui::TableSetColumnIndex(1);
        renderResourceList("Strongholds", campaign.partyStrongholds, _controller._newPartyStronghold,
                           [&](const std::string &value) { _controller.addPartyStronghold(value); },
                           [&](int index) { _controller.removePartyStronghold(index); });
        ImGui::EndTable();
    }

    ImGui::Text(localizedUiLabel("Group Inventory Weight: %.1f lb").c_str(), calculateInventoryWeight(partyInventory, items));
    if (!partyInventory.entries.empty() && ImGui::BeginTable("PartyInventory", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Item").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Qty").c_str(), ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Weight").c_str(), ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();
        int pendingRemoval = -1;
        for (int i = 0; i < static_cast<int>(partyInventory.entries.size()); ++i)
        {
            InventoryEntryModel &entry = partyInventory.entries[static_cast<std::size_t>(i)];
            const ItemModel *item = findItemById(items, entry.itemId);
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(item ? item->name : localizedUiLabel("<Missing Item>"));
            ImGui::TableSetColumnIndex(1);
            int quantity = entry.quantity;
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputInt(localizedUiLabel("##Qty").c_str(), &quantity))
                _controller.setPartyInventoryEntryQuantity(i, quantity);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text(localizedUiLabel("%.1f").c_str(), item ? item->weight * static_cast<double>(entry.quantity) : 0.0);
            ImGui::TableSetColumnIndex(3);
            if (ImGui::SmallButton(localizedUiText("Remove")))
                pendingRemoval = i;
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (pendingRemoval >= 0)
            _controller.removePartyInventoryEntry(pendingRemoval);
    }
    if (!items.empty())
    {
        if (_controller._selectedPartyItemIndex >= static_cast<int>(items.size()))
            _controller._selectedPartyItemIndex = 0;
        ImGui::SetNextItemWidth(260.0f);
        if (FormUi::BeginCombo(localizedUiLabel("Add Group Item").c_str(), items[static_cast<std::size_t>(_controller._selectedPartyItemIndex)].name.c_str()))
        {
            for (int i = 0; i < static_cast<int>(items.size()); ++i)
            {
                const bool selected = _controller._selectedPartyItemIndex == i;
                if (ImGui::Selectable(items[static_cast<std::size_t>(i)].name.c_str(), selected))
                    _controller._selectedPartyItemIndex = i;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        FormUi::InputInt(localizedUiLabel("Qty").c_str(), &_controller._partyItemQuantity);
        ImGui::SameLine();
        if (ImGui::Button(localizedUiLabel("Add to Group").c_str()))
            _controller.addPartyInventoryItem(items[static_cast<std::size_t>(_controller._selectedPartyItemIndex)].metadata.id, _controller._partyItemQuantity);
    }

    ImGui::SeparatorText(localizedUiLabel("Filters").c_str());
    ImGui::SetNextItemWidth(260.0f);
    FormUi::InputText(localizedUiLabel("Search").c_str(), &_controller._searchText);
    ImGui::SameLine();
    const std::vector<std::string> classFilters = _controller.getClassFilters();
    if (_controller._classFilterIndex >= static_cast<int>(classFilters.size()))
        _controller._classFilterIndex = 0;
    ImGui::SetNextItemWidth(180.0f);
    const auto localizedClassFilter = [](const std::string &value) {
        return value == "All Classes" || value == "Unclassed" ? localizedUiLabel(value.c_str()) : value;
    };
    const std::string currentClassFilter = localizedClassFilter(classFilters[static_cast<std::size_t>(_controller._classFilterIndex)]);
    if (FormUi::BeginCombo(localizedUiLabel("Class").c_str(), currentClassFilter.c_str()))
    {
        for (int i = 0; i < static_cast<int>(classFilters.size()); ++i)
        {
            const bool selected = _controller._classFilterIndex == i;
            const std::string classFilter = localizedClassFilter(classFilters[static_cast<std::size_t>(i)]);
            if (ImGui::Selectable(classFilter.c_str(), selected))
                _controller._classFilterIndex = i;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    FormUi::Checkbox(localizedUiLabel("Wounded only").c_str(), &_controller._showWoundedOnly);
    ImGui::SameLine();
    FormUi::Checkbox(localizedUiLabel("Spellcasters only").c_str(), &_controller._showSpellcastersOnly);

    const std::vector<int> visibleIndexes = _controller.getVisibleCharacterIndexes();
    ImGui::SeparatorText(localizedUiLabel("Party Members").c_str());
    if (visibleIndexes.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No players match the current filters.").c_str());
        return;
    }

    if (ImGui::BeginTable("PlayerOverviewTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, 390.0f)))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Player").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.22f);
        ImGui::TableSetupColumn(localizedUiLabel("Status").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.20f);
        ImGui::TableSetupColumn(localizedUiLabel("Defense").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.18f);
        ImGui::TableSetupColumn(localizedUiLabel("Resources").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.20f);
        ImGui::TableSetupColumn(localizedUiLabel("Actions").c_str(), ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableHeadersRow();

        const bool canOpenSheets = static_cast<bool>(_controller._openCharacterSheetsHandler);
        const bool canOpenInventory = static_cast<bool>(_controller._openInventoryHandler);
        const bool canOpenLiveSession = static_cast<bool>(_controller._openLiveSessionHandler);
        const bool canOpenMap = static_cast<bool>(_controller._openMapHandler) && _controller.hasPartyMapTarget();
        const std::string mapDisabledReason = _controller.partyMapDisabledReason();

        for (const int characterIndex : visibleIndexes)
        {
            CharacterModel character = campaign.characters[static_cast<std::size_t>(characterIndex)];
            normalizeCharacterHitPoints(character);
            const RuleArmorSummary armor = campaign.ruleSet ? CharacterRules::evaluateArmor(*campaign.ruleSet, character, items) : RuleArmorSummary{};
            const int passivePerception = campaign.ruleSet ? campaign.ruleSet->getPassiveScore(character.attributes, "Perception", character.level) : 10;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(character.nameBuffer);
            const std::string className = character.selectedClass.empty() ? localizedUiLabel("Unclassed") : localizedUiLabel(character.selectedClass.c_str());
            ImGui::TextDisabled(localizedUiLabel("Level %d %s").c_str(), character.level, className.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(localizedUiLabel("%d/%d HP").c_str(), character.currentHitPoints, character.maxHitPoints);
            tableTextWrapped(conditionsLabel(character));
            ImGui::TableSetColumnIndex(2);
            const std::string defenseSummary =
                localizedUiLabel("AC") + " " + std::to_string(armor.armorClass) +
                " / " + localizedUiLabel("Passive") + " " + std::to_string(passivePerception) +
                " / " + std::to_string(static_cast<int>(calculateInventoryWeight(character.inventory, items))) + " " + localizedUiLabel("lb");
            tableTextWrapped(defenseSummary);
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(spellSlotsLabel(character.spellcasting));
            ImGui::TableSetColumnIndex(4);
            ImGui::PushID(characterIndex);
            if (drawRowActionButton("Sheet##OpenCharacterSheet",
                                    canOpenSheets,
                                    "Character details are unavailable in the current workspace.",
                                    UiButtonRole::Primary,
                                    "Open Character Sheets with this player selected when the destination can resolve the character."))
                _controller.openCharacterSheet(characterIndex);
            ImGui::SameLine();
            if (drawRowActionButton("Items##OpenInventory",
                                    canOpenInventory,
                                    "Character inventory is unavailable in the current workspace.",
                                    UiButtonRole::Secondary,
                                    "Open Inventory & Equipment with this player selected as the owner when possible."))
                _controller.openInventory(characterIndex);
            ImGui::SameLine();
            if (drawRowActionButton("Map##OpenPartyMap",
                                    canOpenMap,
                                    mapDisabledReason.c_str(),
                                    UiButtonRole::Secondary,
                                    "Open Map Viewer on the party's current map position context."))
                _controller.openPartyMap();
            ImGui::SameLine();
            if (drawRowActionButton("Live##OpenLiveSession",
                                    canOpenLiveSession,
                                    "Live session context is unavailable in the current workspace.",
                                    UiButtonRole::Secondary,
                                    "Open Live Dashboard for party, scene, map, notes, and game time context."))
                _controller.openLiveSessionContext();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}
