#include "Views/Editor/EditorViewDetails.hpp"

namespace gm::editor_view_detail
{
bool renderTaskButton(const char *label, const char *tooltip, UiButtonRole role, bool selected)
{
    UiButtonStyleScope style(role);
    if (selected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
    }
    const std::string localizedLabel = localizedUiLabel(label);
    const bool pressed = ImGui::Button(localizedLabel.c_str(), ImVec2(-1.0f, 0.0f));
    showHelpTooltip(tooltip);
    if (selected)
        ImGui::PopStyleColor(3);
    return pressed;
}

bool renderWorkflowButton(const char *label, const char *tooltip, UiButtonRole role, bool selected)
{
    return renderTaskButton(label, tooltip, role, selected);
}

bool renderWorkflowButton(const char *label, bool enabled, const char *disabledReason, const char *tooltip,
                          UiButtonRole role, bool selected)
{
    if (!enabled)
    {
        return drawUiButton(label, false, disabledReason, role, ImVec2(-1.0f, 0.0f), tooltip);
    }
    return renderWorkflowButton(label, tooltip, role, selected);
}

bool renderSidebarGroupButton(const char *label, bool &open, bool containsActive)
{
    const std::string buttonLabel = std::string("   ") + localizedUiText(label) + "###SidebarGroup" + label;
    UiButtonStyleScope style(containsActive ? UiButtonRole::Primary : UiButtonRole::Secondary);
    if (containsActive)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
    }

    const bool pressed = ImGui::Button(buttonLabel.c_str(), ImVec2(-1.0f, 0.0f));
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float centerY = (min.y + max.y) * 0.5f;
    const float centerX = min.x + 13.0f;
    const float size = 4.8f;
    const ImU32 chevronColor = ImGui::GetColorU32(containsActive ? kWarmAccent : ImVec4(0.72f, 0.82f, 0.84f, 1.0f));
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    if (open)
    {
        drawList->AddLine(ImVec2(centerX - size, centerY - size * 0.45f), ImVec2(centerX, centerY + size * 0.55f),
                          chevronColor, 1.8f);
        drawList->AddLine(ImVec2(centerX, centerY + size * 0.55f), ImVec2(centerX + size, centerY - size * 0.45f),
                          chevronColor, 1.8f);
    }
    else
    {
        drawList->AddLine(ImVec2(centerX - size * 0.45f, centerY - size), ImVec2(centerX + size * 0.55f, centerY),
                          chevronColor, 1.8f);
        drawList->AddLine(ImVec2(centerX + size * 0.55f, centerY), ImVec2(centerX - size * 0.45f, centerY + size),
                          chevronColor, 1.8f);
    }
    showHelpTooltip(open ? "Collapse this workflow group." : "Expand this workflow group.");

    if (containsActive)
        ImGui::PopStyleColor(3);
    if (pressed)
        open = !open;
    return pressed;
}

SidebarSubmenuFrame beginSidebarSubmenuFrame(const char *id, bool containsActive)
{
    ImGui::PushID(id);
    ImGui::Indent(12.0f);
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    ImGui::Indent(10.0f);
    return SidebarSubmenuFrame{id, containsActive};
}

void endSidebarSubmenuFrame(const SidebarSubmenuFrame &frame)
{
    ImGui::Unindent(10.0f);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    ImGui::EndGroup();

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImU32 borderColor = ImGui::GetColorU32(frame.containsActive ? kAccent : ImVec4(0.36f, 0.42f, 0.45f, 1.0f));
    const ImU32 accentColor =
        ImGui::GetColorU32(frame.containsActive ? kWarmAccent : ImVec4(0.52f, 0.80f, 0.76f, 0.65f));
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRect(min, max, borderColor, 4.0f, 0, 1.4f);
    drawList->AddLine(ImVec2(min.x + 5.0f, min.y + 6.0f), ImVec2(min.x + 5.0f, max.y - 6.0f), accentColor, 2.0f);

    ImGui::Unindent(12.0f);
    ImGui::PopID();
}

void renderSidebarEntryButton(const SidebarNavEntry &entry, const std::string &activeWorkflowLabel)
{
    ImGui::Indent(6.0f);
    ImGui::PushID((std::string(entry.group) + "::" + entry.label).c_str());
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.20f, 0.21f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.31f, 0.33f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.13f, 0.37f, 0.40f, 1.0f));
    if (renderWorkflowButton(entry.label, entry.enabled, entry.disabledReason, "Open this workflow.", entry.role,
                             activeWorkflowLabel == entry.label))
        entry.open();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImGui::PopID();
    ImGui::Unindent(6.0f);
}

bool groupContainsActiveWorkflow(const std::vector<SidebarNavEntry> &entries, const char *group,
                                 const std::string &activeWorkflowLabel)
{
    return std::any_of(entries.begin(), entries.end(), [&](const SidebarNavEntry &entry) {
        return std::string(entry.group) == group && activeWorkflowLabel == entry.label;
    });
}

void renderSidebarGroup(const char *label, bool &open, const std::vector<SidebarNavEntry> &entries,
                        const std::string &activeWorkflowLabel)
{
    const bool containsActive = groupContainsActiveWorkflow(entries, label, activeWorkflowLabel);
    renderSidebarGroupButton(label, open, containsActive);
    if (!open)
        return;

    const SidebarSubmenuFrame submenuFrame = beginSidebarSubmenuFrame(label, containsActive);
    for (const SidebarNavEntry &entry : entries)
    {
        if (std::string(entry.group) == label)
            renderSidebarEntryButton(entry, activeWorkflowLabel);
    }
    endSidebarSubmenuFrame(submenuFrame);
}

const SidebarNavEntry *findSidebarEntry(const std::vector<SidebarNavEntry> &entries, const char *label)
{
    const auto found = std::find_if(entries.begin(), entries.end(),
                                    [&](const SidebarNavEntry &entry) { return std::string(entry.label) == label; });
    return found == entries.end() ? nullptr : &(*found);
}

void addRecommendedSidebarEntry(std::vector<const SidebarNavEntry *> &recommendations,
                                const std::vector<SidebarNavEntry> &entries, const char *label)
{
    if (recommendations.size() >= 3)
        return;

    const SidebarNavEntry *entry = findSidebarEntry(entries, label);
    if (!entry || !entry->enabled)
        return;

    const auto duplicate = std::find(recommendations.begin(), recommendations.end(), entry);
    if (duplicate == recommendations.end())
        recommendations.push_back(entry);
}

void renderSidebarRecommendedActions(const Campaign &campaign, const ProjectSaveState &saveState,
                                     const std::vector<SidebarNavEntry> &entries,
                                     const std::string &activeWorkflowLabel)
{
    std::vector<const SidebarNavEntry *> recommendations;

    if (saveState.dirty)
        addRecommendedSidebarEntry(recommendations, entries, "Save");

    if (activeSession(campaign))
    {
        addRecommendedSidebarEntry(recommendations, entries, "Live Dashboard");
        addRecommendedSidebarEntry(recommendations, entries, "Combat");
        addRecommendedSidebarEntry(recommendations, entries, "GM Screen");
    }
    else if (campaign.characters.empty())
    {
        addRecommendedSidebarEntry(recommendations, entries, "New Player");
        addRecommendedSidebarEntry(recommendations, entries, "Story Board");
        addRecommendedSidebarEntry(recommendations, entries, "New Map");
    }
    else if (campaign.storyThreads.empty())
    {
        addRecommendedSidebarEntry(recommendations, entries, "Story Board");
        addRecommendedSidebarEntry(recommendations, entries, "Party");
        addRecommendedSidebarEntry(recommendations, entries, "New Map");
    }
    else if (campaign.maps.empty())
    {
        addRecommendedSidebarEntry(recommendations, entries, "New Map");
        addRecommendedSidebarEntry(recommendations, entries, "Story Board");
        addRecommendedSidebarEntry(recommendations, entries, "Live Dashboard");
    }
    else
    {
        addRecommendedSidebarEntry(recommendations, entries, "Story Board");
        addRecommendedSidebarEntry(recommendations, entries, "Party");
        addRecommendedSidebarEntry(recommendations, entries, "Live Dashboard");
    }

    addRecommendedSidebarEntry(recommendations, entries, "Find Campaign");
    addRecommendedSidebarEntry(recommendations, entries, "Campaign Home");

    renderSectionLabel("Recommended");
    for (const SidebarNavEntry *entry : recommendations)
        renderSidebarEntryButton(*entry, activeWorkflowLabel);
}

void renderSidebarWorkflowHeader(const Campaign &campaign, const std::string &modeLabel,
                                 const std::string &nextActionLabel)
{
    pushBoldFont();
    ImGui::TextColored(kAccent, localizedUiLabel("Active Workflow").c_str());
    popBoldFont();
    if (ImGui::BeginTable("SidebarActiveWorkflowHeader", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Label").c_str(), ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Value").c_str(), ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(kMutedText, localizedUiLabel("Campaign").c_str());
        ImGui::TableNextColumn();
        tableTextWrapped(campaign.name.empty() ? localizedUiText("Campaign") : campaign.name);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(kMutedText, localizedUiLabel("Mode").c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(localizedUiText(modeLabel.c_str()));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(kMutedText, localizedUiLabel("Next").c_str());
        ImGui::TableNextColumn();
        tableTextWrapped(localizedUiText(nextActionLabel.c_str()));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(kMutedText, localizedUiLabel("Ruleset").c_str());
        ImGui::TableNextColumn();
        tableTextWrapped(campaign.ruleSet ? campaign.ruleSet->getName() : localizedUiText("None"));

        ImGui::EndTable();
    }
}

void renderSidebarSearchResults(const std::string &query, const std::vector<SidebarNavEntry> &entries,
                                const std::string &activeWorkflowLabel)
{
    if (query.empty())
        return;

    renderSectionLabel("Matching Tools");
    int shown = 0;
    for (const SidebarNavEntry &entry : entries)
    {
        if (!sidebarEntryMatches(entry, query))
            continue;

        const std::string buttonLabel =
            std::string(localizedUiText(entry.label)) + "##SidebarSearchResult" + std::to_string(shown);
        bool pressed = false;
        if (entry.enabled)
        {
            pressed =
                drawSmallUiButton(buttonLabel.c_str(), entry.role, "Open this tool from the sidebar search result.");
        }
        else
        {
            UiButtonStyleScope style(entry.role);
            ImGui::BeginDisabled(true);
            ImGui::SmallButton(buttonLabel.c_str());
            ImGui::EndDisabled();
            showHelpTooltip(entry.disabledReason, ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled);
        }

        if (pressed)
            entry.open();
        ImGui::SameLine();
        const std::string groupLabel = std::string(localizedUiText(entry.group)) +
                                       (entry.enabled ? "" : uiTextForLocale(" - unavailable", " – nicht verfügbar"));
        ImGui::TextColored(kMutedText, localizedUiLabel("%s").c_str(), groupLabel.c_str());
        ++shown;
        if (shown >= 8)
        {
            ImGui::TextDisabled(localizedUiLabel("Refine the filter for more results.").c_str());
            break;
        }
    }

    if (shown == 0)
        ImGui::TextDisabled(localizedUiLabel("No matching tool.").c_str());
}

void renderBookmarkLauncherList(EditorWindowController &controller,
                                const std::vector<CampaignEntityBookmarkModel> &bookmarks, const char *emptyText,
                                const char *prefix, UiButtonRole role, std::size_t limit)
{
    if (bookmarks.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), emptyText);
        return;
    }

    std::size_t shown = 0;
    for (const CampaignEntityBookmarkModel &bookmark : bookmarks)
    {
        if (shown >= limit)
            break;

        const std::string label = std::string(prefix) + (bookmark.label.empty() ? "<Untitled>" : bookmark.label) +
                                  "##LauncherBookmark" + bookmark.entityId + prefix;
        if (drawSmallUiButton(label.c_str(), role, "Open this entity and record it as recently used."))
            controller.executePaletteCommand(CampaignCommandPaletteItem{"open:" + bookmark.entityId, bookmark.label,
                                                                        bookmark.entityType, bookmark.entityId,
                                                                        bookmark.entityType});
        ImGui::SameLine();
        const std::string opensText =
            bookmark.openCount > 0 ? " | " + formatLocalizedUi("%d opens", bookmark.openCount) : "";
        const std::string entityType = localizedUiLabel(bookmark.entityType.c_str());
        ImGui::TextColored(kMutedText, localizedUiLabel("%s%s").c_str(), entityType.c_str(), opensText.c_str());
        ++shown;
    }

    if (bookmarks.size() > shown)
        ImGui::TextDisabled(localizedUiLabel("+%d more in Global Search").c_str(),
                            static_cast<int>(bookmarks.size() - shown));
}
}
