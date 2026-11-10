#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

namespace gm::map_creator_view_detail
{
std::string terrainSelectionSummary(const gm::mapui::TerrainGroupSelection &selection, bool splitScope)
{
    const std::string kind = selection.kind == MapTileKind::Mountain || selection.kind == MapTileKind::Hill
                                 ? uiTextForLocale("Mountains / hills", "Berge / Hügel")
                                 : localizedUiLabel(mapTileKindLabel(selection.kind));
    const std::string count = std::to_string(selection.cells.size()) +
                              (selection.isPartial() ? std::string(uiTextForLocale(" of ", " von ")) +
                                                           std::to_string(selection.sourceCellCount)
                                                     : "");
    return kind + " · " + count + uiTextForLocale(" cells", " Zellen") + (splitScope ? "\n" : " · ") +
           (selection.isPartial() ? uiTextForLocale("partial area", "Teilbereich")
                                  : uiTextForLocale("whole connected area", "gesamtes zusammenhängendes Gebiet"));
}

void drawTerrainSelectionCells(ImDrawList *canvas, ImVec2 origin, float cellSize,
                               const gm::mapui::TerrainGroupSelection &selection, int dx, int dy, ImU32 edge,
                               ImU32 fill)
{
    for (const auto p : selection.cells)
    {
        const ImVec2 a(origin.x + (p.x + dx) * cellSize, origin.y + (p.y + dy) * cellSize);
        const ImVec2 b(a.x + cellSize, a.y + cellSize);
        canvas->AddRectFilled(a, b, fill);
        // A mark inside every selected cell makes holes and mixed terrain readable.
        if (cellSize >= 6.f)
            canvas->AddLine(ImVec2(a.x + cellSize * .25f, a.y + cellSize * .75f),
                            ImVec2(a.x + cellSize * .75f, a.y + cellSize * .25f), edge, 1.f);
        const auto boundary = [&](ImVec2 start, ImVec2 end) {
            canvas->AddLine(start, end, IM_COL32(20, 25, 26, 220), 4.f);
            canvas->AddLine(start, end, edge, 2.f);
        };
        if (!selection.contains(p.x - 1, p.y))
            boundary(a, ImVec2(a.x, b.y));
        if (!selection.contains(p.x + 1, p.y))
            boundary(ImVec2(b.x, a.y), b);
        if (!selection.contains(p.x, p.y - 1))
            boundary(a, ImVec2(b.x, a.y));
        if (!selection.contains(p.x, p.y + 1))
            boundary(ImVec2(a.x, b.y), b);
    }
}

void drawWorldPlaceSharingHelp(const MapModel &map)
{
    if (map.atlasLevel != MapLevel::World ||
        !ImGui::TreeNode(uiTextForLocale("Why is a place hidden?", "Warum ist ein Ort verborgen?")))
        return;
    ImGui::TextWrapped(
        "%s",
        uiTextForLocale(
            "Enabling place symbols or names does not reveal a world-map settlement or landmark by itself. Each also needs a revealed campaign place. New generated places stay hidden until you choose to add and reveal them.",
            "Orte und Markierungen sowie Namen und Beschriftungen schalten Kategorien frei. Jede Siedlung und jeder Orientierungspunkt auf der Weltkarte benötigt zusätzlich einen enthüllten Kampagnenort. Neu erzeugte Orte bleiben verborgen, bis du sie hinzufügst und enthüllst."));
    ImGui::TextWrapped(
        "%s",
        uiTextForLocale(
            "In Edit, select the place, enable Discovered, turn off GM Only and save the map. Use Add to campaign world in its inspector. For an existing link, go to Campaign > World > Places, select the place, set Visibility to Discovered or Public, then Save place. Finally review the map's player categories and fog.",
            "Wähle unter Bearbeiten den Ort, aktiviere Entdeckt, deaktiviere Nur für SL und speichere die Karte. Nutze im Inspektor Zur Kampagnenwelt hinzufügen. Bei einer bestehenden Verknüpfung: Kampagne > Welt > Orte, Ort wählen, Sichtbarkeit auf Entdeckt oder Öffentlich setzen und Ort speichern. Prüfe anschließend Spielerfreigaben und Nebel der Karte."));
    ImGui::TreePop();
}

int nameCultureIndex(const std::string &culture)
{
    const std::string normalized = mapGenNormalizedNameCulture(culture);
    for (int index = 0; index < static_cast<int>(std::size(nameCultureIds)); ++index)
        if (normalized == nameCultureIds[index])
            return index;
    return 3;
}

MapImageTraceAuthoring::Mode imageTraceMode(int index)
{
    return static_cast<MapImageTraceAuthoring::Mode>(std::clamp(index, 0, 5));
}

const char *imageTraceDefaultName(MapImageTraceAuthoring::Mode mode)
{
    switch (mode)
    {
    case MapImageTraceAuthoring::Mode::Coastline:
        return "Traced coastline";
    case MapImageTraceAuthoring::Mode::TerrainArea:
        return "Traced area";
    case MapImageTraceAuthoring::Mode::Road:
        return "Traced road";
    case MapImageTraceAuthoring::Mode::River:
        return "Traced river";
    case MapImageTraceAuthoring::Mode::Settlement:
        return "Traced settlement";
    case MapImageTraceAuthoring::Mode::PointOfInterest:
        return "Traced point of interest";
    }
    return "Traced feature";
}

std::string localizedModelPickerLabel(const char *label)
{
    if (!label)
        return {};
    const std::string_view value(label);
    if ((!value.empty() && value.front() == '<') || value == "No parent map" || value == "No linked entity")
        return localizedUiLabel(label);
    return std::string(value);
}

std::string formatMapExportMemory(std::uint64_t bytes)
{
    const double mebibytes = static_cast<double>(bytes) / (1024.0 * 1024.0);
    return formatLocalizedUi("About %.1f MiB uncompressed", mebibytes);
}

std::string formatMapPackageByteSize(std::uint64_t bytes)
{
    const gm::localization::Locale locale = gm::localization::LocalizationService::instance().locale();
    constexpr double kibibyte = 1024.0;
    constexpr double mebibyte = kibibyte * 1024.0;
    if (static_cast<double>(bytes) >= mebibyte)
        return gm::localization::formatNumber(static_cast<double>(bytes) / mebibyte, 1, locale) + " MiB";
    if (static_cast<double>(bytes) >= kibibyte)
        return gm::localization::formatNumber(static_cast<double>(bytes) / kibibyte, 1, locale) + " KiB";
    return gm::localization::formatInteger(static_cast<std::int64_t>(bytes), locale) + " B";
}

std::string mapPackageVersionLabel(int version)
{
    if (version <= 0)
        return localizedUiLabel("Not recorded");
    return localizedUiLabel("Version") + " " + std::to_string(version);
}

const char *mapPackageVisibilityLabel(const MapPackagePreview &preview)
{
    if (preview.visibleToPlayers)
        return "Visible to players";
    if (preview.knownToPlayers)
        return "Known to players";
    return "Game Master only";
}

const char *mapPackageAtlasStatusLabel(MapAtlasStatus status)
{
    switch (status)
    {
    case MapAtlasStatus::Standalone:
        return "Standalone map";
    case MapAtlasStatus::Linked:
        return "Linked to a parent map";
    case MapAtlasStatus::Stale:
        return "Parent map changed";
    case MapAtlasStatus::Invalid:
        return "Invalid atlas link";
    case MapAtlasStatus::UpdateAvailable:
        return "Parent update available";
    case MapAtlasStatus::Conflict:
        return "Atlas conflict";
    case MapAtlasStatus::Unpositioned:
        return "Parent selected, detail area not positioned";
    }
    return "Standalone map";
}

const char *mapPackageAssetKindLabel(MapPackageAssetKind kind)
{
    switch (kind)
    {
    case MapPackageAssetKind::Background:
        return "Background image";
    case MapPackageAssetKind::ImportedReference:
        return "Original imported image";
    case MapPackageAssetKind::WorkingProxy:
        return "Working preview image";
    }
    return "Map image";
}

UiButtonPairResult drawResponsiveButtonPair(const char *id, const char *firstLabel, UiButtonRole firstRole,
                                            const char *firstTooltip, const char *secondLabel, UiButtonRole secondRole,
                                            const char *secondTooltip, bool secondEnabled,
                                            const char *secondDisabledReason, bool firstEnabled,
                                            const char *firstDisabledReason)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    const std::string localizedFirst = localizedUiLabel(firstLabel);
    const std::string localizedSecond = localizedUiLabel(secondLabel);
    const int columns = gm::mapui::mapCreatorPairedActionColumnCount(
        ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(localizedFirst.c_str(), nullptr, true).x,
        ImGui::CalcTextSize(localizedSecond.c_str(), nullptr, true).x, style.FramePadding.x, style.CellPadding.x);

    UiButtonPairResult result;
    if (ImGui::BeginTable(id, columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextColumn();
        result.first =
            drawUiButton(firstLabel, firstEnabled, firstDisabledReason, firstRole, ImVec2(-1.0f, 0.0f), firstTooltip);
        ImGui::TableNextColumn();
        result.second = drawUiButton(secondLabel, secondEnabled, secondDisabledReason, secondRole, ImVec2(-1.0f, 0.0f),
                                     secondTooltip);
        ImGui::EndTable();
    }
    return result;
}

bool drawChoiceButton(const char *label, bool selected, bool enabled, const char *disabledReason, ImVec2 size)
{
    if (selected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(54, 66, 62, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(62, 80, 75, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(47, 69, 67, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(51, 157, 170, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }
    const bool pressed = drawUiButton(label, enabled, disabledReason, UiButtonRole::Secondary, size);
    if (selected)
    {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }
    return pressed;
}

bool drawMapConfirmationModal(const char *popupId, const char *message, const char *confirmLabel)
{
    setResponsiveAutoSizeWindow(560.0f);
    const std::string localizedPopupId = localizedUiLabel(popupId);
    return drawConfirmationModal(localizedPopupId.c_str(), message, confirmLabel);
}

ImU32 withAlpha(ImU32 color, int alpha)
{
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
    rgba.w = static_cast<float>(alpha) / 255.0f;
    return ImGui::ColorConvertFloat4ToU32(rgba);
}

bool beginSidePanel(const char *label, bool forceOpen)
{
    if (forceOpen)
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    return ImGui::TreeNodeEx(localizedUiLabel(label).c_str(), ImGuiTreeNodeFlags_None);
}

bool mapContainsVisibleDraftContent(const MapModel &map)
{
    if (!map.backgroundImagePath.empty() || !map.tokens.empty() || !map.regions.empty() || !map.features.empty() ||
        !map.routes.empty() || !map.generation.graph.features.empty())
        return true;

    return std::any_of(map.cells.begin(), map.cells.end(), [](const MapCellModel &cell) {
        return cell.kind != MapTileKind::Empty || cell.wayKind != MapWayKind::None || cell.fogged ||
               cell.elevation != 0 || cell.difficultTerrain || cell.blocksLineOfSight ||
               cell.movementBlockedMask != 0 || cell.lineOfSightBlockedMask != 0 ||
               cell.movementOneWayBlockedMask != 0 || cell.lineOfSightOneWayBlockedMask != 0;
    });
}

void drawPendingMapPreview(bool readyToGenerate)
{
    ImGui::SeparatorText(localizedUiLabel("Map preview").c_str());
    const float previewHeight = std::clamp(ImGui::GetContentRegionAvail().y - 16.0f, 200.0f, 520.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 32, 31, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
    if (ImGui::BeginChild("PendingMapPreview", ImVec2(0.0f, previewHeight), true))
    {
        const char *title = readyToGenerate ? "Ready for three designs" : "Your map designs will appear here";
        const char *description =
            readyToGenerate
                ? "Choose the landscape on the left and create three designs. The current draft stays unchanged until you choose one."
                : "Start with your notes, choose an idea, or create an empty world. You can adjust every generated result afterward.";
        const float contentWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x);
        const float wrapWidth = std::min(520.0f, contentWidth);
        const std::string localizedTitle = localizedUiLabel(title);
        const std::string localizedDescription = localizedUiLabel(description);
        const float titleHeight = ImGui::CalcTextSize(localizedTitle.c_str(), nullptr, false, wrapWidth).y;
        const float descriptionHeight = ImGui::CalcTextSize(localizedDescription.c_str(), nullptr, false, wrapWidth).y;
        const float contentHeight = titleHeight + ImGui::GetStyle().ItemSpacing.y + descriptionHeight;
        const float topSpacing = std::max(12.0f, (previewHeight - contentHeight) * 0.42f);
        ImGui::Dummy(ImVec2(0.0f, topSpacing));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (contentWidth - wrapWidth) * 0.5f));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(225, 173, 83, 255));
        ImGui::TextWrapped("%s", localizedTitle.c_str());
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (contentWidth - wrapWidth) * 0.5f));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", localizedDescription.c_str());
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void drawDisabledWrappedText(const char *text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", localizedUiLabel(text).c_str());
    ImGui::PopStyleColor();
}

void drawDisabledWrappedValue(const std::string &text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text.c_str());
    ImGui::PopStyleColor();
}

void drawCandidateArtPreviewToggle(bool &enabled)
{
    FormUi::Checkbox(localizedUiLabel("Preview Candidate Art (review only)").c_str(), &enabled);
    showHelpTooltip(
        "Review-only preview. Candidate art is not accepted runtime art; default rendering still uses accepted assets or documented fallbacks.");
}

bool shouldDrawOverviewFeatureLabel(const MapModel &map, const MapGeneratedFeature &feature)
{
    if (feature.name.empty())
        return false;
    if (map.atlasLevel != MapLevel::World)
        return true;

    std::string label = feature.name;
    const auto first = label.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return false;
    const auto last = label.find_last_not_of(" \t\r\n");
    label = label.substr(first, last - first + 1);
    if (label.size() > 44 || label.find_first_of(".;:\n\r") != std::string::npos)
        return false;

    std::string lower = label;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    const std::array<std::string_view, 12> genericPrefixes = {"settlement ", "landmark ",   "siedlung ", "landmarke ",
                                                              "generated ",  "erzeugt ",    "nation ",   "city ",
                                                              "capital ",    "hauptstadt ", "stadt ",    "ort "};
    for (const std::string_view prefix : genericPrefixes)
    {
        if (lower.rfind(prefix, 0) != 0)
            continue;
        const std::string_view suffix(lower.data() + prefix.size(), lower.size() - prefix.size());
        if (suffix.empty() || std::all_of(suffix.begin(), suffix.end(), [](unsigned char character) {
                return std::isdigit(character) != 0 || std::isspace(character) != 0;
            }))
            return false;
    }

    // A cartographic label is a short proper name, not a sentence copied out of
    // the free-form brief. This also protects old drafts created by the previous
    // parser, while the corrected parser supplies proper names to new maps.
    int wordCount = 1;
    for (const char character : label)
        if (std::isspace(static_cast<unsigned char>(character)) != 0)
            ++wordCount;
    const std::array<std::string_view, 6> noteFragments = {" teilen sich ", " share ",  " es gibt ",
                                                           " there is ",    " namens ", " named "};
    return wordCount <= 5 && std::none_of(noteFragments.begin(), noteFragments.end(), [&](std::string_view fragment) {
               return lower.find(fragment) != std::string::npos;
           });
}

} // namespace gm::map_creator_view_detail
