#pragma once
#include "Models/Map/MapRenderPlan.hpp"
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

// Test/measurement artifact, deliberately independent of contentHash. Do not
// persist this format in campaigns or use it as publication authority.
namespace render_plan_test {
class SnapshotWriter {
    std::string bytes_;
public:
    void u64(std::uint64_t value) { for(int i=0;i<8;++i) bytes_.push_back(char((value>>(i*8))&255)); }
    void u32(std::uint32_t value) { for(int i=0;i<4;++i) bytes_.push_back(char((value>>(i*8))&255)); }
    void boolean(bool value) { bytes_.push_back(value?'\1':'\0'); }
    void byte(unsigned char value) { bytes_.push_back(char(value)); }
    void integer(int value) { static_assert(sizeof(int)==4);u32(static_cast<std::uint32_t>(value)); }
    template<class E> void enumeration(E value) { static_assert(std::is_enum_v<E>);integer(static_cast<int>(value)); }
    void floating(float value) {
        static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
        std::uint32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));u32(bits);
    }
    void string(const std::string &value) { u64(value.size());bytes_.append(value); }
    void strings(const std::vector<std::string> &values) { u64(values.size());for(const auto &v:values)string(v); }
    void point(const MapFeaturePoint &value) { floating(value.x);floating(value.y); }
    void color(const gm::map_render::OverviewRenderColor &value) {
        bytes_.push_back(char(value.r));bytes_.push_back(char(value.g));
        bytes_.push_back(char(value.b));bytes_.push_back(char(value.a));
    }
    void ink(const gm::map_render::AtlasInkPrimitive &value) {
        u64(value.points.size());for(const auto &p:value.points)point(p);
        color(value.color);floating(value.strokeCells);boolean(value.filled);
        floating(value.minimumCellPixels);floating(value.maximumCellPixels);
        u64(value.vertexColors.size());for(const auto &c:value.vertexColors)color(c);
    }
    void inks(const std::vector<gm::map_render::AtlasInkPrimitive> &values) { u64(values.size());for(const auto &v:values)ink(v); }
    void primitive(const MapRenderPrimitive &value) {
        enumeration(value.kind);u32(value.layer);string(value.sourceId);string(value.assetRole);
        string(value.assetId);string(value.label);boolean(value.closed);
        u64(value.points.size());for(const auto &p:value.points)point(p);inks(value.detailInk);
    }
    std::string take() { return std::move(bytes_); }
};
inline std::string snapshot(const MapRenderPlan &p) {
    SnapshotWriter w;w.string("GM_RENDER_PLAN_FULL_FIELDS_V1");
    w.string(p.mapId);w.u64(p.mapRevision);w.enumeration(p.level);w.enumeration(p.scope);
    w.enumeration(p.gridType);w.enumeration(p.mapVisibility);w.enumeration(p.audience);
    w.string(p.locale);w.string(p.styleId);w.string(p.styleSnapshotId);w.integer(p.styleVersion);
    w.u32(p.generationSeed);w.integer(p.generatorVersion);w.integer(p.recipeVersion);
    w.enumeration(p.terrainLandscape);w.boolean(p.continuousRendering);w.u32(p.layers);
    w.u32(p.playerReleaseLayers);w.boolean(p.applyFog);w.boolean(p.showPoliticalOverlay);
    w.boolean(p.showGrid);w.boolean(p.backgroundVisible);
    w.string(p.backgroundInteractiveAssetId);w.string(p.backgroundInteractiveProjectRelativePath);
    w.string(p.backgroundInteractiveContentHash);w.string(p.backgroundExportAssetId);
    w.string(p.backgroundExportProjectRelativePath);w.string(p.backgroundExportContentHash);
    w.strings(p.generatedFeatureIds);w.strings(p.manualFeatureIds);w.strings(p.regionIds);
    w.strings(p.routeIds);w.strings(p.tokenIds);
    w.u64(p.primitives.size());for(const auto &primitive:p.primitives)w.primitive(primitive);
    w.inks(p.atlasInk);w.inks(p.atlasWaterInk);w.inks(p.atlasFeatureInk);
    w.integer(p.labelInkMask.width);w.integer(p.labelInkMask.height);w.floating(p.labelInkMask.samplesPerCell);
    w.u64(p.labelInkMask.covered.size());for(auto value:p.labelInkMask.covered)w.byte(value);
    w.u64(p.contentHash);return w.take();
}
} // namespace render_plan_test
