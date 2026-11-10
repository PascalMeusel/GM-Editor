#include "Models/Map/MapPngExportTask.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include "Models/Map/MapUvttExport.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>

MapPngPreparedExport::~MapPngPreparedExport()
{
    // Only remove the unique staging directory we created. Unexpected entries
    // and recovery backups are deliberately retained for inspection.
    std::error_code ec;
    if (staging.empty())
        return;
    for (const auto &name : names)
        std::filesystem::remove(staging / name, ec);
    std::filesystem::remove(staging, ec);
}

bool MapPngPreparedExport::publish(const EntityId &currentMapId, std::uint64_t currentSerial, bool playerConsent)
{
    if (!success)
        return false;
    if (currentMapId != job.map.metadata.id || currentSerial != job.authoringSerial ||
        (job.options.playerSafe && !playerConsent))
    {
        error = "The map or player review changed while exporting. Export again.";
        return false;
    }
    const auto directory = mapFileIoPath(job.destination.parent_path());
    const auto backup = staging / "previous-output";
    std::vector<std::filesystem::path> saved, published;
    std::error_code ec;
    for (const auto &name : names)
    {
        const auto target = directory / name;
        if (!std::filesystem::exists(target, ec))
        {
            if (ec)
            {
                error = ec.message();
                return false;
            }
            continue;
        }
        if (!job.overwrite || std::filesystem::is_symlink(target, ec) || !std::filesystem::is_regular_file(target, ec))
        {
            error = "The export destination changed or cannot be replaced.";
            return false;
        }
    }
    if (job.universalVtt)
    {
        try
        {
            if (names.size() != 1) throw std::runtime_error("Universal VTT requires one prepared file.");
            const auto prepared = staging / names.front(), target = directory / names.front();
            if (job.overwrite)
            {
                std::ifstream input(prepared, std::ios::binary);
                const std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
                if (!input || input.bad() || bytes.empty()) throw std::runtime_error("Could not read prepared UVTT output.");
                AtomicProjectFile::write(target, bytes);
            }
            else std::filesystem::create_hard_link(prepared, target);
            return true;
        }
        catch (const std::exception &exception) { error = exception.what(); return false; }
    }
    std::filesystem::create_directory(backup, ec);
    if (ec)
    {
        error = ec.message();
        return false;
    }
    auto rollback = [&] {
        bool restored = true;
        for (auto i = published.rbegin(); i != published.rend(); ++i)
        {
            std::filesystem::rename(directory / *i, staging / *i, ec);
            restored = restored && !ec;
        }
        for (auto i = saved.rbegin(); i != saved.rend(); ++i)
        {
            std::filesystem::rename(backup / *i, directory / *i, ec);
            restored = restored && !ec;
        }
        if (!restored)
            error += " Previous output retained at " + backup.u8string();
        else
            std::filesystem::remove(backup, ec);
    };
    for (const auto &name : names)
    {
        if (std::filesystem::exists(directory / name, ec))
        {
            std::filesystem::rename(directory / name, backup / name, ec);
            if (ec)
            {
                error = ec.message();
                rollback();
                return false;
            }
            saved.push_back(name);
        }
        std::filesystem::rename(staging / name, directory / name, ec);
        if (ec)
        {
            error = ec.message();
            rollback();
            return false;
        }
        published.push_back(name);
    }
    for (const auto &name : saved)
        std::filesystem::remove(backup / name, ec);
    std::filesystem::remove(backup, ec);
    return true;
}

void MapPngExportTask::start(MapPngExportJob job)
{
    if (busy())
        throw std::logic_error("A PNG export is already being prepared.");
    if (job.universalVtt && (job.options.playerSafe || job.viewport || !job.tileStem.empty()))
        throw std::invalid_argument("Universal VTT is a whole-map GM transfer.");
    if (job.destination.filename().empty() || job.tileStem.find_first_of("/\\") != std::string::npos ||
        job.tileStem == "." || job.tileStem == "..")
        throw std::invalid_argument("PNG output requires a filename and a single tile-name component.");
    job.destination = std::filesystem::absolute(job.destination).lexically_normal();
    pending_ = std::async(std::launch::async, [job = std::move(job)]() mutable {
        auto result = std::make_unique<MapPngPreparedExport>();
        result->job = std::move(job);
        const auto started = std::chrono::steady_clock::now();
        try
        {
            auto &request = result->job;
            if (request.options.showBackground && request.map.importedReference.enabled &&
                (!request.options.playerSafe || !request.map.importedReference.gmOnly))
            {
                const auto inspection = MapImageImport::inspectManagedReference(request.map.importedReference, true);
                if (!inspection.healthy)
                    throw std::runtime_error(MapImageImport::managedReferenceErrorMessage(inspection.errorCode));
            }
            std::filesystem::create_directories(mapFileIoPath(request.destination.parent_path()));
            const auto staging = mapFileIoPath(request.destination.parent_path() / createEntityId(".gm-png-export"));
            if (!std::filesystem::create_directory(staging))
                throw std::runtime_error("Could not create PNG staging directory.");
            result->staging = staging;
            if (request.tileStem.empty())
            {
                result->names = {request.destination.filename()};
                const auto path = result->staging / result->names.front();
                const auto plan = compileMapRenderPlan(request.map, mapPngExportRenderPlanRequest(request.options));
                result->success = request.universalVtt ? exportMapUvtt(request.map, path, request.options.pixelsPerCell, true, &result->error)
                    : request.viewport ? exportMapPngRegion(request.map, path, plan, *request.viewport,
                                                                        request.options.pixelsPerCell, &result->error)
                                                   : exportMapPng(request.map, path, plan,
                                                                  request.options.pixelsPerCell, &result->error);
            }
            else
            {
                MapPngTiledExportPlan written;
                result->success = exportMapPngTiled(request.map, result->staging, request.tileStem, request.options,
                                                    false, &written, &result->error);
                for (const auto &tile : written.tiles)
                    result->names.push_back(tile.fileName);
                result->names.push_back(request.tileStem + "_tiles.json");
            }
        }
        catch (const std::exception &error)
        {
            result->success = false;
            result->error = error.what();
        }
        catch (...)
        {
            result->success = false;
            result->error = "Unknown PNG export failure.";
        }
        result->milliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        return result;
    });
}

std::unique_ptr<MapPngPreparedExport> MapPngExportTask::takeReady()
{
    if (!pending_.valid() || pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return {};
    return pending_.get();
}
