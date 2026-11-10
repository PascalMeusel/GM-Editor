#pragma once

#include "Models/Map/MapModels.hpp"

#include <cstdint>
#include <string>
#include <vector>

enum class AssetQaSeverity
{
    Info,
    Warning,
    Blocker
};

enum class AssetReviewVerdict
{
    Pending,
    Accept,
    Repaint,
    Replace
};

struct AssetQaFinding
{
    std::string code;
    AssetQaSeverity severity = AssetQaSeverity::Info;
    std::string assetId;
    std::string mapId;
    std::uint32_t seed = 0;
    std::string message;
    std::string evidencePath;
};

struct AssetUsageEvidence
{
    std::string assetId;
    std::string mapId;
    std::string mapGroup;
    std::uint32_t seed = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int rotationQuarterTurns = 0;
    std::string imagePath;
};

struct AssetReviewDecision
{
    std::string assetId;
    AssetReviewVerdict verdict = AssetReviewVerdict::Pending;
    int styleFit = 0;
    int perspective = 0;
    int paletteAndLighting = 0;
    int scaleAndReadability = 0;
    int contextualFit = 0;
    std::string reviewer;
    std::string reason;
    std::string reviewedAtUtc;
};

struct AssetReplacementWorkOrder
{
    std::string id;
    std::string assetId;
    std::string family;
    std::vector<std::string> defectCodes;
    std::vector<std::string> evidencePaths;
    std::vector<std::string> referenceAssetIds;
    std::string prompt;
    int requiredWidth = 0;
    int requiredHeight = 0;
    bool requiresTransparency = false;
};

struct AssetQaRunManifest
{
    int schemaVersion = 1;
    std::string runId;
    std::string styleId;
    int styleVersion = 0;
    std::string manifestSha256;
    std::string artDirectionId;
    std::uint32_t seed = 0;
    int requestedMapCount = 0;
    int generatedMapCount = 0;
    std::vector<AssetQaFinding> findings;
    std::vector<AssetUsageEvidence> usage;
};

struct ProductionStyleSnapshot
{
    int schemaVersion = 1;
    std::string snapshotId;
    std::string styleId;
    int styleVersion = 0;
    std::string sourceManifestSha256;
    std::string artDirectionId;
    std::string createdAtUtc;
    std::vector<std::string> acceptedAssetIds;
};

enum class GenerationQaGroup
{
    World,
    RegionTravel,
    CitySettlement,
    TacticalOutdoor,
    DungeonCave
};

struct GenerationQaScenario
{
    int schemaVersion = 1;
    std::string id;
    GenerationQaGroup group = GenerationQaGroup::World;
    std::string band;
    std::uint32_t seed = 0;
    MapGenerationRecipe recipe;
};

struct GenerationInvariantResult
{
    std::string code;
    bool passed = true;
    bool critical = false;
    std::string details;
};

struct GeneratorRegressionCase
{
    int schemaVersion = 1;
    std::string signature;
    std::string scenarioId;
    GenerationQaGroup group = GenerationQaGroup::World;
    std::uint32_t seed = 0;
    std::string recipePath;
    std::string firstSeenAtUtc;
    std::vector<std::string> failedInvariantCodes;
};

