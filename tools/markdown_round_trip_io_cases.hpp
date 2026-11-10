std::string markdownHex(const std::string &value)
{std::string out;for(unsigned char ch:value){out+="0123456789abcdef"[ch>>4];out+="0123456789abcdef"[ch&15];}return out;}
std::filesystem::path markdownEdit(const std::filesystem::path &root,const EntityId &id)
{return root/"editable"/(markdownHex(id)+".md");}
// Independent length-frame reader/writer for corruption fixtures, not the app's parser.
std::vector<std::string> markdownManifestFields(const std::string &text)
{
    const std::string header="TTRPG_MARKDOWN_EDITABLE_V1\n";check(text.compare(0,header.size(),header)==0,"Export used an unexpected manifest version");
    std::vector<std::string> fields;std::size_t at=header.size();
    while(at<text.size()) {
        const auto line=text.find('\n',at);check(line!=std::string::npos,"Export manifest field is unframed");
        const auto size=std::stoull(text.substr(at,line-at));at=line+1;
        check(size<=text.size()-at,"Export manifest field is truncated");fields.push_back(text.substr(at,size));at+=size;
    }return fields;
}
std::string markdownManifest(const std::vector<std::string> &fields)
{std::string out="TTRPG_MARKDOWN_EDITABLE_V1\n";for(const auto &f:fields)out+=std::to_string(f.size())+"\n"+f;return out;}
TaskResult markdownReadResult(const Campaign &c,const std::filesystem::path &path,TaskOptions options={})
{TaskInput in;in.kind=TaskKind::UpdatePreview;in.campaignId=c.metadata.id;in.path=path;return markdown_test::run(in,std::move(options));}
void markdownRoundTripIo(const std::filesystem::path &root)
{
    auto c=fixture();const auto id=c.storyThreads[0].metadata.id;
    const std::string raw="\xef\xbb\xbf# Ruß 灯\r\n---\r\nprivate: true\r\n---\r\n![literal](missing.png)\r\n";
    c.storyThreads[0].gmNotes=raw;const auto destination=root/"roundtrip";markdown_test::exported(c,destination);
    check(bytes(markdownEdit(destination,id))==raw&&bytes(destination/"baseline"/(markdownHex(id)+".utf8"))==raw,
        "Raw editable/base companions lost BOM, Unicode, CRLF or literal Markdown");
    auto p=markdown_test::preview(c,destination);
    check(p.entries.size()==1&&p.entries[0].id==id&&p.entries[0].baseText==raw&&p.entries[0].editedText==raw,
        "Actual manifest reader changed raw note identity/text");
    const auto manifestPath=destination/"EDITABLE_NOTES.v1";const auto original=bytes(manifestPath);
    const auto fields=markdownManifestFields(original);
    check(fields.size()==9&&fields[0]==c.metadata.id&&fields[1]=="1"&&fields[2]==id&&fields[3]=="story-thread-gm-notes",
        "Manifest stable identity/type/count contract changed");
    write(markdownEdit(destination,id),raw+"EXTERNAL_EDIT");p=markdown_test::preview(c,destination);
    check(p.entries[0].editedText==raw+"EXTERNAL_EDIT"&&classifyUpdates(c,p)[0].state==UpdateState::Eligible,
        "Actual companion edit did not produce an eligible GM note update");
    TaskInput validate;validate.kind=TaskKind::CheckUpdates;validate.updates=std::make_shared<const UpdatePreview>(p);
    check(markdown_test::run(validate).success,"Unchanged reviewed file set failed revalidation");
    const auto time=std::filesystem::last_write_time(markdownEdit(destination,id));
    write(markdownEdit(destination,id),raw+"REPLACED_EDIT");std::filesystem::last_write_time(markdownEdit(destination,id),time);
    check(!markdown_test::run(validate).success,"Same-size/time file replacement escaped exact reviewed-byte check");
    write(markdownEdit(destination,id),raw+"EXTERNAL_EDIT");
    for(int mode=0;mode<10;++mode) {
        auto changed=fields;
        if(mode==0)changed[0]=createEntityId("other-campaign");
        if(mode==1)changed[1]="10001";
        if(mode==2)changed[3]="npc";
        if(mode==3)changed[5]="../outside.md";
        if(mode==4)changed[6]="editable/"+markdownHex(id)+".md";
        if(mode==5)changed[7]="99999999999999999999999999999";
        if(mode==6)changed[8]="fnv1a64:0000000000000000";
        if(mode==7){changed[1]="2";changed.insert(changed.end(),fields.begin()+2,fields.end());}
        auto malformed=markdownManifest(changed);
        if(mode==8)malformed+="trailing";
        if(mode==9)malformed.replace(malformed.find("_V1\n"),4,"_V9\n");
        write(manifestPath,malformed);check(!markdownReadResult(c,destination).success,"Malformed/foreign/ambiguous manifest was accepted");
    }
    write(manifestPath,original);
    TaskOptions tiny;tiny.limits.noteBytes=raw.size()-1;
    check(!markdownReadResult(c,destination,tiny).success,"Per-note reader bound was ignored");
    tiny={};tiny.limits.rawBytes=raw.size()*2;
    check(!markdownReadResult(c,destination,tiny).success,"Reader aggregate did not count both baseline and edited bytes");
    tiny={};tiny.limits.manifestBytes=original.size()-1;
    check(!markdownReadResult(c,destination,tiny).success,"Manifest byte bound was ignored");
    tiny={};tiny.limits.notes=0;check(!markdownReadResult(c,destination,tiny).success,"Reader count bound was ignored");
    for(int mode=0;mode<3;++mode) {
        Limits limits;if(mode==0)limits.noteBytes=raw.size()-1;if(mode==1)limits.rawBytes=raw.size()*2-1;
        if(mode==2)limits.notes=0;
        const auto limited=captureExport(c,limits);
        check(!limited.editableError.empty()&&limited.editableNotes.empty()&&!limited.documents.empty(),
            "Editable bound discarded reading copy or retained oversized editable payload");
    }
    Limits tooSmall;tooSmall.documentBytes=1;bool refused=false;
    try{(void)captureExport(c,tooSmall);}catch(const std::exception &){refused=true;}
    check(refused,"Formatted document capture accepted exceeded byte bound");
    TaskOptions readingOptions;readingOptions.limits.noteBytes=4;TaskInput reading;
    reading.source=std::make_shared<const ExportInput>(captureExport(c,readingOptions.limits));
    auto readingInventory=markdown_test::run(reading,readingOptions);check(readingInventory.success,"Reading-only inventory rejected editable cap");
    reading.kind=TaskKind::StageExport;reading.inventory=std::make_shared<const ExportPreview>(readingInventory.inventory);
    reading.path=root/"reading-only";reading.editable=true;
    check(!markdown_test::run(reading,readingOptions).success&&!std::filesystem::exists(reading.path),"Editable export bypassed unavailable raw payload");
    reading.editable=false;auto readingStage=markdown_test::run(reading,readingOptions);
    check(readingStage.success,"Reading-copy export inherited editable-only note limit");
    ProjectDirectoryPublish::renameExclusive(readingStage.staging,reading.path);
    check(!std::filesystem::exists(reading.path/"EDITABLE_NOTES.v1")&&
        bytes(reading.path/reading.source->documents[1].relativePath).find(raw)!=std::string::npos,
        "Reading-only publication lost prose or silently created editable manifest");
    check(bytes(destination/"baseline"/(markdownHex(id)+".utf8"))==raw,"Update previews modified baseline bytes");
    c.storyThreads[0].gmNotes.clear();const auto empty=root/"empty-roundtrip";markdown_test::exported(c,empty);
    const auto emptyPreview=markdown_test::preview(c,empty);
    check(emptyPreview.entries.size()==1&&emptyPreview.entries[0].baseText.empty()&&emptyPreview.entries[0].editedText.empty()&&
        classifyUpdates(c,emptyPreview)[0].state==UpdateState::Unchanged,"Zero-byte raw note did not round-trip unchanged");
    write(markdownEdit(empty,id),"Authored from empty baseline");const auto authored=markdown_test::preview(c,empty);
    check(authored.entries[0].baseText.empty()&&authored.entries[0].editedText=="Authored from empty baseline"&&
        classifyUpdates(c,authored)[0].state==UpdateState::Eligible,"Edit from zero-byte baseline was not eligible");
}
