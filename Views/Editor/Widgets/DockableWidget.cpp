#include "DockableWidget.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include <algorithm>

unsigned DockableWidget::s_embeddingDepth = 0;
ImGuiID DockableWidget::s_defaultDockId = 0;
ImGuiID DockableWidget::s_sidebarDockId = 0;

DockableWidget::DockableWidget(const std::string& title, ImGuiWindowFlags flags, ImVec2 defaultSize)
    : _title(title), _isVisible(true), _dockOnNextUpdate(true), _flags(flags), _defaultSize(defaultSize)
{
    // You can add any other default setup here
}

void DockableWidget::update()
{
    // If the window is not visible, do nothing.
    if (!_isVisible)
        return;

    if (s_embeddingDepth != 0)
    {
        ImGui::PushID(_title.c_str());
        const bool visible = ImGui::BeginChild("EmbeddedEditorContent", ImVec2(0.0f, 0.0f),
            ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        _dockOnNextUpdate = false;
        if (_focusUpdatesRemaining > 0)
        {
            ImGui::SetWindowFocus();
            --_focusUpdatesRemaining;
        }
        if (visible) draw();
        ImGui::EndChild();
        ImGui::PopID();
        return;
    }

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImVec2 maxSize(viewport->WorkSize.x * 0.92f, viewport->WorkSize.y * 0.92f);
    const ImVec2 windowSize(std::min(_defaultSize.x, maxSize.x), std::min(_defaultSize.y, maxSize.y));
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(360.0f, 240.0f), maxSize);
    if (_dockOnNextUpdate && s_defaultDockId != 0)
        ImGui::SetNextWindowDockID(s_defaultDockId, ImGuiCond_Always);
    if (_focusUpdatesRemaining > 0)
        ImGui::SetNextWindowFocus();

    // Use the application's chosen control spacing, including comfortable
    // sizing and the user's optional compact-table preference.

    // The ImGui::Begin call creates the window.
    // The second parameter is a pointer to a boolean which, if the user
    // closes the window, will be set to false. We use our _isVisible member here.
    const std::string localizedTitle = std::string(gm::localization::trUi(_title.c_str())) + "###" + _title;
    const bool isExpanded = ImGui::Begin(localizedTitle.c_str(), &_isVisible, _flags);
    _dockOnNextUpdate = false;
    if (_focusUpdatesRemaining > 0)
        --_focusUpdatesRemaining;
    if (isExpanded)
    {
        // If the window is not collapsed, call the derived class's draw method.
        draw();
    }
    // Always call End, even if Begin returned false.
    ImGui::End();
}

void DockableWidget::show()
{
    _isVisible = true;
    // A newly docked tab is selected on the following frame. Retain the focus
    // request across that transition so a linked editor actually comes forward.
    _focusUpdatesRemaining = 2;
}

void DockableWidget::hide()
{
    _isVisible = false;
    _focusUpdatesRemaining = 0;
}

bool DockableWidget::isVisible() const
{
    return _isVisible;
}

void DockableWidget::toggleVisibility()
{
    _isVisible = !_isVisible;
    _focusUpdatesRemaining = _isVisible ? 2 : 0;
}

void DockableWidget::requestDockOnNextUpdate()
{
    _dockOnNextUpdate = true;
}

void DockableWidget::setDefaultDockId(ImGuiID dockId)
{
    s_defaultDockId = dockId;
}

ImGuiID DockableWidget::defaultDockId()
{
    return s_defaultDockId;
}

void DockableWidget::setSidebarDockId(ImGuiID dockId)
{
    s_sidebarDockId = dockId;
}

ImGuiID DockableWidget::sidebarDockId()
{
    return s_sidebarDockId;
}
