#pragma once
#include "backup_restore_test_fixture.hpp"
namespace backup_restore_test {
inline void presenceReversals()
{
    for(const bool hasCampaign:{false,true})for(const bool hasPackage:{false,true}) {
        const auto tag=std::string("presence_")+(hasCampaign?"1":"0")+(hasPackage?"1":"0");
        Fixture f(tag,hasCampaign,hasPackage);
        const auto forward=service::restoreBackup(f.selected);const auto undo=safety(forward,f.a.metadata.id);f.restored();
        require(undo.campaignPresent==hasCampaign,"Safety receipt lost prior campaign absence/presence");
        require(undo.packageAction==(hasPackage?service::PackageRestoreAction::Restore:service::PackageRestoreAction::Remove),
            "Safety receipt lost prior package absence/presence");
        const auto safetyBytes=tree(forward.safetyBackupPath);
        f.advanceOther("after_forward_restore");
        const auto backward=service::restoreBackup(undo);const auto redo=safety(backward,f.a.metadata.id);f.prior(hasCampaign,hasPackage);
        require(backward.campaignRemoved==!hasCampaign&&backward.packageRemoved==!hasPackage&&backward.packageRestored==hasPackage,
            "Result did not describe actual restoration/removal");
        require(tree(forward.safetyBackupPath)==safetyBytes,"Restoring safety changed selected snapshot/package/receipt bytes");
        const auto redoBytes=tree(backward.safetyBackupPath);
        f.advanceOther("after_backward_restore");
        const auto repeated=service::restoreBackup(redo);safety(repeated,f.a.metadata.id);f.restored();
        require(tree(backward.safetyBackupPath)==redoBytes&&tree(forward.safetyBackupPath)==safetyBytes,
            "Second reversal mutated an earlier safety checkpoint");
        std::cout<<"PASS prior campaign/package presence "<<hasCampaign<<'/'<<hasPackage<<" and discoverable reversals\n";
    }
}
inline void staleSelections()
{
    Fixture f("stale_pair");
    const auto attachment=f.source/"package-before-import.ttrpgpkg"/"Assets"/"Files"/"authored.txt";
    const auto original=bytes(attachment);const auto time=fs::last_write_time(attachment);
    std::string changed=original;changed.back()=changed.back()=='B'?'X':'B';write(attachment,changed);fs::last_write_time(attachment,time);
    f.sourceBytes=tree(f.source);
    const auto rejected=service::restoreBackup(f.selected);
    require(!rejected.success&&!rejected.errorMessage.empty(),"Same-size/time source package replacement accepted stale selection");f.prior();
    // A fresh selection must be usable; this distinguishes rejection from an unusable fixture.
    f.selected=entry(f.selected.snapshotPath,f.b.metadata.id);f.bBytes=tree(f.source/"package-before-import.ttrpgpkg");
    const auto restored=service::restoreBackup(f.selected);safety(restored,f.a.metadata.id);f.restored();
    const auto selectedSafety=entry(restored.safetyBackupPath/"repository-before-restore.db",f.a.metadata.id);
    SQLite::Database snapshot(mapFileIoPath(selectedSafety.snapshotPath).u8string(),SQLite::OPEN_READWRITE);
    SQLite::Statement update(snapshot,"UPDATE campaigns SET description=? WHERE stable_id=?;");
    update.bind(1,"EXTERNAL_BACKUP_REPLACEMENT");update.bind(2,f.a.metadata.id);update.exec();
    const auto safetyBytes=tree(restored.safetyBackupPath);
    const auto stale=service::restoreBackup(selectedSafety);
    require(!stale.success&&!advertised(selectedSafety.snapshotPath,f.a.metadata.id),"Stale receipt/snapshot binding was accepted or advertised");
    require(tree(restored.safetyBackupPath)==safetyBytes,"Rejected stale snapshot was modified");f.restored();
    Fixture guarded("self_contained");
    for(const std::string suffix:{"-journal","-wal","-shm"}) {
        const fs::path sidecar(guarded.selected.snapshotPath.string()+suffix);
        write(sidecar,"RETAIN_UNREVIEWED_SQLITE_SIDECAR");guarded.sourceBytes=tree(guarded.source);
        const auto refused=service::restoreBackup(guarded.selected);std::string warning;
        require(!refused.success&&!refused.errorMessage.empty()&&!advertised(guarded.selected.snapshotPath,guarded.a.metadata.id,&warning)&&!warning.empty(),
            "SQLite sidecar-backed source was accepted/offered: "+suffix);
        guarded.prior();fs::remove(sidecar);guarded.sourceBytes=tree(guarded.source);
    }
    guarded.selected=entry(guarded.selected.snapshotPath,guarded.a.metadata.id);
    safety(service::restoreBackup(guarded.selected),guarded.a.metadata.id);guarded.restored();
    const auto rollbackBytes=bytes(guarded.selected.snapshotPath);auto walHeader=rollbackBytes;
    require(walHeader.size()>20,"Snapshot fixture header missing");walHeader[18]=2;walHeader[19]=2;
    write(guarded.selected.snapshotPath,walHeader);guarded.sourceBytes=tree(guarded.source);
    require(!advertised(guarded.selected.snapshotPath,guarded.a.metadata.id)&&!service::restoreBackup(guarded.selected).success,
        "Closed WAL header without sidecars was offered/accepted");
    guarded.restored(); // Exact source inventory also proves SQLite created no sidecars.
    write(guarded.selected.snapshotPath,rollbackBytes);
    {
        SQLite::Database future(mapFileIoPath(guarded.selected.snapshotPath).u8string(),SQLite::OPEN_READWRITE);
        future.exec("PRAGMA user_version=2147483647;");
    }
    guarded.sourceBytes=tree(guarded.source);std::string warning;
    require(!advertised(guarded.selected.snapshotPath,guarded.a.metadata.id,&warning)&&!warning.empty()&&
        !service::restoreBackup(guarded.selected).success,"Unsupported future-schema source was offered/accepted");
    guarded.restored();
}
inline void corruptAndMismatchedReceipts()
{
    Fixture first("receipt_first"),second("receipt_second");
    auto foreignPair=first.selected;foreignPair.packageBackupPath=second.selected.packageBackupPath;
    const auto wrongPair=service::restoreBackup(foreignPair);
    require(!wrongPair.success&&!wrongPair.errorMessage.empty(),"Caller-supplied foreign package pair was accepted");first.prior();second.prior();
    const auto one=service::restoreBackup(first.selected),two=service::restoreBackup(second.selected);
    const auto selected=safety(one,first.a.metadata.id);safety(two,second.a.metadata.id);
    const auto receipt=one.safetyBackupPath/"restore-state.txt";const auto original=bytes(receipt);
    for(const auto &replacement:{original+"TRAILING_CORRUPTION",bytes(two.safetyBackupPath/"restore-state.txt")}) {
        write(receipt,replacement);const auto malformed=tree(one.safetyBackupPath);
        const auto result=service::restoreBackup(selected);std::string warning;
        require(!result.success&&!result.errorMessage.empty()&&!advertised(selected.snapshotPath,first.a.metadata.id,&warning)&&!warning.empty(),
            "Corrupt/foreign receipt was accepted or downgraded to legacy");
        require(tree(one.safetyBackupPath)==malformed,"Rejected receipt was modified");first.restored();second.restored();
    }
    write(receipt,original);
    auto rebound=entry(selected.snapshotPath,first.a.metadata.id);const auto restored=service::restoreBackup(rebound);
    safety(restored,first.a.metadata.id);first.prior();second.restored();
}
inline void conservativeLegacy()
{
    Fixture f("legacy");const auto forward=service::restoreBackup(f.selected);const auto modern=safety(forward,f.a.metadata.id);f.restored();
    const auto receipt=forward.safetyBackupPath/"restore-state.txt";const auto receiptBytes=bytes(receipt);
    fs::remove(receipt);
    require(!advertised(modern.snapshotPath,f.a.metadata.id)&&!service::restoreBackup(modern).success,
        "Incomplete modern capture was downgraded to legacy");
    write(receipt,receiptBytes);
    const auto legacyRoot=ProjectPackageLayout::campaignsDirectory()/"RestoreBackups"/(f.a.metadata.id+"-restore_fixture");
    fs::create_directories(legacyRoot);fs::copy_file(mapFileIoPath(modern.snapshotPath),mapFileIoPath(legacyRoot/"repository-before-restore.db"));
    package(legacyRoot/"package-before-restore.ttrpgpkg",f.a,"A");
    const auto legacy=entry(legacyRoot/"repository-before-restore.db",f.a.metadata.id);
    require(legacy.legacySafety&&legacy.campaignPresent&&legacy.packageAction==service::PackageRestoreAction::Keep&&legacy.packageBackupPath.empty(),
        "Legacy safety inferred destructive package/presence semantics");
    const auto before=tree(legacyRoot);const auto result=service::restoreBackup(legacy);safety(result,f.a.metadata.id);
    checkCampaign(f.a,true);require(tree(f.target)==f.bBytes,"Legacy data-only restore changed package");
    require(tree(legacyRoot)==before,"Legacy source modified");f.invariants();
    const auto shortId=campaign("legacy_ambiguous","short"),longId=campaign("legacy_ambiguous-restore_long","long");
    CampaignRepository::saveCampaign(shortId);CampaignRepository::saveCampaign(longId);
    const auto folder=ProjectPackageLayout::campaignsDirectory()/"RestoreBackups"/"legacy_ambiguous-restore_long-restore_fixture";
    fs::create_directories(folder);const auto snapshot=folder/"repository-before-restore.db";
    CampaignRepository::writeSnapshot(mapFileIoPath(snapshot).u8string());const auto original=tree(folder);
    require(!advertised(snapshot,shortId.metadata.id)&&!advertised(snapshot,longId.metadata.id),"Ambiguous legacy folder guessed campaign identity");
    require(tree(folder)==original,"Legacy discovery modified ambiguous source");
}
inline void reservedAndAliasedTargets()
{
    for(const std::string id:{"RestoreBackups","ImportBackups"}) {
        const auto a=campaign(id,"A"),b=campaign(id,"B");CampaignRepository::saveCampaign(b);
        const auto folder=ProjectPackageLayout::campaignsDirectory()/"ImportBackups"/(id+"-import_reserved_fixture");
        fs::create_directories(folder);const auto snapshot=folder/"repository-before-import.db";
        CampaignRepository::writeSnapshot(mapFileIoPath(snapshot).u8string());package(folder/"package-before-import.ttrpgpkg",b,"B");
        CampaignRepository::saveCampaign(a);const auto before=tree(ProjectPackageLayout::campaignsDirectory());
        service::BackupEntry selected;selected.kind=service::BackupKind::ImportReplacement;selected.snapshotPath=snapshot;
        selected.packageBackupPath=folder/"package-before-import.ttrpgpkg";selected.campaignId=id;selected.campaignName=b.name;
        for(const auto &value:listed())if(value.snapshotPath==snapshot)selected=value;
        const auto result=service::restoreBackup(selected);
        require(!result.success&&!result.errorMessage.empty(),"Reserved managed target accepted: "+id);
        checkCampaign(a,true);require(tree(ProjectPackageLayout::campaignsDirectory())==before,"Reserved target created/copied/deleted managed backup paths");
    }
    Fixture f("alias");auto alias=campaign("restore_:alias","unrelated_alias");
    require(ProjectPackageLayout::campaignPackageDirectory(alias)==f.target,"Alias fixture does not collide");
    CampaignRepository::saveCampaign(alias);const auto aliasRows=rows(alias.metadata.id),before=tree(ProjectPackageLayout::campaignsDirectory());
    const auto result=service::restoreBackup(f.selected);require(!result.success&&!result.errorMessage.empty(),"Sanitized identity alias accepted");
    require(rows(alias.metadata.id)==aliasRows&&tree(ProjectPackageLayout::campaignsDirectory())==before,"Alias refusal changed campaign or managed backup tree");f.prior();
}
inline void destructiveOwnershipGuards()
{
    for(const std::string mode:{"foreign","missing","invalid"}) {
        Fixture f("ownership_"+mode,false,true);const auto manifest=f.target/"project.json";
        if(mode=="foreign")write(manifest,ProjectManifest::serializeManifest(ProjectManifest::buildManifest(f.other)));
        else if(mode=="missing")fs::remove(manifest);else write(manifest,"NOT_A_PROJECT_MANIFEST");
        f.aBytes=tree(f.target);const auto before=tree(ProjectPackageLayout::campaignsDirectory());
        const auto result=service::restoreBackup(f.selected);
        require(!result.success&&!result.errorMessage.empty(),"Destructive restore accepted "+mode+" orphan package identity");
        f.prior(false,true);require(tree(ProjectPackageLayout::campaignsDirectory())==before,"Ownership refusal changed managed backup tree");
    }
    Fixture removal("ownership_remove",false,false);const auto forward=service::restoreBackup(removal.selected);
    const auto undo=safety(forward,removal.a.metadata.id);removal.restored();
    write(removal.target/"project.json","NOT_A_PROJECT_MANIFEST");removal.bBytes=tree(removal.target);
    const auto before=tree(ProjectPackageLayout::campaignsDirectory());
    const auto result=service::restoreBackup(undo);
    require(!result.success&&!result.errorMessage.empty(),"Receipt-backed package removal accepted unknown current ownership");
    removal.restored();require(tree(ProjectPackageLayout::campaignsDirectory())==before,"Removal ownership refusal mutated backup tree");
    // A data-only restore must remain possible without touching an unknown package.
    for(const std::string mode:{"invalid","missing"}) {
        Fixture data("ownership_data_only_"+mode);
        if(mode=="missing")fs::remove(data.target/"project.json");else write(data.target/"project.json","NOT_A_PROJECT_MANIFEST");
        data.aBytes=tree(data.target);
        const auto snapshot=root/"Resources"/"Backups"/data.a.metadata.id/"before-save.db";
        fs::create_directories(snapshot.parent_path());fs::copy_file(mapFileIoPath(data.selected.snapshotPath),mapFileIoPath(snapshot));
        const auto selected=entry(snapshot,data.a.metadata.id);require(selected.packageAction==service::PackageRestoreAction::Keep,"Pre-save fixture not data-only");
        const auto restored=service::restoreBackup(selected);const auto undo=safety(restored,data.a.metadata.id);
        require(undo.packageAction==service::PackageRestoreAction::Keep&&undo.packageBackupPath.empty(),
            "Data-only safety reversal would modify untouched unknown package");
        checkCampaign(data.b,true);require(tree(data.target)==data.aBytes,"Data-only restore changed unknown package");data.invariants();
        const auto safetyBytes=tree(restored.safetyBackupPath);data.advanceOther("before_data_only_reversal");
        const auto reversed=service::restoreBackup(undo);const auto redo=safety(reversed,data.a.metadata.id);data.prior();
        require(redo.packageAction==service::PackageRestoreAction::Keep&&tree(restored.safetyBackupPath)==safetyBytes,
            "Data-only reversal changed source bytes or inferred a package action");
    }
}
}
