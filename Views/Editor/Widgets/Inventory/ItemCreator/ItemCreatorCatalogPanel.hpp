    if (!ImGui::BeginTabBar("ItemCatalogTabs"))
        return;

    if (ImGui::BeginTabItem(gm::localization::trUi("Catalog Browser"), nullptr, _openCatalog ? ImGuiTabItemFlags_SetSelected : 0))
    {
        _openCatalog = false;
        drawHelpText("Item Overview", "Browse ruleset-provided gear and campaign custom items together. Campaign items can be opened in the editor tab.");

        const std::vector<ItemCreatorController::CatalogEntry> entries = _controller.getCatalogEntries();
        const std::vector<std::string> categories = _controller.getCatalogCategories(entries);
        if (_controller._selectedCatalogCategoryIndex >= static_cast<int>(categories.size()))
            _controller._selectedCatalogCategoryIndex = 0;

        TaskUi::fieldLabel(gm::localization::trUi("Search Catalog"));
        FormUi::InputText("##CatalogSearch", &_controller._catalogSearch);
        if (_controller._selectedCatalogSourceIndex != 0 || _controller._selectedCatalogCategoryIndex != 0)
        {
            if (drawUiButton(uiTextForLocale("Clear active filters", "Aktive Filter zurücksetzen")))
            { _controller._selectedCatalogSourceIndex = 0; _controller._selectedCatalogCategoryIndex = 0; }
        }
        if (ImGui::TreeNode(uiTextForLocale("Filters", "Filter")))
        {
        const char *sourceFilters[] = {gm::localization::trUi("All Sources"), gm::localization::trUi("Ruleset"), gm::localization::trUi("Campaign")};
        ImGui::SetNextItemWidth(180.0f);
        FormUi::Combo(gm::localization::trUi("Source"), &_controller._selectedCatalogSourceIndex, sourceFilters, 3);
        TaskUi::sameLineIfFits();
        ImGui::SetNextItemWidth(220.0f);
        if (FormUi::BeginCombo(gm::localization::trUi("Category"), categories[_controller._selectedCatalogCategoryIndex].c_str()))
        {
            for (int i = 0; i < static_cast<int>(categories.size()); ++i)
            {
                const bool selected = _controller._selectedCatalogCategoryIndex == i;
                if (ImGui::Selectable(categories[i].c_str(), selected))
                {
                    _controller._selectedCatalogCategoryIndex = i;
                    _controller._selectedCatalogIndex = 0;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::TreePop();
        }

        const std::vector<ItemCreatorController::CatalogEntry> visibleEntries = _controller.getVisibleCatalogEntries(entries, categories);
        if (_controller._selectedCatalogIndex >= static_cast<int>(visibleEntries.size()))
            _controller._selectedCatalogIndex = 0;

        const bool browserWide = TaskUi::hasRoomForColumns(44.0f);
        if (ImGui::BeginTable("CatalogBrowserLayout", browserWide ? 2 : 1, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(gm::localization::trUi("Items"), ImGuiTableColumnFlags_WidthStretch, 0.58f);
            if (browserWide) ImGui::TableSetupColumn(gm::localization::trUi("Details"), ImGuiTableColumnFlags_WidthStretch, 0.42f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (visibleEntries.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No items match the current filters."));
            }
            else if (ImGui::BeginTable("CatalogBrowserItemsWithSource", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, TaskUi::listHeight(browserWide ? 0.95f : 0.4f, 30.0f))))
            {
                ImGui::TableSetupColumn(localizedUiText("Item"), ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn(localizedUiText("Category"));
        ImGui::TableSetupColumn(localizedUiText("Source"));
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

                for (int i = 0; i < static_cast<int>(visibleEntries.size()); ++i)
                {
                    const ItemCreatorController::CatalogEntry &entry = visibleEntries[static_cast<std::size_t>(i)];
                    const ItemModel &item = entry.item;
                    ImGui::PushID(i);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable(item.name.empty() ? "<Unnamed Item>" : item.name.c_str(), _controller._selectedCatalogIndex == i, ImGuiSelectableFlags_SpanAllColumns))
                        _controller._selectedCatalogIndex = i;
                    showHelpTooltip(item.description.empty() ? item.name.c_str() : item.description.c_str());
                    ImGui::TableSetColumnIndex(1);
                    tableTextWrapped(getItemCategory(item));
                    ImGui::TableSetColumnIndex(2);
                    tableTextWrapped(localizedUiLabel(entry.campaignCustom ? "Campaign" : "Ruleset"));
                    ImGui::PopID();
            }
                ImGui::EndTable();
            }

            ImGui::TableNextColumn();
            if (!visibleEntries.empty())
            {
                const ItemCreatorController::CatalogEntry &selectedEntry = visibleEntries[static_cast<std::size_t>(_controller._selectedCatalogIndex)];
                drawCatalogItemDetails(selectedEntry.item, selectedEntry.source);
                const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
                if (ImGui::Button(german ? "Als neuen Gegenstand kopieren" : "Copy as new item"))
                {
                    _controller.copyItemAsDraft(selectedEntry.item);
                    _openCustomEditor = true;
                }
                if (_controller._assignHandler && !isItemArchived(selectedEntry.item))
                {
                    TaskUi::sameLineIfFits();
                    if (ImGui::Button(german ? "Zuweisen / ausrüsten" : "Assign / equip"))
                        _controller._assignHandler(selectedEntry.item.metadata.id);
                }
                if (selectedEntry.campaignCustom)
                {
                    ImGui::Separator();
                    if (ImGui::Button(gm::localization::trUi("Open In Editor")))
                    {
                        _controller.selectCatalogCustomItem(selectedEntry);
                        _openCustomEditor = true;
                    }
                    showHelpTooltip("Load this campaign item into the custom item editor tab.");
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
