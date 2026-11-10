#include "Views/Editor/EditorWindowView.hpp"
#include "Controllers/Editor/EditorWindowController.hpp"
#include "Controllers/Editor/EditorDraftRecoveryController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include <algorithm>

namespace
{
const char *recoveryText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
        ? german : english;
}

void draftText(const char *english, const char *german, const std::string &value)
{
    ImGui::TextDisabled("%s", recoveryText(english, german));
    if (value.empty()) ImGui::TextDisabled("%s", recoveryText("(empty)", "(leer)"));
    else ImGui::TextWrapped("%s", value.c_str());
}

void drawDraftContents(const EditorDraftRecovery::Store &store, const Campaign &campaign)
{
    ImGui::TextWrapped(recoveryText("%zu item drafts; %d character note draft", "%zu Gegenstandsentwürfe; %d Figurennotizentwurf"),
                       store.items.size(), store.notes ? 1 : 0);
    for (const auto &[id, draft] : store.items)
    {
        ImGui::PushID(id.c_str());
        const std::string name = draft.item.name.empty()
            ? recoveryText("Unnamed item", "Unbenannter Gegenstand") : draft.item.name;
        const bool expanded = ImGui::TreeNodeEx("ItemDraft", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", name.c_str());
        showHelpTooltip(id.c_str());
        if (expanded)
        {
            ImGui::TextDisabled("%s", draft.base
                ? recoveryText("Changes to an existing item", "Änderungen an einem bestehenden Gegenstand")
                : recoveryText("New item; not yet saved", "Neuer Gegenstand; noch nicht gespeichert"));
            draftText("Description", "Beschreibung", draft.item.description);
            if (draft.powerDirty)
            {
                ImGui::SeparatorText(recoveryText("Unfinished item power", "Unfertige Gegenstandskraft"));
                draftText("Name", "Name", draft.power.name);
                draftText("Notes", "Notizen", draft.power.notes);
            }
            ImGui::TextWrapped("%s", recoveryText(
                "Restore to inspect all item and power fields in the item editor, or copy the complete draft data below.",
                "Stelle den Entwurf wieder her, um alle Gegenstands- und Kraftfelder im Editor zu prüfen, oder kopiere unten die vollständigen Entwurfsdaten."));
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (store.notes)
    {
        const auto found = std::find_if(campaign.characters.begin(), campaign.characters.end(), [&](const auto &character) {
            return character.metadata.id == store.notes->characterId;
        });
        const char *name = found == campaign.characters.end()
            ? recoveryText("Unavailable character", "Nicht verfügbare Figur")
            : found->nameBuffer[0] ? found->nameBuffer : recoveryText("Unnamed character", "Unbenannte Figur");
        const bool expanded = ImGui::TreeNodeEx("CharacterNotes", ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen,
            "%s: %s", recoveryText("Character notes", "Figurennotizen"), name);
        showHelpTooltip(store.notes->characterId.c_str());
        if (expanded)
        {
            draftText("Backstory", "Vorgeschichte", store.notes->backstory);
            draftText("Player notes", "Spielernotizen", store.notes->playerNotes);
            draftText("Private GM notes", "Private SL-Notizen", store.notes->gmNotes);
            ImGui::TreePop();
        }
    }
}
}

void EditorWindowView::renderEditorDraftRecoveryLauncher()
{
    auto &recovery = _controller.editorDraftRecovery();
    const bool privateMode = recovery.privateMode();
    const char *help = privateMode
        ? recoveryText("Return to GM mode to review private drafts.", "Wechsle zur SL-Ansicht, um private Entwürfe zu prüfen.")
        : recovery.pending()
            ? recoveryText("Saved private drafts need review. Restore, copy or discard them without changing saved campaign records.",
                           "Gesicherte private Entwürfe müssen geprüft werden. Wiederherstellen, Kopieren oder Verwerfen verändert keine gespeicherten Kampagneneinträge.")
            : recovery.cleanupPending()
                ? recoveryText("The campaign was saved, but draft checkpoint cleanup needs attention.",
                               "Die Kampagne wurde gespeichert, aber die Bereinigung der Entwurfssicherung muss geprüft werden.")
                : !recovery.error().empty()
                    ? recoveryText("Private draft recovery needs attention. Your current drafts remain available.",
                                   "Die private Entwurfssicherung muss geprüft werden. Aktuelle Entwürfe bleiben erhalten.")
                    : recoveryText("Review recovery for item drafts, unfinished powers and character notes.",
                                   "Prüfe die Sicherung von Gegenstandsentwürfen, unfertigen Kräften und Figurennotizen.");
    if (drawUiButton(recoveryText("Item & note drafts", "Gegenstands- und Notizentwürfe"),
                     recovery.attention() && !privateMode, help, UiButtonRole::Secondary, ImVec2(-1, 0), help))
    {
        recovery.open = true;
        _editorDraftRecoveryCopied = false;
    }
    if (!privateMode && recovery.attention())
    {
        const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(minimum.x, minimum.y + 2),
            ImVec2(minimum.x + 3, maximum.y - 2), ImGui::GetColorU32(WorkspaceUi::Amber));
    }
}

void EditorWindowView::renderEditorDraftRecovery()
{
    auto &recovery = _controller.editorDraftRecovery();
    // Gate before accessing any names, prose, checkpoint errors or selectors.
    if (recovery.privateMode()) { recovery.open = false; _editorDraftRecoveryCopied = false; return; }
    if (!recovery.open) { _editorDraftRecoveryCopied = false; return; }
    const auto *viewport = ImGui::GetMainViewport();
    const ImVec2 maximum(std::max(120.0f, viewport->WorkSize.x - 24),
                         std::max(180.0f, viewport->WorkSize.y - 24));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * .5f,
                                 viewport->WorkPos.y + viewport->WorkSize.y * .5f),
                          ImGuiCond_Appearing, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(440.0f, maximum.x), std::min(380.0f, maximum.y)), maximum);
    ImGui::SetNextWindowSize(ImVec2(std::min(720.0f, maximum.x), std::min(650.0f, maximum.y)), ImGuiCond_Appearing);
    const std::string title = std::string(recoveryText("Item and note draft recovery", "Gegenstands- und Notizentwürfe wiederherstellen")) + "###EditorDraftRecovery";
    if (!ImGui::Begin(title.c_str(), &recovery.open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }

    const bool pending = recovery.pending(), bound = recovery.bound();
    EditorDraftRecovery::Store current;
    if (!pending && bound) current = recovery.drafts();
    const auto &saved = recovery.recovered();
    const auto &contents = pending ? saved.drafts : current;
    const char *restoreLabel = recoveryText("Restore saved drafts", "Gesicherte Entwürfe wiederherstellen");
    const char *copyLabel = recoveryText("Copy complete draft data", "Vollständige Entwurfsdaten kopieren");
    const char *discardLabel = recoveryText("Discard these drafts", "Diese Entwürfe verwerfen");
    const char *retryLabel = recoveryText("Retry recovery", "Sicherung erneut versuchen");
    const char *closeLabel = recoveryText("Close review", "Prüfung schließen");
    const auto padding = ImGui::GetStyle().FramePadding;
    const float wrapWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - padding.x * 2);
    float footer = ImGui::GetStyle().ItemSpacing.y;
    for (const char *label : {restoreLabel, copyLabel, discardLabel, retryLabel, closeLabel})
        footer += std::max(ImGui::GetFrameHeight(), ImGui::CalcTextSize(label, nullptr, false, wrapWidth).y + padding.y * 2)
                  + ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("RecoveryDraftBody", ImVec2(0, std::max(32.0f, ImGui::GetContentRegionAvail().y - footer)), true);
    ImGui::TextWrapped("%s", recoveryText(
        "These are private unfinished edits. Restore loads them for review; it does not save them into the campaign. Use the item editor or Save Notes when ready.",
        "Dies sind private, unfertige Änderungen. Wiederherstellen lädt sie zur Prüfung, ohne sie in der Kampagne zu speichern. Nutze danach den Gegenstandseditor oder Notizen speichern."));
    if (!bound)
        ImGui::TextWrapped("%s", recoveryText("The campaign or storage location changed. Recovery actions are blocked to protect the original drafts.",
            "Kampagne oder Speicherort haben sich geändert. Wiederherstellungsaktionen sind zum Schutz der ursprünglichen Entwürfe gesperrt."));
    else if (pending && saved.status != EditorDraftRecovery::ReadStatus::Ready)
        ImGui::TextWrapped("%s", recoveryText("This checkpoint cannot be restored. Copy its complete data if available, retry reading it, or explicitly discard it.",
            "Diese Sicherung kann nicht wiederhergestellt werden. Kopiere verfügbare vollständige Daten, lies sie erneut oder verwirf sie ausdrücklich."));
    else if (!contents.empty()) drawDraftContents(contents, _controller.currentCampaign());
    else ImGui::TextWrapped("%s", recoveryText("No unfinished item or note edits are loaded.", "Keine unfertigen Gegenstands- oder Notizänderungen sind geladen."));
    if (recovery.cleanupPending())
        ImGui::TextWrapped("%s", recoveryText("The campaign save succeeded. Retry checkpoint cleanup; saving the campaign again is not required.",
            "Die Kampagne wurde erfolgreich gespeichert. Wiederhole die Bereinigung der Entwurfssicherung; erneutes Speichern der Kampagne ist nicht erforderlich."));
    if (bound && pending && !saved.error.empty()) ImGui::TextWrapped("%s", saved.error.c_str());
    if (!recovery.error().empty()) ImGui::TextWrapped("%s", recovery.error().c_str());
    if (_editorDraftRecoveryCopied) ImGui::TextWrapped("%s", recoveryText("Complete draft data copied to the clipboard.", "Vollständige Entwurfsdaten in die Zwischenablage kopiert."));
    ImGui::EndChild();

    const char *unavailable = recoveryText("This action is unavailable for the current recovery state.", "Diese Aktion ist im aktuellen Sicherungszustand nicht verfügbar.");
    if (drawUiButton(restoreLabel,
                     bound && pending && saved.status == EditorDraftRecovery::ReadStatus::Ready, unavailable,
                     UiButtonRole::Primary, ImVec2(-1, 0)))
    { _editorDraftRecoveryCopied = false; recovery.restore(); }
    if (drawUiButton(copyLabel,
                     bound && (pending ? saved.rawAvailable : !current.empty()), unavailable,
                     UiButtonRole::Secondary, ImVec2(-1, 0)))
    {
        const auto text = recovery.copy();
        if (!text.empty()) { ImGui::SetClipboardText(text.c_str()); _editorDraftRecoveryCopied = true; }
    }
    if (drawUiButton(discardLabel,
                     bound && (pending || !current.empty() || recovery.cleanupPending()), unavailable,
                     UiButtonRole::Destructive, ImVec2(-1, 0)))
    { _editorDraftRecoveryCopied = false; if (recovery.discard(true)) recovery.open = false; }
    if (drawUiButton(retryLabel,
                     bound && recovery.attention(), unavailable, UiButtonRole::Secondary, ImVec2(-1, 0)))
    { _editorDraftRecoveryCopied = false; if (recovery.pending()) recovery.retryRead(); else recovery.checkpoint(true); }
    if (drawUiButton(closeLabel, UiButtonRole::Secondary, ImVec2(-1, 0))) recovery.open = false;
    ImGui::End();
}
