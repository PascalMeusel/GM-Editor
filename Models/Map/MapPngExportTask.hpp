#pragma once
#include "Models/Map/MapPngExport.hpp"
#include <future>
#include <memory>
#include <optional>

struct MapPngExportJob
{
    MapModel map;
    MapPngExportOptions options;
    std::filesystem::path destination;
    std::optional<MapPngExportRect> viewport;
    std::string tileStem;
    std::uint64_t authoringSerial = 0;
    bool overwrite = false;
    bool universalVtt = false;
};

struct MapPngPreparedExport
{
    MapPngExportJob job;
    bool success = false;
    std::string error;
    double milliseconds = 0;
    std::filesystem::path staging;
    std::vector<std::filesystem::path> names;
    ~MapPngPreparedExport();
    // UI thread only, after checking current map/serial/player consent.
    // Rename rollback preserves existing output on publication failure.
    bool publish(const EntityId &currentMapId, std::uint64_t currentSerial, bool playerConsent);
};

class MapPngExportTask
{
  public:
    bool busy() const
    {
        return pending_.valid();
    }
    void start(MapPngExportJob job);
    std::unique_ptr<MapPngPreparedExport> takeReady();

  private:
    std::future<std::unique_ptr<MapPngPreparedExport>> pending_;
};
