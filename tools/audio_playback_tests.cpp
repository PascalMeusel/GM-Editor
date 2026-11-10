#include "Models/Audio/AudioBackend.hpp"
#include "Models/Audio/AudioPlaybackService.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
class RecordingAudioBackend final : public AudioCommandBackend
{
public:
    const char *name() const override { return "Recording Audio Backend"; }

    bool sendCommand(const std::string &command, std::string &errorMessage) const override
    {
        commands.push_back(command);
        errorMessage.clear();
        return true;
    }

    bool queryCommand(const std::string &command, std::string &result, std::string &errorMessage) const override
    {
        commands.push_back(command);
        result = command.find(" length") != std::string::npos ? "1000" : "0";
        errorMessage.clear();
        return true;
    }

    mutable std::vector<std::string> commands;
};

bool require(bool condition, const std::string &message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool approximately(float actual, float expected, float tolerance = 0.015f)
{
    return std::abs(actual - expected) <= tolerance;
}

AudioAssetModel makeAsset(const std::string &id)
{
    AudioAssetModel asset;
    asset.metadata.id = id;
    asset.name = id;
    return asset;
}

std::filesystem::path makeAudioFixture()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path path = std::filesystem::temp_directory_path() / ("gm_editor_audio_fade_" + std::to_string(stamp) + ".wav");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "test";
    return path;
}

bool testFadeInIsNonBlockingAndTickDriven(const std::filesystem::path &fixture)
{
    auto backend = std::make_unique<RecordingAudioBackend>();
    AudioPlaybackService playback(std::move(backend));
    AudioAssetModel asset = makeAsset("fade_in");
    std::string error;

    const auto started = std::chrono::steady_clock::now();
    const bool scheduled = playback.fadeIn(asset, fixture, 0.8f, true, 5.0f, error);
    const auto callTime = std::chrono::steady_clock::now() - started;

    bool ok = true;
    ok &= require(scheduled && error.empty(), "Fade-in should schedule successfully");
    ok &= require(callTime < std::chrono::milliseconds(250), "Scheduling a five-second fade must not block the caller");
    ok &= require(playback.isActive(asset.metadata.id) && playback.isFading(asset.metadata.id), "Fade-in should leave an active scheduled channel");
    ok &= require(approximately(playback.volume(asset.metadata.id), 0.0f), "Fade-in should start silent");
    ok &= require(playback.update(2.5f, error), "Fade-in midpoint tick should succeed");
    ok &= require(approximately(playback.volume(asset.metadata.id), 0.4f), "Fade-in midpoint volume should be interpolated");
    ok &= require(playback.update(2.5f, error), "Fade-in completion tick should succeed");
    ok &= require(approximately(playback.volume(asset.metadata.id), 0.8f) && playback.activeFadeCount() == 0, "Fade-in should finish at its target volume");
    return ok;
}

bool testFadeOutStopsOnlyAfterCompletion(const std::filesystem::path &fixture)
{
    AudioPlaybackService playback(std::make_unique<RecordingAudioBackend>());
    AudioAssetModel asset = makeAsset("fade_out");
    std::string error;
    bool ok = true;
    ok &= require(playback.play(asset, fixture, 0.8f, true, error), "Fade-out fixture should start");
    ok &= require(playback.fadeOut(asset.metadata.id, 2.0f, true, error), "Fade-out should schedule");
    ok &= require(playback.isActive(asset.metadata.id), "Fade-out must not stop synchronously");
    ok &= require(playback.update(1.0f, error), "Fade-out midpoint tick should succeed");
    ok &= require(playback.isActive(asset.metadata.id) && approximately(playback.volume(asset.metadata.id), 0.4f), "Fade-out should remain active at its midpoint");
    ok &= require(playback.update(1.0f, error), "Fade-out completion tick should succeed");
    ok &= require(!playback.isActive(asset.metadata.id) && playback.activeFadeCount() == 0, "Fade-out should close the channel only after completion");
    return ok;
}

bool testCrossfadeProgressAndPlaylistReceipt(const std::filesystem::path &fixture)
{
    AudioPlaybackService playback(std::make_unique<RecordingAudioBackend>());
    AudioAssetModel from = makeAsset("from_track");
    AudioAssetModel to = makeAsset("to_track");
    std::string error;
    bool ok = true;
    ok &= require(playback.play(from, fixture, 0.8f, true, error), "Crossfade source should start");
    ok &= require(playback.crossfade(from.metadata.id, to, fixture, 0.6f, true, 2.0f, error), "Crossfade should schedule both channels");

    AudioPlaylistPlaybackState state;
    state.playlistId = "playlist";
    state.queueAssetIds = {from.metadata.id, to.metadata.id};
    state.currentAssetId = to.metadata.id;
    state.transitionActive = true;
    state.transitionLabel = "Crossfade";
    playback.setPlaylistPlaybackState(state);

    ok &= require(playback.activeFadeCount() == 2 && playback.isActive(from.metadata.id) && playback.isActive(to.metadata.id), "Crossfade should initially keep both channels active");
    ok &= require(playback.update(1.0f, error), "Crossfade midpoint tick should succeed");
    ok &= require(approximately(playback.volume(from.metadata.id), 0.4f), "Crossfade should lower the source at midpoint");
    ok &= require(approximately(playback.volume(to.metadata.id), 0.3f), "Crossfade should raise the target at midpoint");
    ok &= require(playback.playlistPlaybackState().transitionActive, "Playlist transition should remain active until fades finish");
    ok &= require(playback.update(1.0f, error), "Crossfade completion tick should succeed");
    ok &= require(!playback.isActive(from.metadata.id) && playback.isActive(to.metadata.id), "Crossfade should close only the source channel");
    ok &= require(approximately(playback.volume(to.metadata.id), 0.6f), "Crossfade target should reach its requested volume");
    ok &= require(!playback.playlistPlaybackState().transitionActive, "Playlist transition receipt should clear when fades finish");
    return ok;
}

bool testManualVolumeCancelsScheduledFade(const std::filesystem::path &fixture)
{
    AudioPlaybackService playback(std::make_unique<RecordingAudioBackend>());
    AudioAssetModel asset = makeAsset("manual_override");
    std::string error;
    bool ok = true;
    ok &= require(playback.fadeIn(asset, fixture, 1.0f, true, 4.0f, error), "Fade should schedule before manual override");
    ok &= require(playback.setVolume(asset.metadata.id, 0.25f, error), "Manual volume override should succeed");
    ok &= require(playback.activeFadeCount() == 0, "Manual volume should cancel the stale fade");
    ok &= require(playback.update(4.0f, error), "Tick after manual override should succeed");
    ok &= require(approximately(playback.volume(asset.metadata.id), 0.25f), "Cancelled fade must not overwrite a manual volume");
    return ok;
}

bool testPauseFreezesFadeProgress(const std::filesystem::path &fixture)
{
    AudioPlaybackService playback(std::make_unique<RecordingAudioBackend>());
    AudioAssetModel asset = makeAsset("paused_fade");
    std::string error;
    bool ok = true;
    ok &= require(playback.play(asset, fixture, 1.0f, true, error), "Paused fade fixture should start");
    ok &= require(playback.fadeOut(asset.metadata.id, 2.0f, false, error), "Paused fade should schedule");
    ok &= require(playback.pause(asset.metadata.id, error), "Scheduled fade channel should pause");
    ok &= require(playback.update(1.0f, error), "Tick while paused should succeed");
    ok &= require(approximately(playback.volume(asset.metadata.id), 1.0f), "Paused fade must not advance");
    ok &= require(playback.resume(asset.metadata.id, error), "Scheduled fade channel should resume");
    ok &= require(playback.update(2.0f, error), "Resumed fade should complete");
    ok &= require(approximately(playback.volume(asset.metadata.id), 0.0f) && playback.activeFadeCount() == 0, "Resumed fade should reach its target");
    return ok;
}
}

int main()
{
    const std::filesystem::path fixture = makeAudioFixture();
    bool ok = true;
    ok &= testFadeInIsNonBlockingAndTickDriven(fixture);
    ok &= testFadeOutStopsOnlyAfterCompletion(fixture);
    ok &= testCrossfadeProgressAndPlaylistReceipt(fixture);
    ok &= testManualVolumeCancelsScheduledFade(fixture);
    ok &= testPauseFreezesFadeProgress(fixture);
    std::error_code cleanupError;
    std::filesystem::remove(fixture, cleanupError);

    if (!ok)
        return 1;
    std::cout << "Audio playback transition tests passed.\n";
    return 0;
}
