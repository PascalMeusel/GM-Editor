#pragma once
#include "Controllers/Editor/EditorWindowController.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Map/MapPackageDraftInstallService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "imgui.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <thread>

namespace MapBoundaryTests
{
inline void require(bool condition, const std::string &message) { if (!condition) throw std::runtime_error(message); }
inline void environment(const char *key, const std::string &value)
{
#ifdef _WIN32
    require(_putenv_s(key, value.c_str()) == 0, "Cannot isolate map boundary fixture");
#else
    require(setenv(key, value.c_str(), 1) == 0, "Cannot isolate map boundary fixture");
#endif
}
template<class Predicate> void until(Predicate done)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!done())
    {
        require(std::chrono::steady_clock::now() < deadline, "Timed out waiting for generated worker fixture");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
struct Gate
{
    std::promise<void> enteredPromise, releasePromise;
    std::shared_future<void> entered = enteredPromise.get_future().share();
    std::shared_future<void> released = releasePromise.get_future().share();
    std::atomic_bool signalled{false}, opened{false}, timedOut{false};
    void wait()
    {
        if (!signalled.exchange(true)) enteredPromise.set_value();
        // A broken reset must fail rather than leave hosted CI deadlocked.
        if (released.wait_for(std::chrono::seconds(3)) != std::future_status::ready) timedOut = true;
    }
    void open() { if (!opened.exchange(true)) releasePromise.set_value(); }
    ~Gate() { open(); }
};
struct GateRelease { Gate &gate; ~GateRelease() { gate.open(); } };
inline Campaign fixture(const std::string &label)
{
    Campaign campaign; campaign.ruleSet = std::make_shared<DND5e>();
    campaign.name = label + campaign.metadata.id;
    MapModel map; map.name = label + " saved map"; map.width = map.height = 4;
    map.isMainMap = true; resizeMapCells(map); ensureDefaultMapLayer(map);
    campaign.maps.push_back(map); CampaignRepository::saveCampaign(campaign); return campaign;
}
inline Campaign reload(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns()) if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Generated campaign missing after reload");
}
inline std::string bytes(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary); require(bool(stream), "Cannot read generated canary");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
inline std::map<std::string, std::string> files(const std::filesystem::path &root)
{
    std::map<std::string, std::string> result;
    if (std::filesystem::exists(root)) for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
        if (entry.is_regular_file()) result[entry.path().lexically_relative(root).generic_string()] = bytes(entry.path());
    return result;
}
}

// Action/state access only. Tests invoke production Editor Save, Discard,
// campaign replacement and retired-work tick; no Application/native setup.
struct MapCampaignBoundaryProbe
{
    static MapCreatorController &map(EditorWindowController &editor) { return *editor._children.mapCreator; }
    static MapCreatorView &view(MapCreatorController &map) { return *map._view; }
    static MapModel &draft(MapCreatorController &map) { return map._draftMap; }
    static auto &transfer(MapCreatorController &map) { return map._transfer; }
    static auto &history(MapCreatorController &map) { return map._history; }
    static auto &generation(MapCreatorController &map) { return map._generation; }
    static auto &editing(MapCreatorController &map) { return map._editing; }
    static auto &integration(MapCreatorController &map) { return map._integration; }
    static void edit(MapCreatorController &map, const std::string &name)
    { map.captureUndoSnapshot("Synthetic edit"); map._draftMap.name = name; ++map._draftMap.revision; }
    static void journal(MapCreatorController &map) { map.updateCrashJournal(); }
    static void startExport(MapCreatorController &map, MapPngExportJob job) { map.startPngExport(std::move(job)); }
    static void pollExport(MapCreatorController &map) { map.updatePngExport(); }
    static void queue(MapCreatorController &map) { map.queueDraftGeneration(); }
    static void dispatch(MapCreatorController &map) { map.dispatchQueuedGeneration(); }
    static void cancel(MapCreatorController &map) { map.cancelDraftGeneration(); }
    static void exportCollision(MapCreatorController &map, const MapPngExportJob &job)
    { map._draftMap = job.map; map._editing.authoringSerial = job.authoringSerial; }
    static void dirtyView(MapCreatorController &map)
    {
        auto &v = *map._view;
        v._tokenDragArmed = v._terrainDragging = v._areaDragging = v._imageTraceActive = true;
        v._generatedFeatureGeometryEditActive = true; v._regionHandle = 1;
        v._loadMapReviewRequested = v._newMapReviewRequested = true;
        v._imageTraceName = "OLD PRIVATE TRACE"; v._imageTraceControlPoints.push_back({1, 1});
        v._imageTraceGmOnly = false;
        v._mapEditConflictsReady = true; v._mapEditConflictMessage = "OLD PRIVATE CONFLICT";
    }
    static bool cleanView(MapCreatorController &map)
    {
        auto &v = *map._view;
        return !v.hasPendingAuthoringInteraction() && !v._loadMapReviewRequested && !v._newMapReviewRequested &&
            v._imageTraceName.empty() && v._imageTraceControlPoints.empty() && v._imageTraceGmOnly && !v._mapEditConflictsReady &&
            v._mapEditConflictMessage.empty() && !v._setupPreviewMap && !v._canvasPreviewCompiler.state().source;
    }
    static void blockedJobs(MapCreatorController &map, MapBoundaryTests::Gate &gate)
    {
        auto &g = map._generation;
        g.inProgress = true; g.activeSerial = ++g.requestSerial;
        g.cancellation = std::make_shared<std::atomic_bool>(false);
        g.future = std::async(std::launch::async, [&gate, serial = g.activeSerial] {
            gate.wait(); map_creator_controller::DraftGenerationResult result; result.requestSerial = serial; return result;
        });
        auto &v = *map._view; v._setupPreviewCancel = std::make_shared<std::atomic_bool>(false);
        v._setupPreviewJob = std::async(std::launch::async, [&gate] { gate.wait(); MapModel map; map.name = "OLD PREVIEW"; return map; });
        v._mapEditInspection = std::async(std::launch::async, [&gate, id = map._draftMap.metadata.id,
                                                           serial = map._editing.authoringSerial] {
            gate.wait(); return MapCreatorView::ConflictInspection{id, serial, {}, "OLD ERROR"};
        });
    }
    static bool pendingRetired(MapCreatorController &map)
    { return map._generation.future.valid() || map._view->_setupPreviewJob.valid() || map._view->_mapEditInspection.valid(); }
};
