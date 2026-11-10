#pragma once
#include "backup_restore_delete_cases.hpp"
namespace backup_restore_test {
inline bool same(const fs::path &left,const fs::path &right)
{return mapFileIoPath(left).lexically_normal()==mapFileIoPath(right).lexically_normal();}
inline bool named(const fs::path &path,const std::string &prefix)
{return path.filename().string().find(prefix)!=std::string::npos;}
struct SqlFailure {
    SQLite::Database database;
    SqlFailure(const EntityId &id,const std::string &operation)
        :database(mapFileIoPath(ApplicationPaths::databasePath()).u8string(),SQLite::OPEN_READWRITE)
    {
        require(id.find('\'')==std::string::npos,"Unsafe SQL fixture identity");
        const auto row=operation=="DELETE"?"OLD":"NEW";
        database.exec("CREATE TRIGGER refuse_restore BEFORE "+operation+" ON campaigns WHEN "+row+".stable_id='"+id+
            "' BEGIN SELECT RAISE(ABORT,'injected restore persistence failure'); END;");
    }
    ~SqlFailure(){try{database.exec("DROP TRIGGER refuse_restore;");}catch(...) {}}
};
inline void sqlFailureAfterDisplacement()
{
    Fixture f("sql_update");auto ops=service::detail::systemOperations();const auto rename=ops.renameDirectory;
    bool displaced=false,installed=false;
    ops.renameDirectory=[&](const auto &from,const auto &to){rename(from,to);
        if(same(from,f.target))displaced=true;if(same(to,f.target)&&named(from,"restore-staging"))installed=true;};
    service::RestoreResult result;
    {SqlFailure failure(f.a.metadata.id,"UPDATE");result=service::detail::restoreWithOperations(f.selected,ops);}
    require(displaced&&installed&&!result.success&&result.errorMessage.find("injected restore persistence failure")!=std::string::npos,
        "SQL fault did not occur after actual package displacement/install: "+result.errorMessage);
    f.prior();require(fs::is_regular_file(result.safetyBackupPath/"restore-state.txt"),"SQL failure lost durable prior-state safety receipt");
    auto retry=service::restoreBackup(f.selected);safety(retry,f.a.metadata.id);f.restored();
    const auto originalMode=DatabaseCampaignDeletionProbe::mode();
    struct RestoreMode {int value;~RestoreMode(){try{DatabaseCampaignDeletionProbe::mode(value);}catch(...){}}} mode{originalMode};
    for(const int initialMode:{0,1}) {
        Fixture absent("sql_delete_"+std::to_string(initialMode),false,false,nestedCampaign);
        const auto forward=service::restoreBackup(absent.selected);const auto remove=safety(forward,absent.a.metadata.id);absent.restored();
        absent.advanceOther("after_removal_safety");const auto selectedChildren=ownedRows(absent.a.metadata.id),otherChildren=ownedRows(absent.other.metadata.id);
        requirePopulated(selectedChildren);requirePopulated(otherChildren);
        DatabaseCampaignDeletionProbe::mode(initialMode);require(DatabaseCampaignDeletionProbe::mode()==initialMode,"Cannot set initial deletion mode");
        ops=service::detail::systemOperations();const auto move=ops.renameDirectory;bool deletionDisplaced=false;
        ops.renameDirectory=[&](const auto &from,const auto &to){move(from,to);if(same(from,absent.target))deletionDisplaced=true;};
        {SqlFailure failure(absent.a.metadata.id,"DELETE");result=service::detail::restoreWithOperations(remove,ops);}
        require(deletionDisplaced&&!result.success&&result.errorMessage.find("injected restore persistence failure")!=std::string::npos,
            "Campaign-removal SQL fault did not follow package displacement: "+result.errorMessage);absent.restored();
        require(DatabaseCampaignDeletionProbe::mode()==initialMode&&ownedRows(absent.a.metadata.id)==selectedChildren&&ownedRows(absent.other.metadata.id)==otherChildren,
            "Failed deletion changed connection mode or partially deleted selected/unrelated children");
        const auto removed=service::restoreBackup(remove);safety(removed,absent.a.metadata.id);absent.prior(false,false);
        requireDeleted(selectedChildren);require(DatabaseCampaignDeletionProbe::mode()==initialMode&&ownedRows(absent.other.metadata.id)==otherChildren,
            "Successful deletion changed mode or newer unrelated nested rows");
    }
}
inline void copySwapAndRollbackFailures()
{
    Fixture copy("copy_failure");auto ops=service::detail::systemOperations();bool reached=false;
    ops.copyFile=[&](const auto &,const auto &){reached=true;throw std::runtime_error("injected copy refusal");};
    const auto copied=service::detail::restoreWithOperations(copy.selected,ops);
    require(reached&&!copied.success&&!copied.errorMessage.empty(),"Copy fault not reached/refused");copy.prior();
    Fixture swap("swap_failure");ops=service::detail::systemOperations();const auto rename=ops.renameDirectory;bool displaced=false,swapInstallFailed=false;
    ops.renameDirectory=[&](const auto &from,const auto &to){
        if(same(to,swap.target)&&named(from,"restore-staging")){swapInstallFailed=true;throw std::runtime_error("injected install refusal");}
        rename(from,to);if(same(from,swap.target))displaced=true;
    };
    const auto swapped=service::detail::restoreWithOperations(swap.selected,ops);
    require(displaced&&swapInstallFailed&&!swapped.success,"Swap failure did not follow actual displacement");swap.prior();
    Fixture rollback("rollback_failure");ops=service::detail::systemOperations();const auto actual=ops.renameDirectory;
    fs::path retained;bool installFailed=false,rollbackFailed=false;
    ops.renameDirectory=[&](const auto &from,const auto &to){
        if(same(to,rollback.target)&&named(from,"restore-staging")){installFailed=true;throw std::runtime_error("injected install refusal");}
        if(same(to,rollback.target)&&named(from,"restore-displaced")){rollbackFailed=true;throw std::runtime_error("injected rollback refusal");}
        actual(from,to);if(same(from,rollback.target))retained=to;
    };
    const auto failed=service::detail::restoreWithOperations(rollback.selected,ops);
    require(installFailed&&rollbackFailed&&!failed.success&&!failed.errorMessage.empty(),"Rollback fault not reached/reported");
    require(!retained.empty()&&tree(retained)==rollback.aBytes,"Rollback failure deleted displaced usable package");
    require(std::any_of(failed.retainedRecoveryPaths.begin(),failed.retainedRecoveryPaths.end(),[&](const auto &p){return same(p,retained);}),
        "Rollback failure did not expose exact retained prior-package path");
    checkCampaign(rollback.a,true);rollback.invariants();
    const auto recover=entry(failed.safetyBackupPath/"repository-before-restore.db",rollback.a.metadata.id);
    const auto recovered=service::restoreBackup(recover);safety(recovered,rollback.a.metadata.id);rollback.prior();
    require(tree(retained)==rollback.aBytes,"Recovering from safety erased separately retained failed-rollback evidence");
}
inline void cleanupWarningAndLinkedSource()
{
    Fixture cleanup("cleanup_warning");auto ops=service::detail::systemOperations();const auto remove=ops.removeDirectory;
    fs::path retained;
    ops.removeDirectory=[&](const auto &path){
        if(named(path,"restore-displaced")){retained=path;throw std::runtime_error("injected cleanup refusal");}remove(path);
    };
    const auto result=service::detail::restoreWithOperations(cleanup.selected,ops);
    require(result.success&&!result.warnings.empty()&&!retained.empty(),"Postcommit cleanup failure was silent or misreported as uncommitted");
    cleanup.restored();require(tree(retained)==cleanup.aBytes,"Cleanup failure removed previous package bytes");
    require(std::any_of(result.retainedRecoveryPaths.begin(),result.retainedRecoveryPaths.end(),[&](const auto &p){return same(p,retained);}),
        "Cleanup warning omitted retained path");
    Fixture link("linked_source");const auto location=link.source/"package-before-import.ttrpgpkg"/"Assets"/"linked";
    std::error_code error;fs::create_directory_symlink(ProjectPackageLayout::campaignPackageDirectory(link.other)/"Assets",location,error);
    if(error){std::cout<<"SKIP backup restore symlink fixture: "<<error.message()<<'\n';return;}
    const auto rejected=service::restoreBackup(link.selected);
    require(!rejected.success&&!rejected.errorMessage.empty(),"Linked source package accepted");
    fs::remove(location);link.prior();
}
}
