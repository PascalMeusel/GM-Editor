#include "workflow_test_database.hpp"
#include "Models/Persistence/MapCellSerializationCache.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <fstream>
#include "Models/Rulesets/DND/DND5e.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
Campaign reload(const EntityId &id) {
    for (auto c : CampaignRepository::loadAllCampaigns()) if (c.metadata.id == id) return c;
    throw std::runtime_error("Missing campaign");
}
}
int main() {
 try {
    MapCellSerializationCache cache(4096);
    std::vector<MapCellModel> cells(3);
    auto verify = [&] { require(cache.serialize("map", cells) == PersistenceSerialization::serializeMapCells(cells), "Cached cells differ from production serialization"); };
    verify(); verify();
    cells[0].kind=static_cast<MapTileKind>(1);verify();
    cells[0].rotationQuarterTurns=3;verify();cells[0].rotationLocked=true;verify();
    cells[0].symbolVariantMode=static_cast<MapSymbolVariantMode>(1);verify();
    cells[0].wayKind=static_cast<MapWayKind>(1);verify();cells[0].movementCost=7;verify();
    cells[0].difficultTerrain=true;verify();cells[0].blocksLineOfSight=true;verify();
    cells[0].lighting=static_cast<MapLightingMode>(1);verify();cells[0].fogged=true;verify();
    cells[0].elevation=8;verify();cells[0].movementBlockedMask=3;verify();
    cells[0].lineOfSightBlockedMask=7;verify();cells[0].movementOneWayBlockedMask=5;verify();
    cells[0].lineOfSightOneWayBlockedMask=2;verify();cells.resize(1000);verify();
    for (int i=0;i<200;++i) { cache.serialize(std::to_string(i), {MapCellModel{}}); require(cache.bytes()<=4096,"Cache exceeded budget"); }
    verify();
    isolateWorkflowDatabase();
    Campaign campaign;campaign.ruleSet=std::make_shared<DND5e>();campaign.name="Generated incremental save fixture";
    MapModel map;map.name="Map";map.width=4;map.height=4;resizeMapCells(map);ensureDefaultMapLayer(map);
    MapTokenModel token;token.name="Token";token.layerId=map.layers[0].metadata.id;map.tokens.push_back(token);
    campaign.maps.push_back(map);CampaignRepository::saveCampaign(campaign);
    SQLite::Database control(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
    control.exec("CREATE TABLE write_audit (kind TEXT);");
    for (const std::string table : {"maps","map_layers","map_tokens"})
        for (const std::string operation : {"UPDATE","DELETE"})
            control.exec("CREATE TRIGGER audit_"+table+operation+" AFTER "+operation+" ON "+table+" BEGIN INSERT INTO write_audit VALUES ('"+table+"'); END;");
    campaign.description="Only the campaign description changed";CampaignRepository::saveCampaign(campaign);
    require(control.execAndGet("SELECT count(*) FROM write_audit").getInt()==0,"Unchanged map rows were rewritten");
    campaign.maps[0].cells[2].elevation=9;
    control.exec("CREATE TRIGGER reject_map BEFORE INSERT ON maps BEGIN SELECT RAISE(ABORT,'injected map failure'); END;");
    bool failed=false;try { CampaignRepository::saveCampaign(campaign); } catch (...) { failed=true; }
    require(failed && reload(campaign.metadata.id).maps[0].cells[2].elevation!=9,"Failed save changed disk");
    control.exec("DROP TRIGGER reject_map;");CampaignRepository::saveCampaign(campaign);
    require(reload(campaign.metadata.id).maps[0].cells[2].elevation==9,"Retry skipped a cached uncommitted change");
    control.exec("UPDATE maps SET cells='' WHERE name='Map';");CampaignRepository::saveCampaign(campaign);
    require(reload(campaign.metadata.id).maps[0].cells[2].elevation==9,"Cache concealed external database change");
    campaign.maps[0].tokens[0].x=2;campaign.maps[0].layers[0].visible=false;CampaignRepository::saveCampaign(campaign);
    auto read=reload(campaign.metadata.id);require(read.maps[0].tokens[0].x==2 && !read.maps[0].layers[0].visible,"Changed children were skipped");
    campaign.maps[0].tokens.clear();CampaignRepository::saveCampaign(campaign);
    require(reload(campaign.metadata.id).maps[0].tokens.empty(),"Deleted child survived save");
    // Upserts must not conceal duplicate identities or steal another campaign's rows.
    MapModel second;second.name="Second map";second.width=4;second.height=4;resizeMapCells(second);ensureDefaultMapLayer(second);
    campaign.maps.push_back(second);CampaignRepository::saveCampaign(campaign);
    const auto rejectDuplicate = [&](Campaign invalid) {
        bool rejected=false;try { CampaignRepository::saveCampaign(invalid); } catch (...) { rejected=true; }
        require(rejected,"Conflicting child identity was silently accepted");
        require(reload(campaign.metadata.id).maps.size()==2,"Rejected identity change modified disk");
    };
    auto invalid=campaign;invalid.maps[1].layers[0].metadata.id=invalid.maps[0].layers[0].metadata.id;rejectDuplicate(invalid);
    invalid=campaign;invalid.maps[1].metadata.id=invalid.maps[0].metadata.id;rejectDuplicate(invalid);
    invalid=campaign;invalid.maps[0].tokens={token};invalid.maps[1].tokens={token};rejectDuplicate(invalid);
    Campaign foreign;foreign.name="Separate identity owner";foreign.ruleSet=std::make_shared<DND5e>();
    MapModel foreignMap;foreignMap.name="Foreign map";resizeMapCells(foreignMap);ensureDefaultMapLayer(foreignMap);
    MapTokenModel foreignToken;foreignToken.name="Foreign token";foreignMap.tokens={foreignToken};foreign.maps={foreignMap};CampaignRepository::saveCampaign(foreign);
    invalid=campaign;invalid.maps[0].tokens={foreignToken};rejectDuplicate(invalid);
    invalid=campaign;invalid.maps[0].layers[0].metadata.id=foreignMap.layers[0].metadata.id;rejectDuplicate(invalid);
    require(reload(foreign.metadata.id).maps[0].tokens[0].metadata.id==foreignToken.metadata.id,"Another campaign lost its token");
    campaign.maps[0].tokens={token};CampaignRepository::saveCampaign(campaign);
    campaign.maps[0].tokens.clear();campaign.maps[1].tokens={token};campaign.maps[1].tokens[0].layerId=campaign.maps[1].layers[0].metadata.id;
    CampaignRepository::saveCampaign(campaign);
    auto moved=reload(campaign.metadata.id);require(moved.maps[0].tokens.empty() && moved.maps[1].tokens.size()==1 && moved.maps[1].tokens[0].metadata.id==token.metadata.id,"Valid child move lost identity");
    auto directory=ApplicationPaths::databasePath().parent_path();
    const auto snapshot=directory / "recovery.db";
    const auto liveJournal=control.execAndGet("PRAGMA journal_mode").getString();
    const auto liveSync=control.execAndGet("PRAGMA synchronous").getInt();
    CampaignRepository::writeRecoverySnapshot(snapshot.u8string());
    require(control.execAndGet("PRAGMA journal_mode").getString()==liveJournal && control.execAndGet("PRAGMA synchronous").getInt()==liveSync,"Snapshot changed live database durability settings");
    { SQLite::Database copy(snapshot.u8string(),SQLite::OPEN_READONLY);
      require(copy.execAndGet("PRAGMA integrity_check").getString()=="ok","Recovery integrity failed");
      require(copy.execAndGet("SELECT cells FROM maps WHERE name='Map'").getString()==control.execAndGet("SELECT cells FROM maps WHERE name='Map'").getString(),"Recovery content differs"); }
    campaign.maps[0].name="Changed after recovery";CampaignRepository::saveCampaign(campaign);
    failed=false;try { CampaignRepository::writeRecoverySnapshot(snapshot.u8string()); } catch (...) { failed=true; }
    require(failed,"Recovery overwrote an existing snapshot");
    { SQLite::Database copy(snapshot.u8string(),SQLite::OPEN_READONLY);require(copy.execAndGet("SELECT name FROM maps WHERE name='Map'").getString()=="Map","Previous recovery was replaced"); }
    for (const auto &entry:std::filesystem::directory_iterator(directory)) require(entry.path().filename().string().find(".pending_")==std::string::npos,"Recovery staging file leaked");
    std::cout<<"Incremental persistence and recovery tests passed\n";return 0;
 } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
