    if (beginContextTreeNode("Combat Metadata", combatPanelOpen, "Armor defenses, weapon attack data, and combat-facing item actions."))
    {
        if (!isArmorItem(model) && !isWeaponItem(model))
            ImGui::TextDisabled(localizedUiLabel("Set Category or Type to Armor or Weapon to edit equipment combat fields.").c_str());

        if (isArmorItem(model))
        {
            TaskUi::fieldLabel(localizedUiText("Armor Class Bonus"));
            FormUi::InputInt("##Armor Class Bonus", &model.armorClassBonus);
            showHelpTooltip("How much this armor adds to Armor Class when equipped.");
            TaskUi::fieldLabel(localizedUiText("Damage Reduction"));
            FormUi::InputInt("##Damage Reduction", &model.damageReduction);
            showHelpTooltip("How much damage this armor subtracts from incoming hits when equipped.");
            model.armorClassBonus = std::max(0, model.armorClassBonus);
            model.damageReduction = std::max(0, model.damageReduction);
        }
        if (isWeaponItem(model))
        {
            TaskUi::fieldLabel(localizedUiText("Attack Bonus"));
            FormUi::InputInt("##Attack Bonus", &model.weaponAttackBonus);
            showHelpTooltip("Extra modifier added on top of ability and proficiency when this weapon is equipped.");
            TaskUi::fieldLabel(localizedUiText("Damage"));
            FormUi::InputText("##Damage", &model.weaponDamage);
            showHelpTooltip("Damage formula shown in combat, such as 1d8 piercing or 2d6 slashing.");
            TaskUi::fieldLabel(localizedUiText("Damage Type"));
            FormUi::InputText("##Damage Type", &model.weaponDamageType);
            showHelpTooltip("Damage type such as piercing, slashing, bludgeoning, fire, or radiant.");
            TaskUi::fieldLabel(localizedUiText("Attack Ability"));
            FormUi::InputText("##Attack Ability", &model.weaponAbility);
            showHelpTooltip("Use Strength, Dexterity, or Finesse. Finesse uses the better of Strength and Dexterity.");
            const char *attackKinds[] = {"Melee", "Ranged", "Thrown", "MeleeOrThrown"};
            std::vector<std::string> localizedAttackKinds;
            std::vector<const char *> localizedAttackKindLabels;
            localizedAttackKinds.reserve(4);
            localizedAttackKindLabels.reserve(4);
            int attackKindIndex = 0;
            for (int i = 0; i < 4; ++i)
            {
                if (model.weaponAttackKind == attackKinds[i])
                    attackKindIndex = i;
                localizedAttackKinds.push_back(localizedUiLabel(attackKinds[i]));
            }
            for (const std::string &label : localizedAttackKinds)
                localizedAttackKindLabels.push_back(label.c_str());
            if (FormUi::Combo(localizedUiLabel("Attack Kind").c_str(), &attackKindIndex, localizedAttackKindLabels.data(), 4))
                model.weaponAttackKind = attackKinds[attackKindIndex];
            TaskUi::fieldLabel(localizedUiText("Reach (ft)"));
            FormUi::InputInt("##Reach (ft)", &model.weaponReachFeet);
            TaskUi::fieldLabel(localizedUiText("Normal Range (ft)"));
            FormUi::InputInt("##Normal Range (ft)", &model.weaponRangeFeet);
            TaskUi::fieldLabel(localizedUiText("Long Range (ft)"));
            FormUi::InputInt("##Long Range (ft)", &model.weaponLongRangeFeet);
            TaskUi::fieldLabel(localizedUiText("Mastery"));
            FormUi::InputText("##Mastery", &model.weaponMastery);
            TaskUi::fieldLabel(localizedUiText("Versatile Damage"));
            FormUi::InputText("##Versatile Damage", &model.weaponVersatileDamage);
            TaskUi::fieldLabel(localizedUiText("Ammunition"));
            FormUi::InputText("##Ammunition", &model.ammunitionType);
            model.weaponReachFeet = std::max(0, model.weaponReachFeet);
            model.weaponRangeFeet = std::max(model.weaponReachFeet, model.weaponRangeFeet);
            model.weaponLongRangeFeet = std::max(model.weaponRangeFeet, model.weaponLongRangeFeet);

            if (beginContextTreeNode("Weapon Properties", !model.weaponProperties.empty(), "Weapon rules keywords used by combat and item browsing."))
            {
                propertyCheckbox(model, "Ammunition");
                TaskUi::sameLineIfFits();
                propertyCheckbox(model, "Finesse");
                TaskUi::sameLineIfFits();
                propertyCheckbox(model, "Heavy");
                propertyCheckbox(model, "Light");
                TaskUi::sameLineIfFits();
                propertyCheckbox(model, "Loading");
                TaskUi::sameLineIfFits();
                propertyCheckbox(model, "Thrown");
                propertyCheckbox(model, "Two-Handed");
                TaskUi::sameLineIfFits();
                propertyCheckbox(model, "Versatile");
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    ImGui::SeparatorText(gm::localization::trUi("Item Powers"));
    showHelpTooltip("Reusable item actions that appear in combat when the item is equipped or attuned.");
    if (beginContextTreeNode("Power List & Builder", !model.powers.empty(), "Reusable item actions that appear in combat when the item is equipped or attuned."))
    {
        if (model.powers.empty())
        {
            ImGui::TextDisabled(localizedUiLabel("No item powers defined.").c_str());
        }
        else if (ImGui::BeginTable("DraftItemPowers", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Name").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
            ImGui::TableSetupColumn(localizedUiLabel("Kind").c_str(), ImGuiTableColumnFlags_WidthFixed, 96.0f);
            ImGui::TableSetupColumn(localizedUiLabel("Range").c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn(localizedUiLabel("Damage").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.2f);
            ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();
            int pendingRemovedPower = -1;
            for (int powerIndex = 0; powerIndex < static_cast<int>(model.powers.size()); ++powerIndex)
            {
                const MonsterActionModel &power = model.powers[static_cast<std::size_t>(powerIndex)];
                ImGui::PushID(powerIndex);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), power.name.c_str());
                ImGui::TableSetColumnIndex(1);
                const std::string powerKind = power.actionKind.empty() ? localizedUiLabel("Item Power") : localizedUiLabel(power.actionKind.c_str());
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), powerKind.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text(localizedUiLabel("%d ft").c_str(), std::max(power.reachFeet, power.rangeFeet));
                ImGui::TableSetColumnIndex(3);
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), power.damage.empty() ? "-" : power.damage.c_str());
                ImGui::TableSetColumnIndex(4);
                if (ImGui::SmallButton(localizedUiLabel("Remove").c_str()))
                    pendingRemovedPower = powerIndex;
                ImGui::PopID();
            }
            ImGui::EndTable();
            if (pendingRemovedPower >= 0)
                _controller.removePowerFromDraftItem(pendingRemovedPower);
        }
        MonsterActionModel &powerDraft = _controller._itemPowerDraft;
        if (powerDraft.actionKind.empty())
            powerDraft.actionKind = "Item Power";
        if (beginAdvancedTreeNode("Add Item Power", "Open the builder only when this item needs a new combat-facing power."))
        {
            DraftEditSignal::Scope powerEdits([this] {
                _controller._powerDraftEdited = true;
                _controller.markDraftEdited();
            });
            TaskUi::fieldLabel(localizedUiText("Power Name"));
            FormUi::InputText("##Power Name", &powerDraft.name);
            TaskUi::fieldLabel(localizedUiText("Power Kind"));
            FormUi::InputText("##Power Kind", &powerDraft.actionKind);
            TaskUi::fieldLabel(localizedUiText("Power Reach"));
            FormUi::InputInt("##Power Reach", &powerDraft.reachFeet);
            TaskUi::fieldLabel(localizedUiText("Power Normal Range"));
            FormUi::InputInt("##Power Normal Range", &powerDraft.normalRangeFeet);
            TaskUi::fieldLabel(localizedUiText("Power Max Range"));
            FormUi::InputInt("##Power Max Range", &powerDraft.rangeFeet);
            TaskUi::fieldLabel(localizedUiText("Power Attack Bonus"));
            FormUi::InputText("##Power Attack Bonus", &powerDraft.attackBonus);
            TaskUi::fieldLabel(localizedUiText("Power Damage"));
            FormUi::InputText("##Power Damage", &powerDraft.damage);
            TaskUi::fieldLabel(localizedUiText("Power Notes"));
            FormUi::InputTextMultiline("##Power Notes", &powerDraft.notes, ImVec2(-1.0f, 64.0f), ImGuiInputTextFlags_WordWrap);
            powerDraft.reachFeet = std::max(0, powerDraft.reachFeet);
            powerDraft.normalRangeFeet = std::max(powerDraft.reachFeet, powerDraft.normalRangeFeet);
            powerDraft.rangeFeet = std::max(powerDraft.normalRangeFeet, powerDraft.rangeFeet);
            if (ImGui::Button(gm::localization::trUi("Add Item Power")))
                _controller.addPowerToDraftItem();
            if(drawUiButton(uiTextForLocale("Clear unfinished power", "Unfertige Kraft verwerfen"),
                _controller._powerDraftEdited,
                uiTextForLocale("There is no unfinished power to clear.", "Es gibt keine unfertige Kraft zum Verwerfen."),
                UiButtonRole::Secondary,ImVec2(-1,0),
                uiTextForLocale("Clear only this unfinished power. Existing powers and other item edits remain.",
                    "Verwirf nur diese unfertige Kraft. Vorhandene Kräfte und andere Gegenstandsänderungen bleiben erhalten.")))
                _controller.clearUnfinishedPower();
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }


    ImGui::Separator();
    ImGui::TreePop();
    }
    if (ImGui::Button(germanDrafts ? "Entwurf als neuen Gegenstand kopieren" : "Copy draft as new item"))
        _controller.copyItemAsDraft(model);
    if (_controller._assignHandler && _controller._selectedItemIndex >= 0 && !isItemArchived(model))
    {
        TaskUi::sameLineIfFits();
        if (ImGui::Button(germanDrafts ? "Speichern und zuweisen" : "Save and assign") && _controller.saveItem())
            _controller._assignHandler(_controller.getModel().metadata.id);
    }
    ImGui::SeparatorText(uiTextForLocale("Item preview","Gegenstandsvorschau"));
    ImGui::TextWrapped("%s",model.name.c_str());
    ImGui::TextWrapped("%s",model.description.c_str());
    ImGui::TextDisabled("%.2f lb · %s",model.weight,formatCopperValue(model.baseValue).c_str());
    if (_controller._selectedItemIndex >= 0)
    {
        const bool archived = isItemArchived(model);
        if (ImGui::Button(gm::localization::trUi(archived ? "Restore Item" : "Archive Item")))
            ImGui::OpenPopup(localizedUiLabel(archived ? "Restore Item" : "Archive Item").c_str());
        TaskUi::sameLineIfFits();
    }
    if (_controller._selectedItemIndex >= 0 && ImGui::Button(gm::localization::trUi("Delete Item")))
        ImGui::OpenPopup(localizedUiLabel("Delete Item").c_str());

    if (beginBoundedConfirmationModal(localizedUiLabel("Archive Item").c_str()))
    {
        const auto summary = _controller.getReferenceSummary(model.metadata.id);
        ImGui::TextWrapped(localizedUiLabel("Archive this custom item? Existing inventories and story links will keep their references.").c_str());
        if (summary.hasReferences())
        {
            ImGui::SeparatorText(gm::localization::trUi("Current References"));
            ImGui::TextWrapped(localizedUiLabel("Inventory entries: %d player, %d NPC | Quantity: %d | Story links: %d").c_str(),
                               summary.playerEntryCount,
                               summary.npcEntryCount,
                               summary.totalQuantity,
                               summary.storyLinkCount);
        }
        if (drawModalActionButtons("Archive", UiButtonRole::Advanced, "Cancel", 110.0f))
            _controller.archiveSelectedItem(true);
        ImGui::EndPopup();
    }

    if (beginBoundedConfirmationModal(localizedUiLabel("Restore Item").c_str()))
    {
        ImGui::TextWrapped(localizedUiLabel("Restore this archived item to active campaign use?").c_str());
        if (drawModalActionButtons("Restore", UiButtonRole::Primary, "Cancel", 110.0f))
            _controller.archiveSelectedItem(false);
        ImGui::EndPopup();
    }

    if (beginBoundedConfirmationModal(localizedUiLabel("Delete Item").c_str()))
    {
        const auto summary = _controller.getReferenceSummary(model.metadata.id);
        ImGui::TextWrapped(localizedUiLabel("Delete this custom item and remove it from inventories and story links?").c_str());
        if (summary.hasReferences())
        {
            ImGui::SeparatorText(gm::localization::trUi("References That Will Be Removed"));
            ImGui::TextWrapped(localizedUiLabel("Inventory entries: %d player, %d NPC | Quantity: %d | Story links: %d").c_str(),
                               summary.playerEntryCount,
                               summary.npcEntryCount,
                               summary.totalQuantity,
                               summary.storyLinkCount);
            const int maxLabels = std::min(6, static_cast<int>(summary.labels.size()));
            for (int i = 0; i < maxLabels; ++i)
                ImGui::BulletText("%s", summary.labels[static_cast<std::size_t>(i)].c_str());
            if (static_cast<int>(summary.labels.size()) > maxLabels)
                ImGui::TextDisabled(localizedUiLabel("...and %d more references.").c_str(), static_cast<int>(summary.labels.size()) - maxLabels);
        }
        else
        {
            ImGui::TextDisabled(localizedUiLabel("No inventory entries or story links reference this item.").c_str());
        }
        if (drawModalActionButtons("Delete", UiButtonRole::Destructive, "Cancel", 110.0f))
            _controller.deleteSelectedItem();
        ImGui::EndPopup();
    }

    ImGui::EndTable();
    ImGui::EndChild();
    ImGui::Separator();
    if (drawUiButton(_controller._selectedItemIndex >= 0 ? "Save Item" : "Create Item", UiButtonRole::Primary) && _controller.saveItem())
        _openCatalog = true;
    ImGui::EndTabItem();
    ImGui::EndTabBar();
