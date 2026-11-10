#include "Models/DataBaseManager.hpp"
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/ProjectLogger.hpp"
#include "Models/Rulesets/RulesetDefinitionSerialization.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "Models/Rulesets/Clockwork/ClockworkBloodline.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "Models/Persistence/DatabaseRows.hpp"

using namespace PersistenceSerialization;

std::shared_ptr<IRuleSet> createRuleSetFromName(const std::string &name)
{
    auto clockworkRuleset = std::make_shared<ClockworkBloodlineRuleset>();
    if (clockworkRuleset->matchesRulesetName(name))
    {
        return clockworkRuleset;
    }

    auto dndRuleset = std::make_shared<DND5e>();
    if (dndRuleset->matchesRulesetName(name))
    {
        return dndRuleset;
    }
    std::cerr << "Warnung: Unbekanntes Regelsystem '" << name << "'" << std::endl;
    ProjectLogger::getInstance().warning("DatabaseManager", "Unknown ruleset: " + name);
    return nullptr;
}

std::vector<EntityId> characterIds(const std::vector<CharacterModel> &characters)
{
    std::vector<EntityId> ids;
    ids.reserve(characters.size());
    for (const CharacterModel &character : characters)
        ids.push_back(character.metadata.id);
    return ids;
}

std::vector<EntityId> npcIds(const std::vector<NPCModel> &npcs)
{
    std::vector<EntityId> ids;
    ids.reserve(npcs.size());
    for (const NPCModel &npc : npcs)
        ids.push_back(npc.metadata.id);
    return ids;
}

std::vector<EntityId> factionIds(const std::vector<FactionModel> &factions)
{
    std::vector<EntityId> ids;
    ids.reserve(factions.size());
    for (const FactionModel &faction : factions)
        ids.push_back(faction.metadata.id);
    return ids;
}

std::vector<EntityId> deityIds(const std::vector<DeityModel> &deities)
{
    std::vector<EntityId> ids;
    ids.reserve(deities.size());
    for (const DeityModel &deity : deities)
        ids.push_back(deity.metadata.id);
    return ids;
}

std::vector<EntityId> nationIds(const std::vector<NationModel> &nations)
{
    std::vector<EntityId> ids;
    ids.reserve(nations.size());
    for (const NationModel &nation : nations)
        ids.push_back(nation.metadata.id);
    return ids;
}

std::vector<EntityId> worldPlaceIds(const std::vector<WorldPlaceModel> &places)
{
    std::vector<EntityId> ids;
    ids.reserve(places.size());
    for (const WorldPlaceModel &place : places)
        ids.push_back(place.metadata.id);
    return ids;
}

std::string serializeNormalizedPolygon(const std::vector<NormalizedWorldPointModel> &polygon)
{
    std::ostringstream stream;
    stream << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (std::size_t index = 0; index < polygon.size(); ++index)
    {
        if (index > 0)
            stream << ';';
        stream << normalizedWorldCoordinate(polygon[index].x) << ',' << normalizedWorldCoordinate(polygon[index].y);
    }
    return stream.str();
}

std::vector<NormalizedWorldPointModel> deserializeNormalizedPolygon(const std::string &serialized)
{
    std::vector<NormalizedWorldPointModel> polygon;
    std::stringstream points(serialized);
    std::string pointValue;
    while (std::getline(points, pointValue, ';'))
    {
        const std::size_t comma = pointValue.find(',');
        if (comma == std::string::npos)
            continue;
        try
        {
            NormalizedWorldPointModel point;
            point.x = normalizedWorldCoordinate(std::stod(pointValue.substr(0, comma)));
            point.y = normalizedWorldCoordinate(std::stod(pointValue.substr(comma + 1)));
            polygon.push_back(point);
        }
        catch (const std::exception &)
        {
            // Ignore only the malformed point and preserve the remaining polygon.
        }
    }
    return polygon;
}

std::vector<EntityId> itemIds(const std::vector<ItemModel> &items)
{
    std::vector<EntityId> ids;
    ids.reserve(items.size());
    for (const ItemModel &item : items)
        ids.push_back(item.metadata.id);
    return ids;
}

std::vector<EntityId> shopStableIds(const std::vector<ShopModel> &shops)
{
    std::vector<EntityId> ids;
    ids.reserve(shops.size());
    for (const ShopModel &shop : shops)
        ids.push_back(shop.metadata.id);
    return ids;
}

std::vector<EntityId> lootParcelIds(const std::vector<LootParcelModel> &parcels)
{
    std::vector<EntityId> ids;
    ids.reserve(parcels.size());
    for (const LootParcelModel &parcel : parcels)
        ids.push_back(parcel.metadata.id);
    return ids;
}

std::vector<EntityId> spellIds(const std::vector<SpellModel> &spells)
{
    std::vector<EntityId> ids;
    ids.reserve(spells.size());
    for (const SpellModel &spell : spells)
        ids.push_back(spell.metadata.id);
    return ids;
}

std::vector<EntityId> monsterIds(const std::vector<MonsterDefinitionModel> &monsters)
{
    std::vector<EntityId> ids;
    ids.reserve(monsters.size());
    for (const MonsterDefinitionModel &monster : monsters)
        ids.push_back(monster.metadata.id);
    return ids;
}

std::vector<EntityId> sessionIds(const std::vector<SessionModel> &sessions)
{
    std::vector<EntityId> ids;
    ids.reserve(sessions.size());
    for (const SessionModel &session : sessions)
        ids.push_back(session.metadata.id);
    return ids;
}

std::vector<EntityId> timelineEventIds(const std::vector<TimelineEventModel> &events)
{
    std::vector<EntityId> ids;
    ids.reserve(events.size());
    for (const TimelineEventModel &event : events)
        ids.push_back(event.metadata.id);
    return ids;
}

std::vector<EntityId> randomTableIds(const std::vector<RandomTableModel> &tables)
{
    std::vector<EntityId> ids;
    ids.reserve(tables.size());
    for (const RandomTableModel &table : tables)
        ids.push_back(table.metadata.id);
    return ids;
}

std::vector<std::string> monthNames(const CampaignCalendarModel &calendar)
{
    std::vector<std::string> names;
    names.reserve(calendar.months.size());
    for (const CalendarMonthModel &month : calendar.months)
        names.push_back(month.name);
    return names;
}

std::vector<std::string> monthLengths(const CampaignCalendarModel &calendar)
{
    std::vector<std::string> lengths;
    lengths.reserve(calendar.months.size());
    for (const CalendarMonthModel &month : calendar.months)
        lengths.push_back(std::to_string(month.days));
    return lengths;
}

std::vector<CalendarMonthModel> calendarMonthsFromStorage(const std::string &namesValue,
                                                          const std::string &lengthsValue)
{
    const std::vector<std::string> names = deserializeStringList(namesValue);
    const std::vector<std::string> lengths = deserializeStringList(lengthsValue);
    std::vector<CalendarMonthModel> months;
    const std::size_t count = std::max(names.size(), lengths.size());
    months.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        CalendarMonthModel month;
        if (index < names.size())
            month.name = names[index];
        if (index < lengths.size() && !lengths[index].empty())
            month.days = std::max(1, std::atoi(lengths[index].c_str()));
        months.push_back(month);
    }
    return months;
}

std::vector<EntityId> mapIds(const std::vector<MapModel> &maps)
{
    std::vector<EntityId> ids;
    ids.reserve(maps.size());
    for (const MapModel &map : maps)
        ids.push_back(map.metadata.id);
    return ids;
}

std::vector<EntityId> dungeonIds(const std::vector<DungeonModel> &dungeons)
{
    std::vector<EntityId> ids;
    ids.reserve(dungeons.size());
    for (const DungeonModel &dungeon : dungeons)
        ids.push_back(dungeon.metadata.id);
    return ids;
}

std::vector<EntityId> handoutIds(const std::vector<HandoutModel> &handouts)
{
    std::vector<EntityId> ids;
    ids.reserve(handouts.size());
    for (const HandoutModel &handout : handouts)
        ids.push_back(handout.metadata.id);
    return ids;
}

std::vector<EntityId> audioAssetIds(const std::vector<AudioAssetModel> &assets)
{
    std::vector<EntityId> ids;
    ids.reserve(assets.size());
    for (const AudioAssetModel &asset : assets)
        ids.push_back(asset.metadata.id);
    return ids;
}

std::vector<EntityId> audioPlaylistIds(const std::vector<AudioPlaylistModel> &playlists)
{
    std::vector<EntityId> ids;
    ids.reserve(playlists.size());
    for (const AudioPlaylistModel &playlist : playlists)
        ids.push_back(playlist.metadata.id);
    return ids;
}

std::vector<EntityId> audioAmbienceLayerIds(const std::vector<AudioAmbienceLayerModel> &layers)
{
    std::vector<EntityId> ids;
    ids.reserve(layers.size());
    for (const AudioAmbienceLayerModel &layer : layers)
        ids.push_back(layer.metadata.id);
    return ids;
}

std::vector<EntityId> audioTriggerIds(const std::vector<AudioTriggerModel> &triggers)
{
    std::vector<EntityId> ids;
    ids.reserve(triggers.size());
    for (const AudioTriggerModel &trigger : triggers)
        ids.push_back(trigger.metadata.id);
    return ids;
}

std::vector<EntityId> rulesetDefinitionIds(const std::vector<RulesetDefinitionModel> &definitions)
{
    std::vector<EntityId> ids;
    ids.reserve(definitions.size());
    for (const RulesetDefinitionModel &definition : definitions)
        ids.push_back(definition.metadata.id);
    return ids;
}

std::vector<EntityId> rulesetContentPackIds(const std::vector<RulesetContentPackModel> &packs)
{
    std::vector<EntityId> ids;
    ids.reserve(packs.size());
    for (const RulesetContentPackModel &pack : packs)
        ids.push_back(pack.metadata.id);
    return ids;
}

std::vector<EntityId> houseRuleIds(const std::vector<HouseRuleModel> &rules)
{
    std::vector<EntityId> ids;
    ids.reserve(rules.size());
    for (const HouseRuleModel &rule : rules)
        ids.push_back(rule.metadata.id);
    return ids;
}

std::vector<EntityId> savedFilterIds(const std::vector<CampaignSavedFilterModel> &filters)
{
    std::vector<EntityId> ids;
    ids.reserve(filters.size());
    for (const CampaignSavedFilterModel &filter : filters)
        ids.push_back(filter.metadata.id);
    return ids;
}

std::vector<EntityId> entityBookmarkIds(const std::vector<CampaignEntityBookmarkModel> &bookmarks)
{
    std::vector<EntityId> ids;
    ids.reserve(bookmarks.size());
    for (const CampaignEntityBookmarkModel &bookmark : bookmarks)
        ids.push_back(bookmark.metadata.id);
    return ids;
}
