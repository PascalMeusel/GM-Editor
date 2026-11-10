// Internal renderWorkspaceLanding continuation: ruleset review, save, and project actions.
                ImGui::EndPopup();
            }
        }
        if (!_rulesetUpgradeStatus.empty())
        {
            const std::string localizedUpgradeStatus = gm::rulesets::formatBackendError(
                _rulesetUpgradeStatus, gm::localization::LocalizationService::instance().locale());
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUpgradeStatus.c_str());
        }
        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        renderProjectTools();
    }

    if (beginCards())
    {
        switch (_activeWorkspace)
        {
        case EditorWorkspace::Story:
            if (WorkspaceUi::actionCard("story-board", uiText("Story board", "Szenenübersicht"),
                                        uiText("Build arcs, scenes, clues, consequences, and links.",
                                               "Plane Handlungsbögen, Szenen, Hinweise, Folgen und Verknüpfungen."),
                                        uiText("Open Story", "Geschichte öffnen"), UiButtonRole::Primary, nextCard()))
                _controller.showStoryWorkspace();
            if (WorkspaceUi::actionCard(
                    "story-live", uiText("Stage a scene", "Szene bereitstellen"),
                    uiText("Review the live session and move a prepared scene to the table.",
                           "Prüfe die Live-Sitzung und bringe eine vorbereitete Szene an den Tisch."),
                    uiText("Open Live", "Live öffnen"), UiButtonRole::Secondary, nextCard()))
                activateWorkspace(EditorWorkspace::Live);
            if (WorkspaceUi::actionCard("story-search", uiText("Find a connection", "Verknüpfung finden"),
                                        uiText("Search characters, maps, encounters, handouts, and backlinks.",
                                               "Durchsuche Figuren, Karten, Begegnungen, Handouts und Rückverweise."),
                                        uiText("Search campaign", "Kampagne durchsuchen"), UiButtonRole::Secondary,
                                        nextCard()))
                _universalSearchOpen = true;
            break;

        case EditorWorkspace::Cast:
            if (WorkspaceUi::actionCard("cast-party", uiText("Party", "Gruppe"),
                                        uiText("Review the player roster, party status, travel, and shared inventory.",
                                               "Prüfe Besetzung, Gruppenzustand, Reise und gemeinsames Inventar."),
                                        uiText("Open Party", "Gruppe öffnen"), UiButtonRole::Primary, nextCard()))
                _controller.showPlayerOverview();
            if (WorkspaceUi::actionCard(
                    "cast-sheets", uiText("Character sheets", "Charakterbögen"),
                    uiText("Edit a selected character's profile, rules, inventory, magic, and story.",
                           "Bearbeite Profil, Regeln, Inventar, Magie und Geschichte einer Figur."),
                    uiText("Open Sheets", "Bögen öffnen"), UiButtonRole::Secondary, nextCard()))
                _controller.showCharacterSheet();
            if (WorkspaceUi::actionCard("cast-npcs", uiText("NPCs and creatures", "NSC und Kreaturen"),
                                        uiText("Browse people, creatures, monsters, factions, and deities.",
                                               "Durchsuche Personen, Kreaturen, Monster, Fraktionen und Gottheiten."),
                                        uiText("Open Directory", "Verzeichnis öffnen"), UiButtonRole::Secondary,
                                        nextCard()))
                _controller.showNPCOverview();
            if (WorkspaceUi::actionCard(
                    "cast-new-player", uiText("Create player", "Spielfigur erstellen"),
                    uiText("Use a guided flow for identity, rules, abilities, story, and review.",
                           "Nutze einen geführten Ablauf für Identität, Regeln, Werte und Geschichte."),
                    uiText("New Player", "Neue Spielfigur"), UiButtonRole::Secondary, nextCard()))
                _controller.showPlayerCreator();
            if (WorkspaceUi::actionCard("cast-new-npc", uiText("Create NPC", "NSC erstellen"),
                                        uiText("Create a person, creature, faction figure, deity, or monster draft.",
                                               "Erstelle Person, Kreatur, Fraktionsfigur, Gottheit oder Monster."),
                                        uiText("New NPC", "Neuer NSC"), UiButtonRole::Secondary, nextCard()))
                _controller.showNPCCreator();
            break;

        case EditorWorkspace::World:
            if (WorkspaceUi::actionCard(
                    "world-maps", uiText("Atlas", "Atlas"),
                    uiText(
                        "Navigate world, region, site, and tactical maps; create linked details only where they are needed.",
                        "Navigiere durch Welt-, Regions-, Orts- und Kampfkarten und erstelle Details nur dort, wo sie gebraucht werden."),
                    uiText("Open Atlas", "Atlas öffnen"), UiButtonRole::Primary, nextCard()))
                _controller.showMapViewer();
            if (WorkspaceUi::actionCard(
                    "world-edit-map", uiText("Sketch the world", "Welt skizzieren"),
                    uiText(
                        "Draw the facts you already know, lock them, and let the generator fill the remaining geography.",
                        "Zeichne bekannte Vorgaben, sperre sie und lasse den Generator die übrige Geografie ergänzen."),
                    uiText("Edit Atlas Map", "Atlaskarte bearbeiten"), UiButtonRole::Secondary, nextCard()))
                _controller.showMapCreator();
            if (WorkspaceUi::actionCard(
                    "world-organizations", uiText("Nations and places", "Nationen und Orte"),
                    uiText(
                        "Define nations, capitals, cities, territories, factions, and powers as reusable world objects.",
                        "Definiere Nationen, Hauptstädte, Städte, Gebiete, Fraktionen und Mächte als wiederverwendbare Weltobjekte."),
                    uiText("Open World Directory", "Weltverzeichnis öffnen"), UiButtonRole::Secondary, nextCard()))
                _workspaceFilter.clear();
            if (WorkspaceUi::actionCard(
                    "world-shops", uiText("Shops", "Händler"),
                    uiText("Edit a world entity's stock and move items to or from the party.",
                           "Bearbeite Warenbestände und übertrage Gegenstände zur oder von der Gruppe."),
                    uiText("Open Shops", "Händler öffnen"), UiButtonRole::Secondary, nextCard()))
                _controller.showShopManager();
            if (WorkspaceUi::actionCard(
                    "world-dungeons", uiText("Dungeon structures", "Dungeonstrukturen"),
                    uiText("Maintain legacy room connections while dungeon maps move into the shared canvas.",
                           "Pflege Raumverbindungen im Kontext der gemeinsamen Kartenfläche."),
                    uiText("Open Rooms", "Räume öffnen"), UiButtonRole::Advanced, nextCard()))
                _controller.showDungeonBuilder();
            break;

        case EditorWorkspace::Encounters:
            if (WorkspaceUi::actionCard("encounter-prep", uiText("Prepare encounter", "Begegnung vorbereiten"),
                                        uiText("Build the roster, starting positions, difficulty, waves, and notes.",
                                               "Plane Besetzung, Startpositionen, Schwierigkeit, Wellen und Notizen."),
                                        uiText("Open Encounter Prep", "Vorbereitung öffnen"), UiButtonRole::Primary,
                                        nextCard()))
                _controller.showEncounterBuilder();
            if (WorkspaceUi::actionCard("encounter-map", uiText("Encounter map", "Begegnungskarte"),
                                        uiText("Prepare terrain and starting positions for the selected encounter.",
                                               "Bereite Gelände und Startpositionen der gewählten Begegnung vor."),
                                        uiText("Open Encounter Prep", "Begegnungsplanung öffnen"),
                                        UiButtonRole::Secondary, nextCard()))
                _controller.showEncounterBuilder();
            if (WorkspaceUi::actionCard("encounter-run", uiText("Run combat", "Kampf leiten"),
                                        uiText("Move a prepared encounter into the unified live-session flow.",
                                               "Übernimm die vorbereitete Begegnung in den gemeinsamen Live-Ablauf."),
                                        uiText("Open Live Combat", "Live-Kampf öffnen"), UiButtonRole::Secondary,
                                        nextCard()))
            {
                activateWorkspace(EditorWorkspace::Live);
                _controller.showBattleTracker();
            }
            break;

        case EditorWorkspace::Compendium:
            if (WorkspaceUi::actionCard(
                    "compendium-random-tables", uiText("Random tables", "Zufallstabellen"),
                    uiText("Author reusable weighted or dice-range prompts and roll them during Live play.",
                           "Erstelle wiederverwendbare gewichtete oder würfelbasierte Impulse und würfle sie im Live-Spiel."),
                    uiText("Open Tables", "Tabellen öffnen"), UiButtonRole::Primary, nextCard()))
            {
                _compendiumCategoryFilter = 4;
                _workspaceFilter.clear();
            }
            if (WorkspaceUi::actionCard("compendium-items", uiText("Browse equipment", "Ausrüstung durchsuchen"),
                                        uiText("Ruleset equipment stays read-only until you copy it; campaign items are your saved entries.",
                                               "Regelwerksausrüstung bleibt unverändert, bis du sie kopierst; Kampagnengegenstände sind deine gespeicherten Einträge."),
                                        uiText("Browse equipment", "Ausrüstung durchsuchen"), UiButtonRole::Primary, nextCard()))
                _controller.showItemCreator();
            if (WorkspaceUi::actionCard("compendium-spells", uiText("Browse spells", "Zauber durchsuchen"),
                                        uiText("Browse ruleset spells, then keep campaign spell entries separately.",
                                               "Durchsuche Regelwerkszauber und verwalte Kampagnenzauber getrennt davon."),
                                        uiText("Browse spells", "Zauber durchsuchen"), UiButtonRole::Secondary, nextCard()))
                _controller.showSpellLibrary();
            if (WorkspaceUi::actionCard(
                    "compendium-audio", uiText("Audio library", "Audiobibliothek"),
                    uiText("Import music, ambience, and effects; prepare playlists and triggers.",
                           "Importiere Musik, Atmosphäre und Effekte; plane Wiedergabelisten und Auslöser."),
                    uiText("Open Audio", "Audio öffnen"), UiButtonRole::Secondary, nextCard()))
                _controller.showAudioManager();
            if (WorkspaceUi::actionCard(
                    "compendium-inventory", uiText("Ownership and loot", "Besitz und Beute"),
                    uiText(
                        "Inventories belong to party members and actors in Cast; encounter loot stays with Encounter prep.",
                        "Inventare gehören zu Gruppenmitgliedern und Figuren; Begegnungsbeute bleibt in der Begegnungsplanung."),
                    uiText("Open Cast", "Figuren öffnen"), UiButtonRole::Secondary, nextCard()))
                activateWorkspace(EditorWorkspace::Cast);
            break;

        case EditorWorkspace::Live:
            if (WorkspaceUi::actionCard(
                    "live-session", uiText("Run the table", "Spieltisch leiten"),
                    uiText("Scenes, exploration, notes, clock, recap, and phase controls in one place.",
                           "Szenen, Erkundung, Notizen, Uhr, Rückblick und Phasensteuerung an einem Ort."),
                    uiText("Open Live Session", "Live-Sitzung öffnen"), UiButtonRole::Primary, nextCard()))
                _controller.showLiveSessionDashboard();
            if (WorkspaceUi::actionCard("live-combat", uiText("Combat", "Kampf"),
                                        uiText("Run initiative, movement, actions, effects, breaks, and wrap-up.",
                                               "Leite Initiative, Bewegung, Aktionen, Effekte, Pausen und Abschluss."),
                                        uiText("Open Combat", "Kampf öffnen"), UiButtonRole::Secondary, nextCard()))
                _controller.showBattleTracker();
            if (WorkspaceUi::actionCard(
                    "live-display", uiText("Player display", "Spieleranzeige"),
                    uiText(
                        "Preview and control only the scene, map, initiative, NPCs, and handouts players may see.",
                        "Prüfe und steuere ausschließlich Szenen, Karten, Initiative, NSC und Handouts, die Spieler sehen dürfen."),
                    uiText("Review Display", "Anzeige prüfen"), UiButtonRole::Secondary, nextCard()))
                _controller.showPresentationPanel();
            if (WorkspaceUi::actionCard(
                    "live-audio", uiText("Table audio", "Audio am Spieltisch"),
                    uiText(
                        "Control active music, ambience, effects, playlists, and break behavior.",
                        "Steuere laufende Musik, Atmosphäre, Effekte, Wiedergabelisten und das Verhalten in Pausen."),
                    uiText("Open Mixer", "Mixer öffnen"), UiButtonRole::Secondary, nextCard()))
                _controller.showAudioQuickControls();
            break;

        case EditorWorkspace::Settings: {
            const EditorLayoutProfileStatus &layoutStatus = EditorLayoutProfileService::getInstance().status();
            const std::vector<ProjectLogEntry> problems = ProjectLogger::getInstance().recentProblems(5);
            if (WorkspaceUi::actionCard(
                    "settings-save", uiText("Save and recovery", "Speichern und Wiederherstellung"),
                    saveState.dirty
                        ? uiText("This campaign has unsaved changes.",
                                 "Diese Kampagne enthält ungespeicherte Änderungen.")
                        : uiText("Autosave, recovery, and backups are healthy.",
                                 "Automatische Speicherung, Wiederherstellung und Sicherungen sind bereit."),
                    saveState.dirty ? uiText("Save Now", "Jetzt speichern")
                                    : gm::localization::tr(gm::localization::TextId::CommonSaved),
                    saveState.dirty ? UiButtonRole::Primary : UiButtonRole::Secondary, nextCard()) &&
                saveState.dirty)
                _controller.saveCurrentCampaign("settings save");
            if (WorkspaceUi::actionCard(
                    "settings-health", gm::localization::tr(gm::localization::TextId::SettingsProjectHealth),
                    problems.empty() ? uiText("No recent warnings or errors.", "Keine aktuellen Warnungen oder Fehler.")
                                     : (std::to_string(problems.size()) +
                                        uiText(" recent warnings or errors.", " aktuelle Warnungen oder Fehler.")),
                    uiText("Rebuild Index", "Index neu aufbauen"), UiButtonRole::Advanced, nextCard()))
                _controller.rebuildCampaignIndex();
            if (WorkspaceUi::actionCard("settings-layout", uiText("Workspace layout", "Arbeitsbereichslayout"),
                                        layoutStatus.lastError.empty()
                                            ? uiText("The stable workspace layout is available.",
                                                     "Das stabile Arbeitsbereichslayout ist verfügbar.")
                                            : layoutStatus.lastError.c_str(),
                                        uiText("Save Layout", "Layout speichern"), UiButtonRole::Advanced, nextCard()))
                EditorLayoutProfileService::getInstance().saveCurrentProfile();
            if (WorkspaceUi::actionCard(
                    "settings-audio", uiText("Asset diagnostics", "Asset-Diagnose"),
                    uiText(
                        "Inspect missing campaign audio and relink files without exposing diagnostics in normal workflows.",
                        "Prüfe fehlende Audiodateien und verknüpfe sie erneut."),
                    uiText("Open Audio Diagnostics", "Audio-Diagnose öffnen"), UiButtonRole::Advanced, nextCard()))
                _controller.showAudioManager();
            break;
        }

        case EditorWorkspace::Home:
        default:
            break;
        }
        ImGui::EndTable();
    }

    ImGui::End();
