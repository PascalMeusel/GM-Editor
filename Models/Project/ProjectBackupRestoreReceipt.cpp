#include "Models/Project/ProjectBackupRestoreInternal.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include <charconv>
#include <array>
#include <fstream>

namespace ProjectBackupRestore::detail {
namespace {
void field(std::string &out,const std::string &value){require(value.size()<=4096,"Restore receipt field is too long.");out+=std::to_string(value.size())+":"+value;}
std::string take(const std::string &bytes,std::size_t &at){
    const auto end=bytes.find(':',at);require(end!=std::string::npos&&end>at&&end-at<8,"Invalid restore receipt length.");
    std::size_t size=0;const auto parsed=std::from_chars(bytes.data()+at,bytes.data()+end,size);
    require(parsed.ec==std::errc{}&&parsed.ptr==bytes.data()+end&&size<=4096&&size<=bytes.size()-end-1,"Invalid restore receipt field.");
    at=end+1;auto value=bytes.substr(at,size);at+=size;return value;
}
bool flag(const std::string &s){require(s=="0"||s=="1","Invalid restore receipt presence flag.");return s=="1";}
void validate(const Receipt &r){
    require(!r.campaignId.empty()&&r.campaignId.size()<=512&&r.campaignId.find('\0')==std::string::npos&&
        !r.campaignName.empty()&&r.campaignName.size()<=4096&&r.campaignName.find('\0')==std::string::npos,"Invalid restore receipt identity.");
    require(r.timestampUtc.size()==20&&r.timestampUtc[4]=='-'&&r.timestampUtc[7]=='-'&&r.timestampUtc[10]=='T'&&
        r.timestampUtc[13]==':'&&r.timestampUtc[16]==':'&&r.timestampUtc[19]=='Z',"Invalid restore receipt timestamp.");
    for(std::size_t i=0;i<19;++i)if(i!=4&&i!=7&&i!=10&&i!=13&&i!=16)
        require(r.timestampUtc[i]>='0'&&r.timestampUtc[i]<='9',"Invalid restore receipt timestamp.");
    require(!r.databaseFingerprint.empty()&&(r.packagePresent?!r.packageFingerprint.empty():r.packageFingerprint.empty()),"Incomplete restore receipt fingerprints.");
}
}
std::string receiptBytes(const Receipt &r){
    validate(r);std::string out;for(const auto &s:{std::string("GM_BEFORE_RESTORE_1"),r.campaignId,r.campaignName,r.timestampUtc,
        std::string(r.campaignPresent?"1":"0"),std::string(r.packagePresent?"1":"0"),std::string(r.packageChanged?"1":"0"),r.databaseFingerprint,r.packageFingerprint})field(out,s);return out;
}
Receipt readReceipt(const fs::path &root){
    const auto path=root/ReceiptFile;require(regular(path)&&fs::file_size(mapFileIoPath(path))<=65536,"Restore receipt is missing or too large.");
    std::ifstream input(mapFileIoPath(path),std::ios::binary);require(bool(input),"Cannot read restore receipt.");
    std::string bytes;std::array<char,4096> buffer{};while(input){input.read(buffer.data(),buffer.size());
        const auto n=static_cast<std::size_t>(input.gcount());require(n<=65536-bytes.size(),"Restore receipt grew beyond its limit.");bytes.append(buffer.data(),n);}
    require(!input.bad(),"Could not finish reading restore receipt.");std::size_t at=0;require(take(bytes,at)=="GM_BEFORE_RESTORE_1","Unknown restore receipt version.");
    Receipt r;r.campaignId=take(bytes,at);r.campaignName=take(bytes,at);r.timestampUtc=take(bytes,at);r.campaignPresent=flag(take(bytes,at));
    r.packagePresent=flag(take(bytes,at));r.packageChanged=flag(take(bytes,at));r.databaseFingerprint=take(bytes,at);r.packageFingerprint=take(bytes,at);
    require(at==bytes.size(),"Trailing restore receipt bytes.");validate(r);return r;
}
void writeReceipt(const fs::path &root,const Receipt &r){
    safePath(root/ReceiptFile);require(!fs::exists(mapFileIoPath(root/ReceiptFile)),"Safety receipt already exists.");
    AtomicProjectFile::write(root/ReceiptFile,receiptBytes(r));
}
void bindEntry(BackupEntry &entry){
    std::string value;const auto add=[&](const std::string &s){field(value,s);};
    add(std::to_string(static_cast<int>(entry.kind)));add(fs::absolute(entry.snapshotPath).lexically_normal().generic_u8string());
    add(entry.campaignId);add(entry.campaignName);add(entry.campaignPresent?"1":"0");add(std::to_string(static_cast<int>(entry.packageAction)));
    add(entry.legacySafety?"1":"0");add(fileFingerprint(entry.snapshotPath));
    if(entry.packageAction==PackageRestoreAction::Restore){add(fs::absolute(entry.packageBackupPath).lexically_normal().generic_u8string());
        add(inventoryFingerprint(inventory(entry.packageBackupPath)));}
    if(entry.kind==BackupKind::BeforeRestore&&!entry.legacySafety)add(fileFingerprint(entry.snapshotPath.parent_path()/ReceiptFile));
    entry.selectionFingerprint=fingerprint(value);
}
BackupEntry resolveBeforeRestore(const fs::path &root){
    require(inside(root,restoreRoot())&&samePath(root.parent_path(),restoreRoot())&&directory(root),"Invalid safety backup directory.");
    const auto r=readReceipt(root);const auto snapshot=root/RestoreDatabase,package=root/RestorePackage;
    require(root.filename().u8string().rfind(ProjectAssets::sanitizePathSegment(r.campaignId)+"-",0)==0,"Safety directory identity differs from its receipt.");
    require(fileFingerprint(snapshot)==r.databaseFingerprint,"Safety database changed since capture.");
    const auto campaigns=inspectSnapshot(snapshot);std::size_t matches=0;
    for(const auto &c:campaigns)if(c.id==r.campaignId){++matches;require(c.name==r.campaignName,"Safety database campaign name differs from its receipt.");}
    require(matches==(r.campaignPresent?1u:0u),"Safety database campaign presence differs from its receipt.");
    if(r.packagePresent)require(inventoryFingerprint(inventory(package))==r.packageFingerprint,"Safety package bytes changed since capture.");
    else {safePath(package);require(!fs::exists(mapFileIoPath(package)),"Receipt declares no package but a package is present.");}
    BackupEntry e;e.kind=BackupKind::BeforeRestore;e.snapshotPath=snapshot;e.campaignId=r.campaignId;e.campaignName=r.campaignName;
    e.timestampUtc=r.timestampUtc;e.label="Before restore — "+r.timestampUtc;e.campaignPresent=r.campaignPresent;
    e.packageAction=!r.packageChanged?PackageRestoreAction::Keep:(r.packagePresent?PackageRestoreAction::Restore:PackageRestoreAction::Remove);
    if(r.packagePresent&&r.packageChanged)e.packageBackupPath=package;e.byteCount=fs::file_size(mapFileIoPath(snapshot));bindEntry(e);return e;
}
}
