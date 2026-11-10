#include "Controllers/Editor/EditorWindowControllerSupport.hpp"

const std::string &EditorWindowController::shortcutStatusMessage() const
{
    if(!_editorDraftRecoveryStatus.empty()&&_shortcuts.statusMessage==_editorDraftRecoveryStatus)
        return _editorDraftRecovery->error(); // Resolve owned status even after Retry clears its error.
    if(!view->_worldRecovery.error.empty()&&_shortcuts.statusMessage==view->_worldRecovery.error)
        return worldRecoveryError();
    return _shortcuts.statusMessage;
}

bool EditorWindowController::checkpointEditorDraftRecoveryForLeave()
{
    const bool result=_editorDraftRecovery->checkpoint(true);
    if(!result)_shortcuts.statusMessage=_editorDraftRecoveryStatus=_editorDraftRecovery->error();
    return result;
}

bool EditorWindowController::checkpointWorldRecoveryForLeave()
{
    const bool result=view->checkpointWorldRecovery(true);
    if(!result)_shortcuts.statusMessage=view->_worldRecovery.error;
    return result;
}
const std::string &EditorWindowController::worldRecoveryError() const
{
    if(_currentCampaign.presentationModeActive&&!view->_worldRecovery.error.empty()) {
        static const std::string en="Private World recovery needs attention. Return to GM mode to review it.";
        static const std::string de="Die private Weltsicherung benötigt Aufmerksamkeit. Prüfe sie im SL-Modus.";
        return gm::localization::LocalizationService::instance().locale()==gm::localization::Locale::German?de:en;
    }
    return view->_worldRecovery.error;
}
