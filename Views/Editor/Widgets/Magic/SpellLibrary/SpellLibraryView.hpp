#pragma once

#include "Models/Magic/SpellModels.hpp"
#include "Views/Editor/Widgets/DockableWidget.hpp"

class SpellLibraryController;

class SpellLibraryView : public DockableWidget
{
public:
    explicit SpellLibraryView(SpellLibraryController &controller);
    void requestEditorTab() { _openCustomEditor = true; }
    void requestImportTab() { _openImport = true; _openCustomEditor = false; }
    void requestCatalogTab() { _openCatalog = true; _openCustomEditor = false; }

protected:
    void draw() override;

private:
    SpellLibraryController &_controller;
    bool _openCustomEditor = false;
    bool _openImport = false, _openCatalog = false;

    void drawCatalogBrowser();
    void drawImportPanel();
    void drawSpellList();
    void drawSpellEditor();
    void drawSchoolCombo(const char *label, SpellSchool &school);
    void drawResolutionCombo(const char *label, SpellResolutionType &resolution);
};
