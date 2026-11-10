#include "Models/Project/ProjectBackupRestoreInternal.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectManifestService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include <algorithm>
#include <optional>

namespace ProjectBackupRestore::detail {
namespace {
bool overlaps(const fs::path &a,const fs::path &b){return samePath(a,b)||inside(a,b)||inside(b,a);}
fs::path fresh(const fs::path &root,const std::string &prefix){
    const auto path=root/(prefix+AtomicProjectFile::uniqueSuffix());safePath(path);
    require(!fs::exists(mapFileIoPath(path)),"Restore working directory already exists.");return path;
}
void validateTarget(const BackupEntry &entry,const fs::path &target,const std::vector<Campaign> &campaigns){
    const auto root=ProjectPackageLayout::campaignsDirectory();safePath(root);safePath(target);
    require(samePath(target.parent_path(),root)&&inside(target,root),"Campaign package is outside the managed campaign root.");
    for(const auto &reserved:{restoreRoot(),importRoot(),preSaveRoot()})
        require(!overlaps(target,reserved),"Campaign package overlaps a managed backup folder. Its stable ID is reserved.");
    require(!overlaps(target,entry.snapshotPath.parent_path())&&
        (entry.packageBackupPath.empty()||!overlaps(target,entry.packageBackupPath)),"Campaign package overlaps the selected backup.");
    for(const auto &campaign:campaigns)if(campaign.metadata.id!=entry.campaignId)
        require(!samePath(target,ProjectPackageLayout::campaignPackageDirectory(campaign)),"Another campaign uses the same sanitized package directory.");
    if(!fs::exists(mapFileIoPath(target)))return;
    require(directory(target),"Campaign package path is obstructed by a file.");
    const auto manifestPath=target/"project.json";safePath(manifestPath);std::string error;
    const auto manifest=ProjectManifest::loadManifest(mapFileIoPath(manifestPath),error);
    if(error.empty())require(manifest.campaignId==entry.campaignId,"The current package belongs to a different campaign ID.");
    // A data-only restore never replaces these files. An ambiguous folder may
    // be copied for safety, but cannot authorize destructive package changes.
    if(entry.packageAction!=PackageRestoreAction::Keep)
        require(error.empty()&&manifest.campaignId==entry.campaignId,"Cannot replace a package without a valid manifest for this campaign ID.");
}
void retain(RestoreResult &result,const fs::path &path){
    if(!path.empty()&&std::find(result.retainedRecoveryPaths.begin(),result.retainedRecoveryPaths.end(),path)==result.retainedRecoveryPaths.end())
        result.retainedRecoveryPaths.push_back(path);
}
void cleanup(const fs::path &path,const Operations &ops,RestoreResult &result,const std::string &label){
    if(path.empty())return;
    try{removeOwnedTree(path,ops);}catch(const std::exception &e){retain(result,path);result.warnings.push_back(label+": "+e.what()+" Retained at "+path.u8string());}
}
void captureBefore(const BackupEntry &entry,const std::optional<Campaign> &previous,const fs::path &target,
                   const Inventory &priorPackage,bool packagePresent,const Operations &ops,RestoreResult &result){
    safePath(restoreRoot());fs::create_directories(mapFileIoPath(restoreRoot()));
    result.safetyBackupPath=fresh(restoreRoot(),ProjectAssets::sanitizePathSegment(entry.campaignId)+"-restore-v1-");
    fs::create_directory(mapFileIoPath(result.safetyBackupPath));
    // The version namespace AND permanent marker prevent incomplete capture
    // directories from being mistaken for the older receipt-less format.
    AtomicProjectFile::write(result.safetyBackupPath/"restore-layout-v1","GM_RESTORE_CAPTURE_1\n");
    const auto database=result.safetyBackupPath/RestoreDatabase;
    CampaignRepository::writeSnapshot(mapFileIoPath(database).u8string());
    Receipt receipt;receipt.campaignId=entry.campaignId;receipt.campaignName=previous?previous->name:entry.campaignName;
    receipt.campaignPresent=previous.has_value();receipt.packagePresent=packagePresent;receipt.timestampUtc=timestamp(database);
    receipt.packageChanged=entry.packageAction!=PackageRestoreAction::Keep;
    receipt.databaseFingerprint=fileFingerprint(database);
    if(packagePresent){
        copyTree(target,result.safetyBackupPath/RestorePackage,ops);
        require(inventory(target)==priorPackage,"Current package changed while the safety backup was captured.");
        receipt.packageFingerprint=inventoryFingerprint(priorPackage);
    }
    writeReceipt(result.safetyBackupPath,receipt);
    resolveBeforeRestore(result.safetyBackupPath); // Verify the complete pair before any mutation.
}
}

RestoreResult restoreWithOperations(const BackupEntry &selected,const Operations &requested){
    RestoreResult result;auto ops=systemOperations();
    if(requested.copyFile)ops.copyFile=requested.copyFile;
    if(requested.renameDirectory)ops.renameDirectory=requested.renameDirectory;
    if(requested.removeDirectory)ops.removeDirectory=requested.removeDirectory;
    fs::path target,staged,displaced;bool packageDisplaced=false,packageInstalled=false;
    try {
        const auto entry=currentSelection(selected);
        Campaign restored;std::string error;
        if(entry.campaignPresent){
            const bool read=CampaignRepository::readCampaignFromSnapshot(mapFileIoPath(entry.snapshotPath).u8string(),entry.campaignId,restored,error);
            require(read,error);
        }
        const auto campaigns=CampaignRepository::loadAllCampaigns();std::optional<Campaign> previous;
        for(const auto &campaign:campaigns){
            if(campaign.metadata.id==entry.campaignId)previous=campaign;
            else if(entry.campaignPresent)require(campaign.name!=restored.name,"Another campaign currently uses this backup's name. Rename it before restoring.");
        }
        target=ProjectPackageLayout::campaignPackageDirectory(entry.campaignId);
        validateTarget(entry,target,campaigns);
        const bool packagePresent=fs::exists(mapFileIoPath(target));
        const auto priorPackage=packagePresent?inventory(target):Inventory{};
        captureBefore(entry,previous,target,priorPackage,packagePresent,ops,result);
        if(entry.packageAction==PackageRestoreAction::Restore){
            staged=fresh(ProjectPackageLayout::campaignsDirectory(),".restore-staging-");
            copyTree(entry.packageBackupPath,staged,ops);
        }
        // These checks are negative guards after slow I/O, not an authorization
        // from a filename, display label, metadata timestamp or prior selection.
        currentSelection(entry);validateTarget(entry,target,campaigns);
        require(fs::exists(mapFileIoPath(target))==packagePresent&&(!packagePresent||inventory(target)==priorPackage),
                "The current campaign package changed during restore preparation.");
        if(entry.packageAction!=PackageRestoreAction::Keep){
            if(packagePresent){
                displaced=fresh(ProjectPackageLayout::campaignsDirectory(),".restore-displaced-");
                ops.renameDirectory(target,displaced);packageDisplaced=true;
            }
            if(entry.packageAction==PackageRestoreAction::Restore){ops.renameDirectory(staged,target);packageInstalled=true;}
        }
        // Both repository operations own a transaction. On an SQL
        // failure they roll back themselves; attempting a second save here
        // would rerun the same failing trigger and falsely report failed undo.
        if(entry.campaignPresent)CampaignRepository::saveCampaign(restored);
        else CampaignRepository::deleteCampaignByStableId(entry.campaignId);
        result.success=true;result.restoredCampaignName=entry.campaignName;
        result.campaignRemoved=!entry.campaignPresent;
        result.packageRestored=entry.packageAction==PackageRestoreAction::Restore;
        result.packageRemoved=entry.packageAction==PackageRestoreAction::Remove;
        if(packageDisplaced)cleanup(displaced,ops,result,"Restore committed, but old package cleanup failed");
    }catch(const std::exception &exception){
        result.errorMessage=exception.what();
        if(packageInstalled){
            try{removeOwnedTree(target,ops);packageInstalled=false;}
            catch(const std::exception &e){retain(result,target);result.warnings.push_back(std::string("Could not remove the installed package during rollback: ")+e.what());}
        }
        if(packageDisplaced){
            try{safePath(target);require(!fs::exists(mapFileIoPath(target)),"Rollback target is occupied.");ops.renameDirectory(displaced,target);packageDisplaced=false;}
            catch(const std::exception &e){retain(result,displaced);result.warnings.push_back(std::string("Could not return the previous package: ")+e.what());}
        }
        cleanup(staged,ops,result,"Could not clean restore staging");
        if(!result.safetyBackupPath.empty())retain(result,result.safetyBackupPath);
        if(packageInstalled||packageDisplaced)result.errorMessage+=" Package rollback is incomplete; retained recovery paths must be reviewed.";
    }
    return result;
}
}

namespace ProjectBackupRestore {
RestoreResult restoreBackup(const BackupEntry &entry){return detail::restoreWithOperations(entry,{});}
}
