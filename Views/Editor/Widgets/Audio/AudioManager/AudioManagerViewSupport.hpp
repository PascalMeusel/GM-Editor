#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "AudioManagerView.hpp"

#include "Controllers/Editor/Widgets/AudioManager/AudioManagerController.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "Views/Editor/Widgets/Project/ProjectAssetRepairPanel.hpp"
#include "imgui.h"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace
{
bool volumePercent(const char *label,float &volume) {
    float percent=volume*100.f;
    if(!FormUi::SliderFloat(label,&percent,0.f,100.f,"%.0f %%"))return false;
    volume=percent/100.f;return true;
}
const char *defaultTriggerEvent(const std::string &type) {
    if(type=="MapRegion")return "Party Entered";
    if(type=="CombatEvent")return "Combat Started";
    if(type=="SessionEvent")return "Session Started";
    return "Scene Started";
}
void chooseAudio(const Campaign &campaign,const char *id,EntityId &selected) {
    const char *preview=uiTextForLocale("Choose sound","Klang wählen");
    for(const auto &asset:campaign.audioAssets)if(asset.metadata.id==selected)preview=asset.name.c_str();
    if(FormUi::BeginCombo(id,preview)) {
        for(const auto &asset:campaign.audioAssets)if(ImGui::Selectable(asset.name.c_str(),asset.metadata.id==selected))selected=asset.metadata.id;
        ImGui::EndCombo();
    }
}
void chooseTriggerContext(const Campaign &campaign,const std::string &type,EntityId &selected,std::string &event) {
    std::vector<std::pair<EntityId,std::string>> sources;
    std::vector<const char*> events;
    if(type=="StoryBeat") {
        for(const auto &thread:campaign.storyThreads)for(const auto &scene:thread.beats)sources.emplace_back(scene.metadata.id,scene.title);
        events={"Scene Started"};
    } else if(type=="SessionEvent") {
        for(const auto &session:campaign.sessions)sources.emplace_back(session.metadata.id,session.title);
        events={"Session Started"};
    } else if(type=="CombatEvent") {
        for(const auto &encounter:campaign.encounters)sources.emplace_back(encounter.metadata.id,encounter.name);
        events={"Combat Started","Round Started","Combat Ended"};
    } else if(type=="MapRegion") {
        for(const auto &map:campaign.maps)for(const auto &region:map.regions)sources.emplace_back(region.metadata.id,map.name+" / "+region.name);
        events={"Party Entered"};
    }
    std::string preview=selected.empty()?uiTextForLocale("Any matching source","Jede passende Quelle"):uiTextForLocale("Missing source","Quelle fehlt");
    for(const auto &source:sources)if(source.first==selected)preview=source.second;
    if(FormUi::BeginCombo("##TriggerNamedSource",preview.c_str())) {
        if(ImGui::Selectable(uiTextForLocale("Any matching source","Jede passende Quelle"),selected.empty()))selected.clear();
        for(const auto &source:sources)if(ImGui::Selectable((source.second+"##"+source.first).c_str(),selected==source.first))selected=source.first;
        ImGui::EndCombo();
    }
    if(FormUi::BeginCombo("##TriggerSupportedEvent",localizedUiText(event.c_str()))) {
        for(const auto candidate:events)if(ImGui::Selectable(localizedUiText(candidate),event==candidate))event=candidate;
        ImGui::EndCombo();
    }
    if(ImGui::TreeNode(uiTextForLocale("Advanced identifiers","Erweiterte Kennungen"))) {
        FormUi::InputText("Source ID",&selected);FormUi::InputText("Event",&event);ImGui::TreePop();
    }
}
std::string audioDisplayName(const AudioAssetModel &asset)
{
    if (!asset.name.empty())
        return asset.name;
    if (!asset.originalFileName.empty())
        return asset.originalFileName;
    return "Untitled audio";
}

std::string audioDisplayName(const AudioAssetModel *asset)
{
    return asset ? audioDisplayName(*asset) : "None";
}

std::string formatSeconds(float seconds)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << seconds << "s";
    return stream.str();
}

const char *onOffLabel(bool enabled)
{
    return enabled ? "On" : "Off";
}

std::string joinPreviewNames(const std::vector<std::string> &names)
{
    if (names.empty())
        return "none";

    std::string result;
    constexpr size_t maxNames = 3;
    for (size_t i = 0; i < names.size() && i < maxNames; ++i)
    {
        if (!result.empty())
            result += ", ";
        result += names[i];
    }

    if (names.size() > maxNames)
        result += ", +" + std::to_string(names.size() - maxNames) + " more";

    return result;
}

struct PlaylistQueueSummary
{
    int totalItems = 0;
    int playableItems = 0;
    int missingAssets = 0;
    int missingFiles = 0;
    const AudioAssetModel *firstPlayable = nullptr;
};

bool isPlaylistAssetPlayable(const AudioAssetModel *asset)
{
    return asset && !asset->missingFile && !asset->filePath.empty();
}

PlaylistQueueSummary summarizePlaylistQueue(const AudioManagerController &controller, const AudioPlaylistModel &playlist)
{
    PlaylistQueueSummary summary;
    summary.totalItems = static_cast<int>(playlist.audioAssetIds.size());
    for (const EntityId &assetId : playlist.audioAssetIds)
    {
        const AudioAssetModel *asset = controller.findAudioAssetById(assetId);
        if (!asset)
        {
            ++summary.missingAssets;
            continue;
        }

        if (!isPlaylistAssetPlayable(asset))
        {
            ++summary.missingFiles;
            continue;
        }

        ++summary.playableItems;
        if (!summary.firstPlayable)
            summary.firstPlayable = asset;
    }
    return summary;
}

std::string playlistDisabledReason(const PlaylistQueueSummary &summary)
{
    if (summary.totalItems == 0)
        return "Add at least one audio asset to this playlist.";
    if (summary.playableItems == 0)
    {
        std::string reason = "Playlist has no playable audio assets.";
        if (summary.missingAssets > 0)
            reason += " Missing references: " + std::to_string(summary.missingAssets) + ".";
        if (summary.missingFiles > 0)
            reason += " Missing files: " + std::to_string(summary.missingFiles) + ".";
        return reason;
    }
    return {};
}

std::string assetNameById(const AudioManagerController &controller, const EntityId &assetId)
{
    const AudioAssetModel *asset = controller.findAudioAssetById(assetId);
    return asset ? audioDisplayName(*asset) : "(missing asset)";
}

std::string buildSceneTransitionPreview(
    const AudioManagerController &controller,
    const AudioPlaylistModel &playlist,
    const AudioAssetModel *target,
    const std::vector<EntityId> &activeIds,
    float fadeSeconds)
{
    if (!target)
        return "Scene Transition needs at least one playable playlist item.";

    const bool playAsOneShot = playlist.oneShot || target->type == AudioAssetType::Effect;
    std::vector<std::string> stopNames;
    std::string firstFadeSource;
    bool targetAlreadyActive = false;

    for (const EntityId &activeId : activeIds)
    {
        if (activeId == target->metadata.id)
        {
            targetAlreadyActive = true;
            continue;
        }

        if (firstFadeSource.empty())
            firstFadeSource = assetNameById(controller, activeId);
        stopNames.push_back(assetNameById(controller, activeId));
    }

    std::string preview = std::string(playlist.shuffle ? "Preview target: " : "Target: ") + audioDisplayName(*target) + " (" + (playAsOneShot ? "one-shot" : "looping if asset is set to loop") + ").\n";
    if (playlist.shuffle)
        preview += "Queue: shuffle is on; confirming rebuilds the queue and may choose a different target.\n";
    if (firstFadeSource.empty())
        preview += "Fade: no active non-target channel; the target will fade in over " + formatSeconds(fadeSeconds) + ".\n";
    else
        preview += "Fade: " + firstFadeSource + " -> " + audioDisplayName(*target) + " over " + formatSeconds(fadeSeconds) + ".\n";

    preview += "Stop: " + (stopNames.empty() ? std::string("no other active channels.") : joinPreviewNames(stopNames) + " after the transition.") + "\n";
    preview += targetAlreadyActive ? "Continue: the target is already active and remains the scene bed." : "Continue: the target becomes the scene bed.";
    return preview;
}

void drawAudioTypeCombo(const char *label, AudioAssetType &type)
{
    if (FormUi::BeginCombo(label, localizedUiText(audioAssetTypeLabel(type))))
    {
        for (const AudioAssetType candidate : {AudioAssetType::Music, AudioAssetType::Ambience, AudioAssetType::Effect, AudioAssetType::Voice, AudioAssetType::Other})
        {
            const bool selected = candidate == type;
            if (ImGui::Selectable(localizedUiText(audioAssetTypeLabel(candidate)), selected))
                type = candidate;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}
}
