// Internal renderWorkspaceLanding continuation: integrated directory configuration and creation.
                 _controller.workspaceEntityArchived("random_table", entryId),
                 "random_tables"});
        }
    }

    const bool usesIntegratedDirectory =
        _activeWorkspace == EditorWorkspace::Story || _activeWorkspace == EditorWorkspace::Cast ||
        _activeWorkspace == EditorWorkspace::World || _activeWorkspace == EditorWorkspace::Encounters ||
        _activeWorkspace == EditorWorkspace::Compendium;
    if (usesIntegratedDirectory)
    {
        bool createdDirectoryEntry=false;
        if(_activeWorkspace==EditorWorkspace::World)renderWorldDraftStatus();
        const auto currentSelection =
            std::find_if(directoryEntries.begin(), directoryEntries.end(),
                         [&](const WorkspaceEntityEntry &entry) { return entry.id == _workspaceSelectionId; });
        if (currentSelection == directoryEntries.end() && !directoryEntries.empty())
            _workspaceSelectionId = directoryEntries.front().id;

        const char *newLabel = "New";
        if (_activeWorkspace == EditorWorkspace::Story)
            newLabel = uiText("New story", "Neue Geschichte");
        else if (_activeWorkspace == EditorWorkspace::Cast)
            newLabel = uiText("New cast member", "Neue Figur");
        else if (_activeWorkspace == EditorWorkspace::World)
            newLabel = uiText("New world entry", "Neuer Welteintrag");
        else if (_activeWorkspace == EditorWorkspace::Encounters)
            newLabel = uiText("New encounter", "Neue Begegnung");
        else if (_activeWorkspace == EditorWorkspace::Compendium)
            newLabel = uiText("New compendium entry", "Neuer Kompendiumseintrag");

        const float toolbarSpacing = ImGui::GetStyle().ItemSpacing.x;
        const float newActionWidth = ImGui::CalcTextSize(newLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f + 20.0f;
        const float preferredFilterWidth = std::min(460.0f, availableWidth * 0.58f);
        const bool inlineDirectoryToolbar = preferredFilterWidth + toolbarSpacing + newActionWidth <= availableWidth;
        ImGui::SetNextItemWidth(inlineDirectoryToolbar ? preferredFilterWidth : -1.0f);
        FormUi::InputTextWithHint(
            localizedUiLabel("##workspace-filter").c_str(),
            uiText("Filter by name, type, or description...", "Nach Name, Typ oder Beschreibung filtern..."),
            &_workspaceFilter);
        if (inlineDirectoryToolbar)
            ImGui::SameLine();
        if (drawUiButton(newLabel, UiButtonRole::Primary, ImVec2(inlineDirectoryToolbar ? newActionWidth : -1.0f, 0.0f),
                         uiText("Create a new entry in this workspace.",
                                "Erstellt einen neuen Eintrag in diesem Arbeitsbereich.")))
        {
            switch (_activeWorkspace)
            {
            case EditorWorkspace::Story:
                _controller.showStoryWorkspace();
                break;
            case EditorWorkspace::Cast:
                ImGui::OpenPopup(localizedUiLabel("NewCastMember").c_str());
                break;
            case EditorWorkspace::World:
                ImGui::OpenPopup(localizedUiLabel("NewWorldEntry").c_str());
                break;
            case EditorWorkspace::Encounters:
                _controller.showNewEncounterBuilder();
                break;
            case EditorWorkspace::Compendium:
                ImGui::OpenPopup(localizedUiLabel("NewCompendiumEntry").c_str());
                break;
            default:
                break;
            }
        }
        if (_activeWorkspace == EditorWorkspace::Compendium)
        {
            ImGui::TextDisabled("%s", uiText("Campaign entries are listed below. Browse ruleset content separately.",
                                              "Kampagneneintr\u00e4ge stehen unten. Regelwerksinhalte werden getrennt durchsucht."));
            if (drawUiButton(uiText("Browse equipment", "Ausr\u00fcstung durchsuchen"), UiButtonRole::Secondary,
                             ImVec2(-1.0f, 0.0f),
                             uiText("Open the ruleset and campaign item catalog without creating an item draft.",
                                    "\u00d6ffnet den Regelwerks- und Kampagnenkatalog ohne einen Gegenstandsentwurf anzulegen.")))
                _controller.showItemCatalog();
            if (drawUiButton(uiText("Browse spells", "Zauber durchsuchen"), UiButtonRole::Secondary,
                             ImVec2(-1.0f, 0.0f),
                             uiText("Open the ruleset spell library; campaign spells remain separate saved entries.",
                                    "\u00d6ffnet die Regelwerks-Zauberbibliothek; Kampagnenzauber bleiben getrennte gespeicherte Eintr\u00e4ge.")))
                _controller.showSpellLibrary();
        }
        if (ImGui::BeginPopup(localizedUiLabel("NewCastMember").c_str()))
        {
            if (ImGui::MenuItem(uiText("Player character", "Spielercharakter")))
                _controller.showPlayerCreator();
            if (ImGui::MenuItem(uiText("NPC or creature", "NSC oder Kreatur")))
                _controller.showNPCCreator();
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup(localizedUiLabel("NewCompendiumEntry").c_str()))
        {
            if (ImGui::MenuItem(uiText("Item", "Gegenstand")))
                _controller.showItemCreator();
            if (ImGui::MenuItem(uiText("Spell", "Zauber")))
                _controller.showSpellLibrary();
            if (ImGui::MenuItem(uiText("Import audio", "Audio importieren")))
                _controller.showAudioManager();
            if (ImGui::MenuItem(uiText("Handout", "Handout")))
                _controller.showHandoutCreator();
            if (ImGui::MenuItem(uiText("Random table", "Zufallstabelle")))
            {
                if(saveRandomTableDraft()) {
                RandomTableModel table;
                table.name = uiText("New random table", "Neue Zufallstabelle");
                RandomTableEntryModel first;
                first.result = uiText("First result", "Erstes Ergebnis");
                RandomTableEntryModel second;
                second.result = uiText("Second result", "Zweites Ergebnis");
                table.entries = {first, second};
                std::string error;
                if (_controller.saveRandomTable(table, error))
                {
                    _compendiumCategoryFilter = 4;
                    _workspaceSelectionId = table.metadata.id;
                    createdDirectoryEntry=true;_workspaceFilter.clear();
                    reloadRandomTableDraft(table.metadata.id);
                }
                else
                    _randomTableStatus = error;
                }
            }
            ImGui::EndPopup();
        }
        bool openShopDraft=false;
        if (ImGui::BeginPopup(localizedUiLabel("NewWorldEntry").c_str()))
        {
            if (ImGui::MenuItem(uiText("Map", "Karte")))
                _controller.showMapCreator(true);
            if (ImGui::MenuItem(uiText("Nation", "Nation")))
            {
                const std::string defaultName = uiText("New Nation", "Neue Nation");
                const WorldInspector::MutationResult result = _controller.applyWorldInspectorMutation(
                    [defaultName](Campaign &value) { return WorldInspector::createNation(value, defaultName); },
                    "create nation");
                if (result.applied())
                {
                    _worldCategoryFilter = 2;
                    _workspaceSelectionId = result.entityId;
                    createdDirectoryEntry=true;_workspaceFilter.clear();
                }
                reportWorldInspectorResult(result, "Nation created.", "Nation erstellt.");
            }
            if (ImGui::MenuItem(uiText("City or place", "Stadt oder Ort")))
            {
                const std::string defaultName = uiText("New Place", "Neuer Ort");
                const WorldInspector::MutationResult result = _controller.applyWorldInspectorMutation(
                    [defaultName](Campaign &value) {
                        return WorldInspector::createWorldPlace(value, WorldPlaceType::City, defaultName);
                    },
                    "create world place");
                if (result.applied())
                {
                    _worldCategoryFilter = 2;
                    _workspaceSelectionId = result.entityId;
                    createdDirectoryEntry=true;_workspaceFilter.clear();
                }
                reportWorldInspectorResult(result, "Place created.", "Ort erstellt.");
            }
            if (ImGui::MenuItem(uiText("Shop", "Händler")))
            {
                openShopDraft=true;
            }
            if (ImGui::MenuItem(uiText("Dungeon structure", "Dungeonstruktur")))
            {
                const std::string defaultName = uiText("New Dungeon", "Neuer Dungeon");
                const WorldInspector::MutationResult result = _controller.applyWorldInspectorMutation(
                    [defaultName](Campaign &value) { return WorldInspector::createDungeon(value, defaultName); },
                    "create World dungeon");
                if (result.applied())
                {
                    _worldCategoryFilter = 5;
                    _workspaceSelectionId = result.entityId;
                    createdDirectoryEntry=true;_workspaceFilter.clear();
                }
                reportWorldInspectorResult(result, "Dungeon structure created.", "Dungeonstruktur erstellt.");
            }
            ImGui::EndPopup();
        }

        const char *shopDraftTitle=uiText("Create shop###CreateWorldShopDraft", "Händler erstellen###CreateWorldShopDraft");
        if(openShopDraft)ImGui::OpenPopup(shopDraftTitle);
        if(beginBoundedConfirmationModal(shopDraftTitle)) {
            ImGui::SeparatorText(uiText("Create shop", "Händler erstellen"));
            FormUi::InputText(uiText("Shop name", "Händlername"),&_worldNewShopName);
            FormUi::InputText(uiText("Merchant", "Händlerperson"),&_worldNewMerchantName);
            const bool named=_worldNewShopName.find_first_not_of(" \t\r\n")!=std::string::npos;
            ImGui::BeginDisabled(!named);
            if(drawUiButton(uiText("Create shop", "Händler erstellen"),UiButtonRole::Primary)) {
                const auto name=_worldNewShopName,merchant=_worldNewMerchantName;
                const auto result=_controller.applyWorldInspectorMutation([name,merchant](Campaign &value){
                    auto created=WorldInspector::createShop(value,name);
                    if(created.applied())if(auto *shop=WorldInspector::findShop(value,created.entityId))shop->merchantName=merchant;
                    return created;
                },"create named World shop");
                if(result.applied()) {
                    _worldCategoryFilter=4;_workspaceSelectionId=result.entityId;
                    createdDirectoryEntry=true;_workspaceFilter.clear();
                    _worldNewShopName.clear();_worldNewMerchantName.clear();ImGui::CloseCurrentPopup();
                }
                reportWorldInspectorResult(result,"Shop created.","Händler erstellt.");
            }
            ImGui::EndDisabled();
            TaskUi::sameLineIfFits();
            if(ImGui::Button(uiText("Cancel", "Abbrechen")))ImGui::CloseCurrentPopup();
            if(!_worldInspectorStatus.empty())ImGui::TextWrapped("%s",_worldInspectorStatus.c_str());
            ImGui::EndPopup();
        }
        const auto drawCategoryFilterRow = [&](const std::vector<std::string> &labels, int &selection) {
            const float available = ImGui::GetContentRegionAvail().x;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            float used = 0.0f;
            ImGui::PushID("WorkspaceCategoryFilters");
            for (int category = 0; category < static_cast<int>(labels.size()); ++category)
            {
                const float width = ImGui::CalcTextSize(labels[static_cast<std::size_t>(category)].c_str()).x +
                                    ImGui::GetStyle().FramePadding.x * 2.0f + 4.0f;
                if (category > 0 && used > 0.0f && used + spacing + width <= available)
                {
                    ImGui::SameLine();
                    used += spacing;
                }
                else if (category > 0)
                {
                    used = 0.0f;
                }
                ImGui::PushID(category);
                if (drawSmallUiButton(labels[static_cast<std::size_t>(category)].c_str(),
                                      selection == category ? UiButtonRole::Primary : UiButtonRole::Secondary))
                    selection = category;
                ImGui::PopID();
                used += width;
            }
            ImGui::PopID();
        };

        if (_activeWorkspace == EditorWorkspace::World)
        {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            const std::vector<std::string> categoryLabels = {
                std::string(uiText("All", "Alle")) + " (" + std::to_string(directoryEntries.size()) + ")",
                std::string(uiText("Maps", "Karten")) + " (" + std::to_string(campaign.maps.size()) + ")",
                std::string(uiText("Places", "Orte")) + " (" +
                    std::to_string(campaign.nations.size() + campaign.worldPlaces.size()) + ")",
                std::string(uiText("Organizations", "Organisationen")) + " (" +
                    std::to_string(campaign.factions.size() + campaign.deities.size()) + ")",
                std::string(uiText("Shops", "Händler")) + " (" + std::to_string(campaign.shops.size()) + ")",
                std::string(uiText("Dungeons", "Dungeons")) + " (" + std::to_string(campaign.dungeons.size()) + ")"};
            drawCategoryFilterRow(categoryLabels, _worldCategoryFilter);
        }
        else if (_activeWorkspace == EditorWorkspace::Compendium)
        {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            const std::vector<std::string> categoryLabels = {
                std::string(uiText("All", "Alle")) + " (" + std::to_string(directoryEntries.size()) + ")",
                std::string(uiText("Items", "Gegenstände")) + " (" + std::to_string(campaign.customItems.size()) + ")",
                std::string(uiText("Spells", "Zauber")) + " (" + std::to_string(campaign.customSpells.size()) + ")",
                std::string(uiText("Audio", "Audio")) + " (" + std::to_string(campaign.audioAssets.size()) + ")",
                std::string(uiText("Tables", "Tabellen")) + " (" + std::to_string(campaign.randomTables.size()) + ")"};
            drawCategoryFilterRow(categoryLabels, _compendiumCategoryFilter);
        }

        std::vector<const WorkspaceEntityEntry *> visibleEntries;
        for (const WorkspaceEntityEntry &entry : directoryEntries)
        {
            const bool categoryMatches =
                (_activeWorkspace != EditorWorkspace::Compendium || _compendiumCategoryFilter == 0 ||
                 (_compendiumCategoryFilter == 1 && entry.category == "items") ||
                 (_compendiumCategoryFilter == 2 && entry.category == "spells") ||
                 (_compendiumCategoryFilter == 3 && entry.category == "audio") ||
                 (_compendiumCategoryFilter == 4 && entry.category == "random_tables")) &&
                (_activeWorkspace != EditorWorkspace::World || _worldCategoryFilter == 0 ||
                 (_worldCategoryFilter == 1 && entry.category == "maps") ||
                 (_worldCategoryFilter == 2 && entry.category == "places") ||
                 (_worldCategoryFilter == 3 && entry.category == "organizations") ||
                 (_worldCategoryFilter == 4 && entry.category == "shops") ||
                 (_worldCategoryFilter == 5 && entry.category == "dungeons"));
            const std::string searchableText =
                entry.name + " " + entry.type + " " + localizedUiText(entry.type.c_str()) + " " + entry.summary;
            if (categoryMatches && (_workspaceFilter.empty() || containsText(searchableText, _workspaceFilter)))
                visibleEntries.push_back(&entry);
        }
        const bool selectionIsVisible =
            std::any_of(visibleEntries.begin(), visibleEntries.end(),
                        [&](const WorkspaceEntityEntry *entry) { return entry->id == _workspaceSelectionId; });
        if (!selectionIsVisible && !createdDirectoryEntry)
            _workspaceSelectionId = visibleEntries.empty() ? EntityId{} : visibleEntries.front()->id;

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        const std::string resultCount =
            std::to_string(visibleEntries.size()) +
            (visibleEntries.size() == 1 ? uiText(" entry", " Eintrag") : uiText(" entries", " Einträge"));
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), resultCount.c_str());
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        const bool showContextRail = availableWidth >= 1120.0f;
        if (ImGui::BeginTable("IntegratedWorkspaceDirectory", showContextRail ? 3 : 2,
