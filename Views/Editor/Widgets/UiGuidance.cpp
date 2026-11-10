#include "Views/Editor/Widgets/UiGuidance.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "Views/Editor/Widgets/UiPopupPlacement.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace UiGuidance
{
const TopicText &text(Topic topic)
{
    const auto index = static_cast<std::size_t>(topic);
    const auto &catalog = gm::localization::LocalizationService::instance().locale() ==
        gm::localization::Locale::German ? Detail::german : Detail::english;
    return catalog[index < catalog.size() ? index : static_cast<std::size_t>(Topic::Terms)];
}

void drawPurpose(Topic topic)
{
    ImGui::TextWrapped("%s", text(topic).purpose);
}

void drawDetails(Topic topic, const char *buttonLabel)
{
    ImGui::PushID(static_cast<int>(topic));
    const std::string label = std::string(buttonLabel && *buttonLabel ? buttonLabel :
        uiTextForLocale("How this works", "So funktioniert es")) + "###GuidanceDetails";
    if (drawUiButton(label.c_str(), UiButtonRole::Secondary)) ImGui::OpenPopup("##GuidancePopup");
    PopupUi::prepareFixed("##GuidancePopup", ImVec2(620, 520));
    if (ImGui::BeginPopup("##GuidancePopup"))
    {
        const auto &definition = text(topic);
        const char *closeText = uiTextForLocale("Close", "Schließen");
        const std::string closeLabel = std::string(closeText) + "###GuidanceClose";
        const auto &style = ImGui::GetStyle();
        const float width = std::max(1.f, ImGui::GetContentRegionAvail().x);
        const float footerHeight = ImGui::CalcTextSize(closeText, nullptr, false,
            std::max(1.f, width - style.FramePadding.x * 2)).y + style.FramePadding.y * 2;
        const float bodyHeight = std::max(1.f,
            ImGui::GetContentRegionAvail().y - footerHeight - style.ItemSpacing.y);
        // A text-only scrolling child needs its own native navigation focus:
        // flattening would make it NoNav, so PageDown/End could not scroll it.
        if (ImGui::BeginChild("GuidanceBody", ImVec2(0, bodyHeight)))
        {
            ImGui::TextWrapped("%s", definition.title);
            const auto section = [](const char *title, const char *content) {
                ImGui::SeparatorText(title);
                ImGui::TextWrapped("%s", content);
            };
            section(uiTextForLocale("Purpose", "Zweck"), definition.purpose);
            section(uiTextForLocale("Before you begin", "Voraussetzungen"), definition.prerequisites);
            section(uiTextForLocale("What changes", "Auswirkungen"), definition.effects);
            section(uiTextForLocale("Example", "Beispiel"), definition.example);
        }
        ImGui::EndChild();
        // CloseCurrentPopup and Escape use ImGui's existing parent/nav focus
        // restoration. Do not steal focus from the source while help is open.
        if (drawUiButton(closeLabel.c_str(), UiButtonRole::Secondary, ImVec2(-1, footerHeight)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopID();
}
} // namespace UiGuidance
