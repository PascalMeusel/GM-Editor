#pragma once
#include "Models/Project/ProjectBackupRestoreOperations.hpp"
#include "Models/Campaign/Campaign.hpp"
#include <map>

namespace ProjectBackupRestore::detail {
namespace fs=std::filesystem;
inline constexpr const char *ReceiptFile="restore-state.txt";
inline constexpr const char *RestoreDatabase="repository-before-restore.db";
inline constexpr const char *RestorePackage="package-before-restore.ttrpgpkg";
struct SnapshotCampaign { std::string id,name; };
struct FileStamp {
    bool directory=false;
    std::uintmax_t bytes=0;
    std::string hash;
    bool operator==(const FileStamp &other) const {return directory==other.directory&&bytes==other.bytes&&hash==other.hash;}
};
using Inventory=std::map<std::string,FileStamp>;
struct Receipt {
    std::string campaignId,campaignName,timestampUtc,databaseFingerprint,packageFingerprint;
    bool campaignPresent=false,packagePresent=false,packageChanged=false;
};
fs::path preSaveRoot();
fs::path importRoot();
fs::path restoreRoot();
void require(bool,const std::string &);
bool samePath(const fs::path &,const fs::path &);
bool inside(const fs::path &,const fs::path &);
void safePath(const fs::path &);
bool regular(const fs::path &);
bool directory(const fs::path &);
std::string fileFingerprint(const fs::path &);
std::string fingerprint(const std::string &);
std::string timestamp(const fs::path &);
Inventory inventory(const fs::path &);
std::string inventoryFingerprint(const Inventory &);
void copyTree(const fs::path &,const fs::path &,const Operations &);
void removeOwnedTree(const fs::path &,const Operations &);
std::vector<SnapshotCampaign> inspectSnapshot(const fs::path &);
std::string receiptBytes(const Receipt &);
Receipt readReceipt(const fs::path &);
void writeReceipt(const fs::path &,const Receipt &);
BackupEntry resolveBeforeRestore(const fs::path &);
std::vector<BackupEntry> discover(std::vector<std::string> *warnings=nullptr);
BackupEntry currentSelection(const BackupEntry &);
void bindEntry(BackupEntry &);
}
