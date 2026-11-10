#pragma once

#include "Models/Core/EntityMetadata.hpp"

class TabletopToolkitController;
enum class TabletopToolkitPendingDraft;

class TabletopToolkitView
{
public:
    explicit TabletopToolkitView(TabletopToolkitController &controller);
    void drawEmbedded();
    void requestObjectiveFocus(const EntityId &objectiveId);
    void requestClueFocus(const EntityId &clueId);
    void requestObjectivesTab();
    void requestCluesTab();

private:
    TabletopToolkitController &_controller;
    EntityId _focusedObjectiveId;
    EntityId _focusedClueId;
    bool _requestStoryTab = false;
    bool _requestObjectivesTab = false;
    bool _requestCluesTab = false;

    void drawDice();
    void drawStory();
    void drawObjectives();
    void drawClues();
    void drawSafety();
    void drawBoundaries();
    void drawWarnings();
    void drawDiscardPendingDraft(TabletopToolkitPendingDraft draft, const char *entityLabel);
};
