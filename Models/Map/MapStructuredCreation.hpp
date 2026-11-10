#pragma once
#include "Models/Campaign/Campaign.hpp"
#include <algorithm>
#include <cctype>

struct MapSetupEntity
{
    EntityId id;
    std::string name;
    bool nation = false;
    bool existing = false;
    EntityId nationId;
    // Local generated geometry identity; campaign references occupy slots 0/1.
    std::string featureId;
};

inline std::string mapSetupNameKey(std::string name)
{
    const auto first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    name = name.substr(first, name.find_last_not_of(" \t\r\n") - first + 1);
    for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return name;
}

inline bool mapSetupEntityFact(const MapGenerationFact &fact)
{
    return (fact.sourceText == "map_setup:new" || fact.sourceText == "map_setup:existing") &&
        (fact.type == MapGenerationFactType::Nation || fact.type == MapGenerationFactType::Settlement);
}

inline std::vector<MapSetupEntity> mapSetupEntities(const std::vector<MapGenerationFact> &facts)
{
    std::vector<MapSetupEntity> result;
    for (const auto &fact : facts)
        if (mapSetupEntityFact(fact) && !fact.entityRefs.empty())
            result.push_back({fact.entityRefs.front(), fact.label,
                fact.type == MapGenerationFactType::Nation, fact.sourceText == "map_setup:existing",
                fact.entityRefs.size() > 1 ? fact.entityRefs[1] : EntityId{},
                fact.entityRefs.size() > 2 ? fact.entityRefs[2] : std::string{}});
    return result;
}

inline std::vector<MapGenerationFact> mapSetupFacts(const std::vector<MapSetupEntity> &entities)
{
    std::vector<MapGenerationFact> facts;
    int nations = 0, cities = 0;
    for (const auto &entity : entities)
    {
        MapGenerationFact fact;
        fact.id = entity.nation ? "brief_nation_" + std::to_string(nations++) : "brief_city_" + std::to_string(cities++);
        fact.type = entity.nation ? MapGenerationFactType::Nation : MapGenerationFactType::Settlement;
        fact.strength = MapGenerationFactStrength::Must;
        fact.label = fact.value = entity.name;
        fact.entityRefs = {entity.id};
        if (!entity.nationId.empty()) fact.entityRefs.push_back(entity.nationId);
        if (!entity.featureId.empty()) { fact.entityRefs.resize(3); fact.entityRefs[2] = entity.featureId; }
        fact.sourceText = entity.existing ? "map_setup:existing" : "map_setup:new";
        fact.confirmed = true;
        facts.push_back(std::move(fact));
    }
    return facts;
}

inline bool mapSetupFactMatchesFeature(const MapGenerationFact &fact, const MapGeneratedFeature &feature)
{
    if (!mapSetupEntityFact(fact) || fact.entityRefs.empty()) return false;
    const bool nation = fact.type == MapGenerationFactType::Nation;
    const bool correctKind = nation ? (feature.kind == MapGeneratedFeatureKind::Nation ||
        (feature.kind == MapGeneratedFeatureKind::TerrainRegion && feature.materialId.find("territory.") != std::string::npos))
        : feature.kind == MapGeneratedFeatureKind::Settlement;
    if (!correctKind) return false;
    if (fact.entityRefs.size() > 2 && !fact.entityRefs[2].empty()) return fact.entityRefs[2] == feature.id;
    return mapSetupNameKey(fact.label) == mapSetupNameKey(feature.name);
}

struct MapSetupBinding { bool present = false; EntityId entityId; };
inline MapSetupBinding mapSetupBindingForFeature(const MapModel &map, const MapGeneratedFeature &feature)
{
    MapSetupBinding result;
    for (const auto &fact : map.generation.recipe.facts)
        if (mapSetupFactMatchesFeature(fact, feature))
        {
            if (result.present) return {true, {}}; // Ambiguous references never grant player visibility.
            result = {true, fact.entityRefs.front()};
        }
    return result;
}

// Older database readers collapsed [entity, "", feature] to [entity,
// feature]. Repair only a provable owned geometry binding, never an unknown
// nation reference or a reference that still names a real/pending nation.
inline bool repairCollapsedMapSetupFeatureReferences(const Campaign &campaign, MapModel &map)
{
    bool repaired=false;
    for(auto &fact:map.generation.recipe.facts)
    {
        if(!mapSetupEntityFact(fact) || fact.entityRefs.size()!=2 || fact.entityRefs[1].empty()) continue;
        const auto candidate=fact.entityRefs[1];
        if(std::any_of(campaign.nations.begin(),campaign.nations.end(),[&](const auto &n){return n.metadata.id==candidate;}) ||
           std::any_of(map.generation.recipe.facts.begin(),map.generation.recipe.facts.end(),[&](const auto &n){
               return mapSetupEntityFact(n) && n.type==MapGenerationFactType::Nation &&
                   !n.entityRefs.empty() && n.entityRefs.front()==candidate;
           })) continue;
        const bool nation=fact.type==MapGenerationFactType::Nation;
        const auto matches=std::count_if(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &f){
            const bool correctKind=nation ? (f.kind==MapGeneratedFeatureKind::Nation ||
                (f.kind==MapGeneratedFeatureKind::TerrainRegion && f.materialId.find("territory.")!=std::string::npos)) :
                f.kind==MapGeneratedFeatureKind::Settlement;
            return f.id==candidate && correctKind && !f.points.empty();
        });
        if(matches!=1) continue;
        fact.entityRefs={fact.entityRefs.front(),{},candidate};
        repaired=true;
    }
    return repaired;
}

// Validate before mutating the campaign; an existing selection must not turn
// into a newly created entity when another editor removed it.
inline bool mapSetupReferencesValid(const Campaign &campaign, const std::vector<MapGenerationFact> &facts)
{
    const auto entities = mapSetupEntities(facts);
    for (const auto &entity : entities)
    {
        if (entity.existing)
        {
            const bool found = entity.nation
                ? std::any_of(campaign.nations.begin(), campaign.nations.end(), [&](const auto &n) { return n.metadata.id == entity.id; })
                : std::any_of(campaign.worldPlaces.begin(), campaign.worldPlaces.end(), [&](const auto &p) { return p.metadata.id == entity.id; });
            if (!found) return false;
        }
        if (!entity.nation && !entity.nationId.empty())
        {
            const bool knownNation = std::any_of(campaign.nations.begin(), campaign.nations.end(),
                [&](const auto &n) { return n.metadata.id == entity.nationId; });
            const bool newNation = std::any_of(entities.begin(), entities.end(),
                [&](const auto &n) { return n.nation && !n.existing && n.id == entity.nationId; });
            if (!knownNation && !newNation) return false;
        }
    }
    return true;
}

inline bool applyMapSetupEntities(Campaign &campaign, MapModel &map)
{
    if (!mapSetupReferencesValid(campaign, map.generation.recipe.facts)) return false;
    for (auto &fact : map.generation.recipe.facts)
    {
        if (!mapSetupEntityFact(fact) || fact.entityRefs.empty()) continue;
        const bool nation = fact.type == MapGenerationFactType::Nation;
        const auto feature = std::find_if(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &f) {
            return mapSetupFactMatchesFeature(fact, f) && !f.points.empty();
        });
        // Deleted geometry does not delete its underlying campaign entity.
        if (feature == map.generation.graph.features.end()) continue;
        const auto &id = fact.entityRefs.front();
        if (nation)
        {
            auto entry=std::find_if(campaign.nations.begin(),campaign.nations.end(),[&](const auto &n){return n.metadata.id==id;});
            if(entry==campaign.nations.end()) { NationModel n; n.metadata.id=id; n.name=fact.label; campaign.nations.push_back(std::move(n)); entry=std::prev(campaign.nations.end()); }
            feature->gmOnly=entry->visibility==WorldObjectVisibility::GmOnly;
            feature->discovered=!feature->gmOnly;
            const auto territoryId=id+"/map/"+map.metadata.id;
            auto territory=std::find_if(entry->territories.begin(),entry->territories.end(),[&](const auto &t){return t.metadata.id==territoryId;});
            if(territory==entry->territories.end()) { NationTerritoryModel t; t.metadata.id=territoryId; entry->territories.push_back(std::move(t)); territory=std::prev(entry->territories.end()); }
            territory->name=map.name; territory->polygon.clear();
            for(const auto &p:feature->points) territory->polygon.push_back({p.x/std::max(1,map.width),p.y/std::max(1,map.height)});
            normalizeNationTerritory(*territory);
        }
        else
        {
            auto entry=std::find_if(campaign.worldPlaces.begin(),campaign.worldPlaces.end(),[&](const auto &p){return p.metadata.id==id;});
            if(entry==campaign.worldPlaces.end())
            {
                WorldPlaceModel p; p.metadata.id=id; p.name=fact.label; p.type=WorldPlaceType::City;
                if(fact.entityRefs.size()>1) p.nationId=fact.entityRefs[1];
                campaign.worldPlaces.push_back(std::move(p)); entry=std::prev(campaign.worldPlaces.end());
            }
            // Reusing an established city on a regional map preserves its
            // canonical world-map position and all authored details.
            if(entry->linkedMapId.empty() || entry->linkedMapId==map.metadata.id)
            {
                entry->linkedMapId=map.metadata.id; entry->linkedFeatureId=feature->id; entry->hasMapPosition=true;
                entry->mapPosition={feature->points.front().x/std::max(1,map.width),feature->points.front().y/std::max(1,map.height)};
                normalizeWorldPlacePosition(*entry);
            }
            feature->gmOnly=entry->visibility==WorldObjectVisibility::GmOnly;
            feature->discovered=!feature->gmOnly;
        }
        fact.entityRefs.resize(3);
        fact.entityRefs[2] = feature->id;
        fact.sourceText="map_setup:existing";
    }
    return true;
}
