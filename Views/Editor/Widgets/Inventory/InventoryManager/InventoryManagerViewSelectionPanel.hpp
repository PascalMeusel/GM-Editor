    const bool germanInventory = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto &saveState = ProjectSaveService::getInstance().state();
    if (saveState.dirty || !saveState.lastError.empty())
    {
        if (ImGui::Button(germanInventory ? "Inventar speichern / erneut versuchen" : "Save inventory / retry"))
            _controller.persistInventory();
        if (!saveState.lastError.empty()) ImGui::TextWrapped("%s", saveState.lastError.c_str());
    }
    using InventoryTask = InventoryManagerController::InventoryTask;

    const InventoryTask tasks[] = {InventoryTask::FindItem, InventoryTask::EditOwnerInventory,
        InventoryTask::TransferBetweenOwners, InventoryTask::ManagePartyInventory, InventoryTask::LootParcels};
    const char *labels[] = {"Find Item", "Edit Owner Inventory", "Transfer Between Owners", "Manage Party Inventory", "Loot Parcels"};
    int selectedTask = 0;
    for (int i = 0; i < 5; ++i) if (_controller._activeTask == tasks[i]) selectedTask = i;
    TaskUi::fieldLabel(uiTextForLocale("What would you like to do?", "Was möchtest du tun?"));
    if (FormUi::BeginCombo("##InventoryTask", localizedUiText(labels[selectedTask])))
    {
        for (int i = 0; i < 5; ++i)
            if (ImGui::Selectable(localizedUiText(labels[i]), selectedTask == i)) _controller.selectInventoryTask(tasks[i]);
        ImGui::EndCombo();
    }

    const bool showFindItem = _controller._activeTask == InventoryTask::FindItem;
    const bool showEditOwner = _controller._activeTask == InventoryTask::EditOwnerInventory;
    const bool showTransfer = _controller._activeTask == InventoryTask::TransferBetweenOwners;
    const bool showParty = _controller._activeTask == InventoryTask::ManagePartyInventory;
    const bool showLootParcels = _controller._activeTask == InventoryTask::LootParcels;
    const bool showOwnerPicker = showFindItem || showEditOwner || showTransfer;
    const bool showItemLookup = showFindItem || showLootParcels;
    const bool showOwnerSummary = showEditOwner || showTransfer || showParty;
    const bool showOwnerItems = showEditOwner || showTransfer || showParty;
    const std::string localizedOwnerTypes = localizedComboItems("Player\0NPC\0Party\0");

    if (showParty && _controller._ownerType != InventoryManagerController::OwnerType::Party)
    {
        _controller.selectOwner(InventoryManagerController::OwnerType::Party, 0);
    }

    if (showOwnerPicker)
    {
        int ownerTypeIndex = _controller._ownerType == InventoryManagerController::OwnerType::Player ? 0 : (_controller._ownerType == InventoryManagerController::OwnerType::NPC ? 1 : 2);
        if (FormUi::Combo(localizedUiLabel("Inventory Owner").c_str(), &ownerTypeIndex, localizedOwnerTypes.c_str()))
        {
            _controller._ownerType = ownerTypeIndex == 0 ? InventoryManagerController::OwnerType::Player : (ownerTypeIndex == 1 ? InventoryManagerController::OwnerType::NPC : InventoryManagerController::OwnerType::Party);
            _controller.selectOwner(_controller._ownerType, 0);
            _controller._selectedInventoryEntryIndex = -1;
        }
        showHelpTooltip("Choose whether you are editing a player character, an NPC, or shared party inventory.");
    }
    else if (showParty)
    {
        ImGui::TextDisabled(localizedUiLabel("Editing shared party inventory.").c_str());
    }

    if (showOwnerPicker && _controller._ownerType == InventoryManagerController::OwnerType::Party)
    {
        ImGui::TextDisabled(localizedUiLabel("Editing shared party inventory.").c_str());
    }
    else if (showOwnerPicker && _controller._ownerType == InventoryManagerController::OwnerType::Player)
    {
        if (_controller._currentCampaign.characters.empty())
        {
            ImGui::TextWrapped(localizedUiLabel("No players available.").c_str());
            return;
        }

        if (FormUi::BeginCombo(localizedUiLabel("Character").c_str(), _controller.getSelectedOwnerName().c_str()))
        {
            for (int i = 0; i < static_cast<int>(_controller._currentCampaign.characters.size()); ++i)
            {
                const bool selected = _controller._selectedOwnerIndex == i;
                if (ImGui::Selectable(_controller._currentCampaign.characters[i].nameBuffer, selected))
                {
                    _controller.selectOwner(_controller._ownerType, i);
                    _controller._selectedInventoryEntryIndex = -1;
                }
            }
            ImGui::EndCombo();
        }
    }
    else if (showOwnerPicker && _controller._ownerType == InventoryManagerController::OwnerType::NPC)
    {
        if (_controller._currentCampaign.npcs.empty())
        {
            ImGui::TextWrapped(localizedUiLabel("No NPCs available.").c_str());
            return;
        }

        if (FormUi::BeginCombo(localizedUiLabel("NPC").c_str(), _controller.getSelectedOwnerName().c_str()))
        {
            for (int i = 0; i < static_cast<int>(_controller._currentCampaign.npcs.size()); ++i)
            {
                const std::string label = _controller._currentCampaign.npcs[i].name.empty() ? "<Unnamed NPC>" : _controller._currentCampaign.npcs[i].name;
                const bool selected = _controller._selectedOwnerIndex == i;
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    _controller.selectOwner(_controller._ownerType, i);
                    _controller._selectedInventoryEntryIndex = -1;
                }
            }
            ImGui::EndCombo();
        }
    }

    if (showTransfer)
    {
        ImGui::SeparatorText(localizedUiLabel("Transfer Target").c_str());
        int transferTypeIndex = _controller._transferOwnerType == InventoryManagerController::OwnerType::Player ? 0 : (_controller._transferOwnerType == InventoryManagerController::OwnerType::NPC ? 1 : 2);
        if (FormUi::Combo(localizedUiLabel("Transfer To").c_str(), &transferTypeIndex, localizedOwnerTypes.c_str()))
        {
            _controller._transferOwnerType = transferTypeIndex == 0 ? InventoryManagerController::OwnerType::Player : (transferTypeIndex == 1 ? InventoryManagerController::OwnerType::NPC : InventoryManagerController::OwnerType::Party);
            _controller.selectTransferOwner(_controller._transferOwnerType, 0);
        }
        if (_controller._transferOwnerType == InventoryManagerController::OwnerType::Player && !_controller._currentCampaign.characters.empty())
        {
            if (FormUi::BeginCombo(localizedUiLabel("Target Player").c_str(), _controller.getTransferOwnerName().c_str()))
            {
                for (int i = 0; i < static_cast<int>(_controller._currentCampaign.characters.size()); ++i)
                {
                    const bool selected = _controller._transferOwnerIndex == i;
                    if (ImGui::Selectable(_controller._currentCampaign.characters[i].nameBuffer, selected))
                        _controller.selectTransferOwner(_controller._transferOwnerType, i);
                }
                ImGui::EndCombo();
            }
        }
        else if (_controller._transferOwnerType == InventoryManagerController::OwnerType::NPC && !_controller._currentCampaign.npcs.empty())
        {
            if (FormUi::BeginCombo(localizedUiLabel("Target NPC").c_str(), _controller.getTransferOwnerName().c_str()))
            {
                for (int i = 0; i < static_cast<int>(_controller._currentCampaign.npcs.size()); ++i)
                {
                    const std::string label = _controller._currentCampaign.npcs[i].name.empty() ? "<Unnamed NPC>" : _controller._currentCampaign.npcs[i].name;
                    const bool selected = _controller._transferOwnerIndex == i;
                    if (ImGui::Selectable(label.c_str(), selected))
                        _controller.selectTransferOwner(_controller._transferOwnerType, i);
                }
                ImGui::EndCombo();
            }
        }
    }

    const std::vector<ItemModel> items = _controller.getAvailableItems();
    const std::vector<std::string> categories = _controller.getAvailableCategories(items);
    if (_controller._selectedCategoryIndex >= static_cast<int>(categories.size()))
        _controller._selectedCategoryIndex = 0;

    if (showItemLookup)
    {
        if (items.empty())
        {
            ImGui::TextWrapped(localizedUiLabel("No items are available from the ruleset or campaign.").c_str());
        }
        else
        {
            if (FormUi::BeginCombo(localizedUiLabel("Item Category").c_str(), categories[_controller._selectedCategoryIndex].c_str()))
            {
                for (int i = 0; i < static_cast<int>(categories.size()); ++i)
                {
                    const bool selected = _controller._selectedCategoryIndex == i;
                    if (ImGui::Selectable(categories[i].c_str(), selected))
                    {
                        _controller._selectedCategoryIndex = i;
                        // Preserve the selected ID; a hidden item is not silently replaced.
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            FormUi::InputText(localizedUiLabel("Search Items").c_str(), &_controller._itemSearch);
            showHelpTooltip("Filter the item list by item name, type, or category.");
        }
    }

    const std::vector<ItemModel> visibleItems = _controller.getVisibleItems(items, categories);
    _controller.reconcileItemSelection(visibleItems);
    InventoryModel *inventory = _controller.getSelectedInventory();
    if (!inventory)
    {
        ImGui::TextWrapped("%s", germanInventory ? "Dieser Inventarbesitzer ist nicht mehr verfügbar. Wähle einen anderen." :
            "This inventory owner is unavailable. Choose another owner.");
        return;
    }

    InventoryModel *transferInventory = _controller.getTransferInventory();
    const std::string ownerName = displayNameOrFallback(_controller.getSelectedOwnerName(), "No inventory owner");
    const std::string transferName = displayNameOrFallback(_controller.getTransferOwnerName(), "No transfer target");
    const bool transferTargetReady = transferInventory && transferInventory != inventory;
    if (showItemLookup && !visibleItems.empty())
    {
        const std::string preview = _controller._selectedItemIndex < 0 ?
            (germanInventory ? "Gegenstand wählen" : "Choose an item") : visibleItems[static_cast<std::size_t>(_controller._selectedItemIndex)].name;
        if (FormUi::BeginCombo(localizedUiLabel("Item").c_str(), preview.c_str()))
        {
            for (const auto &item : visibleItems)
                if (ImGui::Selectable(("[" + getItemCategory(item) + "] " + item.name + "##" + item.metadata.id).c_str(),
                    item.metadata.id == _controller._selectedItemId)) _controller.selectCatalogItem(item.metadata.id);
            ImGui::EndCombo();
        }
    }
    const std::string selectedItemName = _controller._selectedItemIndex < 0 ? std::string(localizedUiText("No matching item")) : visibleItems[static_cast<std::size_t>(_controller._selectedItemIndex)].name;
    if (showTransfer && !transferTargetReady)
        ImGui::TextWrapped("%s", localizedUiText("Choose a transfer target that differs from the source owner."));

    if (showItemLookup && _controller._selectedItemIndex >= 0)
    {
        const ItemModel &selectedItem = visibleItems[static_cast<std::size_t>(_controller._selectedItemIndex)];
        SelectedPlayerPreview::item(_controller._currentCampaign, _controller.currentSelectedItemId(), _controller._playerReviewHandler);
        if (_controller._editItemHandler)
        {
            const bool custom = std::any_of(_controller._currentCampaign.customItems.begin(), _controller._currentCampaign.customItems.end(),
                [&](const auto &item) { return item.metadata.id == selectedItem.metadata.id; });
            if (ImGui::Button(custom ? (germanInventory ? "Gegenstand bearbeiten" : "Edit item") :
                (germanInventory ? "Kopieren und bearbeiten" : "Copy and edit")))
                _controller._editItemHandler(selectedItem.metadata.id, !custom);
        }
        ImGui::Text(localizedUiLabel("Selected Item: %s").c_str(), selectedItem.name.c_str());
        TaskUi::sameLineIfFits();
        ImGui::TextDisabled(localizedUiLabel("Category: %s").c_str(), getItemCategory(selectedItem).c_str());
        ImGui::TextDisabled(localizedUiLabel("Type: %s | Value: %d cp | Weight: %.2f lb | %s").c_str(),
                            selectedItem.itemType.c_str(),
                            selectedItem.baseValue,
                            selectedItem.weight,
                            selectedItem.stackable ? "Stackable" : "Single item");
        if (isArmorItem(selectedItem))
            ImGui::TextDisabled(localizedUiLabel("Armor: AC +%d | Damage Reduction %d").c_str(), selectedItem.armorClassBonus, selectedItem.damageReduction);
        showHelpTooltip("AC means Armor Class: the number an attack roll must meet or beat. Damage Reduction subtracts from incoming damage.");
        if (isWeaponItem(selectedItem))
            ImGui::TextDisabled(localizedUiLabel("Weapon: %s | %s | Range %d ft").c_str(),
                                selectedItem.weaponDamage.empty() ? "manual damage" : selectedItem.weaponDamage.c_str(),
                                selectedItem.weaponAbility.c_str(),
                                selectedItem.weaponRangeFeet);
        showHelpTooltip("Equipped weapons become selectable attacks in the Battle Tracker.");

        if (showFindItem)
        {
        FormUi::InputInt(localizedUiLabel("Quantity").c_str(), &_controller._quantity);
        if (_controller._quantity < 1)
            _controller._quantity = 1;

        const std::string addTooltip = "Add " + std::to_string(_controller._quantity) + " " + selectedItem.name + " to " + ownerName + ".";
        if (drawUiButton("Add to Owner", UiButtonRole::Primary, ImVec2(0.0f, 0.0f), addTooltip.c_str()))
            _controller.addSelectedItem();
        TaskUi::sameLineIfFits();
        if (ImGui::Button(germanInventory ? "Inventar öffnen / ausrüsten" : "Open inventory / equip"))
            _controller.selectInventoryTask(InventoryTask::EditOwnerInventory);
        }

        if (ImGui::TreeNode(localizedUiLabel("Current Bearers").c_str()))
        {
        const auto bearers = _controller.getBearersForItem(selectedItem.metadata.id);
        showHelpTooltip("These are the players or NPCs currently carrying this exact item. Use Open Inventory to jump to that bearer.");
        if (bearers.empty())
        {
            ImGui::TextDisabled(localizedUiLabel("No player or NPC currently carries this item.").c_str());
        }
        else if (ImGui::BeginTable("SelectedItemBearers", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Bearer").c_str());
            ImGui::TableSetupColumn(localizedUiLabel("Kind").c_str(), ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn(localizedUiLabel("Qty").c_str(), ImGuiTableColumnFlags_WidthFixed, 46.0f);
            ImGui::TableSetupColumn(localizedUiLabel("Equipped").c_str(), ImGuiTableColumnFlags_WidthFixed, 74.0f);
            ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 112.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < static_cast<int>(bearers.size()); ++i)
            {
                const auto &bearer = bearers[i];
                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                tableTextWrapped(bearer.ownerName);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(localizedUiText(bearer.ownerType == InventoryManagerController::OwnerType::Player ? "Player" : "NPC"));
                ImGui::TableSetColumnIndex(2);
                ImGui::Text(localizedUiLabel("%d").c_str(), bearer.quantity);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(localizedUiText(bearer.equipped ? "Yes" : "No"));
                ImGui::TableSetColumnIndex(4);
                if (ImGui::Button(localizedUiLabel("Open Inventory").c_str()))
                    _controller.selectBearer(bearer);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
            ImGui::TreePop();
        }
    }
    else if (showItemLookup && !items.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No items match the current category or search.").c_str());
    }

    const IRuleSet *ruleset = _controller._currentCampaign.ruleSet.get();
