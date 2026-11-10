#include "workflow_test_database.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
Campaign reload(const EntityId &id) {
    for (auto c : CampaignRepository::loadAllCampaigns()) if(c.metadata.id==id) return c;
    throw std::runtime_error("Missing campaign");
}
}
int main() {
 try {
    isolateWorkflowDatabase();
    Campaign c; c.name="Generated collections"; c.ruleSet=std::make_shared<DND5e>();
    CharacterModel actor; c.characters.push_back(actor);
    InventoryEntryModel entry; entry.itemId="fixture-item"; c.partyInventory.entries={entry};
    EncounterModel encounter; encounter.name="Encounter";
    EncounterParticipantModel participant; participant.displayName="Participant";
    encounter.participants={participant}; c.encounters={encounter};
    StoryThreadModel thread; thread.title="Thread";
    StoryBeatModel beat; beat.title="Beat";
    StoryLinkModel link; link.label="Link"; beat.links={link}; thread.beats={beat}; c.storyThreads={thread};
    CampaignRepository::saveCampaign(c);
    SQLite::Database db(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
    db.exec("CREATE TABLE collection_audit (kind TEXT);");
    for(const std::string table:{"inventories","inventory_entries","encounters","encounter_participants","story_threads","story_beats","story_links"})
      for(const std::string op:{"INSERT","UPDATE","DELETE"})
        db.exec("CREATE TRIGGER audit_"+table+op+" AFTER "+op+" ON "+table+" BEGIN INSERT INTO collection_audit VALUES ('"+table+"'); END;");
    c.description="Only campaign changed"; CampaignRepository::saveCampaign(c);
    require(db.execAndGet("SELECT count(*) FROM collection_audit").getInt()==0,"Unchanged collections were rewritten");
    // Moves retain stable identities and the target owner; parent row IDs must not use last_insert_rowid after a no-op.
    c.partyInventory.entries.clear(); c.characters[0].inventory.entries={entry};
    EncounterModel second; second.name="Second"; c.encounters.push_back(second);
    c.encounters[0].participants.clear(); c.encounters[1].participants={participant};
    StoryThreadModel other; other.title="Other"; c.storyThreads.push_back(other);
    c.storyThreads[0].beats.clear(); c.storyThreads[1].beats={beat};
    CampaignRepository::saveCampaign(c);
    auto loaded=reload(c.metadata.id);
    require(loaded.partyInventory.entries.empty() && loaded.characters[0].inventory.entries[0].metadata.id==entry.metadata.id,"Inventory transfer lost owner or identity");
    require(db.execAndGet("SELECT e.name FROM encounter_participants p JOIN encounters e ON e.id=p.encounter_id").getString()=="Second","Participant attached to wrong encounter");
    require(db.execAndGet("SELECT t.title FROM story_beats b JOIN story_threads t ON t.id=b.thread_id").getString()=="Other","Beat attached to wrong thread");
    auto invalid=c; invalid.partyInventory.entries={entry};
    bool rejected=false;try{CampaignRepository::saveCampaign(invalid);}catch(...){rejected=true;}
    require(rejected,"Duplicate entry identity accepted");
    Campaign foreign; foreign.name="Other campaign";foreign.ruleSet=std::make_shared<DND5e>();
    invalid=foreign;invalid.partyInventory.entries={entry};
    rejected=false;try{CampaignRepository::saveCampaign(invalid);}catch(...){rejected=true;}
    require(rejected,"Foreign entry stolen");
    invalid=foreign;invalid.encounters={c.encounters[1]};
    rejected=false;try{CampaignRepository::saveCampaign(invalid);}catch(...){rejected=true;}
    require(rejected,"Foreign encounter stolen");
    invalid=foreign;invalid.storyThreads={c.storyThreads[1]};
    rejected=false;try{CampaignRepository::saveCampaign(invalid);}catch(...){rejected=true;}
    require(rejected,"Foreign story stolen");
    c.characters[0].inventory.entries[0].quantity=7;
    c.encounters[1].participants[0].displayName="Changed";
    c.storyThreads[1].beats[0].title="Changed beat";
    db.exec("CREATE TRIGGER reject_beat BEFORE UPDATE ON story_beats BEGIN SELECT RAISE(ABORT,'injected failure'); END;");
    rejected=false;try{CampaignRepository::saveCampaign(c);}catch(...){rejected=true;}
    require(rejected && reload(c.metadata.id).characters[0].inventory.entries[0].quantity!=7,"Failed save leaked preceding writes");
    db.exec("DROP TRIGGER reject_beat;");CampaignRepository::saveCampaign(c);
    require(reload(c.metadata.id).characters[0].inventory.entries[0].quantity==7,"Retry lost inventory change");
    require(db.execAndGet("SELECT display_name FROM encounter_participants").getString()=="Changed","Retry lost participant change");
    require(db.execAndGet("SELECT title FROM story_beats").getString()=="Changed beat","Retry lost beat change");
    auto participant2=participant;participant2.metadata=EntityMetadata{"participant"};participant2.displayName="First turn";
    c.encounters[1].participants.insert(c.encounters[1].participants.begin(),participant2);
    c.encounters[1].activeParticipantIndex=1;
    auto link2=link;link2.metadata=EntityMetadata{"link"};link2.label="First link";
    c.storyThreads[1].beats[0].links.insert(c.storyThreads[1].beats[0].links.begin(),link2);
    CampaignRepository::saveCampaign(c);
    loaded=reload(c.metadata.id);
    for(const auto &e:loaded.encounters) if(e.metadata.id==c.encounters[1].metadata.id)
      require(e.participants[0].metadata.id==participant2.metadata.id && e.participants[e.activeParticipantIndex].metadata.id==participant.metadata.id,"Turn ordering or active participant lost");
    for(const auto &t:loaded.storyThreads) if(t.metadata.id==c.storyThreads[1].metadata.id)
      require(t.beats[0].links[0].metadata.id==link2.metadata.id,"Story link ordering lost");
    auto extra=entry;extra.metadata=EntityMetadata{"entry"};extra.quantity=3;
    c.characters[0].inventory.entries.insert(c.characters[0].inventory.entries.begin(),extra);
    CampaignRepository::saveCampaign(c);
    require(reload(c.metadata.id).characters[0].inventory.entries[0].metadata.id==extra.metadata.id,"Entry ordering lost");
    c.characters.clear();c.encounters.clear();c.storyThreads.clear();CampaignRepository::saveCampaign(c);
    for(const std::string table:{"inventory_entries","encounters","encounter_participants","story_threads","story_beats","story_links"})
      require(db.execAndGet("SELECT count(*) FROM "+table).getInt()==0,"Removed collection survived");
    std::cout<<"Collection persistence passed\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
