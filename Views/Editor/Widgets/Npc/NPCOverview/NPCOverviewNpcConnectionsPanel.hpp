        if (_detailSection == 4)
        {
            ImGui::SeparatorText(localizedUiLabel("Inventory").c_str());
            showHelpTooltip("Items currently carried by this NPC. Equipped items affect combat if this NPC is added to a battle.");
            if (selectedNpc.inventory.entries.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("No inventory items assigned.").c_str());
            }
            else
            {
                int pendingRemovedInventoryEntry = -1;
                drawInventoryCombatSummary(selectedNpc.inventory, items);
                ImGui::SeparatorText(localizedUiLabel("Equipped Gear").c_str());
                drawInventoryGroupTable("SelectedNpcEquippedInventory", selectedNpc.inventory, items, true, [&](int entryIndex) {
                    _controller.toggleInventoryEntryEquipped(entryIndex);
                }, [&](int entryIndex) {
                    pendingRemovedInventoryEntry = entryIndex;
                }, [&](int entryIndex, int quantity) {
                    _controller.setInventoryEntryQuantity(entryIndex, quantity);
                });
                ImGui::SeparatorText(localizedUiLabel("Carried Items").c_str());
                drawInventoryGroupTable("SelectedNpcCarriedInventory", selectedNpc.inventory, items, false, [&](int entryIndex) {
                    _controller.toggleInventoryEntryEquipped(entryIndex);
                }, [&](int entryIndex) {
                    pendingRemovedInventoryEntry = entryIndex;
                }, [&](int entryIndex, int quantity) {
                    _controller.setInventoryEntryQuantity(entryIndex, quantity);
                });
                if (pendingRemovedInventoryEntry >= 0)
                    _controller.removeInventoryEntry(pendingRemovedInventoryEntry);
            }

        }

        if (_detailSection == 5)
        {
            const std::vector<StoryReference> storyReferences = findStoryReferences(campaign, selectedNpc.metadata.id, "NPC");
            ImGui::SeparatorText(localizedUiLabel("Connected Story Arcs").c_str());
            showHelpTooltip("Story arcs or scenes that explicitly connect to this NPC, faction, creature, or deity.");
            if (storyReferences.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("No story arcs are linked to this entry yet.").c_str());
            }
            else if (ImGui::BeginTable("SelectedNpcStories", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn(localizedUiLabel("Story Arc").c_str());
                ImGui::TableSetupColumn(localizedUiLabel("Arc Kind").c_str(), ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn(localizedUiLabel("Scene or Note").c_str());
                ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 96.0f);
                ImGui::TableHeadersRow();

                for (int i = 0; i < static_cast<int>(storyReferences.size()); ++i)
                {
                    const StoryReference &reference = storyReferences[i];
                    if (reference.threadIndex < 0 || reference.threadIndex >= static_cast<int>(campaign.storyThreads.size()))
                        continue;

                    const StoryThreadModel &thread = campaign.storyThreads[reference.threadIndex];
                    ImGui::PushID(i);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    tableTextWrapped(thread.title.empty() ? localizedUiLabel("<Untitled Story>") : thread.title);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(localizedUiText(storyThreadTypeLabel(thread.type)));
                    ImGui::TableSetColumnIndex(2);
                    if (!reference.sceneTitle.empty())
                        tableTextWrapped(reference.sceneTitle);
                    else if (!reference.note.empty())
                        tableTextWrapped(reference.note);
                    else
                        ImGui::TextDisabled(localizedUiLabel("Arc-level connection").c_str());
                    ImGui::TableSetColumnIndex(3);
                    if (ImGui::Button(localizedUiLabel("Open Story").c_str()))
                        _controller.openStoryThread(thread.metadata.id);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

        }

        if (_detailSection == 6)
        {
            if (!selectedNpc.conditions.empty())
            {
                ImGui::SeparatorText(localizedUiLabel("Current Conditions").c_str());
                for (const std::string &condition : selectedNpc.conditions)
                    ImGui::BulletText("%s", condition.c_str());
            }

            const std::vector<EncounterReference> encounterReferences = findEncounterReferences(campaign, selectedNpc.metadata.id);
            ImGui::SeparatorText(localizedUiLabel("Combat Appearances").c_str());
            showHelpTooltip("Prepared battlefields where this NPC has already been added as a combat participant.");
            if (encounterReferences.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("This entry is not currently present in a prepared combat.").c_str());
            }
            else
            {
                for (const EncounterReference &reference : encounterReferences)
                {
                    const std::string defeated = reference.defeated ? localizedUiLabel(" – defeated") : std::string{};
                    ImGui::BulletText(localizedUiLabel("%s – round %d%s").c_str(), reference.name.c_str(), reference.round, defeated.c_str());
                }
            }

        }

        if (_detailSection == 7)
        {
            drawMutedHelpText("Ability scores and limited-use resources are available here when this NPC needs mechanics.", "The table summary above stays visible for roleplay and quick reference.");

            if (!selectedNpc.attributes.empty())
            {
                ImGui::SeparatorText(localizedUiLabel("Ability Scores").c_str());
                showHelpTooltip("Core stats used by the ruleset and combat helpers.");
                if (ImGui::BeginTable("SelectedNpcAttributes", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn(localizedUiLabel("Ability").c_str());
                    ImGui::TableSetupColumn(localizedUiLabel("Score").c_str(), ImGuiTableColumnFlags_WidthFixed, 132.0f);
                    for (const auto &attribute : selectedNpc.attributes)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        tableTextWrapped(attribute.first);
                        ImGui::TableSetColumnIndex(1);
                        int score = attribute.second;
                        ImGui::PushID(attribute.first.c_str());
                        ImGui::SetNextItemWidth(-1.0f);
                        if (FormUi::InputInt(localizedUiLabel("##Score").c_str(), &score))
                            _controller.setSelectedNpcAttribute(attribute.first, score);
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::SeparatorText(localizedUiLabel("Resources").c_str());
            showHelpTooltip("Limited-use NPC resources with per-turn, rest, day, or custom recharge notes.");
            if (beginContextTreeNode("NPC Resource List", !selectedNpc.resourceTrackers.empty(), "Show limited-use NPC resources only when this NPC has resources to manage or you are adding one."))
            {
                if (selectedNpc.resourceTrackers.empty())
                {
                    ImGui::TextDisabled(localizedUiLabel("No resources defined.").c_str());
                }
                else if (ImGui::BeginTable("SelectedNpcResources", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn(localizedUiLabel("Resource").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.32f);
                    ImGui::TableSetupColumn(localizedUiLabel("Uses").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.22f);
                    ImGui::TableSetupColumn(localizedUiLabel("Recharge").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
                    ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 116.0f);
                    ImGui::TableHeadersRow();
                    int pendingRemovedResource = -1;
                    for (int trackerIndex = 0; trackerIndex < static_cast<int>(selectedNpc.resourceTrackers.size()); ++trackerIndex)
                    {
                        ResourceTrackerModel tracker = selectedNpc.resourceTrackers[static_cast<std::size_t>(trackerIndex)];
                        normalizeResourceTracker(tracker);
                        ImGui::PushID(trackerIndex);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        tableTextWrapped(tracker.name);
                        ImGui::TableSetColumnIndex(1);
                        int used = tracker.used;
                        ImGui::SetNextItemWidth(76.0f);
                        if (FormUi::InputInt(localizedUiLabel("##Used").c_str(), &used))
                            _controller.setSelectedNpcResourceUsed(trackerIndex, used);
                        ImGui::SameLine();
                        ImGui::TextDisabled(localizedUiLabel("/ %d max, %d left").c_str(), tracker.maximum, std::max(0, tracker.maximum - tracker.used));
                        ImGui::TableSetColumnIndex(2);
                        tableTextWrapped(tracker.rechargeRule);
                        ImGui::TableSetColumnIndex(3);
                        if (ImGui::SmallButton(localizedUiText("Reset")))
                            _controller.resetSelectedNpcResource(trackerIndex);
                        ImGui::SameLine();
                        if (ImGui::SmallButton(localizedUiText("Remove")))
                            pendingRemovedResource = trackerIndex;
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                    if (pendingRemovedResource >= 0)
                        _controller.removeResourceTrackerFromSelectedNpc(pendingRemovedResource);
                }
                ResourceTrackerModel &resourceDraft = _controller._npcResourceDraft;
                if (beginAdvancedTreeNode("Add NPC Resource", "Open only when creating a new limited-use NPC resource."))
                {
                    ImGui::BeginDisabled(_controller._resourceDraftPendingSave);
                    FormUi::InputText(localizedUiLabel("Resource Name").c_str(), &resourceDraft.name);
                    FormUi::InputInt(localizedUiLabel("Maximum Uses").c_str(), &resourceDraft.maximum);
                    FormUi::InputInt(localizedUiLabel("Used Uses").c_str(), &resourceDraft.used);
                    FormUi::InputText(localizedUiLabel("Recharge Rule").c_str(), &resourceDraft.rechargeRule);
                    FormUi::InputTextMultiline(localizedUiLabel("Resource Notes").c_str(), &resourceDraft.notes, ImVec2(-1.0f, 54.0f), ImGuiInputTextFlags_WordWrap);
                    normalizeResourceTracker(resourceDraft);
                    if (ImGui::Button(localizedUiLabel("Add Resource").c_str()))
                        _controller.addResourceTrackerToSelectedNpc();
                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }

        }


    }

    if (!compact) ImGui::EndTable();
