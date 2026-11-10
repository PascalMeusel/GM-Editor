#include "Views/Editor/Widgets/UiInteractionTrace.hpp"
#include "imgui_internal.h"
#include <stdexcept>

UiInteractionTrace::UiInteractionTrace(ImGuiContext *context) : context_(context)
{
    if (!context_) throw std::invalid_argument("UI trace needs an ImGui context");
    previousEngine_ = context_->TestEngine;
    previousHooks_ = context_->TestEngineHookItems;
    if (previousEngine_) throw std::logic_error("An interaction observer already owns this context");
    context_->TestEngine = this;
    context_->TestEngineHookItems = true;
}
UiInteractionTrace::~UiInteractionTrace()
{
    context_->TestEngine = previousEngine_;
    context_->TestEngineHookItems = previousHooks_;
}
void UiInteractionTrace::beginFrame() { items_.clear(); }
std::vector<UiInteractionTrace::Item> UiInteractionTrace::find(const std::string &text) const
{
    std::vector<Item> result;
    for (const auto &entry : items_)
        if (entry.second.hasRectangle) {
            const auto &label = entry.second.label;
            const auto stable = label.find("###");
            if (label == text || (stable != std::string::npos && label.substr(stable + 3) == text))
                result.push_back(entry.second);
        }
    return result;
}
void UiInteractionTrace::rectangle(ImGuiID id, ImVec2 minimum, ImVec2 maximum, const char *window)
{
    auto &item = items_[id];
    item.id = id; item.minimum = minimum; item.maximum = maximum;
    item.window = window ? window : ""; item.hasRectangle = true;
}
void UiInteractionTrace::label(ImGuiID id, const char *text)
{
    auto &item = items_[id];
    item.id = id; item.label = text ? text : "";
}
const char *UiInteractionTrace::labelFor(ImGuiID id) const
{
    const auto found = items_.find(id);
    return found == items_.end() ? nullptr : found->second.label.c_str();
}

void ImGuiTestEngineHook_ItemAdd(ImGuiContext *context, ImGuiID id, const ImRect &rectangle, const ImGuiLastItemData *)
{
    if (context->TestEngine)
        static_cast<UiInteractionTrace *>(context->TestEngine)->rectangle(id, rectangle.Min, rectangle.Max,
            context->CurrentWindow ? context->CurrentWindow->Name : "");
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext *context, ImGuiID id, const char *label, ImGuiItemStatusFlags)
{
    if (context->TestEngine) static_cast<UiInteractionTrace *>(context->TestEngine)->label(id, label);
}
void ImGuiTestEngineHook_Log(ImGuiContext *, const char *, ...) {}
const char *ImGuiTestEngine_FindItemDebugLabel(ImGuiContext *context, ImGuiID id)
{
    return context->TestEngine ? static_cast<UiInteractionTrace *>(context->TestEngine)->labelFor(id) : nullptr;
}
