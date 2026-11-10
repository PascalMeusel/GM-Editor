#pragma once

#include "Models/Map/MapModels.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

// A fog brush gesture is one map edit, regardless of how many cells the mouse
// crosses. The controller may render the in-memory changes immediately, but it
// persists only after the gesture ends. Keeping the complete original map also
// makes Escape and a failed save exact rollbacks rather than best-effort fog
// repairs that could lose authored cell state.
class MapViewerFogStroke
{
public:
    bool begin(const MapModel &map, bool fogged)
    {
        if (_before.has_value() || !validGrid(map) || map.metadata.id.empty() ||
            map.revision == (std::numeric_limits<std::uint64_t>::max)())
            return false;

        _before = map;
        _targetFogged = fogged;
        _changed = false;
        _changedCellCount = 0;
        return true;
    }

    bool paint(MapModel &map, int x, int y)
    {
        if (!_before.has_value() || !isCurrentTarget(map) ||
            x < 0 || y < 0 || x >= map.width || y >= map.height)
            return false;

        const std::size_t index = static_cast<std::size_t>(y) *
                                      static_cast<std::size_t>(map.width) +
                                  static_cast<std::size_t>(x);
        MapCellModel &cell = map.cells[index];
        if (cell.fogged == _targetFogged)
            return false;

        cell.fogged = _targetFogged;
        ++_changedCellCount;
        if (!_changed)
        {
            _changed = true;
            map.revision = _before->revision + 1;
            map.metadata.touch();
        }
        return true;
    }

    bool active() const { return _before.has_value(); }
    bool changed() const { return _changed; }
    std::size_t changedCellCount() const { return _changedCellCount; }
    const EntityId &mapId() const
    {
        static const EntityId empty;
        return _before.has_value() ? _before->metadata.id : empty;
    }

    bool readyToPersist(const MapModel &map) const
    {
        return _before.has_value() && isCurrentTarget(map) && _changed;
    }

    // Call only after persistence succeeds (or when a no-op gesture ends).
    void accept()
    {
        _before.reset();
        _targetFogged = false;
        _changed = false;
        _changedCellCount = 0;
    }

    bool rollback(MapModel &map)
    {
        if (!_before.has_value() || map.metadata.id != _before->metadata.id)
            return false;
        map = std::move(*_before);
        accept();
        return true;
    }

private:
    static bool validGrid(const MapModel &map)
    {
        if (map.width <= 0 || map.height <= 0)
            return false;
        const std::size_t width = static_cast<std::size_t>(map.width);
        const std::size_t height = static_cast<std::size_t>(map.height);
        if (width > (std::numeric_limits<std::size_t>::max)() / height)
            return false;
        return map.cells.size() == width * height;
    }

    bool isCurrentTarget(const MapModel &map) const
    {
        if (!_before.has_value() || map.metadata.id != _before->metadata.id ||
            map.width != _before->width || map.height != _before->height ||
            map.cells.size() != _before->cells.size())
            return false;
        const std::uint64_t expectedRevision =
            _before->revision + static_cast<std::uint64_t>(_changed ? 1 : 0);
        return map.revision == expectedRevision;
    }

    std::optional<MapModel> _before;
    bool _targetFogged = false;
    bool _changed = false;
    std::size_t _changedCellCount = 0;
};
