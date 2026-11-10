#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"

class InventoryManagerController;

class InventoryManagerView : public DockableWidget
{
public:
    explicit InventoryManagerView(InventoryManagerController &controller);

protected:
    void draw() override;

private:
    friend struct ContextPreviewUiProbe;
    InventoryManagerController &_controller;
};
