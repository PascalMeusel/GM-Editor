#include "CampaignMarkdownIo.hpp"
#include "CampaignMarkdownDocuments.hpp"
#include "CampaignMarkdownFiles.hpp"
#include <set>

namespace CampaignMarkdownExchange::detail
{
std::string editablePath(const EntityId &id){return "editable/"+safeId(id)+".md";}
std::string baselinePath(const EntityId &id){return "baseline/"+safeId(id)+".utf8";}
namespace {
constexpr auto Header="TTRPG_MARKDOWN_EDITABLE_V1\n";
void field(std::string &out,const std::string &text,std::size_t max)
{
    const auto size=std::to_string(text.size());require(size.size()+1<=max-out.size()&&text.size()<=max-out.size()-size.size()-1,"Editable manifest exceeds 8 MiB.");
    out+=size;out+='\n';out+=text;
}
std::size_t number(const std::string &s,std::size_t max)
{
    require(!s.empty()&&s.size()<=20,"Invalid manifest number.");std::size_t n=0;
    for(unsigned char c:s){require(c>='0'&&c<='9'&&n<=max/10,"Invalid or oversized manifest number.");n=n*10+(c-'0');require(n<=max,"Manifest number exceeds its limit.");}return n;
}
struct Reader {
    const std::string &bytes;std::size_t at=0;
    std::string get(std::size_t max){const auto end=bytes.find('\n',at);require(end!=std::string::npos&&end-at<=20,"Malformed manifest length.");
        const auto n=number(bytes.substr(at,end-at),max);at=end+1;require(n<=bytes.size()-at,"Truncated manifest field.");auto value=bytes.substr(at,n);at+=n;return value;}
};
}
void writeEditable(const ExportInput &input,const std::filesystem::path &stage,const Io &io)
{
    require(input.editableNotes.size()<=io.limits.notes,"Editable export exceeds 10,000 notes.");
    std::string manifest=Header;require(manifest.size()<=io.limits.manifestBytes,"Manifest budget is too small.");
    field(manifest,input.campaignId,io.limits.manifestBytes);field(manifest,std::to_string(input.editableNotes.size()),io.limits.manifestBytes);
    std::size_t total=0;std::set<EntityId> ids;
    for(const auto &note:input.editableNotes){io.check();require(ids.insert(note.id).second,"Duplicate editable note ID.");
        require(note.title.size()<=4096&&note.text.size()<=io.limits.noteBytes&&note.text.size()<=(io.limits.rawBytes-total)/2,"Editable note text exceeds the 4 MiB/64 MiB aggregate limits.");total+=note.text.size()*2;
        require(utf8(note.text),"Editable text must be UTF-8 without NUL bytes.");
        for(const auto &v:std::vector<std::string>{note.id,"story-thread-gm-notes",note.title,editablePath(note.id),baselinePath(note.id),std::to_string(note.text.size()),digest(note.text)})field(manifest,v,io.limits.manifestBytes);
    }
    std::filesystem::create_directory(mapFileIoPath(stage/"editable"));std::filesystem::create_directory(mapFileIoPath(stage/"baseline"));
    for(const auto &note:input.editableNotes){io.write(stage/editablePath(note.id),note.text);io.write(stage/baselinePath(note.id),note.text);}
    io.write(stage/ManifestName,manifest);
}
UpdatePreview readUpdates(const EntityId &campaign,const std::filesystem::path &folder,const Io &io)
{
    UpdatePreview result;result.sourceDirectory=folder;
    const auto manifest=io.read(folder/ManifestName,io.limits.manifestBytes);
    require(manifest.rfind(Header,0)==0,"Unsupported editable-note manifest version.");Reader r{manifest,std::char_traits<char>::length(Header)};
    result.campaignId=r.get(96);require(result.campaignId==campaign&&!campaign.empty(),"This editable export belongs to a different campaign.");
    const auto count=number(r.get(20),io.limits.notes);require(count<=(manifest.size()-r.at)/14,"Manifest count exceeds available fields.");
    result.files.push_back({ManifestName,manifest});std::set<EntityId> ids;std::size_t total=0;
    for(std::size_t i=0;i<count;++i){io.check();UpdateEntry entry;entry.id=r.get(96);require(ids.insert(entry.id).second,"Duplicate editable note identity.");
        require(r.get(64)=="story-thread-gm-notes","Unsupported editable note kind.");entry.title=r.get(4096);
        const auto edited=r.get(256),base=r.get(256);require(edited==editablePath(entry.id)&&base==baselinePath(entry.id),"Manifest paths do not match their stable note identity.");
        const auto bytes=number(r.get(20),io.limits.noteBytes);entry.baseHash=r.get(64);
        require(bytes<=io.limits.rawBytes-total,"Editable base and edited text exceed 64 MiB.");
        entry.baseText=io.read(folder/base,std::min(io.limits.noteBytes,io.limits.rawBytes-total));total+=entry.baseText.size();
        require(entry.baseText.size()==bytes&&digest(entry.baseText)==entry.baseHash,"Editable baseline bytes no longer match the manifest.");
        entry.editedText=io.read(folder/edited,std::min(io.limits.noteBytes,io.limits.rawBytes-total));total+=entry.editedText.size();
        result.files.push_back({base,entry.baseText});result.files.push_back({edited,entry.editedText});result.entries.push_back(std::move(entry));
    }
    require(r.at==manifest.size(),"Unexpected trailing manifest data.");result.success=true;return result;
}
void checkUpdates(const UpdatePreview &review,const Io &io)
{
    require(review.success,"Review exported note updates first.");
    for(const auto &file:review.files){io.check();const auto max=file.relativePath==ManifestName?io.limits.manifestBytes:io.limits.noteBytes;
        require(io.read(review.sourceDirectory/std::filesystem::u8path(file.relativePath),max)==file.bytes,"Editable files changed after review. Refresh before applying.");}
    const auto fresh=readUpdates(review.campaignId,review.sourceDirectory,io);
    require(fresh.files.size()==review.files.size(),"Editable file inventory changed after review.");
}
}
