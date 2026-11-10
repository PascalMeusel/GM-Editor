#include "EditorLayoutProfileService.hpp"

#include "Models/Project/ProjectPackageLayout.hpp"
#include "imgui.h"
#include "imgui_internal.h"

#include <exception>
#include <filesystem>

EditorLayoutProfileService &EditorLayoutProfileService::getInstance()
{
    static EditorLayoutProfileService service;
    return service;
}

EditorLayoutSessionMode EditorLayoutProfileService::modeForCampaign(const Campaign &campaign)
{
    if (campaign.presentationModeActive)
        return EditorLayoutSessionMode::Presentation;

    for (const SessionModel &session : campaign.sessions)
    {
        if (session.liveModeActive)
            return EditorLayoutSessionMode::Live;
    }

    return EditorLayoutSessionMode::Prep;
}

const char *EditorLayoutProfileService::modeKey(EditorLayoutSessionMode mode)
{
    switch (mode)
    {
    case EditorLayoutSessionMode::Live:
        return "live";
    case EditorLayoutSessionMode::Presentation:
        return "presentation";
    case EditorLayoutSessionMode::Prep:
    default:
        return "prep";
    }
}

const char *EditorLayoutProfileService::modeLabel(EditorLayoutSessionMode mode)
{
    switch (mode)
    {
    case EditorLayoutSessionMode::Live:
        return "Live Session";
    case EditorLayoutSessionMode::Presentation:
        return "Presentation";
    case EditorLayoutSessionMode::Prep:
    default:
        return "Prep";
    }
}

std::filesystem::path EditorLayoutProfileService::profilePathForCampaign(const Campaign &campaign, EditorLayoutSessionMode mode)
{
    return ProjectPackageLayout::layoutForCampaign(campaign).rootDirectory / "Layouts" /
           (std::string("editor-") + modeKey(mode) + ".ini");
}

void EditorLayoutProfileService::beginFrameForCampaign(const Campaign &campaign)
{
    const EditorLayoutSessionMode mode = modeForCampaign(campaign);
    const std::filesystem::path nextPath = profilePathForCampaign(campaign, mode);
    const std::string nextKey = campaign.metadata.id + ":" + modeKey(mode);

    if (_hasActiveProfile && activeProfileKey() == nextKey && _status.profilePath == nextPath)
        return;

    saveCurrentProfile();
    loadProfileForCampaign(campaign, mode);
}

void EditorLayoutProfileService::saveIfRequested()
{
    ImGuiIO &io = ImGui::GetIO();
    if (!io.WantSaveIniSettings)
        return;

    saveCurrentProfile();
    io.WantSaveIniSettings = false;
}

void EditorLayoutProfileService::saveCurrentProfile()
{
    if (!_hasActiveProfile || _status.profilePath.empty())
        return;

    try
    {
        std::filesystem::create_directories(_status.profilePath.parent_path());
        ImGui::SaveIniSettingsToDisk(_status.profilePath.string().c_str());
        _status.lastMessage = std::string("Saved ") + modeLabel(_status.mode) + " layout.";
        _status.lastError.clear();
    }
    catch (const std::exception &error)
    {
        _status.lastError = error.what();
    }
}

void EditorLayoutProfileService::suspend()
{
    saveCurrentProfile();
    _hasActiveProfile = false;
}

std::string EditorLayoutProfileService::activeProfileKey() const
{
    return _status.campaignId + ":" + modeKey(_status.mode);
}

void EditorLayoutProfileService::loadProfileForCampaign(const Campaign &campaign, EditorLayoutSessionMode mode)
{
    _status.campaignId = campaign.metadata.id;
    _status.mode = mode;
    _status.profilePath = profilePathForCampaign(campaign, mode);
    _status.lastError.clear();
    _status.profileLoaded = false;

    try
    {
        std::filesystem::create_directories(_status.profilePath.parent_path());
        ImGui::ClearIniSettings();
        if (std::filesystem::exists(_status.profilePath))
        {
            ImGui::LoadIniSettingsFromDisk(_status.profilePath.string().c_str());
            _status.profileLoaded = true;
            _status.lastMessage = std::string("Loaded ") + modeLabel(mode) + " layout.";
        }
        else
        {
            _status.lastMessage = std::string("Created ") + modeLabel(mode) + " layout profile.";
        }
        ImGui::GetIO().WantSaveIniSettings = false;
        _hasActiveProfile = true;
    }
    catch (const std::exception &error)
    {
        _status.lastError = error.what();
        _hasActiveProfile = false;
    }
}
