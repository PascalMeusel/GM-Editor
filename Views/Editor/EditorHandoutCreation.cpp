#include "Models/Project/NativeAssetPicker.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include "Views/Editor/EditorViewDetails.hpp"

using namespace gm::editor_view_detail;

bool EditorWindowView::saveHandoutCreationDraft()
{
    if (!_handoutCreationDirty)
        return true;
    if (_handoutCreationDraft.title.find_first_not_of(" \t\r\n") == std::string::npos)
    {
        _handoutCreationError = uiTextForLocale("A handout title is required.", "Ein Handout-Titel ist erforderlich.");
        _createHandoutRequested = true;
        return false;
    }
    const auto authored = _handoutCreationDraft;
    // Saving one Compendium draft must not commit independent World drafts.
    const auto result = WorldInspector::applyWithRollback(
        _controller._currentCampaign,
        [authored](Campaign &campaign) {
            campaign.handouts.push_back(authored);
            return WorldInspector::MutationResult{WorldInspector::MutationStatus::Applied, "", authored.metadata.id};
        },
        [this](Campaign &) { return _controller.persistCurrentCampaign("create hidden handout"); });
    if (!result.applied())
    {
        _handoutCreationError = result.message;
        _createHandoutRequested = true;
        return false;
    }
    _controller.markSearchIndexDirty();
    _handoutCreationDraft = HandoutModel{};
    _handoutCreationDirty = false;
    _handoutCreationError.clear();
    return true;
}

void EditorWindowView::renderHandoutCreation()
{
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto text = [&](const char *en, const char *de) { return german ? de : en; };
    const char *title = text("Create handout###HandoutCreation", "Handout erstellen###HandoutCreation");
    if (_createHandoutRequested)
    {
        ImGui::OpenPopup(title);
        _createHandoutRequested = false;
    }
    if (!beginBoundedConfirmationModal(title))
        return;
    DraftEditSignal::Scope edits([this] { _handoutCreationDirty = true; });
    auto &draft = _handoutCreationDraft;
    FormUi::InputText(text("Title", "Titel"), &draft.title);
    TaskUi::fieldLabel(text("Player text", "Spielertext"));
    FormUi::InputTextMultiline("##HandoutBody", &draft.body, ImVec2(-1, 110));
    if (ImGui::Button(text("Browse attachment", "Anhang auswählen")))
    {
        const auto chosen = gm::assets::chooseImage(_handoutCreationError, true);
        if (chosen)
        {
            ProjectAssetImport::AssetImportRequest request;
            request.kind = ProjectAssetImport::AssetKind::Handout;
            request.sourcePath = *chosen;
            request.assetId = draft.metadata.id;
            request.displayName = draft.title;
            const auto imported = ProjectAssetImport::importAsset(_controller.currentCampaign(), request);
            if (imported.success)
            {
                draft.filePath = imported.projectPath;
                _handoutCreationDirty = true;
                _handoutCreationError.clear();
            }
            else
                _handoutCreationError = imported.errorMessage;
        }
    }
    if (!draft.filePath.empty())
    {
        ImGui::TextWrapped("%s", std::filesystem::path(draft.filePath).filename().string().c_str());
        if (ImGui::SmallButton(text("Remove attachment", "Anhang entfernen")))
        {
            draft.filePath.clear();
            _handoutCreationDirty = true;
        }
    }
    if (ImGui::TreeNode(text("Private GM notes", "Private SL-Notizen")))
    {
        FormUi::InputTextMultiline("##GMNotes", &draft.gmNotes, ImVec2(-1, 70));
        ImGui::TreePop();
    }
    ImGui::SeparatorText(text("Player preview · Hidden", "Spielervorschau · Verborgen"));
    ImGui::TextWrapped("%s", draft.title.c_str());
    ImGui::TextWrapped("%s", draft.body.c_str());
    ImGui::TextWrapped("%s",
                       text("Save prepares this handout. Review and reveal it separately in Player Display.",
                            "Speichern bereitet das Handout vor. Separat in der Spieleranzeige prüfen und freigeben."));
    if (!_handoutCreationError.empty())
        ImGui::TextWrapped("%s", _handoutCreationError.c_str());
    const bool named = draft.title.find_first_not_of(" \t\r\n") != std::string::npos;
    ImGui::BeginDisabled(!named);
    if (drawUiButton(text("Save hidden handout", "Verborgenes Handout speichern"), UiButtonRole::Primary))
    {
        _handoutCreationDirty = true;
        if (saveHandoutCreationDraft())
            ImGui::CloseCurrentPopup();
        else
            _createHandoutRequested = false; // Keep this modal and its focus in place for retry.
    }
    ImGui::EndDisabled();
    TaskUi::sameLineIfFits();
    if (ImGui::Button(text("Cancel", "Abbrechen")))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
