#include "Models/Map/MapRenderCompileMemoInternal.hpp"
#include "Models/Map/MapRenderCompileMemoAmbient.hpp"

namespace gm::map_render
{
MapRenderCompileKey encodeMapRenderCompileKey(
    const MapModel &map, const MapRenderPlanRequest &request,
    const MapRenderPlan &plan, std::size_t maximumBytes)
{
    using namespace MapPackageServiceInternal;
    // nullptr preserves exact float bits, including signed zero/NaN payloads;
    // this in-memory key is not a validated/importable map package.
    Writer writer(nullptr, maximumBytes);
    writer.u32(1); // Key contract version, scoped to this process/build.
    detail::encodeCompileAmbientState(writer);
    writeMap(writer, map);
    // The package codec deliberately maps unknown landform ordinals to the
    // automatic token. The compiler can observe the raw enum, so retain it.
    writeEnum(writer, map.generation.recipe.landform);

    writeEnum(writer, request.audience); writer.string(request.locale);
    writer.u32(request.layers); writer.boolean(request.applyFog);
    writer.boolean(request.showPoliticalOverlay); writer.boolean(request.showGrid);
    writer.boolean(static_cast<bool>(request.additionalGeneratedFeatureVisibility));

    // Encode every pre-geometry plan field, not only values currently consumed
    // by one renderer. Callback identity is deliberately irrelevant: its exact
    // selected results were just recomputed by the ordinary selection loop.
    writer.string(plan.mapId); writer.u64(plan.mapRevision);
    writeEnum(writer, plan.level); writeEnum(writer, plan.scope);
    writeEnum(writer, plan.gridType); writeEnum(writer, plan.mapVisibility);
    writeEnum(writer, plan.audience); writer.string(plan.locale);
    writer.string(plan.styleId); writer.string(plan.styleSnapshotId);
    writer.i32(plan.styleVersion); writer.u32(plan.generationSeed);
    writer.i32(plan.generatorVersion); writer.i32(plan.recipeVersion);
    writeEnum(writer, plan.terrainLandscape); writer.boolean(plan.continuousRendering);
    writer.u32(plan.layers); writer.u32(plan.playerReleaseLayers);
    writer.boolean(plan.applyFog); writer.boolean(plan.showPoliticalOverlay);
    writer.boolean(plan.showGrid); writer.boolean(plan.backgroundVisible);
    writer.string(plan.backgroundInteractiveAssetId);
    writer.string(plan.backgroundInteractiveProjectRelativePath);
    writer.string(plan.backgroundInteractiveContentHash);
    writer.string(plan.backgroundExportAssetId);
    writer.string(plan.backgroundExportProjectRelativePath);
    writer.string(plan.backgroundExportContentHash);
    writeStrings(writer, plan.generatedFeatureIds);
    writeStrings(writer, plan.manualFeatureIds);
    writeStrings(writer, plan.regionIds);
    writeStrings(writer, plan.routeIds);
    writeStrings(writer, plan.tokenIds);
    return writer.take();
}
} // namespace gm::map_render
