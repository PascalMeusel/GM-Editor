#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <filesystem>
#include <string>

enum class EditorLayoutSessionMode
{
    Prep,
    Live,
    Presentation
};

struct EditorLayoutProfileStatus
{
    EntityId campaignId;
    EditorLayoutSessionMode mode = EditorLayoutSessionMode::Prep;
    std::filesystem::path profilePath;
    std::string lastMessage;
    std::string lastError;
    bool profileLoaded = false;
};

class EditorLayoutProfileService
{
public:
    static EditorLayoutProfileService &getInstance();

    void beginFrameForCampaign(const Campaign &campaign);
    void saveIfRequested();
    void saveCurrentProfile();
    void suspend();

    const EditorLayoutProfileStatus &status() const { return _status; }

    static EditorLayoutSessionMode modeForCampaign(const Campaign &campaign);
    static const char *modeKey(EditorLayoutSessionMode mode);
    static const char *modeLabel(EditorLayoutSessionMode mode);
    static std::filesystem::path profilePathForCampaign(const Campaign &campaign, EditorLayoutSessionMode mode);

private:
    EditorLayoutProfileService() = default;

    std::string activeProfileKey() const;
    void loadProfileForCampaign(const Campaign &campaign, EditorLayoutSessionMode mode);

    EditorLayoutProfileStatus _status;
    bool _hasActiveProfile = false;
};
