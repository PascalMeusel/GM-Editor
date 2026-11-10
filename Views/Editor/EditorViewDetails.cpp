#include "Views/Editor/EditorViewDetails.hpp"

namespace gm::editor_view_detail
{
const char *saveUiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german
                                                                                                          : english;
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

bool containsText(const std::string &haystack, const std::string &needle)
{
    if (needle.empty())
        return true;
    return lowerCopy(haystack).find(lowerCopy(needle)) != std::string::npos;
}

int mainPlatformMonitorIndex()
{
    const ImGuiPlatformIO &platform = ImGui::GetPlatformIO();
    if (platform.Monitors.empty())
        return -1;
    const ImGuiViewport *mainViewport = ImGui::GetMainViewport();
    const ImVec2 center(mainViewport->Pos.x + mainViewport->Size.x * 0.5f,
                        mainViewport->Pos.y + mainViewport->Size.y * 0.5f);
    for (int monitorIndex = 0; monitorIndex < platform.Monitors.Size; ++monitorIndex)
    {
        const ImGuiPlatformMonitor &monitor = platform.Monitors[monitorIndex];
        if (center.x >= monitor.MainPos.x && center.y >= monitor.MainPos.y &&
            center.x < monitor.MainPos.x + monitor.MainSize.x && center.y < monitor.MainPos.y + monitor.MainSize.y)
            return monitorIndex;
    }
    return 0;
}

bool sidebarEntryMatches(const SidebarNavEntry &entry, const std::string &query)
{
    return containsText(entry.group, query) || containsText(entry.label, query) || containsText(entry.keywords, query);
}

ImFont *getBoldFont()
{
    ImFontAtlas *fonts = ImGui::GetIO().Fonts;
    return fonts->Fonts.Size > 1 ? fonts->Fonts[1] : nullptr;
}

void pushBoldFont()
{
    if (ImFont *font = getBoldFont())
        ImGui::PushFont(font);
}

void popBoldFont()
{
    if (getBoldFont())
        ImGui::PopFont();
}

void drawTransportIcon(TransportIconKind kind, const ImVec2 &min, const ImVec2 &max, ImU32 color)
{
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    const float size = std::min(max.x - min.x, max.y - min.y) * 0.42f;

    switch (kind)
    {
    case TransportIconKind::Play:
        drawList->AddTriangleFilled(ImVec2(center.x - size * 0.45f, center.y - size * 0.68f),
                                    ImVec2(center.x - size * 0.45f, center.y + size * 0.68f),
                                    ImVec2(center.x + size * 0.68f, center.y), color);
        break;
    case TransportIconKind::Pause: {
        const float barWidth = size * 0.32f;
        const float gap = size * 0.22f;
        const float height = size * 1.28f;
        drawList->AddRectFilled(ImVec2(center.x - gap - barWidth, center.y - height * 0.5f),
                                ImVec2(center.x - gap, center.y + height * 0.5f), color, 1.0f);
        drawList->AddRectFilled(ImVec2(center.x + gap, center.y - height * 0.5f),
                                ImVec2(center.x + gap + barWidth, center.y + height * 0.5f), color, 1.0f);
        break;
    }
    case TransportIconKind::Stop: {
        const float half = size * 0.58f;
        drawList->AddRectFilled(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half),
                                color, 1.0f);
        break;
    }
    case TransportIconKind::Next: {
        drawList->AddTriangleFilled(ImVec2(center.x - size * 0.68f, center.y - size * 0.68f),
                                    ImVec2(center.x - size * 0.68f, center.y + size * 0.68f),
                                    ImVec2(center.x + size * 0.18f, center.y), color);
        const float barWidth = size * 0.22f;
        drawList->AddRectFilled(ImVec2(center.x + size * 0.38f, center.y - size * 0.68f),
                                ImVec2(center.x + size * 0.38f + barWidth, center.y + size * 0.68f), color, 1.0f);
        break;
    }
    }
}

bool drawTransportIconButton(const char *id, TransportIconKind kind, bool enabled, const char *disabledReason,
                             UiButtonRole role, const char *tooltip)
{
    UiButtonStyleScope style(role);
    if (!enabled)
        ImGui::BeginDisabled(true);

    const bool pressed = ImGui::Button(id, ImVec2(44.0f, 0.0f));
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();

    if (!enabled)
        ImGui::EndDisabled();

    const ImU32 iconColor =
        ImGui::GetColorU32(enabled ? ImVec4(0.95f, 0.98f, 0.98f, 1.0f) : ImVec4(0.48f, 0.52f, 0.56f, 1.0f));
    drawTransportIcon(kind, itemMin, itemMax, iconColor);
    showHelpTooltip(enabled ? tooltip : uiTextOrFallback(disabledReason, "This action is not available right now."),
                    enabled ? ImGuiHoveredFlags_DelayShort
                            : ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled);
    return enabled && pressed;
}

void renderCenteredTextColored(const ImVec4 &color, const std::string &text)
{
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float textWidth = ImGui::CalcTextSize(text.c_str()).x;
    if (availableWidth > textWidth)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availableWidth - textWidth) * 0.5f);
    ImGui::TextColored(color, localizedUiLabel("%s").c_str(), text.c_str());
}

void renderSectionLabel(const char *label)
{
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    pushBoldFont();
    ImGui::TextColored(kWarmAccent, localizedUiLabel("%s").c_str(), localizedUiText(label));
    popBoldFont();
    ImGui::Separator();
}

bool renderLauncherButton(const char *label)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.29f, 0.33f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.43f, 0.47f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.56f, 0.58f, 1.0f));
    pushBoldFont();
    const std::string localizedLabel = localizedUiLabel(label);
    const bool pressed = ImGui::Button(localizedLabel.c_str(), ImVec2(-1.0f, 0.0f));
    popBoldFont();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    return pressed;
}

bool renderLauncherButton(const char *label, const char *tooltip)
{
    const bool pressed = renderLauncherButton(label);
    showHelpTooltip(tooltip);
    return pressed;
}

void renderMetric(const char *label, int value)
{
    ImGui::TableNextColumn();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanelBg);
    ImGui::BeginChild(label, ImVec2(0.0f, 74.0f), true, ImGuiWindowFlags_NoScrollbar);
    renderCenteredTextColored(kMutedText, localizedUiText(label));
    pushBoldFont();
    renderCenteredTextColored(kAccent, std::to_string(value));
    popBoldFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void renderLabeledCount(const char *label, int value, const ImVec4 &valueColor)
{
    pushBoldFont();
    ImGui::TextColored(kMutedText, localizedUiLabel("%s").c_str(), localizedUiText(label));
    popBoldFont();
    ImGui::SameLine();
    ImGui::TextColored(valueColor, localizedUiLabel("%d").c_str(), value);
}

void renderHubLine(const char *label, const std::string &value)
{
    ImGui::TextColored(kMutedText, localizedUiLabel("%s").c_str(), localizedUiText(label));
    ImGui::SameLine();
    tableTextWrapped(value);
}

void renderHubPanel(const char *id, const char *title, const std::vector<std::pair<std::string, std::string>> &lines)
{
    ImGui::TableNextColumn();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanelBg);
    ImGui::BeginChild(id, ImVec2(0.0f, 132.0f), true);
    pushBoldFont();
    ImGui::TextColored(kWarmAccent, localizedUiLabel("%s").c_str(), localizedUiText(title));
    popBoldFont();
    ImGui::Separator();
    for (const auto &line : lines)
        renderHubLine(line.first.c_str(), line.second);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void renderSaveChangeSummary(const char *label, const std::vector<std::string> &changes)
{
    if (changes.empty())
        return;

    ImGui::TextColored(kMutedText, localizedUiLabel("%s").c_str(), localizedUiText(label));
    for (const std::string &change : changes)
        ImGui::BulletText("%s", change.c_str());
}

std::string knowledgeGraphEdgeKey(const CampaignGraphEdge &edge)
{
    return edge.fromId + "|" + edge.relationship + "|" + edge.toId;
}

const CampaignGraphNode *findGraphNode(const CampaignKnowledgeGraph &graph, const EntityId &id)
{
    const auto found = std::find_if(graph.nodes.begin(), graph.nodes.end(),
                                    [&](const CampaignGraphNode &node) { return node.id == id; });
    return found == graph.nodes.end() ? nullptr : &(*found);
}

const CampaignGraphEdge *findGraphEdge(const CampaignKnowledgeGraph &graph, const std::string &key)
{
    const auto found = std::find_if(graph.edges.begin(), graph.edges.end(),
                                    [&](const CampaignGraphEdge &edge) { return knowledgeGraphEdgeKey(edge) == key; });
    return found == graph.edges.end() ? nullptr : &(*found);
}

bool edgeTouchesNode(const CampaignGraphEdge &edge, const EntityId &nodeId)
{
    return nodeId.empty() || edge.fromId == nodeId || edge.toId == nodeId;
}

std::string graphEdgeSummary(const CampaignGraphEdge &edge)
{
    return edge.summary.empty() ? edge.fromLabel + " -> " + edge.toLabel + " [" + edge.relationship + "]"
                                : edge.summary;
}

const char *healthSeverityLabel(CampaignHealthSeverity severity)
{
    switch (severity)
    {
    case CampaignHealthSeverity::Error:
        return "Error";
    case CampaignHealthSeverity::Warning:
        return "Warning";
    case CampaignHealthSeverity::Info:
    default:
        return "Info";
    }
}

ImVec4 healthSeverityColor(CampaignHealthSeverity severity)
{
    switch (severity)
    {
    case CampaignHealthSeverity::Error:
        return ImVec4(0.95f, 0.42f, 0.38f, 1.0f);
    case CampaignHealthSeverity::Warning:
        return kWarmAccent;
    case CampaignHealthSeverity::Info:
    default:
        return kMutedText;
    }
}

const char *healthActionLabel(CampaignHealthAction action)
{
    switch (action)
    {
    case CampaignHealthAction::SaveCampaign:
        return "Save";
    case CampaignHealthAction::RebuildIndex:
        return "Rebuild";
    case CampaignHealthAction::OpenSearch:
        return "Search";
    case CampaignHealthAction::OpenAudioLibrary:
        return "Audio Repair";
    case CampaignHealthAction::None:
    default:
        return "No Action";
    }
}

bool containsDiagnosticKey(const std::vector<std::string> &keys, const std::string &key)
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

void removeDiagnosticKey(std::vector<std::string> &keys, const std::string &key)
{
    keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
}

void addDiagnosticKey(std::vector<std::string> &keys, const std::string &key)
{
    if (!containsDiagnosticKey(keys, key))
        keys.push_back(key);
}

std::string campaignHealthStateLabel(const std::vector<std::string> &ignoredKeys,
                                     const std::vector<std::string> &resolvedKeys, const std::string &key)
{
    if (containsDiagnosticKey(resolvedKeys, key))
        return "Resolved";
    if (containsDiagnosticKey(ignoredKeys, key))
        return "Ignored";
    return "Open";
}

std::vector<CampaignHealthDiagnosticRow> buildCampaignHealthRows(const Campaign &campaign,
                                                                 const ProjectSaveState &saveState,
                                                                 const std::vector<CampaignBrokenReference> &brokenRefs,
                                                                 const std::vector<ProjectLogEntry> &recentProblems)
{
    std::vector<CampaignHealthDiagnosticRow> rows;
    for (std::size_t index = 0; index < saveState.lastSaveWarnings.size(); ++index)
        rows.push_back({"save:follow_up:" + std::to_string(index), CampaignHealthSeverity::Warning,
                        saveState.lastError.empty() ? saveUiText("Saved with warnings", "Gespeichert mit Warnungen")
                                                    : saveUiText("Save warnings", "Speicherwarnungen"),
                        saveState.lastSaveWarnings[index], CampaignHealthAction::SaveCampaign});
    if (!saveState.lastError.empty())
    {
        rows.push_back({"save:last_error", CampaignHealthSeverity::Error, "Campaign", saveState.lastError,
                        CampaignHealthAction::SaveCampaign});
    }
    if (saveState.dirty)
    {
        rows.push_back({"save:dirty", CampaignHealthSeverity::Warning, "Campaign",
                        "Unsaved campaign changes are pending.", CampaignHealthAction::SaveCampaign});
    }
    if (!saveState.lastBackupError.empty())
    {
        rows.push_back({"save:backup_error", CampaignHealthSeverity::Warning, "Backups", saveState.lastBackupError,
                        CampaignHealthAction::SaveCampaign});
    }
    if (!saveState.lastRecoveryError.empty())
    {
        rows.push_back({"save:recovery_error", CampaignHealthSeverity::Warning, "Recovery", saveState.lastRecoveryError,
                        CampaignHealthAction::SaveCampaign});
    }

    for (const CampaignBrokenReference &brokenRef : brokenRefs)
    {
        const std::string relationship = localizedUiLabel(brokenRef.relationship.c_str());
        rows.push_back(
            {"broken:" + brokenRef.sourceEntityId + ":" + brokenRef.relationship + ":" + brokenRef.missingEntityId,
             CampaignHealthSeverity::Error,
             brokenRef.sourceTitle + " [" + localizedUiLabel(brokenRef.sourceType.c_str()) + "]",
             formatLocalizedUi("Missing %s target: %s", relationship.c_str(), brokenRef.missingEntityId.c_str()),
             CampaignHealthAction::OpenSearch});
    }

    for (const AudioAssetModel &asset : campaign.audioAssets)
    {
        if (!asset.missingFile)
            continue;
        rows.push_back({"audio:" + asset.metadata.id, CampaignHealthSeverity::Warning,
                        (asset.name.empty()
                             ? (asset.originalFileName.empty() ? std::string("Audio asset") : asset.originalFileName)
                             : asset.name) +
                            " [Audio]",
                        "Audio file is missing or needs relinking.", CampaignHealthAction::OpenAudioLibrary});
    }

    for (const ProjectLogEntry &entry : recentProblems)
    {
        rows.push_back(
            {"log:" + entry.occurredAtUtc + ":" + entry.source + ":" + entry.message,
             entry.level == ProjectLogLevel::Error ? CampaignHealthSeverity::Error : CampaignHealthSeverity::Warning,
             entry.source, entry.message, CampaignHealthAction::RebuildIndex});
    }

    if (rows.empty())
    {
        rows.push_back({"health:clear", CampaignHealthSeverity::Info, "Campaign",
                        "No save, reference, asset, or warning diagnostics need attention.",
                        CampaignHealthAction::None});
    }
    return rows;
}

}
