    if (showOwnerSummary)
    {
    ImGui::Separator();
    ImGui::SeparatorText(localizedUiLabel("Currency").c_str());
    const int totalCurrencyValue = inventoryCurrencyTotalValue(*inventory, ruleset);
    const std::string totalCurrency = formatCurrencyTotalValue(ruleset, totalCurrencyValue);
    ImGui::Text(localizedUiLabel("Total Value: %s").c_str(), totalCurrency.c_str());
    showHelpTooltip("Total value is the sum of every denomination using the active ruleset's base currency values.");
    if (ImGui::TreeNode(uiTextForLocale("Edit coins", "Münzen bearbeiten")))
    {
    if (ImGui::BeginTable("InventoryCurrency", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Denomination").c_str(), ImGuiTableColumnFlags_WidthFixed, 116.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Count").c_str(), ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Unit").c_str(), ImGuiTableColumnFlags_WidthFixed, 82.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Subtotal").c_str(), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        const auto drawCurrencyRow = [&](const char *label, const char *code, int currentAmount) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text(localizedUiLabel("%s (%s)").c_str(), label, code);
            ImGui::TableSetColumnIndex(1);
            int amount = currentAmount;
            ImGui::SetNextItemWidth(-1.0f);
            const std::string inputId = std::string("##InventoryCurrency") + code;
            if (FormUi::InputInt(inputId.c_str(), &amount))
                _controller.setSelectedInventoryCoin(code, amount);
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(formatCurrencyUnitValue(currencyBaseUnitValue(ruleset, code)));
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(formatCurrencySubtotal(ruleset, code, currentAmount));
        };

        drawCurrencyRow("Copper", "cp", inventory->copper);
        drawCurrencyRow("Silver", "sp", inventory->silver);
        drawCurrencyRow("Gold", "gp", inventory->gold);
        drawCurrencyRow("Platinum", "pp", inventory->platinum);
        ImGui::EndTable();
    }
    if (ruleset)
    {
        InventoryModel previewInventory = *inventory;
        const int beforeValue = ruleset->getInventoryCurrencyValue(*inventory);
        ruleset->normalizeInventoryCurrency(previewInventory);
        const int afterValue = ruleset->getInventoryCurrencyValue(previewInventory);
        const bool currencyWouldChange = !sameCoinBreakdown(*inventory, previewInventory);
        const std::string beforeCoins = formatCoinBreakdown(*inventory);
        const std::string afterCoins = formatCoinBreakdown(previewInventory);
        const std::string sameTotal = formatCurrencyTotalValue(ruleset, beforeValue);
        ImGui::TextWrapped(localizedUiLabel("Make Change preview: %s -> %s. Total stays the same: %s.").c_str(),
                           beforeCoins.c_str(),
                           afterCoins.c_str(),
                           sameTotal.c_str());
        if (afterValue != beforeValue)
            ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.38f, 1.0f), localizedUiLabel("Currency rules changed the total unexpectedly; conversion is blocked.").c_str());
        if (drawUiButton("Make Change",
                         currencyWouldChange && afterValue == beforeValue,
                         currencyWouldChange ? "Currency rules changed the total unexpectedly." : "This inventory is already converted to the largest available coins.",
                         UiButtonRole::Primary,
                         ImVec2(0.0f, 0.0f),
                         "Convert the same total value into the largest coins available in the active ruleset."))
            _controller.normalizeSelectedInventoryCurrency();
    }
    else
    {
        drawDisabledUiButton("Make Change", "Choose a ruleset before converting currency.", UiButtonRole::Primary);
    }

    ImGui::TreePop();
    }
    const double carriedWeight = calculateInventoryWeight(*inventory, items);
    ImGui::Text(localizedUiLabel("Carried Weight: %.2f lb").c_str(), carriedWeight);
    showHelpTooltip("Total weight of every assigned item, equipped or not.");
    if (_controller._currentCampaign.ruleSet)
    {
        const RuleCarryingCapacitySummary carrying = _controller._currentCampaign.ruleSet->evaluateCarryingCapacity(_controller.getSelectedOwnerAttributes(), *inventory, items);
        if (carrying.capacityKnown)
        {
            TaskUi::sameLineIfFits();
            ImGui::TextDisabled(localizedUiLabel("Capacity: %.2f lb").c_str(), carrying.capacityWeight);
            for (const std::string &warning : carrying.warnings)
                ImGui::TextColored(carrying.overCapacity ? ImVec4(0.95f, 0.42f, 0.38f, 1.0f) : ImVec4(0.92f, 0.74f, 0.42f, 1.0f), localizedUiLabel("%s").c_str(), warning.c_str());
        }
        else
        {
            TaskUi::sameLineIfFits();
            ImGui::TextDisabled(localizedUiLabel("Capacity unknown").c_str());
        }
    }
    ImGui::Text(localizedUiLabel("Equipped Armor: Armor Class +%d | Damage Reduction %d").c_str(),
                calculateEquippedArmorClassBonus(*inventory, items),
                calculateEquippedDamageReduction(*inventory, items));
    showHelpTooltip("Only equipped armor contributes these combat values.");
    if (_controller._currentCampaign.ruleSet)
    {
        const RuleEquipmentSlotValidation slotValidation = _controller._currentCampaign.ruleSet->validateEquipmentSlots(*inventory, items);
        for (const std::string &warning : slotValidation.warnings)
            ImGui::TextColored(ImVec4(0.92f, 0.74f, 0.42f, 1.0f), localizedUiLabel("%s").c_str(), warning.c_str());
    }
    }

    if (showOwnerItems)
    {
    ImGui::SeparatorText(localizedUiLabel("Owner Items").c_str());
    showHelpTooltip("Select a compact row first, then use Item Detail to review context and make changes.");
    if (inventory->entries.empty())
    {
        _controller._selectedInventoryEntryIndex = -1;
        ImGui::TextDisabled(localizedUiLabel("%s is not carrying any items.").c_str(), ownerName.c_str());
    }
    else
    {
        // Stable entry IDs remain empty after deletion until the user selects another row.

        const int ownerItemColumnCount = 3;
        if (ImGui::BeginTable("InventoryCompactEntries", ownerItemColumnCount, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Item").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.6f);
            ImGui::TableSetupColumn(localizedUiLabel("State").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn(localizedUiLabel("Qty").c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < static_cast<int>(inventory->entries.size()); ++i)
            {
                const InventoryEntryModel &entry = inventory->entries[static_cast<std::size_t>(i)];
                const ItemModel *item = findItemById(items, entry.itemId);
                const std::string transferDisabledReason = inventoryEntryTransferDisabledReason(inventory, transferInventory, entry);
                const bool canTransferEntry = transferDisabledReason.empty();
                const std::string transferItemName = inventoryEntryItemName(item);
                const std::string transferTooltip = formatLocalizedUi("Move %d %s from %s to %s. Stackable items merge when possible.",
                                                                       entry.quantity,
                                                                       transferItemName.c_str(),
                                                                       ownerName.c_str(),
                                                                       transferName.c_str());

                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const bool selectedEntry = _controller._selectedInventoryEntryIndex == i;
                if (ImGui::Selectable(transferItemName.c_str(), selectedEntry, ImGuiSelectableFlags_SpanAllColumns))
                    _controller.selectInventoryEntry(i);
                showHelpTooltip("Select this row to inspect quantity, transfer context, equipment state, and edit actions below.");
                ImGui::TableSetColumnIndex(1);
                tableTextWrapped(inventoryEntryStateSummary(entry, item));
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", entry.quantity);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    ImGui::SeparatorText(localizedUiLabel("Item Detail").c_str());
    if (_controller._selectedInventoryEntryIndex < 0 || _controller._selectedInventoryEntryIndex >= static_cast<int>(inventory->entries.size()))
    {
        ImGui::TextDisabled(localizedUiLabel("Select an owner item row to edit quantity, equipment state, container, transfer, or removal.").c_str());
    }
    else
    {
        const int selectedEntryIndex = _controller._selectedInventoryEntryIndex;
        InventoryEntryModel &entry = inventory->entries[static_cast<std::size_t>(selectedEntryIndex)];
        const ItemModel *item = findItemById(items, entry.itemId);
        SelectedPlayerPreview::item(_controller._currentCampaign, _controller.currentSelectedItemId(), _controller._playerReviewHandler);
        const double eachWeight = item ? item->weight : 0.0;
        const std::string selectedEntryName = inventoryEntryItemName(item);
        const std::string transferDisabledReason = inventoryEntryTransferDisabledReason(inventory, transferInventory, entry);
        const bool canTransferEntry = transferDisabledReason.empty();
        const std::string transferTooltip = formatLocalizedUi("Move %d %s from %s to %s. Stackable items merge when possible.",
                                                               entry.quantity,
                                                               selectedEntryName.c_str(),
                                                               ownerName.c_str(),
                                                               transferName.c_str());

        const std::string rowSelection = showTransfer
                                             ? formatLocalizedUi("Source: %s | Target: %s | Item: %s", ownerName.c_str(), transferName.c_str(), selectedEntryName.c_str())
                                             : formatLocalizedUi("Owner: %s | Item: %s | Qty: %d", ownerName.c_str(), selectedEntryName.c_str(), entry.quantity);
        const std::string rowNextAction = showTransfer
                                              ? (canTransferEntry ? formatLocalizedUi("Ready to move all to %s", transferName.c_str()) : transferDisabledReason)
                                              : std::string(localizedUiText("Edit quantity, equipment, container, state, notes, or remove this row."));
        UiStatusActionStrip rowStrip;
        rowStrip.mode = localizedUiText("Inventory row selected");
        rowStrip.selection = rowSelection.c_str();
        rowStrip.nextAction = rowNextAction.c_str();
        rowStrip.help = localizedUiText(showTransfer ? "This detail area previews source, target, quantity, and disabled reasons before a transfer." : "This detail area keeps row edits separate from item lookup and transfer setup.");
        if (showTransfer) ImGui::TextWrapped("%s", rowSelection.c_str());

        const bool detailWide = TaskUi::hasRoomForColumns();
        if (ImGui::BeginTable("InventorySelectedEntryDetail", detailWide ? 2 : 1, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Summary").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            if (detailWide) ImGui::TableSetupColumn(localizedUiLabel("Controls").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            drawInlineLabelValue("Item", selectedEntryName.c_str());
            drawInlineLabelValue("Category", inventoryEntryCategory(item).c_str());
            drawInlineLabelValue("Owner", ownerName.c_str());
            if (showTransfer)
                drawInlineLabelValue("Transfer Target", transferName.c_str());
            drawInlineLabelValue("Quantity", std::to_string(entry.quantity).c_str());
            drawInlineLabelValue("Container", inventoryEntryContainerLabel(*inventory, items, entry).c_str());
            const std::string totalWeight = formatLocalizedUi("%.2f lb total", eachWeight * static_cast<double>(entry.quantity));
            drawInlineLabelValue("Weight", totalWeight.c_str());
            drawInlineLabelValue("State", inventoryEntryStateSummary(entry, item).c_str());
            if (showTransfer && !transferDisabledReason.empty())
                ImGui::TextColored(ImVec4(0.92f, 0.74f, 0.42f, 1.0f), localizedUiLabel("%s").c_str(), transferDisabledReason.c_str());
            if (item && item->container)
            {
                const double containedWeight = calculateContainedWeight(*inventory, items, entry.metadata.id);
                ImGui::TextDisabled(localizedUiLabel("Container load: %.1f/%.1f lb").c_str(), containedWeight, item->containerCapacityWeight);
                if (item->containerCapacityWeight > 0.0 && containedWeight > item->containerCapacityWeight)
                    ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.38f, 1.0f), localizedUiLabel("Over capacity").c_str());
            }
            if (item && isWeaponItem(*item))
                ImGui::TextDisabled(localizedUiLabel("Weapon: %s | %s | Range %d ft").c_str(),
                                    item->weaponDamage.empty() ? "manual damage" : item->weaponDamage.c_str(),
                                    item->weaponAbility.c_str(),
                                    item->weaponRangeFeet);
            if (item && isArmorItem(*item))
                ImGui::TextDisabled(localizedUiLabel("Armor: AC +%d | Damage Reduction %d").c_str(), item->armorClassBonus, item->damageReduction);

            ImGui::TableNextColumn();
            int quantity = entry.quantity;
            ImGui::SetNextItemWidth(120.0f);
            if (FormUi::InputInt(localizedUiLabel("Quantity##SelectedEntry").c_str(), &quantity))
                _controller.setInventoryEntryQuantity(selectedEntryIndex, quantity);
            showHelpTooltip("Edit the carried quantity for the selected row.");

            if (drawUiButton(entry.equipped ? "Unequip" : "Equip",
                             UiButtonRole::Secondary,
                             ImVec2(110.0f, 0.0f),
                             "Toggle whether the item is actively worn or wielded. Equipped weapons appear in combat; equipped armor affects defense."))
                _controller.toggleEquipped(selectedEntryIndex);

            if (ImGui::TreeNode(uiTextForLocale("Equipment slot and container", "Ausrüstungsplatz und Behälter")))
            {
            ImGui::SetNextItemWidth(-1.0f);
            if (_controller._currentCampaign.ruleSet && FormUi::BeginCombo(localizedUiLabel("Slot##SelectedEntry").c_str(), entry.slot.empty() ? localizedUiText("None") : entry.slot.c_str()))
            {
                if (ImGui::Selectable(localizedUiLabel("None").c_str(), entry.slot.empty()))
                    _controller.setInventoryEntrySlot(selectedEntryIndex, "");
                for (const RuleEquipmentSlotDefinition &slot : _controller._currentCampaign.ruleSet->getEquipmentSlots())
                {
                    const bool selected = entry.slot == slot.label;
                    if (ImGui::Selectable(slot.label.c_str(), selected))
                        _controller.setInventoryEntrySlot(selectedEntryIndex, slot.label);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            else if (!_controller._currentCampaign.ruleSet)
            {
                std::string slot = entry.slot;
                if (FormUi::InputText(localizedUiLabel("Slot##SelectedEntryText").c_str(), &slot))
                    _controller.setInventoryEntrySlot(selectedEntryIndex, slot);
            }

            const std::string containerLabel = inventoryEntryContainerLabel(*inventory, items, entry);
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::BeginCombo(localizedUiLabel("Container##SelectedEntry").c_str(), containerLabel.c_str()))
            {
                if (ImGui::Selectable(localizedUiLabel("None").c_str(), entry.containerEntryId.empty()))
                    _controller.setInventoryEntryContainer(selectedEntryIndex, "");
                for (int containerIndex = 0; containerIndex < static_cast<int>(inventory->entries.size()); ++containerIndex)
                {
                    if (containerIndex == selectedEntryIndex)
                        continue;
                    const InventoryEntryModel &candidateEntry = inventory->entries[static_cast<std::size_t>(containerIndex)];
                    const ItemModel *candidateItem = findItemById(items, candidateEntry.itemId);
                    if (!candidateItem || !candidateItem->container)
                        continue;
                    const bool selected = entry.containerEntryId == candidateEntry.metadata.id;
                    const std::string label = displayNameOrFallback(candidateItem->name, "<Container>");
                    if (ImGui::Selectable(label.c_str(), selected))
                        _controller.setInventoryEntryContainer(selectedEntryIndex, candidateEntry.metadata.id);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TreePop();
            }
            if (item && item->requiresAttunement)
            {
                bool attuned = entry.attuned;
                if (FormUi::Checkbox(localizedUiLabel("Attuned##SelectedEntry").c_str(), &attuned))
                    _controller.setInventoryEntryAttuned(selectedEntryIndex, attuned);
            }
            if (item && item->maxCharges > 0)
            {
                int charges = entry.charges < 0 ? item->maxCharges : entry.charges;
                ImGui::SetNextItemWidth(120.0f);
                if (FormUi::InputInt(localizedUiLabel("Charges##SelectedEntry").c_str(), &charges))
                    _controller.setInventoryEntryCharges(selectedEntryIndex, std::min(charges, item->maxCharges));
            }
            if (item && item->ammunition)
            {
                int ammunitionRemaining = entry.ammunitionRemaining;
                ImGui::SetNextItemWidth(120.0f);
                if (FormUi::InputInt(localizedUiLabel("Ammo##SelectedEntry").c_str(), &ammunitionRemaining))
                    _controller.setInventoryEntryAmmunition(selectedEntryIndex, ammunitionRemaining);
            }
            if (item && item->maxDurability > 0)
            {
                int durability = entry.durability < 0 ? item->maxDurability : entry.durability;
                ImGui::SetNextItemWidth(120.0f);
                if (FormUi::InputInt(localizedUiLabel("Durability##SelectedEntry").c_str(), &durability))
                    _controller.setInventoryEntryDurability(selectedEntryIndex, std::min(durability, item->maxDurability));
            }
            std::string stateNotes = entry.stateNotes;
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputText(localizedUiLabel("Notes##SelectedEntry").c_str(), &stateNotes))
                _controller.setInventoryEntryStateNotes(selectedEntryIndex, stateNotes);
            if (showTransfer)
            {
                if (drawUiButton("Move All",
                                 canTransferEntry,
                                 transferDisabledReason.c_str(),
                                 UiButtonRole::Primary,
                                 ImVec2(110.0f, 0.0f),
                                 transferTooltip.c_str()))
                    _controller.transferInventoryEntryToTarget(selectedEntryIndex, entry.quantity);
                TaskUi::sameLineIfFits();
            }
            if (drawUiButton("Remove",
                             UiButtonRole::Destructive,
                             ImVec2(110.0f, 0.0f),
                             "Remove this row from the selected owner inventory."))
                _controller.removeInventoryEntry(selectedEntryIndex);
            ImGui::EndTable();
        }
    }
    }
