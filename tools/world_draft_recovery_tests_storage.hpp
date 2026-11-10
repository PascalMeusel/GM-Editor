#pragma once
#include "world_draft_recovery_tests_fixture.hpp"

namespace world_recovery_test
{
inline void storageCases()
{
    const auto c=campaign();const auto s=authored(c);const auto expected=encoded(c,s);
    require(read(c).status==ReadStatus::Absent,"Missing World checkpoint was not clean absence");
    auto written=save(c,s);require(written.success,written.error);const auto original=bytes(path(c));
    auto pending=read(c);require(pending.status==ReadStatus::Ready&&pending.rawAvailable&&pending.rawCheckpoint==original&&
        encoded(c,pending.drafts)==expected,"Disk read lost original checkpoint or changed drafts");
    require(c.worldPlaces[0].name=="Original place","Read restored automatically");
    auto copied=copyCheckpoint(pending);require(copied.success&&unbase64(copied.text)==original,"Copy was not byte-exact");
    require(TextDraftRecovery::save(c,"unrelated-session",{{"note","UNRELATED_RECOVERY"}}).empty(),"Cannot prepare independent key");
    auto tooLarge=s;tooLarge.places.at("place").edited.notes.assign(MaxTextBytes+1,'x');
    require(!save(c,tooLarge).success&&bytes(path(c))==original,"Rejected encode replaced last checkpoint");
    const auto other=TextDraftRecovery::read(c,"unrelated-session").fields;
    for(const auto &bad:std::vector<std::string>{"",std::string("bad\0file",8),original.substr(0,original.size()-1)})
    {
        AtomicProjectFile::write(path(c),bad);pending=read(c);copied=copyCheckpoint(pending);
        require(pending.status==ReadStatus::Invalid&&pending.drafts.empty()&&pending.rawAvailable&&copied.success&&
            unbase64(copied.text)==bad&&bytes(path(c))==bad,"Malformed/empty bytes not retained exactly for explicit Copy");
    }
    require(TextDraftRecovery::save(c,Key,{{"world","unsupported World payload"}}).empty(),"Cannot write malformed payload fixture");
    pending=read(c);require(pending.status==ReadStatus::Invalid&&pending.rawAvailable&&pending.drafts.empty(),"Invalid World payload exposed partial Store");
    auto otherCampaign=c;otherCampaign.metadata.id="other-recovery-campaign";
    fs::create_directories(path(otherCampaign).parent_path());AtomicProjectFile::write(path(otherCampaign),original);
    pending=read(otherCampaign);require(pending.status==ReadStatus::Invalid&&copyCheckpoint(pending).success,"Foreign outer envelope restored or became uncopyable");
    AtomicProjectFile::write(path(c),std::string(8U*1024U*1024U+1,'x'));pending=read(c);
    require(pending.status==ReadStatus::Unavailable&&!pending.rawAvailable&&!copyCheckpoint(pending).success,"Oversized read exposed a partial copy");
    AtomicProjectFile::write(path(c),original);
#ifdef _WIN32
    struct Lock {HANDLE file=INVALID_HANDLE_VALUE;~Lock(){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);}} lock;
    lock.file=CreateFileW(path(c).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(lock.file!=INVALID_HANDLE_VALUE,"Cannot hold checkpoint replacement/removal lock");
    auto changed=s;changed.places.at("place").edited.notes="LATEST_UNCHECKPOINTED_TEXT";
    require(!save(c,changed).success&&!remove(c).success&&bytes(path(c))==original&&
        changed.places.at("place").edited.notes=="LATEST_UNCHECKPOINTED_TEXT","Failed storage changed old disk/new RAM");
    CloseHandle(lock.file);lock.file=INVALID_HANDLE_VALUE;
    require(save(c,changed).success&&read(c).drafts.places.at("place").edited.notes=="LATEST_UNCHECKPOINTED_TEXT","Checkpoint retry lost latest text");
#else
    if(geteuid()!=0){const auto parent=path(c).parent_path();const auto permissions=fs::status(parent).permissions();
        fs::permissions(parent,fs::perms::owner_read|fs::perms::owner_exec,fs::perm_options::replace);
        const auto failedWrite=save(c,s),failedRemove=remove(c);fs::permissions(parent,permissions,fs::perm_options::replace);
        require(!failedWrite.success&&!failedRemove.success&&bytes(path(c))==original,"Read-only directory did not preserve checkpoint");}
    else std::cout<<"SKIP POSIX permissions fault while running as root; rejected encode and unsafe path retain byte canaries\n";
#endif
    const auto linked=c.metadata.id+"-linked";auto linkedCampaign=c;linkedCampaign.metadata.id=linked;
    fs::create_directories(path(linkedCampaign).parent_path());std::error_code error;
    fs::create_symlink(path(c),path(linkedCampaign),error);
    if(!error){const auto before=bytes(path(c));const auto linkedRead=read(linkedCampaign);
        require(linkedRead.status==ReadStatus::Unavailable&&!linkedRead.rawAvailable&&!save(linkedCampaign,s).success&&
            !remove(linkedCampaign).success&&bytes(path(c))==before,"Unsafe linked storage was read/written/removed");}
    else std::cout<<"SKIP symlink fixture: "<<error.message()<<'\n';
    require(save(c,{}).success&&read(c).status==ReadStatus::Absent&&TextDraftRecovery::read(c,"unrelated-session").fields==other,
        "Explicit empty checkpoint did not remove only its own key");
}
inline std::string digest(const std::string &value)
{
    std::uint64_t hash=14695981039346656037ull;for(unsigned char c:value){hash^=c;hash*=1099511628211ull;}
    std::ostringstream out;out<<"fnv1a64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;return out.str();
}
inline unsigned long processId()
{
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<unsigned long>(getpid());
#endif
}
inline void receipt(const fs::path &root,const char *mode,const Campaign &c,const std::string &checkpoint)
{
    const auto destination=root/(std::string(mode)+".json");require(!fs::exists(destination),"Receipt already exists; preserve earlier evidence");
    std::ostringstream out;out<<"{\n\"mode\":\""<<mode<<"\",\n\"process_id\":"<<processId()<<
        ",\n\"checkpoint_hash\":\""<<digest(checkpoint)<<"\",\n\"checkpoint_bytes\":"<<checkpoint.size()<<
        ",\n\"expected_payload_hash\":\""<<digest(encoded(c,authored(c)))<<
        "\",\n\"scope\":\"disk codec restart; no automatic Editor restore or database durability claim\"\n}\n";
    AtomicProjectFile::write(destination,out.str());std::cout<<out.str();
}
inline void processMode(const std::string &mode,const fs::path &argument)
{
    const auto root=fs::absolute(argument).lexically_normal();
    constexpr const char *marker="GM_WORLD_RECOVERY_PROCESS_FIXTURE_1\n";
    if(mode=="--produce"){
        require(!fs::exists(root)&&fs::create_directory(root),"Producer requires a new fixture directory with an existing parent");
        AtomicProjectFile::write(root/"fixture-owner.txt",marker);isolate(root);const auto c=campaign();
        const auto result=save(c,authored(c));require(result.success,result.error);
        receipt(root,"producer",c,bytes(path(c)));return;
    }
    require(mode=="--consume"&&fs::is_directory(root)&&!fs::is_symlink(fs::symlink_status(root))&&
        bytes(root/"fixture-owner.txt")==marker,"Consumer requires the producer's owned fixture directory");
    isolate(root);const auto c=campaign();const auto before=bytes(path(c));Store liveDrafts;
    const auto pending=read(c);require(liveDrafts.empty()&&pending.status==ReadStatus::Ready&&c.worldPlaces[0].name=="Original place",
        "Restart read restored automatically or did not load saved bytes");
    require(encoded(c,pending.drafts)==encoded(c,authored(c)),"Restart changed baseline/authored fields");
    compareStores(authored(c),pending.drafts);
    // Explicit test-owned handoff, independent of the read API and real UI.
    liveDrafts=pending.drafts;mergeCases(liveDrafts);
    require(bytes(path(c))==before,"Consumer applied/removes checkpoint without explicit storage action");
    receipt(root,"consumer",c,before);
}
}
