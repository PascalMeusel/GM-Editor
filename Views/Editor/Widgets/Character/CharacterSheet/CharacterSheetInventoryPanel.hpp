        if (ImGui::BeginTabItem(localizedUiLabel("Inventory").c_str()))
        {
    ImGui::SeparatorText(localizedUiLabel("Equipment and Inventory").c_str());
    showHelpTooltip("This view is centered on the selected character. Add common items here; use Inventory & Equipment for transfers, party inventory, shops, and loot parcels.");

    ImGui::SeparatorText(localizedUiLabel("Add Item").c_str());
    if (items.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No ruleset or campaign items are available to assign.").c_str());
        drawDisabledUiButton("Add Item", "Create or import items before assigning them to a character.", UiButtonRole::Primary);
    }
    else
    {
        if (_controller._selectedItemToAddIndex < 0 || _controller._selectedItemToAddIndex >= static_cast<int>(items.size()))
            _controller._selectedItemToAddIndex = 0;

        std::vector<int> filteredItemIndices;
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            if (itemAssignmentSearchMatches(_campaign, items[static_cast<std::size_t>(i)], _controller._itemAssignmentSearch))
                filteredItemIndices.push_back(i);
        }
        if (!filteredItemIndices.empty() &&
            std::find(filteredItemIndices.begin(), filteredItemIndices.end(), _controller._selectedItemToAddIndex) == filteredItemIndices.end())
        {
            _controller._selectedItemToAddIndex = filteredItemIndices.front();
        }

        ImGui::SetNextItemWidth(260.0f);
        FormUi::InputText(localizedUiLabel("Search Items").c_str(), &_controller._itemAssignmentSearch);
        ImGui::SameLine();
        ImGui::TextDisabled(localizedUiLabel("%d/%d").c_str(), static_cast<int>(filteredItemIndices.size()), static_cast<int>(items.size()));

        const bool hasFilteredItems = !filteredItemIndices.empty();
        const ItemModel *previewItem = hasFilteredItems ? &items[static_cast<std::size_t>(_controller._selectedItemToAddIndex)] : nullptr;
        const std::string comboPreview = previewItem ? previewItem->name : localizedUiLabel("No matching items");
        ImGui::BeginDisabled(!hasFilteredItems);
        if (FormUi::BeginCombo(localizedUiLabel("Assign Item").c_str(), comboPreview.c_str()))
        {
            for (const int itemIndex : filteredItemIndices)
            {
                const ItemModel &item = items[static_cast<std::size_t>(itemIndex)];
                const bool selected = _controller._selectedItemToAddIndex == itemIndex;
                const std::string category = localizedUiLabel(getItemCategory(item).c_str());
                const std::string type = localizedUiLabel(item.itemType.c_str());
                const std::string label = item.name + " – " + category + " – " + type + " – " + itemSourceLabel(_campaign, item);
                if (ImGui::Selectable(label.c_str(), selected))
                    _controller._selectedItemToAddIndex = itemIndex;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        if (previewItem && ImGui::BeginTable("CharacterItemAssignmentPreview", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Category").c_str());
            ImGui::TableSetupColumn(localizedUiLabel("Source").c_str());
            ImGui::TableSetupColumn(localizedUiLabel("Carry State").c_str());
            ImGui::TableSetupColumn(localizedUiLabel("Use").c_str());
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(localizedUiLabel(getItemCategory(*previewItem).c_str()));
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(itemSourceLabel(_campaign, *previewItem));
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(itemAssignmentContext(character, *previewItem));
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(localizedUiLabel(previewItem->stackable ? "Stackable" : "Single item"));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(localizedUiLabel("Value") + " " + std::to_string(previewItem->baseValue) + " " + localizedUiLabel("cp"));
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(localizedUiLabel("Weight") + " " + itemWeightLabel(previewItem->weight));
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(itemCombatSummary(*previewItem));
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(itemUseSummary(*previewItem));
            ImGui::EndTable();
        }

        ImGui::SetNextItemWidth(120.0f);
        FormUi::InputInt(localizedUiLabel("Quantity").c_str(), &_controller._itemAssignmentQuantity);
        _controller._itemAssignmentQuantity = std::max(1, _controller._itemAssignmentQuantity);
        ImGui::SameLine();
        FormUi::Checkbox(localizedUiLabel("Add Equipped").c_str(), &_controller._itemAssignmentEquipped);
        showHelpTooltip("Equipment state is applied to the new row. Stackable items merge into an existing row and preserve any existing equipped state.");

        std::string disabledReason;
        if (!hasFilteredItems)
            disabledReason = localizedUiLabel("No items match the search.");
        else if (!previewItem)
            disabledReason = localizedUiLabel("Choose an item to assign.");

        const std::string addTooltip = localizedUiLabel("Add the selected item to this character.");
        if (drawUiButton("Add Item",
                         disabledReason.empty(),
                         disabledReason.c_str(),
                         UiButtonRole::Primary,
                         ImVec2(120.0f, 0.0f),
                         addTooltip.c_str()) &&
            previewItem)
        {
            _controller.addItemToSelectedCharacter(previewItem->metadata.id, _controller._itemAssignmentQuantity, _controller._itemAssignmentEquipped);
        }
    }

    ImGui::SeparatorText(localizedUiLabel("Current Inventory").c_str());
    if (character.inventory.entries.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No inventory items assigned.").c_str());
    }
    else
    {
        int pendingRemovedInventoryEntry = -1;
        drawInventoryCombatSummary(character.inventory, items);
        ImGui::SeparatorText(localizedUiLabel("Equipped Gear").c_str());
        showHelpTooltip("Weapons, armor, and other items currently marked as equipped.");
        drawInventoryGroupTable("CharacterEquippedInventory", character.inventory, items, true, [&](int entryIndex) {
            _controller.toggleInventoryEntryEquipped(entryIndex);
        }, [&](int entryIndex) {
            pendingRemovedInventoryEntry = entryIndex;
        }, [&](int entryIndex, int quantity) {
            _controller.setInventoryEntryQuantity(entryIndex, quantity);
        });
        ImGui::SeparatorText(localizedUiLabel("Carried Items").c_str());
        showHelpTooltip("Items carried in bags, packs, containers, or pockets but not actively equipped.");
        drawInventoryGroupTable("CharacterCarriedInventory", character.inventory, items, false, [&](int entryIndex) {
            _controller.toggleInventoryEntryEquipped(entryIndex);
        }, [&](int entryIndex) {
            pendingRemovedInventoryEntry = entryIndex;
        }, [&](int entryIndex, int quantity) {
            _controller.setInventoryEntryQuantity(entryIndex, quantity);
        });
        if (pendingRemovedInventoryEntry >= 0)
            _controller.removeInventoryEntry(pendingRemovedInventoryEntry);
    }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndTable();
