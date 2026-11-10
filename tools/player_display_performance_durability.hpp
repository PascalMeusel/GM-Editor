#pragma once
#include "Models/Project/ProjectSaveService.hpp"
#include <SQLiteCpp/SQLiteCpp.h>

namespace player_perf
{
// Checked on the next explicit control frame, outside the measured save call.
// Read each snapshot before any later save can prune it; never load it into UI.
struct DurabilityReceipt
{
    ProjectSaveState state;
    std::string before,after,operation;
    bool beforeEnabled=false;
    int cycle=0;
    bool afterEnabled=true;
    void verify(const Fixture &fixture,Metrics &metrics) const
    {
        const auto &config=ProjectSaveService::getInstance().config();
        require(config.backupSnapshotsEnabled&&config.recoverySnapshotsEnabled&&
            config.maxBackupSnapshots==10&&config.maxRecoverySnapshots==5,"Durability defaults changed during run");
        require(state.campaignId==fixture.campaign.metadata.id&&state.lastError.empty()&&state.lastBackupError.empty()&&
            state.lastRecoveryError.empty()&&state.lastSaveWarnings.empty(),"Measured save reported durability warnings/errors");
        const auto root=std::filesystem::weakly_canonical(std::filesystem::u8path(metrics.facts.at("run_root")));
        std::ofstream output(root/"durability-receipts.jsonl",std::ios::app);
        require(bool(output),"Cannot open durability receipt evidence");
        for(bool recovery:{false,true})
        {
            const auto path=std::filesystem::weakly_canonical(std::filesystem::u8path(recovery?state.lastRecoverySnapshotPath:state.lastBackupPath));
            const auto relative=path.lexically_relative(root);
            require(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!=".."&&std::filesystem::is_regular_file(path),
                "Snapshot is absent or outside the isolated run");
            SQLite::Database db(path.u8string(),SQLite::OPEN_READONLY);
            require(db.execAndGet("PRAGMA integrity_check").getString()=="ok","Snapshot integrity check failed");
            SQLite::Statement foreignKeys(db,"PRAGMA foreign_key_check");
            require(!foreignKeys.executeStep(),"Snapshot foreign-key check failed");
            require(db.execAndGet("SELECT COUNT(*) FROM campaigns").getInt()==1,"Snapshot contains unrelated campaigns");
            SQLite::Statement row(db,"SELECT c.description,d.enabled,d.public_map_id FROM campaigns c JOIN local_player_display d ON d.campaign_id=c.id WHERE c.stable_id=?");
            row.bind(1,fixture.campaign.metadata.id);require(row.executeStep(),"Snapshot lacks synthetic campaign/display row");
            const auto description=row.getColumn(0).getString();const bool enabled=row.getColumn(1).getInt()!=0;
            const auto mapId=row.getColumn(2).getString();
            require(description==(recovery?after:before)&&enabled==(recovery?afterEnabled:beforeEnabled)&&
                mapId==fixture.campaign.maps.front().metadata.id,"Snapshot raw pre/post canary or display state mismatch");
            require(!row.executeStep(),"Duplicate synthetic campaign snapshot row");
            SQLite::Statement handout(db,"SELECT COUNT(*) FROM handouts WHERE stable_id=?");handout.bind(1,fixture.handoutId);
            require(handout.executeStep()&&handout.getColumn(0).getInt()==1,"Snapshot lost selected handout");
            output<<"{\"cycle\":"<<cycle<<",\"operation\":"<<json(operation)<<",\"kind\":"<<json(recovery?"post-recovery":"pre-backup")
                <<",\"path\":"<<json(relative.generic_u8string())<<",\"description\":"<<json(description)<<",\"enabled\":"<<(enabled?"true":"false")
                <<",\"public_map_id\":"<<json(mapId)<<",\"bytes\":"<<std::filesystem::file_size(path)
                <<",\"fnv1a64\":"<<json(ProjectAssetImport::computeContentHash(path))<<",\"integrity\":\"ok\",\"foreign_key_findings\":0}\n";
        }
        output.flush();require(bool(output),"Cannot finish durability receipts");
        metrics.checks.push_back("cycle"+std::to_string(cycle)+" "+operation+": isolated pre/post snapshot integrity, foreign keys, exact canaries and raw display state");
    }
};
}
