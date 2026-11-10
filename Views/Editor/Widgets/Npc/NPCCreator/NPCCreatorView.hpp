#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"

class NPCCreatorController;

class NPCCreatorView : public DockableWidget
{
public:
    explicit NPCCreatorView(NPCCreatorController &controller);

protected:
    void draw() override;

private:
    NPCCreatorController &_controller;
};
