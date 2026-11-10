#include "StoryWorkspaceViewSupport.hpp"

void StoryWorkspaceView::drawBeatBoard()
{
    StoryThreadModel *thread = _controller.getSelectedThread();
    if (!thread)
    {
        drawHelpText("Scenes and Reveals", "A scene is a playable moment inside an arc: a clue, reveal, decision, encounter, consequence, or turning point.");
        ImGui::Separator();
        drawWrappedMuted("A scene is a playable narrative moment: a reveal, decision, clue, consequence, encounter, or turning point.");
        return;
    }

    ImGui::Text(localizedUiLabel("Scenes and Reveals (%d)").c_str(), static_cast<int>(thread->beats.size()));
    showHelpTooltip("These are the concrete moments that can happen at the table inside the selected arc.");
    ImGui::Separator();
    if (ImGui::TreeNode(localizedUiLabel("Scene Board").c_str()))
    {
        drawSceneStatusOverview(*thread, _controller._selectedBeatIndex);
        ImGui::TreePop();
    }

    const StoryBeatModel *selectedBeat = _controller.getSelectedBeat();
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::BeginCombo("##StorySceneSelection", selectedBeat ? selectedBeat->title.c_str() : localizedUiText("No scene selected")))
    {
        for (int index = 0; index < static_cast<int>(thread->beats.size()); ++index)
        {
            ImGui::PushID(index);
            const auto &entry = thread->beats[index];
            if (ImGui::Selectable(entry.title.c_str(), index == _controller._selectedBeatIndex))
                _controller._selectedBeatIndex = index;
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (ImGui::TreeNodeEx(localizedUiLabel("New Scene").c_str(), thread->beats.empty() ? ImGuiTreeNodeFlags_DefaultOpen : 0))
    {
    ImGui::SeparatorText(localizedUiLabel("New Scene").c_str());
    showHelpTooltip("Add a specific moment that may happen during play.");
    FormUi::InputText(localizedUiLabel("Scene Title##NewBeat").c_str(), &_controller._draftBeat.title);
    drawStatusCombo("Current State##NewBeat", _controller._draftBeat.status);
    FormUi::InputText(localizedUiLabel("World Date##NewBeat").c_str(), &_controller._draftBeat.inWorldDate);
    showHelpTooltip("Optional in-fiction date or timing, such as 'Night 2', 'Harvest Moon', or 'Before the siege'.");
    ImGui::TextUnformatted(localizedUiText("What Happens"));
    FormUi::InputTextMultiline("##NewBeatSummary", &_controller._draftBeat.summary, ImVec2(-1.0f, 88.0f), ImGuiInputTextFlags_WordWrap);
    if (drawUiButton("Add Scene", UiButtonRole::Primary))
        _controller.addBeat();
        ImGui::TreePop();
    }

    StoryBeatModel *beat = _controller.getSelectedBeat();
    if (!beat)
        return;

    const bool saveScene = drawUiButton("Save Scene", UiButtonRole::Primary);
    if (ImGui::TreeNode(localizedUiLabel("Scene order").c_str()))
    {
        if (drawUiButton("Move up", UiButtonRole::Secondary)) _controller.moveBeat(_controller._selectedBeatIndex, -1);
        if (drawUiButton("Move down", UiButtonRole::Secondary)) _controller.moveBeat(_controller._selectedBeatIndex, 1);
        ImGui::TreePop();
        beat = _controller.getSelectedBeat();
        if (!beat) return;
    }
    ImGui::SeparatorText(localizedUiLabel("Selected Scene").c_str());
    FormUi::InputText(localizedUiLabel("Scene Title##Beat").c_str(), &beat->title);
    drawStatusCombo("Current State##Beat", beat->status);
    FormUi::InputText(localizedUiLabel("World Date##Beat").c_str(), &beat->inWorldDate);
    drawHelpText("GM Outline", "Private planning outline for the playable moment, clue, reveal, decision, or consequence. This is never sent to Player Display.");
    FormUi::InputTextMultiline(localizedUiLabel("##BeatSummary").c_str(), &beat->summary, ImVec2(-1.0f, 78.0f), ImGuiInputTextFlags_WordWrap);
    if (ImGui::CollapsingHeader(localizedUiLabel("Scene details").c_str()))
    {
    drawHelpText("Player-Facing Summary", "Short public scene text that can be shown to the table.");
    FormUi::InputTextMultiline(localizedUiLabel("##BeatPlayerSummary").c_str(), &beat->playerSummary, ImVec2(-1.0f, 56.0f), ImGuiInputTextFlags_WordWrap);
    drawHelpText("Read-Aloud Text", "Prepared narration that is safe to read or show to players.");
    FormUi::InputTextMultiline(localizedUiLabel("##BeatReadAloud").c_str(), &beat->readAloudText, ImVec2(-1.0f, 68.0f), ImGuiInputTextFlags_WordWrap);
    drawHelpText("Clues", "GM-only clue notes and hidden discoveries for this scene.");
    FormUi::InputTextMultiline(localizedUiLabel("##BeatClues").c_str(), &beat->clues, ImVec2(-1.0f, 88.0f), ImGuiInputTextFlags_WordWrap);
    drawHelpText("Consequences", "GM-only outcomes, branches, or follow-up changes.");
    FormUi::InputTextMultiline(localizedUiLabel("##BeatConsequences").c_str(), &beat->consequences, ImVec2(-1.0f, 88.0f), ImGuiInputTextFlags_WordWrap);
    drawHelpText("Private GM Notes", "Behind-the-screen notes for this scene.");
    FormUi::InputTextMultiline(localizedUiLabel("##BeatNotes").c_str(), &beat->gmNotes, ImVec2(-1.0f, 86.0f), ImGuiInputTextFlags_WordWrap);

    }
    if (ImGui::CollapsingHeader(localizedUiLabel("Preparation checklist").c_str()))
        drawSceneChecklist(*beat);

    if (ImGui::CollapsingHeader(localizedUiLabel("Audio and launch").c_str()))
    {


    ImGui::SeparatorText(localizedUiLabel("Narrative Cue").c_str());
    drawHelpText("Scene Music", "Choose music or ambience that should play when this scene starts at the table.");
    const AudioAssetModel *cueAsset = _controller.findAudioAsset(beat->cueAudioAssetId);
    const std::string launchPreview = sceneLaunchPreview(*beat, cueAsset);
    const char *cuePreview = cueAsset ? cueAsset->name.c_str() : localizedUiText("No music cue");
    if (FormUi::BeginCombo(localizedUiLabel("Music Cue").c_str(), cuePreview))
    {
        const bool noCueSelected = beat->cueAudioAssetId.empty();
        if (ImGui::Selectable(localizedUiLabel("No music cue").c_str(), noCueSelected))
            beat->cueAudioAssetId.clear();
        if (noCueSelected)
            ImGui::SetItemDefaultFocus();

        for (const AudioAssetModel &asset : _controller._currentCampaign.audioAssets)
        {
            const bool selected = beat->cueAudioAssetId == asset.metadata.id;
            const std::string label = asset.name + " (" + audioAssetTypeLabel(asset.type) + ")";
            if (ImGui::Selectable(label.c_str(), selected))
            {
                beat->cueAudioAssetId = asset.metadata.id;
                beat->cueVolume = asset.defaultVolume;
                beat->cueLoop = asset.loop;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (!_controller._currentCampaign.audioAssets.empty())
    {
        FormUi::SliderFloat(localizedUiLabel("Cue Volume").c_str(), &beat->cueVolume, 0.0f, 1.0f, "%.2f");
        FormUi::Checkbox(localizedUiLabel("Loop Cue").c_str(), &beat->cueLoop);
    }
    else
    {
        ImGui::TextDisabled(localizedUiLabel("Import music in the Audio Library before assigning a scene cue.").c_str());
    }
    if (drawUiButton("Launch Scene", UiButtonRole::Primary, ImVec2(140.0f, 0.0f), "Mark this scene active, stage linked player-display state, save, run Scene Started triggers, and start the selected cue if available."))
        ImGui::OpenPopup(localizedUiLabel("Launch Scene Review").c_str());
    setResponsiveAutoSizeWindow(640.0f, 420.0f);
    if (ImGui::BeginPopupModal(localizedUiLabel("Launch Scene Review").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        int revealNpcCount = 0;
        int revealHandoutCount = 0;
        for (const StoryLinkModel &link : beat->links)
        {
            if (link.type != StoryLinkType::Reveals)
                continue;
            if (link.targetEntityType == "NPC")
                ++revealNpcCount;
            else if (link.targetEntityType == "Handout")
                ++revealHandoutCount;
        }

        ImGui::TextWrapped(localizedUiLabel("Launch this scene as the active table moment?").c_str());
        drawInlineLabelValue("Scene", beat->title.empty() ? localizedUiText("Untitled scene") : beat->title.c_str());
        drawInlineLabelValue("Arc", thread->title.empty() ? localizedUiText("Untitled arc") : thread->title.c_str());
        drawInlineLabelValue("Cue", cueAsset ? cueAsset->name.c_str() : (beat->cueAudioAssetId.empty() ? localizedUiText("No direct cue") : localizedUiText("Missing direct cue")));
        const std::string revealSummary = std::to_string(revealNpcCount) + " " + localizedUiText("NPCs") + ", " +
                                          std::to_string(revealHandoutCount) + " " + localizedUiText("handouts");
        drawInlineLabelValue("Player Display reveals", revealSummary.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped(localizedUiLabel("This will mark the arc and scene Active, stage player-safe scene/map/initiative state, run Scene Started triggers, save the campaign, and start the direct cue when available.").c_str());
        ImGui::PopStyleColor();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launchPreview.c_str());
        if (drawModalActionButtons("Launch Scene", UiButtonRole::Primary))
        {
            _controller.startSelectedBeatCue();
            // Session transactions replace campaign vectors; do not keep this
            // frame's editor pointers after the launch/add-to-plan command.
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (drawUiButton("Stop Cue", UiButtonRole::Secondary, ImVec2(110.0f, 0.0f), "Stop the direct cue selected for this scene. Triggered audio may need to be stopped from audio controls."))
        _controller.stopSelectedBeatCue();
    }
    if (!_controller._statusMessage.empty())
        ImGui::TextColored(ImVec4(0.48f, 0.78f, 0.54f, 1.0f), localizedUiLabel("%s").c_str(), _controller._statusMessage.c_str());
    if (!_controller._errorMessage.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.38f, 1.0f), localizedUiLabel("%s").c_str(), _controller._errorMessage.c_str());

    if (saveScene)
    {
        beat->metadata.touch();
        thread->metadata.touch();
        _controller.saveCampaign();
    }
    if (ImGui::TreeNode(localizedUiLabel("Scene actions").c_str()))
    {
        const bool requestDelete = drawUiButton("Delete Scene", UiButtonRole::Destructive);
        ImGui::TreePop();
        if (requestDelete) ImGui::OpenPopup(localizedUiLabel("Delete Scene").c_str());
    }

    if (ImGui::BeginPopupModal(localizedUiLabel("Delete Scene").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(localizedUiLabel("Delete this scene from the selected story arc?").c_str());
        drawInlineLabelValue("Scene", beat->title.empty() ? localizedUiText("Untitled scene") : beat->title.c_str());
        if (drawModalActionButtons("Delete", UiButtonRole::Destructive, "Cancel", 110.0f))
            _controller.deleteSelectedBeat();
        ImGui::EndPopup();
    }

    ImGui::SeparatorText(localizedUiLabel("Scene Connections").c_str());
    showHelpTooltip("Connections tie this scene to the campaign object it uses, affects, reveals, or depends on.");
    drawLinks(true);
    if (ImGui::TreeNode(localizedUiLabel("Add Scene Connection").c_str()))
    {
        drawLinkEditor(true);
        ImGui::TreePop();
    }
}

void StoryWorkspaceView::drawSceneChecklist(const StoryBeatModel &beat)
{
    const AudioAssetModel *cueAsset = _controller.findAudioAsset(beat.cueAudioAssetId);
    const SceneChecklistFacts facts = buildSceneChecklistFacts(_controller._currentCampaign, beat, cueAsset);

    const ChecklistState npcState = facts.npcLinks > 0 ? ChecklistState::Ready : ChecklistState::Open;
    const ChecklistState mapState = (facts.mapLinks + facts.encounterMaps) > 0 ? ChecklistState::Ready : ChecklistState::Open;
    const ChecklistState encounterState = facts.encounterLinks > 0 ? ChecklistState::Ready : ChecklistState::Open;
    const ChecklistState handoutState = facts.handoutLinks > 0 ? ChecklistState::Ready : ChecklistState::Open;
    const ChecklistState audioState = facts.missingCue ? ChecklistState::Missing : ((facts.directCue || facts.audioLinks > 0) ? ChecklistState::Ready : ChecklistState::Open);
    const ChecklistState secretsState = (facts.clueLinks + facts.npcSecrets + facts.mapSecrets) > 0 ? ChecklistState::Ready : ChecklistState::Open;
    const ChecklistState rewardState = facts.rewardLinks > 0 ? ChecklistState::Ready : ChecklistState::Open;

    const std::array<ChecklistState, 7> states = {npcState, mapState, encounterState, handoutState, audioState, secretsState, rewardState};
    int readyCount = 0;
    int openCount = 0;
    int missingCount = 0;
    for (ChecklistState state : states)
    {
        if (state == ChecklistState::Ready)
            ++readyCount;
        else if (state == ChecklistState::Missing)
            ++missingCount;
        else
            ++openCount;
    }

    auto summarizeLinks = [&](const std::vector<const StoryLinkModel *> &links) {
        if (links.empty())
            return std::string{};

        std::string summary;
        const std::size_t shown = std::min<std::size_t>(links.size(), 3);
        for (std::size_t i = 0; i < shown; ++i)
        {
            if (!summary.empty())
                summary += ", ";
            const StoryLinkModel &link = *links[i];
            summary += _controller.describeEntity(link);
            if (!link.label.empty())
                summary += " (" + link.label + ")";
        }
        if (links.size() > shown)
            summary += ", +" + std::to_string(links.size() - shown) + " " + localizedUiText("more");
        return summary;
    };

    auto clueSummary = [&]() {
        std::vector<const StoryLinkModel *> clueLinks;
        for (const StoryLinkModel &link : beat.links)
        {
            if (checklistLinkIsClue(link))
                clueLinks.push_back(&link);
        }

        std::string summary = summarizeLinks(clueLinks);
        if (facts.npcSecrets > 0)
        {
            if (!summary.empty())
                summary += " | ";
            summary += std::to_string(facts.npcSecrets) + " " +
                       localizedUiText(facts.npcSecrets == 1 ? "linked NPC secret" : "linked NPC secrets");
        }
        if (facts.mapSecrets > 0)
        {
            if (!summary.empty())
                summary += " | ";
            summary += std::to_string(facts.mapSecrets) + " " +
                       localizedUiText(facts.mapSecrets == 1 ? "linked map secret" : "linked map secrets");
        }
        return summary;
    };

    std::string mapDetail = summarizeLinks(checklistLinksForType(beat, "Map"));
    if (facts.encounterMaps > 0)
    {
        if (!mapDetail.empty())
            mapDetail += " | ";
        mapDetail += std::to_string(facts.encounterMaps) + " " +
                     localizedUiText(facts.encounterMaps == 1 ? "encounter map" : "encounter maps");
    }

    std::vector<const StoryLinkModel *> rewardLinks = checklistLinksForType(beat, "Item");
    const std::vector<const StoryLinkModel *> lootLinks = checklistLinksForType(beat, "Loot");
    rewardLinks.insert(rewardLinks.end(), lootLinks.begin(), lootLinks.end());

    std::string audioDetail;
    if (facts.missingCue)
        audioDetail = localizedUiText("Selected music cue is missing from the audio library.");
    else
    {
        if (cueAsset)
            audioDetail = std::string(localizedUiText("Cue:")) + " " + cueAsset->name;
        const std::string linkedAudio = summarizeLinks(checklistLinksForType(beat, "Audio"));
        if (!linkedAudio.empty())
        {
            if (!audioDetail.empty())
                audioDetail += " | ";
            audioDetail += linkedAudio;
        }
    }

    ImGui::SeparatorText(localizedUiLabel("Scene Table Checklist").c_str());
    showHelpTooltip("A derived table-readiness checklist for the selected scene. It uses scene connections, linked entity fields, and the direct music cue.");
    const std::string checklistSelection =
        std::to_string(readyCount) + " " + localizedUiText("ready") + " / " +
        std::to_string(openCount) + " " + localizedUiText("open") + " / " +
        std::to_string(missingCount) + " " + localizedUiText("missing");
    const std::string nextAction = missingCount > 0
                                       ? "Fix missing checklist items before launching."
                                       : (openCount > 0 ? "Add optional table assets with Scene Connections below." : "Scene has every checklist category covered.");
    UiStatusActionStrip checklistStrip;
    checklistStrip.mode = "Scene Checklist";
    checklistStrip.selection = checklistSelection.c_str();
    checklistStrip.nextAction = nextAction.c_str();
    checklistStrip.help = "Open rows are empty because no matching scene connection or cue exists yet. Missing rows point to broken references.";
    drawStatusActionStrip("StorySceneChecklistStatus", checklistStrip);

    if (ImGui::BeginTable("StorySceneChecklist", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Need").c_str(), ImGuiTableColumnFlags_WidthFixed, 102.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Status").c_str(), ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Source").c_str());
        ImGui::TableHeadersRow();

        drawChecklistRow("NPCs", npcState, summarizeLinks(checklistLinksForType(beat, "NPC")), "No involved NPCs linked to this scene.");
        drawChecklistRow("Map", mapState, mapDetail, "No map linked; theater-of-mind is fine, or add a Map connection.");
        drawChecklistRow("Encounter", encounterState, summarizeLinks(checklistLinksForType(beat, "Encounter")), "No encounter linked; add one if this scene can turn tactical.");
        drawChecklistRow("Handouts", handoutState, summarizeLinks(checklistLinksForType(beat, "Handout")), "No handouts linked for player-facing reveals.");
        drawChecklistRow("Audio", audioState, audioDetail, "No cue or linked audio; add one if ambience matters.");
        drawChecklistRow("Secrets/Clues", secretsState, clueSummary(), "No reveal links or linked NPC/map secrets found.");
        drawChecklistRow("Rewards", rewardState, summarizeLinks(rewardLinks), "No item or loot links for rewards.");

        ImGui::EndTable();
    }
}
