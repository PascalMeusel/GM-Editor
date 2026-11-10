#pragma once
#include "backup_restore_test_fixture.hpp"
#include "Models/DataBaseManager.hpp"

// Inspect the repository's real connection. A separate SQLite connection's
// PRAGMA would not prove the delete operation preserved its caller's mode.
struct DatabaseCampaignDeletionProbe {
    static int mode(){return DatabaseManager::getInstance()->m_db->execAndGet("PRAGMA foreign_keys;").getInt();}
    static void mode(int value){DatabaseManager::getInstance()->m_db->exec(value?"PRAGMA foreign_keys=ON;":"PRAGMA foreign_keys=OFF;");}
};
namespace backup_restore_test {
inline void nestedCampaign(Campaign &c)
{
    const auto &id=c.metadata.id;c.metadata.customProperties["deletion_canary"]=c.description;
    ItemModel item;item.metadata.id=id+"_item";item.name="Stored item";item.container=true;c.customItems.push_back(item);
    InventoryEntryModel parent(item.metadata.id,1);parent.metadata.id=id+"_party_entry";
    InventoryEntryModel child(item.metadata.id,2);child.metadata.id=id+"_contained_entry";child.containerEntryId=parent.metadata.id;
    c.partyInventory.metadata.id=id+"_party_inventory";c.partyInventory.entries={parent,child};
    parent.metadata.id=id+"_character_entry";c.characters.front().inventory.metadata.id=id+"_character_inventory";
    c.characters.front().inventory.entries={parent};
    MapModel map;map.metadata.id=id+"_map";map.name="Stored map";map.width=2;map.height=2;map.cells.resize(4);
    MapLayerModel layer;layer.metadata.id=id+"_layer";layer.name="Stored layer";map.layers.push_back(layer);
    MapTokenModel token;token.metadata.id=id+"_token";token.name="Stored token";token.layerId=layer.metadata.id;
    token.linkedEntityId=c.characters.front().metadata.id;map.tokens.push_back(token);c.maps.push_back(map);
    EncounterModel encounter;encounter.metadata.id=id+"_encounter";encounter.name="Stored encounter";encounter.theaterOfMind=true;
    EncounterParticipantModel participant;participant.metadata.id=id+"_participant";participant.displayName="Stored participant";
    participant.currentHitPoints=7;participant.maxHitPoints=11;encounter.participants.push_back(participant);c.encounters.push_back(encounter);
    SessionModel session;session.metadata.id=id+"_session";session.title="Stored session";session.roughNotes=c.description;
    SessionPlanEntry planned;planned.metadata.id=id+"_plan";planned.storyBeatId=id+"_scene";planned.prepNote=c.description;session.planEntries.push_back(planned);
    SessionNoteModel note;note.metadata.id=id+"_note";note.body=c.description;session.notes.push_back(note);
    session.encounterIds.push_back(encounter.metadata.id);c.sessions.push_back(session);
}
using OwnedRows=std::map<std::string,std::map<long long,std::string>>;
inline OwnedRows ownedRows(const EntityId &id)
{
    SQLite::Database db(mapFileIoPath(ApplicationPaths::databasePath()).u8string(),SQLite::OPEN_READONLY);OwnedRows result;
    const std::map<std::string,std::string> filters{
        {"characters","campaign_id"},{"inventories","campaign_id"},{"maps","campaign_id"},{"sessions","campaign_id"},
        {"encounters","campaign_id"},{"local_player_display","campaign_id"},{"campaign_safety_profiles","campaign_id"},
        {"campaign_entity_properties","campaign_id"},
        {"inventory_entries","inventory_id IN (SELECT id FROM inventories WHERE campaign_id"},
        {"map_layers","map_id IN (SELECT id FROM maps WHERE campaign_id"},
        {"map_tokens","map_id IN (SELECT id FROM maps WHERE campaign_id"},
        {"session_notes","session_id IN (SELECT id FROM sessions WHERE campaign_id"},
        {"encounter_participants","encounter_id IN (SELECT id FROM encounters WHERE campaign_id"}};
    for(const auto &[table,filter]:filters) {
        const bool nested=filter.find(" IN ")!=std::string::npos;
        SQLite::Statement query(db,"SELECT rowid,* FROM "+table+" WHERE "+filter+
            " IN (SELECT id FROM campaigns WHERE stable_id=?)"+(nested?")":"")+" ORDER BY rowid;");query.bind(1,id);
        auto &rows=result[table];while(query.executeStep()) {
            std::string serialized;
            for(int column=1;column<query.getColumnCount();++column) {
                const auto value=query.getColumn(column);const auto text=value.isNull()?std::string{}:value.getString();
                serialized+=(value.isNull()?"N":"V")+std::to_string(text.size())+":"+text;
            }
            rows.emplace(query.getColumn(0).getInt64(),std::move(serialized));
        }
    }
    return result;
}
inline void requirePopulated(const OwnedRows &rows)
{
    for(const auto &[table,values]:rows)require(!values.empty(),"Nested deletion fixture has no rows in "+table);
    const auto &sessions=rows.at("sessions");require(std::any_of(sessions.begin(),sessions.end(),[](const auto &row){return row.second.find("_plan")!=std::string::npos;}),
        "Nested deletion fixture lost its serialized session plan");
}
inline void requireDeleted(const OwnedRows &before)
{
    SQLite::Database db(mapFileIoPath(ApplicationPaths::databasePath()).u8string(),SQLite::OPEN_READONLY);
    for(const auto &[table,values]:before)for(const auto &[id,ignored]:values) {
        SQLite::Statement query(db,"SELECT COUNT(*) FROM "+table+" WHERE rowid=?;");query.bind(1,id);
        require(query.executeStep()&&query.getColumn(0).getInt()==0,"Deleted campaign retained child row in "+table);
    }
    SQLite::Statement check(db,"PRAGMA foreign_key_check;");require(!check.executeStep(),"Campaign deletion left foreign-key violations");
}
}
