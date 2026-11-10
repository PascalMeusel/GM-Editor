// Internal renderWorkspaceLanding continuation: directory entry collection.
    struct WorkspaceEntityEntry
    {
        EntityId id;
        std::string type;
        std::string name;
        std::string summary;
        std::function<void()> open;
        std::string archiveType;
        bool archived = false;
        std::string category;
    };
    std::vector<WorkspaceEntityEntry> directoryEntries;
    const auto stableEntryId = [](const EntityId &id, const char *type, std::size_t index) {
        return id.empty() ? std::string(type) + "_" + std::to_string(index) : id;
    };
    const bool germanUi =
        gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto uiText = [germanUi](const char *english, const char *german) { return germanUi ? german : english; };
    const char *archivePopupTitle = uiText("Change archive state###ChangeWorkspaceArchiveState",
                                           "Archivstatus ändern###ChangeWorkspaceArchiveState");

    if (_activeWorkspace == EditorWorkspace::Story)
    {
        for (std::size_t index = 0; index < campaign.storyThreads.size(); ++index)
        {
            const StoryThreadModel &thread = campaign.storyThreads[index];
            if (thread.beats.empty())
            {
                const EntityId entryId = stableEntryId(thread.metadata.id, "story", index);
                directoryEntries.push_back(
                    {entryId, "Story arc",
                     thread.title.empty() ? uiText("Untitled story arc", "Unbenannter Handlungsbogen") : thread.title,
                     uiText("No scenes yet | ", "Noch keine Szenen | ") +
                         (thread.summary.empty() ? uiText("No summary yet", "Noch keine Zusammenfassung")
                                                 : thread.summary),
                     [this, entryId]() { _controller.openWorkspaceEntity("story", entryId); }});
                continue;
            }
            for (std::size_t beatIndex = 0; beatIndex < thread.beats.size(); ++beatIndex)
            {
                const StoryBeatModel &beat = thread.beats[beatIndex];
                const EntityId entryId = stableEntryId(beat.metadata.id, "scene", index * 1000 + beatIndex);
                directoryEntries.push_back(
                    {entryId, "Scene", beat.title.empty() ? uiText("Untitled scene", "Unbenannte Szene") : beat.title,
                     (thread.title.empty() ? uiText("Untitled story arc", "Unbenannter Handlungsbogen")
                                           : thread.title) +
                         " | " +
                         (beat.summary.empty() ? uiText("No summary yet", "Noch keine Zusammenfassung") : beat.summary),
                     [this, entryId]() { _controller.openWorkspaceEntity("scene", entryId); }});
            }
        }
    }
    else if (_activeWorkspace == EditorWorkspace::Cast)
    {
        for (std::size_t index = 0; index < campaign.characters.size(); ++index)
        {
            const CharacterModel &character = campaign.characters[index];
            const EntityId entryId = stableEntryId(character.metadata.id, "character", index);
            directoryEntries.push_back(
                {entryId, "Player character",
                 character.nameBuffer[0] ? std::string(character.nameBuffer)
                                         : uiText("Unnamed character", "Unbenannte Spielfigur"),
                 std::string(uiText("Level ", "Stufe ")) + std::to_string(character.level) +
                     (character.selectedClass.empty() ? std::string{}
                                                      : " " + localizedUiLabel(character.selectedClass.c_str())),
                 [this, entryId]() { _controller.openWorkspaceEntity("character", entryId); }, "character",
                 _controller.workspaceEntityArchived("character", entryId)});
        }
        for (std::size_t index = 0; index < campaign.npcs.size(); ++index)
        {
            const NPCModel &npc = campaign.npcs[index];
            const EntityId entryId = stableEntryId(npc.metadata.id, "npc", index);
            directoryEntries.push_back(
                {entryId, "NPC / creature", npc.name.empty() ? uiText("Unnamed NPC", "Unbenannter NSC") : npc.name,
                 npc.role.empty()
                     ? (npc.description.empty() ? uiText("No role yet", "Noch keine Rolle") : npc.description)
                     : npc.role,
                 [this, entryId]() { _controller.openWorkspaceEntity("npc", entryId); }, "npc",
                 _controller.workspaceEntityArchived("npc", entryId)});
        }
        for (std::size_t index = 0; index < campaign.customMonsters.size(); ++index)
        {
            const MonsterDefinitionModel &monster = campaign.customMonsters[index];
            const EntityId entryId = stableEntryId(monster.metadata.id, "monster", index);
            directoryEntries.push_back(
                {entryId, "Monster",
                 monster.name.empty() ? uiText("Unnamed monster", "Unbenanntes Monster") : monster.name,
                 std::string(uiText("CR ", "HG ")) +
                     (monster.challengeRating.empty() ? std::string("-") : monster.challengeRating) +
                     uiText(" | HP ", " | TP ") + std::to_string(monster.hitPoints),
                 [this, entryId]() { _controller.openWorkspaceEntity("monster", entryId); }, "monster",
                 _controller.workspaceEntityArchived("monster", entryId)});
        }
    }
    else if (_activeWorkspace == EditorWorkspace::World)
    {
        for (std::size_t index = 0; index < campaign.maps.size(); ++index)
        {
            const MapModel &map = campaign.maps[index];
            const EntityId entryId = stableEntryId(map.metadata.id, "map", index);
            directoryEntries.push_back({entryId,
                                        uiText("Map", "Karte"),
                                        map.name.empty() ? uiText("Untitled map", "Unbenannte Karte") : map.name,
                                        std::to_string(map.width) + " × " + std::to_string(map.height) + " | " +
                                            std::to_string(map.tokens.size()) + uiText(" tokens", " Marker"),
                                        [this, entryId]() { _controller.editWorkspaceMap(entryId); },
                                        {},
                                        false,
                                        "maps"});
        }
        for (std::size_t index = 0; index < campaign.nations.size(); ++index)
        {
            const NationModel &nation = campaign.nations[index];
            const EntityId entryId = stableEntryId(nation.metadata.id, "nation", index);
            directoryEntries.push_back(
                {entryId,
                 uiText("Nation", "Nation"),
                 nation.name.empty() ? uiText("Unnamed nation", "Unbenannte Nation") : nation.name,
                 nation.government.empty() ? (nation.culture.empty() ? uiText("No government or culture yet",
                                                                              "Noch keine Regierung oder Kultur")
                                                                     : nation.culture)
                                           : nation.government,
                 {},
                 {},
                 false,
                 "places"});
        }
        for (std::size_t index = 0; index < campaign.worldPlaces.size(); ++index)
        {
            const WorldPlaceModel &place = campaign.worldPlaces[index];
            const EntityId entryId = stableEntryId(place.metadata.id, "world_place", index);
            const char *placeType = place.type == WorldPlaceType::Capital    ? uiText("Capital", "Hauptstadt")
                                    : place.type == WorldPlaceType::City     ? uiText("City", "Stadt")
                                    : place.type == WorldPlaceType::Town     ? uiText("Town", "Kleinstadt")
                                    : place.type == WorldPlaceType::Village  ? uiText("Village", "Dorf")
                                    : place.type == WorldPlaceType::Landmark ? uiText("Landmark", "Landmarke")
                                                                             : uiText("Site", "Ort");
            directoryEntries.push_back({entryId,
                                        placeType,
                                        place.name.empty() ? uiText("Unnamed place", "Unbenannter Ort") : place.name,
                                        place.description.empty()
                                            ? uiText("No description yet", "Noch keine Beschreibung")
                                            : place.description,
                                        {},
                                        {},
                                        false,
                                        "places"});
        }
        for (std::size_t index = 0; index < campaign.factions.size(); ++index)
        {
            const FactionModel &faction = campaign.factions[index];
            const EntityId entryId = stableEntryId(faction.metadata.id, "faction", index);
            directoryEntries.push_back(
                {entryId,
                 faction.type.empty() ? uiText("Organization", "Organisation") : faction.type.c_str(),
                 faction.name.empty() ? uiText("Unnamed organization", "Unbenannte Organisation") : faction.name,
                 faction.summary.empty() ? uiText("No summary yet", "Noch keine Zusammenfassung") : faction.summary,
                 [this, entryId]() { _controller.openWorkspaceEntity("faction", entryId); },
                 {},
                 false,
                 "organizations"});
        }
        for (std::size_t index = 0; index < campaign.deities.size(); ++index)
        {
            const DeityModel &deity = campaign.deities[index];
            const EntityId entryId = stableEntryId(deity.metadata.id, "deity", index);
            directoryEntries.push_back(
                {entryId,
                 deity.powerType.empty() ? uiText("Power", "Macht") : deity.powerType.c_str(),
                 deity.name.empty() ? uiText("Unnamed power", "Unbenannte Macht") : deity.name,
                 deity.domains.empty() ? uiText("No domains yet", "Noch keine Domänen") : deity.domains,
                 [this, entryId]() { _controller.openWorkspaceEntity("deity", entryId); },
                 {},
                 false,
                 "organizations"});
        }
        for (std::size_t index = 0; index < campaign.shops.size(); ++index)
        {
            const ShopModel &shop = campaign.shops[index];
            directoryEntries.push_back({stableEntryId(shop.metadata.id, "shop", index),
                                        uiText("Shop", "Händler"),
                                        shop.name.empty() ? uiText("Unnamed shop", "Unbenannter Händler") : shop.name,
                                        std::to_string(shop.stock.size()) +
                                            uiText(" stock entries", " Bestandseinträge") +
                                            (shop.merchantName.empty() ? std::string{} : " | " + shop.merchantName),
                                        {},
                                        {},
                                        false,
                                        "shops"});
        }
        for (std::size_t index = 0; index < campaign.dungeons.size(); ++index)
        {
            const DungeonModel &dungeon = campaign.dungeons[index];
            directoryEntries.push_back(
                {stableEntryId(dungeon.metadata.id, "dungeon", index),
                 uiText("Dungeon structure", "Dungeonstruktur"),
                 dungeon.name.empty() ? uiText("Unnamed dungeon", "Unbenannter Dungeon") : dungeon.name,
                 std::to_string(dungeon.rooms.size()) + uiText(" rooms", " Räume"),
                 {},
                 {},
                 false,
                 "dungeons"});
        }
    }
    else if (_activeWorkspace == EditorWorkspace::Encounters)
    {
        for (std::size_t index = 0; index < campaign.encounters.size(); ++index)
        {
            const EncounterModel &encounter = campaign.encounters[index];
            const EntityId entryId = stableEntryId(encounter.metadata.id, "encounter", index);
            directoryEntries.push_back(
                {entryId,
                 encounter.theaterOfMind ? uiText("Theater of mind", "Erzähltheater")
                                         : uiText("Mapped encounter", "Begegnung mit Karte"),
                 encounter.name.empty() ? uiText("Untitled encounter", "Unbenannte Begegnung") : encounter.name,
                 std::to_string(encounter.participants.size()) +
                     uiText(" participants | Round ", " Beteiligte | Runde ") + std::to_string(encounter.round),
                 [this, entryId]() { _controller.openWorkspaceEntity("encounter", entryId); }});
        }
    }
    else if (_activeWorkspace == EditorWorkspace::Compendium)
    {
        for (std::size_t index = 0; index < campaign.customItems.size(); ++index)
        {
            const ItemModel &item = campaign.customItems[index];
            const EntityId entryId = stableEntryId(item.metadata.id, "item", index);
            directoryEntries.push_back(
                {entryId, "Item", item.name.empty() ? uiText("Unnamed item", "Unbenannter Gegenstand") : item.name,
                 item.category.empty()
                     ? (item.description.empty() ? uiText("Campaign item", "Kampagnengegenstand") : item.description)
                     : localizedUiLabel(item.category.c_str()),
                 [this, entryId]() { _controller.openWorkspaceEntity("item", entryId); }, "item",
                 _controller.workspaceEntityArchived("item", entryId), "items"});
        }
        for (std::size_t index = 0; index < campaign.customSpells.size(); ++index)
        {
            const SpellModel &spell = campaign.customSpells[index];
            const EntityId entryId = stableEntryId(spell.metadata.id, "spell", index);
            directoryEntries.push_back(
                {entryId,
                 "Spell",
                 spell.name.empty() ? uiText("Unnamed spell", "Unbenannter Zauber") : spell.name,
                 std::string(uiText("Level ", "Grad ")) + std::to_string(spell.level) + " | " + spell.castingTime,
                 [this, entryId]() { _controller.openWorkspaceEntity("spell", entryId); },
                 {},
                 false,
                 "spells"});
        }
        for (std::size_t index = 0; index < campaign.audioAssets.size(); ++index)
        {
            const AudioAssetModel &asset = campaign.audioAssets[index];
            const EntityId entryId = stableEntryId(asset.metadata.id, "audio", index);
            directoryEntries.push_back(
                {entryId,
                 "Audio",
                 asset.name.empty() ? uiText("Unnamed audio", "Unbenanntes Audio") : asset.name,
                 asset.missingFile ? uiText("Missing file - repair required", "Datei fehlt – Reparatur erforderlich")
                                   : (std::to_string(asset.durationSeconds) + uiText(" seconds", " Sekunden")),
                 [this, entryId]() { _controller.openWorkspaceEntity("audio", entryId); },
                 {},
                 false,
                 "audio"});
        }
        for (std::size_t index = 0; index < campaign.randomTables.size(); ++index)
        {
            const RandomTableModel &table = campaign.randomTables[index];
            const EntityId entryId = stableEntryId(table.metadata.id, "random_table", index);
            directoryEntries.push_back(
                {entryId,
                 uiText("Random table", "Zufallstabelle"),
                 table.name.empty() ? uiText("Unnamed random table", "Unbenannte Zufallstabelle") : table.name,
                 std::to_string(table.entries.size()) + uiText(" results", " Ergebnisse") + " | " +
                     (table.selectionMode == RandomTableSelectionMode::DiceRange
                          ? table.diceExpression
                          : uiText("weighted", "gewichtet")),
                 [this, entryId]() { _controller.openWorkspaceEntity("random_table", entryId); },
                 "random_table",
