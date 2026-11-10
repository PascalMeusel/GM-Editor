#include "Views/Editor/Widgets/Session/GMScreen/GMScreenViewSupport.hpp"
#include "Views/Editor/Presentation/PresentationReviewUi.hpp"
#include "Views/Editor/Presentation/PlayerReviewContent.hpp"
#include "Models/Session/PlayerDisplayReviewMapPolicy.hpp"

void GMScreenView::preparePublicDisplayReview(PublicDisplayReviewAction action)
{
    _publicDisplayReviewAction=action;_reviewError.clear();
    if(!_playerReviewSession.requestPreparation(_controller._currentCampaign,PlayerDisplayReview::currentLocaleCode(),_playerReviewContextEpoch))
        _reviewError=uiTextForLocale("The staged content or its files could not be reviewed. Repair the selection and refresh.",
            "Die Inhalte oder ihre Dateien konnten nicht geprüft werden. Korrigiere die Auswahl und aktualisiere die Vorschau.");
}

void GMScreenView::pollPlayerReview()
{
    if(_controller._currentCampaign.presentationModeActive||!isVisible()) {resetPlayerReview();return;}
    if(_publicDisplayReviewAction==PublicDisplayReviewAction::None)return;
    const bool wasPreparing=_playerReviewSession.preparing();
    const bool adopted=_playerReviewSession.pollPreparation(_controller._currentCampaign,PlayerDisplayReview::currentLocaleCode(),_playerReviewContextEpoch);
    if(adopted)_reviewError.clear();
    else if(wasPreparing&&!_playerReviewSession.preparing())_reviewError=uiTextForLocale(
        "The review was not updated. Refresh changed content, or use Prepare now if background preparation is unavailable.",
        "Die Vorschau wurde nicht aktualisiert. Aktualisiere geänderte Inhalte oder nutze Jetzt vorbereiten, falls die Vorbereitung im Hintergrund nicht verfügbar ist.");
}

void GMScreenView::finishPlayerReviewFrame()
{
    // Closing, collapsing, or leaving the Public Display tab must not leave a
    // background request capable of restoring a review on a later visit.
    if(_publicDisplayReviewAction!=PublicDisplayReviewAction::None&&
        (!isVisible()||_playerReviewDrawFrame!=ImGui::GetFrameCount()))resetPlayerReview();
}

bool GMScreenView::rejectUnreadyPlayerReview(const PlayerDisplayReviewSnapshot &review)
{
    if(&review!=&_playerReviewSession.review())return false;
    if(!_controller._currentCampaign.presentationModeActive&&
        _playerReviewSession.knownCurrent(_controller._currentCampaign,PlayerDisplayReview::currentLocaleCode()))return false;
    _reviewError=uiTextForLocale("This review is not ready. Wait for preparation or refresh it before confirming.",
        "Diese Vorschau ist nicht bereit. Warte auf die Vorbereitung oder aktualisiere sie vor dem Bestätigen.");
    return true;
}

void GMScreenView::drawPublicDisplayReviewModal(const char *popupId)
{
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    setResponsiveWindowSize(ImVec2(960,680),ImVec2(440,360));
    if(!ImGui::BeginPopupModal(localizedUiLabel(popupId).c_str(),nullptr,ImGuiWindowFlags_NoSavedSettings))return;
    const gm::presentation_review_ui::FooterFocusTransfer footerFocus;
    if(_controller._currentCampaign.presentationModeActive||_publicDisplayReviewAction==PublicDisplayReviewAction::None)
    {resetPlayerReview();ImGui::CloseCurrentPopup();ImGui::EndPopup();return;}
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Escape))
    {resetPlayerReview();ImGui::CloseCurrentPopup();ImGui::EndPopup();return;}
    const auto &review=_playerReviewSession.review();
    const bool current=_playerReviewSession.knownCurrent(_controller._currentCampaign,PlayerDisplayReview::currentLocaleCode());
    const bool preparing=_playerReviewSession.preparing();
    if(drawUiButton(uiTextForLocale("Refresh review","Vorschau aktualisieren"),!preparing,
        uiTextForLocale("Preparation is in progress. Cancel to close the review.","Die Vorbereitung läuft. Mit Abbrechen schließt du die Vorschau."),UiButtonRole::Secondary))
    {footerFocus.rememberKeyboardActivation();preparePublicDisplayReview(_publicDisplayReviewAction);ImGui::EndPopup();return;}
    if(_playerReviewSession.foregroundRequired()&&drawUiButton(uiTextForLocale(
        "Prepare now (may pause)","Jetzt vorbereiten (kann pausieren)"),UiButtonRole::Secondary))
    {
        footerFocus.rememberKeyboardActivation();
        const bool ready=_playerReviewSession.prepareInForeground(_controller._currentCampaign,
            PlayerDisplayReview::currentLocaleCode(),_playerReviewContextEpoch);
        _reviewError=ready?std::string{}:uiTextForLocale("The staged content or its files could not be reviewed. Repair the selection and refresh.",
            "Die Inhalte oder ihre Dateien konnten nicht geprüft werden. Korrigiere die Auswahl und aktualisiere die Vorschau.");
        ImGui::EndPopup();return;
    }
    const char *confirm=_publicDisplayReviewAction==PublicDisplayReviewAction::Arm?
        uiTextForLocale("Open player window","Spielerfenster öffnen"):localizedUiText("Show to players");
    const auto footer=gm::presentation_review_ui::measureFooter(localizedUiText(confirm),localizedUiText("Cancel"));
    ImGui::BeginChild("PlayerDisplayReviewBody",ImVec2(0,gm::presentation_review_ui::reviewBodyHeight(footer)),ImGuiChildFlags_NavFlattened);
    ImGui::TextWrapped("%s",localizedUiText("Compare the complete GM view with exactly what the local player display will receive."));
    if(preparing)ImGui::TextWrapped("%s",review.eligible?uiTextForLocale(
        "Preparing an updated review. The previous preview is shown below; it cannot be confirmed.",
        "Die aktualisierte Vorschau wird vorbereitet. Unten bleibt die frühere Vorschau sichtbar; sie kann nicht bestätigt werden."):
        uiTextForLocale("Preparing the review… You can cancel without changing the player display.",
            "Die Vorschau wird vorbereitet… Du kannst abbrechen, ohne die Spieleranzeige zu ändern."));
    else if(!current)
    {
        gm::presentation_review_ui::drawWrappedWarning(_reviewError.empty()?uiTextForLocale(
            "This preview shows the earlier review. Content changed; refresh it before sharing.",
            "Diese Vorschau zeigt die frühere Prüfung. Inhalte wurden geändert; aktualisiere sie vor dem Teilen."):_reviewError);
        if(!_playerReviewSession.foregroundRequired()&&
            (review.issue==PlayerDisplayReviewIssue::MapUnavailable||review.issue==PlayerDisplayReviewIssue::MapOnlyNeedsPublicMap))
        {
            if(drawUiButton(uiTextForLocale("Continue without map or artwork","Ohne Karte oder Kartenbild fortfahren"),UiButtonRole::Secondary))
            {
                footerFocus.rememberKeyboardActivation();
                auto &display=_controller._currentCampaign.localPlayerDisplay;
                display.mapOnly=false;display.showMap=false;display.showSharedArt=false;
                ::blackoutLocalPlayerDisplay(display);_controller.touchLocalPlayerDisplay();
                preparePublicDisplayReview(_publicDisplayReviewAction);
                ImGui::EndChild();ImGui::EndPopup();return;
            }
        }
    }
    if(!_reviewError.empty()&&current)gm::presentation_review_ui::drawWrappedWarning(_reviewError);
    gm::player_review_ui::drawMaps(_playerReviewSession);
    gm::player_review_ui::drawContent(_playerReviewSession);
    ImGui::EndChild();
    if(drawUiButton(confirm,current,"The review is incomplete or stale. Close it and review the staged content again.",
        UiButtonRole::Primary,footer.confirm,"Publish only the immutable player-safe state shown in this review."))
    {
        const auto restore=_controller._currentCampaign.localPlayerDisplay;
        const bool applied=_publicDisplayReviewAction==PublicDisplayReviewAction::Arm?
            _controller.armLocalPlayerDisplayFromReview(review):_controller.publishLocalPlayerDisplay(review);
        if(applied)
        {
            if(_publicDisplayReviewAction==PublicDisplayReviewAction::Arm)
            {_publicDisplayRestoreSnapshot=restore;_hasPublicDisplayRestoreSnapshot=true;}
            resetPlayerReview();ImGui::CloseCurrentPopup();
        }
        else _reviewError=localizedUiLabel("The staged content changed. Nothing was shown to players.");
    }
    gm::presentation_review_ui::nextFooterButton(footer);
    footerFocus.focusCancel();
    if(drawUiButton("Cancel",UiButtonRole::Secondary,footer.cancel,"Close this review without changing the player display."))
    {resetPlayerReview();ImGui::CloseCurrentPopup();}
    ImGui::EndPopup();
}
