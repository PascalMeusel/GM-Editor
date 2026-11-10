#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"

class PlayerOverviewController;

class PlayerOverviewView : public DockableWidget
{
public:
    explicit PlayerOverviewView(PlayerOverviewController &controller);

protected:
    void draw() override;

private:
    PlayerOverviewController &_controller;
};
