#include "Views/Editor/EditorViewDetails.hpp"

using namespace gm::editor_view_detail;

EditorWindowView::EditorWindowView(EditorWindowController &controller) : _controller(controller)
{
}

void EditorWindowView::update(std::shared_ptr<GLFWwindow> window)
{
    renderWindowSelector();
    if (_activeWorkspace == EditorWorkspace::Home)
        renderCampaignHome();
    else
        renderWorkspaceLanding();
    renderPresentationPanel();
    renderUniversalSearch();
    renderGlobalSearch();
    renderKnowledgeGraph();
    renderCommandPalette();
    renderEditorDraftRecovery();
    renderToasts();
    checkpointWorldRecovery();
}
