#include "Controllers/Editor/Widgets/MapCreator/MapCreatorController.hpp"
#include "Models/Localization/LocalizationService.hpp"

bool MapCreatorController::attachDraftMapToEncounter()
{
    if (!_integration.attachSavedMapToEncounterHandler || generationBusy() || !saveDraftMap()) return false;
    if (!_persistence.saveReceipt.committed || _persistence.saveReceipt.mapId.empty()) return false;
    const bool assigned = _integration.attachSavedMapToEncounterHandler(_persistence.saveReceipt.mapId);
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    _integration.statusMessage = assigned
        ? (german ? "Karte gespeichert und der Begegnung zugewiesen." : "Map saved and assigned to the encounter.")
        : (german ? "Karte gespeichert; Zuweisung fehlgeschlagen. Prüfe die Begegnung und versuche es erneut."
                  : "Map saved; assignment failed. Check the encounter and retry.");
    return assigned;
}
