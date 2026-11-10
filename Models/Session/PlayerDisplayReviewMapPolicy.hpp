#pragma once

#include "Models/Session/PlayerDisplayReviewTypes.hpp"

namespace PlayerDisplayReview
{
inline std::string currentLocaleCode()
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
               ? "de"
               : "en";
}

inline const MapModel *publicMap(const Campaign &campaign)
{
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    if (display.publicMapId.empty())
        return nullptr;
    return findPlayerSafePublishedMap(campaign, display.publicMapId);
}

inline MapModel playerSafeMapSource(const MapModel &map)
{
    MapModel filtered = map;
    filtered.tokens.erase(std::remove_if(filtered.tokens.begin(), filtered.tokens.end(), [&](const MapTokenModel &token) {
                              return token.hiddenFromPlayers || !playerSafeMapLayerIsVisible(map, token.layerId) ||
                                     !playerSafeMapPositionIsRevealed(map, token.x, token.y);
                          }),
                          filtered.tokens.end());
    filtered.features.erase(std::remove_if(filtered.features.begin(), filtered.features.end(), [&](const MapFeatureModel &feature) {
                                return feature.gmOnly || !feature.discovered ||
                                       !playerSafeMapCellIsRevealed(map, feature.x, feature.y);
                            }),
                            filtered.features.end());
    filtered.generation.graph.features.erase(
        std::remove_if(filtered.generation.graph.features.begin(),
                       filtered.generation.graph.features.end(),
                       [&](const MapGeneratedFeature &feature) {
                           // Point stamps frequently extend beyond their anchor
                           // cell. Drawing one below the final fog mask can
                           // therefore reveal its silhouette in an adjacent,
                           // already revealed cell. Keep linear geography in
                           // the plan (fog clips its unrevealed sections), but
                           // fail closed for generated point content until its
                           // anchor itself is revealed.
                           const bool pointContent =
                               feature.kind == MapGeneratedFeatureKind::Settlement ||
                               feature.kind == MapGeneratedFeatureKind::Landmark ||
                               feature.kind == MapGeneratedFeatureKind::BuildingGroup ||
                               feature.kind == MapGeneratedFeatureKind::TacticalSetpiece;
                           if (!pointContent || feature.points.empty())
                               return false;
                           return !playerSafeMapPositionIsRevealed(
                               map, feature.points.front().x, feature.points.front().y);
                       }),
        filtered.generation.graph.features.end());
    if (!playerSafeMapBackgroundArtIsFullyRevealed(map))
    {
        filtered.backgroundImagePath.clear();
        filtered.importedReference.gmOnly = true;
    }
    return filtered;
}

inline std::uint32_t playerMapLayers(const MapModel &map)
{
    return mapPlayerReleasedRenderLayers(map);
}

inline bool worldNationFeatureIsPublic(const Campaign &campaign, const MapGeneratedFeature &feature)
{
    const auto matchesStableId = [&](const NationModel &nation) {
        if (!feature.id.empty() && nation.metadata.id == feature.id)
            return true;
        return !feature.id.empty() && std::any_of(nation.territories.begin(), nation.territories.end(),
                                                   [&](const NationTerritoryModel &territory) {
                                                       return territory.metadata.id == feature.id;
                                                   });
    };
    const auto matchesName = [&](const NationModel &nation) {
        if (!feature.name.empty() && nation.name == feature.name)
            return true;
        return !feature.name.empty() && std::any_of(nation.territories.begin(), nation.territories.end(),
                                                     [&](const NationTerritoryModel &territory) {
                                                         return territory.name == feature.name;
                                                     });
    };
    const auto uniqueVisibleMatch = [&](const auto &predicate) {
        const NationModel *match = nullptr;
        for (const NationModel &nation : campaign.nations)
        {
            if (!predicate(nation))
                continue;
            if (match != nullptr)
                return false;
            match = &nation;
        }
        return match != nullptr && match->visibility != WorldObjectVisibility::GmOnly;
    };

    const int stableMatches = static_cast<int>(std::count_if(campaign.nations.begin(), campaign.nations.end(), matchesStableId));
    if (stableMatches != 0)
        return stableMatches == 1 && uniqueVisibleMatch(matchesStableId);
    return uniqueVisibleMatch(matchesName);
}

inline bool worldPlaceFeatureIsPublic(const Campaign &campaign, const MapModel &map,
                                      const MapGeneratedFeature &feature)
{
    const auto binding = mapSetupBindingForFeature(map, feature);
    const auto exactMatch = [&](const WorldPlaceModel &place) {
        if (binding.present) return place.metadata.id == binding.entityId;
        return place.linkedMapId == map.metadata.id && !place.linkedFeatureId.empty() &&
               place.linkedFeatureId == feature.id;
    };
    const auto legacyNameMatch = [&](const WorldPlaceModel &place) {
        return place.linkedMapId == map.metadata.id && place.linkedFeatureId.empty() &&
               !feature.name.empty() && place.name == feature.name;
    };
    const auto uniqueVisibleMatch = [&](const auto &predicate) {
        const WorldPlaceModel *match = nullptr;
        for (const WorldPlaceModel &place : campaign.worldPlaces)
        {
            if (!predicate(place))
                continue;
            if (match != nullptr)
                return false;
            match = &place;
        }
        return match != nullptr && match->visibility != WorldObjectVisibility::GmOnly;
    };

    const int exactMatches = static_cast<int>(std::count_if(campaign.worldPlaces.begin(), campaign.worldPlaces.end(), exactMatch));
    if (binding.present || exactMatches != 0)
        return exactMatches == 1 && uniqueVisibleMatch(exactMatch);
    return uniqueVisibleMatch(legacyNameMatch);
}

// Every player-facing map surface uses the same campaign consent. Category
// grants alone do not publish unlinked world places or private nations.
inline bool generatedFeatureIsPublic(const Campaign &campaign, const MapModel &map,
                                      const MapGeneratedFeature &feature)
{
    const auto binding = mapSetupBindingForFeature(map, feature);
    if (binding.present)
    {
        const bool nation = feature.kind == MapGeneratedFeatureKind::Nation ||
            (feature.kind == MapGeneratedFeatureKind::TerrainRegion && feature.materialId.find("territory.") != std::string::npos);
        int matches = 0;
        bool visible = false;
        const auto inspect = [&](const auto &entries) {
            for (const auto &entry : entries) if (!binding.entityId.empty() && entry.metadata.id == binding.entityId)
            { ++matches; visible = entry.visibility != WorldObjectVisibility::GmOnly; }
        };
        if (nation) inspect(campaign.nations); else inspect(campaign.worldPlaces);
        return matches == 1 && visible;
    }
    if (map.atlasLevel != MapLevel::World)
        return true;
    if (feature.kind == MapGeneratedFeatureKind::Nation ||
        (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
         feature.materialId.find("territory.") != std::string::npos))
        return worldNationFeatureIsPublic(campaign, feature);
    if (feature.kind == MapGeneratedFeatureKind::Settlement || feature.kind == MapGeneratedFeatureKind::Landmark)
        return worldPlaceFeatureIsPublic(campaign, map, feature);
    return true;
}

} // namespace PlayerDisplayReview
