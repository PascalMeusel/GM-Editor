#pragma once

#include "Models/Map/MapOverviewPoliticalProjection.hpp"

namespace gm::map_render::detail
{
// Borrowed only during one synchronous water build from an immutable, already
// audience-filtered map. Neither the map nor projection may change in this scope.
// Original entry points do not retain or use this context across invocations.
class OverviewGroundSamplingCache
{
public:
    OverviewGroundSamplingCache(const MapModel &map,
                                const std::vector<MapTileKind> &ground)
        : _map(&map), _ground(ground), _width(map.width), _height(map.height)
    {
        if (_width <= 0 || _height <= 0 ||
            ground.size() != static_cast<std::size_t>(_width) * _height)
            return;
        _thin.resize(ground.size());
        for (int y = 0; y < _height; ++y)
            for (int x = 0; x < _width; ++x)
                _thin[static_cast<std::size_t>(mapCellIndex(map, x, y))] =
                    overviewHasThinTerrainCore(map, x, y) ? 1u : 0u;
    }

    OverviewGroundSamplingCache(const OverviewGroundSamplingCache &) = delete;
    OverviewGroundSamplingCache &operator=(const OverviewGroundSamplingCache &) = delete;

    bool matches(const MapModel &map) const
    {
        return _map == &map && _width == map.width && _height == map.height &&
               _width > 0 && _height > 0 &&
               _ground.size() == static_cast<std::size_t>(_width) * _height &&
               _thin.size() == _ground.size();
    }

    const std::vector<MapTileKind> *projection(const MapModel &map) const
    {
        return matches(map) ? &_ground : nullptr;
    }

    bool thinCore(const MapModel &map, int x, int y) const
    {
        if (matches(map) && isMapCellInside(map, x, y))
            return _thin[static_cast<std::size_t>(mapCellIndex(map, x, y))] != 0u;
        return overviewHasThinTerrainCore(map, x, y);
    }

    MapTileKind ground(const MapModel &map, int x, int y) const
    {
        if (matches(map))
            return overviewGroundKindFromProjection(map, _ground, x, y);
        return overviewGroundKindForRender(map, x, y);
    }

private:
    const MapModel *_map;
    const std::vector<MapTileKind> &_ground;
    int _width;
    int _height;
    std::vector<std::uint8_t> _thin;
};
} // namespace gm::map_render::detail
