#include "CampaignMarkdownIo.hpp"
#include "CampaignMarkdownFiles.hpp"
#include <array>
#include <iomanip>
#include <sstream>

namespace CampaignMarkdownExchange::detail
{
namespace {
struct Hash {
    std::uint64_t value=14695981039346656037ull;
    void add(const char *p,std::size_t n){for(std::size_t i=0;i<n;++i){value^=static_cast<unsigned char>(p[i]);value*=1099511628211ull;}}
    std::string str() const {std::ostringstream s;s<<"fnv1a64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<value;return s.str();}
};
}
void safePath(const std::filesystem::path &input)
{
    require(input.is_absolute(),"Exchange paths must be absolute.");
    for(const auto &part:input)require(part!="..","Exchange paths cannot contain parent traversal.");
    const auto path=input.lexically_normal();auto current=path.root_path();
    const auto check=[&](bool leaf) {
        std::error_code ec;const auto status=std::filesystem::symlink_status(mapFileIoPath(current),ec);
        if(ec&&ec!=std::errc::no_such_file_or_directory)throw std::filesystem::filesystem_error("Inspect exchange path",current,ec);
        require(!std::filesystem::is_symlink(status),"Exchange does not follow symbolic links.");
        if(!std::filesystem::exists(status))return;
#ifdef _WIN32
        const auto flags=GetFileAttributesW(mapFileIoPath(current).c_str());
        if(flags==INVALID_FILE_ATTRIBUTES)throw std::system_error(GetLastError(),std::system_category(),"Inspect exchange attributes");
        require(!(flags&FILE_ATTRIBUTE_REPARSE_POINT),"Exchange does not follow junctions or reparse points.");
#endif
        require(leaf?(std::filesystem::is_directory(status)||std::filesystem::is_regular_file(status)):std::filesystem::is_directory(status),"Exchange path contains an obstructed directory or unsupported file.");
    };
    check(false);for(const auto &part:path.relative_path()){current/=part;check(current==path);}
}
std::string digest(const std::string &bytes){Hash h;h.add(bytes.data(),bytes.size());return h.str();}
void Io::check() const {if(cancelled&&cancelled())throw Cancelled();}
void Io::report(std::uint64_t bytes,std::uint64_t files) const {check();if(progress)progress(phase,bytes,files);}
std::string Io::read(const std::filesystem::path &path,std::size_t max,bool text) const
{
    check();safePath(path);require(std::filesystem::is_regular_file(mapFileIoPath(path)),"Select an existing regular exchange file.");
    const auto size=std::filesystem::file_size(mapFileIoPath(path));require(size<=max,"Exchange file exceeds its byte limit (notes: 4 MiB; manifest: 8 MiB).");
    std::ifstream in(mapFileIoPath(path),std::ios::binary);require(bool(in),"Cannot read exchange file.");
    std::string result;result.reserve(static_cast<std::size_t>(size));std::array<char,65536> chunk{};
    while(in){check();in.read(chunk.data(),chunk.size());const auto n=static_cast<std::size_t>(in.gcount());
        require(n<=max-result.size(),"Exchange file grew beyond its byte limit.");result.append(chunk.data(),n);report(n);}
    require(!in.bad()&&result.size()==size&&std::filesystem::file_size(mapFileIoPath(path))==size,"Exchange file changed or could not be fully read.");
    safePath(path);require(!text||utf8(result),"Markdown must be UTF-8 without NUL bytes.");report(0,1);return result;
}
std::string Io::hash(const std::filesystem::path &path,std::uintmax_t expected) const
{
    check();safePath(path);require(std::filesystem::is_regular_file(mapFileIoPath(path))&&std::filesystem::file_size(mapFileIoPath(path))==expected,"Attachment changed or is unavailable.");
    std::ifstream in(mapFileIoPath(path),std::ios::binary);require(bool(in),"Cannot read attachment.");
    Hash h;std::array<char,65536> chunk{};std::uintmax_t total=0;
    while(in){check();in.read(chunk.data(),chunk.size());const auto n=static_cast<std::size_t>(in.gcount());
        require(n<=expected-total,"Attachment grew while being read.");total+=n;h.add(chunk.data(),n);report(n);}
    require(!in.bad()&&total==expected&&std::filesystem::file_size(mapFileIoPath(path))==expected,"Attachment changed or could not be fully read.");safePath(path);report(0,1);return h.str();
}
void Io::write(const std::filesystem::path &path,const std::string &text) const
{
    check();safePath(path);require(!std::filesystem::exists(mapFileIoPath(path)),"An unexpected staging file already exists.");
    std::ofstream out(mapFileIoPath(path),std::ios::binary);require(bool(out),"Cannot create exchange staging file.");
    for(std::size_t i=0;i<text.size();){check();const auto n=std::min<std::size_t>(65536,text.size()-i);out.write(text.data()+i,n);require(bool(out),"Cannot write exchange staging file.");i+=n;report(n);}
    out.close();require(bool(out),"Cannot close exchange staging file.");report(0,1);
}
void Io::copy(const std::filesystem::path &source,const std::filesystem::path &target,const Attachment &asset) const
{
    check();safePath(source);safePath(target);
    require(std::filesystem::is_regular_file(mapFileIoPath(source))&&std::filesystem::file_size(mapFileIoPath(source))==asset.byteSize&&!std::filesystem::exists(mapFileIoPath(target)),"Selected attachment changed or staging is occupied.");
    std::ifstream in(mapFileIoPath(source),std::ios::binary);std::ofstream out(mapFileIoPath(target),std::ios::binary);require(bool(in)&&bool(out),"Cannot copy selected attachment.");
    Hash h;std::array<char,65536> chunk{};std::uintmax_t total=0;
    while(in){check();in.read(chunk.data(),chunk.size());const auto n=static_cast<std::size_t>(in.gcount());require(n<=asset.byteSize-total,"Attachment grew while copying.");
        total+=n;h.add(chunk.data(),n);out.write(chunk.data(),n);require(bool(out),"Cannot write selected attachment.");report(n);}
    out.close();require(!in.bad()&&bool(out)&&total==asset.byteSize&&h.str()==asset.contentHash,"Attachment changed while copying; nothing was published.");
    safePath(source);require(hash(target,asset.byteSize)==asset.contentHash,"Staged attachment verification failed.");report(0,1);
}
}
