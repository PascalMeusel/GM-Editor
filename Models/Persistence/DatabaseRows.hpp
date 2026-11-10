#pragma once
#include "Models/DataBaseManager.hpp"
#include <string>
#include <vector>

std::shared_ptr<IRuleSet> createRuleSetFromName(const std::string &name);
std::vector<EntityId> characterIds(const std::vector<CharacterModel> &characters);
std::vector<EntityId> npcIds(const std::vector<NPCModel> &npcs);
std::vector<EntityId> factionIds(const std::vector<FactionModel> &factions);
std::vector<EntityId> deityIds(const std::vector<DeityModel> &deities);
std::vector<EntityId> nationIds(const std::vector<NationModel> &nations);
std::vector<EntityId> worldPlaceIds(const std::vector<WorldPlaceModel> &places);
std::string serializeNormalizedPolygon(const std::vector<NormalizedWorldPointModel> &polygon);
std::vector<NormalizedWorldPointModel> deserializeNormalizedPolygon(const std::string &serialized);
std::vector<EntityId> itemIds(const std::vector<ItemModel> &items);
std::vector<EntityId> shopStableIds(const std::vector<ShopModel> &shops);
std::vector<EntityId> lootParcelIds(const std::vector<LootParcelModel> &parcels);
std::vector<EntityId> spellIds(const std::vector<SpellModel> &spells);
std::vector<EntityId> monsterIds(const std::vector<MonsterDefinitionModel> &monsters);
std::vector<EntityId> sessionIds(const std::vector<SessionModel> &sessions);
std::vector<EntityId> timelineEventIds(const std::vector<TimelineEventModel> &events);
std::vector<EntityId> randomTableIds(const std::vector<RandomTableModel> &tables);
std::vector<std::string> monthNames(const CampaignCalendarModel &calendar);
std::vector<std::string> monthLengths(const CampaignCalendarModel &calendar);
std::vector<CalendarMonthModel> calendarMonthsFromStorage(const std::string &namesValue,
                                                          const std::string &lengthsValue);
std::vector<EntityId> mapIds(const std::vector<MapModel> &maps);
std::vector<EntityId> dungeonIds(const std::vector<DungeonModel> &dungeons);
std::vector<EntityId> handoutIds(const std::vector<HandoutModel> &handouts);
std::vector<EntityId> audioAssetIds(const std::vector<AudioAssetModel> &assets);
std::vector<EntityId> audioPlaylistIds(const std::vector<AudioPlaylistModel> &playlists);
std::vector<EntityId> audioAmbienceLayerIds(const std::vector<AudioAmbienceLayerModel> &layers);
std::vector<EntityId> audioTriggerIds(const std::vector<AudioTriggerModel> &triggers);
std::vector<EntityId> rulesetDefinitionIds(const std::vector<RulesetDefinitionModel> &definitions);
std::vector<EntityId> rulesetContentPackIds(const std::vector<RulesetContentPackModel> &packs);
std::vector<EntityId> houseRuleIds(const std::vector<HouseRuleModel> &rules);
std::vector<EntityId> savedFilterIds(const std::vector<CampaignSavedFilterModel> &filters);
std::vector<EntityId> entityBookmarkIds(const std::vector<CampaignEntityBookmarkModel> &bookmarks);
std::string serializeHouseRuleChanges(const std::vector<HouseRuleChangeModel> &changes);
std::vector<HouseRuleChangeModel> deserializeHouseRuleChanges(const std::string &serializedChanges);
void deleteContentPacksMissingStableIds(SQLite::Database &database, int64_t campaignId,
                                        const std::vector<EntityId> &stableIds);
RulesetDefinitionModel readRulesetDefinition(SQLite::Statement &query);
void bindRulesetDefinition(SQLite::Statement &statement, int64_t campaignId, const RulesetDefinitionModel &definition,
                           bool installedGlobally);
RulesetContentPackModel readRulesetContentPack(SQLite::Statement &query);
HouseRuleModel readHouseRule(SQLite::Statement &query);
CampaignSavedFilterModel readSavedFilter(SQLite::Statement &query);
CampaignEntityBookmarkModel readEntityBookmark(SQLite::Statement &query);
