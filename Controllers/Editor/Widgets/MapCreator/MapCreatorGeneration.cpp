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

void MapCreatorController::interpretWorldBrief()
{
    markDraftEdited();
    const MapWorldBrief brief = MapWorldBriefInterpreter::interpret(_generation.worldBrief);
    _generation.briefNationNames = brief.nationNames;
    _generation.briefCityNames = brief.cityNames;
    _generation.briefFacts = brief.facts;
    while (static_cast<int>(_generation.briefNationNames.size()) < brief.statedNationCount)
        _generation.briefNationNames.push_back(gm::localization::trUi("Nation") + std::string(" ") + std::to_string(_generation.briefNationNames.size() + 1));
    _generation.briefCapitalName = brief.capitalName;
    _generation.worldBriefParsed = true;
    _generation.generatorRequestedSettlements = std::max(_generation.generatorRequestedSettlements, static_cast<int>(_generation.briefCityNames.size()) + (_generation.briefCapitalName.empty() ? 0 : 1));
    if (_draftMap.name.empty() || _draftMap.name == "New Map" || _draftMap.name == gm::localization::trUi("New Map"))
        _draftMap.name = gm::localization::trUi("World map");
    if (_draftMap.description.empty() && !_generation.worldBrief.empty())
        _draftMap.description = _generation.worldBrief;
    _integration.statusMessage = !brief.unrecognizedText.empty()
                         ? gm::localization::trUi("Some notes need review. Check recognised facts and unresolved notes before generating.")
                         : brief.hasFacts()
                         ? gm::localization::trUi("Known world facts are ready. Review them or create designs now.")
                         : gm::localization::trUi("No named facts were found. You can still create a world and adjust it afterward.");
}

void MapCreatorController::synchronizeStructuredSetup()
{
    _generation.structuredCreation = true;
    _generation.worldBriefParsed = true;
    _generation.worldBrief.clear();
    _generation.briefCapitalName.clear();
    _generation.briefNationNames.clear();
    _generation.briefCityNames.clear();
    for (const auto &entity : _generation.setupEntities)
        (entity.nation ? _generation.briefNationNames : _generation.briefCityNames).push_back(entity.name);
    _generation.briefFacts = mapSetupFacts(_generation.setupEntities);
    synchronizeBriefFactDrafts();
    _generation.generatorRequestedSettlements = std::max(_generation.generatorRequestedSettlements, static_cast<int>(_generation.briefCityNames.size()));
    _draftMap.generation.recipe.landform = _draftMap.generation.recipe.landform == MapLandform::Automatic
        ? MapLandform::Continental : _draftMap.generation.recipe.landform;
    if (_draftMap.creationIntent == MapCreationIntent::TravelScene)
    {
        auto &recipe = _draftMap.generation.recipe;
        recipe.travelScenePurpose = std::clamp(_generation.travelPurposeIndex, 0, 4);
        recipe.travelSceneBiome = kStandaloneTravelBiomes[static_cast<std::size_t>(std::clamp(_generation.travelBiomeIndex, 0, 6))];
        recipe.travelSceneDangerLevel = std::clamp(_generation.travelDangerLevel, 0, 5);
        recipe.travelSceneCombatReady = _generation.travelCombatReady;
        recipe.travelSceneSeed = _generation.generatorSeed;
    }
    _draftMap.generation.recipe.waterLevel = _generation.generatorWaterLevel;
    _draftMap.generation.recipe.mountainDensity = _generation.generatorMountainDensity;
    _draftMap.generation.recipe.forestDensity = _generation.generatorForestDensity;
    _draftMap.generation.recipe.settlementDensity = _generation.generatorSettlementDensity;
    _draftMap.generation.recipe.routeDensity = _generation.generatorRouteDensity;
    _draftMap.generation.recipe.tacticalDensity = _generation.generatorTacticalDensity;
    _draftMap.generation.recipe.landscape = static_cast<MapLandscapeProfile>(_generation.generatorLandscapeIndex);
    _draftMap.generation.recipe.purpose = static_cast<MapGenerationPurpose>(_generation.generatorPurposeIndex);
    _draftMap.generation.recipe.settlementCount = _generation.generatorRequestedSettlements;
    _draftMap.generation.recipe.landmarkCount = _generation.generatorRequestedLandmarks;
    _draftMap.generation.recipe.seed = _generation.generatorSeed;
    _history.crashJournalPending = true;
}

void MapCreatorController::synchronizeBriefFactDrafts()
{
    std::vector<std::string> managedIds;
    const auto upsert = [&](const std::string &id, MapGenerationFactType type,
                            const std::string &value, MapGenerationFactStrength defaultStrength,
                            bool preserveExplicitValue = false) {
        managedIds.push_back(id);
        const auto existing = std::find_if(_generation.briefFacts.begin(), _generation.briefFacts.end(), [&](const MapGenerationFact &fact) {
            return fact.id == id;
        });
        if (existing != _generation.briefFacts.end())
        {
            existing->type = type;
            if (!preserveExplicitValue || existing->strength == MapGenerationFactStrength::Automatic ||
                (existing->label.empty() && existing->value.empty()))
            {
                existing->label = value;
                existing->value = value;
            }
            if (existing->sourceText.empty())
                existing->sourceText = _generation.worldBrief;
            return;
        }
        MapGenerationFact fact;
        fact.id = id;
        fact.type = type;
        fact.strength = defaultStrength;
        fact.label = value;
        fact.value = value;
        fact.sourceText = _generation.worldBrief;
        fact.confidence = 1.0f;
        fact.confirmed = true;
        _generation.briefFacts.push_back(std::move(fact));
    };

    const int namedNationCount = static_cast<int>(std::count_if(
        _generation.briefNationNames.begin(), _generation.briefNationNames.end(), [](const std::string &name) { return !name.empty(); }));
    if (namedNationCount > 0)
        upsert("brief_nation_count", MapGenerationFactType::NationCount,
               std::to_string(namedNationCount), MapGenerationFactStrength::Must, true);
    for (int nationIndex = 0; nationIndex < static_cast<int>(_generation.briefNationNames.size()); ++nationIndex)
    {
        const std::string &name = _generation.briefNationNames[static_cast<std::size_t>(nationIndex)];
        if (!name.empty())
            upsert("brief_nation_" + std::to_string(nationIndex), MapGenerationFactType::Nation,
                   name, MapGenerationFactStrength::Must);
    }
    if (!_generation.briefCapitalName.empty())
        upsert("brief_capital", MapGenerationFactType::Capital, _generation.briefCapitalName,
               MapGenerationFactStrength::Must);
    for (int cityIndex = 0; cityIndex < static_cast<int>(_generation.briefCityNames.size()); ++cityIndex)
    {
        const std::string &name = _generation.briefCityNames[static_cast<std::size_t>(cityIndex)];
        if (!name.empty())
            upsert("brief_city_" + std::to_string(cityIndex), MapGenerationFactType::Settlement,
                   name, MapGenerationFactStrength::Must);
    }

    const auto isManagedNamedFact = [](const MapGenerationFact &fact) {
        return fact.id == "brief_nation_count" || fact.id == "brief_capital" ||
               fact.id.rfind("brief_nation_", 0) == 0 || fact.id.rfind("brief_city_", 0) == 0;
    };
    _generation.briefFacts.erase(std::remove_if(_generation.briefFacts.begin(), _generation.briefFacts.end(), [&](const MapGenerationFact &fact) {
                         return isManagedNamedFact(fact) &&
                                std::find(managedIds.begin(), managedIds.end(), fact.id) == managedIds.end();
                     }),
                     _generation.briefFacts.end());

    for (MapGenerationFact &fact : _generation.briefFacts)
        if (fact.type == MapGenerationFactType::SharedRiver)
        {
            fact.entityRefs.clear();
            std::copy_if(_generation.briefNationNames.begin(), _generation.briefNationNames.end(),
                         std::back_inserter(fact.entityRefs), [](const std::string &name) { return !name.empty(); });
        }
    _draftMap.generation.recipe.facts = _generation.briefFacts;
}

void MapCreatorController::setBriefFactStrength(std::size_t factIndex, MapGenerationFactStrength strength)
{
    synchronizeBriefFactDrafts();
    if (factIndex >= _generation.briefFacts.size() || _generation.briefFacts[factIndex].strength == strength)
        return;
    _generation.briefFacts[factIndex].strength = strength;
    _draftMap.generation.recipe.facts = _generation.briefFacts;
    invalidateGenerationRequest();
    _integration.statusMessage = gm::localization::trUi("Fact priority updated. Generate new designs to apply it.");
}

void MapCreatorController::setBriefFactConfirmed(std::size_t factIndex, bool confirmed)
{
    synchronizeBriefFactDrafts();
    if (factIndex >= _generation.briefFacts.size() || _generation.briefFacts[factIndex].confirmed == confirmed)
        return;
    _generation.briefFacts[factIndex].confirmed = confirmed;
    _draftMap.generation.recipe.facts = _generation.briefFacts;
    invalidateGenerationRequest();
    _integration.statusMessage = confirmed
                         ? gm::localization::trUi("Fact interpretation confirmed.")
                         : gm::localization::trUi("Fact marked for review.");
}

void MapCreatorController::makeUnrecognizedBriefNotesOptional()
{
    synchronizeBriefFactDrafts();
    bool changed = false;
    for (MapGenerationFact &fact : _generation.briefFacts)
        if (fact.type == MapGenerationFactType::Note && fact.strength == MapGenerationFactStrength::Must)
        {
            fact.strength = MapGenerationFactStrength::Automatic;
            fact.confirmed = true;
            changed = true;
        }
    if (!changed)
        return;
    _draftMap.generation.recipe.facts = _generation.briefFacts;
    invalidateGenerationRequest();
    _integration.statusMessage = gm::localization::trUi("Unrecognized required notes are now kept as optional notes.");
}

MapGenerationValidationReport MapCreatorController::validateDraftGenerationRequest()
{
    synchronizeBriefFactDrafts();
    MapGenerationOptions options = buildGenerationOptions();
    mapGenApplyFactsToOptions(options);
    return validateMapGenerationRequest(options);
}

void MapCreatorController::synchronizeWorldBriefEntities()
{
    if (_generation.structuredCreation)
    {
        applyMapSetupEntities(_currentCampaign, _draftMap);
        return;
    }
    if (!_generation.worldBriefParsed || _draftMap.atlasLevel != MapLevel::World)
        return;

    const auto sameName = [](const std::string &left, const std::string &right) {
        return MapWorldBriefInterpreter::asciiLower(left) == MapWorldBriefInterpreter::asciiLower(right);
    };
    for (const std::string &nationName : _generation.briefNationNames)
    {
        if (nationName.empty())
            continue;
        auto nationIt = std::find_if(_currentCampaign.nations.begin(), _currentCampaign.nations.end(), [&](const NationModel &nation) {
            return sameName(nation.name, nationName);
        });
        if (nationIt == _currentCampaign.nations.end())
        {
            NationModel nation;
            nation.name = nationName;
            _currentCampaign.nations.push_back(std::move(nation));
            nationIt = std::prev(_currentCampaign.nations.end());
        }
        const auto featureIt = std::find_if(_draftMap.generation.graph.features.begin(), _draftMap.generation.graph.features.end(), [&](const MapGeneratedFeature &feature) {
            return feature.kind == MapGeneratedFeatureKind::TerrainRegion && feature.materialId.find("territory.") != std::string::npos && sameName(feature.name, nationName);
        });
        if (featureIt == _draftMap.generation.graph.features.end() || featureIt->points.size() < 3)
            continue;
        featureIt->gmOnly = nationIt->visibility == WorldObjectVisibility::GmOnly;
        featureIt->discovered = nationIt->visibility != WorldObjectVisibility::GmOnly;
        const std::string generatedTerritoryName = _draftMap.name + " (" + gm::localization::trUi("generated") + ")";
        auto territoryIt = std::find_if(nationIt->territories.begin(), nationIt->territories.end(), [&](const NationTerritoryModel &territory) {
            return territory.name == generatedTerritoryName;
        });
        if (territoryIt == nationIt->territories.end())
        {
            NationTerritoryModel territory;
            territory.name = generatedTerritoryName;
            nationIt->territories.push_back(std::move(territory));
            territoryIt = std::prev(nationIt->territories.end());
        }
        territoryIt->polygon.clear();
        for (const MapFeaturePoint &point : featureIt->points)
            territoryIt->polygon.push_back({point.x / std::max(1, _draftMap.width), point.y / std::max(1, _draftMap.height)});
        normalizeNationTerritory(*territoryIt);
        nationIt->metadata.touch();
    }

    if (!_generation.briefCapitalName.empty())
    {
        const auto featureIt = std::find_if(_draftMap.generation.graph.features.begin(), _draftMap.generation.graph.features.end(), [&](const MapGeneratedFeature &feature) {
            return feature.kind == MapGeneratedFeatureKind::Settlement && sameName(feature.name, _generation.briefCapitalName) && !feature.points.empty();
        });
        if (featureIt != _draftMap.generation.graph.features.end())
        {
            auto placeIt = std::find_if(_currentCampaign.worldPlaces.begin(), _currentCampaign.worldPlaces.end(), [&](const WorldPlaceModel &place) {
                return sameName(place.name, _generation.briefCapitalName);
            });
            if (placeIt == _currentCampaign.worldPlaces.end())
            {
                WorldPlaceModel place;
                place.name = _generation.briefCapitalName;
                _currentCampaign.worldPlaces.push_back(std::move(place));
                placeIt = std::prev(_currentCampaign.worldPlaces.end());
            }
            placeIt->type = WorldPlaceType::Capital;
            placeIt->linkedMapId = _draftMap.metadata.id;
            placeIt->linkedFeatureId = featureIt->id;
            placeIt->hasMapPosition = true;
            placeIt->mapPosition = {
                featureIt->points.front().x / std::max(1, _draftMap.width),
                featureIt->points.front().y / std::max(1, _draftMap.height)};
            normalizeWorldPlacePosition(*placeIt);
            featureIt->gmOnly = placeIt->visibility == WorldObjectVisibility::GmOnly;
            featureIt->discovered = placeIt->visibility != WorldObjectVisibility::GmOnly;
            placeIt->metadata.touch();
            if (_generation.briefNationNames.size() == 1)
            {
                auto nationIt = std::find_if(_currentCampaign.nations.begin(), _currentCampaign.nations.end(), [&](const NationModel &nation) {
                    return sameName(nation.name, _generation.briefNationNames.front());
                });
                if (nationIt != _currentCampaign.nations.end())
                {
                    placeIt->nationId = nationIt->metadata.id;
                    nationIt->capitalPlaceId = placeIt->metadata.id;
                }
            }
        }
    }

    for (const std::string &cityName : _generation.briefCityNames)
    {
        if (cityName.empty())
            continue;
        const auto featureIt = std::find_if(_draftMap.generation.graph.features.begin(), _draftMap.generation.graph.features.end(), [&](const MapGeneratedFeature &feature) {
            return feature.kind == MapGeneratedFeatureKind::Settlement && sameName(feature.name, cityName) && !feature.points.empty();
        });
        if (featureIt == _draftMap.generation.graph.features.end())
            continue;
        auto placeIt = std::find_if(_currentCampaign.worldPlaces.begin(), _currentCampaign.worldPlaces.end(), [&](const WorldPlaceModel &place) {
            return sameName(place.name, cityName);
        });
        if (placeIt == _currentCampaign.worldPlaces.end())
        {
            WorldPlaceModel place;
            place.name = cityName;
            _currentCampaign.worldPlaces.push_back(std::move(place));
            placeIt = std::prev(_currentCampaign.worldPlaces.end());
        }
        placeIt->type = WorldPlaceType::City;
        placeIt->linkedMapId = _draftMap.metadata.id;
        placeIt->linkedFeatureId = featureIt->id;
        placeIt->hasMapPosition = true;
        placeIt->mapPosition = {
            featureIt->points.front().x / std::max(1, _draftMap.width),
            featureIt->points.front().y / std::max(1, _draftMap.height)};
        normalizeWorldPlacePosition(*placeIt);
        featureIt->gmOnly = placeIt->visibility == WorldObjectVisibility::GmOnly;
        featureIt->discovered = placeIt->visibility != WorldObjectVisibility::GmOnly;
        placeIt->metadata.touch();
    }
}
