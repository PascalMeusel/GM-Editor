#pragma once

#include "Models/Map/MapGenerationModels.hpp"

enum class MapRenderAssetRole
{
    TerrainMaterial,
    BlendMask,
    TransitionDecal,
    RoadProfile,
    RiverProfile,
    BridgeStamp,
    VegetationCluster,
    MountainCluster,
    SettlementStamp,
    TacticalProp,
    DungeonStamp,
    MapSymbol,
    GroundDetail
};

struct MapRenderAssetEntry
{
    std::string id;
    MapRenderAssetRole role = MapRenderAssetRole::TerrainMaterial;
    std::string path;
    std::string provenance;
    std::string tool;
    std::string license;
    std::vector<MapLevel> supportedScales;
    int footprintWidth = 1;
    int footprintHeight = 1;
    float minimumZoom = 0.35f;
    float maximumZoom = 4.0f;
    float minimumDisplayCells = 1.0f;
    float maximumDisplayCells = 4.0f;
    float placementDensity = 1.0f;
    float minimumClusterDistance = 0.0f;
    std::string semanticRole;
    std::string renderingClass;
    std::vector<std::string> compatibleNeighborFamilies;
    int variant = 0;
    bool accepted = false;
};

// Rendering is deliberately scale-specific even though every atlas level uses
// the same art direction. A world map is an atlas, not a battle map viewed from
// farther away, and therefore has different density, labelling and grid rules.
struct MapRenderScaleProfile
{
    MapLevel level = MapLevel::Region;
    float materialCellsPerRepeat = 6.0f;
    float symbolDensity = 0.12f;
    float vegetationClusterDistance = 4.0f;
    float mountainClusterDistance = 5.0f;
    float roadWidthCells = 0.45f;
    float riverWidthCells = 0.85f;
    float settlementStampCells = 1.7f;
    float buildingStampCells = 1.4f;
    bool showEntityLabels = true;
    bool showGridByDefault = false;
    bool allowTacticalProps = false;
    bool allowIndividualBuildings = false;
};

struct MapRenderStyleManifest
{
    int schemaVersion = 2;
    std::string styleId = "fantasy_neutral";
    std::string displayName = "Fantasy Neutral";
    std::string perspective = "orthographic_top_down";
    std::string lighting = "neutral_north_west";
    int materialSourcePixels = 512;
    int tacticalPixelsPerCell = 256;
    std::string productionSnapshotId;
    std::vector<MapRenderScaleProfile> scaleProfiles;
    std::vector<MapRenderAssetEntry> assets;
};

// Render caches are derived runtime data. They intentionally contain no GPU
// handles so campaign serialization can never accidentally persist them.
struct MapRenderChunkState
{
    int chunkX = 0;
    int chunkY = 0;
    std::uint64_t sourceRevision = 0;
    bool dirty = true;
};

struct MapRenderCache
{
    EntityId mapId;
    int chunkSize = 16;
    int lod = 0;
    std::uint64_t sourceRevision = 0;
    std::vector<MapRenderChunkState> chunks;
};

struct MapModel
{
    EntityMetadata metadata{"map"};
    std::string name;
    std::string description;
    MapScope scope = MapScope::World;
    MapLevel atlasLevel = MapLevel::World;
    MapKind mapKind = MapKind::General;
    MapAtlasStatus atlasStatus = MapAtlasStatus::Standalone;
    MapVisibility visibility = MapVisibility::GameMasterOnly;
    MapPlayerReleaseProfile playerRelease;
    MapCreationIntent creationIntent = MapCreationIntent::WorldOverview;
    MapSizePreset sizePreset = MapSizePreset::Standard;
    std::uint64_t revision = 0;
    MapAtlasPlacement atlasPlacement;
    MapSpatialReference spatialReference;
    GridType gridType = GridType::Square;
    int width = 24;
    int height = 18;
    float cellSize = 1.0f;
    bool isMainMap = false;
    EntityId parentMapId;
    std::string backgroundImagePath;
    float backgroundImageOpacity = 0.65f;
    MapImportedReference importedReference;
    std::vector<MapCellModel> cells;
    std::vector<MapLayerModel> layers;
    std::vector<MapTokenModel> tokens;
    std::vector<MapRegionModel> regions;
    std::vector<MapFeatureModel> features;
    std::vector<MapRouteModel> routes;
    MapGenerationMetadata generation;
};

struct PartyMapLocationModel
{
    EntityId mapId;
    int x = 0;
    int y = 0;
};

struct PartyTravelPlanModel
{
    EntityMetadata metadata{"party_travel"};
    EntityId currentMapId;
    EntityId routeId;
    std::string routeName;
    std::string travelPace = "Normal";
    float distanceMiles = 0.0f;
    float milesTraveled = 0.0f;
    int watches = 0;
    int rationsUsed = 0;
    int randomEncounterChecks = 0;
    std::string notes;
};

