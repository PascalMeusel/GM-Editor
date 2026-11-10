#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "ProjectAssetRepairPanel.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <string>

namespace ProjectAssetRepairPanel
{
bool draw(const char *id, AssetRepairPanelState &state, const AssetRepairPanelEntry &entry)
{
    ImGui::PushID(id);
    const std::string assetKind = localizedUiLabel(ProjectAssetImport::assetKindLabel(entry.kind));
    ImGui::TextWrapped(localizedUiLabel("Stored File: %s").c_str(), entry.storedPath.c_str());
    if (!entry.resolvedPath.empty())
        ImGui::TextWrapped(localizedUiLabel("Resolved File: %s").c_str(), entry.resolvedPath.c_str());

    if (!entry.missing)
    {
        ImGui::TextDisabled(localizedUiLabel("%s file is available.").c_str(), assetKind.c_str());
        ImGui::PopID();
        return false;
    }

    ImGui::TextColored(ImVec4(0.95f, 0.36f, 0.30f, 1.0f), localizedUiLabel("%s file is missing.").c_str(), assetKind.c_str());
    ImGui::SetNextItemWidth(-1.0f);
    FormUi::InputText(localizedUiLabel("Replacement File").c_str(), &state.replacementPath);
    const bool relink = ImGui::Button(localizedUiLabel("Relink").c_str());
    ImGui::PopID();
    return relink;
}
}
