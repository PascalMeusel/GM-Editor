#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

#include "AudioManagerController.hpp"

#include "Models/Audio/AudioMixService.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/ProjectLogger.hpp"
#include "Models/Project/ProjectSaveService.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <random>
#include <utility>

namespace
{
std::string uiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
}

AudioManagerController::AudioManagerController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService)
    : _currentCampaign(currentCampaign), _playbackService(std::move(playbackService))
{
    _view = std::make_unique<AudioManagerView>(*this);
}

void AudioManagerController::configure()
{
    if (_saveFailed && _saveFailureCampaignId != _currentCampaign.metadata.id)
    {
        _saveFailed = false;
        _importPendingSave = false;
        _saveErrorMessage.clear();
    }
    loadBundledCatalog();
    if (_selectedAssetIndex < 0 && !_currentCampaign.audioAssets.empty())
        selectAsset(0);
    if (_selectedPlaylistIndex < 0 && !_currentCampaign.audioPlaylists.empty())
        selectPlaylist(0);
}

void AudioManagerController::update(std::shared_ptr<GLFWwindow> window)
{
    (void)window;
    if (!m_isOpen)
        return;

    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}

AudioAssetModel *AudioManagerController::getSelectedAsset()
{
    if (_selectedAssetIndex < 0 || _selectedAssetIndex >= static_cast<int>(_currentCampaign.audioAssets.size()))
        return nullptr;
    return &_currentCampaign.audioAssets[static_cast<size_t>(_selectedAssetIndex)];
}

const AudioAssetModel *AudioManagerController::getSelectedAsset() const
{
    if (_selectedAssetIndex < 0 || _selectedAssetIndex >= static_cast<int>(_currentCampaign.audioAssets.size()))
        return nullptr;
    return &_currentCampaign.audioAssets[static_cast<size_t>(_selectedAssetIndex)];
}

void AudioManagerController::selectAsset(int index)
{
    if (_currentCampaign.audioAssets.empty())
    {
        _selectedAssetIndex = -1;
        return;
    }

    _selectedAssetIndex = std::clamp(index, 0, static_cast<int>(_currentCampaign.audioAssets.size()) - 1);
    syncSelectedControlsFromAsset();
}

void AudioManagerController::importAsset()
{
    if (_saveFailed) { retrySave(); return; }

    _errorMessage.clear();
    _statusMessage.clear();

    const std::filesystem::path sourcePath(_importSourcePath);
    AudioAssetModel asset;
    ProjectAssetImport::AssetImportRequest request;
    request.kind = ProjectAssetImport::AssetKind::Audio;
    request.sourcePath = sourcePath;
    request.assetId = asset.metadata.id;
    request.displayName = _importName;
    const ProjectAssetImport::AssetImportResult importedAsset = ProjectAssetImport::importAsset(_currentCampaign, request);
    if (!importedAsset.success)
    {
        _errorMessage = importedAsset.errorMessage;
        ProjectLogger::getInstance().warning("Audio Import", _errorMessage);
        return;
    }

    asset.name = importedAsset.displayName;
    asset.originalFileName = importedAsset.originalFileName;
    asset.originalSourcePath = importedAsset.originalSourcePath;
    asset.type = _importType;
    asset.defaultVolume = std::clamp(_importVolume, 0.0f, 1.0f);
    asset.loop = _importLoop;
    asset.filePath = importedAsset.projectPath;

    asset.metadata.touch();
    _currentCampaign.audioAssets.push_back(asset);
    selectAsset(static_cast<int>(_currentCampaign.audioAssets.size()) - 1);
    if (!saveCampaign()) { _importPendingSave = true; return; }

    _importSourcePath.clear();
    _importName.clear();
    _statusMessage = uiText("Imported audio asset: ", "Audio importiert: ") + asset.name;
    ProjectEventBus::getInstance().publish(ProjectEvent{ProjectEventType::AudioChanged,
                                                        _currentCampaign.metadata.id,
                                                        asset.metadata.id,
                                                        "audio",
                                                        _statusMessage});
}

void AudioManagerController::browseForAudioFile()
{
#if defined(_WIN32)
    char fileName[MAX_PATH] = {};
    OPENFILENAMEA openFileName = {};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.hwndOwner = nullptr;
    openFileName.lpstrFilter = "Audio Files\0*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif;*.wma;*.mid;*.midi\0All Files\0*.*\0";
    openFileName.lpstrFile = fileName;
    openFileName.nMaxFile = MAX_PATH;
    openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&openFileName))
    {
        _importSourcePath = fileName;
        if (_importName.empty())
            _importName = std::filesystem::path(_importSourcePath).stem().string();
    }
#else
    _statusMessage = uiText("Paste an audio file path to import on this platform.", "Füge auf dieser Plattform einen Audiodateipfad zum Importieren ein.");
#endif
}

void AudioManagerController::relinkSelectedAsset()
{
    AudioAssetModel *asset = getSelectedAsset();
    if (!asset)
        return;

#if defined(_WIN32)
    char fileName[MAX_PATH] = {};
    OPENFILENAMEA openFileName = {};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.hwndOwner = nullptr;
    openFileName.lpstrFilter = "Audio Files\0*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif;*.wma;*.mid;*.midi\0All Files\0*.*\0";
    openFileName.lpstrFile = fileName;
    openFileName.nMaxFile = MAX_PATH;
    openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&openFileName))
        repairSelectedAssetFromPath(*asset, std::filesystem::path(fileName));
#else
    _statusMessage = uiText("Paste and import the file again to relink audio on this platform.", "Füge die Datei erneut ein und importiere sie, um Audio auf dieser Plattform neu zu verknüpfen.");
#endif
}

void AudioManagerController::relinkSelectedAssetFromPath(const std::string &sourcePath)
{
    AudioAssetModel *asset = getSelectedAsset();
    if (!asset)
        return;

    if (sourcePath.empty())
    {
        _errorMessage = uiText("Choose an existing audio file first.", "Wähle zuerst eine vorhandene Audiodatei aus.");
        return;
    }

    repairSelectedAssetFromPath(*asset, std::filesystem::path(sourcePath));
}

void AudioManagerController::saveSelectedAssetSettings()
{
    AudioAssetModel *asset = getSelectedAsset();
    if (!asset)
        return;

    asset->defaultVolume = std::clamp(_selectedVolume, 0.0f, 1.0f);
    asset->loop = _selectedLoop;
    asset->metadata.touch();
    if (!saveCampaign()) return;
    _statusMessage = uiText("Saved audio settings: ", "Audioeinstellungen gespeichert: ") + asset->name;
    _errorMessage.clear();
}

void AudioManagerController::removeSelectedAsset()
{
    if (_saveFailed) { retrySave(); return; }

    AudioAssetModel *asset = getSelectedAsset();
    if (!asset)
        return;

    if (_playbackService->isActive(asset->metadata.id))
    {
        std::string ignored;
        _playbackService->stop(asset->metadata.id, ignored);
    }

    const std::string removedName = asset->name;
    const EntityId removedAssetId = asset->metadata.id;
    _currentCampaign.audioAssets.erase(_currentCampaign.audioAssets.begin() + _selectedAssetIndex);
    for (AudioPlaylistModel &playlist : _currentCampaign.audioPlaylists)
    {
        playlist.audioAssetIds.erase(std::remove(playlist.audioAssetIds.begin(), playlist.audioAssetIds.end(), removedAssetId), playlist.audioAssetIds.end());
        playlist.metadata.touch();
    }
    if (_currentCampaign.audioAssets.empty())
        _selectedAssetIndex = -1;
    else
        selectAsset(std::min(_selectedAssetIndex, static_cast<int>(_currentCampaign.audioAssets.size()) - 1));

    rebuildPlaylistQueue(false);
    if (!saveCampaign()) return;
    _statusMessage = uiText("Removed audio asset: ", "Audio entfernt: ") + removedName;
    _errorMessage.clear();
}
