#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"
#include <string>

class BattlefieldCreatorController;

class BattlefieldCreatorView : public DockableWidget
{
public:
    explicit BattlefieldCreatorView(BattlefieldCreatorController &controller);

protected:
    void draw() override;

private:
    BattlefieldCreatorController &_controller;

    void drawControls();
    void drawCanvas();
    void finishUndoablePropertyEditIfIdle();
    bool undoableInputInt(const char *label, int &value, const char *undoLabel);
    bool undoableInputText(const char *label, std::string &value, const char *undoLabel);
    bool undoableInputTextMultiline(const char *label, std::string &value, const ImVec2 &size, const char *undoLabel);
};
