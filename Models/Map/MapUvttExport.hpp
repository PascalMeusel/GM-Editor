#pragma once
#include "Models/Map/MapPngExport.hpp"
#include <array>

struct MapUvttGeometry
{
    // Coordinates use half-cell integers internally, grid squares in UVTT.
    std::vector<std::array<int, 4>> walls;
    std::vector<std::array<int, 4>> doors;
    std::size_t omittedDirectionalCells = 0;
    std::size_t omittedMovementOnlyCells = 0;
};

bool mapUvttExportSupported(const MapModel &map, int pixelsPerCell, std::string *error = nullptr);
MapUvttGeometry mapUvttGeometry(const MapModel &map);

// Full-map GM transfer, never a player publication: excludes text labels,
// tokens/notes and fog state. Only tactical square maps are supported.
// Writes a single self-contained UVTT 0.2 file using atomic replacement.
bool exportMapUvtt(const MapModel &map, const std::filesystem::path &destination,
                   int pixelsPerCell, bool overwriteExisting, std::string *error = nullptr);
