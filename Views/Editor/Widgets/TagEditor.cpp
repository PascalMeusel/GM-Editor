#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "TagEditor.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"

#include "Models/Project/CampaignTagService.hpp"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>

namespace
{
bool hasTag(const std::vector<std::string> &tags, const std::string &tag)
{
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}
}

bool renderTagEditor(const char *id,
                     std::vector<std::string> &tags,
                     TagEditorState &state,
                     const std::vector<std::string> &suggestions)
{
    bool changed = false;
    ImGui::PushID(id);

    for (std::size_t index = 0; index < tags.size();)
    {
        ImGui::PushID(static_cast<int>(index));
        const std::string buttonLabel = tags[index] + "  x";
        if (ImGui::SmallButton(buttonLabel.c_str()))
        {
            tags.erase(tags.begin() + static_cast<std::ptrdiff_t>(index));
            changed = true;
            ImGui::PopID();
            continue;
        }
        ImGui::PopID();
        ++index;
        if (index < tags.size())
            ImGui::SameLine();
    }

    ImGui::SetNextItemWidth(-80.0f);
    const bool submitted = FormUi::InputTextWithHint(localizedUiLabel("##TagDraft").c_str(), localizedUiText("Add tag"), &state.draftTag, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button(localizedUiLabel("Add").c_str()) || submitted)
    {
        std::vector<std::string> nextTags = tags;
        nextTags.push_back(state.draftTag);
        nextTags = CampaignTags::normalize(nextTags);
        if (nextTags != tags)
        {
            tags = nextTags;
            changed = true;
        }
        state.draftTag.clear();
    }

    if (!suggestions.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("Suggestions").c_str());
        for (const std::string &suggestion : suggestions)
        {
            if (suggestion.empty() || hasTag(tags, suggestion))
                continue;
            if (ImGui::SmallButton(suggestion.c_str()))
            {
                tags.push_back(suggestion);
                tags = CampaignTags::normalize(tags);
                changed = true;
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }

    ImGui::PopID();
    return changed;
}
