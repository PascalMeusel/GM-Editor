#pragma once

#include "Models/Map/MapTileTypes.hpp"

enum class MapGenerationPurpose
{
    Region,
    TravelArea,
    SettlementEnvirons,
    Wilderness
};

enum class MapLandscapeProfile
{
    RiverValley,
    ForestFrontier,
    Highlands,
    MountainPass,
    LakeCountry
};

// Independent of biome: explicit shape choices for new generation recipes.
// Automatic preserves the interpretation of historical saved maps.
// Append choices: persisted legacy ordinal values and tokens remain stable.
enum class MapLandform { Automatic, Islands, Continental, InlandLake, SingleIsland, Coast, Peninsula };

inline const char *mapLandformToken(MapLandform value)
{
    switch (value)
    {
    case MapLandform::Islands: return "islands";
    case MapLandform::Continental: return "continental";
    case MapLandform::InlandLake: return "inland_lake";
    case MapLandform::SingleIsland: return "single_island";
    case MapLandform::Coast: return "coast";
    case MapLandform::Peninsula: return "peninsula";
    default: return "automatic";
    }
}

inline MapLandform mapLandformFromToken(const std::string &value)
{
    if (value == "islands") return MapLandform::Islands;
    if (value == "continental") return MapLandform::Continental;
    if (value == "inland_lake") return MapLandform::InlandLake;
    if (value == "single_island") return MapLandform::SingleIsland;
    if (value == "coast") return MapLandform::Coast;
    if (value == "peninsula") return MapLandform::Peninsula;
    return MapLandform::Automatic;
}

enum class MapGeneratedFeatureKind
{
    TerrainRegion,
    River,
    Road,
    Trail,
    Settlement,
    Landmark,
    Bridge,
    Vegetation,
    MountainRange,
    Coastline,
    WaterBody,
    Nation,
    District,
    BuildingGroup,
    TacticalSetpiece
};

enum class MapGenerationQualityLevel
{
    Draft,
    Balanced,
    Polished
};

enum class MapGenerationFactStrength
{
    Must,
    Prefer,
    Automatic
};

enum class MapGenerationFactType
{
    Note,
    Nation,
    NationCount,
    Region,
    Settlement,
    Capital,
    WaterBody,
    River,
    SharedRiver,
    Coast,
    Island,
    MountainRange,
    Forest,
    Road,
    RelativePosition,
    IncludedElement,
    ExcludedElement
};

struct MapGenerationFact
{
    std::string id;
    MapGenerationFactType type = MapGenerationFactType::Note;
    MapGenerationFactStrength strength = MapGenerationFactStrength::Automatic;
    std::string label;
    std::vector<std::string> entityRefs;
    std::string value;
    std::string sourceText;
    float confidence = 1.0f;
    bool confirmed = false;
};

struct MapFeaturePoint
{
    float x = 0.0f;
    float y = 0.0f;
};

struct MapFeatureLineage
{
    EntityId sourceMapId;
    std::string sourceFeatureId;
    std::uint64_t sourceRevision = 0;
};

struct MapGenerationConstraint
{
    std::string id;
    std::string label;
    MapGeneratedFeatureKind featureKind = MapGeneratedFeatureKind::Landmark;
    float x = 0.0f;
    float y = 0.0f;
    bool required = true;
    bool locked = false;
};

struct MapDimensions
{
    int width = 72;
    int height = 48;
};

// Canonical V5 creation contract. UI controls, background generation jobs,
// QA tools and imports should exchange this type instead of independently
// combining legacy scope/kind/purpose values.
struct MapCreationRequest
{
    MapLandform landform = MapLandform::Automatic;
    int waterLevel = 20, mountainDensity = 18, forestDensity = 25;
    int settlementDensity = 8, routeDensity = 28, tacticalDensity = 18;
    MapCreationIntent intent = MapCreationIntent::WorldOverview;
    MapSizePreset sizePreset = MapSizePreset::Standard;
    MapDimensions dimensions;
    MapSpatialReference spatialReference;
    std::uint32_t seed = 240529;
    int generatorVersion = 5;
    int recipeVersion = 3;
    std::string styleId = "fantasy_neutral";
    int styleVersion = 3;
    std::string styleSnapshotId = "fantasy_neutral_v3_3_ec659c205685c2d2";
    MapGenerationQualityLevel quality = MapGenerationQualityLevel::Balanced;
    std::string contentLanguage = "de";
    std::string nameCulture = "neutral_fantasy";
    bool allowAdditionalNations = false;
    EntityId parentMapId;
    MapNormalizedRect parentSelection;
    std::vector<MapGenerationFact> facts;
    std::vector<MapGenerationConstraint> constraints;
};

struct MapGenerationRecipe
{
    int generatorVersion = 5;
    int recipeVersion = 3;
    std::string recipeId = "river_valley";
    std::string styleId = "fantasy_neutral";
    int styleVersion = 3;
    std::string styleSnapshotId = "fantasy_neutral_v3_3_ec659c205685c2d2";
    MapLevel scale = MapLevel::Region;
    MapGenerationPurpose purpose = MapGenerationPurpose::Region;
    MapLandscapeProfile landscape = MapLandscapeProfile::RiverValley;
    MapLandform landform = MapLandform::Automatic;
    // -1 means an older recipe did not record the requested density.
    int waterLevel = -1, mountainDensity = -1, forestDensity = -1;
    int settlementDensity = -1, routeDensity = -1, tacticalDensity = -1;
    // Absent (-1) on historical maps: opening them never opts into regeneration.
    int travelScenePurpose = -1;
    std::string travelSceneBiome;
    int travelSceneDangerLevel = 1;
    bool travelSceneCombatReady = false;
    std::uint32_t travelSceneSeed = 240529;

    MapGenerationQualityLevel quality = MapGenerationQualityLevel::Balanced;
    MapCreationIntent creationIntent = MapCreationIntent::RegionExploration;
    MapSizePreset sizePreset = MapSizePreset::Standard;
    std::string contentLanguage = "de";
    std::string nameCulture = "neutral_fantasy";
    bool allowAdditionalNations = false;
    std::uint32_t seed = 240529;
    int settlementCount = 3;
    int landmarkCount = 4;
    int edgeConnectionCount = 2;
    std::vector<std::string> knownFacts;
    std::vector<std::string> requiredElements;
    std::vector<std::string> optionalElements;
    std::vector<std::string> excludedElements;
    std::vector<std::string> lockedFeatureIds;
    std::vector<MapGenerationConstraint> constraints;
    std::vector<MapGenerationFact> facts;
};

struct MapGeneratedFeature
{
    std::string id;
    MapGeneratedFeatureKind kind = MapGeneratedFeatureKind::Landmark;
    std::string name;
    std::string materialId;
    float width = 1.0f;
    bool closed = false;
    bool locked = false;
    // Generated geography can participate in the same player-safety contract
    // as hand-authored map features. Older saves omit these fields and remain
    // public/discovered by default for compatibility.
    bool discovered = true;
    bool gmOnly = false;
    std::vector<MapFeaturePoint> points;
    MapFeatureLineage lineage;
};

struct MapFeatureGraph
{
    int schemaVersion = 2;
    std::vector<MapGeneratedFeature> features;
};

struct MapGenerationMetadata
{
    bool enabled = false;
    bool continuousRendering = false;
    MapGenerationRecipe recipe;
    MapFeatureGraph graph;
};

