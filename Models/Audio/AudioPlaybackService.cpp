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

AudioPlaybackService::AudioPlaybackService()
    : _backend(createAudioCommandBackend())
{
}

AudioPlaybackService::AudioPlaybackService(std::unique_ptr<AudioCommandBackend> backend)
    : _backend(std::move(backend))
{
}

AudioPlaybackService::~AudioPlaybackService()
{
    stopAll();
}

bool AudioPlaybackService::play(AudioAssetModel &asset, const std::filesystem::path &resolvedPath, float volume, bool loop, std::string &errorMessage)
{
    if (!std::filesystem::exists(resolvedPath))
    {
        asset.missingFile = true;
        asset.metadata.touch();
        errorMessage = "Audio file does not exist: " + resolvedPath.string();
        return false;
    }
    asset.missingFile = false;

    const std::string alias = aliasForAsset(asset.metadata.id);
    std::string ignored;
    sendCommand("stop " + alias, ignored);
    sendCommand("close " + alias, ignored);
    _fadeByAssetId.erase(asset.metadata.id);
    _aliasesByAssetId.erase(asset.metadata.id);
    _pausedByAssetId.erase(asset.metadata.id);
    _pausedPositionMsByAssetId.erase(asset.metadata.id);
    _loopByAssetId.erase(asset.metadata.id);
    _volumeByAssetId.erase(asset.metadata.id);

    const std::string path = resolvedPath.string();
    if (!sendCommand("open \"" + path + "\" alias " + alias, errorMessage))
        return false;

    const float initialVolume = validVolume(volume);
    _aliasesByAssetId[asset.metadata.id] = alias;
    _pausedByAssetId[asset.metadata.id] = false;
    _pausedPositionMsByAssetId.erase(asset.metadata.id);
    _loopByAssetId[asset.metadata.id] = loop;
    _volumeByAssetId[asset.metadata.id] = initialVolume;

    sendCommand("set " + alias + " time format milliseconds", ignored);
    sendCommand("setaudio " + alias + " volume to " + std::to_string(std::clamp(static_cast<int>(initialVolume * 1000.0f), 0, 1000)), ignored);

    const std::string playCommand = "play " + alias + (loop ? " repeat" : "");
    if (!sendCommand(playCommand, errorMessage))
    {
        sendCommand("close " + alias, ignored);
        _aliasesByAssetId.erase(asset.metadata.id);
        _pausedByAssetId.erase(asset.metadata.id);
        _loopByAssetId.erase(asset.metadata.id);
        _volumeByAssetId.erase(asset.metadata.id);
        return false;
    }

    asset.lastPlayedAtUtc = currentUtcTimestamp();
    const int knownDurationSeconds = durationSeconds(asset.metadata.id);
    if (knownDurationSeconds > 0)
        asset.durationSeconds = knownDurationSeconds;
    asset.metadata.touch();
    return true;
}

bool AudioPlaybackService::fadeIn(AudioAssetModel &asset, const std::filesystem::path &resolvedPath, float targetVolume, bool loop, float durationSeconds, std::string &errorMessage)
{
    if (!play(asset, resolvedPath, 0.0f, loop, errorMessage))
        return false;

    return scheduleFade(asset.metadata.id, 0.0f, validVolume(targetVolume), durationSeconds, false, errorMessage);
}

bool AudioPlaybackService::fadeOut(const EntityId &assetId, float durationSeconds, bool stopAfterFade, std::string &errorMessage)
{
    const auto volumeIt = _volumeByAssetId.find(assetId);
    const float startVolume = volumeIt == _volumeByAssetId.end() ? 1.0f : volumeIt->second;
    return scheduleFade(assetId, startVolume, 0.0f, durationSeconds, stopAfterFade, errorMessage);
}

bool AudioPlaybackService::crossfade(const EntityId &fromAssetId, AudioAssetModel &toAsset, const std::filesystem::path &toResolvedPath, float targetVolume, bool loop, float durationSeconds, std::string &errorMessage)
{
    const auto fromVolumeIt = _volumeByAssetId.find(fromAssetId);
    const float fromStartVolume = fromVolumeIt == _volumeByAssetId.end() ? 1.0f : fromVolumeIt->second;
    const float toTargetVolume = validVolume(targetVolume);
    if (!play(toAsset, toResolvedPath, 0.0f, loop, errorMessage))
        return false;

    if (fromAssetId == toAsset.metadata.id || !isActive(fromAssetId))
        return scheduleFade(toAsset.metadata.id, 0.0f, toTargetVolume, durationSeconds, false, errorMessage);

    if (!scheduleFade(fromAssetId, fromStartVolume, 0.0f, durationSeconds, true, errorMessage))
    {
        std::string ignored;
        stop(toAsset.metadata.id, ignored);
        return false;
    }

    if (scheduleFade(toAsset.metadata.id, 0.0f, toTargetVolume, durationSeconds, false, errorMessage))
        return true;

    // Keep the original channel usable if starting the second half ever fails.
    _fadeByAssetId.erase(fromAssetId);
    std::string ignored;
    applyVolume(fromAssetId, fromStartVolume, ignored);
    stop(toAsset.metadata.id, ignored);
    return false;
}

bool AudioPlaybackService::pause(const EntityId &assetId, std::string &errorMessage)
{
    const auto it = _aliasesByAssetId.find(assetId);
    if (it == _aliasesByAssetId.end())
    {
        errorMessage = "No active audio asset is selected.";
        return false;
    }

    std::string positionText;
    int pausedPositionMs = 0;
    if (queryCommand("status " + it->second + " position", positionText, errorMessage))
        pausedPositionMs = std::max(0, std::atoi(positionText.c_str()));

    if (!sendCommand("stop " + it->second, errorMessage))
        return false;

    _pausedPositionMsByAssetId[assetId] = pausedPositionMs;
    _pausedByAssetId[assetId] = true;
    return true;
}

bool AudioPlaybackService::resume(const EntityId &assetId, std::string &errorMessage)
{
    const auto it = _aliasesByAssetId.find(assetId);
    if (it == _aliasesByAssetId.end())
    {
        errorMessage = "No paused audio asset is selected.";
        return false;
    }

    const auto positionIt = _pausedPositionMsByAssetId.find(assetId);
    const int pausedPositionMs = positionIt == _pausedPositionMsByAssetId.end() ? 0 : positionIt->second;
    if (!sendCommand("seek " + it->second + " to " + std::to_string(pausedPositionMs), errorMessage))
        return false;

    const bool loop = _loopByAssetId.find(assetId) != _loopByAssetId.end() && _loopByAssetId[assetId];
    if (!sendCommand("play " + it->second + (loop ? " repeat" : ""), errorMessage))
        return false;

    _pausedPositionMsByAssetId.erase(assetId);
    _pausedByAssetId[assetId] = false;
    return true;
}

bool AudioPlaybackService::stop(const EntityId &assetId, std::string &errorMessage)
{
    _fadeByAssetId.erase(assetId);
    const auto it = _aliasesByAssetId.find(assetId);
    if (it == _aliasesByAssetId.end())
    {
        finishPlaylistTransitionIfIdle();
        return true;
    }

    std::string ignored;
    sendCommand("stop " + it->second, ignored);
    sendCommand("close " + it->second, ignored);
    _aliasesByAssetId.erase(it);
    _pausedByAssetId.erase(assetId);
    _pausedPositionMsByAssetId.erase(assetId);
    _loopByAssetId.erase(assetId);
    _volumeByAssetId.erase(assetId);
    refreshPlaylistStateFromActiveAssets();
    finishPlaylistTransitionIfIdle();
    errorMessage.clear();
    return true;
}

bool AudioPlaybackService::setVolume(const EntityId &assetId, float volume, std::string &errorMessage)
{
    _fadeByAssetId.erase(assetId);
    const bool changed = applyVolume(assetId, volume, errorMessage);
    finishPlaylistTransitionIfIdle();
    return changed;
}

bool AudioPlaybackService::applyVolume(const EntityId &assetId, float volume, std::string &errorMessage)
{
    const auto it = _aliasesByAssetId.find(assetId);
    if (it == _aliasesByAssetId.end())
    {
        errorMessage.clear();
        return true;
    }

    const float clampedVolume = validVolume(volume, this->volume(assetId));
    const int mciVolume = std::clamp(static_cast<int>(clampedVolume * 1000.0f), 0, 1000);
    if (!sendCommand("setaudio " + it->second + " volume to " + std::to_string(mciVolume), errorMessage))
        return false;
    _volumeByAssetId[assetId] = clampedVolume;
    return true;
}

bool AudioPlaybackService::update(float deltaSeconds, std::string &errorMessage)
{
    errorMessage.clear();
    const float elapsed = std::isfinite(deltaSeconds) && deltaSeconds > 0.0f ? deltaSeconds : 0.0f;
    bool success = true;
    std::vector<EntityId> stopAfterFade;

    for (auto it = _fadeByAssetId.begin(); it != _fadeByAssetId.end();)
    {
        const EntityId assetId = it->first;
        FadeTransition &fade = it->second;
        if (!isActive(assetId))
        {
            it = _fadeByAssetId.erase(it);
            continue;
        }
        if (isPaused(assetId))
        {
            ++it;
            continue;
        }

        fade.elapsedSeconds = std::min(fade.durationSeconds, fade.elapsedSeconds + elapsed);
        fade.secondsSinceCommand += elapsed;
        const bool complete = fade.elapsedSeconds >= fade.durationSeconds;
        if (!complete && fade.secondsSinceCommand < kFadeCommandIntervalSeconds)
        {
            ++it;
            continue;
        }

        const float progress = fade.durationSeconds <= 0.0f ? 1.0f : std::clamp(fade.elapsedSeconds / fade.durationSeconds, 0.0f, 1.0f);
        const float nextVolume = fade.startVolume + ((fade.targetVolume - fade.startVolume) * progress);
        std::string transitionError;
        if (!applyVolume(assetId, nextVolume, transitionError))
        {
            if (errorMessage.empty())
                errorMessage = transitionError;
            success = false;
            it = _fadeByAssetId.erase(it);
            continue;
        }

        fade.secondsSinceCommand = 0.0f;
        if (!complete)
        {
            ++it;
            continue;
        }

        if (fade.stopAfterFade)
            stopAfterFade.push_back(assetId);
        it = _fadeByAssetId.erase(it);
    }

    for (const EntityId &assetId : stopAfterFade)
    {
        std::string stopError;
        if (!stop(assetId, stopError) && success)
        {
            errorMessage = stopError;
            success = false;
        }
    }
    finishPlaylistTransitionIfIdle();
    return success;
}
