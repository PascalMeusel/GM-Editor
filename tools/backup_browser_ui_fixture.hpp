#pragma once
#include "Controllers/MainMenu/MainMenuWindowController.hpp"
#include "Views/MainMenu/MainMenuWindowView.hpp"
#include "Views/Editor/Widgets/UiInteractionTrace.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "imgui_internal.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

struct BackupBrowserUiProbe
{
    static void open(MainMenuWindowController &controller) { controller.view->openBackupBrowserPopup(); }
    static void draw(MainMenuWindowController &controller) { controller.view->handleBackupBrowserPopup(); }
    static const auto &entries(MainMenuWindowController &controller) { return controller.view->_backupEntries; }
    static std::string selected(MainMenuWindowController &controller) { return controller.view->_selectedBackupKey; }
    static bool confirmed(MainMenuWindowController &controller) { return controller.view->_confirmBackupRestore; }
    static bool openNow(MainMenuWindowController &controller) { return controller.view->_showBackupBrowserPopup; }
    static std::string status(MainMenuWindowController &controller) { return controller.view->_backupBrowserStatus; }
    static auto safety(MainMenuWindowController &controller) { return controller.view->_lastRestoreSafetyPath; }
};
namespace backup_ui
{
namespace fs = std::filesystem;
inline void require(bool value, const std::string &message) { if (!value) throw std::runtime_error(message); }
inline const char *label(bool de, const char *en, const char *german) { return de ? german : en; }
inline std::string key(const fs::path &path, const std::string &id) { return path.lexically_normal().generic_u8string() + "\n" + id; }
inline void env(const char *name, const fs::path &value)
{
#ifdef _WIN32
    require(_putenv_s(name, value.u8string().c_str()) == 0, "Cannot isolate backup UI environment");
#else
    require(setenv(name, value.u8string().c_str(), 1) == 0, "Cannot isolate backup UI environment");
#endif
}
inline void isolate(const fs::path &root)
{
    fs::create_directories(root / "data"); fs::create_directories(root / "temp");
    env("TTRPG_DATA_ROOT", root / "data"); env("TTRPG_DB_PATH", root / "data/test.db");
    env("TTRPG_SETTINGS_PATH", root / "data/settings.ini");
    for (const char *name : {"TEMP", "TMP", "TMPDIR"}) env(name, root / "temp");
}
inline std::string bytes(const fs::path &path)
{ std::ifstream file(path, std::ios::binary); require(bool(file), "Cannot read synthetic snapshot"); return {std::istreambuf_iterator<char>(file), {}}; }
inline void write(const fs::path &path, const std::string &value)
{ std::ofstream file(path, std::ios::binary | std::ios::trunc); file.write(value.data(), value.size()); file.flush(); require(bool(file), "Cannot write synthetic snapshot"); }
inline Campaign read(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns()) if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Expected synthetic campaign is absent");
}

// Observed production browser widgets, CPU ImGui only: no Application/native window,
// rendering backend, images, audio or OS clipboard. All paths are invocation-isolated.
class Ui
{
    ImGuiContext *context_ = ImGui::CreateContext();
    std::unique_ptr<UiInteractionTrace> trace_;
public:
    std::function<void()> draw;
    std::string submitted;
    Ui(bool de, bool compact)
    {
        gm::localization::LocalizationService::instance().setPreference(de ? gm::localization::LocalePreference::German : gm::localization::LocalePreference::English, false);
        auto &io = ImGui::GetIO(); io.IniFilename = nullptr; io.DeltaTime = 1.f / 60;
        io.DisplaySize = compact ? ImVec2(1280, 720) : ImVec2(1600, 1000);
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImFontConfig font; font.SizePixels = 18; io.Fonts->AddFontDefault(&font);
        ImGui::GetStyle().FontScaleMain = compact ? 1.5f : 1.f;
        auto &platform = ImGui::GetPlatformIO(); platform.Platform_SetClipboardTextFn = nullptr;
        platform.Platform_GetClipboardTextFn = nullptr; platform.Platform_SetImeDataFn = nullptr; platform.Platform_OpenInShellFn = nullptr;
        unsigned char *pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        require(pixels && width > 0 && height > 0, "Synthetic font atlas unavailable");
        trace_ = std::make_unique<UiInteractionTrace>(context_);
    }
    ~Ui() { trace_.reset(); ImGui::DestroyContext(context_); }
    void frame(int count = 1)
    {
        while (count--)
        {
            trace_->beginFrame(); ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Backup UI fixture", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
            ImGui::LogToBuffer(); draw(); submitted = context_->LogBuffer.c_str();
            ImGui::LogFinish(); ImGui::End(); ImGui::Render();
        }
    }
    UiInteractionTrace::Item item(const std::string &caption, ImGuiID id = 0) const
    {
        auto found = trace_->find(caption);
        found.erase(std::remove_if(found.begin(), found.end(), [&](const auto &value) {
            const auto *owner = ImGui::FindWindowByName(value.window.c_str());
            return (id && id != value.id) || (owner && owner->ID == value.id);
        }), found.end());
        if (found.size() != 1)
        {
            std::ostringstream out; out << "Expected unique backup control '" << caption << "' id=" << id << " count=" << found.size();
            for (const auto &[key, value] : trace_->items()) if (!value.label.empty()) out << "\n" << key << " '" << value.label << "' " << value.window;
            throw std::runtime_error(out.str());
        }
        return found.front();
    }
    void visible(const UiInteractionTrace::Item &value) const
    {
        const auto *owner = ImGui::FindWindowByName(value.window.c_str()); const auto display = ImGui::GetIO().DisplaySize;
        if (!owner || value.minimum.x < owner->InnerClipRect.Min.x - 1 || value.minimum.y < owner->InnerClipRect.Min.y - 1 ||
            value.maximum.x > owner->InnerClipRect.Max.x + 1 || value.maximum.y > owner->InnerClipRect.Max.y + 1 ||
            value.minimum.x < 0 || value.minimum.y < 0 || value.maximum.x > display.x || value.maximum.y > display.y)
        {
            std::ostringstream out; out << "Backup widget clipped '" << value.label << "' owner=" << value.window << " rect="
                << value.minimum.x << ',' << value.minimum.y << '-' << value.maximum.x << ',' << value.maximum.y;
            if (owner) out << " clip=" << owner->InnerClipRect.Min.x << ',' << owner->InnerClipRect.Min.y << '-'
                << owner->InnerClipRect.Max.x << ',' << owner->InnerClipRect.Max.y << " scroll=" << owner->Scroll.y << '/' << owner->ScrollMax.y;
            throw std::runtime_error(out.str());
        }
    }
    void click(const std::string &caption, ImGuiID requested = 0)
    {
        frame(3); ImGuiID id = 0;
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto value = item(caption, requested); visible(value);
            require(!id || id == value.id, "Backup target identity changed while hovering"); id = value.id;
            ImGui::GetIO().AddMousePosEvent((value.minimum.x + value.maximum.x) * .5f, (value.minimum.y + value.maximum.y) * .5f); frame();
        }
        require(context_->HoveredId == id && !context_->HoveredIdIsDisabled, "Backup action is not the exact enabled hovered target");
        ImGui::GetIO().AddMouseButtonEvent(0, true); frame(); ImGui::GetIO().AddMouseButtonEvent(0, false); frame(3);
    }
    ImGuiWindow *window(const char *part) const
    {
        ImGuiWindow *result = nullptr;
        for (auto *window : context_->Windows) if (window->Active && std::string(window->Name).find(part) != std::string::npos)
        { require(!result, std::string("Ambiguous browser child: ") + part); result = window; }
        require(result, std::string("Missing browser child: ") + part); return result;
    }
    void select(const ProjectBackupRestore::BackupEntry &entry)
    {
        frame(3); auto *list = window("/BackupList_");
        const ImGuiID id = ImHashStr(entry.campaignName.c_str(), 0, ImHashStr(key(entry.snapshotPath, entry.campaignId).c_str(), 0, list->ID));
        const auto move = [&] { ImGui::GetIO().AddMousePosEvent(list->InnerClipRect.GetCenter().x, list->InnerClipRect.GetCenter().y); frame(2); };
        move(); ImGui::GetIO().AddMouseWheelEvent(0, 40); frame(3);
        for (int attempt = 0; attempt < 60; ++attempt)
        {
            const auto found = trace_->items().find(id);
            if (found != trace_->items().end() && !found->second.label.empty() && found->second.minimum.y >= list->InnerClipRect.Min.y &&
                found->second.maximum.y <= list->InnerClipRect.Max.y)
            { click(entry.campaignName, id); return; }
            move(); ImGui::GetIO().AddMouseWheelEvent(0, -.5f); frame(3);
        }
        throw std::runtime_error("Managed backup row never became visible: " + entry.snapshotPath.string());
    }
    void footer(bool de, bool removing = false)
    {
        for (const char *caption : {label(de, "I have reviewed the scope", "Umfang geprüft"),
            removing ? label(de, "Remove selected campaign", "Ausgewählte Kampagne entfernen") : label(de, "Restore selected backup", "Ausgewählte Sicherung wiederherstellen"),
            label(de, "Review safety backup", "Sicherheitssicherung prüfen"), label(de, "Refresh", "Aktualisieren"), label(de, "Close", "Schließen")}) visible(item(caption));
        auto *status = window("/BackupBrowserStatus_");
        require(status->Pos.y >= 0 && status->Pos.y + status->Size.y <= ImGui::GetIO().DisplaySize.y, "Backup failure feedback viewport is clipped");
    }
    void keyPress(ImGuiKey key)
    { ImGui::GetIO().AddKeyEvent(key, true); frame(); ImGui::GetIO().AddKeyEvent(key, false); frame(2); }
    void keyboard(const char *caption)
    {
        const auto target = item(caption); visible(target);
        std::ostringstream route;
        const auto observe = [&] {
            route << "\nnav=" << context_->NavId << " scope=" << context_->NavFocusScopeId
                << " active=" << context_->ActiveId << " window=" << (context_->NavWindow ? context_->NavWindow->Name : "<none>");
        };
        observe();
        for (int count = 0; count < 50 && context_->NavId != target.id; ++count) { keyPress(ImGuiKey_Tab); observe(); }
        require(context_->NavId == target.id, std::string("Backup keyboard navigation did not reach '") + caption +
            "' id=" + std::to_string(target.id) + " owner=" + target.window + route.str());
        const auto current = item(caption, target.id); visible(current);
        require(current.window == target.window, "Backup keyboard target changed owner before activation");
        keyPress(ImGuiKey_Space);
    }
};
}
