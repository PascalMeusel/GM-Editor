#pragma once
#include "Models/Map/MapModels.hpp"
#include <optional>

// The render plan and the renderer's terrain source must both remain stable
// during a brush stroke. Caching only the plan still invalidates the native
// ground/coast/color caches as live cells are painted. GM canvas use only;
// persistence, player review, export and brush overlays use the live draft.
class MapBrushRenderSnapshot
{
public:
    void begin(const MapModel &map) { _map=map; }
    void clear() { _map.reset(); }
    const MapModel &source(const MapModel &live) const
    {
        return _map && _map->metadata.id==live.metadata.id && _map->revision==live.revision &&
            _map->width==live.width && _map->height==live.height ? *_map : live;
    }
private:
    std::optional<MapModel> _map;
};
