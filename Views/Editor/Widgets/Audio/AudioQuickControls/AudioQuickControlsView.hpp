#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"

class AudioQuickControlsController;
struct AudioQuickChannelSummary;
struct AudioQuickPlaylistSummary;

class AudioQuickControlsView : public DockableWidget
{
public:
    explicit AudioQuickControlsView(AudioQuickControlsController &controller);

protected:
    void draw() override;

private:
    AudioQuickControlsController &_controller;

    void drawActivePlaybackActions(const AudioQuickChannelSummary &summary);
    void drawPlaylistSnapshot(const AudioQuickPlaylistSummary &summary);
    void drawActiveChannels();
};
