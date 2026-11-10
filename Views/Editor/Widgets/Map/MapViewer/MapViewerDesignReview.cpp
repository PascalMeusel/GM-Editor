#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewInternal.hpp"

using namespace gm::map_viewer_detail;

void MapViewerView::drawDetailDesignReview()
{
    const char *title = uiTextForLocale("Compare detail maps###AtlasDesignReview",
                                      "Detailkarten vergleichen###AtlasDesignReview");
    const bool hasDesigns = !_controller._detailCandidates.empty();
    if (hasDesigns && _reviewedDetailRequestSerial != _controller._detailRequestSerial)
    {
        _reviewedDetailRequestSerial = _controller._detailRequestSerial;
        _openDetailDesignReview = true;
    }
    if (_openDetailDesignReview)
    {
        _openDetailDesignReview = false;
        _enlargedDetailDesign = -1;
        _detailDesignCreateFailed = false;
        if (hasDesigns)
            ImGui::OpenPopup(title);
    }

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x * .95f, viewport->WorkSize.y * .93f), ImGuiCond_Always);
    bool open = true;
    if (!ImGui::BeginPopupModal(title, &open, ImGuiWindowFlags_NoSavedSettings |
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        return;

    if (!hasDesigns || !_controller.hasDetailSelection())
    {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    const int count = static_cast<int>(_controller._detailCandidates.size());
    _controller._selectedDetailCandidate = std::clamp(_controller._selectedDetailCandidate, 0, count - 1);
    if (_enlargedDetailDesign >= count)
        _enlargedDetailDesign = -1;
    ImGui::TextWrapped("%s", uiTextForLocale(
        "Compare the maps, select a design, or enlarge it for a closer look. Your atlas changes only when you create the selected map.",
        "Vergleiche die Karten, wähle einen Entwurf oder vergrößere ihn. Der Atlas ändert sich erst, wenn du die gewählte Karte erstellst."));
    ImGui::TextWrapped("%s", _controller._detailMapName.c_str());
    if (_detailDesignCreateFailed)
        ImGui::TextWrapped("%s", _controller._atlasStatusMessage.c_str());

    // Only the previews scroll. Selection and closing remain within reach at
    // smaller window sizes and at the maximum text scale.
    const float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.0f;
    ImGui::BeginChild("AtlasDesignReviewContent",
                      ImVec2(0.0f, std::max(60.0f, ImGui::GetContentRegionAvail().y - footerHeight)), false);
    if (_enlargedDetailDesign >= 0)
    {
        if (drawUiButton(uiTextForLocale("Back to all designs", "Zurück zu allen Entwürfen"), UiButtonRole::Secondary))
            _enlargedDetailDesign = -1;
        for (int index = 0; index < count; ++index)
        {
            ImGui::SameLine();
            const std::string label = localizedUiLabel("Design") + " " + std::to_string(index + 1);
            if (drawUiButton(label.c_str(), _enlargedDetailDesign == index ? UiButtonRole::Primary : UiButtonRole::Secondary))
            {
                _enlargedDetailDesign = index;
                _controller._selectedDetailCandidate = index;
            }
        }
    }
    if (_enlargedDetailDesign >= 0)
    {
        const int index = _enlargedDetailDesign;
        const auto &candidate = _controller._detailCandidates[static_cast<std::size_t>(index)];
        ImGui::Text("%s %d", localizedUiLabel("Design").c_str(), index + 1);
        const auto version = (_controller._detailRequestSerial << 8u) ^ static_cast<std::uint64_t>(index + 1);
        drawAtlasDesignPreview(candidate.map, _renderPlanCache, version, "##EnlargedAtlasDesign",
                               std::max(100.0f, ImGui::GetContentRegionAvail().y - 4.0f), true);
    }
    else
    {
        const float minimumWidth = ImGui::GetFontSize() * 15.0f;
        const int columns = std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x / minimumWidth), 1, count);
        const float previewHeight = std::max(180.0f, ImGui::GetContentRegionAvail().y -
                                            ImGui::GetFrameHeightWithSpacing() * 3.0f - 16.0f);
        if (ImGui::BeginTable("AtlasDesignComparison", columns, ImGuiTableFlags_SizingStretchSame))
        {
            for (int index = 0; index < count; ++index)
            {
                ImGui::TableNextColumn();
                ImGui::PushID(index);
                const auto &candidate = _controller._detailCandidates[static_cast<std::size_t>(index)];
                const bool selected = _controller._selectedDetailCandidate == index;
                const std::string label = localizedUiLabel("Design") + " " + std::to_string(index + 1);
                ImGui::TextUnformatted(label.c_str());
                ImGui::TextColored(ImVec4(.35f, .78f, .66f, 1.0f), "%s · %d/100",
                                   localizedUiLabel("Valid").c_str(), candidate.quality.score);
                const auto version = (_controller._detailRequestSerial << 8u) ^ static_cast<std::uint64_t>(index + 1);
                if (drawAtlasDesignPreview(candidate.map, _renderPlanCache, version, "##CompareAtlasDesign", previewHeight, selected))
                    _controller._selectedDetailCandidate = index;
                const std::string selectLabel = std::string(selected
                    ? uiTextForLocale("Selected: ", "Gewählt: ")
                    : uiTextForLocale("Select: ", "Wählen: ")) + label;
                const float buttonWidth = std::max(60.0f, (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * .6f);
                if (drawUiButton(selectLabel.c_str(), selected ? UiButtonRole::Primary : UiButtonRole::Secondary,
                                 ImVec2(buttonWidth, 0.0f)))
                    _controller._selectedDetailCandidate = index;
                ImGui::SameLine();
                if (drawUiButton(uiTextForLocale("Enlarge", "Vergrößern"), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
                {
                    _controller._selectedDetailCandidate = index;
                    _enlargedDetailDesign = index;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::Separator();
    const float createWidth = std::min(ImGui::GetContentRegionAvail().x * .72f,
                                      ImGui::CalcTextSize(localizedUiLabel("Create selected detail map").c_str()).x + 32.0f);
    if (drawUiButton(localizedUiLabel("Create selected detail map").c_str(), UiButtonRole::Primary, ImVec2(createWidth, 0.0f)))
    {
        // This can reallocate campaign maps and clear the candidate vector.
        // No preview references are accessed after committing the selection.
        if (_controller.createSelectedDetailMap())
            ImGui::CloseCurrentPopup();
        else
            _detailDesignCreateFailed = true;
    }
    ImGui::SameLine();
    if (drawUiButton(uiTextForLocale("Close", "Schließen"), UiButtonRole::Secondary) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
