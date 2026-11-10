#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace map_creator_controller
{
enum class GeneratorPreset
{
    City,
    Forest,
    MountainArea,
    Dungeon,
    Cave
};

enum class SelectedObjectKind
{
    None,
    Token,
    Feature,
    GeneratedFeature,
    Region,
    Route
};

struct UndoSnapshot
{
    MapModel map;
    std::string label;
    MapCreatorPlayerSettingsBaseline playerSettingsBaseline;

    struct WorldPlaceTransition
    {
        WorldPlaceModel place;
        bool shouldExist = false;
    };

    std::optional<WorldPlaceTransition> worldPlaceTransition;
    std::uint64_t historyState = 0;
};

struct GenerationProgressSnapshot
{
    mutable std::mutex mutex;
    int step = 0;
    int totalSteps = 0;
    std::string stage;
};

struct DraftGenerationResult
{
    std::uint64_t requestSerial = 0;
    MapGenerationBatchResult batch;
    int contextualTargetCount = 0;
    int contextualAcceptedCount = 0;
    int contextualCandidateCount = 0;
    long long contextualCoverageElapsedMs = 0;
};

struct PngExportVisibilitySettings
{
    bool showGrid = false;
    bool showLabels = true;
    bool showPoliticalOverlay = false;
    bool showAnnotations = true;
    bool showRoutes = true;
    bool showTokens = true;
    bool showBackground = true;
    bool applyFog = false;
};
} // namespace map_creator_controller
