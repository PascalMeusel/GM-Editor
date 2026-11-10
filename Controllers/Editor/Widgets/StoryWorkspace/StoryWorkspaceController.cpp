#include "StoryWorkspaceController.hpp"
#include "Models/Audio/AudioMixService.hpp"
#include "Models/Project/CampaignBookmarkService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Story/SceneLaunchService.hpp"
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include <algorithm>
#include <cstddef>
#include <utility>

StoryWorkspaceController::StoryWorkspaceController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService)
    : _currentCampaign(currentCampaign), _playbackService(std::move(playbackService))
{
    _view = std::make_unique<StoryWorkspaceView>(*this);
    configureDrafts();
}

void StoryWorkspaceController::update(std::shared_ptr<GLFWwindow> window)
{
    if (!m_isOpen)
        return;

    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}

bool StoryWorkspaceController::selectThreadById(const EntityId &threadId)
{
    for (int i = 0; i < static_cast<int>(_currentCampaign.storyThreads.size()); ++i)
    {
        if (_currentCampaign.storyThreads[i].metadata.id == threadId)
        {
            _selectedThreadIndex = i;
            _selectedBeatIndex = _currentCampaign.storyThreads[i].beats.empty() ? -1 : 0;
            return true;
        }
    }
    return false;
}

bool StoryWorkspaceController::selectBeatById(const EntityId &beatId)
{
    for (int threadIndex = 0; threadIndex < static_cast<int>(_currentCampaign.storyThreads.size()); ++threadIndex)
    {
        StoryThreadModel &thread = _currentCampaign.storyThreads[static_cast<std::size_t>(threadIndex)];
        for (int beatIndex = 0; beatIndex < static_cast<int>(thread.beats.size()); ++beatIndex)
        {
            if (thread.beats[static_cast<std::size_t>(beatIndex)].metadata.id == beatId)
            {
                _selectedThreadIndex = threadIndex;
                _selectedBeatIndex = beatIndex;
                return true;
            }
        }
    }
    return false;
}

StoryThreadModel *StoryWorkspaceController::getSelectedThread()
{
    if (_currentCampaign.storyThreads.empty())
        return nullptr;
    if (_selectedThreadIndex < 0 || _selectedThreadIndex >= static_cast<int>(_currentCampaign.storyThreads.size()))
        _selectedThreadIndex = 0;
    return &_currentCampaign.storyThreads[_selectedThreadIndex];
}

const StoryThreadModel *StoryWorkspaceController::getSelectedThread() const
{
    if (_currentCampaign.storyThreads.empty())
        return nullptr;
    if (_selectedThreadIndex < 0 || _selectedThreadIndex >= static_cast<int>(_currentCampaign.storyThreads.size()))
        return &_currentCampaign.storyThreads.front();
    return &_currentCampaign.storyThreads[_selectedThreadIndex];
}

StoryBeatModel *StoryWorkspaceController::getSelectedBeat()
{
    StoryThreadModel *thread = getSelectedThread();
    if (!thread || thread->beats.empty())
        return nullptr;
    if (_selectedBeatIndex < 0 || _selectedBeatIndex >= static_cast<int>(thread->beats.size()))
        _selectedBeatIndex = 0;
    return &thread->beats[_selectedBeatIndex];
}

const StoryBeatModel *StoryWorkspaceController::getSelectedBeat() const
{
    const StoryThreadModel *thread = getSelectedThread();
    if (!thread || thread->beats.empty())
        return nullptr;
    if (_selectedBeatIndex < 0 || _selectedBeatIndex >= static_cast<int>(thread->beats.size()))
        return &thread->beats.front();
    return &thread->beats[_selectedBeatIndex];
}

void StoryWorkspaceController::configureDrafts()
{
    _draftThread = StoryThreadModel{};
    _draftThread.title = gm::localization::trUi("New Story Thread");
    _draftThread.type = StoryThreadType::MainStory;
    _draftThread.status = StoryStatus::Idea;

    _draftBeat = StoryBeatModel{};
    _draftBeat.title = gm::localization::trUi("New Beat");
    _draftBeat.status = StoryStatus::Idea;

    _draftThreadLink = StoryLinkModel{};
    _draftThreadLink.type = StoryLinkType::Related;
    _draftBeatLink = StoryLinkModel{};
    _draftBeatLink.type = StoryLinkType::Related;
}

void StoryWorkspaceController::addThread()
{
    if (_draftThread.title.empty())
        return;

    _draftThread.metadata.touch();
    auto existing = std::find_if(_currentCampaign.storyThreads.begin(), _currentCampaign.storyThreads.end(),
        [&](const auto &thread) { return thread.metadata.id == _draftThread.metadata.id; });
    if (existing == _currentCampaign.storyThreads.end())
    {
        _currentCampaign.storyThreads.push_back(_draftThread);
        _selectedThreadIndex = static_cast<int>(_currentCampaign.storyThreads.size()) - 1;
    }
    else
    {
        existing->title = _draftThread.title;
        existing->summary = _draftThread.summary;
        existing->type = _draftThread.type;
        existing->status = _draftThread.status;
        existing->metadata.touch();
        _selectedThreadIndex = static_cast<int>(existing - _currentCampaign.storyThreads.begin());
    }
    _selectedBeatIndex = -1;
    if (saveCampaign()) configureDrafts();
}

void StoryWorkspaceController::deleteSelectedThread()
{
    if (_currentCampaign.storyThreads.empty() || _selectedThreadIndex < 0 || _selectedThreadIndex >= static_cast<int>(_currentCampaign.storyThreads.size()))
        return;

    _currentCampaign.storyThreads.erase(_currentCampaign.storyThreads.begin() + _selectedThreadIndex);
    if (_selectedThreadIndex >= static_cast<int>(_currentCampaign.storyThreads.size()))
        _selectedThreadIndex = static_cast<int>(_currentCampaign.storyThreads.size()) - 1;
    _selectedBeatIndex = -1;
    saveCampaign();
}

void StoryWorkspaceController::addBeat()
{
    StoryThreadModel *thread = getSelectedThread();
    if (!thread || _draftBeat.title.empty())
        return;

    // Retry updates the staged scene by stable identity, even if the user
    // switched arcs after a failed save. It must not create another scene.
    for (std::size_t index = 0; index < _currentCampaign.storyThreads.size(); ++index)
        for (const auto &beat : _currentCampaign.storyThreads[index].beats)
            if (beat.metadata.id == _draftBeat.metadata.id)
            { thread = &_currentCampaign.storyThreads[index]; _selectedThreadIndex = static_cast<int>(index); }
    _draftBeat.metadata.touch();
    auto existing = std::find_if(thread->beats.begin(), thread->beats.end(),
        [&](const auto &beat) { return beat.metadata.id == _draftBeat.metadata.id; });
    if (existing == thread->beats.end()) { thread->beats.push_back(_draftBeat); _selectedBeatIndex = static_cast<int>(thread->beats.size()) - 1; }
    else
    {
        existing->title = _draftBeat.title; existing->summary = _draftBeat.summary;
        existing->status = _draftBeat.status; existing->inWorldDate = _draftBeat.inWorldDate;
        existing->metadata.touch();
        _selectedBeatIndex = static_cast<int>(existing - thread->beats.begin());
    }
    thread->metadata.touch();
    if (!saveCampaign()) return;
    _draftBeat = StoryBeatModel{};
    _draftBeat.title = gm::localization::trUi("New Beat");
    _draftBeat.status = StoryStatus::Idea;
}

void StoryWorkspaceController::deleteSelectedBeat()
{
    StoryThreadModel *thread = getSelectedThread();
    if (!thread || _selectedBeatIndex < 0 || _selectedBeatIndex >= static_cast<int>(thread->beats.size()))
        return;

    thread->beats.erase(thread->beats.begin() + _selectedBeatIndex);
    if (_selectedBeatIndex >= static_cast<int>(thread->beats.size()))
        _selectedBeatIndex = static_cast<int>(thread->beats.size()) - 1;
    thread->metadata.touch();
    saveCampaign();
}
