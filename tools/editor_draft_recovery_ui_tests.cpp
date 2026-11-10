#include "editor_draft_recovery_fixture.hpp"
#include "Controllers/Editor/EditorWindowController.hpp"
#include "Controllers/Editor/EditorDraftRecoveryController.hpp"
#include "Views/Editor/EditorWindowView.hpp"
#include "Views/Editor/Widgets/UiInteractionTrace.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "imgui_internal.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

struct EditorDraftRecoveryUiProbe
{
    static void renderRecovery(EditorWindowController &editor)
    {
        editor.view->renderEditorDraftRecoveryLauncher();
        editor.view->renderEditorDraftRecovery();
    }
    static void privateMode(EditorWindowController &editor, bool value)
    { editor._currentCampaign.presentationModeActive = value; }
};

namespace
{
constexpr const char *ItemCanary = "PRIVATE_ITEM_DRAFT_217";
constexpr const char *NotesCanary = "PRIVATE_CHARACTER_DRAFT_389";
void require(bool value, const std::string &message)
{ if (!value) throw std::runtime_error(message); }
const char *label(bool de, const char *en, const char *german) { return de ? german : en; }
Campaign readCampaign(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns()) if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Synthetic recovery campaign is missing");
}
void canonicalUnchanged(const Campaign &original)
{
    const auto actual = readCampaign(original.metadata.id);
    require(actual.customItems.empty() && actual.characters.size() == 1 &&
            actual.characters.front().metadata.id == original.characters.front().metadata.id &&
            actual.characters.front().gmNotes == "Saved notes remain canonical",
            "A recovery UI action committed private drafts or changed canonical identity");
}
std::filesystem::path checkpointPath(const Campaign &campaign)
{
    return ProjectPackageLayout::layoutForCampaign(campaign).recoveryDirectory / "Drafts" /
        (std::string(EditorDraftRecovery::Key) + ".draft");
}

// CPU ImGui only. Real Editor services are idle: no Application, native window,
// rendering backend, image assets, audio assets/playback or OS clipboard access.
class RecoveryUi
{
    ImGuiContext *context_ = ImGui::CreateContext();
    std::unique_ptr<UiInteractionTrace> trace_;
public:
    std::function<void()> draw;
    std::string submitted, clipboard;
    explicit RecoveryUi(bool de, bool compact)
    {
        gm::localization::LocalizationService::instance().setPreference(de ?
            gm::localization::LocalePreference::German : gm::localization::LocalePreference::English, false);
        auto &io = ImGui::GetIO(); io.IniFilename = nullptr; io.DeltaTime = 1.f / 60;
        io.DisplaySize = compact ? ImVec2(1280, 720) : ImVec2(1600, 1000);
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImFontConfig font; font.SizePixels = 18.f; io.Fonts->AddFontDefault(&font);
        ImGui::GetStyle().FontScaleMain = compact ? 1.5f : 1.f;
        auto &platform = ImGui::GetPlatformIO(); platform.Platform_ClipboardUserData = &clipboard;
        platform.Platform_GetClipboardTextFn = [](ImGuiContext *) { return ""; };
        platform.Platform_SetClipboardTextFn = [](ImGuiContext *, const char *text) {
            *static_cast<std::string *>(ImGui::GetPlatformIO().Platform_ClipboardUserData) = text;
        };
        platform.Platform_SetImeDataFn = nullptr; platform.Platform_OpenInShellFn = nullptr;
        unsigned char *pixels = nullptr; int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        require(pixels && width > 0 && height > 0, "Synthetic font atlas did not build");
        trace_ = std::make_unique<UiInteractionTrace>(context_);
    }
    ~RecoveryUi() { trace_.reset(); ImGui::DestroyContext(context_); }
    void frame(int count = 1)
    {
        while (count--)
        {
            trace_->beginFrame(); ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Recovery UI fixture", nullptr,
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
            ImGui::LogToBuffer(); draw(); submitted = context_->LogBuffer.c_str();
            ImGui::LogFinish(); ImGui::End(); ImGui::Render();
        }
    }
    UiInteractionTrace::Item item(const char *caption) const
    {
        auto found = trace_->find(caption);
        found.erase(std::remove_if(found.begin(), found.end(), [](const auto &value) {
            const auto *owner = ImGui::FindWindowByName(value.window.c_str());
            return owner && owner->ID == value.id;
        }), found.end());
        if (found.size() != 1)
        {
            std::ostringstream message; message << "Expected unique recovery control: " << caption << " count=" << found.size();
            for (const auto &[id, value] : trace_->items()) if (!value.label.empty())
                message << "\n" << id << " '" << value.label << "' owner=" << value.window;
            throw std::runtime_error(message.str());
        }
        return found.front();
    }
    void visible(const UiInteractionTrace::Item &value) const
    {
        const auto *owner = ImGui::FindWindowByName(value.window.c_str());
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (!owner || value.minimum.x < owner->InnerClipRect.Min.x - 1 || value.minimum.y < owner->InnerClipRect.Min.y - 1 ||
            value.maximum.x > owner->InnerClipRect.Max.x + 1 || value.maximum.y > owner->InnerClipRect.Max.y + 1 ||
            value.minimum.x < 0 || value.minimum.y < 0 || value.maximum.x > display.x || value.maximum.y > display.y)
        {
            std::ostringstream message; message << "Recovery control is clipped: " << value.label << " owner=" << value.window
                << " rect=" << value.minimum.x << ',' << value.minimum.y << '-' << value.maximum.x << ',' << value.maximum.y;
            if (owner) message << " clip=" << owner->InnerClipRect.Min.x << ',' << owner->InnerClipRect.Min.y << '-'
                << owner->InnerClipRect.Max.x << ',' << owner->InnerClipRect.Max.y << " scroll=" << owner->Scroll.y;
            throw std::runtime_error(message.str());
        }
    }
    void click(const char *caption)
    {
        frame(3); ImGuiID id = 0;
        for (int i = 0; i < 3; ++i)
        {
            const auto value = item(caption); visible(value);
            require(!id || id == value.id, "Recovery control identity changed while hovering"); id = value.id;
            ImGui::GetIO().AddMousePosEvent((value.minimum.x + value.maximum.x) * .5f,
                                          (value.minimum.y + value.maximum.y) * .5f); frame();
        }
        require(context_->HoveredId == id && !context_->HoveredIdIsDisabled, "Recovery control is not the observed enabled hit target");
        ImGui::GetIO().AddMouseButtonEvent(0, true); frame();
        ImGui::GetIO().AddMouseButtonEvent(0, false); frame(3);
    }
    void key(ImGuiKey value)
    { ImGui::GetIO().AddKeyEvent(value, true); frame(); ImGui::GetIO().AddKeyEvent(value, false); frame(2); }
    void keyboardActivate(const char *caption)
    {
        const auto target = item(caption); visible(target);
        for (int i = 0; i < 24 && context_->NavId != target.id; ++i) key(ImGuiKey_Tab);
        require(context_->NavId == target.id, "Keyboard navigation did not reach the real recovery control");
        key(ImGuiKey_Space);
    }
    void disabled(const char *caption)
    {
        const auto target = item(caption); visible(target);
        ImGui::GetIO().AddMousePosEvent((target.minimum.x + target.maximum.x) * .5f,
                                      (target.minimum.y + target.maximum.y) * .5f); frame(3);
        require(context_->HoveredId == target.id && context_->HoveredIdIsDisabled,
                "Invalid checkpoint Restore was not the exact disabled production control");
    }
    void footer(bool de)
    {
        for (const auto *caption : {label(de, "Restore saved drafts", "Gesicherte Entwürfe wiederherstellen"),
            label(de, "Copy complete draft data", "Vollständige Entwurfsdaten kopieren"),
            label(de, "Discard these drafts", "Diese Entwürfe verwerfen"),
            label(de, "Retry recovery", "Sicherung erneut versuchen"), label(de, "Close review", "Prüfung schließen")})
            visible(item(caption));
    }
};

void recoveryCase(bool de, bool compact)
{
    std::cout << "Editor recovery UI: " << (de ? "DE" : "EN") << (compact ? " compact150" : " normal") << '\n';
    RecoveryUi ui(de, compact);
    Campaign campaign; campaign.name = "Synthetic item note recovery " + campaign.metadata.id;
    campaign.ruleSet = std::make_shared<DND5e>(); CharacterModel character;
    std::snprintf(character.nameBuffer, sizeof(character.nameBuffer), "%s", "Synthetic character");
    character.gmNotes = "Saved notes remain canonical"; campaign.characters.push_back(character);
    require(ProjectSaveService::getInstance().saveCampaign(campaign, "synthetic recovery UI baseline"), "Cannot save recovery UI baseline");
    EditorDraftRecovery::Store store; EditorDraftRecovery::ItemDraft item;
    item.item.name = ItemCanary; item.item.description = "Unfinished private description";
    item.powerDirty = true; item.power.notes = "UNFINISHED_POWER_CANARY";
    store.items[item.item.metadata.id] = item;
    store.notes = EditorDraftRecovery::Notes{character.metadata.id, "Backstory draft", "Player note draft",
        std::string(NotesCanary) + std::string(6000, 'x'),
        character.backstory, character.playerNotes, character.gmNotes};
    require(EditorDraftRecovery::save(campaign, store).empty(), "Cannot seed private recovery checkpoint");
    const auto encoded = EditorDraftRecovery::encode(campaign.metadata.id, store).payload;
    const auto launcher = label(de, "Item & note drafts", "Gegenstands- und Notizentwürfe");
    const auto discard = label(de, "Discard these drafts", "Diese Entwürfe verwerfen");
    {
        EditorWindowController editor; require(editor.setCurrentCampaign(campaign), "Cannot open checkpoint campaign");
        ui.draw = [&] { EditorDraftRecoveryUiProbe::renderRecovery(editor); }; ui.frame(4);
        require(editor.editorDraftRecovery().pending(), "Checkpoint was silently restored");
        const auto before = ui.item(launcher); ui.click(launcher); ui.footer(de);
        require(ui.submitted.find(ItemCanary) != std::string::npos && ui.submitted.find(NotesCanary) != std::string::npos &&
            ui.submitted.find("Synthetic character") != std::string::npos, "Private GM recovery/character identity positive control is absent");
        canonicalUnchanged(campaign);
        ui.click(label(de, "Copy complete draft data", "Vollständige Entwurfsdaten kopieren"));
        require(!ui.clipboard.empty() && editor.editorDraftRecovery().pending(), "Copy changed pending state or omitted checkpoint bytes");
        ui.click(label(de, "Restore saved drafts", "Gesicherte Entwürfe wiederherstellen"));
        require(!editor.editorDraftRecovery().pending() && EditorDraftRecovery::encode(campaign.metadata.id,
            editor.editorDraftRecovery().drafts()).payload == encoded, "Restore altered or committed the item/power/notes payload");
        canonicalUnchanged(campaign); ui.footer(de);
        ui.clipboard.clear(); ui.keyboardActivate(label(de, "Copy complete draft data", "Vollständige Entwurfsdaten kopieren"));
        require(!ui.clipboard.empty(), "Keyboard Copy did not activate the actual recovery button");
        EditorDraftRecoveryUiProbe::privateMode(editor, true); ui.frame(3);
        require(!editor.editorDraftRecovery().open && ui.submitted.find(ItemCanary) == std::string::npos &&
            ui.submitted.find(NotesCanary) == std::string::npos, "Presentation submitted private recovery text or kept its window open");
        EditorDraftRecoveryUiProbe::privateMode(editor, false); ui.frame(3);
        const auto after = ui.item(launcher);
        require(before.id == after.id && before.minimum.x == after.minimum.x && before.minimum.y == after.minimum.y &&
            before.maximum.x == after.maximum.x && before.maximum.y == after.maximum.y, "Recovery state changed launcher geometry/identity");
        ui.click(launcher); ui.click(label(de, "Close review", "Prüfung schließen"));
        require(editor.editorDraftRecovery().checkpoint() && !editor.editorDraftRecovery().drafts().empty(), "Close lost restored unfinished edits");
        ui.draw = {};
    }
    {
        EditorWindowController editor; require(editor.setCurrentCampaign(readCampaign(campaign.metadata.id)), "Restart campaign load failed");
        ui.draw = [&] { EditorDraftRecoveryUiProbe::renderRecovery(editor); }; ui.frame(4);
        require(editor.editorDraftRecovery().pending(), "Fresh controller did not offer explicit recovery");
        ui.click(launcher); ui.footer(de);
        const auto checkpoint = checkpointPath(campaign);
        auto held = checkpoint; held += ".held"; std::filesystem::rename(checkpoint, held);
        std::filesystem::create_directory(checkpoint); { std::ofstream marker(checkpoint / "deny-remove"); marker << "synthetic blocker"; }
        ui.click(discard);
        require(editor.editorDraftRecovery().open && editor.editorDraftRecovery().pending() &&
            !editor.editorDraftRecovery().error().empty(), "Failed discard closed review or lost pending recovery");
        ui.footer(de); canonicalUnchanged(campaign);
        std::filesystem::remove(checkpoint / "deny-remove"); std::filesystem::remove(checkpoint);
        std::filesystem::rename(held, checkpoint); ui.click(discard);
        require(!editor.editorDraftRecovery().open && !editor.editorDraftRecovery().pending() &&
            EditorDraftRecovery::read(campaign).status == EditorDraftRecovery::ReadStatus::Absent, "Explicit discard did not remove the checkpoint");
        canonicalUnchanged(campaign); ui.draw = {};
    }
    {
        const std::string malformed = "SYNTHETIC_PRIVATE_MALFORMED_CHECKPOINT";
        { std::ofstream file(checkpointPath(campaign), std::ios::binary); file << malformed; require(bool(file), "Cannot seed malformed recovery fixture"); }
        EditorWindowController editor; require(editor.setCurrentCampaign(readCampaign(campaign.metadata.id)), "Malformed checkpoint load failed");
        ui.draw = [&] { EditorDraftRecoveryUiProbe::renderRecovery(editor); }; ui.frame(4); ui.click(launcher);
        const auto &recovery = editor.editorDraftRecovery();
        require(recovery.pending() && recovery.recovered().status == EditorDraftRecovery::ReadStatus::Invalid &&
            !recovery.recovered().error.empty() && ui.submitted.find(recovery.recovered().error) != std::string::npos,
            "Malformed checkpoint omitted its private review error or bypassed pending review");
        ui.footer(de); ui.disabled(label(de, "Restore saved drafts", "Gesicherte Entwürfe wiederherstellen"));
        ui.click(label(de, "Retry recovery", "Sicherung erneut versuchen"));
        require(recovery.pending() && recovery.recovered().rawBytes == malformed, "Retry changed malformed saved checkpoint bytes");
        ui.click(label(de, "Copy complete draft data", "Vollständige Entwurfsdaten kopieren"));
        require(ui.clipboard == EditorDraftRecovery::copyCheckpoint(recovery.recovered()), "Malformed checkpoint Copy omitted exact complete bytes");
        ui.click(discard); canonicalUnchanged(campaign);
        require(EditorDraftRecovery::read(campaign).status == EditorDraftRecovery::ReadStatus::Absent,
            "Explicit malformed checkpoint discard was not durable");
        ui.draw = {};
    }
}
}

int main()
{
    try
    {
        const auto root = std::filesystem::temp_directory_path() / ("gm-editor-recovery-ui-" + createEntityId("run"));
        editor_recovery_test::isolate(root);
        auto &save = ProjectSaveService::getInstance(); save.setBackupSnapshotsEnabled(false); save.setRecoverySnapshotsEnabled(false);
        for (bool de : {false, true}) for (bool compact : {false, true}) recoveryCase(de, compact);
        std::cout << "4 editor recovery UI groups: explicit Restore/Copy/checked Discard, malformed Retry/Copy, keyboard activation, private gate, compact footer and fresh-owner recovery passed.\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
