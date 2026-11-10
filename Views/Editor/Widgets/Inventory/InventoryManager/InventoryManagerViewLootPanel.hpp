    if (showLootParcels)
    {
    ImGui::SeparatorText(localizedUiLabel("Loot Parcels").c_str());
    ImGui::TextWrapped(localizedUiLabel("Loot parcels are temporary reward bundles (encounter drops, quest rewards, or chest contents).").c_str());
    ImGui::TextWrapped(localizedUiLabel("Fill each parcel with coins and selected items, then award it once so the party receives that reward set.").c_str());
    const int awardedParcelCount = static_cast<int>(std::count_if(_controller._currentCampaign.lootParcels.begin(),
                                                                  _controller._currentCampaign.lootParcels.end(),
                                                                  [](const LootParcelModel &parcel) { return parcel.awarded; }));
    const int pendingParcelCount = static_cast<int>(_controller._currentCampaign.lootParcels.size()) - awardedParcelCount;
    const std::string lootSelection = formatLocalizedUi("Destination: Party Inventory | Selected item: %s", selectedItemName.c_str());
    const std::string lootNextAction = _controller._currentCampaign.lootParcels.empty()
                                           ? std::string(localizedUiText("Create a reward bundle, then add coins or the selected item."))
                                           : formatLocalizedUi("%d pending, %d awarded. Non-empty pending parcels can be awarded.", pendingParcelCount, awardedParcelCount);
    UiStatusActionStrip lootStrip;
    lootStrip.mode = localizedUiText("Loot parcels: preview, add, award");
    lootStrip.selection = lootSelection.c_str();
    lootStrip.nextAction = lootNextAction.c_str();
    lootStrip.help = localizedUiText("Parcel rows preview contents, destination, and disabled reasons before any reward is awarded.");
    drawStatusActionStrip("InventoryManagerLootStrip", lootStrip);
    if (ImGui::Button(localizedUiLabel("New Loot Parcel").c_str()))
        _controller.createLootParcel();
    if (_controller._currentCampaign.lootParcels.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No loot parcels yet.").c_str());
    }
    else if (ImGui::BeginTable("LootParcels", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Parcel Name").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Coins").c_str(), ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Preview").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn(localizedUiLabel("Destination").c_str(), ImGuiTableColumnFlags_WidthFixed, 156.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Actions").c_str(), ImGuiTableColumnFlags_WidthFixed, 250.0f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < static_cast<int>(_controller._currentCampaign.lootParcels.size()); ++i)
        {
            LootParcelModel &parcel = _controller._currentCampaign.lootParcels[static_cast<std::size_t>(i)];
            const bool parcelAwarded = parcel.awarded;
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            std::string parcelName = parcel.name;
            if (FormUi::InputText(localizedUiLabel("##ParcelName").c_str(), &parcelName))
                _controller.setLootParcelName(i, parcelName);
            ImGui::TableSetColumnIndex(1);
            int copper = parcel.copper;
            ImGui::SetNextItemWidth(74.0f);
            ImGui::BeginDisabled(parcelAwarded);
            if (FormUi::InputInt(localizedUiLabel("cp##LootCp").c_str(), &copper))
                _controller.setLootParcelCoin(i, "cp", copper);
            ImGui::EndDisabled();
            showHelpTooltip(parcelAwarded ? "Awarded parcels keep their awarded coin amounts locked." : "Copper to add to this reward parcel.", ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled);
            TaskUi::sameLineIfFits();
            int silver = parcel.silver;
            ImGui::SetNextItemWidth(74.0f);
            ImGui::BeginDisabled(parcelAwarded);
            if (FormUi::InputInt(localizedUiLabel("sp##LootSp").c_str(), &silver))
                _controller.setLootParcelCoin(i, "sp", silver);
            ImGui::EndDisabled();
            showHelpTooltip(parcelAwarded ? "Awarded parcels keep their awarded coin amounts locked." : "Silver to add to this reward parcel.", ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled);
            int gold = parcel.gold;
            ImGui::SetNextItemWidth(74.0f);
            ImGui::BeginDisabled(parcelAwarded);
            if (FormUi::InputInt(localizedUiLabel("gp##LootGp").c_str(), &gold))
                _controller.setLootParcelCoin(i, "gp", gold);
            ImGui::EndDisabled();
            showHelpTooltip(parcelAwarded ? "Awarded parcels keep their awarded coin amounts locked." : "Gold to add to this reward parcel.", ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled);
            TaskUi::sameLineIfFits();
            int platinum = parcel.platinum;
            ImGui::SetNextItemWidth(74.0f);
            ImGui::BeginDisabled(parcelAwarded);
            if (FormUi::InputInt(localizedUiLabel("pp##LootPp").c_str(), &platinum))
                _controller.setLootParcelCoin(i, "pp", platinum);
            ImGui::EndDisabled();
            showHelpTooltip(parcelAwarded ? "Awarded parcels keep their awarded coin amounts locked." : "Platinum to add to this reward parcel.", ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled);
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(lootParcelAwardPreview(parcel, ruleset, items));
            showHelpTooltip("Coins and item entries queued for this reward parcel.");
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(lootParcelDestinationSummary(parcel));
            ImGui::TableSetColumnIndex(4);
            const std::string addItemDisabledReason = lootParcelAddItemDisabledReason(parcel, !visibleItems.empty());
            const bool canAddItemToParcel = addItemDisabledReason.empty();
            if (drawUiButton("Add Item",
                             canAddItemToParcel,
                             addItemDisabledReason.c_str(),
                             UiButtonRole::Secondary,
                             ImVec2(0.0f, 0.0f),
                             "Add the currently selected item to this parcel."))
                ImGui::OpenPopup("ParcelItemPicker");
            if(ImGui::BeginPopup("ParcelItemPicker")) {
                ImGui::TextWrapped("%s",parcel.name.c_str());
                for(const auto &item:items)if(!isItemArchived(item))
                    if(ImGui::Selectable((item.name+"##"+item.metadata.id).c_str()))_controller.addItemToLootParcel(i,item.metadata.id);
                ImGui::EndPopup();
            }
            TaskUi::sameLineIfFits();
            const std::string awardDisabledReason = lootParcelAwardDisabledReason(parcel);
            const bool canAwardParcel = awardDisabledReason.empty();
            if (drawUiButton("Award to Party",
                             canAwardParcel,
                             awardDisabledReason.c_str(),
                             UiButtonRole::Primary,
                             ImVec2(0.0f, 0.0f),
                             "Confirm awarding this parcel to Party Inventory."))
            {
                ImGui::OpenPopup(localizedUiLabel("Award Loot Parcel").c_str());
            }
            drawWrappedDisabledText(lootParcelActionDisabledSummary(parcel, addItemDisabledReason, awardDisabledReason));
            setResponsiveAutoSizeWindow(620.0f, 400.0f);
            if (ImGui::BeginPopupModal(localizedUiLabel("Award Loot Parcel").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                const std::string summary = lootParcelAwardPreview(parcel, ruleset, items);
                ImGui::TextWrapped(localizedUiLabel("Award '%s' to Party Inventory?").c_str(), parcel.name.empty() ? "Loot Parcel" : parcel.name.c_str());
                drawInlineLabelValue("Destination", "Party Inventory");
                drawInlineLabelValue("Preview", summary.c_str());
                ImGui::TextWrapped(localizedUiLabel("This marks the parcel as awarded and locks its reward controls.").c_str());
                if (drawModalActionButtons("Award", UiButtonRole::Primary, "Cancel", 110.0f))
                    _controller.awardLootParcelToParty(i);
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}
