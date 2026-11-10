#include "Controllers/Editor/Widgets/MapCreator/MapCreatorController.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorFilePathPolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorInternals.hpp"
#include "Models/Map/MapUvttExport.hpp"
#include "Models/Localization/LocalizationService.hpp"

using namespace gm::map_creator_detail;

void MapCreatorController::exportDraftUvtt()
{
    if (_transfer.pngExportTask.busy()) return;
    const int pixelsPerCell = mapPngPixelsPerCellForPreset(_draftMap,
        static_cast<MapPngExportSizePreset>(std::clamp(_transfer.exportPngSizePresetIndex,
            static_cast<int>(MapPngExportSizePreset::Screen), static_cast<int>(MapPngExportSizePreset::Custom))),
        _transfer.exportPixelsPerCell);
    std::string error;
    if (!mapUvttExportSupported(_draftMap, pixelsPerCell, &error))
    {
        _integration.statusMessage = error;
        return;
    }
    std::filesystem::path path = std::filesystem::path("Resources") / "Maps" / "Exports" /
        (sanitizeFileName(_draftMap.name) + ".uvtt");
    bool overwrite = false;
#if defined(_WIN32)
    if (!browseForSavePath("Universal VTT\0*.uvtt\0", "uvtt",
                          mapCreatorFilesystemPathToUtf8(path.filename()), path)) return;
    overwrite = true; // Native dialog asks before replacement.
#else
    if (!mapCreatorManualPathIsReady(_transfer.manualUvttExportPath))
    {
        _integration.statusMessage = "Enter a Universal VTT destination path before exporting.";
        return;
    }
    path = mapCreatorPathWithDefaultExtension(mapCreatorFilesystemPathFromUtf8(_transfer.manualUvttExportPath), ".uvtt");
#endif
    _transfer.manualUvttExportPath = mapCreatorFilesystemPathToUtf8(path);
    MapPngExportJob job;
    job.map = _draftMap;
    job.destination = path;
    job.options.pixelsPerCell = pixelsPerCell;
    job.options.showGrid = job.options.showLabels = job.options.showAnnotations = job.options.showRoutes = job.options.showTokens = false;
    job.options.applyFog = false;
    job.authoringSerial = _editing.authoringSerial;
    job.overwrite = overwrite;
    job.universalVtt = true;
    startPngExport(std::move(job));
}
