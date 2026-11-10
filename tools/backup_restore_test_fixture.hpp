#pragma once
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectBackupRestoreService.hpp"
#include "Models/Project/ProjectBackupRestoreOperations.hpp"
#include "Models/Project/ProjectManifestService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

namespace backup_restore_test {
namespace fs=std::filesystem;
namespace service=ProjectBackupRestore;
using Tree=std::map<std::string,std::string>;
inline fs::path root;
inline void require(bool condition,const std::string &message)
{if(!condition)throw std::runtime_error(message);}
inline void environment(const char *name,const fs::path &value)
{
#ifdef _WIN32
    require(_putenv_s(name,value.string().c_str())==0,"Cannot isolate fixture environment");
#else
    require(setenv(name,value.string().c_str(),1)==0,"Cannot isolate fixture environment");
#endif
}
inline void initialize()
{
    root=fs::temp_directory_path()/("gm-backup-restore-"+AtomicProjectFile::uniqueSuffix());
    fs::create_directories(root);
    environment("TTRPG_DATA_ROOT",root);environment("TTRPG_DB_PATH",root/"repository.db");
    environment("TTRPG_SETTINGS_PATH",root/"settings.ini");
    std::cout<<"Backup restore isolated fixture: "<<root.string()<<'\n';
}
inline std::string bytes(const fs::path &path)
{
    std::ifstream file(mapFileIoPath(path),std::ios::binary);
    require(file.good(),"Cannot read fixture "+path.string());
    return {std::istreambuf_iterator<char>(file),{}};
}
inline void write(const fs::path &path,const std::string &value)
{fs::create_directories(mapFileIoPath(path.parent_path()));AtomicProjectFile::write(path,value);}
inline Tree tree(const fs::path &path)
{
    Tree result;if(!fs::exists(mapFileIoPath(path)))return result;
    const auto native=mapFileIoPath(path);
    for(const auto &entry:fs::recursive_directory_iterator(native)) {
        require(!entry.is_symlink(),"Unexpected link in inventory fixture");
        const auto relative=entry.path().lexically_relative(native).generic_string();
        if(entry.is_directory())result.emplace(relative+"/",std::string{});
        else if(entry.is_regular_file())result.emplace(relative,bytes(entry.path()));
    }
    return result;
}
inline void erase(const fs::path &path)
{
    const auto relative=fs::absolute(path).lexically_normal().lexically_relative(fs::absolute(root).lexically_normal());
    require(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!=".."&&relative!=".","Refusing cleanup outside synthetic root");
    fs::remove_all(mapFileIoPath(path));
}
inline Campaign campaign(const std::string &id,const std::string &state)
{
    Campaign result;result.metadata.id=id;result.name="Generated "+id+" "+state;result.description="PRIVATE_"+id+"_"+state;
    result.ruleSet=std::make_shared<DND5e>();CharacterModel hero;hero.metadata.id=id+"_hero";
    std::snprintf(hero.nameBuffer,sizeof(hero.nameBuffer),"Hero %s",state.c_str());hero.gmNotes="GM_"+id+"_"+state;
    result.characters.push_back(hero);return result;
}
inline Tree rows(const EntityId &id,const fs::path &database=ApplicationPaths::databasePath())
{
    SQLite::Database db(mapFileIoPath(database).u8string(),SQLite::OPEN_READONLY);Tree result;
    SQLite::Statement integrity(db,"PRAGMA integrity_check;");require(integrity.executeStep()&&integrity.getColumn(0).getString()=="ok","Fixture integrity failed");
    SQLite::Statement foreignKeys(db,"PRAGMA foreign_key_check;");require(!foreignKeys.executeStep(),"Fixture foreign keys failed");
    SQLite::Statement query(db,"SELECT stable_id,name,description FROM campaigns WHERE stable_id=?;");query.bind(1,id);
    if(!query.executeStep())return result;
    result["id"]=query.getColumn(0).getString();result["name"]=query.getColumn(1).getString();result["description"]=query.getColumn(2).getString();
    require(!query.executeStep(),"Duplicate campaign identity");
    SQLite::Statement children(db,"SELECT ch.stable_id,ch.name,ch.gm_notes FROM characters ch JOIN campaigns c ON c.id=ch.campaign_id WHERE c.stable_id=? ORDER BY ch.stable_id;");
    children.bind(1,id);while(children.executeStep()) {
        const auto child=children.getColumn(0).getString();result["character/"+child+"/name"]=children.getColumn(1).getString();
        result["character/"+child+"/gm"]=children.getColumn(2).getString();
    }
    return result;
}
inline void checkCampaign(const Campaign &expected,bool present)
{
    const auto saved=rows(expected.metadata.id);
    if(!present){require(saved.empty(),"Absent campaign was resurrected: "+expected.metadata.id);return;}
    require(saved.at("id")==expected.metadata.id&&saved.at("name")==expected.name&&saved.at("description")==expected.description,
        "Wrong restored campaign state for "+expected.metadata.id);
    require(saved.at("character/"+expected.characters.front().metadata.id+"/gm")==expected.characters.front().gmNotes,
        "Wrong restored child/private content for "+expected.metadata.id);
}
inline void package(const fs::path &path,const Campaign &c,const std::string &state)
{
    require(!fs::exists(mapFileIoPath(path)),"Package fixture already exists");
    write(path/"project.json",ProjectManifest::serializeManifest(ProjectManifest::buildManifest(c)));
    write(path/"Assets"/"Files"/"authored.txt","PACKAGE_"+c.metadata.id+"_"+state);
    fs::create_directories(mapFileIoPath(path/"Assets"/"Images"/"empty"));
    write(path/"Recovery"/"Drafts"/"private.draft","PRIVATE_RECOVERY_"+state);
    CampaignRepository::writeCampaignSnapshot(c,mapFileIoPath(path/"campaign.db").u8string());
}
inline std::vector<service::BackupEntry> listed(std::string *warnings=nullptr)
{
    std::string error;auto result=service::listBackups(error);
    // Later cases deliberately retain corrupt/incomplete backup folders. Those
    // must be reported and omitted without hiding independent usable entries.
    if(warnings)*warnings=error;return result;
}
inline service::BackupEntry entry(const fs::path &snapshot,const EntityId &id)
{
    const auto all=listed();std::vector<service::BackupEntry> matches;
    for(const auto &value:all)if(value.snapshotPath.lexically_normal()==snapshot.lexically_normal()&&value.campaignId==id)matches.push_back(value);
    require(matches.size()==1,"Expected one discoverable backup: "+snapshot.string()+"; found "+std::to_string(matches.size()));return matches.front();
}
inline bool advertised(const fs::path &snapshot,const EntityId &id,std::string *warnings=nullptr)
{
    const auto all=listed(warnings);return std::any_of(all.begin(),all.end(),[&](const auto &e){return e.snapshotPath.lexically_normal()==snapshot.lexically_normal()&&e.campaignId==id;});
}
inline service::BackupEntry safety(const service::RestoreResult &result,const EntityId &id)
{
    require(result.success,"Restore failed: "+result.errorMessage);
    require(!result.safetyBackupPath.empty(),"Successful restore omitted safety location");
    auto found=entry(result.safetyBackupPath/"repository-before-restore.db",id);
    require(found.kind==service::BackupKind::BeforeRestore&&!found.legacySafety&&!found.selectionFingerprint.empty()&&!found.timestampUtc.empty(),
        "New safety checkpoint lacks explicit discoverable identity");return found;
}
struct Fixture {
    Campaign a,b,other;fs::path source,target;service::BackupEntry selected;Tree sourceBytes,otherRows,otherBytes,aBytes,bBytes;
    Fixture(const std::string &tag,bool campaignPresent=true,bool packagePresent=true,
            const std::function<void(Campaign &)> &enrich={})
        :a(campaign("restore_"+tag,"A")),b(campaign(a.metadata.id,"B")),other(campaign("unrelated_"+tag,"U"))
    {
        if(enrich){enrich(a);enrich(b);enrich(other);}
        CampaignRepository::saveCampaign(other);package(ProjectPackageLayout::campaignPackageDirectory(other),other,"U");
        CampaignRepository::saveCampaign(b);
        source=ProjectPackageLayout::campaignsDirectory()/"ImportBackups"/(b.metadata.id+"-import_fixture");
        fs::create_directories(source);CampaignRepository::writeSnapshot(mapFileIoPath(source/"repository-before-import.db").u8string());
        package(source/"package-before-import.ttrpgpkg",b,"B");bBytes=tree(source/"package-before-import.ttrpgpkg");
        sourceBytes=tree(source);selected=entry(source/"repository-before-import.db",b.metadata.id);
        require(selected.packageAction==service::PackageRestoreAction::Restore,"Import pair not discovered as package restore");
        // The input repository deliberately contains an older unrelated state.
        // Replacing the whole DB would lose this subsequently saved canary.
        advanceOther("after_source_capture");
        if(campaignPresent)CampaignRepository::saveCampaign(a);else CampaignRepository::deleteCampaignByStableId(a.metadata.id);
        target=ProjectPackageLayout::campaignPackageDirectory(a);
        if(packagePresent)package(target,a,"A");aBytes=tree(target);
    }
    void advanceOther(const std::string &phase) {
        other.description="UNRELATED_CURRENT_"+phase;other.characters.front().gmNotes="UNRELATED_GM_"+phase;
        CampaignRepository::saveCampaign(other);
        write(ProjectPackageLayout::campaignPackageDirectory(other)/"Assets"/"Files"/"current.txt","UNRELATED_ASSET_"+phase);
        otherRows=rows(other.metadata.id);otherBytes=tree(ProjectPackageLayout::campaignPackageDirectory(other));
    }
    void invariants() const {
        require(rows(other.metadata.id)==otherRows,"Restore changed unrelated campaign rows");
        require(tree(ProjectPackageLayout::campaignPackageDirectory(other))==otherBytes,"Restore changed unrelated package/assets");
        require(tree(source)==sourceBytes,"Restore modified selected source backup bytes");
    }
    void prior(bool present=true,bool packagePresent=true) const {
        checkCampaign(a,present);require(fs::exists(target)==packagePresent,"Wrong prior package presence");
        require(tree(target)==aBytes,"Prior package bytes changed");invariants();
    }
    void restored() const {checkCampaign(b,true);require(tree(target)==bBytes,"Restored package does not match source bytes");invariants();}
};
}
