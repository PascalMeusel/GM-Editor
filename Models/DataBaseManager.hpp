// DataBaseManager.hpp
#pragma once
#include "Models/Persistence/MapCellSerializationCache.hpp"
#include <memory>
#include <vector>
#include "SQLiteCpp/SQLiteCpp.h"
#include "Models/Campaign/Campaign.hpp"

class DatabaseManager
{
    friend struct DatabaseCampaignDeletionProbe;
    friend struct SpellMigrationProbe;
  private:
    std::unique_ptr<SQLite::Database> m_db;
    static std::shared_ptr<DatabaseManager> instance;
    Campaign currentCampaign;
    MapCellSerializationCache cellSerializationCache;
    // Prepared once per save; discarded at the next transaction, including retry.
    std::unique_ptr<SQLite::Statement> inventoryUpsert, inventoryEntryUpsert, inventoryRowId;

    DatabaseManager(const std::string &dbPath, bool migrationSourceAlreadyPreserved = false);

    bool columnExists(const std::string &tableName, const std::string &columnName);
    void addColumnIfMissing(const std::string &tableName, const std::string &columnName,
                            const std::string &columnDefinition);
    void recordSchemaMigrationIfMissing(int version, const std::string &description);
    void deleteRowsMissingStableIds(const std::string &tableName, int64_t campaignId,
                                    const std::vector<EntityId> &stableIds);
    void saveInventory(int64_t campaignId, const std::string &ownerType, const EntityId &ownerEntityId,
                       const InventoryModel &inventory);
    InventoryModel loadInventoryForOwner(int64_t campaignId, const std::string &ownerType,
                                         const EntityId &ownerEntityId, const InventoryModel &fallbackInventory);

    // Domain passes share m_db and never open or commit transactions.
    void saveCampaignEntityProperties(int64_t campaignId, const Campaign &campaign);
    void loadCampaignEntityProperties(Campaign &campaign);
    void saveCampaignPlayerDisplay(int64_t campaignId, const Campaign &campaign);
    void saveCampaignRulesets(int64_t campaignId, const Campaign &campaign);
    void saveCampaignCampaignPreferences(int64_t campaignId, const Campaign &campaign);
    void saveCampaignPartyInventory(int64_t campaignId, const Campaign &campaign);
    void saveCampaignCharacters(int64_t campaignId, const Campaign &campaign);
    void saveCampaignFactionsAndDeities(int64_t campaignId, const Campaign &campaign);
    void saveCampaignNationsAndPlaces(int64_t campaignId, const Campaign &campaign);
    void saveCampaignNpcs(int64_t campaignId, const Campaign &campaign);
    void saveCampaignItems(int64_t campaignId, const Campaign &campaign);
    void saveCampaignShopsAndLoot(int64_t campaignId, const Campaign &campaign);
    void saveCampaignDungeons(int64_t campaignId, const Campaign &campaign);
    void saveCampaignHandouts(int64_t campaignId, const Campaign &campaign);
    void saveCampaignSessions(int64_t campaignId, const Campaign &campaign);
    void saveCampaignSpellsAndMonsters(int64_t campaignId, const Campaign &campaign);
    void saveCampaignMaps(int64_t campaignId, const Campaign &campaign);
    void saveCampaignAudio(int64_t campaignId, const Campaign &campaign);
    void saveCampaignEncounters(int64_t campaignId, const Campaign &campaign);
    void saveCampaignStory(int64_t campaignId, const Campaign &campaign);
    void saveCampaignTimeline(int64_t campaignId, const Campaign &campaign);
    void saveCampaignRandomTables(int64_t campaignId, const Campaign &campaign);
    void saveCampaignTabletopToolkit(int64_t campaignId, const Campaign &campaign);
    void loadCampaignPlayerDisplay(Campaign &campaign);
    void loadCampaignRulesetsAndPreferences(Campaign &campaign);
    void loadCampaignCharacters(Campaign &campaign);
    void loadCampaignFactionsAndDeities(Campaign &campaign);
    void loadCampaignNationsAndPlaces(Campaign &campaign);
    void loadCampaignNpcs(Campaign &campaign);
    void loadCampaignItems(Campaign &campaign);
    void loadCampaignShopsAndLoot(Campaign &campaign);
    void loadCampaignSpellsAndMonsters(Campaign &campaign);
    void loadCampaignMaps(Campaign &campaign);
    void loadCampaignDungeons(Campaign &campaign);
    void loadCampaignHandoutsAndDocuments(Campaign &campaign);
    void loadCampaignSessions(Campaign &campaign);
    void loadCampaignAudio(Campaign &campaign);
    void loadCampaignEncounters(Campaign &campaign);
    void loadCampaignStory(Campaign &campaign);
    void loadCampaignTimeline(Campaign &campaign);
    void loadCampaignRandomTables(Campaign &campaign);
    void loadCampaignTabletopToolkit(Campaign &campaign);
    void initializeTabletopToolkitSchema();

  public:
    DatabaseManager(const DatabaseManager &) = delete;
    void operator=(const DatabaseManager &) = delete;

    static std::shared_ptr<DatabaseManager> getInstance();
    // Portable export is deliberately separate from a whole-repository backup.
    static void writeCampaignSnapshot(const Campaign &campaign, const std::string &snapshotPath);
    static bool readCampaignFromSnapshot(const std::string &snapshotPath, const std::string &campaignStableId,
                                         Campaign &campaign, std::string &errorMessage);

    void saveCampaign(const Campaign &campaign);
    void deleteCampaignByStableId(const std::string &campaignStableId);
    void saveInstalledRulesetDefinition(const RulesetDefinitionModel &definition);
    void removeInstalledRulesetDefinition(const std::string &stableId);
    void writeSnapshot(const std::string &snapshotPath);
    void writeRecoverySnapshot(const std::string &snapshotPath);
    std::vector<Campaign> loadCampaignSummaries();
    std::vector<Campaign> loadAllCampaigns();
    std::vector<RulesetDefinitionModel> loadInstalledRulesetDefinitions();
    Campaign &loadCampaign(std::string campaignName);
    Campaign &getActiveCampaign()
    {
        return currentCampaign;
    }
};
