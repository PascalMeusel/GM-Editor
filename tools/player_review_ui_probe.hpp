#pragma once
#include "Controllers/Editor/EditorWindowController.hpp"
#include "Controllers/Editor/Widgets/GMScreen/GMScreenController.hpp"
#include "Views/Editor/EditorWindowView.hpp"
#include "Models/Session/PlayerReviewSession.hpp"
// Opens only the existing panel; review and confirmation use actual widgets.
struct PlayerReviewUiProbe
{
    static Campaign &campaign(EditorWindowController &e){return e._currentCampaign;}
    static void open(EditorWindowController &e){e.view->_presentationPanelOpen=true;}
    static void close(EditorWindowController &e){e.view->_presentationPanelOpen=false;}
    static void draw(EditorWindowController &e){e.view->renderPresentationPanel();}
    static void draw(GMScreenController &g){g._view->drawLocalPlayerDisplayControls();}
    static GMScreenController &gm(EditorWindowController &e){return *e._children.gmScreen;}
    static LocalPlayerDisplayController &display(EditorWindowController &e){return *e._children.localPlayerDisplay;}
    static NPCOverviewController &npc(EditorWindowController &e){return *e._children.npcOverview;}
    static ItemCreatorController &item(EditorWindowController &e){return *e._children.itemCreator;}
    static InventoryManagerController &inventory(EditorWindowController &e){return *e._children.inventoryManager;}
    static bool contextualPending(const EditorWindowController &e){return e._workspace.playerReviewTarget.has_value();}
    static auto route(const EditorWindowController &e){return std::make_pair(e.view->_activeWorkspace,e._workspace.navigation.current().editorRoute);}
    static PlayerReviewSession &owner(EditorWindowController &e){return e.view->_playerReviewSession;}
    static PlayerReviewSession &owner(GMScreenController &g){return g._view->_playerReviewSession;}
    static bool restore(EditorWindowController &e){return e.view->_hasPresentationRestoreSnapshot;}
    static bool restore(GMScreenController &g){return g._view->_hasPublicDisplayRestoreSnapshot;}
    static void runtimeMeasurements(EditorWindowController &e,PlayerDisplayReview::Measurements *m)
    {e._children.localPlayerDisplay->setReviewMeasurements(m);}
    static const PlayerDisplayReviewSnapshot &approved(const LocalPlayerDisplayController &d){return d._approvedReviewSnapshot;}
    static const PlayerDisplayReviewSnapshot &cached(const LocalPlayerDisplayController &d){return d._reviewCache;}
    static bool publicationProof(const LocalPlayerDisplayController &d){return d._approvedReviewSnapshotValid&&d._reviewCacheValid;}
};
