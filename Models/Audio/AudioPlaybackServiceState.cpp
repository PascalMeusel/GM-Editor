#include "AudioPlaybackService.hpp"

#include "AudioBackend.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace
{
constexpr float kFadeCommandIntervalSeconds = 1.0f / 30.0f;
constexpr float kMaximumFadeDurationSeconds = 60.0f;

float validVolume(float value, float fallback = 0.0f)
{
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : std::clamp(fallback, 0.0f, 1.0f);
}

float validFadeDuration(float value)
{
    if (!std::isfinite(value) || value <= 0.0f)
        return 0.0f;
    return std::min(value, kMaximumFadeDurationSeconds);
}
}

void AudioPlaybackService::stopAll()
{
    std::string ignored;
    for (const auto &entry : _aliasesByAssetId)
    {
        sendCommand("stop " + entry.second, ignored);
        sendCommand("close " + entry.second, ignored);
    }
    _aliasesByAssetId.clear();
    _pausedByAssetId.clear();
    _pausedPositionMsByAssetId.clear();
    _loopByAssetId.clear();
    _volumeByAssetId.clear();
    _fadeByAssetId.clear();
    clearPlaylistPlaybackState();
}

AudioPlaybackBatchResult AudioPlaybackService::pausePlaying(std::string &errorMessage)
{
    const std::vector<EntityId> activeIds = activeAssetIds();
    AudioPlaybackBatchResult result;
    result.considered = static_cast<int>(activeIds.size());
    errorMessage.clear();

    for (const EntityId &assetId : activeIds)
    {
        if (isPaused(assetId))
            continue;
        if (pause(assetId, errorMessage))
            ++result.changed;
        else
            break;
    }

    return result;
}

AudioPlaybackBatchResult AudioPlaybackService::resumePaused(std::string &errorMessage)
{
    const std::vector<EntityId> activeIds = activeAssetIds();
    AudioPlaybackBatchResult result;
    result.considered = static_cast<int>(activeIds.size());
    errorMessage.clear();

    for (const EntityId &assetId : activeIds)
    {
        if (!isPaused(assetId))
            continue;
        if (resume(assetId, errorMessage))
            ++result.changed;
        else
            break;
    }

    return result;
}

void AudioPlaybackService::setPlaylistPlaybackState(AudioPlaylistPlaybackState state)
{
    state.active = true;
    _playlistState = std::move(state);
    refreshPlaylistStateFromActiveAssets();
}

void AudioPlaybackService::clearPlaylistPlaybackState()
{
    _playlistState = {};
}

AudioPlaylistPlaybackState AudioPlaybackService::playlistPlaybackState() const
{
    return _playlistState;
}

bool AudioPlaybackService::isActive(const EntityId &assetId) const
{
    return _aliasesByAssetId.find(assetId) != _aliasesByAssetId.end();
}

bool AudioPlaybackService::isPaused(const EntityId &assetId) const
{
    const auto it = _pausedByAssetId.find(assetId);
    return it != _pausedByAssetId.end() && it->second;
}

bool AudioPlaybackService::loopEnabled(const EntityId &assetId) const
{
    const auto it = _loopByAssetId.find(assetId);
    return it != _loopByAssetId.end() && it->second;
}

float AudioPlaybackService::volume(const EntityId &assetId) const
{
    const auto it = _volumeByAssetId.find(assetId);
    return it == _volumeByAssetId.end() ? 0.0f : it->second;
}

bool AudioPlaybackService::isFading(const EntityId &assetId) const
{
    return _fadeByAssetId.find(assetId) != _fadeByAssetId.end();
}

std::size_t AudioPlaybackService::activeFadeCount() const
{
    return _fadeByAssetId.size();
}

int AudioPlaybackService::durationSeconds(const EntityId &assetId) const
{
    const auto it = _aliasesByAssetId.find(assetId);
    if (it == _aliasesByAssetId.end())
        return 0;

    std::string lengthText;
    std::string ignored;
    if (!queryCommand("status " + it->second + " length", lengthText, ignored))
        return 0;

    const int milliseconds = std::max(0, std::atoi(lengthText.c_str()));
    return (milliseconds + 999) / 1000;
}

std::vector<EntityId> AudioPlaybackService::activeAssetIds() const
{
    std::vector<EntityId> ids;
    ids.reserve(_aliasesByAssetId.size());
    for (const auto &entry : _aliasesByAssetId)
        ids.push_back(entry.first);
    return ids;
}

AudioPlaybackCacheStats AudioPlaybackService::cacheStats() const
{
    AudioPlaybackCacheStats stats;
    stats.activeAliases = _aliasesByAssetId.size();
    stats.pausedAssets = _pausedPositionMsByAssetId.size();
    return stats;
}

std::string AudioPlaybackService::aliasForAsset(const EntityId &assetId)
{
    std::string alias = "audio_";
    for (const char c : assetId)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
            alias.push_back(c);
        else
            alias.push_back('_');
    }
    return alias;
}

void AudioPlaybackService::refreshPlaylistStateFromActiveAssets()
{
    if (!_playlistState.active)
        return;

    int activeQueuedCount = 0;
    for (const EntityId &assetId : _playlistState.queueAssetIds)
    {
        if (isActive(assetId))
            ++activeQueuedCount;
    }
    _playlistState.activeQueuedCount = activeQueuedCount;

    if (!_playlistState.currentAssetId.empty() && !isActive(_playlistState.currentAssetId))
        _playlistState.currentIndex = -1;
}

void AudioPlaybackService::finishPlaylistTransitionIfIdle()
{
    if (!_fadeByAssetId.empty() || !_playlistState.transitionActive)
        return;
    _playlistState.transitionActive = false;
    _playlistState.transitionLabel.clear();
}

bool AudioPlaybackService::scheduleFade(const EntityId &assetId,
                                        float startVolume,
                                        float targetVolume,
                                        float durationSeconds,
                                        bool stopAfterFade,
                                        std::string &errorMessage)
{
    if (!isActive(assetId))
    {
        errorMessage = "No active audio asset is selected.";
        return false;
    }

    const float currentVolume = volume(assetId);
    const float clampedStart = validVolume(startVolume, currentVolume);
    const float clampedTarget = validVolume(targetVolume, currentVolume);
    const float clampedDuration = validFadeDuration(durationSeconds);
    _fadeByAssetId.erase(assetId);

    if (!applyVolume(assetId, clampedStart, errorMessage))
        return false;

    if (clampedDuration <= 0.0f || std::abs(clampedStart - clampedTarget) <= 0.0001f)
    {
        if (!applyVolume(assetId, clampedTarget, errorMessage))
            return false;
        if (stopAfterFade)
            return stop(assetId, errorMessage);
        finishPlaylistTransitionIfIdle();
        return true;
    }

    FadeTransition transition;
    transition.startVolume = clampedStart;
    transition.targetVolume = clampedTarget;
    transition.durationSeconds = clampedDuration;
    transition.stopAfterFade = stopAfterFade;
    _fadeByAssetId[assetId] = transition;
    errorMessage.clear();
    return true;
}

bool AudioPlaybackService::sendCommand(const std::string &command, std::string &errorMessage) const
{
    if (!_backend)
    {
        errorMessage = "Audio playback backend is unavailable.";
        return false;
    }
    return _backend->sendCommand(command, errorMessage);
}

bool AudioPlaybackService::queryCommand(const std::string &command, std::string &result, std::string &errorMessage) const
{
    if (!_backend)
    {
        result.clear();
        errorMessage = "Audio playback backend is unavailable.";
        return false;
    }
    return _backend->queryCommand(command, result, errorMessage);
}

