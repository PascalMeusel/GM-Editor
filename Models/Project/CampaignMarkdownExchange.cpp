#include "CampaignMarkdownIo.hpp"
#include "CampaignMarkdownDocuments.hpp"
#include "CampaignMarkdownFiles.hpp"
#include "ProjectDirectoryPublishExclusive.hpp"

namespace CampaignMarkdownExchange
{
ExportPreview previewExport(const Campaign &c)
{
    TaskResult output;
    try {TaskInput input;input.kind=TaskKind::ExportInventory;input.source=std::make_shared<const ExportInput>(captureExport(c));
        detail::Io io;detail::runTask(input,output,io);return std::move(output.inventory);}
    catch(const std::exception &e){ExportPreview result;result.campaignId=c.metadata.id;result.error=e.what();return result;}
}
Result exportNotes(const Campaign &c,const ExportPreview &preview,const std::filesystem::path &destination,const std::vector<std::string> &selected)
{
    Result result;TaskResult output;
    try {
        const auto source=std::make_shared<const ExportInput>(captureExport(c));
        detail::require(preview.success&&preview.campaignId==c.metadata.id&&detail::sameDocuments(preview.documents,source->documents),"Campaign notes changed after preview; review again.");
        detail::require(!destination.empty(),"Choose a new output directory.");
        TaskInput input;input.kind=TaskKind::StageExport;input.source=source;input.inventory=std::make_shared<const ExportPreview>(preview);
        input.path=std::filesystem::absolute(destination).lexically_normal();input.attachments=selected;input.editable=false;
        detail::Io io;detail::runTask(input,output,io);
        ProjectDirectoryPublish::renameExclusive(output.staging,output.destination);
        result.success=result.changed=true;result.destination=output.destination;output.ownedStage.reset();output.staging.clear();
    }catch(const std::exception &e){result.error=e.what();detail::cleanupStage(output);if(!output.staging.empty())result.error+=" Incomplete staging retained at "+output.staging.u8string();}
    return result;
}
ImportPreview previewImport(const Campaign &c,const std::filesystem::path &path)
{
    try {TaskInput input;input.kind=TaskKind::ImportPreview;input.campaignId=c.metadata.id;input.path=std::filesystem::absolute(path).lexically_normal();
        TaskResult result;detail::Io io;detail::runTask(input,result,io);return std::move(result.imported);}
    catch(const std::exception &e){ImportPreview result;result.error=e.what();return result;}
}
Result importNote(Campaign &c,const ImportPreview &preview,const SessionPlan::Persist &persist)
{
    Result result;
    try {
        const auto id=c.metadata.id;const auto provider=c.ruleSet;
        const auto root=std::filesystem::absolute(ProjectPackageLayout::findProjectRoot()).lexically_normal();
        const auto current=[&]{return c.metadata.id==id&&c.ruleSet==provider&&std::filesystem::absolute(ProjectPackageLayout::findProjectRoot()).lexically_normal()==root;};
        detail::require(bool(persist),"A persistence callback is required.");
        TaskInput input;input.kind=TaskKind::CheckImport;input.imported=std::make_shared<const ImportPreview>(preview);
        TaskResult checked;detail::Io io;detail::runTask(input,checked,io);
        const auto applied=applyImportedNote(c,checked.imported,[&](Campaign &staged){
            return ProjectSaveResult{persist(staged)?ProjectSaveOutcome::Committed:ProjectSaveOutcome::NotCommitted,{},{}};
        },current);
        result.success=applied.state==ApplyState::Applied||applied.state==ApplyState::NoChange;
        result.changed=applied.committed();result.error=applied.error;result.noteId=preview.noteId;
    }catch(const std::exception &e){result.error=e.what();}
    return result;
}
}
