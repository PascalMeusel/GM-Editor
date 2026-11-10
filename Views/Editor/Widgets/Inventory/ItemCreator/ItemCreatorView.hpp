#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"

class ItemCreatorController;

class ItemCreatorView : public DockableWidget
{
public:
    explicit ItemCreatorView(ItemCreatorController &controller);
    void requestEditorTab() { _openCustomEditor = true; }
    void requestCatalogBrowser() { _openCatalog = true; _openCustomEditor = false; }

protected:
    void draw() override;

private:
    friend struct ContextPreviewUiProbe;
    ItemCreatorController &_controller;
    bool _openCustomEditor = false;
    bool _openCatalog = false;
};
