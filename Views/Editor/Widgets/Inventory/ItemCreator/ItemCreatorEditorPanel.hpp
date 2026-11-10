    if (!ImGui::BeginTabItem(gm::localization::trUi("Custom Item Editor"), nullptr, _openCustomEditor ? ImGuiTabItemFlags_SetSelected : 0))
    {
        ImGui::EndTabBar();
        return;
    }

    _openCustomEditor = false;
    ImGui::BeginChild("ItemDraftFields", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 18.0f));
    const bool editorWide = false; // Creation has one dedicated draft surface.
    if (!ImGui::BeginTable("ItemCatalogEditorLayout", editorWide ? 2 : 1, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::EndChild();
        ImGui::EndTabItem();
        ImGui::EndTabBar();
        return;
    }

    ImGui::TableSetupColumn(gm::localization::trUi("Item Catalog"), ImGuiTableColumnFlags_WidthStretch, 0.34f);
    if (editorWide) ImGui::TableSetupColumn(gm::localization::trUi("Item Editor"), ImGuiTableColumnFlags_WidthStretch, 0.66f);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    const bool germanDrafts = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    if (!_controller._retainedDrafts.empty() && FormUi::BeginCombo("##RetainedItemDrafts",
            germanDrafts ? "Entwurf fortsetzen" : "Resume a draft"))
    {
        EntityId restoreId;
        for (const auto &entry : _controller._retainedDrafts)
        {
            const auto &draft = entry.second;
            const std::string label = (draft.item.name.empty() ?
                (germanDrafts ? "Unbenannter Entwurf" : "Untitled draft") : draft.item.name) + "##" + entry.first;
            if (ImGui::Selectable(label.c_str(), entry.first == _controller._model.metadata.id)) restoreId = entry.first;
        }
        ImGui::EndCombo();
        if (!restoreId.empty())
        {
            _controller.retainCurrentDraft();
            _controller.restoreDraft(restoreId);
        }
    }
    if (ImGui::Button(gm::localization::trUi("New Item"), ImVec2(-1.0f, 0.0f)))
        _controller.createNewDraft();

    if(ImGui::TreeNode(uiTextForLocale("Choose another campaign item","Anderen Kampagnengegenstand wählen"))) {
    ImGui::Separator();
    if (_controller._currentCampaign.customItems.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No custom items yet."));
    }
    else if (ImGui::BeginTable("CustomItemDirectoryTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, TaskUi::listHeight(editorWide ? 0.65f : 0.30f))))
    {
        ImGui::TableSetupColumn(localizedUiText("Item"), ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn(localizedUiText("Status"));
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(_controller._currentCampaign.customItems.size()); ++i)
        {
            const ItemModel &item = _controller._currentCampaign.customItems[static_cast<std::size_t>(i)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(item.name.empty() ? localizedUiText("Unnamed item") : item.name.c_str(), _controller._selectedItemIndex == i, ImGuiSelectableFlags_SpanAllColumns))
                _controller.selectItem(i);
            showHelpTooltip(item.description.empty() ? item.name.c_str() : item.description.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(localizedUiText(isItemArchived(item) ? "Archived" : "Active"));
            }
        ImGui::EndTable();
    }

    ImGui::TreePop();
    }
    ImGui::TableNextColumn();
    ItemModel &model = _controller.getModel();
    DraftEditSignal::Scope draftEdits([this] { _controller.markDraftEdited(); });

    if (!_controller._statusMessage.empty())
        ImGui::TextColored(ImVec4(0.48f, 0.78f, 0.54f, 1.0f), localizedUiLabel("%s").c_str(), _controller._statusMessage.c_str());
    if (!_controller._errorMessage.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.55f, 0.45f, 1.0f));
        ImGui::TextWrapped("%s", _controller._errorMessage.c_str());
        ImGui::PopStyleColor();
        if (ImGui::Button(gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? "Speicherung wiederholen" : "Retry saving"))
            _controller.saveItem();
    }

    TaskUi::fieldLabel(localizedUiText("Name"));

    FormUi::InputText("##Name", &model.name);
    SelectedPlayerPreview::item(_controller._currentCampaign, model.metadata.id, _controller._playerReviewHandler, &model);
    if (ImGui::TreeNode(uiTextForLocale("Custom catalog grouping", "Eigene Kataloggruppe"))) {
    TaskUi::fieldLabel(localizedUiText("Category"));
    FormUi::InputText("##Category", &model.category);
    showHelpTooltip("Broad bucket for browsing, such as Weapon, Armor, Magic Item, Tool, or Adventuring Gear.");
    const std::vector<std::string> categories = _controller.getAvailableCategories();
    const std::string categoryPreview = model.category.empty() ? gm::localization::trUi("Choose...") : localizedUiLabel(model.category.c_str());
    if (FormUi::BeginCombo(gm::localization::trUi("Category Preset"), categoryPreview.c_str()))
    {
        for (const auto &category : categories)
        {
            const bool selected = model.category == category;
            const std::string categoryLabel = localizedUiLabel(category.c_str());
            if (DraftEditSignal::record(ImGui::Selectable(categoryLabel.c_str(), selected)))
                model.category = category;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    TaskUi::fieldLabel(uiTextForLocale("Custom type name", "Eigener Typname"));
    FormUi::InputText("##CustomType", &model.itemType);
    ImGui::TreePop();
    }
    TaskUi::fieldLabel(localizedUiText("Type"));
    if(FormUi::BeginCombo("##Type",localizedUiLabel(model.itemType.c_str()).c_str())) {
        for(const char *type:{"Gear","Weapon","Armor","Consumable","Container","Custom"})
            if(ImGui::Selectable(localizedUiLabel(type).c_str(),model.itemType==type)) {
                model.itemType=type;model.category=type;_controller.markDraftEdited();
            }
        ImGui::EndCombo();
    }
    const char *rarities = "Common\0Uncommon\0Rare\0Very Rare\0Legendary\0Artifact\0Custom\0";
    const std::string localizedRarities = localizedComboItems(rarities);
    int rarityIndex = static_cast<int>(model.rarity);
    const bool factsWide=TaskUi::hasRoomForColumns(42.f);
    if(ImGui::BeginTable("ItemFacts",factsWide?3:1,ImGuiTableFlags_SizingStretchSame)) {
    ImGui::TableNextColumn();
    TaskUi::fieldLabel(localizedUiText("Rarity"));
    ImGui::SetNextItemWidth(-1);
    if (FormUi::Combo("##Rarity", &rarityIndex, localizedRarities.c_str()))
        model.rarity = static_cast<ItemRarity>(rarityIndex);
    ImGui::TableNextColumn();
    TaskUi::fieldLabel(localizedUiText("Weight (lb)"));
    FormUi::InputDouble("##Weight (lb)", &model.weight, 0.1, 1.0, "%.2f");
    ImGui::TableNextColumn();
    TaskUi::fieldLabel(localizedUiText("Value (cp)"));
    FormUi::InputInt("##Value (cp)", &model.baseValue);
    ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextUnformatted(gm::localization::trUi("Description"));
    FormUi::InputTextMultiline(localizedUiLabel("##ItemDescription").c_str(), &model.description, ImVec2(-1.0f, 90.0f), ImGuiInputTextFlags_WordWrap);
    ImGui::SeparatorText(uiTextForLocale("Player-facing image","Bild für Spieler"));
    if(ImGui::Button(uiTextForLocale("Attach image…","Bild anhängen…")))_controller.browseImage();
    if(const auto image=model.metadata.customProperties.find("item.image");image!=model.metadata.customProperties.end() && !image->second.empty()) {
        const auto &texture=getCachedImageTexture(ProjectAssets::resolveProjectPath(image->second));
        if(texture.loaded && texture.textureId && texture.width>0 && texture.height>0) {
            const float scale=std::min(std::min(220.f,ImGui::GetContentRegionAvail().x)/texture.width,180.f/texture.height);
            ImGui::Image(texture.textureId,ImVec2(texture.width*scale,texture.height*scale));
        } else ImGui::TextWrapped("%s",uiTextForLocale("Image missing or unreadable. Attach it again.","Bild fehlt oder ist nicht lesbar. Bitte erneut anhängen."));
        if(ImGui::Button(uiTextForLocale("Prepare image for players","Bild für Spieler vorbereiten")))_controller.prepareImageHandout();
        ImGui::TextWrapped("%s",uiTextForLocale("Saves a hidden handout. Review and reveal it separately in Player Display.","Speichert ein verborgenes Handout. Separat in der Spieleranzeige prüfen und freigeben."));
        if(ImGui::Button(uiTextForLocale("Remove image from draft","Bild aus Entwurf entfernen"))) {model.metadata.customProperties.erase("item.image");_controller.markDraftEdited();}
    }
    if (ImGui::TreeNode(uiTextForLocale("Equipment and rules", "Ausrüstung und Regeln")))
    {
    ImGui::SeparatorText(gm::localization::trUi("Item Capabilities"));
    FormUi::Checkbox(gm::localization::trUi("Stackable"), &model.stackable);
    showHelpTooltip("Allow inventory entries to represent multiple copies of this item.");
    FormUi::Checkbox(gm::localization::trUi("Container"), &model.container);
    showHelpTooltip("Enable bag, pack, chest, or similar carried-container fields.");
    TaskUi::sameLineIfFits();
    FormUi::Checkbox(gm::localization::trUi("Consumable"), &model.consumable);
    showHelpTooltip("Mark the item as spent when used, such as a potion, scroll, or alchemical item.");
    TaskUi::sameLineIfFits();
    FormUi::Checkbox(gm::localization::trUi("Ammunition"), &model.ammunition);
    showHelpTooltip("Track this item as ammunition or a consumable projectile.");

    const bool inventoryStateOpen = hasInventoryState(model);
    if (beginContextTreeNode("Inventory State", inventoryStateOpen, "Carrying rules, container capacity, expendable state, and durability."))
    {
        if (!inventoryStateOpen)
            ImGui::TextDisabled(localizedUiLabel("Enable Container, Consumable, Ammunition, or Durability when this item needs inventory state.").c_str());

        if (model.container)
        {
            TaskUi::fieldLabel(localizedUiText("Capacity Weight (lb)"));
            FormUi::InputDouble("##Capacity Weight (lb)", &model.containerCapacityWeight, 1.0, 5.0, "%.2f");
            TaskUi::fieldLabel(localizedUiText("Capacity Items"));
            FormUi::InputInt("##Capacity Items", &model.containerCapacityItems);
            model.containerCapacityWeight = std::max(0.0, model.containerCapacityWeight);
            model.containerCapacityItems = std::max(0, model.containerCapacityItems);
        }
        TaskUi::fieldLabel(localizedUiText("Max Durability"));
        FormUi::InputInt("##Max Durability", &model.maxDurability);
        showHelpTooltip("Leave at 0 when this item does not need durability tracking.");
        model.maxDurability = std::max(0, model.maxDurability);
        ImGui::TreePop();
    }

    const bool magicStateOpen = isMagicItemCandidate(model) || hasMagicItemState(model);
    if (beginContextTreeNode("Magic Item State", magicStateOpen, "Attunement, charges, activation, command words, curses, and spell links."))
    {
        if (!magicStateOpen)
            ImGui::TextDisabled(localizedUiLabel("Use this for magic items, charged items, spell-linked items, or cursed gear.").c_str());

        FormUi::Checkbox(localizedUiLabel("Requires Attunement").c_str(), &model.requiresAttunement);
        FormUi::Checkbox(localizedUiLabel("Cursed").c_str(), &model.cursed);
        TaskUi::fieldLabel(localizedUiText("Max Charges"));
        FormUi::InputInt("##Max Charges", &model.maxCharges);
        model.maxCharges = std::max(0, model.maxCharges);
        if (model.maxCharges > 0)
        {
            TaskUi::fieldLabel(localizedUiText("Recharge Rule"));
            FormUi::InputText("##Recharge Rule", &model.rechargeRule);
        }
        else if (!model.rechargeRule.empty())
        {
            ImGui::BeginDisabled(true);
            TaskUi::fieldLabel(localizedUiText("Recharge Rule"));
            FormUi::InputText("##Recharge Rule", &model.rechargeRule);
            ImGui::EndDisabled();
            showHelpTooltip("Set Max Charges above 0 to edit recharge behavior.", ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled);
        }
        TaskUi::fieldLabel(localizedUiText("Activation"));
        FormUi::InputText("##Activation", &model.activation);
        TaskUi::fieldLabel(localizedUiText("Command Words"));
        FormUi::InputText("##Command Words", &model.commandWords);
        if (FormUi::BeginCombo(localizedUiLabel("Linked Spell").c_str(), model.linkedSpellId.empty() ? localizedUiText("None") : model.linkedSpellId.c_str()))
        {
            if (DraftEditSignal::record(ImGui::Selectable(localizedUiLabel("None").c_str(), model.linkedSpellId.empty())))
                model.linkedSpellId.clear();
            for (const SpellModel &spell : _controller._currentCampaign.customSpells)
            {
                const bool selected = model.linkedSpellId == spell.metadata.id;
                const std::string label = spell.name.empty() ? spell.metadata.id : spell.name;
                if (DraftEditSignal::record(ImGui::Selectable(label.c_str(), selected)))
                    model.linkedSpellId = spell.metadata.id;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (_controller._currentCampaign.customSpells.empty())
            ImGui::TextDisabled(localizedUiLabel("No custom spells available to link.").c_str());
        ImGui::TreePop();
    }

    const bool combatPanelOpen = hasCombatMetadata(model);
