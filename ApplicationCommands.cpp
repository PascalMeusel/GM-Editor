#include "ApplicationInternal.hpp"
#include "Controllers/Editor/EditorDraftRecoveryController.hpp"

using namespace gm::application_detail;

void Application::renderQuickStartWindow()
{
    if (!quickStartWindowOpen)
        return;

    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto text = [german](const char *english, const char *germanText) { return german ? germanText : english; };
    ImGui::SetNextWindowSize(ImVec2(640.0f, 540.0f), ImGuiCond_Appearing);
    if (ImGui::Begin(text("Quick Start###GMEditorQuickStart", "Erste Schritte###GMEditorQuickStart"),
                     &quickStartWindowOpen))
    {
        ImGui::TextWrapped("%s", text(
            "GM Editor helps you prepare a campaign and run the next session from one local project.",
            "GM Editor hilft dir, eine Kampagne vorzubereiten und die nächste Spielsitzung aus einem lokalen Projekt zu leiten."));
        ImGui::Spacing();

        ImGui::SeparatorText(text("The shortest useful path", "Der kürzeste sinnvolle Weg"));
        ImGui::BulletText("%s", text("Create or open a campaign in Campaigns.",
                                     "Erstelle oder öffne unter Kampagnen eine Kampagne."));
        ImGui::BulletText("%s", text("Use Home to see what is ready and choose one clear next action.",
                                     "Nutze Start, um die Bereitschaft zu sehen und eine klare nächste Aktion zu wählen."));
        ImGui::BulletText("%s", text("Prepare people in Cast and scenes in Story; stage a scene when it is ready.",
                                     "Bereite Figuren unter Figuren und Szenen unter Geschichte vor; stelle eine fertige Szene bereit."));
        ImGui::BulletText("%s", text("Build or generate maps in World. A short note is enough—the generator fills gaps.",
                                     "Erstelle oder erzeuge Karten unter Welt. Eine kurze Notiz genügt – der Generator füllt Lücken."));
        ImGui::BulletText("%s", text("Prepare combat in Encounters, then transfer it to Live.",
                                     "Bereite Kämpfe unter Begegnungen vor und übernimm sie anschließend nach Live."));
        ImGui::BulletText("%s", text("Run Setup, Scene, Exploration, Combat, Break, and Wrap-up in Live.",
                                     "Leite in Live Aufbau, Szene, Erkundung, Kampf, Pause und Abschluss."));

        ImGui::Spacing();
        ImGui::SeparatorText(text("Where things live", "Wo du etwas findest"));
        ImGui::TextWrapped("%s", text(
            "Compendium contains items, spells, and audio. Rulesets defines executable sheets and rules. Settings contains backups, recovery, project health, language, and accessibility.",
            "Das Kompendium enthält Gegenstände, Zauber und Audio. Regelwerke definiert ausführbare Bögen und Regeln. Einstellungen enthält Sicherungen, Wiederherstellung, Projektstatus, Sprache und Barrierefreiheit."));

        ImGui::Spacing();
        ImGui::SeparatorText(text("Useful controls", "Nützliche Bedienung"));
        ImGui::BulletText("%s", text("Ctrl+K: search entities, navigate, or run a command.",
                                     "Strg+K: Inhalte suchen, navigieren oder einen Befehl ausführen."));
        ImGui::BulletText("%s", text("Ctrl+S: save the current campaign. F1: keyboard help.",
                                     "Strg+S: aktuelle Kampagne speichern. F1: Tastaturhilfe."));
        ImGui::BulletText("%s", text("Player Display is the only player-facing window. Review its public state before arming it.",
                                     "Die Spieleranzeige ist das einzige Fenster für Spieler. Prüfe den öffentlichen Zustand vor dem Aktivieren."));

        ImGui::Spacing();
        ImGui::TextDisabled("%s", text(
            "Campaign data, backups, and settings remain on this computer unless you explicitly export a package.",
            "Kampagnendaten, Sicherungen und Einstellungen bleiben auf diesem Computer, sofern du nicht bewusst ein Paket exportierst."));
        ImGui::Spacing();
        if (ImGui::Button(text("Close", "Schließen"), ImVec2(120.0f, 0.0f)))
            quickStartWindowOpen = false;
        ImGui::SetItemDefaultFocus();
    }
    ImGui::End();
}

void Application::renderSupportWindow()
{
    if (!supportWindowOpen)
        return;

    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto text = [german](const char *english, const char *germanText) { return german ? germanText : english; };
    constrainAutoSizedWindowToViewport(560.0f);
    if (ImGui::Begin(text("About & Support###GMEditorSupport", "Über & Support###GMEditorSupport"),
                     &supportWindowOpen,
                     ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(ProductInfo::windowTitle);
        ImGui::TextDisabled("%s %s", text("Version", "Version"), ProductInfo::version);
        ImGui::Separator();
        ImGui::TextWrapped("%s", text(
            "GM Editor is a free, open-source campaign preparation and local session tool. This community preview focuses on local workflows; online play, cloud synchronization, and runtime AI are not included.",
            "GM Editor ist ein kostenloses Open-Source-Werkzeug für Kampagnenvorbereitung und lokale Spielsitzungen. Diese Community-Vorschau konzentriert sich auf lokale Abläufe; Online-Spiel, Cloud-Synchronisierung und Laufzeit-KI sind nicht enthalten."));

        ImGui::Spacing();
        ImGui::TextUnformatted(text("Local files", "Lokale Dateien"));
        ImGui::Separator();
        const std::string dataRoot = ApplicationPaths::userDataRoot().u8string();
        const std::string logPath = (ApplicationPaths::userDataRoot() / "Logs" / "last-crash.txt").u8string();
        ImGui::TextDisabled("%s", text("Campaign data, settings, backups, and logs stay on this computer.",
                                       "Kampagnendaten, Einstellungen, Sicherungen und Logs bleiben auf diesem Computer."));
        ImGui::TextWrapped("%s: %s", text("Data folder", "Datenordner"), dataRoot.c_str());
        ImGui::TextWrapped("%s: %s", text("Last crash report", "Letzter Absturzbericht"), logPath.c_str());
        const char *copyDataLabel = text("Copy data folder", "Datenordner kopieren");
        const char *copyDiagnosticsLabel = text("Copy diagnostics", "Diagnose kopieren");
        const float actionPadding = ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
        const gm::ux::ResponsiveTwoActionLayout copyActions = gm::ux::responsiveTwoActionLayout(
            ImGui::GetContentRegionAvail().x,
            ImGui::GetStyle().ItemSpacing.x,
            ImGui::CalcTextSize(copyDataLabel).x + actionPadding,
            ImGui::CalcTextSize(copyDiagnosticsLabel).x + actionPadding);
        if (drawUiButton(copyDataLabel, UiButtonRole::Secondary, ImVec2(copyActions.firstWidth, 0.0f)))
            ImGui::SetClipboardText(dataRoot.c_str());
        if (copyActions.inlineActions)
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
        if (drawUiButton(copyDiagnosticsLabel, UiButtonRole::Secondary, ImVec2(copyActions.secondWidth, 0.0f)))
        {
            std::ostringstream diagnostics;
            diagnostics << "GM Editor " << ProductInfo::version << " " << ProductInfo::releaseChannel << '\n'
                        << "locale=" << (german ? "de" : "en") << '\n'
                        << "storage=" << ApplicationPaths::storageDescription() << '\n';
            ImGui::SetClipboardText(diagnostics.str().c_str());
        }
        ImGui::TextDisabled("%s", text(
            "Copied diagnostics contain product settings only. Campaign text and files are never attached automatically.",
            "Die kopierte Diagnose enthält nur Produktangaben. Kampagnentexte und Dateien werden niemals automatisch angehängt."));

        ImGui::Spacing();
        ImGui::TextWrapped("%s", text(
            "For help, include the copied diagnostics, what you expected, and the steps that led to the problem. Add campaign files only when you deliberately choose to share them.",
            "Für Hilfe sende die kopierte Diagnose, das erwartete Ergebnis und die Schritte bis zum Problem. Füge Kampagnendateien nur hinzu, wenn du sie bewusst teilen möchtest."));
        ImGui::Spacing();
        if (ImGui::Button(text("Close", "Schließen"), ImVec2(120.0f, 0.0f)))
            supportWindowOpen = false;
        ImGui::SetItemDefaultFocus();
    }
    ImGui::End();
}

void Application::requestEditorAppCommand(EditorAppCommand command)
{
    if (command == EditorAppCommand::None)
        return;

    if (activeWindowController == editor &&
        (!editor->checkpointEditorDraftRecoveryForLeave() || !editor->checkpointWorldRecoveryForLeave() || !editor->checkpointOutcomeRecoveryForLeave() || editor->hasUnsavedChanges()))
    {
        pendingEditorAppCommand = command;
        editorAppCommandPromptRequested = true;
        return;
    }

    executeEditorAppCommand(command);
}

void Application::executeEditorAppCommand(EditorAppCommand command)
{
    switch (command)
    {
    case EditorAppCommand::LoadGame:
    case EditorAppCommand::BackToMainMenu:
        editor->setWindowOpacity(false);
        mainMenu->setWindowOpacity(true);
        activeWindowController = mainMenu;
        break;
    case EditorAppCommand::ExitProgram:
        glfwSetWindowShouldClose(mainWindow.get(), true);
        break;
    case EditorAppCommand::None:
        break;
    }
}

void Application::renderEditorAppCommandPrompt()
{
    auto &localization = gm::localization::LocalizationService::instance();
    const std::string popupTitle = std::string(localization.uiText("Unsaved Campaign Changes")) +
                                   "###EditorAppCommandPrompt";
    if (editorAppCommandPromptRequested)
    {
        ImGui::OpenPopup(popupTitle.c_str());
        editorAppCommandPromptRequested = false;
    }

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(520.0f, 0.0f),
        ImVec2(std::min(720.0f, viewport->WorkSize.x * 0.90f), viewport->WorkSize.y * 0.90f));
    if (!ImGui::BeginPopupModal(popupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    const char *question = localization.uiText("Save this campaign before continuing?");
    std::string saveReason = "save before leaving campaign";
    std::string discardReason = "leaving campaign";
    switch (pendingEditorAppCommand)
    {
    case EditorAppCommand::LoadGame:
        question = localization.uiText("Open another campaign? Save this campaign first.");
        saveReason = "save before load game";
        discardReason = "loading another campaign";
        break;
    case EditorAppCommand::BackToMainMenu:
        question = localization.uiText("Return to campaign selection? Save this campaign first.");
        saveReason = "save before main menu";
        discardReason = "returning to campaign selection";
        break;
    case EditorAppCommand::ExitProgram:
        question = localization.uiText("Exit GM Editor? Save this campaign first.");
        saveReason = "save before exit";
        discardReason = "exiting the program";
        break;
    case EditorAppCommand::None:
        break;
    }

    const ProjectSaveState &saveState = ProjectSaveService::getInstance().state();
    ImGui::TextWrapped("%s", question);
    if(!editor->worldRecoveryError().empty())ImGui::TextWrapped("%s",editor->worldRecoveryError().c_str());
    if(!editor->editorDraftRecovery().error().empty())ImGui::TextWrapped("%s",editor->editorDraftRecovery().error().c_str());
    if(!editor->draftDiscardError().empty())ImGui::TextWrapped("%s",editor->draftDiscardError().c_str());
    if (!editor->outcomeRecoveryError().empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.42f, 0.38f, 1.0f));
        ImGui::TextWrapped("%s", editor->outcomeRecoveryError().c_str());
        ImGui::PopStyleColor();
    }
    if (!editor->worldDraftSaveError().empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.42f, 0.38f, 1.0f));
        ImGui::TextWrapped("%s", editor->worldDraftSaveError().c_str());
        ImGui::PopStyleColor();
    }
    if (!saveState.pendingChangeSummary.empty())
    {
        ImGui::Spacing();
        ImGui::TextUnformatted(localization.uiText("Changed since last save:"));
        for (const std::string &change : saveState.pendingChangeSummary)
        {
            const char *translatedChange = localization.uiText(change.c_str());
            if (localization.locale() == gm::localization::Locale::German && std::string(translatedChange) == change)
                translatedChange = localization.uiText("Campaign content changed");
            ImGui::BulletText("%s", translatedChange);
        }
    }
    if (!saveState.lastError.empty())
    {
        const char *localizedError = localization.uiText(saveState.lastError.c_str());
        if (localization.locale() == gm::localization::Locale::German &&
            std::string(localizedError) == saveState.lastError)
        {
            localizedError = localization.uiText(
                "The campaign could not be saved. Open Settings > Project Health for details.");
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.42f, 0.38f, 1.0f));
        ImGui::TextWrapped(localization.uiText("Last save error: %s"), localizedError);
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    const char *saveLabel = localization.uiText("Save and Continue");
    const char *discardLabel = localization.uiText("Discard Changes");
    const char *cancelLabel = localization.uiText("Cancel");
    const float saveWidth = std::max(180.0f, ImGui::CalcTextSize(saveLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f);
    const float discardWidth = std::max(160.0f, ImGui::CalcTextSize(discardLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f);
    const float cancelWidth = std::max(110.0f, ImGui::CalcTextSize(cancelLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f);
    const bool inlineActions = saveWidth + discardWidth + cancelWidth + ImGui::GetStyle().ItemSpacing.x * 2.0f <=
                               ImGui::GetContentRegionAvail().x;
    const ImVec2 saveButtonSize(inlineActions ? saveWidth : -1.0f, 0.0f);
    const ImVec2 discardButtonSize(inlineActions ? discardWidth : -1.0f, 0.0f);
    const ImVec2 cancelButtonSize(inlineActions ? cancelWidth : -1.0f, 0.0f);

    if (drawUiButton(saveLabel,
                     UiButtonRole::Primary,
                     saveButtonSize,
                     "Save the campaign, then complete the requested action."))
    {
        if (editor->saveCurrentCampaign(saveReason))
        {
            const EditorAppCommand command = pendingEditorAppCommand;
            pendingEditorAppCommand = EditorAppCommand::None;
            ImGui::CloseCurrentPopup();
            executeEditorAppCommand(command);
        }
    }
    if (inlineActions)
        ImGui::SameLine();
    if (drawUiButton(discardLabel,
                     UiButtonRole::Destructive,
                     discardButtonSize,
                     "Continue without saving these changes."))
    {
        const EditorAppCommand command = pendingEditorAppCommand;
        if(editor->discardUnsavedChanges("discard before " + discardReason)) {
            pendingEditorAppCommand = EditorAppCommand::None;
            ImGui::CloseCurrentPopup();
            executeEditorAppCommand(command);
        }
    }
    if (inlineActions)
        ImGui::SameLine();
    if (drawUiButton(cancelLabel,
                     UiButtonRole::Secondary,
                     cancelButtonSize,
                     "Keep working in this campaign."))
    {
        pendingEditorAppCommand = EditorAppCommand::None;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();
    ImGui::EndPopup();
}
