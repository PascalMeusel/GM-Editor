#pragma once

#include "Models/Map/MapRenderPlan.hpp"
#include <functional>
#include <memory>
#include <string>

struct MapPreviewCompilation
{
    // A previous preview is useful while refreshing, but MUST be drawn with
    // this immutable source, never with the current mutable editor draft.
    std::shared_ptr<const MapModel> source;
    std::shared_ptr<const MapRenderPlan> plan;
    bool current = false;
    bool refreshing = false;
    std::string error;
    double compilationMilliseconds = 0;
    std::uint64_t discardedResults = 0;
};

// GM authoring only. One bounded CPU worker, one latest pending request; no
// graphics calls, campaign references, or destructor waits during an edit.
// Authoritative player review and export continue to compile current content.
class MapPreviewCompiler
{
  public:
    using Compiler = std::function<MapRenderPlan(const MapModel &, const MapRenderPlanRequest &)>;
    explicit MapPreviewCompiler(Compiler compiler = compileMapRenderPlan);
    ~MapPreviewCompiler();
    MapPreviewCompiler(const MapPreviewCompiler &) = delete;
    MapPreviewCompiler &operator=(const MapPreviewCompiler &) = delete;

    void request(const MapModel &map, const MapRenderPlanRequest &request, std::uint64_t authoringSerial);
    MapPreviewCompilation state() const;
    void retry();
    void clear();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
