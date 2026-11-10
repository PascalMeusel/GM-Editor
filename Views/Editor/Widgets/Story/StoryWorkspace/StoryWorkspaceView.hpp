#pragma once

#include "Models/Story/StoryModels.hpp"
#include "Views/Editor/Widgets/DockableWidget.hpp"

class StoryWorkspaceController;

class StoryWorkspaceView : public DockableWidget
{
public:
    explicit StoryWorkspaceView(StoryWorkspaceController &controller);

protected:
    void draw() override;

private:
    StoryWorkspaceController &_controller;

    void drawThreadBrowser();
    void drawThreadEditor();
    void drawBeatBoard();
    void drawSceneChecklist(const StoryBeatModel &beat);
    void drawLinkEditor(bool forBeat);
    void drawLinks(bool forBeat);
    void drawThreadTypeCombo(const char *label, StoryThreadType &type);
    void drawStatusCombo(const char *label, StoryStatus &status);
    void drawLinkTypeCombo(const char *label, StoryLinkType &type);
};
