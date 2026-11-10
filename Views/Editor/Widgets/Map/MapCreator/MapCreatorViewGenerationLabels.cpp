#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

namespace gm::map_creator_view_detail
{
const char *sizePresetLabel(MapSizePreset preset)
{
    switch (preset)
    {
    case MapSizePreset::Small:
        return "Small";
    case MapSizePreset::Standard:
        return "Standard";
    case MapSizePreset::Large:
        return "Large";
    case MapSizePreset::Custom:
        return "Custom";
    }
    return "Standard";
}

const char *sizePresetDescription(MapSizePreset preset)
{
    switch (preset)
    {
    case MapSizePreset::Small:
        return "Compact and quick to overview.";
    case MapSizePreset::Standard:
        return "Recommended for most campaign maps.";
    case MapSizePreset::Large:
        return "More room for places, routes, and exploration.";
    case MapSizePreset::Custom:
        return "Choose exact cell dimensions under advanced settings.";
    }
    return "Recommended for most campaign maps.";
}

const char *generationIssueMessage(const MapGenerationIssue &issue)
{
    if (issue.code == "route.disconnected")
        return "The main route is interrupted.";
    if (issue.code == "terrain.disconnected")
        return "The usable map surface is split into disconnected areas.";
    if (issue.code == "settlement.no_access")
        return "At least one structure has no usable approach.";
    if (issue.code == "river.shared_requirement")
        return "The required great river does not yet connect all named nations to a valid outlet.";
    if (issue.code == "river.disconnected")
        return "The main river is interrupted or has no valid outlet.";
    if (issue.code == "bridge.invalid")
        return "A bridge does not connect both banks correctly.";
    if (issue.code == "road.detour")
        return "The road network contains long detours.";
    if (issue.code == "composition.empty")
        return "Large areas have no visual or playable point of interest.";
    if (issue.code == "settlement.requested_count_missing")
        return "The requested number of places could not be placed on usable land.";
    if (issue.code == "composition.cluttered")
        return "Too many small details compete with the important map features.";
    if (issue.code == "art.wrong_scale")
        return "The map contains details that belong to a closer zoom level.";
    if (issue.code == "art.fallback")
        return "Some visual roles still use fallback art.";
    if (issue.code == "request.include_exclude_conflict")
        return "The same element is both required and excluded.";
    if (issue.code == "request.constraint_excluded")
        return "A required placed element is also excluded.";
    if (issue.code == "request.must_note_untyped")
        return "A required note must be confirmed as a fact first.";
    if (issue.code == "request.nation_count_invalid")
        return "The number of nations is not a valid whole number.";
    if (issue.code == "request.nation_count_conflict")
        return "The nation count is smaller than the list of named nations.";
    if (issue.code == "request.relative_relation_invalid")
        return "The spatial relationship is incomplete or ambiguous.";
    if (issue.code == "request.relative_relation_not_executable")
        return "This required spatial relationship cannot be placed automatically yet.";
    if (issue.code == "requirement.constraint_missing")
        return "A required placed feature could not be created.";
    if (issue.code == "requirement.element_missing")
        return "A required element is missing from the map.";
    if (issue.code == "fact.must_unsatisfied")
        return "A required fact is not satisfied.";
    if (issue.code == "fact.prefer_adjusted")
        return "A preferred fact was adjusted to keep the map valid.";
    if (issue.code == "quality.minimum_not_met")
        return "The design did not reach the minimum quality in every relevant category.";
    return issue.severity == MapGenerationIssueSeverity::Critical ? "A required map check failed."
                                                                  : "A map quality check needs attention.";
}

const char *generationIssueAction(const MapGenerationIssue &issue)
{
    if (issue.code == "route.disconnected")
        return "Create new roads or move a route endpoint.";
    if (issue.code == "terrain.disconnected")
        return "Try another design or add a valid crossing.";
    if (issue.code == "settlement.no_access")
        return "Connect the affected place to the road network.";
    if (issue.code == "river.shared_requirement")
        return "Keep the named nations and capital, then try new designs.";
    if (issue.code == "river.disconnected")
        return "Create a new river while keeping the settlements.";
    if (issue.code == "bridge.invalid")
        return "Move the crossing or create new bridges.";
    if (issue.code == "road.detour")
        return "Shorten secondary roads with automatic repair.";
    if (issue.code == "composition.empty")
        return "Add landmarks or try a different composition.";
    if (issue.code == "settlement.requested_count_missing")
        return "Choose another design, reduce the place count, or make more land usable.";
    if (issue.code == "composition.cluttered")
        return "Reduce small details for this map scale.";
    if (issue.code == "art.wrong_scale")
        return "Remove close-range details or create a detail map.";
    if (issue.code == "art.fallback")
        return "Choose the production style or repair missing assets.";
    if (issue.code == "request.include_exclude_conflict")
        return "Keep the element required or excluded, but not both.";
    if (issue.code == "request.constraint_excluded")
        return "Remove the exclusion or make the placement optional.";
    if (issue.code == "request.must_note_untyped")
        return "Confirm the note as a typed fact before generation.";
    if (issue.code == "request.nation_count_invalid")
        return "Enter a whole number of nations.";
    if (issue.code == "request.nation_count_conflict")
        return "Increase the nation count or remove one of the named nations.";
    if (issue.code == "request.relative_relation_invalid")
        return "Review the relationship and identify every referenced place.";
    if (issue.code == "request.relative_relation_not_executable")
        return "Change this relationship to a preference or position the places manually.";
    if (issue.code == "requirement.constraint_missing" || issue.code == "requirement.element_missing")
        return "Regenerate the requested feature or edit the required fact.";
    if (issue.code == "fact.must_unsatisfied")
        return "Regenerate the map or edit the conflicting required fact.";
    if (issue.code == "fact.prefer_adjusted")
        return "Review the adjustment or change the preferred fact to required.";
    if (issue.code == "quality.minimum_not_met")
        return "Try another design or adjust the facts.";
    return "Review the facts or try new designs.";
}

float mapCreatorMinimumZoom(MapLevel level)
{
    switch (level)
    {
    case MapLevel::World:
        return 0.10f;
    case MapLevel::Region:
        return 0.18f;
    case MapLevel::Site:
        return 0.28f;
    case MapLevel::Tactical:
        return 0.50f;
    }
    return 0.18f;
}

GenerationPreviewSummary generationPreviewSummary(const MapModel &map)
{
    GenerationPreviewSummary summary;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        summary.roads +=
            feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail;
        summary.rivers += feature.kind == MapGeneratedFeatureKind::River;
        summary.settlements += feature.kind == MapGeneratedFeatureKind::Settlement;
        summary.landmarks += feature.kind == MapGeneratedFeatureKind::Landmark;
    }
    return summary;
}

std::pair<const char *, int> generationPreviewStrength(const MapGenerationQualityReport &quality)
{
    std::pair<const char *, int> strongest{"Composition", quality.compositionPercent};
    const std::array<std::pair<const char *, int>, 3> alternatives = {
        {{"Roads", quality.roadEfficiencyPercent},
         {"Rivers", quality.hydrologyPercent},
         {"Settlement", quality.settlementSuitabilityPercent}}};
    for (const auto &candidate : alternatives)
        if (candidate.second > strongest.second)
            strongest = candidate;
    return strongest;
}

std::vector<const MapGenerationIssue *> generationPreviewReviewIssues(const MapGenerationResult &candidate)
{
    std::vector<const MapGenerationIssue *> issues;
    std::vector<std::string> identities;
    const auto append = [&](const std::vector<MapGenerationIssue> &source) {
        for (const MapGenerationIssue &issue : source)
        {
            if (issue.severity == MapGenerationIssueSeverity::Critical)
                continue;
            const std::string identity = issue.code.empty() ? issue.message : issue.code;
            if (std::find(identities.begin(), identities.end(), identity) != identities.end())
                continue;
            identities.push_back(identity);
            issues.push_back(&issue);
        }
    };
    append(candidate.validation.warnings);
    append(candidate.quality.issues);
    return issues;
}

std::string generationCompositionLabel(const MapModel &map)
{
    int water = 0;
    int forest = 0;
    int mountain = 0;
    for (const MapCellModel &cell : map.cells)
    {
        water += cell.kind == MapTileKind::Water;
        forest += cell.kind == MapTileKind::Forest;
        mountain += cell.kind == MapTileKind::Mountain;
    }
    const int area = std::max(1, map.width * map.height);
    const GenerationPreviewSummary summary = generationPreviewSummary(map);
    const int land = std::max(1, area - water);
    return std::to_string((area - water) * 100 / area) + "% " + localizedUiLabel("Land") + " · " +
           std::to_string(forest * 100 / land) + "% " + localizedUiLabel("Forest") + " · " +
           std::to_string(mountain * 100 / land) + "% " + localizedUiLabel("Mountains") + " · " +
           std::to_string(summary.roads) + " " + localizedUiLabel("Roads");
}

void drawGenerationPreviewLegend()
{
    const int columns = ImGui::GetContentRegionAvail().x >= 760.0f ? 4 : 2;
    if (!ImGui::BeginTable("GenerationPreviewLegend", columns,
                           ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        return;
    const std::array<std::pair<const char *, ImU32>, 4> items = {{{"Roads", IM_COL32(169, 129, 76, 255)},
                                                                  {"Rivers", IM_COL32(42, 118, 143, 255)},
                                                                  {"Settlements", IM_COL32(242, 185, 72, 255)},
                                                                  {"Landmarks", IM_COL32(222, 174, 76, 255)}}};
    for (int index = 0; index < static_cast<int>(items.size()); ++index)
    {
        ImGui::TableNextColumn();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float centreY = cursor.y + ImGui::GetTextLineHeight() * 0.5f;
        if (index < 2)
            ImGui::GetWindowDrawList()->AddLine(ImVec2(cursor.x + 1.0f, centreY), ImVec2(cursor.x + 17.0f, centreY),
                                                items[index].second, 3.0f);
        else if (index == 2)
            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cursor.x + 9.0f, centreY), 4.0f, items[index].second,
                                                        12);
        else
            ImGui::GetWindowDrawList()->AddQuadFilled(
                ImVec2(cursor.x + 9.0f, centreY - 4.5f), ImVec2(cursor.x + 13.5f, centreY),
                ImVec2(cursor.x + 9.0f, centreY + 4.5f), ImVec2(cursor.x + 4.5f, centreY), items[index].second);
        ImGui::Dummy(ImVec2(21.0f, ImGui::GetTextLineHeight()));
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextUnformatted(localizedUiLabel(items[index].first).c_str());
    }
    ImGui::EndTable();
}

} // namespace gm::map_creator_view_detail
