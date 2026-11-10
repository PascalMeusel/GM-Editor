#include "Models/Project/ProjectBackupRestoreInternal.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectDirectoryPublish.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Persistence/DatabaseSchemaPreflight.hpp"
#include "SQLiteCpp/SQLiteCpp.h"
#include <array>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace ProjectBackupRestore::detail {
namespace {
constexpr std::uintmax_t MaxBytes=16ull*1024*1024*1024;
struct Digest {
    std::uint64_t value=14695981039346656037ull;
    void add(const char *data,std::size_t size){for(std::size_t i=0;i<size;++i){value^=static_cast<unsigned char>(data[i]);value*=1099511628211ull;}}
    std::string str() const {std::ostringstream s;s<<"fnv1a64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<value;return s.str();}
};
bool componentEqual(const fs::path &a,const fs::path &b){
#ifdef _WIN32
    auto x=a.native(),y=b.native();std::transform(x.begin(),x.end(),x.begin(),::towlower);std::transform(y.begin(),y.end(),y.begin(),::towlower);return x==y;
#else
    return a==b;
#endif
}
}
void require(bool ok,const std::string &message){if(!ok)throw std::runtime_error(message);}
fs::path preSaveRoot(){return ProjectPackageLayout::findProjectRoot()/"Resources/Backups";}
fs::path importRoot(){return ProjectPackageLayout::campaignsDirectory()/"ImportBackups";}
fs::path restoreRoot(){return ProjectPackageLayout::campaignsDirectory()/"RestoreBackups";}
bool samePath(const fs::path &a,const fs::path &b){return componentEqual(fs::absolute(a).lexically_normal(),fs::absolute(b).lexically_normal());}
bool inside(const fs::path &p,const fs::path &r){
    auto path=fs::absolute(p).lexically_normal(),root=fs::absolute(r).lexically_normal();auto next=path.begin();
    for(const auto &part:root)if(next==path.end()||!componentEqual(*next++,part))return false;
    return next!=path.end();
}
void safePath(const fs::path &path){
    for(const auto &part:path)require(part!="..","Backup paths cannot contain parent traversal.");
    const auto absolute=fs::absolute(path).lexically_normal();auto current=absolute.root_path();
    const auto inspect=[&](bool leaf){
        std::error_code ec;const auto status=fs::symlink_status(mapFileIoPath(current),ec);
        if(ec&&ec!=std::errc::no_such_file_or_directory)throw fs::filesystem_error("Inspect backup path",current,ec);
        require(!fs::is_symlink(status),"Backup paths cannot contain symbolic links.");
        if(!fs::exists(status))return;
#ifdef _WIN32
        const auto flags=GetFileAttributesW(mapFileIoPath(current).c_str());
        if(flags==INVALID_FILE_ATTRIBUTES)throw std::system_error(GetLastError(),std::system_category(),"Inspect backup attributes");
        require(!(flags&FILE_ATTRIBUTE_REPARSE_POINT),"Backup paths cannot contain junctions or reparse points.");
#endif
        require(leaf?(fs::is_regular_file(status)||fs::is_directory(status)):fs::is_directory(status),"Backup path contains an unsupported file or obstructed directory.");
    };
    inspect(false);for(const auto &part:absolute.relative_path()){current/=part;inspect(current==absolute);}
}
bool regular(const fs::path &p){safePath(p);return fs::is_regular_file(mapFileIoPath(p));}
bool directory(const fs::path &p){safePath(p);return fs::is_directory(mapFileIoPath(p));}
std::string fingerprint(const std::string &s){Digest hash;hash.add(s.data(),s.size());return hash.str()+":"+std::to_string(s.size());}
std::string fileFingerprint(const fs::path &path){
    require(regular(path),"Backup file is missing or is not a regular file.");
    const auto expected=fs::file_size(mapFileIoPath(path));require(expected<=MaxBytes,"Backup file exceeds the 16 GiB review limit.");
    std::ifstream input(mapFileIoPath(path),std::ios::binary);require(bool(input),"Cannot read backup file.");
    Digest hash;std::array<char,65536> buffer{};std::uintmax_t size=0;
    while(input){input.read(buffer.data(),buffer.size());const auto n=static_cast<std::size_t>(input.gcount());
        require(n<=MaxBytes-size,"Backup file grew beyond the review limit.");size+=n;hash.add(buffer.data(),n);}
    require(!input.bad()&&size==expected&&fs::file_size(mapFileIoPath(path))==expected,"Backup file changed or could not be fully read.");
    return hash.str()+":"+std::to_string(size);
}
Inventory inventory(const fs::path &root){
    require(directory(root),"Backup package directory is missing.");Inventory result;std::uintmax_t total=0;std::size_t names=0;
    const auto nativeRoot=mapFileIoPath(root);
    for(const auto &entry:fs::recursive_directory_iterator(nativeRoot)){
        const auto relativePath=entry.path().lexically_relative(nativeRoot);
        require(!relativePath.empty()&&!relativePath.is_absolute(),"Backup inventory entry is outside its directory.");
        for(const auto &part:relativePath)require(part!=".."&&part!=".","Backup inventory entry contains traversal.");
        // Iterators over Windows I/O paths return the extended device prefix.
        // Validate ordinary ancestors, not the synthetic \\?\ root components;
        // extend only when performing filesystem or stream operations.
        const auto ordinary=root/relativePath;safePath(ordinary);const auto relative=relativePath.generic_u8string();
        require(!relative.empty()&&relative.size()<=4096&&result.size()<50000&&relative.size()<=8u*1024u*1024u-names,"Backup package inventory exceeds its bounds.");names+=relative.size();
        FileStamp stamp;stamp.directory=entry.is_directory();
        if(!stamp.directory){require(entry.is_regular_file(),"Unsupported backup package entry.");stamp.bytes=entry.file_size();
            require(stamp.bytes<=MaxBytes-total,"Backup package exceeds the 16 GiB review limit.");total+=stamp.bytes;stamp.hash=fileFingerprint(ordinary);}
        require(result.emplace(relative,std::move(stamp)).second,"Duplicate package inventory path.");
    }return result;
}
std::string inventoryFingerprint(const Inventory &items){
    std::string serial;for(const auto &[name,stamp]:items){serial+=std::to_string(name.size())+":"+name+(stamp.directory?"D":"F")+
        std::to_string(stamp.bytes)+":"+stamp.hash+"\n";}return fingerprint(serial);
}
void copyTree(const fs::path &source,const fs::path &target,const Operations &operations){
    require(!samePath(source,target)&&!inside(target,source)&&!inside(source,target),"Backup copy source and destination overlap.");
    const auto expected=inventory(source);safePath(target);require(!fs::exists(mapFileIoPath(target)),"Staging destination already exists.");
    fs::create_directories(mapFileIoPath(target));
    for(const auto &[name,stamp]:expected){const auto to=target/fs::u8path(name),from=source/fs::u8path(name);safePath(from);safePath(to);
        if(stamp.directory)fs::create_directories(mapFileIoPath(to));else{fs::create_directories(mapFileIoPath(to.parent_path()));operations.copyFile(from,to);}}
    require(inventory(target)==expected&&inventory(source)==expected,"Backup package changed during copying or copied bytes differ.");
}
void removeOwnedTree(const fs::path &path,const Operations &operations){
    require(inside(path,ProjectPackageLayout::campaignsDirectory()),"Refusing to remove a directory outside managed campaigns.");
    safePath(path);if(!fs::exists(mapFileIoPath(path)))return;inventory(path);operations.removeDirectory(path);
    require(!fs::exists(mapFileIoPath(path)),"Managed cleanup did not remove its directory.");
}
std::vector<SnapshotCampaign> inspectSnapshot(const fs::path &path){
    require(regular(path),"Backup database is missing.");
    std::array<unsigned char,20> header{};
    {std::ifstream input(mapFileIoPath(path),std::ios::binary);input.read(reinterpret_cast<char *>(header.data()),header.size());
        require(input.gcount()==static_cast<std::streamsize>(header.size()),"Backup database header is incomplete.");}
    require(header[18]==1&&header[19]==1,"Backup database is not a self-contained rollback-journal snapshot. Export a fresh snapshot before restoring.");
    // Managed snapshots are self-contained. A journal/WAL can change SQLite's
    // read state without changing the reviewed .db bytes, so refuse live pairs.
    for(const char *suffix:{"-journal","-wal","-shm"}){auto sidecar=path;sidecar+=suffix;safePath(sidecar);
        require(!fs::exists(mapFileIoPath(sidecar)),"Backup database has a live SQLite sidecar. Close its writer and create a self-contained snapshot first.");}
    SQLite::Database db(mapFileIoPath(path).u8string(),SQLite::OPEN_READONLY);
    DatabaseSchema::supportedVersion(db);
    SQLite::Statement check(db,"PRAGMA integrity_check;");require(check.executeStep()&&check.getColumn(0).getString()=="ok"&&!check.executeStep(),"Backup failed its database integrity check.");
    std::vector<SnapshotCampaign> result;SQLite::Statement q(db,"SELECT stable_id,name FROM campaigns ORDER BY stable_id;");
    while(q.executeStep()){require(result.size()<10000,"Backup contains too many campaigns.");
        if(!q.getColumn(0).isNull()&&!q.getColumn(1).isNull())result.push_back({q.getColumn(0).getString(),q.getColumn(1).getString()});}return result;
}
std::string timestamp(const fs::path &path){
    const auto fileTime=fs::last_write_time(mapFileIoPath(path));const auto point=std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fileTime-fs::file_time_type::clock::now()+std::chrono::system_clock::now());const auto time=std::chrono::system_clock::to_time_t(point);std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc,&time);
#else
    gmtime_r(&time,&utc);
#endif
    std::ostringstream s;s<<std::put_time(&utc,"%Y-%m-%dT%H:%M:%SZ");return s.str();
}
Operations systemOperations(){return {
    [](const fs::path &a,const fs::path &b){fs::copy_file(mapFileIoPath(a),mapFileIoPath(b),fs::copy_options::none);},
    [](const fs::path &a,const fs::path &b){ProjectDirectoryPublish::rename(mapFileIoPath(a),mapFileIoPath(b));},
    [](const fs::path &p){fs::remove_all(mapFileIoPath(p));}};}
}
