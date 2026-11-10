#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"
#include <filesystem>

class NPCOverviewController;

class NPCOverviewView : public DockableWidget
{
public:
    explicit NPCOverviewView(NPCOverviewController &controller);
    void resetReadingContext();

protected:
    void draw() override;

private:
    friend struct ContextPreviewUiProbe;
    NPCOverviewController &_controller;

    std::string _quickSearch;
    int _detailSection = 0;
    std::string _readingNpcId, _readingCampaignId;
    std::filesystem::path _readingRoot;
    bool _reading = false, _secretsRevealed = false;
    void reconcileReadingContext();
    void drawNpcReading();
    void drawNpcTab();
    void drawMonsterTab();
    void drawFactionTab();
    void drawDeityTab();
};
