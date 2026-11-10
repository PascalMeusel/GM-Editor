#pragma once

#include "Models/Map/MapRenderModels.hpp"

inline MapLevel mapLevelForCreationIntent(MapCreationIntent intent)
{
    switch (intent)
    {
    case MapCreationIntent::WorldOverview:
        return MapLevel::World;
    case MapCreationIntent::RegionExploration:
        return MapLevel::Region;
    case MapCreationIntent::Settlement:
    case MapCreationIntent::TravelScene:
    case MapCreationIntent::Dungeon:
    case MapCreationIntent::Cave:
    case MapCreationIntent::ImportedReference:
        return MapLevel::Site;
    case MapCreationIntent::TacticalEncounter:
        return MapLevel::Tactical;
    }
    return MapLevel::Region;
}

inline MapKind mapKindForCreationIntent(MapCreationIntent intent)
{
    switch (intent)
    {
    case MapCreationIntent::WorldOverview:
    case MapCreationIntent::RegionExploration:
        return MapKind::Geographic;
    case MapCreationIntent::Settlement:
        return MapKind::Settlement;
    case MapCreationIntent::Dungeon:
    case MapCreationIntent::Cave:
        return MapKind::Dungeon;
    case MapCreationIntent::TacticalEncounter:
        return MapKind::Encounter;
    case MapCreationIntent::TravelScene:
    case MapCreationIntent::ImportedReference:
        return MapKind::General;
    }
    return MapKind::General;
}

inline MapScope mapLegacyScopeForCreationIntent(MapCreationIntent intent)
{
    switch (intent)
    {
    case MapCreationIntent::WorldOverview:
    case MapCreationIntent::RegionExploration:
        return MapScope::World;
    case MapCreationIntent::Dungeon:
    case MapCreationIntent::Cave:
        return MapScope::Dungeon;
    default:
        return MapScope::Local;
    }
}

inline MapCreationIntent mapCreationIntentFromLegacy(MapLevel level, MapKind kind, MapScope scope)
{
    if (level == MapLevel::World)
        return MapCreationIntent::WorldOverview;
    if (level == MapLevel::Region)
        return MapCreationIntent::RegionExploration;
    if (level == MapLevel::Tactical || kind == MapKind::Encounter)
        return MapCreationIntent::TacticalEncounter;
    if (kind == MapKind::Settlement)
        return MapCreationIntent::Settlement;
    if (kind == MapKind::Dungeon || kind == MapKind::Interior || scope == MapScope::Dungeon)
        return MapCreationIntent::Dungeon;
    return MapCreationIntent::TravelScene;
}

inline MapDimensions mapDimensionsForPreset(MapCreationIntent intent, MapSizePreset preset)
{
    const int index = preset == MapSizePreset::Small ? 0 : preset == MapSizePreset::Large ? 2 : 1;
    switch (mapLevelForCreationIntent(intent))
    {
    case MapLevel::World:
    {
        constexpr MapDimensions sizes[] = {{48, 32}, {72, 48}, {96, 64}};
        return sizes[index];
    }
    case MapLevel::Region:
    {
        constexpr MapDimensions sizes[] = {{64, 48}, {80, 60}, {96, 72}};
        return sizes[index];
    }
    case MapLevel::Site:
    {
        constexpr MapDimensions sizes[] = {{48, 36}, {64, 48}, {80, 60}};
        return sizes[index];
    }
    case MapLevel::Tactical:
    {
        constexpr MapDimensions sizes[] = {{30, 22}, {40, 30}, {60, 45}};
        return sizes[index];
    }
    }
    return {72, 48};
}

inline MapSizePreset mapSizePresetFromDimensions(MapCreationIntent intent, int width, int height)
{
    for (MapSizePreset preset : {MapSizePreset::Small, MapSizePreset::Standard, MapSizePreset::Large})
    {
        const MapDimensions dimensions = mapDimensionsForPreset(intent, preset);
        if (width == dimensions.width && height == dimensions.height)
            return preset;
    }
    return MapSizePreset::Custom;
}

// Classify a pre-V5 map without changing its persisted legacy fields.  Some
// historical maps only have scope/purpose, while later atlas saves also have
// level/kind.  Strong, purpose-specific evidence wins over stale defaults;
// otherwise an available atlas level remains authoritative.
inline MapCreationIntent mapCreationIntentFromLegacyPersistence(
    MapScope scope,
    MapKind kind,
    MapGenerationPurpose purpose,
    int width,
    int height,
    bool hasAtlasClassification,
    MapLevel atlasLevel)
{
    if (kind == MapKind::Encounter)
        return MapCreationIntent::TacticalEncounter;
    if (kind == MapKind::Settlement)
        return MapCreationIntent::Settlement;
    if (kind == MapKind::Dungeon || kind == MapKind::Interior || scope == MapScope::Dungeon)
        return MapCreationIntent::Dungeon;

    if (hasAtlasClassification)
    {
        switch (atlasLevel)
        {
        case MapLevel::World:
            return MapCreationIntent::WorldOverview;
        case MapLevel::Region:
            return MapCreationIntent::RegionExploration;
        case MapLevel::Tactical:
            return MapCreationIntent::TacticalEncounter;
        case MapLevel::Site:
            return purpose == MapGenerationPurpose::SettlementEnvirons
                       ? MapCreationIntent::Settlement
                       : MapCreationIntent::TravelScene;
        }
    }

    if (scope == MapScope::World)
        return MapCreationIntent::WorldOverview;
    if (purpose == MapGenerationPurpose::SettlementEnvirons)
        return MapCreationIntent::Settlement;
    if (purpose == MapGenerationPurpose::TravelArea)
        return MapCreationIntent::TravelScene;
    if (purpose == MapGenerationPurpose::Region)
        return MapCreationIntent::RegionExploration;

    // Wilderness was also used by the old tactical generator.  Its three V5
    // dimensions provide deterministic evidence when atlas metadata is absent.
    for (MapSizePreset preset : {MapSizePreset::Small, MapSizePreset::Standard, MapSizePreset::Large})
    {
        const MapDimensions tactical = mapDimensionsForPreset(MapCreationIntent::TacticalEncounter, preset);
        if (width == tactical.width && height == tactical.height)
            return MapCreationIntent::TacticalEncounter;
    }
    return MapCreationIntent::TravelScene;
}

inline void applyMapCreationIntent(MapModel &map, MapCreationIntent intent, MapSizePreset preset, bool resize = true)
{
    map.creationIntent = intent;
    map.sizePreset = preset;
    map.atlasLevel = mapLevelForCreationIntent(intent);
    map.mapKind = mapKindForCreationIntent(intent);
    map.scope = mapLegacyScopeForCreationIntent(intent);
    map.generation.recipe.creationIntent = intent;
    map.generation.recipe.sizePreset = preset;
    map.generation.recipe.scale = map.atlasLevel;
    if (map.atlasLevel == MapLevel::Tactical)
    {
        map.gridType = GridType::Square;
        map.cellSize = 5.0f;
    }
    else if (map.gridType != GridType::None)
    {
        map.gridType = GridType::None;
    }
    if (resize && preset != MapSizePreset::Custom)
    {
        const MapDimensions dimensions = mapDimensionsForPreset(intent, preset);
        map.width = dimensions.width;
        map.height = dimensions.height;
        map.cells.resize(static_cast<std::size_t>(map.width * map.height));
    }
}

