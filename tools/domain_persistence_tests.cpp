#include "workflow_test_database.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <cstdio>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
void require(bool value, const std::string &message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() {
    try {
        isolateWorkflowDatabase();
        Campaign campaign;
        campaign.name = "Generated changed-row fixture";
        campaign.ruleSet = std::make_shared<DND5e>();
        CharacterModel character;
        std::snprintf(character.nameBuffer, sizeof(character.nameBuffer), "Character");
        campaign.characters.push_back(character);
        NPCModel npc; npc.name = "NPC"; campaign.npcs.push_back(npc);
        ItemModel item; item.name = "Item"; campaign.customItems.push_back(item);
        CampaignRepository::saveCampaign(campaign);
        SQLite::Database db(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
        db.exec("CREATE TABLE domain_audit (kind TEXT);");
        for (const std::string table : {"characters", "npcs", "custom_items"}) {
            db.exec("CREATE TRIGGER audit_" + table + " AFTER UPDATE ON " + table +
                    " BEGIN INSERT INTO domain_audit VALUES ('" + table + "'); END;");
            db.exec("UPDATE " + table + " SET updated_at='unchanged timestamp';");
        }
        db.exec("DELETE FROM domain_audit;");
        campaign.description = "A campaign-only edit";
        CampaignRepository::saveCampaign(campaign);
        require(db.execAndGet("SELECT count(*) FROM domain_audit").getInt() == 0,
                "Unchanged cast or item rows were updated");

        // Derive coverage from the actual schema, independently of the upsert SQL.
        // Repair each externally changed persisted field, including NULL versus empty.
        for (const std::string table : {"characters", "npcs", "custom_items"}) {
            require(db.execAndGet("SELECT updated_at FROM " + table).getString() == "unchanged timestamp",
                    "No-op save changed modification time");
            const std::string created = db.execAndGet("SELECT created_at FROM " + table).getString();
            std::vector<std::pair<std::string, bool>> columns;
            {
                SQLite::Statement schema(db, "PRAGMA table_info(" + table + ");");
                while (schema.executeStep()) {
                    const auto field = schema.getColumn(1).getString();
                    if (std::set<std::string>{"id", "stable_id", "campaign_id", "created_at", "updated_at"}.count(field)) continue;
                    if (table == "characters" && field == "category") continue; // Legacy, not written by character persistence.
                    columns.emplace_back(field, schema.getColumn(3).getInt() == 0);
                }
            }
            for (const auto &[field, nullable] : columns) {
                const auto quoted = "\"" + field + "\"";
                const auto original = db.execAndGet("SELECT quote(" + quoted + ") FROM " + table).getString();
                db.exec("UPDATE " + table + " SET " + quoted + "='external sentinel';");
                db.exec("DELETE FROM domain_audit;");
                CampaignRepository::saveCampaign(campaign);
                require(db.execAndGet("SELECT quote(" + quoted + ") FROM " + table).getString() == original,
                        "Save missed changed field: " + table + "." + field);
                require(db.execAndGet("SELECT count(*) FROM domain_audit").getInt() == 1,
                        "Changed row did not update exactly once");
                if (nullable && original != "NULL") {
                    db.exec("UPDATE " + table + " SET " + quoted + "=NULL;");
                    CampaignRepository::saveCampaign(campaign);
                    require(db.execAndGet("SELECT quote(" + quoted + ") FROM " + table).getString() == original,
                            "NULL comparison missed field: " + field);
                }
            }
            require(db.execAndGet("SELECT created_at FROM " + table).getString() == created,
                    "Update changed creation time");
        }
        campaign.characters[0].gmNotes = "Pending character edit";
        campaign.npcs[0].secrets = "Pending NPC edit";
        campaign.customItems[0].description = "Pending item edit";
        db.exec("CREATE TRIGGER reject_item BEFORE UPDATE ON custom_items BEGIN SELECT RAISE(ABORT,'injected failure'); END;");
        bool failed = false;
        try { CampaignRepository::saveCampaign(campaign); } catch (...) { failed = true; }
        require(failed, "Injected failure did not abort save");
        require(db.execAndGet("SELECT gm_notes FROM characters").getString() != campaign.characters[0].gmNotes,
                "Earlier character update survived rollback");
        require(db.execAndGet("SELECT secrets FROM npcs").getString() != campaign.npcs[0].secrets,
                "Earlier NPC update survived rollback");
        db.exec("DROP TRIGGER reject_item;");
        CampaignRepository::saveCampaign(campaign);
        require(db.execAndGet("SELECT gm_notes FROM characters").getString() == campaign.characters[0].gmNotes &&
                db.execAndGet("SELECT secrets FROM npcs").getString() == campaign.npcs[0].secrets &&
                db.execAndGet("SELECT description FROM custom_items").getString() == campaign.customItems[0].description,
                "Retry lost pending changes");
        const auto loaded = CampaignRepository::loadAllCampaigns();
        require(loaded.size() == 1 && loaded[0].metadata.id == campaign.metadata.id &&
                loaded[0].characters[0].metadata.id == character.metadata.id &&
                loaded[0].characters[0].gmNotes == campaign.characters[0].gmNotes &&
                loaded[0].npcs[0].metadata.id == npc.metadata.id &&
                loaded[0].npcs[0].secrets == campaign.npcs[0].secrets &&
                loaded[0].customItems[0].metadata.id == item.metadata.id &&
                loaded[0].customItems[0].description == campaign.customItems[0].description,
                "Production load lost identities or committed content");
        campaign.characters.clear(); campaign.npcs.clear(); campaign.customItems.clear();
        CampaignRepository::saveCampaign(campaign);
        for (const std::string table : {"characters", "npcs", "custom_items"})
            require(db.execAndGet("SELECT count(*) FROM " + table).getInt() == 0, "Deleted entity survived");
        std::cout << "Changed-row cast and item persistence passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
