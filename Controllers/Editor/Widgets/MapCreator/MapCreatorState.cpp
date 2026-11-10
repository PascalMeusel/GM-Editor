#include "Models/Project/ContextLinkMetadata.hpp"
#include "Models/Map/MapTravelCreation.hpp"
#include "MapCreatorController.hpp"
#include <random>
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorArtStylePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorFilePathPolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPlayerOutputPolicy.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapStrokeAuthoring.hpp"
#include "Models/Map/MapCheckpointService.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapPackageDraftInstallService.hpp"
#include "Models/Map/MapPackageService.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapWorldBrief.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/WorldInspectorService.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#include <commdlg.h>
#endif

using namespace PersistenceSerialization;

#include "MapCreatorInternals.hpp"
using namespace gm::map_creator_detail;

void MapCreatorController::setTokenLinkFromSelection(MapTokenModel &token)
{
    if (_editing.selectedLinkedEntityIndex <= 0)
        return;

    int cursor = 1;
    for (const CharacterModel &character : _currentCampaign.characters)
    {
        if (_editing.selectedLinkedEntityIndex == cursor)
        {
            token.linkedEntityId = character.metadata.id;
            token.linkedEntityType = "player";
            token.kind = MapTokenKind::Player;
            token.name = std::string(character.nameBuffer);
            return;
        }
        ++cursor;
    }
    for (const NPCModel &npc : _currentCampaign.npcs)
    {
        if (_editing.selectedLinkedEntityIndex == cursor)
        {
            token.linkedEntityId = npc.metadata.id;
            token.linkedEntityType = "npc";
            token.kind = MapTokenKind::NPC;
            token.name = npc.name;
            return;
        }
        ++cursor;
    }
    for (const MonsterDefinitionModel &monster : _currentCampaign.customMonsters)
    {
        if (_editing.selectedLinkedEntityIndex == cursor)
        {
            token.linkedEntityId = monster.metadata.id;
            token.linkedEntityType = "monster";
            token.kind = MapTokenKind::Monster;
            token.name = monster.name;
            return;
        }
        ++cursor;
    }
    const int vehicleIndex = _editing.selectedLinkedEntityIndex - cursor;
    if (vehicleIndex >= 0 && vehicleIndex < static_cast<int>(_currentCampaign.partyVehicles.size()))
    {
        token.linkedEntityType = "vehicle";
        token.linkedEntityId = _currentCampaign.partyVehicles[vehicleIndex];
        token.kind = MapTokenKind::Vehicle;
        token.name = _currentCampaign.partyVehicles[vehicleIndex];
    }
}

void MapCreatorController::resizeDraftCells()
{
    resizeMapCells(_draftMap);
    ensureDefaultMapLayer(_draftMap);
    normalizeObjectSelection();
    _integration.statusMessage = gm::localization::trUi("Applied map dimensions.");
}

void MapCreatorController::syncScopeFromIndex()
{
    syncCreationIntentFromIndex(false);
}

void MapCreatorController::syncCreationIntentFromIndex(bool resizeToPreset)
{
    const MapLevel previousLevel = _draftMap.atlasLevel;
    _generation.selectedCreationIntentIndex = std::clamp(
        _generation.selectedCreationIntentIndex,
        static_cast<int>(MapCreationIntent::WorldOverview),
        static_cast<int>(MapCreationIntent::ImportedReference));
    _generation.selectedSizePresetIndex = std::clamp(
        _generation.selectedSizePresetIndex,
        static_cast<int>(MapSizePreset::Small),
        static_cast<int>(MapSizePreset::Custom));
    const MapCreationIntent intent = static_cast<MapCreationIntent>(_generation.selectedCreationIntentIndex);
    const MapSizePreset preset = static_cast<MapSizePreset>(_generation.selectedSizePresetIndex);
    applyMapCreationIntent(_draftMap, intent, preset, resizeToPreset);
    switch (intent)
    {
    case MapCreationIntent::WorldOverview:
    case MapCreationIntent::RegionExploration:
        _generation.generatorPurposeIndex = static_cast<int>(MapGenerationPurpose::Region);
        break;
    case MapCreationIntent::Settlement:
        _generation.selectedGeneratorIndex = static_cast<int>(GeneratorPreset::City);
        _generation.generatorPurposeIndex = static_cast<int>(MapGenerationPurpose::SettlementEnvirons);
        break;
    case MapCreationIntent::TravelScene:
        _generation.generatorPurposeIndex = static_cast<int>(MapGenerationPurpose::TravelArea);
        break;
    case MapCreationIntent::Dungeon:
        _generation.selectedGeneratorIndex = static_cast<int>(GeneratorPreset::Dungeon);
        break;
    case MapCreationIntent::Cave:
        _generation.selectedGeneratorIndex = static_cast<int>(GeneratorPreset::Cave);
        break;
    case MapCreationIntent::TacticalEncounter:
        _generation.generatorPurposeIndex = static_cast<int>(MapGenerationPurpose::Wilderness);
        break;
    case MapCreationIntent::ImportedReference:
        break;
    }
    _persistence.selectedScopeIndex = static_cast<int>(_draftMap.scope);
    _editing.showEditorGrid = _draftMap.atlasLevel == MapLevel::Tactical;
    if (previousLevel != _draftMap.atlasLevel)
    {
        _transfer.exportGmSettings.showGrid = _editing.showEditorGrid;
        _transfer.exportPlayerSettings.showGrid = _editing.showEditorGrid;
    }
    const std::vector<MapTileKind> palette = getPalette();
    if (_editing.selectedTileIndex >= static_cast<int>(palette.size()))
        _editing.selectedTileIndex = 0;
}

std::vector<MapTileKind> MapCreatorController::getPalette() const
{
    return paletteForMapScope(static_cast<MapScope>(_persistence.selectedScopeIndex));
}

std::vector<const char *> MapCreatorController::getParentMapLabels() const
{
    std::vector<const char *> labels;
    labels.push_back(gm::localization::trUi("No parent map"));
    for (const auto &map : _currentCampaign.maps)
        labels.push_back(map.name.empty() ? "<Unnamed Map>" : map.name.c_str());
    return labels;
}

std::vector<const char *> MapCreatorController::getLinkedEntityLabels() const
{
    std::vector<const char *> labels;
    labels.push_back(gm::localization::trUi("No linked entity"));
    for (const CharacterModel &character : _currentCampaign.characters)
        labels.push_back(character.nameBuffer[0] == '\0' ? "<Unnamed Player>" : character.nameBuffer);
    for (const NPCModel &npc : _currentCampaign.npcs)
        labels.push_back(npc.name.empty() ? "<Unnamed NPC>" : npc.name.c_str());
    for (const MonsterDefinitionModel &monster : _currentCampaign.customMonsters)
        labels.push_back(monster.name.empty() ? "<Unnamed Monster>" : monster.name.c_str());
    for (const std::string &vehicle : _currentCampaign.partyVehicles)
        labels.push_back(vehicle.empty() ? "<Unnamed Vehicle>" : vehicle.c_str());
    return labels;
}

void MapCreatorController::onShow()
{
    _editing.focusViewRequested = true;
    _view->show();
    _view->requestDockOnNextUpdate();
}
