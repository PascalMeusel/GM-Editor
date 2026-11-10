#pragma once

#include "imgui.h"
#include <string>

// An abstract base class for any window that needs to be dockable,
// movable, and resizable.
class DockableWidget
{
public:
    // Constructor allows setting a title and initial flags.
    // By default, it's set to auto-resize to its content.
    DockableWidget(const std::string& title, ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize, ImVec2 defaultSize = ImVec2(520.0f, 420.0f));
    
    // Virtual destructor
    virtual ~DockableWidget() = default;

    // This is the main public method to be called in your application loop.
    // It handles the Begin/End calls and calls the internal draw method.
    void update();

    // Reuse a specialist's production controller/view inside its owning workspace.
    // The scope is restricted to rendering; Player Display updates outside it.
    class EmbeddedScope
    {
    public:
        EmbeddedScope() { ++s_embeddingDepth; }
        ~EmbeddedScope() { --s_embeddingDepth; }
        EmbeddedScope(const EmbeddedScope &) = delete;
        EmbeddedScope &operator=(const EmbeddedScope &) = delete;
    };

    // Methods to control the visibility of the window
    void show();
    void hide();
    bool isVisible() const;
    void toggleVisibility();
    void requestDockOnNextUpdate();

    static void setDefaultDockId(ImGuiID dockId);
    static ImGuiID defaultDockId();
    static void setSidebarDockId(ImGuiID dockId);
    static ImGuiID sidebarDockId();

protected:
    // Pure virtual function that derived classes MUST implement.
    // This is where you will put all your ImGui widgets for a specific feature.
    virtual void draw() = 0;

private:
    std::string _title;
    bool _isVisible;
    bool _dockOnNextUpdate;
    int _focusUpdatesRemaining = 0;
    ImGuiWindowFlags _flags;
    ImVec2 _defaultSize;

    static unsigned s_embeddingDepth;
    static ImGuiID s_defaultDockId;
    static ImGuiID s_sidebarDockId;
};
