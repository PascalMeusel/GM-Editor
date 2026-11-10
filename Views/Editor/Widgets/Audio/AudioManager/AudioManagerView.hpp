#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"

class AudioManagerController;

class AudioManagerView : public DockableWidget
{
public:
    explicit AudioManagerView(AudioManagerController &controller);

protected:
    void draw() override;

private:
    AudioManagerController &_controller;
    bool _openAddAudioDialog = false;

    void drawManagerStatusStrip();
    void drawLibraryTab();
    void drawAddAudioDialog();
    void drawStatusMessages();
    void drawVolumeGroups();
    void drawImportControls();
    void drawBundledCatalog();
    void drawAssetList();
    void drawSelectedAssetControls();
    void drawPlaylistControls();
    void drawAmbienceLayers();
    void drawAudioTriggers();
    void drawActiveChannels();
};
