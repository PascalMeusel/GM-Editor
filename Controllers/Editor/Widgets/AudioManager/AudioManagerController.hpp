#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Audio/AudioCatalog.hpp"
#include "Models/Audio/AudioPlaybackService.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Views/Editor/Widgets/Audio/AudioManager/AudioManagerView.hpp"
#include "Views/Editor/Widgets/Project/ProjectAssetRepairPanel.hpp"
#include <filesystem>
#include <memory>
#include <string>

class AudioManagerController : public IWidgetController
{
public:
    AudioManagerController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService);

    void update(std::shared_ptr<GLFWwindow> window) override;
    void configure();
    void retrySave();

    AudioAssetModel *getSelectedAsset();
    const AudioAssetModel *getSelectedAsset() const;
    void selectAsset(int index);
    void importAsset();
    void browseForAudioFile();
    void relinkSelectedAsset();
    void relinkSelectedAssetFromPath(const std::string &sourcePath);
    void saveSelectedAssetSettings();
    void removeSelectedAsset();
    void playSelectedAsset();
    void fadeInSelectedAsset();
    void fadeOutSelectedAsset();
    void crossfadeToSelectedAsset();
    void pauseActiveAsset();
    void resumeActiveAsset();
    void stopActiveAsset();
    void stopActiveChannel(const EntityId &assetId);
    void stopAllActiveAudio();
    void applySelectedVolume();
    void addCatalogAsset(int index);
    bool catalogAssetAlreadyAdded(int index) const;
    const std::string &statusMessage() const { return _statusMessage; }
    AudioPlaylistModel *getSelectedPlaylist();
    const AudioPlaylistModel *getSelectedPlaylist() const;
    void selectPlaylist(int index);
    void createPlaylist();
    void saveSelectedPlaylist();
    void removeSelectedPlaylist();
    void addSelectedAssetToPlaylist();
    void removeAssetFromSelectedPlaylist(int index);
    void moveSelectedPlaylistAsset(int index, int direction);
    void playSelectedPlaylist();
    void playSelectedPlaylistOneShot();
    void playPreviousQueuedAsset();
    void playNextQueuedAsset();
    void stopPlaylistQueue();
    void sceneTransitionToSelectedPlaylist();
    const AudioAssetModel *getCurrentQueuedAsset() const;
    const AudioAssetModel *getPreviousQueuedAsset() const;
    const AudioAssetModel *getNextQueuedAsset() const;
    const AudioAssetModel *getLastQueuedAsset() const;
    int activePlaylistQueueCount() const;
    const AudioAssetModel *findAudioAssetById(const EntityId &assetId) const;
    AudioAssetModel *findAudioAssetById(const EntityId &assetId);
    const AudioAmbienceLayerModel *findAmbienceLayerById(const EntityId &layerId) const;
    void addAmbienceLayer();
    void saveAmbienceLayer(int index);
    void removeAmbienceLayer(int index);
    void playAmbienceLayer(int index);
    void stopAmbienceLayer(int index);
    void applyAmbienceLayerVolume(int index);
    void saveVolumeGroups();
    void addAudioTrigger();
    void saveAudioTrigger(int index);
    void removeAudioTrigger(int index);
    void fireAudioTrigger(int index);
    std::filesystem::path resolveAudioPath(const AudioAssetModel &asset) const;

private:
    friend class AudioManagerView;

    Campaign &_currentCampaign;
    std::unique_ptr<AudioManagerView> _view;
    std::shared_ptr<AudioPlaybackService> _playbackService;

    std::string _importSourcePath;
    std::string _importName;
    AudioAssetType _importType = AudioAssetType::Music;
    float _importVolume = 1.0f;
    bool _importLoop = true;
    int _selectedAssetIndex = -1;
    float _selectedVolume = 1.0f;
    bool _selectedLoop = false;
    float _fadeSeconds = 2.0f;
    int _selectedPlaylistIndex = -1;
    std::string _newPlaylistName = "New Playlist";
    std::vector<EntityId> _newPlaylistAssets;
    std::string _newLayerName;
    EntityId _newLayerAssetId,_newTriggerSourceId,_newTriggerAssetId;
    float _newLayerVolume=.6f,_newTriggerVolume=.7f;
    bool _newLayerLoop=true;
    std::string _newTriggerName = "New Audio Trigger";
    std::string _newTriggerSourceType = "StoryBeat";
    std::string _newTriggerEventName = "Scene Started";
    std::vector<EntityId> _playlistQueue;
    int _queuedAssetIndex = -1;
    EntityId _lastQueuedAssetId;
    bool _catalogLoaded = false;
    std::vector<AudioCatalogEntry> _catalogEntries;
    std::string _catalogSearch;
    std::string _statusMessage;
    std::string _errorMessage;
    bool _saveFailed = false;
    bool _importPendingSave = false;
    EntityId _saveFailureCampaignId;
    std::string _saveErrorMessage;
    ProjectAssetRepairPanel::AssetRepairPanelState _assetRepairPanelState;

    void loadBundledCatalog();
    bool catalogEntryMatchesFilter(const AudioCatalogEntry &entry) const;
    bool ensureSelectedAssetFileAvailable(AudioAssetModel &asset);
    bool repairSelectedAssetFromPath(AudioAssetModel &asset, const std::filesystem::path &sourcePath);
    void rebuildPlaylistQueue(bool oneShotOverride);
    bool playQueuedAssetAt(int queueIndex, bool oneShotOverride);
    void publishPlaylistPlaybackState(const AudioPlaylistModel &playlist, bool transitionActive = false, const std::string &transitionLabel = "", bool oneShotActive = false);
    bool saveCampaign();
    void syncSelectedControlsFromAsset();
    void onShow() override;
};
