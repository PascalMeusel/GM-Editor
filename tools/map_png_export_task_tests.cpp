#include "Models/Map/MapPngExportTask.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#if defined(_WIN32)
#include <windows.h>
#endif
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
void require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
std::unique_ptr<MapPngPreparedExport> await(MapPngExportTask &task)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    for (;;)
    {
        if (auto result = task.takeReady())
            return result;
        require(std::chrono::steady_clock::now() < deadline, "PNG task timeout");
        std::this_thread::yield();
    }
}
std::string bytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}
int main()
{
    const auto root = std::filesystem::temp_directory_path() / createEntityId("png-task-tests");
    try
    {
        std::filesystem::create_directories(root);
        MapPngExportJob job;
        job.map.width = job.map.height = 8;
        job.map.cells.resize(64);
        job.map.name = "PNG fixture";
        job.options.pixelsPerCell = 8;
        job.options.applyFog = false;
        job.destination = root / "output.png";
        job.authoringSerial = 3;
        job.overwrite = true;
        std::ofstream(job.destination) << "previous output";
        MapPngExportTask task;
        task.start(job);
        job.map.name = "mutated caller";
        auto result = await(task);
        require(result->success && result->job.map.name == "PNG fixture", "Preparation failed or snapshot mutable");
        require(bytes(job.destination) == "previous output", "Preparation overwrote destination");
        require(!result->publish(job.map.metadata.id, 4, true), "Stale export published");
        require(bytes(job.destination) == "previous output", "Stale export damaged destination");
        require(result->publish(job.map.metadata.id, 3, true), "Current export failed publication");
        std::string error;
        require(exportMapPng(result->job.map, root / "expected.png", job.options, &error),
                "Authoritative control export failed");
        require(bytes(job.destination) == bytes(root / "expected.png"), "Async and authoritative PNG differ");
        result.reset();
#if defined(_WIN32)
        task.start(job);
        result = await(task);
        require(result->success, "Locked-destination preparation failed");
        const auto priorLocked = bytes(job.destination);
        HANDLE locked = CreateFileW(mapFileIoPath(job.destination).c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        require(locked != INVALID_HANDLE_VALUE, "Could not lock destination for negative control");
        const bool replaced = result->publish(job.map.metadata.id, 3, true);
        CloseHandle(locked);
        require(!replaced && bytes(job.destination) == priorLocked, "Publication failure damaged existing output");
        result.reset();
#endif
        job.options.playerSafe = true;
        job.map.visibility = MapVisibility::PlayersVisible;
        task.start(job);
        result = await(task);
        require(result->success, "Player preparation failed");
        const auto prior = bytes(job.destination);
        require(!result->publish(job.map.metadata.id, 3, false), "Revoked player consent published");
        require(bytes(job.destination) == prior, "Revocation damaged destination");
        result.reset();
        job.options.playerSafe = false;
        job.tileStem = "fixture";
        job.destination = root / "fixture_tiles.json";
        task.start(job);
        result = await(task);
        require(result->success && result->names.size() >= 2, "Tiled preparation failed");
        require(result->publish(job.map.metadata.id, 3, true), "Tiled publication failed");
        require(!bytes(job.destination).empty(), "Tiled manifest missing");
        result.reset();
        auto longDirectory = root;
        while (longDirectory.native().size() < 280)
            longDirectory /= std::string(55, 'x');
        job.destination = longDirectory / "long-export.png";
        job.tileStem.clear();
        task.start(job);
        result = await(task);
        require(result->success && result->publish(job.map.metadata.id, 3, true),
                "Long-path staging or publication failed");
        require(std::filesystem::exists(mapFileIoPath(job.destination)), "Long-path output missing");
        result.reset();
        std::filesystem::remove_all(mapFileIoPath(root));
        std::cout << "PNG immutable snapshot, stale revision, consent and parity controls passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\nEvidence: " << root << '\n';
        return 1;
    }
}
