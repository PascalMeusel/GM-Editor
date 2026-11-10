#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardViewSupport.hpp"

bool LiveSessionDashboardView::drawTextRecovery()
{
    if (_controller._currentCampaign.presentationModeActive)
    {
        // Pending recovery is left untouched; allow the normal Live controls
        // to render so the GM can leave presentation mode.
        _controller.checkpointTextRecovery(true);
        return false;
    }
    const bool de = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    if (!_controller._textRecoveryError.empty()) ImGui::TextWrapped("%s", _controller._textRecoveryError.c_str());
    if (!_controller._textRecoveryPending) return false;
    ImGui::TextWrapped("%s", de ? "Unfertige Live-Notizen wurden gefunden. Stelle sie zur Prüfung wieder her oder verwerfe die Sicherung."
        : "Unfinished live notes were recovered. Restore them for review or discard the checkpoint.");
    ImGui::BeginDisabled(_controller._textRecoveredFields.empty());
    if (ImGui::Button(de ? "Textentwurf wiederherstellen" : "Restore text draft")) _controller.restoreTextRecovery();
    if (ImGui::Button(de ? "Wiederhergestellten Text kopieren" : "Copy recovered text"))
    {
        std::string text;
        for (const auto &[key, value] : _controller._textRecoveredFields)
            if (key.rfind("base-", 0) != 0) text += key + ": " + value + "\n\n";
        ImGui::SetClipboardText(text.c_str());
    }
    ImGui::EndDisabled();
    if (ImGui::Button(de ? "Wiederhergestellten Text verwerfen" : "Discard recovered text")) _controller.clearTextRecovery();
    return true;
}
