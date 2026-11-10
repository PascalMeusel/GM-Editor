#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::cancelImageTrace()
{
    _imageTraceActive = false;
    _imageTraceControlPoints.clear();
    _imageTraceDraggedPoint = -1;
}

void MapCreatorView::drawImageTraceControls()
{
    MapModel &map = _controller._draftMap;
    if (!map.importedReference.enabled)
    {
        cancelImageTrace();
        return;
    }

    if (_imageTraceActive || !map.importedReference.semanticallyTraced)
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (!ImGui::CollapsingHeader(localizedUiLabel("Trace map image").c_str()))
        return;

    ImGui::TextWrapped(
        "%s",
        localizedUiLabel("Click the imported image to add control points. Drag a point to correct it before finishing.")
            .c_str());

    ImGui::BeginDisabled(_imageTraceActive);
    const int previousMode = _imageTraceModeIndex;
    FormUi::Combo(localizedUiLabel("Feature type").c_str(), &_imageTraceModeIndex,
                  localizedComboItems(imageTraceModeItems).c_str());
    if (_imageTraceModeIndex != previousMode)
        _imageTraceName = localizedUiLabel(imageTraceDefaultName(imageTraceMode(_imageTraceModeIndex)));
    ImGui::EndDisabled();

    if (_imageTraceName.empty())
        _imageTraceName = localizedUiLabel(imageTraceDefaultName(imageTraceMode(_imageTraceModeIndex)));
    FormUi::InputText(localizedUiLabel("Feature name").c_str(), &_imageTraceName);
    FormUi::Checkbox(localizedUiLabel("Snap control points").c_str(), &_imageTraceSnap);
    const MapImageTraceAuthoring::Mode mode = imageTraceMode(_imageTraceModeIndex);
    if (MapImageTraceAuthoring::isLine(mode) || MapImageTraceAuthoring::isClosed(mode))
    {
        ImGui::SameLine();
        FormUi::Checkbox(localizedUiLabel("Smooth shape").c_str(), &_imageTraceSmooth);
    }
    FormUi::Checkbox(localizedUiLabel("GM only").c_str(), &_imageTraceGmOnly);

    if (!_imageTraceActive)
    {
        if (drawUiButton("Start tracing", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                         "Start a focused canvas mode. Existing map content is not changed until you finish."))
        {
            _controller.cancelActiveTool();
            _regenerationPickingArea = false;
            _regenerationAreaDragging = false;
            _imageTraceControlPoints.clear();
            _imageTraceDraggedPoint = -1;
            _imageTraceActive = true;
        }
        return;
    }

    const std::size_t required = MapImageTraceAuthoring::minimumPointCount(mode);
    const std::string progress =
        formatLocalizedUi("Control points: %d / at least %d", static_cast<int>(_imageTraceControlPoints.size()),
                          static_cast<int>(required));
    ImGui::TextWrapped("%s", progress.c_str());
    ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.66f, 1.0f), "%s",
                       localizedUiLabel("Tracing is active. Other editing tools are paused.").c_str());

    const UiButtonPairResult pointActions = drawResponsiveButtonPair(
        "MapImageTracePointActions", "Undo last point", UiButtonRole::Secondary,
        "Remove the most recently placed control point.", "Clear points", UiButtonRole::Secondary,
        "Remove all uncommitted control points.", !_imageTraceControlPoints.empty(), "Place a control point first.");
    if (pointActions.first && !_imageTraceControlPoints.empty())
    {
        _imageTraceControlPoints.pop_back();
        _imageTraceDraggedPoint = -1;
    }
    if (pointActions.second)
    {
        _imageTraceControlPoints.clear();
        _imageTraceDraggedPoint = -1;
    }

    const bool canFinish = _imageTraceControlPoints.size() >= required;
    if (drawUiButton("Finish traced feature", canFinish, "Add the required control points first.",
                     UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Add this semantic feature as one undoable map edit."))
    {
        const MapImageImport::TraceRequest request = MapImageTraceAuthoring::makeRequest(
            mode, _imageTraceName, _imageTraceControlPoints, _imageTraceSmooth, _imageTraceGmOnly);
        if (_controller.commitImageTrace(request))
            cancelImageTrace();
    }
    if (drawUiButton("Cancel tracing", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                     "Discard these uncommitted points without changing the map."))
        cancelImageTrace();
}
