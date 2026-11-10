#include "Views/Editor/EditorPresentationSupport.hpp"
#include "Views/Editor/Presentation/PresentationReviewUi.hpp"
#include "Views/Editor/Presentation/PlayerReviewContent.hpp"

void EditorWindowView::preparePresentationPlayerReview(PresentationReviewAction action)
{
    _presentationReviewAction = action;
    _presentationReviewError.clear();
    _controller._workspace.playerReviewError.clear();
    _playerReviewSession.requestPreparation(_controller.currentCampaign(), PlayerDisplayReview::currentLocaleCode(),
        _playerReviewContextEpoch);
    if(!_playerReviewSession.preparing())updatePresentationPlayerReviewError();
}

void EditorWindowView::pollPresentationPlayerReview()
{
    if(_presentationReviewAction==PresentationReviewAction::None)return;
    const bool wasPreparing=_playerReviewSession.preparing();
    const bool adopted=_playerReviewSession.pollPreparation(_controller.currentCampaign(),
        PlayerDisplayReview::currentLocaleCode(),_playerReviewContextEpoch);
    if(adopted||(wasPreparing&&!_playerReviewSession.preparing()))updatePresentationPlayerReviewError();
}

void EditorWindowView::updatePresentationPlayerReviewError()
{
    const auto &_presentationReview = _playerReviewSession.review();
    _presentationReviewError.clear();
    if (_playerReviewSession.prepared())
        return;
    if(_playerReviewSession.foregroundRequired()) {
        _presentationReviewError=uiTextForLocale(
            "Background preparation could not finish. Prepare now if you want to continue; this may briefly pause the editor.",
            "Die Vorbereitung im Hintergrund konnte nicht abgeschlossen werden. Du kannst jetzt vorbereiten; dabei kann der Editor kurz pausieren.");
        return;
    }
    if (_presentationReview.eligible) {
        _presentationReviewError = uiTextForLocale(
            "The review could not be refreshed. Check the staged content and files; the earlier preview is retained.",
            "Die Vorschau konnte nicht aktualisiert werden. Prüfe Inhalte und Dateien; die frühere Vorschau bleibt erhalten.");
        return;
    }

    switch (_presentationReview.issue)
    {
    case PlayerDisplayReviewIssue::MapOnlyNeedsPublicMap:
        _presentationReviewError =
            localizedUiLabel("Map-only display needs a valid public map before it can be reviewed.");
        break;
    case PlayerDisplayReviewIssue::MapUnavailable:
        _presentationReviewError =
            localizedUiLabel("The selected map is missing, private, or no longer matches the staged player view.");
        break;
    case PlayerDisplayReviewIssue::SceneUnavailable:
        _presentationReviewError =
            localizedUiLabel("The selected public scene no longer exists. Choose another scene and review again.");
        break;
    case PlayerDisplayReviewIssue::InitiativeUnavailable:
        _presentationReviewError = localizedUiLabel(
            "The selected initiative source no longer exists. Choose another encounter and review again.");
        break;
    case PlayerDisplayReviewIssue::NpcUnavailable:
        _presentationReviewError =
            localizedUiLabel("A revealed NPC no longer exists. Update the NPC reveals and review again.");
        break;
    case PlayerDisplayReviewIssue::PartyUnavailable:
        _presentationReviewError = uiTextForLocale(
            "A selected party member is unavailable. Update the party selection and review again.",
            "Ein gewähltes Gruppenmitglied ist nicht verfügbar. Passe die Gruppenauswahl an und prüfe erneut.");
        break;
    case PlayerDisplayReviewIssue::HandoutUnavailable:
        _presentationReviewError = localizedUiLabel(
            "A public handout or its managed file is missing or changed. Repair its asset and review again.");
        break;
    case PlayerDisplayReviewIssue::NoPublicPayload:
    default:
        _presentationReviewError = localizedUiLabel("Choose at least one valid public source before continuing.");
        break;
    }
}

void EditorWindowView::renderPresentationPlayerReviewModal(const char *popupId)
{
    // The comparison contains secrets and must always stay on the GM viewport.
    const ImGuiViewport *gmViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowViewport(gmViewport->ID);
    ImGui::SetNextWindowPos(ImVec2(gmViewport->WorkPos.x + gmViewport->WorkSize.x * 0.5f,
                                   gmViewport->WorkPos.y + gmViewport->WorkSize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    setResponsiveWindowSize(ImVec2(960.0f, 680.0f), ImVec2(440.0f, 360.0f));
    if (!ImGui::BeginPopupModal(localizedUiLabel(popupId).c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;
    const gm::presentation_review_ui::FooterFocusTransfer footerFocus;

    if (ImGui::IsWindowAppearing())
    {
        // The detached player viewport can be above the GM host on one screen.
        // Focus the host once when review opens; keep the player image intact.
        ImGuiPlatformIO &platform = ImGui::GetPlatformIO();
        if (platform.Platform_SetWindowFocus != nullptr)
            platform.Platform_SetWindowFocus(ImGui::GetMainViewport());
    }

    const Campaign &campaign = _controller.currentCampaign();
    if (campaign.presentationModeActive || _presentationReviewAction == PresentationReviewAction::None)
    { _playerReviewSession.clear(); ImGui::CloseCurrentPopup(); ImGui::EndPopup(); return; }
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        _presentationReviewAction=PresentationReviewAction::None;_playerReviewSession.clear();
        ImGui::CloseCurrentPopup();ImGui::EndPopup();return;
    }
    const auto &_presentationReview = _playerReviewSession.review();
    const bool reviewCurrent = _playerReviewSession.knownCurrent(campaign, PlayerDisplayReview::currentLocaleCode());
    const bool preparing=_playerReviewSession.preparing();
    if (drawUiButton(uiTextForLocale("Refresh review", "Vorschau aktualisieren"), !preparing,
        uiTextForLocale("Preparation is in progress. Cancel to close the review.", "Die Vorbereitung läuft. Mit Abbrechen schließt du die Vorschau."), UiButtonRole::Secondary))
    { footerFocus.rememberKeyboardActivation(); preparePresentationPlayerReview(_presentationReviewAction); ImGui::EndPopup(); return; }
    if(_playerReviewSession.foregroundRequired()&&drawUiButton(uiTextForLocale(
        "Prepare now (may pause)","Jetzt vorbereiten (kann pausieren)"),UiButtonRole::Secondary))
    {
        footerFocus.rememberKeyboardActivation();
        _playerReviewSession.prepareInForeground(campaign,PlayerDisplayReview::currentLocaleCode(),_playerReviewContextEpoch);
        updatePresentationPlayerReviewError();ImGui::EndPopup();return;
    }
    const char *confirmLabel = _presentationReviewAction == PresentationReviewAction::Arm
        ? uiTextForLocale("Open player window", "Spielerfenster öffnen") : localizedUiText("Show to players");
    const char *armExplanation=_presentationReviewAction==PresentationReviewAction::Arm?uiTextForLocale(
        "The window opens black. Choose Show to players when everyone is ready.",
        "Das Fenster öffnet sich schwarz. Wähle Zeigen, sobald alle bereit sind."):nullptr;
    const auto footer=gm::presentation_review_ui::measureFooter(confirmLabel,localizedUiText("Cancel"),armExplanation);
    ImGui::BeginChild("PresentationReviewBody",
        ImVec2(0.0f,gm::presentation_review_ui::reviewBodyHeight(footer)),ImGuiChildFlags_NavFlattened);
    ImGui::TextWrapped("%s", uiTextForLocale(
        "Check the map and text below before opening the player window or sharing an update.",
        "Prüfe die Karte und Texte unten, bevor du das Spielerfenster öffnest oder eine Änderung zeigst."));
    if(preparing)
        ImGui::TextWrapped("%s",_presentationReview.eligible?uiTextForLocale(
            "Preparing an updated review. The previous preview is shown below; it cannot be confirmed.",
            "Die aktualisierte Vorschau wird vorbereitet. Unten bleibt die frühere Vorschau sichtbar; sie kann nicht bestätigt werden."):
            uiTextForLocale("Preparing the review… You can cancel without changing the player display.",
                "Die Vorschau wird vorbereitet… Du kannst abbrechen, ohne die Spieleranzeige zu ändern."));
    else if (!reviewCurrent)
    {
        const std::string message =
            _presentationReviewError.empty()
                ? uiTextForLocale(
                      "This preview shows the earlier review. Content changed; refresh it before sharing.",
                      "Diese Vorschau zeigt die frühere Prüfung. Inhalte wurden geändert; aktualisiere sie vor dem Teilen.")
                : _presentationReviewError;
        gm::presentation_review_ui::drawWrappedWarning(message);
        if (!_playerReviewSession.foregroundRequired()&&_presentationReview.issue == PlayerDisplayReviewIssue::MapOnlyNeedsPublicMap)
        {
            ImGui::TextWrapped("%s", uiTextForLocale(
                "This scene has no map selected. Use the scene layout to share its text, or go back and choose a map.",
                "Für diese Szene ist keine Karte gewählt. Nutze die Szenenansicht für ihre Texte oder wähle zuerst eine Karte."));
            if (drawUiButton(uiTextForLocale("Use scene layout", "Szenenansicht verwenden"), UiButtonRole::Secondary))
            {
                footerFocus.rememberKeyboardActivation();
                _controller.setLocalPlayerDisplayMapOnly(false);
                preparePresentationPlayerReview(_presentationReviewAction);
                ImGui::EndChild(); ImGui::EndPopup(); return;
            }
        }
        else if (!_playerReviewSession.foregroundRequired()&&_presentationReview.issue == PlayerDisplayReviewIssue::MapUnavailable)
        {
            ImGui::TextWrapped("%s", uiTextForLocale(
                "Map and Shared Art both need a public map. Continue without them, or choose a public map and review again.",
                "Karte und Kartenbild benötigen beide eine öffentliche Karte. Fahre ohne sie fort oder wähle eine öffentliche Karte und prüfe erneut."));
            if (drawUiButton(uiTextForLocale("Continue without map or artwork", "Ohne Karte oder Kartenbild fortfahren"),
                             UiButtonRole::Secondary))
            {
                footerFocus.rememberKeyboardActivation();
                // These setters black out the display and persist the staged
                // selection; they do not arm or publish the player window.
                _controller.setLocalPlayerDisplayMapOnly(false);
                _controller.setShowPublicMap(false);
                _controller.setShowSharedArt(false);
                preparePresentationPlayerReview(_presentationReviewAction);
                ImGui::EndChild(); ImGui::EndPopup(); return;
            }
        }
    }

    if (reviewCurrent && !_presentationReviewError.empty())
        gm::presentation_review_ui::drawWrappedWarning(_presentationReviewError);
    gm::player_review_ui::drawMaps(_playerReviewSession);
    gm::player_review_ui::drawContent(_playerReviewSession);
    ImGui::EndChild();

    if(armExplanation)ImGui::TextWrapped("%s",armExplanation);
    if (drawUiButton(confirmLabel, reviewCurrent,
                     "The review is incomplete or stale. Close it and review the staged content again.",
                     UiButtonRole::Primary, footer.confirm,
                     "Apply only the player-safe state shown in this review."))
    {
        PlayerDisplayReview::Measure confirmation(_playerReviewSession.measurements(),
            PlayerDisplayReview::Stage::UiConfirmationTotal);
        bool applied = false;
        if (_presentationReviewAction == PresentationReviewAction::Arm)
        {
            const LocalPlayerDisplayModel restore = campaign.localPlayerDisplay;
            applied = _controller.armLocalPlayerDisplayFromReview(_presentationReview, _playerReviewSession.measurements());
            if (applied)
            {
                _presentationRestoreSnapshot = restore;
                _hasPresentationRestoreSnapshot = true;
            }
        }
        else if (_presentationReviewAction == PresentationReviewAction::Publish)
        {
            applied = _controller.publishLocalPlayerDisplay(_presentationReview, _playerReviewSession.measurements());
        }
        if (applied)
        {
            _presentationReviewAction = PresentationReviewAction::None;
            _playerReviewSession.clear();
            ImGui::CloseCurrentPopup();
        }
        else
        {
            _presentationReviewError = _controller.shortcutStatusMessage();
        }
    }
    gm::presentation_review_ui::nextFooterButton(footer);
    footerFocus.focusCancel();
    if (drawUiButton("Cancel", UiButtonRole::Secondary, footer.cancel,
                     "Close this review without changing the player display."))
    {
        _presentationReviewAction = PresentationReviewAction::None;
        _playerReviewSession.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
