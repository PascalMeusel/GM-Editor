#include "Views/Editor/EditorPresentationSupport.hpp"
#include "Models/Session/LocalPlayerDisplayCameraPresets.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"

void EditorWindowView::renderPresentationPanel()
{
    if (!_presentationPanelOpen || _controller.currentCampaign().presentationModeActive)
    { _playerReviewSession.clear(); _presentationReviewAction = PresentationReviewAction::None;
      _controller._workspace.playerReviewTarget.reset(); _controller._workspace.playerReviewError.clear(); }
    if (_controller.currentCampaign().presentationModeActive)
    { _controller._children.gmScreen->resetPlayerDisplayReview(false); return; }
#include "Views/Editor/EditorPresentationPanelSetup.hpp"
#include "Views/Editor/EditorPresentationPanelContent.hpp"
}
