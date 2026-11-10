#include "Models/Project/ContextLinkMetadata.hpp"
#include "Models/Map/MapTravelCreation.hpp"
#include "Models/Map/CurrentMapArtRecipe.hpp"
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

MapGenerationOptions MapCreatorController::buildGenerationOptions()
{
    synchronizeBriefFactDrafts();
    MapGenerationOptions options;
    // Every newly generated editor map uses the current production pipeline.
    // Legacy maps remain loadable; regeneration is the explicit conversion step.
    const bool overview = _draftMap.atlasLevel == MapLevel::World ||
                          _draftMap.atlasLevel == MapLevel::Region;
    options.generatorVersion = _draftMap.atlasLevel == MapLevel::World ? 11 : 12;
    options.recipeVersion = kCurrentMapIllustrationRecipe;
    if (!overview) { options.styleId = "illustrated_atlas"; options.styleVersion = 6; options.styleSnapshotId = "illustrated_atlas_v6"; }
    options.landform = _draftMap.generation.recipe.landform;
    if (overview)
    {
        // Loading and style-only edits preserve the stored recipe. Requesting
        // a new design explicitly uses the current generator/artwork version.
        options.styleId = _draftMap.generation.recipe.styleId;
        options.styleVersion = _draftMap.generation.recipe.styleVersion;
        options.styleSnapshotId = _draftMap.generation.recipe.styleSnapshotId;
    }
    options.scope = _draftMap.scope;
    options.width = _draftMap.width;
    options.height = _draftMap.height;
    options.scale = _draftMap.atlasLevel;
    options.creationIntent = _draftMap.creationIntent;
    options.sizePreset = _draftMap.sizePreset;
    options.contentLanguage = _currentCampaign.contentLanguage == "de" ? "de" : "en";
    options.nameCulture = _draftMap.generation.recipe.nameCulture.empty()
                              ? "neutral_fantasy"
                              : _draftMap.generation.recipe.nameCulture;
    options.seed = _generation.generatorSeed;
    options.waterLevel = _generation.generatorWaterLevel;
    options.mountainDensity = _generation.generatorMountainDensity;
    options.forestDensity = _generation.generatorForestDensity;
    options.settlementDensity = _generation.generatorSettlementDensity;
    options.routeDensity = _generation.generatorRouteDensity;
    options.tacticalDensity = _generation.generatorTacticalDensity;
    options.mountainPattern = static_cast<MapMountainPattern>(std::clamp(_generation.generatorMountainPatternIndex, 0, static_cast<int>(MapMountainPattern::IndividualPeaks)));
    options.roadNetwork = static_cast<MapRoadNetworkMode>(std::clamp(_generation.generatorRoadNetworkIndex, 0, static_cast<int>(MapRoadNetworkMode::Grid)));
    static constexpr int qualityAttempts[] = {12, 24, 48};
    options.qualityAttempts = qualityAttempts[std::clamp(_generation.generatorQualityIndex, 0, 2)];

    if (_draftMap.scope != MapScope::Dungeon)
    {
        const auto briefFactById = [&](const std::string &id) -> const MapGenerationFact * {
            const auto found = std::find_if(_generation.briefFacts.begin(), _generation.briefFacts.end(), [&](const MapGenerationFact &fact) {
                return fact.id == id;
            });
            return found == _generation.briefFacts.end() ? nullptr : &*found;
        };
        options.quality = static_cast<MapGenerationQualityLevel>(std::clamp(_generation.generatorQualityIndex, 0, 2));
        options.purpose = static_cast<MapGenerationPurpose>(std::clamp(_generation.generatorPurposeIndex, 0, 3));
        options.landscape = static_cast<MapLandscapeProfile>(std::clamp(_generation.generatorLandscapeIndex, 0, 4));
        options.requestedSettlements = std::clamp(_generation.generatorRequestedSettlements, 0, 12);
        options.requestedLandmarks = std::clamp(_generation.generatorRequestedLandmarks, 0, 16);
        options.edgeConnectionCount = std::clamp(_generation.generatorEdgeConnections, 0, 4);
        options.facts = _generation.briefFacts;
        options.facts.erase(std::remove_if(options.facts.begin(), options.facts.end(), [](const MapGenerationFact &fact) {
                                return fact.type == MapGenerationFactType::Nation ||
                                       fact.type == MapGenerationFactType::NationCount ||
                                       fact.type == MapGenerationFactType::Capital ||
                                       fact.type == MapGenerationFactType::Settlement;
                            }),
                            options.facts.end());
        const int namedNationCount = static_cast<int>(std::count_if(
            _generation.briefNationNames.begin(), _generation.briefNationNames.end(), [](const std::string &name) {
                return !name.empty();
            }));
        if (namedNationCount > 0)
        {
            MapGenerationFact countFact;
            const MapGenerationFact *draftFact = briefFactById("brief_nation_count");
            if (draftFact)
                countFact = *draftFact;
            countFact.id = "brief_nation_count";
            countFact.type = MapGenerationFactType::NationCount;
            if (!draftFact || countFact.strength == MapGenerationFactStrength::Automatic ||
                (countFact.label.empty() && countFact.value.empty()))
            {
                countFact.label = std::to_string(namedNationCount);
                countFact.value = countFact.label;
            }
            if (countFact.sourceText.empty()) countFact.sourceText = _generation.worldBrief;
            options.facts.push_back(std::move(countFact));
        }
        for (int nationIndex = 0; nationIndex < static_cast<int>(_generation.briefNationNames.size()); ++nationIndex)
        {
            if (_generation.briefNationNames[static_cast<std::size_t>(nationIndex)].empty())
                continue;
            MapGenerationFact fact;
            const std::string factId = "brief_nation_" + std::to_string(nationIndex);
            if (const MapGenerationFact *draftFact = briefFactById(factId))
                fact = *draftFact;
            fact.id = factId;
            fact.type = MapGenerationFactType::Nation;
            fact.label = _generation.briefNationNames[static_cast<std::size_t>(nationIndex)];
            fact.value = fact.label;
            if (fact.sourceText.empty()) fact.sourceText = _generation.worldBrief;
            const bool required = fact.strength == MapGenerationFactStrength::Must;
            options.facts.push_back(std::move(fact));
            options.nationNames.push_back(_generation.briefNationNames[static_cast<std::size_t>(nationIndex)]);
            if (required)
                options.requiredElements.push_back(_generation.briefNationNames[static_cast<std::size_t>(nationIndex)]);
        }
        for (MapGenerationFact &fact : options.facts)
            if (fact.type == MapGenerationFactType::SharedRiver)
            {
                fact.entityRefs.clear();
                std::copy_if(_generation.briefNationNames.begin(), _generation.briefNationNames.end(),
                             std::back_inserter(fact.entityRefs), [](const std::string &name) {
                                 return !name.empty();
                             });
            }
        for (const std::string &nationName : _generation.briefNationNames)
            if (!nationName.empty())
                options.knownFacts.push_back("Nation: " + nationName);
        if (!_generation.worldBrief.empty())
            options.knownFacts.push_back(_generation.worldBrief);
        if (!_generation.briefCapitalName.empty())
        {
            MapGenerationFact fact;
            if (const MapGenerationFact *draftFact = briefFactById("brief_capital"))
                fact = *draftFact;
            fact.id = "brief_capital";
            fact.type = MapGenerationFactType::Capital;
            fact.label = _generation.briefCapitalName;
            fact.value = _generation.briefCapitalName;
            if (fact.sourceText.empty()) fact.sourceText = _generation.worldBrief;
            const bool required = fact.strength == MapGenerationFactStrength::Must;
            options.facts.push_back(std::move(fact));
            if (required)
                options.requiredElements.push_back(_generation.briefCapitalName);
            MapGenerationConstraint capital;
            capital.id = "brief_capital";
            capital.label = _generation.briefCapitalName;
            capital.featureKind = MapGeneratedFeatureKind::Settlement;
            capital.x = static_cast<float>(_draftMap.width) * 0.52f;
            capital.y = static_cast<float>(_draftMap.height) * 0.48f;
            capital.required = required;
            capital.locked = required;
            options.constraints.push_back(std::move(capital));
            options.requestedSettlements = std::max(options.requestedSettlements, 1);
        }
        for (int cityIndex = 0; cityIndex < static_cast<int>(_generation.briefCityNames.size()); ++cityIndex)
        {
            const std::string &cityName = _generation.briefCityNames[static_cast<std::size_t>(cityIndex)];
            if (cityName.empty())
                continue;
            MapGenerationFact fact;
            const std::string factId = "brief_city_" + std::to_string(cityIndex);
            if (const MapGenerationFact *draftFact = briefFactById(factId))
                fact = *draftFact;
            fact.id = factId;
            fact.type = MapGenerationFactType::Settlement;
            fact.label = cityName;
            fact.value = cityName;
            if (fact.sourceText.empty()) fact.sourceText = _generation.worldBrief;
            const bool required = fact.strength == MapGenerationFactStrength::Must;
            options.facts.push_back(std::move(fact));
            if (required)
                options.requiredElements.push_back(cityName);
            constexpr float pi = 3.14159265358979323846f;
            const float angle = static_cast<float>(cityIndex) / static_cast<float>(std::max(1, static_cast<int>(_generation.briefCityNames.size()))) * pi * 2.0f + 0.7f;
            MapGenerationConstraint city;
            city.id = "brief_city_" + std::to_string(cityIndex);
            city.label = cityName;
            city.featureKind = MapGeneratedFeatureKind::Settlement;
            city.x = static_cast<float>(_draftMap.width) * (0.5f + std::cos(angle) * 0.28f);
            city.y = static_cast<float>(_draftMap.height) * (0.5f + std::sin(angle) * 0.25f);
            city.required = required;
            city.locked = required && !_generation.structuredCreation;
            options.constraints.push_back(std::move(city));
        }
        options.requestedSettlements = std::max(options.requestedSettlements,
                                                static_cast<int>(_generation.briefCityNames.size()) + (_generation.briefCapitalName.empty() ? 0 : 1));
        for (const MapGeneratedFeature &feature : _draftMap.generation.graph.features)
        {
            if (!feature.locked || feature.points.empty() ||
                (feature.kind != MapGeneratedFeatureKind::Settlement && feature.kind != MapGeneratedFeatureKind::Landmark))
                continue;
            const bool alreadyConstrained = std::any_of(options.constraints.begin(), options.constraints.end(), [&](const MapGenerationConstraint &constraint) {
                return constraint.id == feature.id ||
                       (constraint.featureKind == feature.kind && MapWorldBriefInterpreter::asciiLower(constraint.label) == MapWorldBriefInterpreter::asciiLower(feature.name));
            });
            if (alreadyConstrained)
                continue;
            options.lockedFeatureIds.push_back(feature.id);
            MapGenerationConstraint constraint;
            constraint.id = feature.id;
            constraint.label = feature.name;
            constraint.featureKind = feature.kind;
            constraint.x = feature.points.front().x;
            constraint.y = feature.points.front().y;
            constraint.required = true;
            constraint.locked = true;
            options.constraints.push_back(std::move(constraint));
        }
        for (const WorldPlaceModel &place : _currentCampaign.worldPlaces)
        {
            if (!place.hasMapPosition || place.linkedMapId != _draftMap.metadata.id)
                continue;
            const bool alreadyConstrained = std::any_of(options.constraints.begin(), options.constraints.end(), [&](const MapGenerationConstraint &constraint) {
                return constraint.id == place.metadata.id;
            });
            if (alreadyConstrained)
                continue;
            MapGenerationConstraint constraint;
            constraint.id = place.metadata.id;
            constraint.label = place.name;
            constraint.featureKind = place.type == WorldPlaceType::Landmark || place.type == WorldPlaceType::Site
                                         ? MapGeneratedFeatureKind::Landmark
                                         : MapGeneratedFeatureKind::Settlement;
            constraint.x = static_cast<float>(normalizedWorldCoordinate(place.mapPosition.x) * std::max(1, _draftMap.width - 1));
            constraint.y = static_cast<float>(normalizedWorldCoordinate(place.mapPosition.y) * std::max(1, _draftMap.height - 1));
            constraint.required = true;
            constraint.locked = true;
            options.constraints.push_back(std::move(constraint));
        }
        options.roadNetwork = options.purpose == MapGenerationPurpose::Wilderness ? MapRoadNetworkMode::Sparse : MapRoadNetworkMode::Connected;
        if (_generation.structuredCreation) return options;
        switch (options.landscape)
        {
        case MapLandscapeProfile::RiverValley:
            options.waterLevel = std::max(options.waterLevel, 38);
            options.mountainDensity = std::min(options.mountainDensity, 42);
            break;
        case MapLandscapeProfile::ForestFrontier:
            options.forestDensity = std::max(options.forestDensity, 58);
            options.waterLevel = std::max(options.waterLevel, 24);
            break;
        case MapLandscapeProfile::Highlands:
            options.mountainDensity = std::max(options.mountainDensity, 42);
            options.waterLevel = std::min(options.waterLevel, 30);
            break;
        case MapLandscapeProfile::MountainPass:
            options.mountainDensity = std::max(options.mountainDensity, 68);
            options.mountainPattern = MapMountainPattern::Ranges;
            break;
        case MapLandscapeProfile::LakeCountry:
            options.waterLevel = std::max(options.waterLevel, 68);
            options.forestDensity = std::max(options.forestDensity, 38);
            break;
        }
        return options;
    }

    const auto preset = static_cast<GeneratorPreset>(std::clamp(_generation.selectedGeneratorIndex, 0, 4));
    if (preset == GeneratorPreset::City)
    {
        options.mountainDensity = 0;
        options.forestDensity = std::min(options.forestDensity, 10);
        options.settlementDensity = std::max(options.settlementDensity, 34);
        options.roadNetwork = MapRoadNetworkMode::Grid;
    }
    else if (preset == GeneratorPreset::Forest)
    {
        options.forestDensity = std::max(options.forestDensity, 45);
        options.waterLevel = std::max(options.waterLevel, 24);
    }
    else if (preset == GeneratorPreset::MountainArea)
    {
        options.mountainDensity = std::max(options.mountainDensity, 52);
        options.mountainPattern = options.mountainPattern == MapMountainPattern::Mixed ? MapMountainPattern::Ranges : options.mountainPattern;
    }
    else if (preset == GeneratorPreset::Dungeon)
    {
        options.scope = MapScope::Dungeon;
    }
    else if (preset == GeneratorPreset::Cave)
    {
        options.scope = MapScope::Dungeon;
        options.caveNetwork = true;
        options.roadNetwork = MapRoadNetworkMode::Connected;
    }

    return options;
}
