#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorWorkflowFrame.hpp"

using namespace gm::map_creator_view_detail;

MapCreatorView::MapCreatorView(MapCreatorController &controller)
    : DockableWidget("Map Creator", 0, ImVec2(980.0f, 720.0f)), _controller(controller)
{
}

MapCreatorView::~MapCreatorView()
{
    if (_setupPreviewCancel)
        _setupPreviewCancel->store(true);
}

void MapCreatorView::draw()
{
    if (_newMapReviewRequested)
    {
        ImGui::OpenPopup("##StartAnotherMap");
        _newMapReviewRequested = false;
    }
    if (ImGui::BeginPopupModal("##StartAnotherMap", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(uiTextForLocale("Create another map", "Eine weitere Karte erstellen"));
        ImGui::TextWrapped("%s: %s", uiTextForLocale("Unsaved draft", "Ungespeicherter Entwurf"),
                           _controller._draftMap.name.c_str());
        if (drawUiButton(uiTextForLocale("Save and start new", "Speichern und neu beginnen"), UiButtonRole::Primary))
        {
            if (_controller.flushDraft())
            {
                _controller.startNewMap(_newMapIntent);
                ImGui::CloseCurrentPopup();
            }
        }
        if (drawUiButton(uiTextForLocale("Keep editing", "Weiter bearbeiten"), UiButtonRole::Secondary))
            ImGui::CloseCurrentPopup();
        if (drawUiButton(uiTextForLocale("Discard draft and start new", "Entwurf verwerfen und neu beginnen"),
                         UiButtonRole::Destructive))
        {
            _controller.startNewMap(_newMapIntent);
            ImGui::CloseCurrentPopup();
        }
        if (_controller._persistence.saveFailed)
            ImGui::TextWrapped("%s", _controller.draftStatusMessage().c_str());
        ImGui::EndPopup();
    }
    const int currentLocale = static_cast<int>(gm::localization::LocalizationService::instance().locale());
    if (_lastStatusLocale >= 0 && _lastStatusLocale != currentLocale)
    {
        // Transient prose belongs to the language in which it was produced.
        // Persistent errors remain visible with a fresh, actionable translation.
        _controller._integration.statusMessage =
            _controller._persistence.saveFailed
                ? uiTextForLocale("Map changes are not saved. Retry saving.",
                                  "Kartenänderungen sind nicht gespeichert. Bitte erneut speichern.")
                : "";
        if (_controller._history.crashJournalWriteFailed)
        {
            if (!_controller._integration.statusMessage.empty())
                _controller._integration.statusMessage += " ";
            _controller._integration.statusMessage +=
                std::string(gm::localization::trUi("Map recovery snapshot failed:")) + " " +
                gm::localization::trUi("The latest recovery snapshot will be retried automatically.");
        }
    }
    _lastStatusLocale = currentLocale;
    MapModel &map = _controller._draftMap;
    const bool generationBusy = _controller.generationBusy();

    if (_dimensionDraftMapId != map.metadata.id)
    {
        _dimensionDraftMapId = map.metadata.id;
        _draftWidth = map.width;
        _draftHeight = map.height;
        _fitCanvasOnNextDraw = true;
        _showWorldPoliticalOverlay = false;
        _lastSelectionSummary.clear();
        _toolCategory = 0;
        _showCompactInspector = false;
        _pendingCanvasInspectorReveal = false;
        _inspectorContext.clear();
        _resetInspectorScroll = true;
        _pendingGeneratedFeatureProjectionId.clear();
        _hasPendingGeneratedFeatureProjection = false;
        _regenerationPickingArea = false;
        _regenerationAreaDragging = false;
        _regenerationAreaStartX = 0;
        _regenerationAreaStartY = 0;
        _regenerationAreaEndX = std::max(0, map.width - 1);
        _regenerationAreaEndY = std::max(0, map.height - 1);
        cancelImageTrace();
    }

    const bool authoring = gm::mapui::mapCreatorIsAuthoring(_workflowStep);
    // Decide before either the palette or table consumes height, so they can
    // never disagree at a responsive breakpoint in the same frame.
    const bool stackedEditLayout = gm::mapui::mapCreatorUsesStackedEditWorkspace(
        ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y, ImGui::GetFontSize());
    // Simple creation renders its complete workspace. Do not append the legacy
    // creation form below it or let both forms handle input in the same frame.
    if (drawWorkflowNavigation(map, generationBusy, authoring, stackedEditLayout))
        return;

    const bool showIdea = _workflowStep == 0;
    const bool showFacts = _workflowStep == 1;
    const bool showDesigns = _workflowStep == 2;
    const bool showEdit = _workflowStep == 3;
    const bool showExport = _workflowStep == 4;
    const bool showPlayerPreview = _workflowStep == 5;
    if (showEdit)
    {
        _controller.normalizeObjectSelection();
        const std::string context = inspectorContext();
        if (context != _inspectorContext)
        {
            _inspectorContext = context;
            _resetInspectorScroll = true;
            _sidebarPage = 0;
        }
    }
    const bool hasPersistentPrimaryAction = showIdea || showFacts || showExport || showPlayerPreview;
    const float persistentActionHeight =
        hasPersistentPrimaryAction ? ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.0f : 0.0f;

    const bool useStackedMapLayout = showEdit
        ? stackedEditLayout
        : gm::mapui::mapCreatorUsesStackedWorkspace(ImGui::GetContentRegionAvail().x,ImGui::GetTextLineHeightWithSpacing());
    WorkflowFrame frame;
    frame.map = &map;
    frame.generationBusy = generationBusy;
    frame.authoring = authoring;
    frame.showIdea = showIdea;
    frame.showFacts = showFacts;
    frame.showDesigns = showDesigns;
    frame.showEdit = showEdit;
    frame.showExport = showExport;
    frame.showPlayerPreview = showPlayerPreview;
    frame.hasPersistentPrimaryAction = hasPersistentPrimaryAction;
    frame.persistentActionHeight = persistentActionHeight;
    frame.useStackedMapLayout = useStackedMapLayout;
    drawWorkflowLayout(frame);
}
