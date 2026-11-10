#include "Models/Map/MapPackageService.hpp"
#include "Models/Map/MapPackageDraftInstallService.hpp"
#include "Models/Map/MapFileIoPath.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace
{
bool require(bool condition, const std::string &message)
{
    if (condition) return true;
    std::cerr << message << std::endl;
    return false;
}

bool near(float left, float right) { return std::abs(left - right) < 0.0001f; }

void writeBytes(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::string contentHash(const std::vector<std::uint8_t> &bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::uint64_t hash = 14695981039346656037ull;
    for (std::uint8_t byte : bytes)
    {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    std::string value = "fnv1a64:";
    for (int shift = 60; shift >= 0; shift -= 4)
        value.push_back(digits[(hash >> shift) & 0x0f]);
    return value;
}

std::uint64_t fnv1a(const std::uint8_t *bytes, std::size_t size)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

void writeLittleEndian(std::vector<std::uint8_t> &bytes, std::size_t offset,
                       std::uint64_t value, std::size_t width)
{
    for (std::size_t index = 0; index < width; ++index)
        bytes[offset + index] = static_cast<std::uint8_t>((value >> (index * 8u)) & 0xffu);
}

class ScopedEnvironment
{
public:
    ScopedEnvironment(const char *name, const std::string &value) : name_(name)
    {
        if (const char *existing = std::getenv(name)) previous_ = existing;
#if defined(_WIN32)
        _putenv_s(name, value.c_str());
#else
        setenv(name, value.c_str(), 1);
#endif
    }

    ~ScopedEnvironment()
    {
#if defined(_WIN32)
        _putenv_s(name_.c_str(), previous_ ? previous_->c_str() : "");
#else
        if (previous_) setenv(name_.c_str(), previous_->c_str(), 1);
        else unsetenv(name_.c_str());
#endif
    }

private:
    std::string name_;
    std::optional<std::string> previous_;
};

MapModel makeRichMap()
{
    MapModel map;
    map.metadata.id = "map-original";
    map.metadata.createdAtUtc = "2026-01-02T03:04:05Z";
    map.metadata.updatedAtUtc = "2026-02-03T04:05:06Z";
    map.metadata.tags = {"portable", "gm"};
    map.metadata.customProperties = {{"weather", "storm"}};
    map.name = "The Portable Vale";
    map.description = "Complete editable map";
    map.scope = MapScope::Dungeon;
    map.atlasLevel = MapLevel::Site;
    map.mapKind = MapKind::Dungeon;
    map.atlasStatus = MapAtlasStatus::Stale;
    map.visibility = MapVisibility::PlayersKnown;
    map.playerRelease = {true, false, true, false, true};
    map.creationIntent = MapCreationIntent::Cave;
    map.sizePreset = MapSizePreset::Custom;
    map.revision = 9876543210ull;
    map.parentMapId = "external-parent";
    map.atlasPlacement = {"external-parent", {0.1f, 0.2f, 0.3f, 0.4f}, 52, 53};
    map.spatialReference = {true, 18.5f, 22.25f, MapDistanceUnit::Miles, -2.0f, 4.0f, 17.5f};
    map.gridType = GridType::Hex;
    map.width = 2;
    map.height = 1;
    map.cellSize = 5.5f;
    map.isMainMap = true;
    map.backgroundImagePath = "old/background.png";
    map.backgroundImageOpacity = 0.42f;
    map.importedReference.enabled = true;
    map.importedReference.assetId = "asset-imported";
    map.importedReference.projectRelativePath = "old/imported.webp";
    map.importedReference.sourceFileName = "imported.webp";
    map.importedReference.contentHash = "old-hash";
    map.importedReference.sourceWidth = 640;
    map.importedReference.sourceHeight = 480;
    map.importedReference.workingProxyAssetId = "asset-working-proxy";
    map.importedReference.workingProxyProjectRelativePath = "old/imported-working.png";
    map.importedReference.workingProxyContentHash = "old-proxy-hash";
    map.importedReference.workingProxyWidth = 320;
    map.importedReference.workingProxyHeight = 240;
    map.importedReference.exifOrientation = 6;
    map.importedReference.workingProxyGenerated = true;
    map.importedReference.crop = {0.11f, 0.12f, 0.75f, 0.76f};
    map.importedReference.rotationDegrees = 91.0f;
    map.importedReference.gmOnly = false;
    map.importedReference.semanticallyTraced = true;

    MapCellModel first;
    first.kind = MapTileKind::Lava; first.rotationQuarterTurns = 3; first.rotationLocked = true;
    first.symbolVariantMode = MapSymbolVariantMode::Individual; first.wayKind = MapWayKind::CaveTunnel;
    first.movementCost = 7; first.difficultTerrain = true; first.blocksLineOfSight = true;
    first.lighting = MapLightingMode::Darkness; first.fogged = true; first.elevation = -4;
    first.movementBlockedMask = 3; first.lineOfSightBlockedMask = 5;
    first.movementOneWayBlockedMask = 7; first.lineOfSightOneWayBlockedMask = 9;
    MapCellModel second; second.kind = MapTileKind::Bridge; second.wayKind = MapWayKind::StoneRoad;
    map.cells = {first, second};

    MapLayerModel layer;
    layer.metadata.id = "layer-original"; layer.metadata.tags = {"terrain"}; layer.name = "Secrets";
    layer.visible = false; layer.locked = true; layer.sortOrder = -2;
    map.layers.push_back(layer);

    MapTokenModel token;
    token.metadata.id = "token-original"; token.name = "Scout"; token.kind = MapTokenKind::Party;
    token.linkedEntityType = "map_feature"; token.linkedEntityId = "feature-original";
    token.assetId = "asset-imported"; token.layerId = "layer-original"; token.x = 1.25f; token.y = 0.75f;
    token.rotation = 33.0f; token.size = 2.5f; token.hiddenFromPlayers = true;
    map.tokens.push_back(token);

    MapRegionModel region;
    region.metadata.id = "region-original"; region.name = "Black vault"; region.description = "Cold";
    region.x = -1; region.y = 2; region.width = 8; region.height = 9; region.movementCost = 4;
    region.lighting = MapLightingMode::Dim; region.difficultTerrain = true;
    region.blocksLineOfSight = true; region.gmOnly = true;
    map.regions.push_back(region);

    MapFeatureModel feature;
    feature.metadata.id = "feature-original"; feature.kind = MapFeatureKind::SecretDoor;
    feature.name = "Whisper door"; feature.description = "Stone"; feature.gmNotes = "DC clue";
    feature.linkedEntityId = "token-original"; feature.targetMapId = "map-original";
    feature.x = 13; feature.y = 14; feature.discovered = true; feature.gmOnly = true; feature.difficultyClass = 19;
    map.features.push_back(feature);

    MapRouteModel route;
    route.metadata.id = "route-original"; route.name = "Loop"; route.fromMapId = "map-original";
    route.toMapId = "map-original"; route.startX = 1; route.startY = 2; route.endX = 3; route.endY = 4;
    route.distanceMiles = 6.75f; route.terrain = "cave"; route.notes = "slippery"; route.discovered = false;
    map.routes.push_back(route);

    map.generation.enabled = true;
    map.generation.continuousRendering = true;
    auto &recipe = map.generation.recipe;
    recipe.generatorVersion = 73; recipe.recipeVersion = 19;
    recipe.recipeId = "portable-recipe-v9"; recipe.styleId = "ink";
    recipe.styleVersion = 41; recipe.styleSnapshotId = "snapshot-immutable-123";
    recipe.scale = MapLevel::Tactical; recipe.purpose = MapGenerationPurpose::SettlementEnvirons;
    recipe.landscape = MapLandscapeProfile::MountainPass; recipe.quality = MapGenerationQualityLevel::Polished;
    recipe.landform = MapLandform::InlandLake;
    recipe.travelScenePurpose = 4; recipe.travelSceneBiome = "Grassland";
    recipe.travelSceneDangerLevel = 2; recipe.travelSceneCombatReady = true; recipe.travelSceneSeed = 998877u;
    recipe.waterLevel = 71; recipe.forestDensity = 0; recipe.mountainDensity = 44;
    recipe.routeDensity = 19; recipe.settlementDensity = 62; recipe.tacticalDensity = 13;
    recipe.creationIntent = MapCreationIntent::ImportedReference; recipe.sizePreset = MapSizePreset::Large;
    recipe.contentLanguage = "de"; recipe.nameCulture = "alpine"; recipe.allowAdditionalNations = true;
    recipe.seed = 0xfedcba98u; recipe.settlementCount = 8; recipe.landmarkCount = 9; recipe.edgeConnectionCount = 10;
    recipe.knownFacts = {"known-a", "known-b"}; recipe.requiredElements = {"bridge"};
    recipe.optionalElements = {"ruin"}; recipe.excludedElements = {"ocean"};
    recipe.lockedFeatureIds = {"generated-original"};
    recipe.constraints.push_back({"constraint-one", "Keep", MapGeneratedFeatureKind::WaterBody,
                                  2.25f, 3.5f, false, true});
    MapGenerationFact fact;
    fact.id = "fact-one"; fact.type = MapGenerationFactType::SharedRiver;
    fact.strength = MapGenerationFactStrength::Must; fact.label = "River";
    fact.entityRefs = {"generated-original", "feature-original"}; fact.value = "yes";
    fact.sourceText = "source"; fact.confidence = 0.625f; fact.confirmed = true;
    recipe.facts.push_back(fact);

    map.generation.graph.schemaVersion = 29;
    MapGeneratedFeature generated;
    generated.id = "generated-original"; generated.kind = MapGeneratedFeatureKind::Coastline;
    generated.name = "Cliff"; generated.materialId = "basalt"; generated.width = 3.75f;
    generated.closed = true; generated.locked = true; generated.discovered = false; generated.gmOnly = true;
    generated.points = {{1.5f, 2.5f}, {8.25f, 9.75f}};
    generated.lineage = {"map-original", "generated-original", 144};
    map.generation.graph.features.push_back(generated);
    return map;
}

bool testRoundTripAndPreview(const std::filesystem::path &directory)
{
    const auto background = directory / "bg.dat";
    const auto imported = directory / "reference.dat";
    const auto proxy = directory / "reference-working.png";
    writeBytes(background, {0, 1, 2, 3, 255});
    writeBytes(imported, {9, 8, 7, 6, 5, 4});
    writeBytes(proxy, {4, 3, 2, 1});
    const auto package = directory / "roundtrip.ttrpgmappkg";
    MapModel source = makeRichMap();
    MapPackageExportOptions exportOptions;
    exportOptions.backgroundAssetPath = background;
    exportOptions.importedAssetPath = imported;
    exportOptions.workingProxyAssetPath = proxy;
    source.importedReference.contentHash = contentHash(readBytes(imported));
    source.importedReference.workingProxyContentHash = contentHash(readBytes(proxy));
    const MapPackageExportResult exported = MapPackageService::exportMap(source, package, exportOptions);
    bool ok = require(exported.success && exported.packageBytes == std::filesystem::file_size(package),
                      "A complete package should export as one file");

    const MapPackagePreview preview = MapPackageService::previewImport(package);
    ok &= require(preview.valid && preview.importEligible &&
                      preview.sourceMapId == "map-original" && preview.mapName == source.name,
                  "Preview should expose map identity without applying import");
    ok &= require(preview.cellCount == 2 && preview.generatedFeatureCount == 1 && preview.manualFeatureCount == 1 &&
                      preview.regionCount == 1 && preview.routeCount == 1 && preview.tokenCount == 1 &&
                      preview.layerCount == 1 && preview.assets.size() == 3 &&
                      preview.embeddedAssetCount == 3 && preview.embeddedAssetBytes == 15 &&
                      preview.packageBytes == std::filesystem::file_size(package) &&
                      preview.embeddedAssetIntegrityVerified && preview.embeddedAssetConflictCount == 0,
                  "Preview should expose package contents and embedded asset sizes");
    ok &= require(preview.generatorVersion == 73 && preview.recipeVersion == 19 && preview.recipeStyleVersion == 41 &&
                      preview.graphSchemaVersion == 29 && preview.recipeId == "portable-recipe-v9" &&
                      preview.styleId == "ink" && preview.styleSnapshotId == "snapshot-immutable-123",
                  "Preview should preserve generator, recipe, graph and style snapshot versions");
    ok &= require(!preview.gameMasterOnly && preview.knownToPlayers && !preview.visibleToPlayers &&
                      preview.enabledPlayerReleaseCategoryCount == 3,
                  "Preview should distinguish GM-only, player-known and player-visible map states");
    ok &= require(preview.playerRelease.politicalBorders &&
                      !preview.playerRelease.waysAndRoutes &&
                      preview.playerRelease.placesAndMarkers &&
                      !preview.playerRelease.labels &&
                      preview.playerRelease.tokensAndAnnotations,
                  "Preview should expose the package's player release categories");
    ok &= require(preview.sourceAtlasStatus == MapAtlasStatus::Stale &&
                      preview.sourceHasAtlasParent && !preview.sourceAtlasParentConflict &&
                      preview.independentCopyDetachesAtlasParent &&
                      preview.independentCopyNormalizesAtlasStatus &&
                      preview.sourceFeatureLineageCount == 1 &&
                      preview.externalFeatureLineageCount == 0 &&
                      preview.independentCopyDetachedLineageCount == 0 &&
                      preview.independentCopyRetainedInternalLineageCount == 1,
                  "Preview should explain source atlas state and independent-copy lineage impact");
    ok &= require(preview.ownedIdFieldCount == 11 && preview.uniqueOwnedIdCount == 11 &&
                      preview.duplicateOwnedIdCount == 0 &&
                      preview.independentCopyIdRemapCount == 11 &&
                      preview.independentCopyAssetIdRemapCount == 2,
                  "Preview should summarize independent-copy identity remapping without exposing new IDs");

    MapPackageImportOptions preserve;
    preserve.standaloneCopy = false;
    preserve.remapEntityIds = false;
    const MapPackageImportResult importedResult = MapPackageService::importMap(package, preserve);
    ok &= require(importedResult.success && importedResult.assets.size() == 3,
                  "Package should import background, immutable original and working proxy");
    if (!importedResult.success) return false;
    const MapModel &actual = importedResult.map;
    ok &= require(actual.metadata.id == source.metadata.id && actual.metadata.customProperties == source.metadata.customProperties &&
                      actual.name == source.name && actual.description == source.description && actual.scope == source.scope &&
                      actual.creationIntent == source.creationIntent && actual.atlasLevel == source.atlasLevel &&
                      actual.mapKind == source.mapKind && actual.visibility == source.visibility && actual.revision == source.revision,
                  "Core metadata and map intent should round-trip");
    ok &= require(actual.playerRelease.politicalBorders &&
                      !actual.playerRelease.waysAndRoutes &&
                      actual.playerRelease.placesAndMarkers &&
                      !actual.playerRelease.labels &&
                      actual.playerRelease.tokensAndAnnotations,
                  "Per-map player release categories should round-trip");
    ok &= require(actual.parentMapId == "external-parent" && actual.atlasPlacement.parentId == "external-parent" &&
                      actual.atlasPlacement.parentRevision == 52 && near(actual.atlasPlacement.parentRect.height, 0.4f) &&
                      actual.spatialReference.calibrated && actual.spatialReference.unit == MapDistanceUnit::Miles &&
                      near(actual.spatialReference.rotationDegrees, 17.5f),
                  "Atlas and spatial data should round-trip when standalone import is disabled");
    ok &= require(actual.cells.size() == 2 && actual.cells[0].fogged && actual.cells[0].kind == MapTileKind::Lava &&
                      actual.cells[0].movementOneWayBlockedMask == 7 && actual.cells[1].kind == MapTileKind::Bridge,
                  "Cells, directional blockers, and fog should round-trip");
    ok &= require(actual.layers[0].locked && actual.tokens[0].layerId == "layer-original" &&
                      actual.regions[0].lighting == MapLightingMode::Dim && actual.features[0].gmNotes == "DC clue" &&
                      actual.routes[0].terrain == "cave",
                  "Layers, tokens, regions, manual features and routes should round-trip");
    ok &= require(actual.generation.enabled && actual.generation.continuousRendering &&
                      actual.generation.recipe.generatorVersion == 73 && actual.generation.recipe.recipeVersion == 19 &&
                      actual.generation.recipe.landform == MapLandform::InlandLake &&
                      actual.generation.recipe.waterLevel == 71 && actual.generation.recipe.forestDensity == 0 &&
                      actual.generation.recipe.mountainDensity == 44 && actual.generation.recipe.routeDensity == 19 &&
                      actual.generation.recipe.settlementDensity == 62 && actual.generation.recipe.tacticalDensity == 13 &&
                      actual.generation.recipe.travelScenePurpose == 4 && actual.generation.recipe.travelSceneBiome == "Grassland" &&
                      actual.generation.recipe.travelSceneDangerLevel == 2 && actual.generation.recipe.travelSceneCombatReady &&
                      actual.generation.recipe.travelSceneSeed == 998877u &&
                      actual.generation.recipe.styleVersion == 41 &&
                      actual.generation.recipe.styleSnapshotId == "snapshot-immutable-123" &&
                      actual.generation.recipe.seed == 0xfedcba98u && actual.generation.recipe.constraints.size() == 1 &&
                      actual.generation.recipe.facts.size() == 1 && near(actual.generation.recipe.facts[0].confidence, 0.625f) &&
                      actual.generation.graph.features[0].points.size() == 2 &&
                      actual.generation.graph.features[0].lineage.sourceRevision == 144,
                  "Generation metadata, recipe, facts, constraints and generated graph should round-trip");
    ok &= require(actual.backgroundImagePath == "assets/background/bg.dat" &&
                      actual.importedReference.projectRelativePath == "assets/imported/reference.dat" &&
                      actual.importedReference.contentHash == importedResult.assets[1].contentHash &&
                      actual.importedReference.workingProxyProjectRelativePath ==
                          "assets/working-proxy/reference-working.png" &&
                      actual.importedReference.workingProxyContentHash == importedResult.assets[2].contentHash &&
                      actual.importedReference.workingProxyWidth == 320 &&
                      actual.importedReference.workingProxyHeight == 240 &&
                      actual.importedReference.exifOrientation == 6 &&
                      actual.importedReference.workingProxyGenerated &&
                      importedResult.assets[0].bytes == std::vector<std::uint8_t>({0, 1, 2, 3, 255}),
                  "All embedded image assets should use safe internal paths and verified hashes");
    return ok;
}

bool testUnicodePackagePathRoundTrip(const std::filesystem::path &directory)
{
    MapModel source;
    source.metadata.id = "unicode-package-map";
    source.name = "München und 日本";
    source.width = 2;
    source.height = 2;
    resizeMapCells(source);
    const std::filesystem::path package =
        directory / std::filesystem::u8path(u8"Karte_München_日本.ttrpgmappkg");
    const MapPackageExportResult exported =
        MapPackageService::exportMap(source, package);
    const MapPackagePreview preview = MapPackageService::previewImport(package);
    return require(exported.success && std::filesystem::exists(package) &&
                       preview.valid && preview.mapName == source.name,
                   "Map package export and preview must preserve a UTF-8 destination path");
}

bool testLongPackagePathRoundTrip(const std::filesystem::path &directory)
{
    // Pass ordinary paths to the public API. Only fixture I/O uses the
    // Windows native prefix; callers should not need to know that detail.
    const auto deepDirectory = directory / std::string(100, 'a') / std::string(100, 'b');
    std::filesystem::create_directories(mapFileIoPath(deepDirectory));
    const auto background = deepDirectory / "background.bin";
    const auto package = deepDirectory / "portable-map.ttrpgmappkg";
    const std::vector<std::uint8_t> assetBytes = {3, 1, 4, 1, 5, 9};
    writeBytes(mapFileIoPath(background), assetBytes);

    MapModel source;
    source.metadata.id = "long-package-path-map";
    source.name = "A map in a deeply nested campaign";
    source.width = 2;
    source.height = 2;
    resizeMapCells(source);
    MapPackageExportOptions options;
    options.backgroundAssetPath = background;
    const auto exported = MapPackageService::exportPackage(source, package, options);
    bool ok = require(package.native().size() > 260 && exported.success &&
                          exported.packagePath == package,
                      "An ordinary path longer than MAX_PATH must support package export: " + exported.error);
    if (!exported.success) return false;
    const auto preview = MapPackageService::previewPackage(package);
    const auto imported = MapPackageService::importPackage(package);
    ok &= require(preview.valid && preview.embeddedAssetCount == 1 &&
                      imported.success && imported.map.name == source.name &&
                      imported.assets.size() == 1 && imported.assets.front().bytes == assetBytes,
                  "Deep package paths must preview and import embedded assets without special caller syntax");
    if (!imported.success || imported.assets.size() != 1) return false;

    source.name = "Replacement in the same deep campaign";
    const auto protectedWrite = MapPackageService::exportPackage(source, package, options);
    ok &= require(!protectedWrite.success &&
                      MapPackageService::previewPackage(package).mapName == preview.mapName,
                  "Long-path exports must preserve existing packages when overwrite is disabled");
    options.allowOverwrite = true;
    const auto replacement = MapPackageService::exportPackage(source, package, options);
    ok &= require(replacement.success && replacement.packagePath == package &&
                      MapPackageService::previewPackage(package).mapName == source.name,
                  "Allowed deep-path overwrite must commit the replacement package: " + replacement.error);

    const auto assetTarget = deepDirectory / "installed";
    std::filesystem::create_directories(mapFileIoPath(assetTarget));
    MapPackageMaterializationOptions materializationOptions;
    materializationOptions.targetDirectory = assetTarget;
    const auto installed = MapPackageService::materializePackageAssets(package, materializationOptions);
    ok &= require(installed.success && installed.assets.size() == 1,
                  "Package assets must materialize into an ordinary deep target directory: " + installed.error);
    if (!installed.success || installed.assets.size() != 1) return false;
    const auto expectedDestination = assetTarget / "assets" / "background" / background.filename();
    ok &= require(installed.assets.front().destinationPath == expectedDestination &&
                      readBytes(mapFileIoPath(expectedDestination)) == assetBytes,
                  "Deep asset installation must preserve both bytes and ordinary returned destination paths");
    const auto refusedInstall = MapPackageService::materializePackageAssets(package, materializationOptions);
    ok &= require(!refusedInstall.success && readBytes(mapFileIoPath(expectedDestination)) == assetBytes,
                  "Deep asset installation must preserve existing bytes unless overwrite is allowed");
    auto replacementAssets = imported.assets;
    replacementAssets.front().bytes = {2, 7, 1, 8};
    replacementAssets.front().contentHash = contentHash(replacementAssets.front().bytes);
    materializationOptions.allowOverwrite = true;
    const auto replacedInstall = MapPackageService::materializeAssets(replacementAssets, materializationOptions);
    ok &= require(replacedInstall.success &&
                      readBytes(mapFileIoPath(expectedDestination)) == replacementAssets.front().bytes,
                  "Allowed deep asset overwrite must commit verified replacement bytes: " + replacedInstall.error);
    return ok;
}

bool testCorruptionTruncationAndTransaction(const std::filesystem::path &directory)
{
    const auto original = directory / "valid.ttrpgmappkg";
    MapPackageExportOptions options;
    options.includeBackgroundAsset = false;
    options.includeImportedAsset = false;
    options.includeWorkingProxyAsset = false;
    bool ok = require(MapPackageService::exportPackage(makeRichMap(), original, options).success,
                      "Fixture package should export");
    std::vector<std::uint8_t> bytes = readBytes(original);
    std::vector<std::uint8_t> corrupt = bytes;
    corrupt.back() ^= 0x5a;
    const auto corruptPath = directory / "corrupt.ttrpgmappkg";
    writeBytes(corruptPath, corrupt);
    const auto truncatedPath = directory / "truncated.ttrpgmappkg";
    bytes.resize(bytes.size() / 2);
    writeBytes(truncatedPath, bytes);
    const MapPackagePreview corruptPreview = MapPackageService::previewPackage(corruptPath);
    ok &= require(!corruptPreview.valid && !corruptPreview.importEligible &&
                      corruptPreview.packageBytes == corrupt.size() &&
                      corruptPreview.formatVersion == 0 && corruptPreview.sourceMapId.empty() &&
                      corruptPreview.mapName.empty() && corruptPreview.embeddedAssetBytes == 0 &&
                      corruptPreview.assets.empty() && !corruptPreview.embeddedAssetIntegrityVerified &&
                      !MapPackageService::importPackage(corruptPath).success,
                  "Checksum corruption should fail preview and import");
    ok &= require(!MapPackageService::previewPackage(truncatedPath).valid &&
                      !MapPackageService::importPackage(truncatedPath).success,
                  "Truncated packages should fail preview and import");

    MapModel target;
    target.metadata.id = "transaction-sentinel";
    target.name = "Do not mutate";
    std::vector<MapPackageAsset> targetAssets(1);
    targetAssets[0].contentHash = "sentinel";
    std::string error;
    ok &= require(!MapPackageService::importInto(corruptPath, target, &targetAssets, &error),
                  "Transactional import should report failure");
    ok &= require(target.metadata.id == "transaction-sentinel" && target.name == "Do not mutate" &&
                      targetAssets.size() == 1 && targetAssets[0].contentHash == "sentinel" && !error.empty(),
                  "Failed import must not mutate its map or asset destinations");
    return ok;
}

bool testOverwriteProtection(const std::filesystem::path &directory)
{
    const auto package = directory / "overwrite.ttrpgmappkg";
    writeBytes(package, {'k', 'e', 'e', 'p'});
    MapPackageExportOptions options;
    options.includeBackgroundAsset = false;
    options.includeImportedAsset = false;
    options.includeWorkingProxyAsset = false;
    bool ok = require(!MapPackageService::exportPackage(makeRichMap(), package, options).success,
                      "Export must refuse an existing destination by default");
    ok &= require(readBytes(package) == std::vector<std::uint8_t>({'k', 'e', 'e', 'p'}),
                  "Refused overwrite must preserve existing bytes");
    options.allowOverwrite = true;
    ok &= require(MapPackageService::exportPackage(makeRichMap(), package, options).success &&
                      MapPackageService::previewPackage(package).valid,
                  "Explicit overwrite should atomically replace the destination");
    return ok;
}

bool testStandaloneIdRemap(const std::filesystem::path &directory)
{
    const auto package = directory / "remap.ttrpgmappkg";
    MapPackageExportOptions options;
    options.includeBackgroundAsset = false;
    options.includeImportedAsset = false;
    options.includeWorkingProxyAsset = false;
    bool ok = require(MapPackageService::exportPackage(makeRichMap(), package, options).success,
                      "ID-remap fixture should export");
    const MapPackageImportResult result = MapPackageService::importPackage(package);
    ok &= require(result.success, "Default standalone import should succeed");
    if (!result.success) return false;
    const MapModel &map = result.map;
    const std::set<std::string> oldIds = {"map-original", "layer-original", "token-original", "region-original",
                                          "feature-original", "route-original", "generated-original", "constraint-one",
                                          "fact-one", "asset-imported", "asset-working-proxy"};
    std::set<std::string> newIds;
    for (const auto &item : result.idRemapping)
    {
        ok &= require(oldIds.count(item.first) == 1 && item.first != item.second,
                      "Every reported ID mapping should replace an owned identity");
        newIds.insert(item.second);
    }
    ok &= require(result.idRemapping.size() == oldIds.size() && newIds.size() == oldIds.size(),
                  "Map and all owned child identities should receive unique new IDs");
    ok &= require(map.metadata.id != "map-original" && map.layers[0].metadata.id != "layer-original" &&
                      map.tokens[0].metadata.id != "token-original" && map.regions[0].metadata.id != "region-original" &&
                      map.features[0].metadata.id != "feature-original" && map.routes[0].metadata.id != "route-original" &&
                      map.generation.graph.features[0].id != "generated-original" &&
                      map.generation.recipe.constraints[0].id != "constraint-one" &&
                      map.generation.recipe.facts[0].id != "fact-one" &&
                      map.importedReference.assetId != "asset-imported" &&
                      map.importedReference.workingProxyAssetId != "asset-working-proxy",
                  "All owned identity fields should be remapped");
    ok &= require(map.tokens[0].layerId == map.layers[0].metadata.id &&
                      map.tokens[0].linkedEntityId == map.features[0].metadata.id &&
                      map.tokens[0].assetId == map.importedReference.assetId &&
                      map.features[0].linkedEntityId == map.tokens[0].metadata.id &&
                      map.features[0].targetMapId == map.metadata.id &&
                      map.routes[0].fromMapId == map.metadata.id && map.routes[0].toMapId == map.metadata.id &&
                      map.generation.recipe.lockedFeatureIds[0] == map.generation.graph.features[0].id &&
                      map.generation.graph.features[0].lineage.sourceMapId == map.metadata.id &&
                      map.generation.graph.features[0].lineage.sourceFeatureId == map.generation.graph.features[0].id,
                  "Internal references should follow remapped identities");
    ok &= require(map.parentMapId.empty() && map.atlasPlacement.parentId.empty() &&
                      map.atlasPlacement.parentRevision == 0 && map.atlasStatus == MapAtlasStatus::Standalone,
                  "Default import should have no external atlas parent link");
    ok &= require(map.generation.recipe.generatorVersion == 73 && map.generation.recipe.recipeVersion == 19 &&
                      map.generation.recipe.styleVersion == 41 &&
                      map.generation.recipe.styleSnapshotId == "snapshot-immutable-123" &&
                      map.generation.graph.schemaVersion == 29,
                  "ID remapping must preserve generation and snapshot versions");
    return ok;
}

bool testIndependentCopyLineageImpact(const std::filesystem::path &directory)
{
    MapModel source = makeRichMap();
    MapGeneratedFeature inherited;
    inherited.id = "inherited-external";
    inherited.kind = MapGeneratedFeatureKind::River;
    inherited.name = "Inherited river";
    inherited.locked = true;
    inherited.points = {{0.0f, 0.5f}, {2.0f, 0.5f}};
    inherited.lineage = {"external-parent", "parent-river", 52};
    source.generation.graph.features.push_back(inherited);

    const auto package = directory / "independent-copy-lineage.ttrpgmappkg";
    MapPackageExportOptions exportOptions;
    exportOptions.includeBackgroundAsset = false;
    exportOptions.includeImportedAsset = false;
    exportOptions.includeWorkingProxyAsset = false;
    bool ok = require(MapPackageService::exportPackage(source, package, exportOptions).success,
                      "Independent-copy lineage fixture should export");

    const MapPackagePreview preview = MapPackageService::previewPackage(package);
    ok &= require(preview.valid && preview.importEligible && preview.sourceHasAtlasParent &&
                      preview.sourceFeatureLineageCount == 2 &&
                      preview.externalFeatureLineageCount == 1 &&
                      preview.independentCopyDetachedLineageCount == 1 &&
                      preview.independentCopyRetainedInternalLineageCount == 1 &&
                      preview.independentCopyIdRemapCount == 12,
                  "Preview should distinguish retained self-lineage from detached external lineage");

    const MapPackageImportResult copied = MapPackageService::importPackage(package);
    ok &= require(copied.success, "Independent copy with external lineage should import");
    if (copied.success)
    {
        const auto external = std::find_if(copied.map.generation.graph.features.begin(),
                                           copied.map.generation.graph.features.end(),
                                           [](const MapGeneratedFeature &feature) {
                                               return feature.name == "Inherited river";
                                           });
        const auto internal = std::find_if(copied.map.generation.graph.features.begin(),
                                           copied.map.generation.graph.features.end(),
                                           [](const MapGeneratedFeature &feature) {
                                               return feature.name == "Cliff";
                                           });
        ok &= require(external != copied.map.generation.graph.features.end() &&
                          external->lineage.sourceMapId.empty() &&
                          external->lineage.sourceFeatureId.empty() &&
                          external->lineage.sourceRevision == 0 && !external->locked,
                      "Independent copy should detach and unlock lineage owned by an external atlas map");
        ok &= require(internal != copied.map.generation.graph.features.end() &&
                          internal->lineage.sourceMapId == copied.map.metadata.id &&
                          internal->lineage.sourceFeatureId == internal->id,
                      "Independent copy should retain and remap lineage that is internal to the package");
    }

    MapPackageImportOptions preserve;
    preserve.standaloneCopy = false;
    preserve.remapEntityIds = false;
    const MapPackageImportResult linked = MapPackageService::importPackage(package, preserve);
    if (!linked.success)
        return require(false, "Linked import should preserve validated source lineage");
    const auto inheritedLinked = std::find_if(linked.map.generation.graph.features.begin(),
                                              linked.map.generation.graph.features.end(),
                                              [](const MapGeneratedFeature &feature) {
                                                  return feature.name == "Inherited river";
                                              });
    ok &= require(linked.map.parentMapId == "external-parent" &&
                      linked.map.atlasStatus == MapAtlasStatus::Stale &&
                      inheritedLinked != linked.map.generation.graph.features.end() &&
                      inheritedLinked->lineage.sourceMapId == "external-parent" &&
                      inheritedLinked->locked,
                  "Non-standalone import should preserve source atlas lineage unchanged");
    return ok;
}

bool testGeneratedRequirementIdentityAliases(const std::filesystem::path &directory)
{
    MapModel source = makeRichMap();
    const std::string id = source.generation.graph.features[0].id;
    source.generation.recipe.constraints[0].id = id;
    source.generation.recipe.facts[0].id = id;
    source.generation.recipe.facts[0].entityRefs = {id};
    const auto package = directory / "generated-requirement-aliases.ttrpgmappkg";
    MapPackageExportOptions options;
    options.includeBackgroundAsset = options.includeImportedAsset = options.includeWorkingProxyAsset = false;
    bool ok = require(MapPackageService::exportPackage(source, package, options).success,
                      "Generated requirement alias fixture should export");
    const auto preview = MapPackageService::previewPackage(package);
    ok &= require(preview.importEligible && preview.duplicateOwnedIdCount == 0,
                  "Fact, constraint and generated feature may share their derivation ID");
    for (bool remap : {false, true})
    {
        MapPackageImportOptions importOptions;
        importOptions.remapEntityIds = remap;
        importOptions.standaloneCopy = false;
        const auto restored = MapPackageService::importPackage(package, importOptions);
        ok &= require(restored.success, "Both recovery and cloning must accept generated requirements");
        if (restored.success)
        {
            const auto &generated = restored.map.generation.graph.features[0];
            ok &= require(restored.map.generation.recipe.facts[0].id == generated.id &&
                          restored.map.generation.recipe.constraints[0].id == generated.id &&
                          restored.map.generation.recipe.facts[0].entityRefs[0] == generated.id &&
                          (remap ? generated.id != id : generated.id == id),
                          "Recovery preserves IDs; cloning remaps linked requirements together");
        }
    }
    source.generation.recipe.facts.push_back(source.generation.recipe.facts.front());
    const auto duplicatePackage = directory / "duplicate-recipe-facts.ttrpgmappkg";
    ok &= require(MapPackageService::exportPackage(source, duplicatePackage, options).success &&
                  !MapPackageService::previewPackage(duplicatePackage).importEligible,
                  "Duplicate facts within one typed namespace remain invalid");
    return ok;
}

bool testOwnedIdentityConflictPreview(const std::filesystem::path &directory)
{
    MapModel source = makeRichMap();
    source.regions[0].metadata.id = source.layers[0].metadata.id;
    const auto package = directory / "owned-id-conflict.ttrpgmappkg";
    MapPackageExportOptions options;
    options.includeBackgroundAsset = false;
    options.includeImportedAsset = false;
    options.includeWorkingProxyAsset = false;
    bool ok = require(MapPackageService::exportPackage(source, package, options).success,
                      "Owned-identity conflict fixture should remain structurally exportable");
    const MapPackagePreview preview = MapPackageService::previewPackage(package);
    ok &= require(preview.valid && !preview.importEligible &&
                      preview.ownedIdFieldCount == 11 && preview.uniqueOwnedIdCount == 10 &&
                      preview.duplicateOwnedIdCount == 1 &&
                      preview.independentCopyIdRemapCount == 10,
                  "Validated preview should expose owned-ID conflicts before import");
    const MapPackageImportResult imported = MapPackageService::importPackage(package);
    ok &= require(!imported.success && imported.idRemapping.empty() &&
                      imported.error == "Map package contains conflicting owned entity identities.",
                  "Import must refuse identity conflicts before generating or applying remaps");
    return ok;
}

bool testSafeTransactionalMaterialization(const std::filesystem::path &directory)
{
    const auto background = directory / "materialize-bg.dat";
    const auto imported = directory / "materialize-reference.dat";
    const auto proxy = directory / "materialize-working.png";
    const std::vector<std::uint8_t> backgroundBytes{11, 22, 33, 44};
    const std::vector<std::uint8_t> importedBytes{55, 66, 77};
    const std::vector<std::uint8_t> proxyBytes{88, 99};
    writeBytes(background, backgroundBytes);
    writeBytes(imported, importedBytes);
    writeBytes(proxy, proxyBytes);
    const auto package = directory / "materialize.ttrpgmappkg";
    MapPackageExportOptions exportOptions;
    exportOptions.backgroundAssetPath = background;
    exportOptions.importedAssetPath = imported;
    exportOptions.workingProxyAssetPath = proxy;
    MapModel source = makeRichMap();
    source.importedReference.contentHash = contentHash(importedBytes);
    source.importedReference.workingProxyContentHash = contentHash(proxyBytes);
    bool ok = require(MapPackageService::exportPackage(source, package, exportOptions).success,
                      "Materialization fixture should export");

    const auto target = directory / "materialized-project";
    std::filesystem::create_directory(target);
    MapPackageMaterializationOptions options;
    options.targetDirectory = target;
    const MapPackageMaterializationResult first =
        MapPackageService::materializePackageAssets(package, options);
    ok &= require(first.success && first.assets.size() == 3,
                  "Embedded package assets should materialize transactionally");
    if (!first.success) return false;
    ok &= require(first.assets[0].projectRelativePath == "assets/background/materialize-bg.dat" &&
                      first.assets[1].projectRelativePath == "assets/imported/materialize-reference.dat" &&
                      first.assets[2].projectRelativePath == "assets/working-proxy/materialize-working.png" &&
                      first.assets[0].destinationPath == target / first.assets[0].projectRelativePath &&
                      readBytes(first.assets[0].destinationPath) == backgroundBytes &&
                      readBytes(first.assets[1].destinationPath) == importedBytes &&
                      readBytes(first.assets[2].destinationPath) == proxyBytes,
                  "Materialization should return usable project-relative paths and exact bytes");

    const MapPackageMaterializationResult refused =
        MapPackageService::materializePackageAssets(package, options);
    ok &= require(!refused.success && readBytes(first.assets[0].destinationPath) == backgroundBytes,
                  "Materialization must neither silently overwrite nor damage existing files");
    writeBytes(first.assets[0].destinationPath, {1, 1, 1});
    options.allowOverwrite = true;
    const MapPackageMaterializationResult replaced =
        MapPackageService::materializePackageAssets(package, options);
    ok &= require(replaced.success && readBytes(first.assets[0].destinationPath) == backgroundBytes,
                  "Explicit overwrite should replace assets with verified package bytes");

    MapPackageImportOptions preserve;
    preserve.standaloneCopy = false;
    preserve.remapEntityIds = false;
    const MapPackageImportResult decoded = MapPackageService::importPackage(package, preserve);
    ok &= require(decoded.success && decoded.assets.size() == 3, "Materialization fixture should decode all image assets");
    const auto unsafeTarget = directory / "unsafe-materialization";
    std::filesystem::create_directory(unsafeTarget);
    std::vector<MapPackageAsset> unsafeAssets = decoded.assets;
    unsafeAssets[0].packagePath = "../escaped.dat";
    MapPackageMaterializationOptions unsafeOptions;
    unsafeOptions.targetDirectory = unsafeTarget;
    ok &= require(!MapPackageService::materializeAssets(unsafeAssets, unsafeOptions).success &&
                      !std::filesystem::exists(directory / "escaped.dat") &&
                      std::filesystem::is_empty(unsafeTarget),
                  "Traversal paths must be rejected without writing anything");

    std::vector<MapPackageAsset> tamperedAssets = decoded.assets;
    tamperedAssets[0].bytes[0] ^= 0xff;
    ok &= require(!MapPackageService::materializeAssets(tamperedAssets, unsafeOptions).success &&
                      std::filesystem::is_empty(unsafeTarget),
                  "Hash-mismatched assets must be rejected before materialization");

    const auto collisionTarget = directory / "collision-materialization";
    std::filesystem::create_directory(collisionTarget);
    std::vector<MapPackageAsset> collidingAssets = decoded.assets;
    collidingAssets[0].packagePath = "assets/same.dat";
    collidingAssets[1].packagePath = "assets/same.dat";
    MapPackageMaterializationOptions collisionOptions;
    collisionOptions.targetDirectory = collisionTarget;
    ok &= require(!MapPackageService::materializeAssets(collidingAssets, collisionOptions).success &&
                      std::filesystem::is_empty(collisionTarget),
                  "Duplicate destinations must be rejected before staging");
    return ok;
}

bool testImportedOriginalAndProxyPackage(const std::filesystem::path &directory)
{
    const auto original = directory / "portable-original.jpg";
    const auto proxy = directory / "portable-working.png";
    writeBytes(original, {0xff, 0xd8, 1, 2, 3, 0xff, 0xd9});
    writeBytes(proxy, {137, 80, 78, 71, 4, 5, 6});
    MapModel source = makeRichMap();
    source.backgroundImagePath = source.importedReference.workingProxyProjectRelativePath;
    source.importedReference.contentHash = contentHash(readBytes(original));
    source.importedReference.workingProxyContentHash = contentHash(readBytes(proxy));
    const auto package = directory / "original-and-proxy.ttrpgmappkg";
    MapPackageExportOptions options;
    options.importedAssetPath = original;
    options.workingProxyAssetPath = proxy;
    const MapPackageExportResult exported = MapPackageService::exportPackage(source, package, options);
    bool ok = require(exported.success, "Managed original/proxy package should export: " + exported.error);
    const MapPackagePreview preview = MapPackageService::previewPackage(package);
    ok &= require(preview.valid && preview.formatVersion == MapPackageService::CurrentFormatVersion &&
                      preview.assets.size() == 2 &&
                      preview.assets[0].kind == MapPackageAssetKind::ImportedReference &&
                      preview.assets[1].kind == MapPackageAssetKind::WorkingProxy,
                  "Aliased runtime background must not duplicate the proxy bytes in a package");
    MapPackageImportOptions preserve;
    preserve.standaloneCopy = false;
    preserve.remapEntityIds = false;
    const MapPackageImportResult imported = MapPackageService::importPackage(package, preserve);
    ok &= require(imported.success &&
                      imported.map.importedReference.projectRelativePath == "assets/imported/portable-original.jpg" &&
                      imported.map.importedReference.workingProxyProjectRelativePath ==
                          "assets/working-proxy/portable-working.png" &&
                      imported.map.backgroundImagePath == "assets/working-proxy/portable-working.png" &&
                      imported.map.importedReference.workingProxyGenerated,
                  "Package import should retain immutable original and select its working proxy");
    return ok;
}

bool testManagedImageIntegrityGate(const std::filesystem::path &directory)
{
    const auto original = directory / "integrity-original.jpg";
    const auto proxy = directory / "integrity-proxy.png";
    writeBytes(original, {0xff, 0xd8, 1, 2, 3, 0xff, 0xd9});
    writeBytes(proxy, {137, 80, 78, 71, 1, 2, 3});
    MapModel source = makeRichMap();
    source.backgroundImagePath = source.importedReference.workingProxyProjectRelativePath;
    source.importedReference.contentHash = contentHash(readBytes(original));
    source.importedReference.workingProxyContentHash = contentHash(readBytes(proxy));

    MapPackageExportOptions options;
    options.importedAssetPath = original;
    options.workingProxyAssetPath = proxy;
    writeBytes(original, {0xff, 0xd8, 9, 9, 9, 0xff, 0xd9});
    const auto package = directory / "changed-original.ttrpgmappkg";
    const MapPackageExportResult changedOriginal = MapPackageService::exportPackage(source, package, options);
    bool ok = require(!changedOriginal.success && !std::filesystem::exists(package),
                      "A changed managed original must fail closed without emitting a package");

    source.importedReference.contentHash = contentHash(readBytes(original));
    writeBytes(proxy, {137, 80, 78, 71, 8, 8, 8});
    const MapPackageExportResult changedProxy = MapPackageService::exportPackage(source, package, options);
    ok &= require(!changedProxy.success && !std::filesystem::exists(package),
                  "A changed managed working proxy must fail closed without emitting a package");
    // Failure is export-only: the editable map graph, fog and tokens remain in
    // the caller-owned model even when an image file goes missing or changes.
    ok &= require(source.generation.graph.features.size() == 1 && source.cells[0].fogged &&
                      source.tokens.size() == 1 && source.features.size() == 1,
                  "Image integrity failures must never mutate editable map semantics");
    return ok;
}

bool testManagedPathResolutionOutsideWorkingDirectory(const std::filesystem::path &directory)
{
    const std::filesystem::path dataRoot = directory / "portable-data-root";
    const std::filesystem::path assetDirectory =
        dataRoot / "Resources" / "Campaigns" / "portable" / "Assets" / "Maps";
    std::filesystem::create_directories(assetDirectory);
    const std::filesystem::path original = assetDirectory / "original.jpg";
    const std::filesystem::path proxy = assetDirectory / "proxy.png";
    const std::vector<std::uint8_t> originalBytes{0xff, 0xd8, 4, 5, 6, 0xff, 0xd9};
    const std::vector<std::uint8_t> proxyBytes{137, 80, 78, 71, 9, 8, 7};
    writeBytes(original, originalBytes);
    writeBytes(proxy, proxyBytes);

    MapModel source = makeRichMap();
    source.importedReference.projectRelativePath =
        "Resources/Campaigns/portable/Assets/Maps/original.jpg";
    source.importedReference.workingProxyProjectRelativePath =
        "Resources/Campaigns/portable/Assets/Maps/proxy.png";
    source.importedReference.contentHash = contentHash(originalBytes);
    source.importedReference.workingProxyContentHash = contentHash(proxyBytes);
    source.backgroundImagePath = source.importedReference.workingProxyProjectRelativePath;

    const ScopedEnvironment dataRootOverride("TTRPG_DATA_ROOT", std::filesystem::absolute(dataRoot).string());
    const auto package = directory / "runtime-root-resolution.ttrpgmappkg";
    const MapPackageExportResult exported = MapPackageService::exportPackage(source, package);
    bool ok = require(exported.success,
                      "Project-relative managed images should resolve through the runtime data root: " +
                          exported.error);
    const MapPackagePreview preview = MapPackageService::previewPackage(package);
    ok &= require(preview.valid && preview.assets.size() == 2,
                  "A package exported outside the data-root working directory should retain original and proxy");
    return ok;
}

bool testLegacyV2RecipeCompatibility(const std::filesystem::path &directory)
{
    const auto current = directory / "format-v6.ttrpgmappkg";
    MapPackageExportOptions options;
    options.includeBackgroundAsset = false;
    options.includeImportedAsset = false;
    options.includeWorkingProxyAsset = false;
    bool ok = require(MapPackageService::exportPackage(makeRichMap(), current, options).success,
                      "Current-format compatibility fixture should export");
    std::vector<std::uint8_t> bytes = readBytes(current);
    // Reconstruct the previous V5 stream by removing only V6 Travel fields.
    const std::vector<std::uint8_t> travelEnvelope{4,0,0,0,9,0,0,0,71,114,97,115,115,108,97,110,100,2,0,0,0,1,221,61,15,0};
    const auto travelMarker = std::search(bytes.begin()+32, bytes.end(), travelEnvelope.begin(), travelEnvelope.end());
    ok &= require(travelMarker != bytes.end(), "Could not locate V6 Travel fields in fixture");
    if (travelMarker == bytes.end()) return false;
    bytes.erase(travelMarker, travelMarker + travelEnvelope.size());
    writeLittleEndian(bytes,12,5,4);
    writeLittleEndian(bytes,16,bytes.size()-32u,8);
    writeLittleEndian(bytes,24,fnv1a(bytes.data()+32,bytes.size()-32u),8);
    const auto v5Path = directory / "format-v5.ttrpgmappkg";
    writeBytes(v5Path,bytes);
    MapPackageImportOptions preserveV5; preserveV5.standaloneCopy=false; preserveV5.remapEntityIds=false;
    const auto importedV5 = MapPackageService::importPackage(v5Path,preserveV5);
    ok &= require(importedV5.success && importedV5.map.generation.recipe.landform == MapLandform::InlandLake &&
                  importedV5.map.generation.recipe.waterLevel == 71 && importedV5.map.generation.recipe.travelScenePurpose == -1 &&
                  importedV5.map.generation.recipe.travelSceneBiome.empty(),
                  "V5 packages must retain existing controls without inventing a Travel recipe");
    // Reconstruct a real V4 payload before testing earlier readers. A V5
    // header relabel alone would leave its new fields in the old byte stream.
    const std::vector<std::uint8_t> setupEnvelope{
        11,0,0,0,'i','n','l','a','n','d','_','l','a','k','e',
        71,0,0,0,44,0,0,0,0,0,0,0,62,0,0,0,19,0,0,0,13,0,0,0};
    const auto setupMarker = std::search(bytes.begin()+32, bytes.end(), setupEnvelope.begin(), setupEnvelope.end());
    ok &= require(setupMarker != bytes.end(), "Could not locate V5 structured setup fields in the fixture");
    if (setupMarker == bytes.end()) return false;
    bytes.erase(setupMarker, setupMarker + setupEnvelope.size());
    writeLittleEndian(bytes,12,4,4);
    writeLittleEndian(bytes,16,bytes.size()-32u,8);
    writeLittleEndian(bytes,24,fnv1a(bytes.data()+32,bytes.size()-32u),8);
    const auto v4Path = directory / "format-v4.ttrpgmappkg";
    writeBytes(v4Path,bytes);
    MapPackageImportOptions preserveV4; preserveV4.standaloneCopy=false; preserveV4.remapEntityIds=false;
    const auto importedV4 = MapPackageService::importPackage(v4Path,preserveV4);
    ok &= require(importedV4.success && importedV4.map.generation.recipe.landform == MapLandform::Automatic &&
                  importedV4.map.generation.recipe.waterLevel == -1 && importedV4.map.generation.recipe.forestDensity == -1 &&
                  importedV4.map.playerRelease.politicalBorders,
                  "V4 packages must retain publication settings and preserve unknown historical generator controls");
    // Remove the V4 player-release fields first. The long envelope includes
    // proxy dimensions, EXIF orientation, and the following cell count so it
    // cannot be confused with an arbitrary Boolean sequence in map content.
    const std::vector<std::uint8_t> releaseEnvelope{
        0x40, 0x01, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00,
        0x06, 0x00, 0x00, 0x00, 0x01,
        0x01, 0x00, 0x01, 0x00, 0x01,
        0x02, 0x00, 0x00, 0x00};
    auto releaseMarker = std::search(bytes.begin() + 32, bytes.end(),
                                     releaseEnvelope.begin(), releaseEnvelope.end());
    ok &= require(releaseMarker != bytes.end(),
                  "Could not locate the V4 player-release fields in the fixture");
    if (releaseMarker == bytes.end()) return false;
    bytes.erase(releaseMarker + 13, releaseMarker + 18);

    // V3 is the immediately preceding format. It contains the full recipe
    // contract but no per-map player publication profile.
    std::vector<std::uint8_t> v3Bytes = bytes;
    writeLittleEndian(v3Bytes, 12, 3, 4);
    const std::uint64_t v3PayloadSize = v3Bytes.size() - 32u;
    writeLittleEndian(v3Bytes, 16, v3PayloadSize, 8);
    writeLittleEndian(v3Bytes, 24,
                      fnv1a(v3Bytes.data() + 32, static_cast<std::size_t>(v3PayloadSize)), 8);
    const auto previous = directory / "format-v3.ttrpgmappkg";
    writeBytes(previous, v3Bytes);
    const MapPackagePreview v3Preview = MapPackageService::previewPackage(previous);
    ok &= require(v3Preview.valid && v3Preview.formatVersion == 3 &&
                      v3Preview.recipeVersion == 19 &&
                      !v3Preview.playerRelease.politicalBorders &&
                      !v3Preview.playerRelease.waysAndRoutes &&
                      !v3Preview.playerRelease.placesAndMarkers &&
                      !v3Preview.playerRelease.labels &&
                      !v3Preview.playerRelease.tokensAndAnnotations,
                  "V3 packages must retain recipe data and default new player categories hidden");
    MapPackageImportOptions preserveV3;
    preserveV3.standaloneCopy = false;
    preserveV3.remapEntityIds = false;
    const MapPackageImportResult importedV3 =
        MapPackageService::importPackage(previous, preserveV3);
    ok &= require(importedV3.success &&
                      importedV3.map.generation.recipe.recipeVersion == 19 &&
                      !importedV3.map.playerRelease.politicalBorders &&
                      !importedV3.map.playerRelease.waysAndRoutes &&
                      !importedV3.map.playerRelease.placesAndMarkers &&
                      !importedV3.map.playerRelease.labels &&
                      !importedV3.map.playerRelease.tokensAndAnnotations,
                  "V3 map import did not apply the fail-closed publication migration");

    // A V2 payload has generatorVersion immediately followed by recipeId. The
    // unique fixture sequence then removes the V3 recipe-version field.
    const std::vector<std::uint8_t> versionPair{73, 0, 0, 0, 19, 0, 0, 0};
    auto marker = std::search(bytes.begin() + 32, bytes.end(), versionPair.begin(), versionPair.end());
    ok &= require(marker != bytes.end(), "Could not locate the V3 recipe-version field in the fixture");
    if (marker == bytes.end()) return false;
    bytes.erase(marker + 4, marker + 8);
    writeLittleEndian(bytes, 12, 2, 4);
    const std::uint64_t payloadSize = bytes.size() - 32u;
    writeLittleEndian(bytes, 16, payloadSize, 8);
    writeLittleEndian(bytes, 24, fnv1a(bytes.data() + 32, static_cast<std::size_t>(payloadSize)), 8);
    const auto legacy = directory / "format-v2.ttrpgmappkg";
    writeBytes(legacy, bytes);

    const MapPackagePreview preview = MapPackageService::previewPackage(legacy);
    ok &= require(preview.valid && preview.formatVersion == 2 && preview.recipeVersion == 3,
                  "V2 packages should remain readable with their historical V3 recipe contract");
    MapPackageImportOptions preserve;
    preserve.standaloneCopy = false;
    preserve.remapEntityIds = false;
    const MapPackageImportResult imported = MapPackageService::importPackage(legacy, preserve);
    ok &= require(imported.success && imported.map.generation.recipe.generatorVersion == 73 &&
                      imported.map.generation.recipe.recipeVersion == 3 &&
                      imported.map.generation.recipe.recipeId == "portable-recipe-v9" &&
                      !imported.map.playerRelease.politicalBorders &&
                      !imported.map.playerRelease.waysAndRoutes &&
                      !imported.map.playerRelease.placesAndMarkers &&
                      !imported.map.playerRelease.labels &&
                      !imported.map.playerRelease.tokensAndAnnotations,
                  "V2 recipe fields after the omitted version must remain aligned");
    return ok;
}

bool testRecoverableDraftInstallTransaction(const std::filesystem::path &directory)
{
    const std::filesystem::path fixtureDirectory = directory / "draft-install-fixture";
    std::filesystem::create_directories(fixtureDirectory);
    const std::filesystem::path background = fixtureDirectory / "world-background.bin";
    const std::vector<std::uint8_t> backgroundBytes{41, 42, 43, 44, 45, 46};
    writeBytes(background, backgroundBytes);
    MapModel source = makeRichMap();
    source.name = "Recoverable package map";
    source.backgroundImagePath = background.string();
    source.importedReference = {};
    const std::filesystem::path package = fixtureDirectory / "recoverable.ttrpgmappkg";
    MapPackageExportOptions exportOptions;
    exportOptions.backgroundAssetPath = background;
    exportOptions.includeImportedAsset = false;
    exportOptions.includeWorkingProxyAsset = false;
    bool ok = require(MapPackageService::exportPackage(source, package, exportOptions).success,
                      "Recoverable draft transaction fixture should export");

    const auto makeOptions = [&](const std::string &name) {
        const std::filesystem::path project = directory / name;
        const std::filesystem::path library = project / "Assets" / "MapPackages";
        std::filesystem::create_directories(library);
        MapPackageDraftInstallOptions options;
        options.projectRoot = project;
        options.assetLibraryDirectory = library;
        return options;
    };
    const auto draftUnchanged = [](const MapModel &before, const MapModel &after) {
        return before.metadata.id == after.metadata.id && before.name == after.name &&
               before.revision == after.revision && before.backgroundImagePath == after.backgroundImagePath;
    };

    for (const auto failure : {
             MapPackageDraftInstallFailurePoint::AfterPackageDecode,
             MapPackageDraftInstallFailurePoint::AfterAssetMaterialization,
             MapPackageDraftInstallFailurePoint::AfterAssetDirectoryCommit,
             MapPackageDraftInstallFailurePoint::AfterDraftSwap})
    {
        MapPackageDraftInstallOptions options =
            makeOptions("ri" + std::to_string(static_cast<int>(failure)));
        options.failurePoint = failure;
        MapModel target;
        target.name = "Keep this draft";
        target.revision = 991;
        const MapModel before = target;
        const MapPackageDraftInstallResult installed =
            MapPackageDraftInstallService::installIntoDraft(package, target, options);
        ok &= require(!installed.success && draftUnchanged(before, target),
                      "Every injected package-install failure must preserve the previous draft");
        ok &= require(std::filesystem::is_empty(options.assetLibraryDirectory),
                      "Every ordinary package-install failure must remove staging and committed assets");
    }

    MapPackageDraftInstallOptions deferred = makeOptions("ds");
    deferred.deferRecoveryAcknowledgement = true;
    MapModel installedDraft;
    installedDraft.name = "Previous draft";
    const MapPackageDraftInstallResult installed =
        MapPackageDraftInstallService::installIntoDraft(package, installedDraft, deferred);
    ok &= require(installed.success && installed.recoveryAvailable &&
                      installedDraft.name == source.name &&
                      !installedDraft.backgroundImagePath.empty() &&
                      !std::filesystem::path(installedDraft.backgroundImagePath).is_absolute(),
                  "A deferred package install should atomically replace the draft with managed relative asset paths: " +
                      installed.error);
    ok &= require(std::filesystem::exists(
                      deferred.projectRoot / installedDraft.backgroundImagePath) &&
                      readBytes(deferred.projectRoot / installedDraft.backgroundImagePath) == backgroundBytes,
                  "A committed package draft should reference the exact materialized bytes");

    MapPackageDraftRecoveryOptions recoveryOptions;
    recoveryOptions.projectRoot = deferred.projectRoot;
    recoveryOptions.assetLibraryDirectory = deferred.assetLibraryDirectory;
    const MapPackageDraftRecoveryResult replay =
        MapPackageDraftInstallService::recoverLatest(recoveryOptions);
    ok &= require(replay.found && replay.success &&
                      replay.transactionId == installed.transactionId &&
                      replay.map.metadata.id == installedDraft.metadata.id &&
                      replay.map.backgroundImagePath == installedDraft.backgroundImagePath,
                  "A deferred successful import should remain exactly recoverable until durable acknowledgement");
    std::string acknowledgeError;
    ok &= require(MapPackageDraftInstallService::acknowledgeRecovery(
                      recoveryOptions, installed.transactionId, &acknowledgeError),
                  "A durably recorded import should clear its recovery record: " + acknowledgeError);
    ok &= require(std::filesystem::exists(installed.finalAssetDirectory) &&
                      !std::filesystem::exists(
                          installed.finalAssetDirectory / ".owner") &&
                      !MapPackageDraftInstallService::recoverLatest(recoveryOptions).found,
                  "Acknowledgement should preserve production assets but prevent stale replay");

    MapPackageDraftInstallOptions interrupted = makeOptions("is");
    interrupted.failurePoint = MapPackageDraftInstallFailurePoint::AfterAssetMaterialization;
    interrupted.retainInjectedFailureForRecovery = true;
    MapModel interruptedTarget;
    interruptedTarget.name = "Still unchanged";
    const MapModel interruptedBefore = interruptedTarget;
    const MapPackageDraftInstallResult interruption =
        MapPackageDraftInstallService::installIntoDraft(package, interruptedTarget, interrupted);
    ok &= require(!interruption.success && interruption.recoveryAvailable &&
                      draftUnchanged(interruptedBefore, interruptedTarget),
                  "A simulated process interruption should leave the previous draft untouched and a recovery record available: " +
                      interruption.error);
    MapPackageDraftRecoveryOptions interruptedRecoveryOptions;
    interruptedRecoveryOptions.projectRoot = interrupted.projectRoot;
    interruptedRecoveryOptions.assetLibraryDirectory = interrupted.assetLibraryDirectory;
    const MapPackageDraftRecoveryResult recovered =
        MapPackageDraftInstallService::recoverLatest(interruptedRecoveryOptions);
    ok &= require(recovered.found && recovered.success && recovered.map.name == source.name &&
                      std::filesystem::exists(recovered.finalAssetDirectory),
                  "Restart recovery should finish a fully staged asset-tree rename and expose the confirmed draft");
    std::string discardError;
    ok &= require(MapPackageDraftInstallService::discardRecovery(
                      interruptedRecoveryOptions, interruption.transactionId, &discardError),
                  "Discarding an unsaved recovered import should succeed: " + discardError);
    ok &= require(!std::filesystem::exists(recovered.finalAssetDirectory) &&
                      std::filesystem::is_empty(interrupted.assetLibraryDirectory),
                  "Discarding recovery should remove only the transaction-owned imported assets");

    MapPackageDraftInstallOptions tampered = makeOptions("tr");
    tampered.failurePoint = MapPackageDraftInstallFailurePoint::AfterAssetDirectoryCommit;
    tampered.retainInjectedFailureForRecovery = true;
    MapModel tamperedTarget;
    const MapPackageDraftInstallResult tamperedInterruption =
        MapPackageDraftInstallService::installIntoDraft(package, tamperedTarget, tampered);
    ok &= require(tamperedInterruption.recoveryAvailable,
                  "Committed interruption fixture should retain recovery state: " + tamperedInterruption.error);
    const std::filesystem::path tamperedAsset =
        tamperedInterruption.finalAssetDirectory / "assets" / "background" /
        background.filename();
    writeBytes(tamperedAsset, {0, 0, 0, 0, 0, 0});
    MapPackageDraftRecoveryOptions tamperedRecoveryOptions;
    tamperedRecoveryOptions.projectRoot = tampered.projectRoot;
    tamperedRecoveryOptions.assetLibraryDirectory = tampered.assetLibraryDirectory;
    const MapPackageDraftRecoveryResult rejectedRecovery =
        MapPackageDraftInstallService::recoverLatest(tamperedRecoveryOptions);
    ok &= require(rejectedRecovery.found && !rejectedRecovery.success,
                  "Restart recovery must fail closed when committed asset bytes were modified");
    ok &= require(MapPackageDraftInstallService::discardRecovery(
                      tamperedRecoveryOptions, tamperedInterruption.transactionId, &discardError),
                  "Even a corrupt interrupted import should be safely discardable when ownership is intact");

    MapPackageDraftInstallOptions orphan = makeOptions("oc");
    const std::filesystem::path incomplete = orphan.assetLibraryDirectory / ".mpi-incomplete";
    std::filesystem::create_directories(incomplete / "p");
    writeBytes(incomplete / "p" / "partial.bin", {1, 2, 3});
    MapPackageDraftRecoveryOptions orphanRecoveryOptions;
    orphanRecoveryOptions.projectRoot = orphan.projectRoot;
    orphanRecoveryOptions.assetLibraryDirectory = orphan.assetLibraryDirectory;
    const MapPackageDraftRecoveryResult noRecovery =
        MapPackageDraftInstallService::recoverLatest(orphanRecoveryOptions);
    ok &= require(noRecovery.success && !noRecovery.found &&
                      std::filesystem::is_empty(orphan.assetLibraryDirectory),
                  "A crash before the persistent transaction marker should leave only safely removable staging debris");
    return ok;
}
}

int main()
{
    const std::filesystem::path directory =
        // Windows service TEMP can use a case or 8.3 alias. Materialization
        // returns canonical paths, so start these exact-path checks canonically.
        std::filesystem::canonical(std::filesystem::temp_directory_path()) /
        createEntityId("ttrpg_map_package_tests");
    std::filesystem::create_directories(directory);
    bool ok = true;
    try
    {
        ok &= testRoundTripAndPreview(directory);
        ok &= testUnicodePackagePathRoundTrip(directory);
        ok &= testLongPackagePathRoundTrip(directory);
        ok &= testCorruptionTruncationAndTransaction(directory);
        ok &= testOverwriteProtection(directory);
        ok &= testStandaloneIdRemap(directory);
        ok &= testIndependentCopyLineageImpact(directory);
        ok &= testOwnedIdentityConflictPreview(directory);
        ok &= testGeneratedRequirementIdentityAliases(directory);
        ok &= testSafeTransactionalMaterialization(directory);
        ok &= testImportedOriginalAndProxyPackage(directory);
        ok &= testManagedImageIntegrityGate(directory);
        ok &= testManagedPathResolutionOutsideWorkingDirectory(directory);
        ok &= testLegacyV2RecipeCompatibility(directory);
        ok &= testRecoverableDraftInstallTransaction(directory);
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Unexpected exception: " << exception.what() << std::endl;
        ok = false;
    }
    std::error_code cleanupError;
    std::filesystem::remove_all(mapFileIoPath(directory), cleanupError);
    ok &= require(!cleanupError, "Package test fixtures should be removed: " + cleanupError.message());
    if (!ok) return 1;
    std::cout << "Map package tests passed" << std::endl;
    return 0;
}
