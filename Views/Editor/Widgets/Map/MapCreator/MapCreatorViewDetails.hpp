#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorView.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorController.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorArtStylePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPlayerOutputPolicy.hpp"
#include "Models/Localization/LocaleFormatting.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapStrokePresentation.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapTravelCreation.hpp"
#include <stdexcept>
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/WorldInspectorService.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Views/Editor/Widgets/Map/MapFogRendering.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapFactsPresentation.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapImageImportInspectorContract.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapImageTraceAuthoring.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorPreviewContract.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorOutputPreviewRendering.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapAuthoringWorkspaceContract.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapGeneratedFeatureDragInteraction.hpp"
#include "Views/Editor/Widgets/Map/MapTileVisuals.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gm::map_creator_view_detail
{

inline const char *sizePresetItems = "Small\0Standard\0Large\0Custom\0";
inline const char *generatorItems = "City\0Forest\0Mountain Area\0Dungeon\0Cave\0";
inline const char *generatorItemLabels[] = {"City", "Forest", "Mountain Area", "Dungeon", "Cave"};
inline const char *generationLandscapeItems = "River Valley\0Forest Frontier\0Highlands\0Mountain Pass\0Lake Country\0";
inline const char *wayItems = "Trail\0Dirt Road\0Stone Road\0River\0Canal\0Dungeon Corridor\0Cave Tunnel\0";
inline const char *variantModeItems = "Auto\0Continuous\0Individual\0";
inline const char *mountainShapeItems = "Mixed\0Ranges\0Individual Peaks\0";
inline const char *roadNetworkItems = "None\0Sparse\0Connected\0Grid\0";
inline const char *generationQualityItems = "Draft - best of 12\0Balanced - best of 24\0Polished - best of 48\0";
inline const char *generationQualityDescriptions[] = {
    "Fast. Checks twelve deterministic variations and presents up to three valid designs.",
    "Recommended. Checks twenty-four variations for roads, hydrology, access, and composition.",
    "Best for final prep. Checks forty-eight variations and presents only valid polished designs."};
inline const char *rotationItems = "0 degrees\0"
                                   "90 degrees\0"
                                   "180 degrees\0"
                                   "270 degrees\0";
inline const char *tacticalPresetItems =
    "Custom Blocker\0Wall\0Ledge\0Stairs Up\0Window\0Low Cover\0Closed Door\0Open Door\0Ramp Up\0Tree Trunk\0Clear Edge\0";
inline const char *tacticalChannelItems = "Movement\0Line of Sight\0Movement + Sight\0";
inline const char *tacticalDirectionItems = "North Edge\0East Edge\0South Edge\0West Edge\0";
inline const char *tacticalDirectionLabels[] = {"North Edge", "East Edge", "South Edge", "West Edge"};
inline const char *tokenKindItems = "Player\0NPC\0Monster\0Vehicle\0Prop\0Marker\0Party\0";
inline const char *featureKindItems = "Named Location\0Door\0Secret Door\0Trap\0Hazard\0Map Pin\0";
inline const char *lightingItems = "Unspecified\0Bright\0Dim\0Darkness\0";
inline const char *distanceUnitItems = "Not calibrated\0Kilometers\0Miles\0Meters\0Feet\0";
inline const char *nameCultureItems = "Germanic / Central European\0Nordic\0Romance\0Neutral Fantasy\0";
inline const char *factStrengthItems = "Must\0Prefer\0Automatic\0";
inline const char *imageTraceModeItems = "Coastline\0Terrain area\0Road\0River\0Settlement\0Point of interest\0";
inline const char *pngExportAudienceItems = "Player-safe copy\0Game Master copy\0";
inline const char *pngExportSizeItems = "Screen size\0"
                                        "Ultra HD (4K)\0"
                                        "100 px per tactical cell\0"
                                        "Custom\0";
inline const char *nameCultureIds[] = {"germanic_central_european", "nordic", "romance", "neutral_fantasy"};

std::string terrainSelectionSummary(const gm::mapui::TerrainGroupSelection &selection, bool splitScope = false);

void drawTerrainSelectionCells(ImDrawList *canvas, ImVec2 origin, float cellSize,
                               const gm::mapui::TerrainGroupSelection &selection, int dx, int dy, ImU32 edge,
                               ImU32 fill);

void drawWorldPlaceSharingHelp(const MapModel &map);

int nameCultureIndex(const std::string &culture);

MapImageTraceAuthoring::Mode imageTraceMode(int index);

const char *imageTraceDefaultName(MapImageTraceAuthoring::Mode mode);

template <typename... Args> std::string formatLocalizedUi(const char *format, Args... args)
{
    const std::string localizedFormat = localizedUiLabel(format);
    const int required = std::snprintf(nullptr, 0, localizedFormat.c_str(), args...);
    if (required <= 0)
        return localizedFormat;

    std::vector<char> buffer(static_cast<std::size_t>(required) + 1);
    std::snprintf(buffer.data(), buffer.size(), localizedFormat.c_str(), args...);
    return std::string(buffer.data(), static_cast<std::size_t>(required));
}

std::string localizedModelPickerLabel(const char *label);

std::string formatMapExportMemory(std::uint64_t bytes);

std::string formatMapPackageByteSize(std::uint64_t bytes);

std::string mapPackageVersionLabel(int version);

const char *mapPackageVisibilityLabel(const MapPackagePreview &preview);

const char *mapPackageAtlasStatusLabel(MapAtlasStatus status);

const char *mapPackageAssetKindLabel(MapPackageAssetKind kind);

struct UiButtonPairResult
{
    bool first = false;
    bool second = false;
};

UiButtonPairResult drawResponsiveButtonPair(const char *id, const char *firstLabel, UiButtonRole firstRole,
                                            const char *firstTooltip, const char *secondLabel, UiButtonRole secondRole,
                                            const char *secondTooltip, bool secondEnabled = true,
                                            const char *secondDisabledReason = nullptr, bool firstEnabled = true,
                                            const char *firstDisabledReason = nullptr);

bool drawChoiceButton(const char *label, bool selected, bool enabled = true, const char *disabledReason = nullptr,
                      ImVec2 size = ImVec2(-1.0f, 0.0f));

bool drawMapConfirmationModal(const char *popupId, const char *message, const char *confirmLabel = "Delete");

ImU32 withAlpha(ImU32 color, int alpha);

bool beginSidePanel(const char *label, bool forceOpen);

bool mapContainsVisibleDraftContent(const MapModel &map);

void drawPendingMapPreview(bool readyToGenerate);

void drawDisabledWrappedText(const char *text);

void drawDisabledWrappedValue(const std::string &text);

void drawCandidateArtPreviewToggle(bool &enabled);

std::string localizedGeneratedFeatureName(const MapGeneratedFeature &feature);

bool shouldDrawOverviewFeatureLabel(const MapModel &map, const MapGeneratedFeature &feature);

const char *sizePresetLabel(MapSizePreset preset);

const char *sizePresetDescription(MapSizePreset preset);

const char *generationIssueMessage(const MapGenerationIssue &issue);

const char *generationIssueAction(const MapGenerationIssue &issue);

float mapCreatorMinimumZoom(MapLevel level);

struct GenerationPreviewSummary
{
    int roads = 0;
    int rivers = 0;
    int settlements = 0;
    int landmarks = 0;
};

GenerationPreviewSummary generationPreviewSummary(const MapModel &map);

std::pair<const char *, int> generationPreviewStrength(const MapGenerationQualityReport &quality);

std::vector<const MapGenerationIssue *> generationPreviewReviewIssues(const MapGenerationResult &candidate);

std::string generationCompositionLabel(const MapModel &map);

void drawGenerationPreviewLegend();

bool drawGenerationMiniMap(const MapModel &map, MapRenderPlanCache &renderPlanCache, std::uint64_t contentVersion,
                           const char *id, float height = 128.0f, float requestedWidth = -1.0f, bool showLabels = true,
                           bool showPoliticalBorders = false, const MapRenderPlanRequest *presentationRequest = nullptr,
                           gm::mapui::MapCreatorPlayerPreviewCache *playerCache = nullptr,
                           const MapPngExportRect *outputView = nullptr);

bool drawGenerationMiniMapFitted(const MapModel &map, MapRenderPlanCache &renderPlanCache, std::uint64_t contentVersion,
                                 const char *id, float maximumHeight,
                                 const MapRenderPlanRequest *presentationRequest = nullptr,
                                 gm::mapui::MapCreatorPlayerPreviewCache *playerCache = nullptr,
                                 const MapPngExportRect *outputView = nullptr);

float drawGenerationInteractivePreview(const MapModel &map, MapRenderPlanCache &renderPlanCache,
                                       std::uint64_t contentVersion, const char *id, float viewportHeight, float &zoom,
                                       bool showLabels, bool showPoliticalBorders, bool resetScroll,
                                       const MapRenderPlanRequest *presentationRequest = nullptr,
                                       gm::mapui::MapCreatorPlayerPreviewCache *playerCache = nullptr);

std::string localizedGeneratedFeatureName(const MapGeneratedFeature &feature);

const char *generatedFeatureKindLabel(MapGeneratedFeatureKind kind);

}
