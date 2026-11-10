#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Audio/AudioPlaybackService.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Views/Editor/Widgets/Audio/AudioQuickControls/AudioQuickControlsView.hpp"
#include <filesystem>
#include <memory>
#include <string>

struct AudioQuickChannelSummary
{
    int music = 0;
    int ambience = 0;
    int effects = 0;
    int other = 0;
    int playing = 0;
    int paused = 0;
    std::string musicText;
    std::string ambienceText;
    std::string effectsText;
};

struct AudioQuickFileDiagnostics
{
    int missingAssets = 0;
    int repairableAssets = 0;
    bool selectedMissing = false;
    bool selectedRepairable = false;
    std::string selectedMessage;
};

struct AudioQuickPlaylistSummary
{
    bool active = false;
    std::string playlistName;
    std::string currentText;
    std::string previousText;
    std::string lastText;
    std::string nextText;
    std::string flagsText;
    int activeQueuedCount = 0;
    int queueSize = 0;
    bool transitionActive = false;
    std::string transitionText;
};

class AudioQuickControlsController : public IWidgetController
{
public:
    AudioQuickControlsController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService);

    void update(std::shared_ptr<GLFWwindow> window) override;
    void configure();

    const AudioAssetModel *getSelectedAsset() const;
    AudioAssetModel *getSelectedAsset();
    void selectAsset(int index);
    void playSelectedAsset();
    void retrySave();
    void pauseSelectedAsset();
    void resumeSelectedAsset();
    void stopSelectedAsset();
    void stopAllAudio();
    void playFirstAssetOfType(AudioAssetType type, bool loop);
    bool hasAssetOfType(AudioAssetType type) const;
    void pauseAllForBreak();
    void resumePausedAudio();
    void pauseChannel(const EntityId &channelId);
    void resumeChannel(const EntityId &channelId);
    void stopChannel(const EntityId &channelId);
    void applySelectedVolume();
    bool isSelectedAssetActive() const;
    bool isSelectedAssetPaused() const;

private:
    friend class AudioQuickControlsView;

    Campaign &_currentCampaign;
    std::shared_ptr<AudioPlaybackService> _playbackService;
    std::unique_ptr<AudioQuickControlsView> _view;

    int _selectedAssetIndex = -1;
    float _volume = 1.0f;
    bool _loop = false;
    std::string _statusMessage;
    std::string _errorMessage;
    bool _saveFailed = false;
    EntityId _saveFailureCampaignId;
    std::string _saveErrorMessage;

    const AudioAssetModel *findAudioAssetById(const EntityId &assetId) const;
    int firstAssetIndexOfType(AudioAssetType type) const;
    const AudioAmbienceLayerModel *findAmbienceLayerById(const EntityId &layerId) const;
    std::string channelDisplayName(const EntityId &channelId) const;
    AudioQuickChannelSummary channelSummary() const;
    AudioQuickFileDiagnostics fileDiagnostics() const;
    AudioQuickPlaylistSummary playlistSummary() const;
    bool ensureSelectedAssetFileAvailable(AudioAssetModel &asset);
    bool saveCampaign();
    void syncControlsFromSelection();
    void onShow() override;
};
