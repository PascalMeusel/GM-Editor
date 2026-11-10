#pragma once

#include "Models/Audio/AudioModels.hpp"
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

class AudioCommandBackend;

struct AudioPlaybackCacheStats
{
    std::size_t activeAliases = 0;
    std::size_t pausedAssets = 0;
};

struct AudioPlaybackBatchResult
{
    int considered = 0;
    int changed = 0;
};

struct AudioPlaylistPlaybackState
{
    bool active = false;
    EntityId playlistId;
    std::string playlistName;
    std::vector<EntityId> queueAssetIds;
    EntityId currentAssetId;
    EntityId previousAssetId;
    EntityId lastAssetId;
    EntityId nextAssetId;
    int currentIndex = -1;
    int activeQueuedCount = 0;
    bool shuffle = false;
    bool repeat = false;
    bool oneShot = false;
    bool transitionActive = false;
    std::string transitionLabel;
};

class AudioPlaybackService
{
public:
    AudioPlaybackService();
    explicit AudioPlaybackService(std::unique_ptr<AudioCommandBackend> backend);
    ~AudioPlaybackService();

    bool play(AudioAssetModel &asset, const std::filesystem::path &resolvedPath, float volume, bool loop, std::string &errorMessage);
    bool fadeIn(AudioAssetModel &asset, const std::filesystem::path &resolvedPath, float targetVolume, bool loop, float durationSeconds, std::string &errorMessage);
    bool fadeOut(const EntityId &assetId, float durationSeconds, bool stopAfterFade, std::string &errorMessage);
    bool crossfade(const EntityId &fromAssetId, AudioAssetModel &toAsset, const std::filesystem::path &toResolvedPath, float targetVolume, bool loop, float durationSeconds, std::string &errorMessage);
    bool pause(const EntityId &assetId, std::string &errorMessage);
    bool resume(const EntityId &assetId, std::string &errorMessage);
    bool stop(const EntityId &assetId, std::string &errorMessage);
    bool setVolume(const EntityId &assetId, float volume, std::string &errorMessage);
    // Advances scheduled fades without blocking the UI thread. Call once per
    // editor frame with that frame's elapsed time.
    bool update(float deltaSeconds, std::string &errorMessage);
    void stopAll();
    AudioPlaybackBatchResult pausePlaying(std::string &errorMessage);
    AudioPlaybackBatchResult resumePaused(std::string &errorMessage);
    void setPlaylistPlaybackState(AudioPlaylistPlaybackState state);
    void clearPlaylistPlaybackState();
    AudioPlaylistPlaybackState playlistPlaybackState() const;
    bool isActive(const EntityId &assetId) const;
    bool isPaused(const EntityId &assetId) const;
    bool loopEnabled(const EntityId &assetId) const;
    float volume(const EntityId &assetId) const;
    bool isFading(const EntityId &assetId) const;
    std::size_t activeFadeCount() const;
    int durationSeconds(const EntityId &assetId) const;
    std::vector<EntityId> activeAssetIds() const;
    AudioPlaybackCacheStats cacheStats() const;

private:
    struct FadeTransition
    {
        float startVolume = 0.0f;
        float targetVolume = 0.0f;
        float durationSeconds = 0.0f;
        float elapsedSeconds = 0.0f;
        float secondsSinceCommand = 0.0f;
        bool stopAfterFade = false;
    };

    std::map<EntityId, std::string> _aliasesByAssetId;
    std::map<EntityId, bool> _pausedByAssetId;
    std::map<EntityId, int> _pausedPositionMsByAssetId;
    std::map<EntityId, bool> _loopByAssetId;
    std::map<EntityId, float> _volumeByAssetId;
    std::map<EntityId, FadeTransition> _fadeByAssetId;
    AudioPlaylistPlaybackState _playlistState;
    std::unique_ptr<AudioCommandBackend> _backend;

    std::string aliasForAsset(const EntityId &assetId);
    void refreshPlaylistStateFromActiveAssets();
    void finishPlaylistTransitionIfIdle();
    bool scheduleFade(const EntityId &assetId, float startVolume, float targetVolume, float durationSeconds, bool stopAfterFade, std::string &errorMessage);
    bool applyVolume(const EntityId &assetId, float volume, std::string &errorMessage);
    bool sendCommand(const std::string &command, std::string &errorMessage) const;
    bool queryCommand(const std::string &command, std::string &result, std::string &errorMessage) const;
};
