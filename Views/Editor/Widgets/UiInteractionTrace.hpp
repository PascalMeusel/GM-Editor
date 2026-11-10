#pragma once
#include "imgui.h"
#include <map>
#include <string>
#include <vector>

// Optional instrumentation for scripted interaction. It observes production
// ImGui widgets; tests still drive the ordinary mouse and keyboard input queue.
class UiInteractionTrace
{
public:
    struct Item
    {
        ImGuiID id = 0;
        ImVec2 minimum{}, maximum{};
        std::string label, window;
        bool hasRectangle = false;
    };
    explicit UiInteractionTrace(ImGuiContext *context);
    ~UiInteractionTrace();
    UiInteractionTrace(const UiInteractionTrace &) = delete;
    UiInteractionTrace &operator=(const UiInteractionTrace &) = delete;
    void beginFrame();
    std::vector<Item> find(const std::string &label) const;
    const std::map<ImGuiID, Item> &items() const { return items_; }
    void rectangle(ImGuiID id, ImVec2 minimum, ImVec2 maximum, const char *window);
    void label(ImGuiID id, const char *text);
    const char *labelFor(ImGuiID id) const;
private:
    ImGuiContext *context_;
    void *previousEngine_;
    bool previousHooks_;
    std::map<ImGuiID, Item> items_;
};
