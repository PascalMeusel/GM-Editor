#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Map/MapStructuredCreation.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr char separator = '\x1f';

bool require(bool condition, const std::string &message)
{
    if (condition)
        return true;
    std::cerr << message << '\n';
    return false;
}

std::string record(const std::vector<std::string> &fields)
{
    std::ostringstream stream;
    for (std::size_t index = 0; index < fields.size(); ++index)
    {
        if (index != 0)
            stream << separator;
        stream << fields[index];
    }
    return stream.str();
}

std::string legacyGeneration(MapGenerationPurpose purpose)
{
    return record({"M", "1", "0", "2", "legacy_recipe", "fantasy_neutral",
                   std::to_string(static_cast<int>(purpose)), "0", "42", "3", "4", "2", "1"});
}

std::string legacyAtlas(MapLevel level, MapKind kind)
{
    return record({"A", "1", std::to_string(static_cast<int>(level)),
                   std::to_string(static_cast<int>(kind)), "0", "0", "7", "",
                   "0", "0", "1", "1", "0", "0"});
}

bool optionalPlayerMapContentIsHidden(const MapModel &map)
{
    return !map.playerRelease.politicalBorders &&
           !map.playerRelease.waysAndRoutes &&
           !map.playerRelease.placesAndMarkers &&
           !map.playerRelease.labels &&
           !map.playerRelease.tokensAndAnnotations;
}
}

int main()
{
    using namespace PersistenceSerialization;
    bool ok = true;

    {
        Campaign campaign;
        WorldPlaceModel place;place.metadata.id="qa-lantern-fair";place.name="Lantern Fair";
        campaign.worldPlaces.push_back(place);
        MapGenerationMetadata metadata;
        MapGenerationFact fact;fact.id="brief_city_0";fact.type=MapGenerationFactType::Settlement;
        fact.sourceText="map_setup:existing";fact.label="Lantern Fair";fact.confirmed=true;
        for(const auto &refs:std::vector<std::vector<std::string>>{
            {place.metadata.id,"","brief_city_0"},{place.metadata.id,"",""},{place.metadata.id,""},{place.metadata.id}})
        {
            fact.entityRefs=refs;metadata.recipe.facts={fact};
            const auto restored=deserializeMapGenerationMetadata(serializeMapGenerationMetadata(metadata));
            ok &= require(restored.recipe.facts.size()==1 && restored.recipe.facts.front().entityRefs==refs,
                "Empty optional nation and trailing feature slots must survive the real database codec");
            ok &= require(mapSetupReferencesValid(campaign,restored.recipe.facts),
                "Loading a saved city without a nation must not make subsequent saves reject its feature ID as a nation");
        }
    }

    {
        Campaign campaign;
        WorldPlaceModel place;place.metadata.id="qa-lantern-fair";campaign.worldPlaces.push_back(place);
        MapModel map;
        MapGenerationFact fact;fact.id="brief_city_0";fact.type=MapGenerationFactType::Settlement;
        fact.sourceText="map_setup:existing";fact.entityRefs={place.metadata.id,"brief_city_0"};
        map.generation.recipe.facts={fact};
        MapGeneratedFeature feature;feature.id="brief_city_0";feature.kind=MapGeneratedFeatureKind::Settlement;feature.points={{2,3}};
        map.generation.graph.features={feature};
        const auto collapsed=map;
        ok &= require(repairCollapsedMapSetupFeatureReferences(campaign,map) &&
            map.generation.recipe.facts.front().entityRefs==std::vector<std::string>{place.metadata.id,"","brief_city_0"} &&
            mapSetupReferencesValid(campaign,map.generation.recipe.facts),
            "A provably collapsed owned city-feature binding must regain its optional nation slot");
        ok &= require(!repairCollapsedMapSetupFeatureReferences(campaign,map),"Binding repair must be idempotent");
        map=collapsed;map.generation.recipe.facts.front().entityRefs[1]="missing-nation";
        ok &= require(!repairCollapsedMapSetupFeatureReferences(campaign,map) && !mapSetupReferencesValid(campaign,map.generation.recipe.facts),
            "Unknown genuine nation references must remain invalid rather than being guessed or created");
        map=collapsed;map.generation.graph.features.front().kind=MapGeneratedFeatureKind::Road;
        ok &= require(!repairCollapsedMapSetupFeatureReferences(campaign,map),"Wrong-kind local geometry must not justify binding repair");
        map=collapsed;map.generation.graph.features.push_back(feature);
        ok &= require(!repairCollapsedMapSetupFeatureReferences(campaign,map),"Ambiguous duplicate local geometry IDs must not be repaired");
        map=collapsed;NationModel nation;nation.metadata.id="brief_city_0";campaign.nations.push_back(nation);
        ok &= require(!repairCollapsedMapSetupFeatureReferences(campaign,map) && map.generation.recipe.facts.front().entityRefs==fact.entityRefs,
            "An actual nation ID must retain precedence even if it collides with local geometry");
        campaign.nations.clear();map=collapsed;
        MapGenerationFact pending;pending.id="new-nation";pending.type=MapGenerationFactType::Nation;pending.sourceText="map_setup:new";pending.entityRefs={"brief_city_0"};
        map.generation.recipe.facts.push_back(pending);
        ok &= require(!repairCollapsedMapSetupFeatureReferences(campaign,map),"A pending new nation relationship must not be mistaken for a collapsed feature slot");
        map=collapsed;map.generation.recipe.facts.front().sourceText="authored";
        ok &= require(!repairCollapsedMapSetupFeatureReferences(campaign,map),"Unrelated fact reference lists must not be migrated");
        map=collapsed;map.generation.recipe.facts.front().type=MapGenerationFactType::Nation;
        map.generation.graph.features.front().kind=MapGeneratedFeatureKind::TerrainRegion;
        map.generation.graph.features.front().materialId="fantasy_neutral.territory.0";
        ok &= require(repairCollapsedMapSetupFeatureReferences(campaign,map),"Owned nation territory geometry must support the same safe positional repair");
    }

    {
        MapModel map;
        map.scope = MapScope::World;
        map.width = 72;
        map.height = 48;
        map.cells.resize(17);
        deserializeMapPersistenceMetadata({}, {}, map);
        ok &= require(map.creationIntent == MapCreationIntent::WorldOverview &&
                          map.atlasLevel == MapLevel::World &&
                          map.sizePreset == MapSizePreset::Standard,
                      "A legacy world map should infer the canonical standard world contract");
        ok &= require(map.scope == MapScope::World && map.width == 72 && map.height == 48 && map.cells.size() == 17,
                      "Compatibility hydration must not resize or rewrite legacy map data");
    }

    {
        MapModel map;
        map.scope = MapScope::Local;
        map.width = 64;
        map.height = 48;
        deserializeMapPersistenceMetadata({}, {}, map);
        ok &= require(map.creationIntent == MapCreationIntent::TravelScene &&
                          map.atlasLevel == MapLevel::Site &&
                          map.sizePreset == MapSizePreset::Standard,
                      "A plain legacy local map must not inherit the default Region recipe as persisted evidence");
    }

    {
        MapModel map;
        map.scope = MapScope::Local;
        map.width = 64;
        map.height = 48;
        const std::string generation = legacyGeneration(MapGenerationPurpose::SettlementEnvirons);
        const std::string unchanged = generation;
        deserializeMapPersistenceMetadata(generation, {}, map);
        ok &= require(map.creationIntent == MapCreationIntent::Settlement &&
                          map.atlasLevel == MapLevel::Site &&
                          map.sizePreset == MapSizePreset::Standard,
                      "A legacy settlement purpose should infer a standard site map");
        ok &= require(map.generation.recipe.creationIntent == MapCreationIntent::Settlement &&
                          map.generation.recipe.sizePreset == MapSizePreset::Standard &&
                          map.generation.recipe.purpose == MapGenerationPurpose::SettlementEnvirons,
                      "Missing recipe contract fields should receive the same in-memory classification");
        ok &= require(generation == unchanged, "Legacy generation bytes must remain untouched while loading");
    }

    {
        MapModel map;
        map.scope = MapScope::World;
        map.width = 80;
        map.height = 60;
        const std::string generation = legacyGeneration(MapGenerationPurpose::TravelArea);
        const std::string atlas = legacyAtlas(MapLevel::Region, MapKind::Geographic);
        const std::string unchangedAtlas = atlas;
        map.playerRelease = {true, true, true, true, true};
        deserializeMapPersistenceMetadata(generation, atlas, map);
        ok &= require(map.creationIntent == MapCreationIntent::RegionExploration &&
                          map.atlasLevel == MapLevel::Region &&
                          map.sizePreset == MapSizePreset::Standard && map.revision == 7,
                      "A valid pre-V5 atlas classification should outrank an ambiguous old purpose");
        ok &= require(atlas == unchangedAtlas, "Legacy atlas bytes must remain untouched while loading");
        ok &= require(optionalPlayerMapContentIsHidden(map),
                      "A legacy atlas record inherited optional player publication consent");
    }

    {
        MapModel map;
        map.scope = MapScope::Local;
        map.width = 40;
        map.height = 30;
        deserializeMapPersistenceMetadata(
            legacyGeneration(MapGenerationPurpose::Wilderness),
            legacyAtlas(MapLevel::Site, MapKind::Encounter), map);
        ok &= require(map.creationIntent == MapCreationIntent::TacticalEncounter &&
                          map.atlasLevel == MapLevel::Tactical &&
                          map.sizePreset == MapSizePreset::Standard,
                      "Encounter kind should repair a stale site level to the canonical tactical contract");
    }

    {
        MapModel map;
        map.scope = MapScope::Local;
        map.width = 24;
        map.height = 18;
        map.cells.resize(9);
        deserializeMapPersistenceMetadata(legacyGeneration(MapGenerationPurpose::TravelArea), {}, map);
        ok &= require(map.creationIntent == MapCreationIntent::TravelScene &&
                          map.atlasLevel == MapLevel::Site &&
                          map.sizePreset == MapSizePreset::Custom,
                      "Historical 24x18 maps must remain custom rather than being silently resized");
        ok &= require(map.width == 24 && map.height == 18 && map.cells.size() == 9,
                      "Custom legacy dimensions and payload must be byte-shape preserving");
    }

    {
        MapModel source;
        source.scope = MapScope::Dungeon;
        source.atlasLevel = MapLevel::Site;
        source.mapKind = MapKind::Dungeon;
        source.creationIntent = MapCreationIntent::Cave;
        source.sizePreset = MapSizePreset::Small;
        source.width = 24;
        source.height = 18;
        source.generation.recipe.creationIntent = MapCreationIntent::Cave;
        source.generation.recipe.sizePreset = MapSizePreset::Small;
        source.playerRelease.politicalBorders = true;
        source.playerRelease.placesAndMarkers = true;
        source.playerRelease.tokensAndAnnotations = true;
        const std::string generation = serializeMapGenerationMetadata(source.generation);
        const std::string atlas = serializeMapAtlasMetadata(source);

        MapModel restored;
        restored.scope = source.scope;
        restored.width = source.width;
        restored.height = source.height;
        deserializeMapPersistenceMetadata(generation, atlas, restored);
        ok &= require(restored.creationIntent == MapCreationIntent::Cave &&
                          restored.atlasLevel == MapLevel::Site &&
                          restored.sizePreset == MapSizePreset::Small,
                      "Explicit V5 creation intent and preset must never be re-inferred from dimensions");
        ok &= require(restored.playerRelease.politicalBorders &&
                          !restored.playerRelease.waysAndRoutes &&
                          restored.playerRelease.placesAndMarkers &&
                          !restored.playerRelease.labels &&
                          restored.playerRelease.tokensAndAnnotations,
                      "Explicit per-map player release categories did not round trip");
    }

    {
        MapModel map;
        map.scope = MapScope::Local;
        map.width = 30;
        map.height = 22;
        map.revision = 91;
        map.atlasPlacement.parentId = "preserved-before-corruption";
        map.playerRelease = {true, true, true, true, true};
        std::string corrupt = legacyAtlas(MapLevel::Region, MapKind::Geographic);
        const std::size_t finalSeparator = corrupt.rfind(separator);
        corrupt.replace(finalSeparator + 1, std::string::npos, "not-a-number");
        deserializeMapPersistenceMetadata(legacyGeneration(MapGenerationPurpose::Wilderness), corrupt, map);
        ok &= require(map.creationIntent == MapCreationIntent::TacticalEncounter &&
                          map.atlasLevel == MapLevel::Tactical &&
                          map.sizePreset == MapSizePreset::Small,
                      "Corrupt additive atlas metadata should fall back to deterministic legacy inference");
        ok &= require(map.revision == 91 && map.atlasPlacement.parentId == "preserved-before-corruption",
                      "A corrupt atlas record must not partially mutate the map before fallback");
        ok &= require(optionalPlayerMapContentIsHidden(map),
                      "A corrupt atlas record retained unsafe player category consent");
    }

    {
        MapModel source;
        source.playerRelease = {true, true, true, true, true};
        const std::string current = serializeMapAtlasMetadata(source);

        MapModel truncated;
        truncated.playerRelease = {true, true, true, true, true};
        const std::size_t finalSeparator = current.rfind(separator);
        const std::string incomplete = current.substr(0, finalSeparator);
        ok &= require(!deserializeMapAtlasMetadata(incomplete, truncated) &&
                          optionalPlayerMapContentIsHidden(truncated),
                      "A truncated V4 consent record did not fail closed");

        MapModel invalidBoolean;
        invalidBoolean.playerRelease = {true, true, true, true, true};
        std::string malformed = current;
        malformed.replace(finalSeparator + 1, std::string::npos, "2");
        ok &= require(!deserializeMapAtlasMetadata(malformed, invalidBoolean) &&
                          optionalPlayerMapContentIsHidden(invalidBoolean),
                      "A non-Boolean V4 consent value granted player visibility");
    }

    for (MapCreationIntent intent : {MapCreationIntent::WorldOverview, MapCreationIntent::RegionExploration,
                                     MapCreationIntent::Settlement, MapCreationIntent::TravelScene,
                                     MapCreationIntent::Dungeon, MapCreationIntent::TacticalEncounter})
    {
        for (MapSizePreset preset : {MapSizePreset::Small, MapSizePreset::Standard, MapSizePreset::Large})
        {
            const MapDimensions dimensions = mapDimensionsForPreset(intent, preset);
            ok &= require(mapSizePresetFromDimensions(intent, dimensions.width, dimensions.height) == preset,
                          "Every published size preset must round-trip through dimension inference");
        }
    }

    if (!ok)
        return 1;
    std::cout << "Map legacy persistence compatibility tests passed.\n";
    return 0;
}
