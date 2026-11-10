// This friend opens only fresh generated test copies, never the live singleton.
struct SpellMigrationProbe
{
    static void open(const std::filesystem::path &path)
    { auto database=std::unique_ptr<DatabaseManager>(new DatabaseManager(path.string())); }
};
namespace
{
bool spellOverrideMigration()
{
    const auto folder=ApplicationPaths::databasePath().parent_path()/createEntityId("spell-schema31");
    if(!std::filesystem::create_directory(folder))throw std::runtime_error("Migration fixture root must be fresh");
    bool ok=true;
    for(bool fail:{false,true}) {
        const auto path=folder/(fail?"failure.db":"success.db");
        DatabaseManager::getInstance()->writeSnapshot(path.string());
        {
            SQLite::Database db(path.string(),SQLite::OPEN_READWRITE);
            // A real previous schema has neither mode column. Retain the entire
            // production schema, but remove only the schema32 additions.
            for(const char *table:{"characters","npcs"})for(const char *column:{"spell_attack_mode","spell_save_dc_mode"})
                db.exec(std::string("ALTER TABLE ")+table+" DROP COLUMN "+column+";");
            db.exec("DELETE FROM schema_migrations WHERE version>=32; PRAGMA user_version=31;");
            db.exec("INSERT INTO campaigns(id,stable_id,name,ruleset_name) VALUES(9001,'migration-campaign','Schema31 caster','DND5e');");
            db.exec("INSERT INTO characters(stable_id,campaign_id,name,level,spell_attack_bonus,spell_save_dc) VALUES('migration-pc',9001,'Legacy PC',5,0,5);");
            db.exec("INSERT INTO npcs(stable_id,campaign_id,name,spell_attack_bonus,spell_save_dc) VALUES('migration-npc',9001,'Legacy NPC',-2,11);");
            if(fail)db.exec("CREATE TRIGGER refuse_schema32 BEFORE INSERT ON schema_migrations WHEN NEW.version=32 BEGIN SELECT RAISE(ABORT,'injected migration record failure'); END;");
            ok&=require(scalarInt(db,"PRAGMA user_version;")==31&&!columnExists(db,"characters","spell_attack_mode"),"Fixture is not actually schema31");
        }
        bool refused=false;try{SpellMigrationProbe::open(path);}catch(const std::exception &){refused=true;}
        {
            SQLite::Database db(path.string(),SQLite::OPEN_READONLY);
            ok&=require(refused==fail,"Migration success/failure disposition wrong");
            ok&=require(scalarInt(db,"PRAGMA user_version;")== (fail?31:32),"Migration version did not commit/rollback atomically");
            ok&=require(columnExists(db,"characters","spell_attack_mode")==!fail&&columnExists(db,"npcs","spell_save_dc_mode")==!fail,
                "Failed migration left partial mode columns");
            ok&=require(scalarInt(db,"SELECT spell_save_dc FROM characters WHERE stable_id='migration-pc';")==5&&
                scalarInt(db,"SELECT spell_attack_bonus FROM npcs WHERE stable_id='migration-npc';")==-2,"Migration changed historical authored numbers");
            if(!fail)ok&=require(scalarInt(db,"SELECT spell_attack_mode+spell_save_dc_mode FROM characters WHERE stable_id='migration-pc';")==0&&
                scalarInt(db,"SELECT spell_attack_mode+spell_save_dc_mode FROM npcs WHERE stable_id='migration-npc';")==0,
                "Schema31 migration did not default both fields to Legacy");
        }
        if(fail) {
            {SQLite::Database db(path.string(),SQLite::OPEN_READWRITE);db.exec("DROP TRIGGER refuse_schema32;");}
            SpellMigrationProbe::open(path);
            SQLite::Database db(path.string(),SQLite::OPEN_READONLY);
            ok&=require(scalarInt(db,"PRAGMA user_version;")==32&&scalarInt(db,"SELECT COUNT(*) FROM schema_migrations WHERE version=32;")==1,
                "Migration retry did not complete exactly once");
        }
    }
    return ok;
}
}
