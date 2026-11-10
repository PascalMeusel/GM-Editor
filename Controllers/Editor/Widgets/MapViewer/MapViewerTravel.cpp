#include "Controllers/Editor/Widgets/MapViewer/MapViewerControllerInternals.hpp"
#include "Models/Session/TravelSessionIntegration.hpp"

void MapViewerController::selectCell(int x, int y)
{
    const MapModel *map = getSelectedMap();
    if (!map || !isMapCellInside(*map, x, y))
    {
        clearSelectedCell();
        return;
    }

    _selectedCellX = x;
    _selectedCellY = y;
}

void MapViewerController::clearSelectedCell()
{
    _selectedCellX = -1;
    _selectedCellY = -1;
}

void MapViewerController::previewPartyMoveToCell(int x, int y)
{
    const MapModel *map = getSelectedMap();
    if (!map || !isMapCellInside(*map, x, y))
        return;

    _pendingPartyMoveX = x;
    _pendingPartyMoveY = y;
    _hasPendingPartyMove = true;
}

void MapViewerController::clearPendingPartyMove()
{
    _pendingPartyMoveX = -1;
    _pendingPartyMoveY = -1;
    _hasPendingPartyMove = false;
}

void MapViewerController::applyPendingPartyMove()
{
    if (!_hasPendingPartyMove)
        return;

    const int destinationX = _pendingPartyMoveX;
    const int destinationY = _pendingPartyMoveY;
    clearPendingPartyMove();
    movePartyToCell(destinationX, destinationY);
}

void MapViewerController::movePartyToCell(int x, int y)
{
    MapModel *map = getSelectedMap();
    if (!map || !isMapCellInside(*map, x, y))
        return;

    _currentCampaign.partyLocation.mapId = map->metadata.id;
    _currentCampaign.partyLocation.x = x;
    _currentCampaign.partyLocation.y = y;
    for (const MapRegionModel &region : map->regions)
    {
        const bool insideRegion = x >= region.x && y >= region.y && x < region.x + region.width && y < region.y + region.height;
        if (insideRegion)
        {
            std::string triggerError;
            AudioTriggers::playMatchingTriggers(_currentCampaign, _playbackService, "MapRegion", region.metadata.id, "Party Entered", triggerError);
        }
    }
    saveCampaign();
}

void MapViewerController::startTravelOnSelectedRoute()
{
    MapModel *map = getSelectedMap();
    if (!map || map->routes.empty())
        return;
    _selectedRouteIndex = std::clamp(_selectedRouteIndex, 0, static_cast<int>(map->routes.size()) - 1);
    const MapRouteModel &route = map->routes[_selectedRouteIndex];
    _currentCampaign.partyTravel.currentMapId = map->metadata.id;
    _currentCampaign.partyTravel.routeId = route.metadata.id;
    _currentCampaign.partyTravel.routeName = route.name;
    _currentCampaign.partyTravel.distanceMiles = route.distanceMiles;
    _currentCampaign.partyTravel.milesTraveled = 0.0f;
    _currentCampaign.partyTravel.watches = 0;
    _currentCampaign.partyTravel.randomEncounterChecks = 0;
    saveCampaign();
}

void MapViewerController::advanceTravelWatch()
{
    static constexpr float slowMilesPerWatch = 6.0f;
    static constexpr float normalMilesPerWatch = 8.0f;
    static constexpr float fastMilesPerWatch = 10.0f;

    float miles = normalMilesPerWatch;
    if (_currentCampaign.partyTravel.travelPace == "Slow")
        miles = slowMilesPerWatch;
    else if (_currentCampaign.partyTravel.travelPace == "Fast")
        miles = fastMilesPerWatch;

    _currentCampaign.partyTravel.watches += 1;
    _currentCampaign.partyTravel.milesTraveled = std::min(_currentCampaign.partyTravel.distanceMiles, _currentCampaign.partyTravel.milesTraveled + miles);
    _currentCampaign.partyTravel.rationsUsed += std::max(1, static_cast<int>(_currentCampaign.characters.size()));
    _currentCampaign.partyTravel.randomEncounterChecks += 1;
    saveCampaign();
}

void MapViewerController::generateTravelSceneCandidates()
{
    MapModel *map = getSelectedMap();
    if (!map || map->atlasLevel == MapLevel::Tactical)
    {
        _travelSceneStatus = gm::localization::trUi("Select a World, Region, or Site map first.");
        return;
    }

    const PartyTravelPlanModel &travel = _currentCampaign.partyTravel;
    const MapRouteModel *route = nullptr;
    if (travel.currentMapId == map->metadata.id && !travel.routeId.empty())
    {
        const auto routeIt = std::find_if(map->routes.begin(), map->routes.end(), [&](const MapRouteModel &candidate) {
            return candidate.metadata.id == travel.routeId;
        });
        if (routeIt != map->routes.end())
            route = &*routeIt;
    }
    if (!route && !map->routes.empty())
    {
        _selectedRouteIndex = std::clamp(_selectedRouteIndex, 0, static_cast<int>(map->routes.size()) - 1);
        route = &map->routes[static_cast<std::size_t>(_selectedRouteIndex)];
    }

    TravelSceneRecipe recipe;
    recipe.schemaVersion = 4;
    recipe.sourceMapId = map->metadata.id;
    recipe.routeId = route ? route->metadata.id : EntityId{};
    recipe.routeName = route ? route->name : std::string{};
    recipe.biome = route && !route->terrain.empty() ? route->terrain : "temperate wilderness";
    recipe.purpose = static_cast<TravelScenePurpose>(std::clamp(_travelScenePurposeIndex, 0, 4));
    recipe.dangerLevel = std::clamp(_travelDangerLevel, 0, 5);
    recipe.seed = _travelSceneSeed;
    recipe.contentLanguage = map->generation.recipe.contentLanguage;
    std::transform(recipe.contentLanguage.begin(), recipe.contentLanguage.end(), recipe.contentLanguage.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    std::replace(recipe.contentLanguage.begin(), recipe.contentLanguage.end(), '_', '-');
    if (recipe.contentLanguage == "de" || recipe.contentLanguage.rfind("de-", 0) == 0)
        recipe.contentLanguage = "de";
    else if (recipe.contentLanguage == "en" || recipe.contentLanguage.rfind("en-", 0) == 0)
        recipe.contentLanguage = "en";
    else
        recipe.contentLanguage = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                                     ? "de"
                                     : "en";
    recipe.nameCulture = map->generation.recipe.nameCulture.empty()
                             ? (recipe.contentLanguage == "de" ? "germanic_central_european" : "neutral_fantasy")
                             : map->generation.recipe.nameCulture;
    recipe.requiredElements = TravelSceneGenerator::parseElementList(_travelRequiredElements);
    recipe.excludedElements = TravelSceneGenerator::parseElementList(_travelExcludedElements);
    recipe.combatReady = _travelSceneCombatReady;

    _travelSceneCandidates = TravelSceneGenerator::generate(*map, route, recipe, 3);
    _travelSceneSessionId = TravelSession::activeSessionId(_currentCampaign);
    _travelSceneCampaignId = _currentCampaign.metadata.id;
    _selectedTravelSceneCandidate = _travelSceneCandidates.empty() ? -1 : 0;
    _travelSceneStatus = gm::localization::trUi(_travelSceneCandidates.empty()
                                                    ? "No usable travel scene could be generated from this map."
                                                    : "Three editable travel scenes are ready.");
}

bool MapViewerController::acceptSelectedTravelScene(bool prepareEncounter)
{
    if (_selectedTravelSceneCandidate < 0 || _selectedTravelSceneCandidate >= static_cast<int>(_travelSceneCandidates.size()))
        return false;
    if (_travelSceneCampaignId != _currentCampaign.metadata.id ||
        _travelSceneSessionId != TravelSession::activeSessionId(_currentCampaign))
    {
        _travelSceneStatus = "The active campaign or session changed. Generate and review travel scenes again.";
        return false;
    }
    TravelSceneCandidate selected = _travelSceneCandidates[static_cast<std::size_t>(_selectedTravelSceneCandidate)];
    if (prepareEncounter && !selected.canPrepareEncounter)
    {
        _travelSceneStatus = gm::localization::trUi("Choose Danger, Obstacle, or enable combat readiness first.");
        return false;
    }

    MapViewerCampaignSaveTransaction<TravelSceneCandidate> transaction(
        _currentCampaign,
        _selectedMapIndex,
        _travelSceneCandidates,
        _selectedTravelSceneCandidate);

    auto uniqueMapId = [&](std::string requested) {
        const std::string base = requested;
        int suffix = 2;
        while (findMapById(requested))
            requested = base + "::" + std::to_string(suffix++);
        return requested;
    };
    selected.localMap.metadata.id = uniqueMapId(selected.localMap.metadata.id);
    selected.localMap.description = selected.summary;
    const EntityId sceneMapId = selected.localMap.metadata.id;
    _currentCampaign.maps.push_back(selected.localMap);

    EntityId destinationMapId = sceneMapId;
    EntityId acceptedEncounterId;
    if (prepareEncounter)
    {
        EncounterAtlasTacticalRequest request;
        request.sourceRect = {0.0f, 0.0f, 1.0f, 1.0f};
        request.width = 24;
        request.height = 18;
        request.cellSize = 5.0f;
        request.tacticalMapId = uniqueMapId(sceneMapId + "::tactical");
        request.name = selected.title + " - " + gm::localization::trUi("Encounter");
        request.revision = 1;

        EncounterModel encounter;
        encounter.name = selected.title;
        encounter.description = selected.summary + " " + selected.complication;
        const EncounterAtlasTacticalResult tactical = EncounterAtlasService::deriveTacticalMap(_currentCampaign.maps.back(), request, &encounter);
        if (!tactical.valid)
        {
            transaction.rollback();
            _travelSceneStatus = tactical.errors.empty()
                                     ? gm::localization::trUi("The tactical travel map could not be prepared.")
                                     : gm::localization::trSystemMessage(tactical.errors.front());
            return false;
        }
        for (const CharacterModel &character : _currentCampaign.characters)
        {
            EncounterParticipantModel participant;
            participant.kind = CombatantKind::Player;
            participant.sourceEntityId = character.metadata.id;
            participant.displayName = character.nameBuffer;
            participant.currentHitPoints = std::max(1, character.currentHitPoints);
            participant.maxHitPoints = std::max(participant.currentHitPoints, character.maxHitPoints);
            participant.armorClass = 10 + characterAbilityModifier(character, "Dexterity");
            participant.speedFeet = character.speedFeet;
            participant.friendly = true;
            encounter.participants.push_back(std::move(participant));
        }
        normalizeEncounter(encounter);
        destinationMapId = tactical.tacticalMap.metadata.id;
        _currentCampaign.maps.push_back(tactical.tacticalMap);
        acceptedEncounterId = encounter.metadata.id;
        _currentCampaign.encounters.push_back(std::move(encounter));
    }

    if (!TravelSession::stageAcceptedScene(_currentCampaign, _travelSceneSessionId, selected,
                                           destinationMapId, acceptedEncounterId, _travelSceneStatus))
    {
        transaction.rollback();
        return false;
    }
    if (_travelSceneSessionId.empty())
    {
        _currentCampaign.partyLocation.mapId = sceneMapId;
        _currentCampaign.partyLocation.x = selected.localMap.width / 2;
        _currentCampaign.partyLocation.y = selected.localMap.height / 2;
    }
    if (!transaction.persistOrRollback([this]() { return saveCampaign(); }))
    {
        ++_mapRenderContentVersion;
        _travelSceneStatus = _atlasStatusMessage;
        return false;
    }

    // Keep every generated option for retry until persistence succeeds. Only
    // then switch maps and retire the accepted batch.
    const bool selectedDestination = selectMapById(destinationMapId);
    _travelSceneCandidates.clear();
    _selectedTravelSceneCandidate = -1;
    _travelSceneStatus = gm::localization::trUi(
        prepareEncounter
            ? "Travel scene and tactical encounter were added to the atlas."
            : "The travel scene was added to the atlas.");
    if (!_travelSceneSessionId.empty())
        _travelSceneStatus += gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
            ? " Als geplante Szene an die laufende Sitzung angehängt; aktive Szene unverändert."
            : " Appended to the live session plan; the active scene is unchanged.";
    return selectedDestination;
}
