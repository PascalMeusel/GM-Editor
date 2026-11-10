#include "editor_draft_recovery_fixture.hpp"
#include <limits>

using namespace editor_recovery_test;
namespace {
void codec(){
    const auto c=campaign();const auto s=fixture();const auto encoded=encode(c.metadata.id,s);require(encoded.success,encoded.error);
    const auto decoded=decode(c.metadata.id,encoded.payload);require(decoded.success,decoded.error);compare(s,decoded.drafts);
    require(!decode("other",encoded.payload).success&&!decode(c.metadata.id,encoded.payload+"x").success,"Wrong identity/trailing data accepted");
    for(std::size_t n=0;n<encoded.payload.size();n+=71)require(!decode(c.metadata.id,encoded.payload.substr(0,n)).success,"Truncated payload accepted");
    auto bad=s;bad.items.begin()->second.item.weight=std::numeric_limits<double>::infinity();require(!encode(c.metadata.id,bad).success,"Infinite value accepted");
    bad=s;bad.items.begin()->second.item.effects.assign(8200,"x");require(!encode(c.metadata.id,bad).success,"Aggregate record bound omitted");
    bad=s;bad.items.begin()->second.item.description.assign(2u*1024u*1024u+1,'x');require(!encode(c.metadata.id,bad).success,"Field bound omitted");
    bad=s;bad.items.begin()->second.base->metadata.id="foreign";require(!encode(c.metadata.id,bad).success,"Foreign baseline accepted");
    bad=s;bad.items.begin()->second.item.metadata.customProperties[""]="invalid";require(!encode(c.metadata.id,bad).success,"Empty property key encoded unreadable checkpoint");
    auto current=s.items.begin()->second.item,unchanged=current;current.metadata.updatedAtUtc="later";
    current.metadata.customProperties["gm.context.place"]="external";require(sameItem(current,unchanged),"Unrelated context/timestamp caused conflict");
    current.description+="different";require(!sameItem(current,unchanged),"Authored change bypasses baseline guard");
}
void storage(){
    const auto c=campaign();const auto s=fixture();require(save(c,s).empty(),"Cannot save checkpoint");const auto original=bytes(path(c));
    auto r=read(c);require(r.status==ReadStatus::Ready&&r.rawAvailable&&r.rawBytes==original,"Checkpoint unavailable");compare(s,r.drafts);
    const auto readable=copyCheckpoint(r);require(readable.find("PRIVATE_ITEM")!=std::string::npos&&readable.find("GM_NOTES")!=std::string::npos&&
        readable.find("\\0")!=std::string::npos&&readable.find('\0')==std::string::npos,"Valid copy lacks readable authored text or truncates at NUL");
    require(TextDraftRecovery::save(c,"other-key",{{"secret","untouched"}}).empty(),"Cannot save unrelated key");
    auto bad=s;bad.items.begin()->second.item.name.assign(2u*1024u*1024u+1,'x');
    require(!save(c,bad).empty()&&bytes(path(c))==original,"Failed encoding overwrote checkpoint");
    for(const auto &raw:std::vector<std::string>{"",std::string("bad\0",4),original.substr(0,original.size()-1)}){
        AtomicProjectFile::write(path(c),raw);r=read(c);require(r.status==ReadStatus::Invalid&&r.rawAvailable&&r.rawBytes==raw&&!copyCheckpoint(r).empty(),"Malformed complete copy unavailable");}
    AtomicProjectFile::write(path(c),std::string(8u*1024u*1024u+1,'x'));r=read(c);
    require(r.status==ReadStatus::Unavailable&&!r.rawAvailable,"Oversized file exposed partial copy");
    AtomicProjectFile::write(path(c),original);require(save(c,{}).empty()&&read(c).status==ReadStatus::Absent&&
        TextDraftRecovery::read(c,"other-key").fields.at("secret")=="untouched","Cleanup affected another recovery key");
}
void process(const std::string &mode,const fs::path &argument){
    const auto root=fs::absolute(argument).lexically_normal();const std::string marker="GM_EDITOR_DRAFT_PROCESS_1\n";
    if(mode=="--produce"){
        require(!fs::exists(root)&&fs::create_directory(root),"Producer requires a fresh directory");AtomicProjectFile::write(root/"owner.txt",marker);isolate(root);
        require(save(campaign(),fixture()).empty(),"Cannot produce checkpoint");
    }else{
        require(mode=="--consume"&&!fs::is_symlink(root)&&bytes(root/"owner.txt")==marker,"Consumer requires owned producer root");isolate(root);
        const auto before=bytes(path(campaign()));const auto pending=read(campaign());require(pending.status==ReadStatus::Ready,"Restart read failed");
        compare(fixture(),pending.drafts);require(bytes(path(campaign()))==before,"Read modified checkpoint");
    }
    const auto receipt=root/(mode=="--produce"?"producer.json":"consumer.json");require(!fs::exists(receipt),"Receipt already exists");
    AtomicProjectFile::write(receipt,"{\"success\":true,\"scope\":\"disk codec restart; no automatic restore or canonical save\"}\n");
}
void indexedProcess(const std::string &mode,const fs::path &argument){
    const auto index=fs::absolute(argument).lexically_normal();
    require(index.filename()=="editor-recovery-process-current.txt","Unexpected process fixture index name");
    fs::create_directories(index.parent_path());
    if(mode=="--produce-index"){
        const auto root=index.parent_path()/EntityMetadata{"er"}.id;
        process("--produce",root);AtomicProjectFile::write(index,root.filename().u8string());
    }else{
        require(mode=="--consume-index","Unknown process fixture mode");const auto name=bytes(index);
        require(!name.empty()&&name.find_first_of("/\\:")==std::string::npos&&name!="."&&name!="..","Invalid process fixture index");
        process("--consume",index.parent_path()/name);
    }
}
}
int main(int argc,char **argv){try{
    if(argc==3){const std::string mode=argv[1];if(mode=="--produce-index"||mode=="--consume-index")indexedProcess(mode,argv[2]);else process(mode,argv[2]);return 0;}
    require(argc==1,"Use --produce or --consume ROOT");
    const auto root=fs::temp_directory_path()/EntityMetadata{"editor-recovery-codec"}.id;isolate(root);codec();storage();
    std::cout<<"Editor draft codec: every authored field, baselines, metadata, power order, malformed copy, bounds and scoped storage passed.\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
