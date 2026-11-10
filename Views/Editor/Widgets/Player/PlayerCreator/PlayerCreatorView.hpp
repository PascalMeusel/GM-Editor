#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"

class PlayerCreatorController;

class PlayerCreatorView : public DockableWidget
{
public:
    explicit PlayerCreatorView(PlayerCreatorController &controller);

protected:
    void draw() override;

private:
    PlayerCreatorController &_controller;
    int _lastStep = -1;
};
