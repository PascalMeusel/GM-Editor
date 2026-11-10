#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"

class CharacterSheetController;

class CharacterSheetView : public DockableWidget
{
public:
    explicit CharacterSheetView(CharacterSheetController &controller);
    ~CharacterSheetView() = default;

protected:
    void draw() override;

private:
    friend struct SpellOverrideUiProbe;
    CharacterSheetController &_controller;
    bool _showUnusedSpellLevels = false;
    void drawAdvancement(bool open);
};
