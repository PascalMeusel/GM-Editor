#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapPngExportPlanning.hpp"
#include "Models/Map/PngExport/PngRenderSession.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

using namespace MapPngExportRaster;

namespace
{
std::string pngJsonEscape(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (character < 0x20u)
            {
                std::ostringstream encoded;
                encoded << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(character);
                escaped += encoded.str();
            }
            else
                escaped.push_back(static_cast<char>(character));
            break;
        }
    }
    return escaped;
}

bool pngManagedTileFileName(std::string_view fileStem, std::string_view fileName)
{
    const std::string prefix = std::string(fileStem) + "_r";
    constexpr std::string_view columnMarker = "_c";
    constexpr std::string_view suffix = ".png";
    if (fileName.size() <= prefix.size() + columnMarker.size() + suffix.size() ||
        fileName.substr(0, prefix.size()) != prefix ||
        fileName.substr(fileName.size() - suffix.size()) != suffix)
        return false;
    const std::size_t column = fileName.find(columnMarker, prefix.size());
    if (column == std::string_view::npos || column == prefix.size() ||
        column + columnMarker.size() >= fileName.size() - suffix.size())
        return false;
    const auto isDigits = [](std::string_view value) {
        return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return std::isdigit(character) != 0;
        });
    };
    return isDigits(fileName.substr(prefix.size(), column - prefix.size())) &&
           isDigits(fileName.substr(column + columnMarker.size(),
                                    fileName.size() - suffix.size() - column - columnMarker.size()));
}
}


static bool exportMapPngRasterized(const MapModel &map,
                                   const std::filesystem::path &path,
                                   const MapRenderPlan &plan,
                                   int requestedPixelsPerCell,
                                   std::string *error,
                                   float mapOffsetX = 0.0f,
                                   float mapOffsetY = 0.0f,
                                   MapNormalizedRect mapViewport = MapNormalizedRect{},
                                   const MapModel *fullTerrainSource = nullptr)
{
    return PngRenderSession(map, path, plan, requestedPixelsPerCell,
                            error, mapOffsetX, mapOffsetY, mapViewport,
                            fullTerrainSource).run();
}

bool exportMapPngTiled(const MapModel &sourceMap,
                       const std::filesystem::path &directory,
                       const std::string &fileStem,
                       const MapRenderPlan &renderPlan,
                       int pixelsPerCell,
                       bool overwriteExisting,
                       MapPngTiledExportPlan *writtenPlan,
                       std::string *error)
{
    // Reject the whole-map audience before even creating a staging directory;
    // per-object filtering cannot authorize terrain from a private map.
    if (!mapPermitsPngExport(sourceMap, renderPlan.audience, error))
        return false;
    if (!mapRenderPlanContentMatchesMap(renderPlan, sourceMap))
    {
        if (error)
            *error = "The map changed after the render preview. Create a new preview.";
        return false;
    }
    const MapModel map = filterMapForRenderPlan(sourceMap, renderPlan);
    const MapPngTiledExportPlan plan = planMapPngTiledExport(
        map, pixelsPerCell, fileStem);
    if (!plan.valid)
    {
        if (error)
            *error = plan.error;
        return false;
    }
    if (plan.tiles.size() > 1 && !map.backgroundImagePath.empty() &&
        !map.importedReference.enabled)
    {
        if (error)
            *error = "A legacy image background must be adopted as a managed map image before tiled export.";
        return false;
    }
    try
    {
        std::error_code filesystemError;
        if (!std::filesystem::exists(directory, filesystemError))
            std::filesystem::create_directories(directory);
        if (filesystemError || !std::filesystem::is_directory(directory, filesystemError))
        {
            if (error)
                *error = "The tiled PNG destination directory is unavailable.";
            return false;
        }

        const std::filesystem::path manifestName = fileStem + "_tiles.json";
        std::vector<std::filesystem::path> outputNames;
        outputNames.reserve(plan.tiles.size() + 1);
        for (const MapPngExportTile &tile : plan.tiles)
            outputNames.push_back(tile.fileName);
        outputNames.push_back(manifestName);

        // Treat every file emitted by an earlier export with this stem as one
        // managed output set. A changed map size can reduce the tile count;
        // collecting the old set here prevents stale tail tiles from surviving
        // an explicitly confirmed replacement.
        std::vector<std::filesystem::path> existingOutputNames;
        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, filesystemError))
        {
            if (filesystemError)
                break;
            if (!entry.is_regular_file(filesystemError) || filesystemError)
                continue;
            const std::string name = entry.path().filename().string();
            if (name == manifestName.string() || pngManagedTileFileName(fileStem, name))
                existingOutputNames.emplace_back(name);
        }
        if (filesystemError)
        {
            if (error)
                *error = "The tiled PNG destination could not be inspected.";
            return false;
        }
        std::sort(existingOutputNames.begin(), existingOutputNames.end());
        const bool hasExistingOutput = !existingOutputNames.empty();
        if (hasExistingOutput && !overwriteExisting)
        {
            if (error)
                *error = "A tiled PNG output file already exists. Confirm replacement before exporting.";
            return false;
        }

        const std::filesystem::path staging = directory / ("." + fileStem + "_tiles_staging");
        std::filesystem::path backup = directory / (fileStem + "_previous_tiles");
        if (hasExistingOutput)
        {
            // Keep prior exports recoverable without making the next confirmed
            // export impossible. Subsequent backups receive a stable numeric
            // suffix and are never silently deleted.
            for (int suffix = 2; std::filesystem::exists(backup, filesystemError) && !filesystemError; ++suffix)
                backup = directory / (fileStem + "_previous_tiles_" + std::to_string(suffix));
        }
        if (std::filesystem::exists(staging, filesystemError))
        {
            if (error)
                *error = "A previous tiled PNG staging directory still exists.";
            return false;
        }
        std::filesystem::create_directory(staging);
        if (filesystemError)
        {
            if (error)
                *error = "The tiled PNG staging directory could not be created.";
            return false;
        }

        std::vector<std::filesystem::path> stagedNames;
        auto cleanStaging = [&]() {
            std::error_code cleanupError;
            for (const std::filesystem::path &name : stagedNames)
                std::filesystem::remove(staging / name, cleanupError);
            std::filesystem::remove(staging / manifestName, cleanupError);
            std::filesystem::remove(staging, cleanupError);
        };

        for (const MapPngExportTile &tile : plan.tiles)
        {
            if (!pngRectIsValidForMap(tile.render, map))
            {
                cleanStaging();
                if (error)
                    *error = "The tiled PNG plan contains an invalid source rectangle.";
                return false;
            }
            const MapModel cropped = cropMapForPngExport(map, tile.render);
            const MapNormalizedRect mapViewport =
                pngNormalizedViewport(map, tile.render);
            std::string tileError;
            if (!exportMapPngRasterized(cropped, staging / tile.fileName,
                                        renderPlan, pixelsPerCell, &tileError,
                                        static_cast<float>(tile.render.x),
                                        static_cast<float>(tile.render.y),
                                        mapViewport, &map))
            {
                cleanStaging();
                if (error)
                    *error = tileError.empty() ? "A tiled PNG image could not be rendered." : tileError;
                return false;
            }
            stagedNames.push_back(tile.fileName);
        }

        {
            std::ofstream manifest(staging / manifestName, std::ios::binary | std::ios::trunc);
            if (!manifest)
            {
                cleanStaging();
                if (error)
                    *error = "The tiled PNG manifest could not be written.";
                return false;
            }
            manifest << "{\n"
                     << "  \"schemaVersion\": 2,\n"
                     << "  \"mapId\": \"" << pngJsonEscape(map.metadata.id) << "\",\n"
                     << "  \"mapRevision\": " << map.revision << ",\n"
                     << "  \"styleSnapshotId\": \""
                     << pngJsonEscape(map.generation.recipe.styleSnapshotId) << "\",\n"
                     << "  \"audience\": \""
                     << (renderPlan.audience == MapRenderAudience::Player ? "player" : "gm")
                     << "\",\n"
                     << "  \"locale\": \"" << pngJsonEscape(renderPlan.locale) << "\",\n"
                     << "  \"renderPlanContentHash\": \"" << std::hex
                     << std::setw(16) << std::setfill('0') << renderPlan.contentHash
                     << std::dec << "\",\n"
                     << "  \"renderPlanLayers\": " << renderPlan.layers << ",\n"
                     << "  \"pixelsPerCell\": " << plan.pixelsPerCell << ",\n"
                     << "  \"fullWidthPixels\": " << plan.fullWidthPixels << ",\n"
                     << "  \"fullHeightPixels\": " << plan.fullHeightPixels << ",\n"
                     << "  \"columns\": " << plan.columns << ",\n"
                     << "  \"rows\": " << plan.rows << ",\n"
                     << "  \"overlapCells\": " << plan.overlapCells << ",\n"
                     << "  \"tiles\": [\n";
            for (std::size_t index = 0; index < plan.tiles.size(); ++index)
            {
                const MapPngExportTile &tile = plan.tiles[index];
                manifest << "    {\"file\": \"" << pngJsonEscape(tile.fileName.generic_string())
                         << "\", \"column\": " << tile.column << ", \"row\": " << tile.row
                         << ", \"content\": [" << tile.content.x << ", " << tile.content.y
                         << ", " << tile.content.width << ", " << tile.content.height
                         << "], \"render\": [" << tile.render.x << ", " << tile.render.y
                         << ", " << tile.render.width << ", " << tile.render.height
                         << "], \"pixelSize\": [" << tile.widthPixels << ", "
                         << tile.heightPixels << "]}" << (index + 1 == plan.tiles.size() ? "\n" : ",\n");
            }
            manifest << "  ]\n}\n";
            if (!manifest)
            {
                manifest.close();
                cleanStaging();
                if (error)
                    *error = "The tiled PNG manifest could not be completed.";
                return false;
            }
        }

        std::vector<std::filesystem::path> backedUp;
        if (hasExistingOutput)
        {
            std::filesystem::create_directory(backup);
            if (filesystemError)
            {
                cleanStaging();
                if (error)
                    *error = "The previous tiled PNG export could not be preserved.";
                return false;
            }
            for (const std::filesystem::path &name : existingOutputNames)
            {
                const std::filesystem::path current = directory / name;
                if (!std::filesystem::exists(current, filesystemError))
                    continue;
                std::filesystem::rename(current, backup / name, filesystemError);
                if (filesystemError)
                {
                    for (auto restored = backedUp.rbegin(); restored != backedUp.rend(); ++restored)
                    {
                        std::error_code rollbackError;
                        std::filesystem::rename(backup / *restored, directory / *restored, rollbackError);
                    }
                    std::error_code cleanupError;
                    std::filesystem::remove(backup, cleanupError);
                    cleanStaging();
                    if (error)
                        *error = "The previous tiled PNG export could not be backed up safely.";
                    return false;
                }
                backedUp.push_back(name);
            }
        }

        std::vector<std::filesystem::path> published;
        for (const std::filesystem::path &name : outputNames)
        {
            std::filesystem::rename(staging / name, directory / name, filesystemError);
            if (!filesystemError)
            {
                published.push_back(name);
                continue;
            }

            for (auto moved = published.rbegin(); moved != published.rend(); ++moved)
            {
                std::error_code rollbackError;
                std::filesystem::rename(directory / *moved, staging / *moved, rollbackError);
            }
            for (auto restored = backedUp.rbegin(); restored != backedUp.rend(); ++restored)
            {
                std::error_code rollbackError;
                std::filesystem::rename(backup / *restored, directory / *restored, rollbackError);
            }
            std::error_code cleanupError;
            std::filesystem::remove(backup, cleanupError);
            cleanStaging();
            if (error)
                *error = "The tiled PNG export could not be published atomically.";
            return false;
        }
        std::filesystem::remove(staging, filesystemError);
        if (writtenPlan)
            *writtenPlan = plan;
        return true;
    }
    catch (const std::exception &exception)
    {
        if (error)
            *error = exception.what();
        return false;
    }
}

bool exportMapPngTiled(const MapModel &map,
                       const std::filesystem::path &directory,
                       const std::string &fileStem,
                       const MapPngExportOptions &options,
                       bool overwriteExisting,
                       MapPngTiledExportPlan *writtenPlan,
                       std::string *error)
{
    const MapRenderPlan plan = compileMapRenderPlan(
        map, mapPngExportRenderPlanRequest(options));
    return exportMapPngTiled(map, directory, fileStem, plan,
                             options.pixelsPerCell, overwriteExisting,
                             writtenPlan, error);
}
