#include "Models/Map/MapUvttExport.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include <cmath>
#include <fstream>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace
{
std::string base64(const std::string &bytes)
{
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t index = 0; index < bytes.size(); index += 3)
    {
        const auto a = static_cast<unsigned char>(bytes[index]);
        const auto b = index + 1 < bytes.size() ? static_cast<unsigned char>(bytes[index + 1]) : 0;
        const auto c = index + 2 < bytes.size() ? static_cast<unsigned char>(bytes[index + 2]) : 0;
        output += alphabet[a >> 2];
        output += alphabet[((a & 3) << 4) | (b >> 4)];
        output += index + 1 < bytes.size() ? alphabet[((b & 15) << 2) | (c >> 6)] : '=';
        output += index + 2 < bytes.size() ? alphabet[c & 63] : '=';
    }
    return output;
}
void point(std::ostream &output, int x, int y)
{
    output << "{\"x\":" << x / 2.0 << ",\"y\":" << y / 2.0 << '}';
}
std::string payload(const MapModel &map, int pixelsPerCell, const MapUvttGeometry &geometry, const std::string &image)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "{\"format\":0.2,\"resolution\":{\"map_origin\":{\"x\":0,\"y\":0},\"map_size\":{\"x\":"
           << map.width << ",\"y\":" << map.height << "},\"pixels_per_grid\":" << pixelsPerCell
           << "},\"line_of_sight\":[";
    bool first = true;
    for (const auto &wall : geometry.walls)
    {
        if (!first) output << ',';
        first = false;
        output << '['; point(output, wall[0], wall[1]); output << ',';
        point(output, wall[2], wall[3]); output << ']';
    }
    output << "],\"objects_line_of_sight\":[],\"portals\":[";
    first = true;
    for (const auto &door : geometry.doors)
    {
        if (!first) output << ',';
        first = false;
        output << "{\"position\":{\"x\":" << (door[0] + door[2]) / 4.0
               << ",\"y\":" << (door[1] + door[3]) / 4.0 << "},\"bounds\":[";
        point(output, door[0], door[1]); output << ','; point(output, door[2], door[3]);
        // closed=false means a light-permitting window to common importers,
        // not a preserved gameplay open state. Our door tiles have no state.
        output << "],\"rotation\":" << (door[0] == door[2] ? 1.5707963267948966 : 0.0)
               << ",\"closed\":true,\"freestanding\":false}";
    }
    output << "],\"environment\":{\"baked_lighting\":false,\"ambient_light\":\"ffffffff\"},\"lights\":[],"
           << "\"creator\":\"GM Editor\",\"gm_editor_export\":{\"audience\":\"gm_transfer\","
           << "\"omissions\":[\"tokens\",\"labels and notes\",\"fog state\",\"dynamic lights\","
           << "\"elevation and movement costs\",\"one-way and movement-only blockers\","
           << "\"region blockers\",\"door marker features and edge-preset door identity\",\"door open/locked state\"],"
           << "\"directional_cells_omitted\":" << geometry.omittedDirectionalCells
           << ",\"movement_only_cells_omitted\":" << geometry.omittedMovementOnlyCells
           << "},\"image\":\"" << image << "\"}";
    return output.str();
}
struct Scratch
{
    std::filesystem::path directory;
    ~Scratch() { if (!directory.empty()) { std::error_code ignored; std::filesystem::remove_all(directory, ignored); } }
};
}

bool mapUvttExportSupported(const MapModel &map, int pixelsPerCell, std::string *error)
{
    const auto fail = [&](const char *message) { if (error) *error = message; return false; };
    if (map.atlasLevel != MapLevel::Tactical || map.gridType != GridType::Square)
        return fail("Universal VTT supports tactical maps with a square grid.");
    if (map.width < 1 || map.height < 1 || map.width > 256 || map.height > 256 ||
        map.cells.size() != static_cast<std::size_t>(map.width) * map.height)
        return fail("Universal VTT requires a complete map of at most 256 by 256 cells.");
    const auto sizing = calculateMapPngExportSizing(map, pixelsPerCell);
    if (!sizing.valid || sizing.widthPixels > 4096 || sizing.heightPixels > 4096 ||
        sizing.uncompressedBytes > 64ull * 1024 * 1024)
        return fail("Universal VTT image exceeds 4096 pixels per side or 64 MiB. Lower the output detail.");
    if (error) error->clear();
    return true;
}

bool exportMapUvtt(const MapModel &map, const std::filesystem::path &destination,
                   int pixelsPerCell, bool overwriteExisting, std::string *error)
{
    try
    {
        if (!mapUvttExportSupported(map, pixelsPerCell, error)) return false;
        if (destination.extension() != ".uvtt" && destination.extension() != ".dd2vtt")
            throw std::runtime_error("Universal VTT output requires a .uvtt or .dd2vtt filename.");
        const auto target = mapFileIoPath(std::filesystem::absolute(destination).lexically_normal());
        const auto status = std::filesystem::symlink_status(target);
        if (std::filesystem::is_symlink(status) ||
            (std::filesystem::exists(status) && (!overwriteExisting || !std::filesystem::is_regular_file(status))))
            throw std::runtime_error("The Universal VTT destination exists or cannot be safely replaced.");
        const auto geometry = mapUvttGeometry(map);
        if (geometry.walls.size() + geometry.doors.size() > 8192)
            throw std::runtime_error("Universal VTT geometry exceeds 8192 segments. Simplify the map first.");
        std::filesystem::create_directories(target.parent_path());
        Scratch scratch;
        const auto directory = target.parent_path() / (".gm-uvtt-" + AtomicProjectFile::uniqueSuffix());
        if (!std::filesystem::create_directory(directory)) throw std::runtime_error("Could not reserve UVTT staging storage.");
        scratch.directory = directory;
#if !defined(_WIN32)
        std::filesystem::permissions(directory, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
#endif
        const auto png = directory / "map.png";
        MapPngExportOptions options;
        options.pixelsPerCell = pixelsPerCell;
        options.showGrid = options.showLabels = options.showAnnotations = options.showRoutes = options.showTokens = false;
        options.applyFog = false;
        if (!exportMapPng(map, png, options, error)) return false;
        if (std::filesystem::file_size(png) > 64ull * 1024 * 1024)
            throw std::runtime_error("Universal VTT embedded image exceeds the 64 MiB limit.");
        std::ifstream input(png, std::ios::binary);
        std::string image{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        if (input.bad() || image.size() < 8 || image.compare(0, 8, "\x89PNG\r\n\x1a\n", 8) != 0)
            throw std::runtime_error("Universal VTT raster did not produce a valid PNG.");
        const auto json = payload(map, pixelsPerCell, geometry, base64(image));
        if (overwriteExisting) AtomicProjectFile::write(target, json);
        else
        {
            // Link installation is atomic and refuses a destination created
            // after preflight; no-overwrite exports never replace raced files.
            const auto prepared = directory / "complete.uvtt";
            AtomicProjectFile::write(prepared, json);
            std::filesystem::create_hard_link(prepared, target);
        }
        if (error) error->clear();
        return true;
    }
    catch (const std::exception &exception) { if (error) *error = exception.what(); return false; }
}
