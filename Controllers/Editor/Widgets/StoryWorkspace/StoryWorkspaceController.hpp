#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Audio/AudioPlaybackService.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Views/Editor/Widgets/Story/StoryWorkspace/StoryWorkspaceView.hpp"
#include <memory>
#include <string>
#include <vector>

struct StoryEntityOption
{
    std::string type;
    std::string id;
    std::string label;
};

class StoryWorkspaceController : public IWidgetController
{
public:
    StoryWorkspaceController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService);

    void update(std::shared_ptr<GLFWwindow> window) override;
    bool selectThreadById(const EntityId &threadId);
    bool selectBeatById(const EntityId &beatId);

private:
    friend class StoryWorkspaceView;

    Campaign &_currentCampaign;
    std::unique_ptr<StoryWorkspaceView> _view;
    std::shared_ptr<AudioPlaybackService> _playbackService;

    int _selectedThreadIndex = 0;
    int _selectedBeatIndex = -1;
    StoryThreadModel _draftThread;
    StoryBeatModel _draftBeat;
    StoryLinkModel _draftThreadLink;
    StoryLinkModel _draftBeatLink;
    std::string _threadLinkTargetType = "Player";
    std::string _beatLinkTargetType = "Player";
    std::string _threadLinkTargetSearch;
    std::string _beatLinkTargetSearch;
    EntityId _threadLinkSelectedTargetId;
    EntityId _beatLinkSelectedTargetId;
    int _threadLinkTargetIndex = -1;
    int _beatLinkTargetIndex = -1;
    std::string _statusMessage;
    std::string _errorMessage;
    bool _saveFailed = false;

    StoryThreadModel *getSelectedThread();
    const StoryThreadModel *getSelectedThread() const;
    StoryBeatModel *getSelectedBeat();
    const StoryBeatModel *getSelectedBeat() const;

    void configureDrafts();
    void addThread();
    void deleteSelectedThread();
    void addBeat();
    void deleteSelectedBeat();
    void addThreadLink();
    void addBeatLink();
    void deleteThreadLink(std::size_t index);
    void deleteBeatLink(std::size_t index);
    void moveBeat(int fromIndex, int offset);
    void startSelectedBeatCue();
    void stopSelectedBeatCue();
    bool saveCampaign();
    void onShow() override;

    std::vector<std::string> getEntityTypes() const;
    std::vector<StoryEntityOption> getEntityOptions(const std::string &type) const;
    StoryEntityOption getRecentEntityOption(const std::string &type) const;
    std::string describeEntity(const StoryLinkModel &link) const;
    AudioAssetModel *findAudioAsset(const EntityId &assetId);
    const AudioAssetModel *findAudioAsset(const EntityId &assetId) const;
};
