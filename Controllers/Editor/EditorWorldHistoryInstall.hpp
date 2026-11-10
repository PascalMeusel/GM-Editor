#pragma once
#include "Models/Project/WorldInspectorHistoryFields.hpp"
#include <tuple>

namespace EditorWorldHistoryDetail
{
inline bool metadataEqual(const EntityMetadata &a,const EntityMetadata &b)
{
    return std::tie(a.id,a.createdAtUtc,a.updatedAtUtc,a.tags,a.customProperties)==
           std::tie(b.id,b.createdAtUtc,b.updatedAtUtc,b.tags,b.customProperties);
}
template<class T,class Equal> bool sequenceEqual(const std::vector<T> &a,const std::vector<T> &b,Equal equal)
{ return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),equal); }
inline bool connectionEqual(const DungeonRoomConnectionModel &a,const DungeonRoomConnectionModel &b)
{
    return metadataEqual(a.metadata,b.metadata)&&std::tie(a.targetRoomId,a.label,a.locked,a.secret,a.discovered)==
        std::tie(b.targetRoomId,b.label,b.locked,b.secret,b.discovered);
}
inline bool roomEqual(const DungeonRoomModel &a,const DungeonRoomModel &b)
{
    return metadataEqual(a.metadata,b.metadata)&&
        std::tie(a.name,a.description,a.keyedDescription,a.readAloudText,a.secrets,a.traps,a.explorationNotes,
            a.explorationState,a.mapX,a.mapY,a.encounterIds,a.lootItemIds)==
        std::tie(b.name,b.description,b.keyedDescription,b.readAloudText,b.secrets,b.traps,b.explorationNotes,
            b.explorationState,b.mapX,b.mapY,b.encounterIds,b.lootItemIds)&&sequenceEqual(a.connections,b.connections,connectionEqual);
}
inline void dungeonFields(const DungeonModel &a,const DungeonModel &b,DungeonModel &v)
{
    WorldInspectorHistory::detail::Strict m;
    m.field(a.name,b.name,v.name,"Dungeon name");m.field(a.description,b.description,v.description,"Dungeon description");
    m.field(a.parentMapId,b.parentMapId,v.parentMapId,"Dungeon parent map");
    m.field(a.activeRoomId,b.activeRoomId,v.activeRoomId,"Dungeon active room");
    m.field(a.gmNotes,b.gmNotes,v.gmNotes,"Dungeon notes");m.field(a.mapIds,b.mapIds,v.mapIds,"Dungeon maps");
    m.field(a.rooms,b.rooms,v.rooms,"Dungeon rooms",[](const auto &x,const auto &y){return sequenceEqual(x,y,roomEqual);});
    if(v.metadata.updatedAtUtc==a.metadata.updatedAtUtc)v.metadata.updatedAtUtc=b.metadata.updatedAtUtc;
}
// Installation only. Dungeon values saved alongside World drafts never enter
// the World history patch and cannot subsequently be undone by that command.
inline void installDungeons(Campaign &current,const Campaign &before,const Campaign &saved,
                             const std::map<EntityId,DungeonModel> &submitted)
{
    using namespace WorldInspectorHistory::detail;
    for(const auto &[id,draft]:submitted) {
        const auto &a=unique(before.dungeons,id,MetadataId{}),&b=unique(saved.dungeons,id,MetadataId{});
        auto &target=unique(current.dungeons,id,MetadataId{});available(target);
        dungeonFields(a,b,target);
    }
}
inline bool sameDungeonDrafts(const std::map<EntityId,DungeonModel> &before,const std::map<EntityId,DungeonModel> &current)
{
    try {
        for(const auto &[id,draft]:before) {
            const auto found=current.find(id);if(found==current.end())return false;
            auto copy=draft;dungeonFields(found->second,draft,copy);
        }
        return true;
    } catch(...){return false;}
}
inline void adoptTimestamp(EntityMetadata &current,const EntityMetadata &before,const EntityMetadata &saved)
{ if(current.updatedAtUtc==before.updatedAtUtc)current.updatedAtUtc=saved.updatedAtUtc; }
struct Times {std::string campaign;std::map<EntityId,std::string> places,nations,shops;};
template<class T> void rememberTimes(const std::vector<T> &rows,const std::vector<WorldInspectorHistory::Change<T>> &changes,
                                      std::map<EntityId,std::string> &out)
{
    using namespace WorldInspectorHistory::detail;
    for(const auto &change:changes)out.emplace(change.before.metadata.id,unique(rows,change.before.metadata.id,MetadataId{}).metadata.updatedAtUtc);
}
inline Times rememberTimes(const Campaign &c,const WorldInspectorHistory::Patch &patch)
{
    Times t;t.campaign=c.metadata.updatedAtUtc;rememberTimes(c.worldPlaces,patch.places,t.places);
    rememberTimes(c.nations,patch.nations,t.nations);rememberTimes(c.shops,patch.shops,t.shops);return t;
}
template<class T> void adoptTimestamps(std::vector<T> &current,const std::vector<T> &saved,const std::map<EntityId,std::string> &before)
{
    using namespace WorldInspectorHistory::detail;
    for(const auto &[id,time]:before) {
        auto &v=unique(current,id,MetadataId{});
        if(v.metadata.updatedAtUtc==time)v.metadata.updatedAtUtc=unique(saved,id,MetadataId{}).metadata.updatedAtUtc;
    }
}
inline void adoptTimestamps(Campaign &current,const Campaign &saved,const Times &before)
{
    if(current.metadata.updatedAtUtc==before.campaign)current.metadata.updatedAtUtc=saved.metadata.updatedAtUtc;
    adoptTimestamps(current.worldPlaces,saved.worldPlaces,before.places);adoptTimestamps(current.nations,saved.nations,before.nations);
    adoptTimestamps(current.shops,saved.shops,before.shops);
}
template<class T> void touch(std::vector<T> &rows,const std::vector<WorldInspectorHistory::Change<T>> &changes)
{ for(const auto &change:changes)WorldInspectorHistory::detail::unique(rows,change.before.metadata.id,WorldInspectorHistory::detail::MetadataId{}).metadata.touch(); }
inline void touch(Campaign &c,const WorldInspectorHistory::Patch &patch)
{ touch(c.worldPlaces,patch.places);touch(c.nations,patch.nations);touch(c.shops,patch.shops); }
}
