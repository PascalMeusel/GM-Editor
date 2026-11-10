#include "Models/Map/MapUvttExport.hpp"
#include "Models/Map/MapPngExportTask.hpp"
#include "Models/Media/RgbaImageLoader.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
namespace
{
void require(bool condition, const std::string &message) { if (!condition) throw std::runtime_error(message); }
std::string bytes(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "Fixture read failed");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::string imageBytes(const std::string &json)
{
    const auto begin = json.find("\"image\":\"");
    require(begin != std::string::npos, "UVTT lacks embedded image");
    const auto end = json.find('"', begin + 9);
    require(end != std::string::npos, "UVTT image not terminated");
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const std::string symbols = alphabet;
    std::string decoded;
    unsigned int word = 0;
    int bits = 0;
    for (std::size_t index = begin + 9; index < end; ++index)
    {
        if (json[index] == '=') break;
        const auto value = symbols.find(json[index]);
        require(value != std::string::npos, "UVTT image is not base64");
        word = (word << 6) | static_cast<unsigned int>(value);
        bits += 6;
        if (bits >= 8) { bits -= 8; decoded += static_cast<char>((word >> bits) & 255); }
    }
    return decoded;
}
std::unique_ptr<MapPngPreparedExport> wait(MapPngExportTask &task)
{
    for (int i = 0; i < 10000; ++i)
    {
        if (auto ready = task.takeReady()) return ready;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("UVTT worker did not finish");
}
MapModel fixture()
{
    MapModel map;
    map.metadata.id = "uvtt_fixture";
    map.name = "SECRET_CAMPAIGN_NAME";
    map.description = "SECRET_PREP_NOTES";
    map.atlasLevel = MapLevel::Tactical;
    map.scope = MapScope::Dungeon;
    map.creationIntent = MapCreationIntent::Dungeon;
    map.generation.recipe.generatorVersion = 10;
    map.gridType = GridType::Square;
    map.width = 5; map.height = 4;
    map.cells.resize(20);
    for (auto &cell : map.cells) applyMapCellTerrainDefaults(cell, MapTileKind::Room);
    applyMapCellTerrainDefaults(map.cells[0], MapTileKind::Wall);
    applyMapCellTerrainDefaults(map.cells[1], MapTileKind::Wall);
    map.cells[7].kind = MapTileKind::Door;
    map.cells[6].wayKind = map.cells[8].wayKind = MapWayKind::DungeonCorridor;
    map.cells[10].lineOfSightBlockedMask = 2;
    map.cells[11].lineOfSightBlockedMask = 8;
    map.cells[14].movementOneWayBlockedMask = 8;
    map.cells[15].movementBlockedMask = 2;
    MapTokenModel token; token.name = "SECRET_TOKEN"; token.x = 3; token.y = 2;
    map.tokens.push_back(token);
    MapFeatureModel feature; feature.name = "SECRET_ANNOTATION"; feature.gmNotes = "SECRET_FEATURE_NOTE";
    map.features.push_back(feature);
    return map;
}
}
int main(int argc, char **argv)
{
    const auto root = fs::temp_directory_path() / ("gm-uvtt-test-" + AtomicProjectFile::uniqueSuffix());
    try
    {
        require(fs::create_directory(root), "Could not reserve fixture directory");
#if defined(_WIN32)
        require(_putenv_s("TTRPG_DATA_ROOT", root.u8string().c_str()) == 0, "Could not isolate fixture");
#else
        require(setenv("TTRPG_DATA_ROOT", root.u8string().c_str(), 1) == 0, "Could not isolate fixture");
#endif
        auto map = fixture();
        std::string error;
        const auto geometry = mapUvttGeometry(map);
        require(geometry.walls.size() == 5 && geometry.doors.size() == 1,
                "Solid-cell boundaries or symmetric edge deduplication incorrect");
        require(geometry.doors[0] == std::array<int,4>{5,2,5,4}, "Door corridor orientation incorrect");
        require(geometry.omittedDirectionalCells == 1 && geometry.omittedMovementOnlyCells == 1,
                "Unsupported tactical metadata was not reported");
        const auto output = root / "fixture.uvtt";
        require(exportMapUvtt(map, output, 32, false, &error), error);
        const auto json = bytes(output);
        require(json.find("\"format\":0.2") != std::string::npos &&
                    json.find("\"map_size\":{\"x\":5,\"y\":4}") != std::string::npos &&
                    json.find("\"pixels_per_grid\":32") != std::string::npos &&
                    json.find("\"closed\":true") != std::string::npos &&
                    json.find("\"lights\":[]") != std::string::npos &&
                    json.find("SECRET_") == std::string::npos, "UVTT contract/omissions incorrect");
        const auto png = root / "decoded.png";
        AtomicProjectFile::write(png, imageBytes(json));
        LoadedRgbaImage image;
        require(loadRgbaImage(png, image) && image.width == 160 && image.height == 128,
                "Embedded image does not decode to grid dimensions");
        auto clean = map;
        clean.tokens.clear(); clean.features.clear(); clean.name.clear(); clean.description.clear();
        const auto cleanOutput = root / "clean.uvtt";
        require(exportMapUvtt(clean, cleanOutput, 32, false, &error), error);
        require(imageBytes(bytes(cleanOutput)) == imageBytes(json), "Private text/token fields leaked into UVTT raster");
        require(!exportMapUvtt(map, output, 32, false, &error) && bytes(output) == json,
                "Unconfirmed export replaced existing bytes");
        map.gridType = GridType::Hex;
        require(!exportMapUvtt(map, output, 32, true, &error) && bytes(output) == json,
                "Unsupported grid replaced previous export");
        map = fixture(); map.width = 256; map.height = 256; map.cells.resize(65536);
        require(!mapUvttExportSupported(map, 128, &error), "Oversized raster was accepted");
        map = fixture();
        const auto linked = root / "symlink.uvtt";
        std::error_code linkError;
        fs::create_symlink(output, linked, linkError);
        if (!linkError)
            require(!exportMapUvtt(map, linked, 32, true, &error) && bytes(output) == json,
                    "Export followed a destination symlink");
        else
            std::cout << "SKIP: UVTT symlink subcase: " << linkError.message() << '\n';

        MapPngExportTask task;
        MapPngExportJob job;
        job.map = map; job.destination = output; job.options.pixelsPerCell = 32;
        job.universalVtt = true; job.authoringSerial = 9; job.overwrite = true;
        task.start(job);
        auto prepared = wait(task);
        require(prepared->success, prepared->error);
        require(!prepared->publish(map.metadata.id, 10, true) && bytes(output) == json,
                "Stale asynchronous UVTT replaced existing output");
        require(prepared->publish(map.metadata.id, 9, true), prepared->error);
        prepared.reset();
        job.destination = root / "raced.uvtt"; job.overwrite = false;
        task.start(job);
        prepared = wait(task);
        require(prepared->success, prepared->error);
        AtomicProjectFile::write(job.destination, "PRESERVE_RACED_FILE");
        require(!prepared->publish(map.metadata.id, 9, true) && bytes(job.destination) == "PRESERVE_RACED_FILE",
                "UVTT publication replaced a file created during preparation");
        prepared.reset();
        for (const auto &entry : fs::directory_iterator(root))
            require(entry.path().filename().string().rfind(".gm-", 0) != 0, "UVTT staging was not cleaned");
        if (argc == 2)
        {
            fs::copy_file(output, fs::u8path(argv[1]), fs::copy_options::none);
            std::cout << "Interoperability fixture: " << argv[1] << '\n';
        }
        fs::remove_all(root);
        std::cout << "UVTT export checks passed\n";
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << "\nGenerated fixtures: " << root.u8string() << '\n';
        return 1;
    }
}
