#include "PersistenceSerialization.hpp"

#include <algorithm>
#include <charconv>
#include <exception>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace PersistenceSerialization
{
std::string serializeMapCells(const std::vector<MapCellModel> &cells)
{
    std::string serialized;
    if (cells.size() <= serialized.max_size() / 32)
        serialized.reserve(cells.size() * 32);
    for (const MapCellModel &cell : cells)
    {
        const int fields[] = {
            static_cast<int>(cell.kind), normalizeMapRotation(cell.rotationQuarterTurns),
            std::max(1, cell.movementCost), cell.difficultTerrain ? 1 : 0,
            cell.blocksLineOfSight ? 1 : 0, static_cast<int>(cell.lighting),
            cell.fogged ? 1 : 0, cell.elevation, normalizeMapEdgeMask(cell.movementBlockedMask),
            normalizeMapEdgeMask(cell.lineOfSightBlockedMask), static_cast<int>(cell.symbolVariantMode),
            static_cast<int>(cell.wayKind), cell.rotationLocked ? 1 : 0,
            normalizeMapEdgeMask(cell.movementOneWayBlockedMask), normalizeMapEdgeMask(cell.lineOfSightOneWayBlockedMask)};
        for (std::size_t index = 0; index < sizeof(fields) / sizeof(fields[0]); ++index)
        {
            char buffer[std::numeric_limits<int>::digits10 + 3];
            const auto encoded = std::to_chars(buffer, buffer + sizeof(buffer), fields[index]);
            serialized.append(buffer, encoded.ptr);
            serialized.push_back(index == 14 ? ',' : ':');
        }
    }
    return serialized;
}

std::vector<MapCellModel> deserializeMapCells(const std::string &serializedCells)
{
    std::vector<MapCellModel> cells;
    std::stringstream stream(serializedCells);
    std::string value;
    while (std::getline(stream, value, ','))
    {
        if (value.empty())
            continue;

        MapCellModel cell;
        const size_t delimiter = value.find(':');
        try
        {
            if (delimiter == std::string::npos)
            {
                applyMapCellTerrainDefaults(cell, static_cast<MapTileKind>(std::stoi(value)));
            }
            else
            {
                std::stringstream fieldStream(value);
                std::vector<std::string> fields;
                std::string field;
                while (std::getline(fieldStream, field, ':'))
                    fields.push_back(field);

                if (!fields.empty())
                    applyMapCellTerrainDefaults(cell, static_cast<MapTileKind>(std::stoi(fields[0])));
                if (fields.size() > 1)
                    cell.rotationQuarterTurns = normalizeMapRotation(std::stoi(fields[1]));
                if (fields.size() > 2)
                    cell.movementCost = std::max(1, std::stoi(fields[2]));
                if (fields.size() > 3)
                    cell.difficultTerrain = fields[3] == "1";
                if (fields.size() > 4)
                    cell.blocksLineOfSight = fields[4] == "1";
                if (fields.size() > 5)
                    cell.lighting = static_cast<MapLightingMode>(std::clamp(std::stoi(fields[5]), 0, static_cast<int>(MapLightingMode::Darkness)));
                if (fields.size() > 6)
                    cell.fogged = fields[6] == "1";
                if (fields.size() > 7)
                    cell.elevation = std::clamp(std::stoi(fields[7]), -20, 20);
                if (fields.size() > 8)
                    cell.movementBlockedMask = normalizeMapEdgeMask(std::stoi(fields[8]));
                if (fields.size() > 9)
                    cell.lineOfSightBlockedMask = normalizeMapEdgeMask(std::stoi(fields[9]));
                if (fields.size() > 10)
                    cell.symbolVariantMode = normalizeMapSymbolVariantMode(std::stoi(fields[10]));
                if (fields.size() > 11)
                    cell.wayKind = normalizeMapWayKind(std::stoi(fields[11]));
                if (fields.size() > 12)
                    cell.rotationLocked = fields[12] == "1";
                else
                    cell.rotationLocked = normalizeMapRotation(cell.rotationQuarterTurns) != 0;
                if (fields.size() > 13)
                    cell.movementOneWayBlockedMask = normalizeMapEdgeMask(std::stoi(fields[13]));
                if (fields.size() > 14)
                    cell.lineOfSightOneWayBlockedMask = normalizeMapEdgeMask(std::stoi(fields[14]));
            }
        }
        catch (const std::exception &)
        {
            cell.kind = MapTileKind::Empty;
            cell.rotationQuarterTurns = 0;
        }
        cells.push_back(cell);
    }
    return cells;
}

std::string serializeMapRegions(const std::vector<MapRegionModel> &regions)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::stringstream stream;
    for (const MapRegionModel &region : regions)
    {
        stream << region.metadata.id << fieldSeparator
               << region.name << fieldSeparator
               << region.description << fieldSeparator
               << region.x << fieldSeparator
               << region.y << fieldSeparator
               << region.width << fieldSeparator
               << region.height << fieldSeparator
               << region.movementCost << fieldSeparator
               << static_cast<int>(region.lighting) << fieldSeparator
               << (region.difficultTerrain ? 1 : 0) << fieldSeparator
               << (region.blocksLineOfSight ? 1 : 0) << fieldSeparator
               << (region.gmOnly ? 1 : 0) << fieldSeparator
               << region.metadata.createdAtUtc << fieldSeparator
               << region.metadata.updatedAtUtc << entrySeparator;
    }
    return stream.str();
}

std::vector<MapRegionModel> deserializeMapRegions(const std::string &serializedRegions)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::vector<MapRegionModel> regions;
    std::stringstream entryStream(serializedRegions);
    std::string value;
    while (std::getline(entryStream, value, entrySeparator))
    {
        if (value.empty())
            continue;

        std::stringstream fieldStream(value);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(fieldStream, field, fieldSeparator))
            fields.push_back(field);

        MapRegionModel region;
        if (!fields.empty() && !fields[0].empty())
            region.metadata.id = fields[0];
        if (fields.size() > 1)
            region.name = fields[1];
        if (fields.size() > 2)
            region.description = fields[2];
        try
        {
            if (fields.size() > 3)
                region.x = std::stoi(fields[3]);
            if (fields.size() > 4)
                region.y = std::stoi(fields[4]);
            if (fields.size() > 5)
                region.width = std::max(1, std::stoi(fields[5]));
            if (fields.size() > 6)
                region.height = std::max(1, std::stoi(fields[6]));
            if (fields.size() > 7)
                region.movementCost = std::max(1, std::stoi(fields[7]));
            if (fields.size() > 8)
                region.lighting = static_cast<MapLightingMode>(std::clamp(std::stoi(fields[8]), 0, static_cast<int>(MapLightingMode::Darkness)));
        }
        catch (const std::exception &)
        {
            region.width = 1;
            region.height = 1;
            region.movementCost = 1;
        }
        if (fields.size() > 9)
            region.difficultTerrain = fields[9] == "1";
        if (fields.size() > 10)
            region.blocksLineOfSight = fields[10] == "1";
        if (fields.size() > 11)
            region.gmOnly = fields[11] == "1";
        if (fields.size() > 12 && !fields[12].empty())
            region.metadata.createdAtUtc = fields[12];
        if (fields.size() > 13 && !fields[13].empty())
            region.metadata.updatedAtUtc = fields[13];
        if (!region.name.empty())
            regions.push_back(region);
    }
    return regions;
}
}
