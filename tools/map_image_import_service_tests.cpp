#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Media/RgbaImageLoader.hpp"

// Use the pinned, header-only production codec for a synthetic test image.
// No marketing artwork or other runtime fixture is required by the source ZIP.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "extern/glfw/deps/stb_image_write.h"
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_STATIC

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace
{
bool require(bool condition, const std::string &message)
{
    if (condition)
        return true;
    std::cerr << "FAILED: " << message << '\n';
    return false;
}

void writeOversizedPngHeader(const std::filesystem::path &path, std::uint32_t width, std::uint32_t height)
{
    std::vector<unsigned char> bytes{
        0x89u, 'P', 'N', 'G', 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 'I', 'H', 'D', 'R',
        static_cast<unsigned char>((width >> 24u) & 0xffu),
        static_cast<unsigned char>((width >> 16u) & 0xffu),
        static_cast<unsigned char>((width >> 8u) & 0xffu),
        static_cast<unsigned char>(width & 0xffu),
        static_cast<unsigned char>((height >> 24u) & 0xffu),
        static_cast<unsigned char>((height >> 16u) & 0xffu),
        static_cast<unsigned char>((height >> 8u) & 0xffu),
        static_cast<unsigned char>(height & 0xffu)};
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

bool copyJpegWithExifOrientation(const std::filesystem::path &source,
                                 const std::filesystem::path &destination,
                                 int orientation)
{
    std::ifstream input(source, std::ios::binary);
    std::vector<unsigned char> sourceBytes((std::istreambuf_iterator<char>(input)), {});
    if (sourceBytes.size() < 2 || sourceBytes[0] != 0xffu || sourceBytes[1] != 0xd8u ||
        orientation < 1 || orientation > 8)
        return false;
    std::vector<unsigned char> exif{
        0xff, 0xe1, 0x00, 0x22,
        'E', 'x', 'i', 'f', 0x00, 0x00,
        'I', 'I', 0x2a, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x01, 0x00,
        0x12, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00,
        static_cast<unsigned char>(orientation), 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00};
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char *>(sourceBytes.data()), 2);
    output.write(reinterpret_cast<const char *>(exif.data()), static_cast<std::streamsize>(exif.size()));
    output.write(reinterpret_cast<const char *>(sourceBytes.data() + 2),
                 static_cast<std::streamsize>(sourceBytes.size() - 2));
    return static_cast<bool>(output);
}

bool writeSyntheticJpeg(const std::filesystem::path &path, const LoadedRgbaImage &image)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    const auto write = [](void *context, void *data, int size) {
        static_cast<std::ofstream *>(context)->write(
            static_cast<const char *>(data), static_cast<std::streamsize>(size));
    };
    const bool encoded = stbi_write_jpg_to_func(
        write, &output, image.width, image.height, 4, image.pixels.data(), 95) != 0;
    output.flush();
    return encoded && static_cast<bool>(output);
}

Campaign campaignFixture()
{
    Campaign campaign;
    campaign.metadata.id = "map_image_import_test_campaign";
    campaign.name = "Map image import test";
    return campaign;
}
}

int main()
{
    bool ok = true;
    const std::filesystem::path root = std::filesystem::current_path() / "project";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root / "Resources");
#if defined(_WIN32)
    _putenv_s("TTRPG_DATA_ROOT", root.string().c_str());
#else
    setenv("TTRPG_DATA_ROOT", root.string().c_str(), 1);
#endif

    ok &= require(MapImageImport::isSupportedSource("map.PNG") &&
                      MapImageImport::isSupportedSource("map.jpg") &&
                      MapImageImport::isSupportedSource("map.JPEG") &&
                      !MapImageImport::isSupportedSource("map.webp"),
                  "Only the promised PNG/JPEG source extensions should enter the managed import path");
    ok &= require(std::string(MapImageImport::errorMessage("map.import.too_large")) ==
                      "The map image exceeds 16384 pixels on one edge or 100 megapixels." &&
                      std::string(MapImageImport::errorMessage("map.import.trace_degenerate")) ==
                      "The traced map feature does not form a valid line or area.",
                  "Stable import error codes must have understandable English UI messages");

    const MapNormalizedRect transformCrop{0.1f, 0.2f, 0.7f, 0.6f};
    const MapNormalizedRect transformViewport{0.2f, 0.125f, 0.55f, 0.75f};
    for (float rotation : {-180.0f, -90.0f, 0.0f, 90.0f, 180.0f})
    {
        const MapImageImport::NormalizedSamplingTransform fullTransform =
            MapImageImport::makeNormalizedSamplingTransform(transformCrop, rotation);
        const MapImageImport::NormalizedSamplingTransform viewportTransform =
            MapImageImport::makeNormalizedSamplingTransform(
                transformCrop, rotation, transformViewport);
        ok &= require(fullTransform.valid && viewportTransform.valid,
                      "Quarter-turn crop transforms must be valid");
        for (float localY : {0.125f, 0.5f, 0.875f})
            for (float localX : {0.125f, 0.5f, 0.875f})
            {
                float viewportU = 0.0f;
                float viewportV = 0.0f;
                float fullU = 0.0f;
                float fullV = 0.0f;
                const bool viewportInside = MapImageImport::sampleNormalizedImage(
                    viewportTransform, localX, localY, viewportU, viewportV);
                const bool fullInside = MapImageImport::sampleNormalizedImage(
                    fullTransform,
                    transformViewport.x + localX * transformViewport.width,
                    transformViewport.y + localY * transformViewport.height,
                    fullU, fullV);
                ok &= require(viewportInside == fullInside &&
                                  std::abs(viewportU - fullU) <= 0.000001f &&
                                  std::abs(viewportV - fullV) <= 0.000001f,
                              "A cropped/tiled viewport must sample the same oriented source coordinates as the full map");
            }
    }
    ok &= require(MapImageImport::canonicalRotationDegrees(89.99999f) == 90.0f &&
                      MapImageImport::canonicalRotationDegrees(-179.99998f) == -180.0f &&
                      !MapImageImport::makeNormalizedSamplingTransform(
                           transformCrop, 90.0f, {-0.1f, 0.0f, 0.5f, 1.0f}).valid,
                  "Quarter turns must be exact and invalid export viewports must fail closed");

    RgbaImageLoadOptions limits;
    limits.maximumSourceEdge = MapImageImport::maximumSourceEdge;
    limits.maximumSourcePixels = static_cast<std::uint64_t>(MapImageImport::maximumSourcePixels);
    LoadedRgbaImage rejected;
    std::string error;
    const std::filesystem::path overEdge = root / "over-edge.png";
    writeOversizedPngHeader(overEdge, 20000u, 1u);
    ok &= require(!loadRgbaImage(overEdge, rejected, limits, &error) && error == "image.too_large" &&
                      rejected.sourceWidth == 20000 && rejected.pixels.empty(),
                  "PNG edge limits must be enforced from the header before decoder/RGBA allocation");
    const std::filesystem::path overPixels = root / "over-pixels.png";
    writeOversizedPngHeader(overPixels, 10001u, 10000u);
    ok &= require(!loadRgbaImage(overPixels, rejected, limits, &error) && error == "image.too_large" &&
                      rejected.pixels.empty(),
                  "The 100 MP limit must be enforced independently of the edge limit");

    LoadedRgbaImage small;
    small.width = 4;
    small.height = 3;
    small.sourceWidth = small.width;
    small.sourceHeight = small.height;
    small.pixels.resize(4u * 3u * 4u, 255u);
    for (std::size_t offset = 0; offset < small.pixels.size(); offset += 4)
    {
        small.pixels[offset] = static_cast<unsigned char>((offset * 3u) & 0xffu);
        small.pixels[offset + 1] = static_cast<unsigned char>((offset * 7u) & 0xffu);
    }
    const std::filesystem::path pngPath = root / "source.png";
    ok &= require(saveRgbaImageAsPng(pngPath, small, &error), "Could not create PNG import fixture: " + error);

    Campaign campaign = campaignFixture();
    MapModel sentinel;
    sentinel.name = "unchanged";
    sentinel.revision = 5;
    MapImageImport::Request request;
    request.sourcePath = pngPath;
    request.intent = MapCreationIntent::ImportedReference;
    request.sizePreset = MapSizePreset::Small;
    request.spatialReference.calibrated = true;
    request.spatialReference.extentWidth = 10.0f;
    request.spatialReference.extentHeight = 8.0f;
    request.spatialReference.unit = MapDistanceUnit::Unspecified;
    const MapImageImport::Result invalidUnit = MapImageImport::importReference(campaign, sentinel, request);
    ok &= require(!invalidUnit.success && invalidUnit.errorCode == "map.import.invalid_unit" &&
                      sentinel.name == "unchanged" && sentinel.revision == 5 && !sentinel.importedReference.enabled,
                  "Invalid calibrated units must fail before files or map state are changed");

    request.spatialReference.unit = MapDistanceUnit::Kilometers;
    request.spatialReference.originX = std::numeric_limits<float>::quiet_NaN();
    const MapImageImport::Result invalidOrigin = MapImageImport::importReference(campaign, sentinel, request);
    ok &= require(!invalidOrigin.success && invalidOrigin.errorCode == "map.import.invalid_calibration" &&
                      !sentinel.importedReference.enabled,
                  "Non-finite spatial calibration must fail closed");

    request.spatialReference.originX = 0.0f;
    MapModel imported;
    const std::string sourceHash = ProjectAssetImport::computeContentHash(pngPath);
    const MapImageImport::Result first = MapImageImport::importReference(campaign, imported, request);
    ok &= require(first.success && imported.importedReference.enabled && imported.importedReference.gmOnly &&
                      imported.width == 48 && imported.height == 36 &&
                      imported.importedReference.assetId.find("map_reference_original_fnv1a64") == 0,
                  "A PNG should import atomically as a GM-only, content-addressed managed reference");
    ok &= require(!std::filesystem::path(imported.importedReference.projectRelativePath).is_absolute() &&
                      ProjectAssetImport::computeContentHash(
                          ProjectAssets::resolveProjectPath(imported.importedReference.projectRelativePath)) == sourceHash,
                  "Managed originals must use a project-relative path and retain their exact source bytes");

    MapModel duplicate;
    const MapImageImport::Result second = MapImageImport::importReference(campaign, duplicate, request);
    ok &= require(second.success && second.deduplicated &&
                      duplicate.importedReference.assetId == imported.importedReference.assetId &&
                      duplicate.importedReference.projectRelativePath == imported.importedReference.projectRelativePath,
                  "Equal source bytes should resolve to the same stable content-addressed managed asset");

    const MapImageImport::ManagedReferenceInspection initialInspection =
        MapImageImport::inspectManagedReference(imported.importedReference, true);
    ok &= require(initialInspection.healthy && initialInspection.originalAvailable &&
                      initialInspection.contentHashesVerified,
                  "A freshly imported managed reference should pass path, file, and hash inspection");

    MapModel unsafeReference = imported;
    unsafeReference.visibility = MapVisibility::PlayersVisible;
    unsafeReference.width = 1;
    unsafeReference.height = 1;
    unsafeReference.cells.resize(1);
    unsafeReference.importedReference.gmOnly = false;
    unsafeReference.importedReference.projectRelativePath = "../outside-project.png";
    MapRenderPlanRequest unsafeGmRequest;
    unsafeGmRequest.audience = MapRenderAudience::GameMaster;
    unsafeGmRequest.layers = MapRenderPlanBackground;
    MapRenderPlanRequest unsafePlayerRequest = unsafeGmRequest;
    unsafePlayerRequest.audience = MapRenderAudience::Player;
    ok &= require(!compileMapRenderPlan(unsafeReference, unsafeGmRequest).backgroundVisible &&
                      !compileMapRenderPlan(unsafeReference, unsafePlayerRequest).backgroundVisible &&
                      MapImageImport::inspectManagedReference(
                          unsafeReference.importedReference, false).errorCode ==
                          "map.import.reference_path_unsafe",
                  "A damaged managed path must fail closed before GM or player rendering can resolve it");

    const std::filesystem::path sourceJpeg = root / "synthetic-source.jpg";
    ok &= require(writeSyntheticJpeg(sourceJpeg, small), "Could not encode the synthetic JPEG fixture");
    const std::string originalJpegHash = ProjectAssetImport::computeContentHash(sourceJpeg);
    LoadedRgbaImage encodedJpeg;
    RgbaImageLoadOptions encodedJpegOptions;
    encodedJpegOptions.applyExifOrientation = false;
    ok &= require(loadRgbaImage(sourceJpeg, encodedJpeg, encodedJpegOptions, &error) &&
                      encodedJpeg.width == small.width && encodedJpeg.height == small.height &&
                      encodedJpeg.width != encodedJpeg.height && !originalJpegHash.empty(),
                  "Could not decode the non-square synthetic JPEG EXIF fixture source: " + error);
    for (int orientation = 1; orientation <= 8; ++orientation)
    {
        const std::filesystem::path exifJpeg =
            root / ("oriented-" + std::to_string(orientation) + ".jpg");
        ok &= require(copyJpegWithExifOrientation(sourceJpeg, exifJpeg, orientation),
                      "Could not prepare JPEG EXIF orientation " + std::to_string(orientation));
        MapModel jpegMap;
        request.sourcePath = exifJpeg;
        const std::string jpegHash = ProjectAssetImport::computeContentHash(exifJpeg);
        const MapImageImport::Result jpeg =
            MapImageImport::importReference(campaign, jpegMap, request);
        const int expectedWidth = orientation >= 5 ? encodedJpeg.height : encodedJpeg.width;
        const int expectedHeight = orientation >= 5 ? encodedJpeg.width : encodedJpeg.height;
        ok &= require(jpeg.success && jpeg.exifOrientation == orientation &&
                          jpeg.displayWidth == expectedWidth &&
                          jpeg.displayHeight == expectedHeight &&
                          jpegMap.importedReference.exifOrientation == orientation &&
                          ProjectAssetImport::computeContentHash(
                              ProjectAssets::resolveProjectPath(
                                  jpegMap.importedReference.projectRelativePath)) == jpegHash,
                      "JPEG import must normalize EXIF orientation " +
                          std::to_string(orientation) +
                          " while retaining immutable source bytes");
        std::filesystem::remove(exifJpeg, ignored);
    }
    ok &= require(ProjectAssetImport::computeContentHash(sourceJpeg) == originalJpegHash,
                  "All eight EXIF imports must preserve the original synthetic JPEG bytes");

    LoadedRgbaImage large;
    large.width = 8193;
    large.height = 2;
    large.sourceWidth = large.width;
    large.sourceHeight = large.height;
    large.pixels.resize(static_cast<std::size_t>(large.width * large.height * 4), 137u);
    const std::filesystem::path largePath = root / "large.png";
    ok &= require(saveRgbaImageAsPng(largePath, large, &error), "Could not prepare the proxy fixture: " + error);
    request.sourcePath = largePath;
    request.spatialReference = {};
    MapModel proxied;
    const MapImageImport::Result proxy = MapImageImport::importReference(campaign, proxied, request);
    ok &= require(proxy.success && proxy.workingProxyGenerated && proxy.displayWidth == 8192 && proxy.displayHeight == 2 &&
                      proxied.importedReference.workingProxyWidth == 8192 &&
                      proxied.importedReference.workingProxyHeight == 2 &&
                      proxied.importedReference.workingProxyAssetId.find("map_reference_proxy_fnv1a64") == 0,
                  "Sources above 8192 pixels must receive a bounded, separately hashed PNG working proxy");
    ok &= require(MapImageImport::inspectManagedReference(
                      proxied.importedReference, true).healthy,
                  "A generated proxy and its immutable original should pass integrity inspection");
    const std::filesystem::path managedProxy = ProjectAssets::resolveProjectPath(
        proxied.importedReference.workingProxyProjectRelativePath);
    std::filesystem::remove(managedProxy, ignored);
    const MapImageImport::ManagedReferenceInspection missingProxyInspection =
        MapImageImport::inspectManagedReference(proxied.importedReference, false);
    ok &= require(!missingProxyInspection.healthy &&
                      missingProxyInspection.originalAvailable &&
                      missingProxyInspection.errorCode == "map.import.proxy_missing" &&
                      proxied.importedReference.enabled,
                  "A missing working proxy must be diagnosed without disabling the recoverable original reference");

    const MapImportedReference beforeCalibration = imported.importedReference;
    const std::uint64_t beforeRevision = imported.revision;
    MapSpatialReference invalidSpatial = imported.spatialReference;
    invalidSpatial.unit = MapDistanceUnit::Unspecified;
    ok &= require(!MapImageImport::updateCalibration(imported, {0.1f, 0.1f, 0.8f, 0.8f}, 15.0f,
                                                    invalidSpatial, &error) &&
                      error == "map.import.invalid_unit" && imported.revision == beforeRevision &&
                      imported.importedReference.crop.x == beforeCalibration.crop.x,
                  "Rejected recalibration must not partially mutate crop, rotation, or revision");
    MapSpatialReference validSpatial = imported.spatialReference;
    validSpatial.unit = MapDistanceUnit::Miles;
    ok &= require(MapImageImport::updateCalibration(imported, {0.1f, 0.2f, 0.7f, 0.6f}, -22.5f,
                                                   validSpatial, &error) &&
                      imported.importedReference.crop.x == 0.1f &&
                      imported.importedReference.rotationDegrees == -22.5f &&
                      imported.revision == beforeRevision + 1,
                  "Valid crop, rotation, and spatial calibration should commit together");

    const std::size_t beforeTrace = imported.generation.graph.features.size();
    MapImageImport::TraceRequest trace;
    trace.kind = MapGeneratedFeatureKind::Landmark;
    trace.closed = true;
    trace.points = {{1.0f, 1.0f}};
    ok &= require(!MapImageImport::addTraceFeature(imported, trace, nullptr, &error) &&
                      error == "map.import.trace_too_short" &&
                      imported.generation.graph.features.size() == beforeTrace,
                  "A closed trace must contain a polygon even when its semantic kind normally uses a point");
    trace.closed = false;
    trace.kind = MapGeneratedFeatureKind::River;
    trace.points = {{1.0f, 1.0f}, {1.0f, 1.0f}};
    ok &= require(!MapImageImport::addTraceFeature(imported, trace, nullptr, &error) &&
                      error == "map.import.trace_degenerate" &&
                      imported.generation.graph.features.size() == beforeTrace,
                  "Zero-length line traces must fail without changing semantics");
    trace.kind = MapGeneratedFeatureKind::TerrainRegion;
    trace.points = {{1.0f, 1.0f}, {2.0f, 2.0f}, {3.0f, 3.0f}};
    ok &= require(!MapImageImport::addTraceFeature(imported, trace, nullptr, &error) &&
                      error == "map.import.trace_degenerate",
                  "Zero-area polygon traces must fail closed");
    trace.kind = MapGeneratedFeatureKind::River;
    trace.points.assign(MapImageImport::maximumTracePoints + 1u, {1.0f, 1.0f});
    ok &= require(!MapImageImport::addTraceFeature(imported, trace, nullptr, &error) &&
                      error == "map.import.trace_too_many_points",
                  "Unbounded trace payloads must be rejected");
    trace.kind = MapGeneratedFeatureKind::TerrainRegion;
    trace.name = "Traced coast area";
    trace.points = {{1.0f, 1.0f}, {8.0f, 1.0f}, {8.0f, 7.0f}, {1.0f, 7.0f}};
    ok &= require(MapImageImport::addTraceFeature(imported, trace, nullptr, &error) &&
                      imported.importedReference.semanticallyTraced &&
                      imported.generation.graph.features.size() == beforeTrace + 1u,
                  "A valid traced polygon should become persistent semantic graph data");

    // Repairing or intentionally replacing only the source image must not
    // regenerate the map or discard authored gameplay state.
    imported.metadata.id = "replace_reference_contract";
    imported.name = "Authored map";
    imported.atlasStatus = MapAtlasStatus::Linked;
    imported.parentMapId = "parent-map";
    imported.atlasPlacement.parentId = imported.parentMapId;
    imported.atlasPlacement.parentRevision = 42;
    imported.cells.resize(static_cast<std::size_t>(imported.width * imported.height));
    imported.cells.front().fogged = true;
    MapTokenModel preservedToken;
    preservedToken.metadata.id = "preserved-token";
    preservedToken.hiddenFromPlayers = true;
    imported.tokens.push_back(preservedToken);
    const std::vector<MapCellModel> cellsBeforeReplacement = imported.cells;
    const std::vector<MapGeneratedFeature> graphBeforeReplacement =
        imported.generation.graph.features;
    const std::vector<MapTokenModel> tokensBeforeReplacement = imported.tokens;
    const MapAtlasPlacement atlasBeforeReplacement = imported.atlasPlacement;
    const int widthBeforeReplacement = imported.width;
    const int heightBeforeReplacement = imported.height;
    const MapCreationIntent intentBeforeReplacement = imported.creationIntent;

    LoadedRgbaImage replacementImage = small;
    replacementImage.pixels.front() ^= 0x7fu;
    const std::filesystem::path replacementPath = root / "replacement.png";
    ok &= require(saveRgbaImageAsPng(replacementPath, replacementImage, &error),
                  "Could not prepare replacement image fixture: " + error);
    MapImageImport::Request replacementRequest = request;
    replacementRequest.sourcePath = replacementPath;
    replacementRequest.spatialReference = imported.spatialReference;
    replacementRequest.preserveExistingMapContract = true;
    const MapImageImport::Result replaced =
        MapImageImport::importReference(campaign, imported, replacementRequest);
    ok &= require(replaced.success && imported.importedReference.gmOnly &&
                      imported.importedReference.semanticallyTraced &&
                      imported.width == widthBeforeReplacement &&
                      imported.height == heightBeforeReplacement &&
                      imported.creationIntent == intentBeforeReplacement &&
                      imported.parentMapId == "parent-map" &&
                      imported.atlasPlacement.parentId == atlasBeforeReplacement.parentId &&
                      imported.atlasPlacement.parentRevision == atlasBeforeReplacement.parentRevision &&
                      imported.cells.size() == cellsBeforeReplacement.size() &&
                      imported.cells.front().fogged == cellsBeforeReplacement.front().fogged &&
                      imported.tokens.size() == tokensBeforeReplacement.size() &&
                      imported.tokens.front().metadata.id == tokensBeforeReplacement.front().metadata.id &&
                      imported.generation.graph.features.size() == graphBeforeReplacement.size() &&
                      imported.generation.graph.features.front().id == graphBeforeReplacement.front().id,
                  "Replacing a managed image must preserve map geometry, semantics, fog, tokens, and atlas lineage");

    const std::filesystem::path managedReplacement =
        ProjectAssets::resolveProjectPath(imported.importedReference.projectRelativePath);
    {
        std::ofstream changed(managedReplacement, std::ios::binary | std::ios::app);
        changed.put('\0');
    }
    const MapImageImport::ManagedReferenceInspection changedInspection =
        MapImageImport::inspectManagedReference(imported.importedReference, true);
    ok &= require(!changedInspection.healthy &&
                      changedInspection.errorCode == "map.import.original_changed" &&
                      imported.generation.graph.features.size() == graphBeforeReplacement.size() &&
                      imported.tokens.size() == tokensBeforeReplacement.size(),
                  "A changed managed image must be diagnosed without mutating traced or gameplay semantics");
    std::filesystem::remove(managedReplacement, ignored);
    const MapImageImport::ManagedReferenceInspection missingInspection =
        MapImageImport::inspectManagedReference(imported.importedReference, false);
    ok &= require(!missingInspection.healthy &&
                      missingInspection.errorCode == "map.import.original_missing" &&
                      imported.generation.graph.features.size() == graphBeforeReplacement.size() &&
                      imported.cells.size() == cellsBeforeReplacement.size(),
                  "A missing image must remain a repairable reference and never delete map semantics");

    MapModel legacy;
    legacy.scope = MapScope::Local;
    legacy.atlasLevel = MapLevel::Region;
    legacy.creationIntent = MapCreationIntent::RegionExploration;
    legacy.sizePreset = MapSizePreset::Custom;
    legacy.width = 11;
    legacy.height = 9;
    legacy.cells.resize(7);
    legacy.backgroundImagePath = pngPath.string();
    legacy.revision = 12;
    const MapImageImport::LegacyReferencePreview preview = MapImageImport::previewLegacyReference(legacy);
    ok &= require(preview.available && preview.contentHash == sourceHash &&
                      legacy.backgroundImagePath == pngPath.string() && !legacy.importedReference.enabled &&
                      legacy.revision == 12,
                  "Legacy image inspection must be a read-only preview");

    MapRenderPlanRequest gmRequest;
    gmRequest.audience = MapRenderAudience::GameMaster;
    gmRequest.layers = MapRenderPlanBackground;
    MapRenderPlanRequest playerRequest = gmRequest;
    playerRequest.audience = MapRenderAudience::Player;
    ok &= require(compileMapRenderPlan(legacy, gmRequest).backgroundVisible &&
                      !compileMapRenderPlan(legacy, playerRequest).backgroundVisible,
                  "Unmanaged legacy image paths must remain GM-visible but fail closed for players");

    const MapImageImport::Result adopted = MapImageImport::adoptLegacyReference(campaign, legacy);
    ok &= require(adopted.success && legacy.importedReference.enabled && legacy.importedReference.gmOnly &&
                      legacy.width == 11 && legacy.height == 9 && legacy.cells.size() == 7 &&
                      legacy.scope == MapScope::Local && legacy.atlasLevel == MapLevel::Region &&
                      legacy.creationIntent == MapCreationIntent::RegionExploration &&
                      legacy.sizePreset == MapSizePreset::Custom,
                  "Explicit legacy adoption must manage the image without changing map geometry or classification");
    ok &= require(!std::filesystem::path(legacy.backgroundImagePath).is_absolute(),
                  "Adoption must replace the external path with a managed project-relative path");

    const std::size_t tracedFeatures = imported.generation.graph.features.size();
    MapImageImport::removeReference(imported);
    ok &= require(!imported.importedReference.enabled && imported.backgroundImagePath.empty() &&
                      imported.generation.graph.features.size() == tracedFeatures,
                  "Removing a managed reference must retain all traced semantics");
    MapModel mixed;
    mixed.importedReference.enabled = true;
    mixed.importedReference.projectRelativePath = "managed/original.png";
    mixed.backgroundImagePath = "legacy/preserved.png";
    MapImageImport::removeReference(mixed);
    ok &= require(mixed.backgroundImagePath == "legacy/preserved.png",
                  "Removing a managed reference must not erase an unrelated legacy background path");

    std::filesystem::remove_all(root, ignored);
    return ok ? 0 : 1;
}
