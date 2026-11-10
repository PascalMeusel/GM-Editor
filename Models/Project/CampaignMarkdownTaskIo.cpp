#include "CampaignMarkdownIo.hpp"
#include "CampaignMarkdownDocuments.hpp"
#include "CampaignMarkdownFiles.hpp"

namespace CampaignMarkdownExchange::detail
{
struct OwnedStage {std::filesystem::path path;};
namespace {
void validateSource(const ExportInput &source,const Limits &limits)
{
    require(source.documents.size()<=limits.documents,"Reading-copy inventory exceeds 20,000 documents.");
    std::size_t bytes=0;
    for(const auto &d:source.documents) {
        for(const auto *value:{&d.id,&d.type,&d.title,&d.relativePath,&d.markdown}) {
            require(value->size()<=limits.documentBytes-bytes,"Reading-copy text exceeds the 64 MiB aggregate limit.");bytes+=value->size();
        }
    }
}
std::filesystem::path attachment(const ExportInput &source,const std::string &name)
{
    const auto relative=std::filesystem::u8path(name);require(!name.empty()&&!relative.is_absolute()&&!relative.has_root_name()&&name.find_first_of(":\0",0,2)==std::string::npos,"Invalid managed attachment path.");
    for(const auto &p:relative)require(p!=".."&&p!=".","Attachment path cannot traverse directories.");
    const auto buckets=ProjectPackageLayout::assetFolderNames();require(relative.begin()!=relative.end()&&std::find(buckets.begin(),buckets.end(),relative.begin()->string())!=buckets.end(),"Attachment is outside managed buckets.");
    const auto full=source.assetsRoot/relative;safePath(full);require(allowedAttachment(full)&&std::filesystem::is_regular_file(mapFileIoPath(full)),"Attachment is unavailable or unsupported.");return full;
}
ExportPreview inventory(const ExportInput &source,const Io &io)
{
    validateSource(source,io.limits);
    ExportPreview result;result.campaignId=source.campaignId;result.documents=source.documents;result.editableError=source.editableError;
    std::uintmax_t total=0;std::size_t visited=0;safePath(source.assetsRoot);
    for(const auto &bucket:ProjectPackageLayout::assetFolderNames()) {
        const auto folder=source.assetsRoot/bucket;safePath(folder);if(!std::filesystem::exists(mapFileIoPath(folder)))continue;
        const auto native=mapFileIoPath(folder);
        for(std::filesystem::recursive_directory_iterator it(native),end;it!=end;++it){io.check();require(++visited<=10000,"Managed attachment inventory exceeds 10,000 entries.");
            const auto path=folder/it->path().lexically_relative(native);
            try{safePath(path);}catch(const std::exception &){it.disable_recursion_pending();result.warnings.push_back("Linked or inaccessible asset omitted.");continue;}
            if(!it->is_regular_file())continue;const auto relative=path.lexically_relative(source.assetsRoot).generic_u8string();
            if(!allowedAttachment(path)){result.warnings.push_back("Unsupported attachment omitted: "+relative);continue;}
            const auto size=it->file_size();if(size>MaxAttachmentBytes||size>MaxAttachmentTotal-total){result.warnings.push_back("Attachment exceeds the 64 MiB/file or 256 MiB total limit: "+relative);continue;}
            result.attachments.push_back({relative,io.hash(path,size),size});total+=size;
        }
    }
    std::sort(result.attachments.begin(),result.attachments.end(),[](const auto &a,const auto &b){return a.relativePath<b.relativePath;});result.success=true;return result;
}
ImportPreview import(const EntityId &campaign,const std::filesystem::path &path,const Io &io)
{
    require(lower(path.extension().string())==".md","Select a .md Markdown file.");ImportPreview result;result.campaignId=campaign;result.sourcePath=path;
    result.text=io.read(path,io.limits.noteBytes);result.title=path.stem().u8string();result.sourceFileName=path.filename().u8string();result.contentHash=digest(result.text);result.success=true;return result;
}
void stage(const TaskInput &input,TaskResult &out,Io &io)
{
    require(input.source&&input.inventory&&input.inventory->success&&input.inventory->campaignId==input.source->campaignId,"Review an export inventory first.");
    const auto &source=*input.source;validateSource(source,io.limits);
    if(input.editable&&!source.editableError.empty())throw std::runtime_error("Editable notes are unavailable: "+source.editableError);
    const auto destination=input.path;safePath(destination);
    require(!ProjectPackageExportPaths::inside(destination,source.storageRoot)&&!ProjectPackageExportPaths::inside(source.storageRoot,destination),"Choose an export folder outside campaign storage.");
    require(!std::filesystem::exists(mapFileIoPath(destination))&&std::filesystem::is_directory(mapFileIoPath(destination.parent_path())),"Choose a new folder whose parent exists; existing files are preserved.");
    std::set<std::string> selected;std::uintmax_t total=0;std::vector<std::pair<Attachment,std::filesystem::path>> files;
    for(const auto &name:input.attachments){require(selected.insert(name).second,"Duplicate attachment selection.");
        const auto a=std::find_if(input.inventory->attachments.begin(),input.inventory->attachments.end(),[&](const auto &v){return v.relativePath==name;});
        require(a!=input.inventory->attachments.end()&&a->byteSize<=MaxAttachmentBytes&&a->byteSize<=MaxAttachmentTotal-total,"Attachment is unreviewed or exceeds its budget.");total+=a->byteSize;files.push_back({*a,attachment(source,name)});}
    auto path=destination;path+=".staging_"+AtomicProjectFile::uniqueSuffix();safePath(path);
    auto ownership=std::make_shared<OwnedStage>();ownership->path=path;
    require(std::filesystem::create_directory(mapFileIoPath(path)),"Cannot create exclusive exchange staging.");out.ownedStage=std::move(ownership);out.staging=path;out.destination=destination;
    std::filesystem::create_directory(mapFileIoPath(path/"notes"));io.phase=TaskPhase::Copying;
    std::string index="# GM campaign notes\n\n**Contains private GM information. Do not use as a player recap.**\n\nThis is a prose export, not a campaign backup. IDs remain stable when titles change.\n\n| Stable ID | Type | Note |\n| --- | --- | --- |\n";
    for(const auto &d:source.documents){
        require(d.type.find_first_of("/\\:")==std::string::npos&&d.relativePath=="notes/"+d.type+"-"+safeId(d.id)+".md","Invalid reading-copy path.");
        io.write(path/std::filesystem::u8path(d.relativePath),d.markdown);index+="| "+codeId(d.id,true)+" | "+d.type+" | ["+inlineText(d.title)+"]("+d.relativePath+") |\n";}
    if(input.editable){writeEditable(source,path,io);index+="\nEdit only files in editable/ to update existing story-thread GM notes. Keep baseline/ and EDITABLE_NOTES.v1 unchanged. Review updates in the original campaign.\n";}
    index+="\n## Explicitly selected attachments\n\n";
    if(!files.empty())std::filesystem::create_directory(mapFileIoPath(path/"attachments"));
    std::size_t number=0;
    for(const auto &[asset,file]:files){const auto name="attachments/"+std::to_string(++number)+lower(file.extension().string());io.copy(file,path/name,asset);index+="- ["+inlineText(asset.relativePath)+"]("+name+")\n";}
    if(files.empty())index+="No attachments selected.\n";
    io.write(path/"INDEX.md",index);
    // Verify every authored file, including exact baseline/edit bytes, before
    // returning an owned stage. No publication or live state access here.
    require(io.read(path/"INDEX.md",io.limits.documentBytes,false)==index,"Staged index changed.");
    for(const auto &d:source.documents)require(io.read(path/std::filesystem::u8path(d.relativePath),io.limits.documentBytes,false)==d.markdown,"Staged reading copy changed.");
    if(input.editable) {const auto checked=readUpdates(source.campaignId,path,io);require(checked.entries.size()==source.editableNotes.size(),"Staged editable inventory changed.");}
}
}
void runTask(const TaskInput &input,TaskResult &out,Io &io)
{
    out.kind=input.kind;
    switch(input.kind) {
    case TaskKind::ExportInventory:require(bool(input.source),"No export input.");io.phase=TaskPhase::Inventory;out.inventory=inventory(*input.source,io);break;
    case TaskKind::ImportPreview:out.imported=import(input.campaignId,input.path,io);break;
    case TaskKind::UpdatePreview:out.updates=readUpdates(input.campaignId,input.path,io);break;
    case TaskKind::StageExport:stage(input,out,io);break;
    case TaskKind::CheckImport:{require(bool(input.imported),"No reviewed import.");const auto fresh=import(input.imported->campaignId,input.imported->sourcePath,io);
        require(fresh.text==input.imported->text&&fresh.contentHash==input.imported->contentHash&&fresh.title==input.imported->title&&fresh.sourceFileName==input.imported->sourceFileName,"Markdown changed after review. Refresh first.");out.imported=*input.imported;break;}
    case TaskKind::CheckUpdates:require(bool(input.updates),"No reviewed updates.");checkUpdates(*input.updates,io);out.updates=*input.updates;break;
    }
    io.check();out.success=true;
}
void cleanupStage(TaskResult &result) noexcept
{
    if(!result.ownedStage)return;
    try {const auto &path=result.ownedStage->path;safePath(path);
        if(std::filesystem::exists(mapFileIoPath(path))) {
            for(const auto &entry:std::filesystem::recursive_directory_iterator(mapFileIoPath(path)))safePath(path/entry.path().lexically_relative(mapFileIoPath(path)));
            std::filesystem::remove_all(mapFileIoPath(path));
        }
        result.ownedStage.reset();result.staging.clear();
    }catch(...) {try{result.error+=" Incomplete staging was retained at "+result.staging.u8string();}catch(...) {}}
}
}
