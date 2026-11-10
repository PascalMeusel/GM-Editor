#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <map>
#include <string>
#include <vector>

namespace PersistenceSerialization
{
std::string serializeAttributes(const std::map<std::string, int> &attributes);
std::map<std::string, int> deserializeAttributes(const std::string &serializedAttributes);

std::string serializeStringList(const std::vector<std::string> &values);
std::vector<std::string> deserializeStringList(const std::string &serializedValues);

std::string serializeCharacterRelationships(const std::vector<CharacterRelationshipModel> &relationships);
std::vector<CharacterRelationshipModel> deserializeCharacterRelationships(const std::string &serializedRelationships);

std::string serializeInventoryEntries(const std::vector<InventoryEntryModel> &entries);
std::vector<InventoryEntryModel> deserializeInventoryEntries(const std::string &serializedEntries);

std::string serializeSpellSlots(const std::vector<int> &slots);
std::vector<int> deserializeSpellSlots(const std::string &serializedSlots);

std::string serializeSpellbookEntries(const std::vector<CharacterSpellEntryModel> &entries);
std::vector<CharacterSpellEntryModel> deserializeSpellbookEntries(const std::string &serializedEntries);

std::string serializeMapCells(const std::vector<MapCellModel> &cells);
std::vector<MapCellModel> deserializeMapCells(const std::string &serializedCells);
std::string serializeMapRegions(const std::vector<MapRegionModel> &regions);
std::vector<MapRegionModel> deserializeMapRegions(const std::string &serializedRegions);
std::string serializeMapFeatures(const std::vector<MapFeatureModel> &features);
std::vector<MapFeatureModel> deserializeMapFeatures(const std::string &serializedFeatures);
std::string serializeMapRoutes(const std::vector<MapRouteModel> &routes);
std::vector<MapRouteModel> deserializeMapRoutes(const std::string &serializedRoutes);
std::string serializeMapGenerationMetadata(const MapGenerationMetadata &generation);
MapGenerationMetadata deserializeMapGenerationMetadata(const std::string &serializedGeneration);
std::string serializeMapAtlasMetadata(const MapModel &map);
bool deserializeMapAtlasMetadata(const std::string &serializedAtlas, MapModel &map);
// Loads the two additive map payloads and supplies a canonical in-memory V5
// creation contract when an older save did not persist one. The serialized
// inputs are never rewritten and no resize or cell migration is performed.
void deserializeMapPersistenceMetadata(const std::string &serializedGeneration,
                                       const std::string &serializedAtlas,
                                       MapModel &map);
std::string serializeDungeonRoomConnections(const std::vector<DungeonRoomConnectionModel> &connections);
std::vector<DungeonRoomConnectionModel> deserializeDungeonRoomConnections(const std::string &serializedConnections);

std::string serializeMonsterActions(const std::vector<MonsterActionModel> &actions);
std::vector<MonsterActionModel> deserializeMonsterActions(const std::string &serializedActions);

std::string serializeActiveEffects(const std::vector<ActiveEffectModel> &effects);
std::vector<ActiveEffectModel> deserializeActiveEffects(const std::string &serializedEffects);

std::string serializeResourceTrackers(const std::vector<ResourceTrackerModel> &trackers);
std::vector<ResourceTrackerModel> deserializeResourceTrackers(const std::string &serializedTrackers);

std::string serializeCombatLog(const std::vector<CombatLogEntryModel> &entries);
std::vector<CombatLogEntryModel> deserializeCombatLog(const std::string &serializedEntries);

std::string serializeDiceRollPresets(const std::vector<DiceRollPresetModel> &presets);
std::vector<DiceRollPresetModel> deserializeDiceRollPresets(const std::string &serializedPresets);
}
