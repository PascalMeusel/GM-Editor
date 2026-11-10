#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorWorkflowFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawWorkflowControlFooter(const WorkflowFrame &frame)
{
    MapModel &map = *frame.map;
    const bool generationBusy = frame.generationBusy;
    const bool authoring = frame.authoring;
    const bool showIdea = frame.showIdea;
    const bool showFacts = frame.showFacts;
    const bool showDesigns = frame.showDesigns;
    const bool showEdit = frame.showEdit;
    const bool showExport = frame.showExport;
    const bool showPlayerPreview = frame.showPlayerPreview;
    const bool hasPersistentPrimaryAction = frame.hasPersistentPrimaryAction;
    const float persistentActionHeight = frame.persistentActionHeight;
    const bool useStackedMapLayout = frame.useStackedMapLayout;
            if (!authoring && !_controller._integration.statusMessage.empty())
            {
                const std::string statusMessage = localizedUiLabel(_controller._integration.statusMessage.c_str());
                ImGui::TextWrapped("%s", statusMessage.c_str());
            }

            if (!showExport && !showEdit && !showPlayerPreview)
            {
                ImGui::Separator();
                if (drawUiButton("Save Map", !generationBusy, "Generation is already running.", UiButtonRole::Secondary,
                                 ImVec2(-1.0f, 0.0f)))
                    _controller.saveDraftMap();
            }
}
