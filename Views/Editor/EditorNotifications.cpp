#include "Views/Editor/EditorViewDetails.hpp"

using namespace gm::editor_view_detail;

void EditorWindowView::renderToasts()
{
    collectToastMessages();

    const double now = ImGui::GetTime();
    _toasts.erase(std::remove_if(_toasts.begin(), _toasts.end(),
                                 [&](const ToastMessage &toast) { return now - toast.createdAt > 5.0; }),
                  _toasts.end());
    if (_toasts.empty())
        return;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float width = std::min(360.0f, viewport->WorkSize.x - 36.0f);
    float y = viewport->WorkPos.y + viewport->WorkSize.y - 18.0f;
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

    for (std::size_t index = 0; index < _toasts.size(); ++index)
    {
        const ToastMessage &toast = _toasts[index];
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 18.0f, y), ImGuiCond_Always,
                                ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.94f);
        const std::string id = "##EditorToast" + std::to_string(index);
        if (ImGui::Begin(id.c_str(), nullptr, flags))
        {
            pushBoldFont();
            ImGui::TextColored(toast.color, localizedUiLabel("%s").c_str(), toast.title.c_str());
            popBoldFont();
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), toast.body.c_str());
        }
        y -= ImGui::GetWindowSize().y + 8.0f;
        ImGui::End();
    }
}

void EditorWindowView::collectToastMessages()
{
    const std::vector<ProjectEvent> &events = ProjectEventBus::getInstance().history();
    for (std::size_t index = 0; index < events.size(); ++index)
    {
        const ProjectEvent &event = events[index];
        if (event.sequence <= _lastToastEventSequence) continue;
        switch (event.type)
        {
        case ProjectEventType::CampaignSaved:
            // Detailed save reasons stay in the event history. The toast is a
            // user-facing confirmation in the active interface language.
            _toasts.erase(std::remove_if(_toasts.begin(), _toasts.end(), [](const ToastMessage &toast) {
                return toast.title == "Not saved" || toast.title == "Nicht gespeichert";
            }), _toasts.end());
            pushToast(gm::localization::tr(gm::localization::TextId::CommonSaved),
                      uiTextForLocale("Campaign data saved.","Kampagnendaten gespeichert."), kAccent);
            break;
        case ProjectEventType::SessionStarted:
        case ProjectEventType::SessionEnded:
            pushToast(localizedUiText(projectEventTypeLabel(event.type)),
                      gm::localization::trSystemMessage(event.message), kAccent);
            break;
        case ProjectEventType::AudioChanged:
        case ProjectEventType::MapChanged:
        case ProjectEventType::InventoryChanged:
            pushToast(localizedUiText(projectEventTypeLabel(event.type)), event.message, kAccent);
            break;
        case ProjectEventType::EntityCreated:
            if (event.message.find("Imported") != std::string::npos ||
                event.message.find("imported") != std::string::npos)
                pushToast("Import", event.message, kAccent);
            break;
        default:
            break;
        }
    }
    if (!events.empty()) _lastToastEventSequence = events.back().sequence;

    const std::vector<ProjectLogEntry> &logs = ProjectLogger::getInstance().history();
    const auto firstUnread = std::find_if(logs.begin(), logs.end(), [&](const ProjectLogEntry &entry) {
        return entry.sequence > _lastToastLogSequence;
    });
    const auto firstLogIndex = static_cast<std::size_t>(firstUnread - logs.begin());
    // A failed transaction is logged by both persistence and the save service.
    // Keep both diagnostics, but show the user one actionable notification.
    std::vector<std::string> saveErrors;
    for (std::size_t index = firstLogIndex; index < logs.size(); ++index)
        if (logs[index].level == ProjectLogLevel::Error && logs[index].source == "ProjectSaveService")
            saveErrors.push_back(logs[index].message);
    for (std::size_t index = firstLogIndex; index < logs.size(); ++index)
    {
        const ProjectLogEntry &entry = logs[index];
        if (entry.level == ProjectLogLevel::Error && entry.source == "ProjectSaveService")
        {
            _toasts.erase(std::remove_if(_toasts.begin(), _toasts.end(), [](const ToastMessage &toast) {
                return toast.title == "Saved" || toast.title == "Gespeichert";
            }), _toasts.end());
            pushToast(uiTextForLocale("Not saved", "Nicht gespeichert"),
                      uiTextForLocale("Your changes are retained. Retry saving.",
                                      "Deine Änderungen bleiben erhalten. Wiederhole die Speicherung."),
                      ImVec4(0.95f, 0.42f, 0.38f, 1.0f));
            continue;
        }
        if (entry.level == ProjectLogLevel::Error && entry.source == "DatabaseManager" &&
            std::any_of(saveErrors.begin(), saveErrors.end(), [&](const std::string &error) {
                return !error.empty() && entry.message.find(error) != std::string::npos;
            })) continue;
        if (entry.level == ProjectLogLevel::Warning)
            pushToast("Warning: " + entry.source, entry.message, kWarmAccent);
        else if (entry.level == ProjectLogLevel::Error)
            pushToast("Error: " + entry.source, entry.message, ImVec4(0.95f, 0.42f, 0.38f, 1.0f));
    }
    if (!logs.empty()) _lastToastLogSequence = logs.back().sequence;
}

void EditorWindowView::pushToast(const std::string &title, const std::string &body, const ImVec4 &color)
{
    if (body.empty())
        return;

    _toasts.erase(std::remove_if(_toasts.begin(), _toasts.end(),
                                 [&](const ToastMessage &toast) { return toast.title == title && toast.body == body; }),
                  _toasts.end());
    _toasts.push_back(ToastMessage{title, body, color, ImGui::GetTime()});
    constexpr std::size_t maxToasts = 2;
    if (_toasts.size() > maxToasts)
        _toasts.erase(_toasts.begin(), _toasts.begin() + static_cast<std::ptrdiff_t>(_toasts.size() - maxToasts));
}
