#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "Views/Editor/Widgets/DraftEditSignal.hpp"
#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

// Field widths in Dear ImGui exclude the right-hand label. Use the actual
// content region (including tables/indentation) and font metrics, rather than
// assuming every translated label fits in the remaining third of a window.
namespace FormUi
{
inline void ensureFieldSpace()
{
    // An explicitly inline field must not become a few pixels wide at the
    // edge of a narrowed window. NewLine respects table columns/indentation.
    if (ImGui::GetCurrentWindow()->DC.IsSameLine &&
        ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 8.0f)
        ImGui::NewLine();
}

class FieldLabel
{
    ImGuiWindow *owner_ = nullptr;
    const char *label_;
    const char *originalLabel_;
public:
    FieldLabel(const char *label, bool stacked) : label_(label), originalLabel_(label)
    {
        if (stacked)
        {
            owner_ = ImGui::GetCurrentWindow();
            ImGui::PushOverrideID(ImGui::GetID(label));
            label_ = ""; // Hashing the empty label returns the overridden seed.
        }
    }
    FieldLabel(const FieldLabel &) = delete;
    ~FieldLabel()
    {
        // BeginCombo may have switched to its popup window. Restore the owner
        // stack, not the popup stack. Ordinary fields keep LastItem untouched.
        if (owner_) {
#ifdef IMGUI_ENABLE_TEST_ENGINE
            // Stacked controls render their caption separately, but observers
            // still need the original label for the same native widget ID.
            auto &g = *ImGui::GetCurrentContext();
            IMGUI_TEST_ENGINE_ITEM_INFO(owner_->IDStack.back(), originalLabel_, g.LastItemData.StatusFlags);
#endif
            owner_->IDStack.pop_back();
        }
    }
    const char *c_str() const { return label_; }
};

inline FieldLabel fieldLabel(const char *label, float requestedWidth = 0.0f)
{
    ensureFieldSpace();
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float desired = requestedWidth == 0.0f ? ImGui::CalcItemWidth() :
        (requestedWidth < 0.0f ? std::max(1.0f, available + requestedWidth) : requestedWidth);
    const float width = std::min(desired, available);
    const char *end = std::strstr(label, "##");
    if (!end) end = label + std::strlen(label);
    const float labelWidth = ImGui::CalcTextSize(label, end).x;
    const bool stacked = end != label &&
        width + ImGui::GetStyle().ItemInnerSpacing.x + labelWidth > available;
    if (stacked)
    {
        const float startX = ImGui::GetCursorPosX();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + available);
        ImGui::TextUnformatted(label, end);
        ImGui::PopTextWrapPos();
        ImGui::SetCursorPosX(startX);
    }
    ImGui::SetNextItemWidth(width);
    // Keep native IDs and LastItem so undo, help and keyboard focus still work.
    return FieldLabel(label, stacked);
}

#define GM_ADAPTIVE_FIELD(name) \
    template<class... Args> inline bool name(const char *label, Args&&... args) \
    { const auto adapted = fieldLabel(label); \
      return DraftEditSignal::record(ImGui::name(adapted.c_str(), std::forward<Args>(args)...)); }

GM_ADAPTIVE_FIELD(InputText)
GM_ADAPTIVE_FIELD(InputTextWithHint)
GM_ADAPTIVE_FIELD(InputInt)
GM_ADAPTIVE_FIELD(InputFloat)
GM_ADAPTIVE_FIELD(InputDouble)
GM_ADAPTIVE_FIELD(InputScalar)
GM_ADAPTIVE_FIELD(SliderFloat)
GM_ADAPTIVE_FIELD(SliderInt)
GM_ADAPTIVE_FIELD(DragFloat2)
GM_ADAPTIVE_FIELD(Combo)
#undef GM_ADAPTIVE_FIELD

template<class... Args> inline bool BeginCombo(const char *label, Args&&... args)
{
    const auto adapted = fieldLabel(label);
    return ImGui::BeginCombo(adapted.c_str(), std::forward<Args>(args)...);
}

inline ImVec2 multilineSize(ImVec2 size)
{
    ensureFieldSpace();
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    if (size.x == 0.0f) size.x = ImGui::CalcItemWidth();
    if (size.x < 0.0f) size.x = std::max(1.0f, available + size.x);
    size.x = std::min(size.x, available);
    return size;
}

inline bool InputTextMultiline(const char *label, std::string *value,
    ImVec2 size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0,
    ImGuiInputTextCallback callback = nullptr, void *userData = nullptr)
{
    size = multilineSize(size);
    const auto adapted = fieldLabel(label, size.x);
    return DraftEditSignal::record(ImGui::InputTextMultiline(adapted.c_str(), value, size, flags, callback, userData));
}

inline bool InputTextMultiline(const char *label, char *value, size_t capacity,
    ImVec2 size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0,
    ImGuiInputTextCallback callback = nullptr, void *userData = nullptr)
{
    size = multilineSize(size);
    const auto adapted = fieldLabel(label, size.x);
    return DraftEditSignal::record(ImGui::InputTextMultiline(adapted.c_str(), value, capacity, size, flags, callback, userData));
}

inline std::string toggleLabel(const char *label)
{
    ensureFieldSpace();
    const char *end = std::strstr(label, "##");
    if (!end) end = label + std::strlen(label);
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x -
        ImGui::GetFrameHeight() - ImGui::GetStyle().ItemInnerSpacing.x);
    if (ImGui::CalcTextSize(label, end).x <= width)
        return std::string(label, end) + "###" + label;
    std::string wrapped;
    const char *start = label;
    while (start < end)
    {
        const char *stop = ImGui::GetFont()->CalcWordWrapPosition(
            ImGui::GetFontSize(), start, end, width);
        if (stop <= start)
        {
            unsigned int character;
            stop = start + ImTextCharFromUtf8(&character, start, end);
            if (stop <= start) break;
        }
        wrapped.append(start, stop);
        start = stop;
        while (start < end && (*start == ' ' || *start == '\t')) ++start;
        if (start < end && *start == '\n') ++start;
        if (start < end && (wrapped.empty() || wrapped.back() != '\n')) wrapped += '\n';
    }
    // Native checkbox/radio semantics keep the whole wrapped label clickable.
    return wrapped + "###" + label;
}

inline bool Checkbox(const char *label, bool *value)
{
    const auto adapted = toggleLabel(label);
    return DraftEditSignal::record(ImGui::Checkbox(adapted.c_str(), value));
}
template<class... Args> inline bool RadioButton(const char *label, Args&&... args)
{
    const auto adapted = toggleLabel(label);
    return DraftEditSignal::record(ImGui::RadioButton(adapted.c_str(), std::forward<Args>(args)...));
}
}
