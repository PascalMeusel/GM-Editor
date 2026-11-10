#include "Models/Project/ProjectBackupRestoreInternal.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectManifestService.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include <algorithm>

namespace ProjectBackupRestore::detail {
namespace {
SnapshotCampaign uniqueCampaign(const std::vector<SnapshotCampaign> &campaigns,const std::function<bool(const SnapshotCampaign &)> &match){
    SnapshotCampaign selected;std::size_t count=0;
    for(const auto &c:campaigns)if(match(c)){selected=c;++count;}
    require(count==1&&!selected.id.empty()&&!selected.name.empty(),"Backup campaign identity is missing or ambiguous.");
    return selected;
}
BackupEntry preSave(const fs::path &snapshot){
    require(samePath(snapshot.parent_path().parent_path(),preSaveRoot())&&snapshot.extension()==".db","Invalid pre-save snapshot location.");
    const auto campaign=uniqueCampaign(inspectSnapshot(snapshot),[&](const auto &c){return ProjectAssets::sanitizePathSegment(c.id)==snapshot.parent_path().filename().u8string();});
    BackupEntry e;e.snapshotPath=snapshot;e.campaignId=campaign.id;e.campaignName=campaign.name;e.label=snapshot.stem().u8string();
    e.timestampUtc=timestamp(snapshot);e.byteCount=fs::file_size(mapFileIoPath(snapshot));bindEntry(e);return e;
}
BackupEntry importBackup(const fs::path &root){
    require(samePath(root.parent_path(),importRoot())&&directory(root),"Invalid import backup location.");
    const auto snapshot=root/"repository-before-import.db";const auto folder=root.filename().u8string();
    const auto campaign=uniqueCampaign(inspectSnapshot(snapshot),[&](const auto &c){return folder.rfind(ProjectAssets::sanitizePathSegment(c.id)+"-",0)==0;});
    BackupEntry e;e.kind=BackupKind::ImportReplacement;e.snapshotPath=snapshot;e.campaignId=campaign.id;e.campaignName=campaign.name;
    const auto package=root/"package-before-import.ttrpgpkg";safePath(package);
    if(fs::exists(mapFileIoPath(package))){
        require(directory(package),"Import package backup is not a directory.");std::string error;
        const auto manifest=ProjectManifest::loadManifest(mapFileIoPath(package/"project.json"),error);
        require(error.empty()&&manifest.campaignId==e.campaignId&&manifest.campaignName==e.campaignName,"Import package does not match its paired database.");
        e.packageBackupPath=package;e.packageAction=PackageRestoreAction::Restore;
    }
    e.timestampUtc=timestamp(snapshot);e.label="Before import — "+e.timestampUtc;e.byteCount=fs::file_size(mapFileIoPath(snapshot));bindEntry(e);return e;
}
BackupEntry beforeRestore(const fs::path &root){
    require(samePath(root.parent_path(),restoreRoot())&&directory(root),"Invalid restore safety location.");
    const auto receipt=root/ReceiptFile;safePath(receipt);
    if(fs::exists(mapFileIoPath(receipt)))return resolveBeforeRestore(root);
    // New capture directories are distinguishable even if copying was stopped
    // before receipt publication. Never reinterpret incomplete work as legacy.
    safePath(root/"restore-layout-v1");
    require(root.filename().u8string().find("-restore-v1-")==std::string::npos&&
        !fs::exists(mapFileIoPath(root/"restore-layout-v1")),"Safety capture is incomplete; its recovery files are retained.");
    const auto snapshot=root/RestoreDatabase;const auto folder=root.filename().u8string();
    const auto campaign=uniqueCampaign(inspectSnapshot(snapshot),[&](const auto &c){return folder.rfind(ProjectAssets::sanitizePathSegment(c.id)+"-restore_",0)==0;});
    BackupEntry e;e.kind=BackupKind::BeforeRestore;e.legacySafety=true;e.snapshotPath=snapshot;e.campaignId=campaign.id;e.campaignName=campaign.name;
    e.timestampUtc=timestamp(snapshot);e.label="Legacy before-restore data only — "+e.timestampUtc;e.byteCount=fs::file_size(mapFileIoPath(snapshot));
    // A missing legacy package cannot prove prior absence or complete capture.
    // Keep all current assets; the old paired folder remains untouched on disk.
    bindEntry(e);return e;
}
void warning(std::vector<std::string> *warnings,const fs::path &path,const std::exception &error){
    if(warnings&&warnings->size()<12)warnings->push_back(path.filename().u8string()+": "+error.what());
}
}
std::vector<BackupEntry> discover(std::vector<std::string> *warnings){
    std::vector<BackupEntry> entries;
    for(const auto &root:{preSaveRoot(),importRoot(),restoreRoot()}){
        try {
            safePath(root);if(!fs::exists(mapFileIoPath(root)))continue;require(directory(root),"Managed backup root is not a directory.");
            for(const auto &candidate:fs::directory_iterator(mapFileIoPath(root))){
                // Convert back to the ordinary caller-visible path; extended
                // Windows paths are used only at filesystem boundaries.
                const auto folder=root/candidate.path().filename();
                try {
                    require(directory(folder),"Backup entry is not a directory.");
                    if(samePath(root,preSaveRoot())){
                        for(const auto &file:fs::directory_iterator(mapFileIoPath(folder))){const auto path=folder/file.path().filename();
                            if(path.extension()!=".db")continue;try{entries.push_back(preSave(path));}catch(const std::exception &e){warning(warnings,path,e);}}
                    }else entries.push_back(samePath(root,importRoot())?importBackup(folder):beforeRestore(folder));
                }catch(const std::exception &e){warning(warnings,folder,e);}
            }
        }catch(const std::exception &e){warning(warnings,root,e);}
    }
    std::sort(entries.begin(),entries.end(),[](const auto &a,const auto &b){return a.timestampUtc==b.timestampUtc?a.snapshotPath.generic_u8string()>b.snapshotPath.generic_u8string():a.timestampUtc>b.timestampUtc;});
    return entries;
}
BackupEntry currentSelection(const BackupEntry &selected){
    require(!selected.selectionFingerprint.empty(),"Refresh the backup browser before restoring.");BackupEntry current;
    if(selected.kind==BackupKind::PreSave)current=preSave(selected.snapshotPath);
    else if(selected.kind==BackupKind::ImportReplacement){
        require(selected.snapshotPath.filename()=="repository-before-import.db","Invalid import snapshot name.");current=importBackup(selected.snapshotPath.parent_path());
    }else if(selected.kind==BackupKind::BeforeRestore){
        require(selected.snapshotPath.filename()==RestoreDatabase,"Invalid safety snapshot name.");current=beforeRestore(selected.snapshotPath.parent_path());
    }else throw std::runtime_error("Unknown backup kind.");
    require(current.selectionFingerprint==selected.selectionFingerprint&&current.campaignId==selected.campaignId&&current.campaignName==selected.campaignName&&
        current.campaignPresent==selected.campaignPresent&&current.packageAction==selected.packageAction&&current.legacySafety==selected.legacySafety&&
        (current.packageBackupPath.empty()?selected.packageBackupPath.empty():(!selected.packageBackupPath.empty()&&samePath(current.packageBackupPath,selected.packageBackupPath))),
        "This backup changed since review. Refresh and confirm the exact entry again.");return current;
}
}

namespace ProjectBackupRestore {
std::vector<BackupEntry> listBackups(std::string &errorMessage){
    std::vector<std::string> warnings;auto entries=detail::discover(&warnings);errorMessage.clear();
    for(const auto &warning:warnings){if(!errorMessage.empty())errorMessage+='\n';errorMessage+="Not offered: "+warning;}
    return entries;
}
}
