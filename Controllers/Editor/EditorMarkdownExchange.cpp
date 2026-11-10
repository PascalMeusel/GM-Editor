#include "EditorMarkdownExchange.hpp"
#include "EditorWindowControllerSupport.hpp"
#include "Models/Project/CampaignMarkdownIo.hpp"
#include "Models/Project/CampaignMarkdownFiles.hpp"
#include "Models/Project/ProjectDirectoryPublishExclusive.hpp"

using namespace CampaignMarkdownExchange;
EditorMarkdownExchange::EditorMarkdownExchange(EditorWindowController &owner,TaskOptions options):owner_(&owner),options_(std::move(options)){}
EditorMarkdownExchange::~EditorMarkdownExchange(){detach();}
EditorMarkdownExchange::Context EditorMarkdownExchange::context() const
{
    if(!owner_)throw std::runtime_error("The editor is no longer available.");
    return {&owner_->_currentCampaign,owner_->_currentCampaign.metadata.id,
        std::filesystem::absolute(ProjectPackageLayout::findProjectRoot()).lexically_normal(),
        gm::localization::LocalizationService::instance().locale()==gm::localization::Locale::German?"de":"en",owner_->view->_playerReviewContextEpoch};
}
bool EditorMarkdownExchange::privateAvailable() const
{
    if(!owner_||!owner_->view->isVisible||owner_->_currentCampaign.presentationModeActive)return false;
    // Context can change between frames before post-draw polling. Gate cached
    // private content as well as completion adoption during that frame.
    return !(review_||busy())||current(context_);
}
bool EditorMarkdownExchange::current(const Context &c) const
{
    if(!owner_||!owner_->view->isVisible||owner_->_currentCampaign.presentationModeActive)return false;
    try {const auto now=context();return now.campaign==c.campaign&&now.id==c.id&&now.root==c.root&&now.locale==c.locale&&now.epoch==c.epoch;}catch(...){return false;}
}
void EditorMarkdownExchange::cancel()
{
    ++token_;if(task_)task_->cancel();
    selected_.clear();status_=Status::Cancelled;message_.clear();
}
void EditorMarkdownExchange::reset(){cancel();review_.reset();export_.reset();context_={};status_=Status::Idle;uiActive_=false;}
void EditorMarkdownExchange::detach() noexcept
{try{reset();}catch(...){}owner_=nullptr;task_.reset();}
Progress EditorMarkdownExchange::progress() const{return task_?task_->progress():Progress{};}
void EditorMarkdownExchange::begin(TaskInput input,Status status)
{
    if(!privateAvailable()){reset();return;}
    context_=context();
    if(status==Status::Preparing)++token_;
    if(!task_)task_=std::make_unique<Task>(options_);
    serial_=task_->request(std::move(input));status_=status;message_.clear();
}
bool EditorMarkdownExchange::accept(std::uint64_t token)
{
    if(!ready()||!review_||review_->token!=token||!current(context_)){
        cancel();message_=uiText("This preview is no longer current. Prepare it again.","Diese Vorschau ist nicht mehr aktuell. Bereite sie erneut vor.");return false;}
    return true;
}
void EditorMarkdownExchange::requestExportInventory()
{
    try {if(!privateAvailable()){reset();return;}auto source=std::make_shared<const ExportInput>(captureExport(owner_->_currentCampaign,options_.limits));
        TaskInput input;input.kind=TaskKind::ExportInventory;input.source=source;begin(std::move(input),Status::Preparing);export_=std::move(source);
    }catch(const std::exception &e){cancel();status_=Status::Failed;message_=e.what();}
}
void EditorMarkdownExchange::requestSingleImportPreview(const std::filesystem::path &path)
{
    try {detail::require(!path.empty(),"Choose a Markdown file.");TaskInput input;input.kind=TaskKind::ImportPreview;input.campaignId=context().id;input.path=std::filesystem::absolute(path).lexically_normal();begin(std::move(input),Status::Preparing);}
    catch(const std::exception &e){cancel();status_=Status::Failed;message_=e.what();}
}
void EditorMarkdownExchange::requestUpdatePreview(const std::filesystem::path &path)
{
    try {detail::require(!path.empty(),"Choose an editable export folder.");TaskInput input;input.kind=TaskKind::UpdatePreview;input.campaignId=context().id;input.path=std::filesystem::absolute(path).lexically_normal();begin(std::move(input),Status::Preparing);}
    catch(const std::exception &e){cancel();status_=Status::Failed;message_=e.what();}
}
void EditorMarkdownExchange::requestExport(std::uint64_t token,const std::vector<std::string> &attachments,const std::filesystem::path &path,bool editable)
{
    if(!accept(token))return;
    try {detail::require(review_->kind==Kind::Export&&export_&&!path.empty(),"Review an export inventory and choose a destination first.");
        detail::require(sameExport(*export_,captureExport(owner_->_currentCampaign,options_.limits)),"Campaign notes changed. Prepare a new export inventory.");
        TaskInput input;input.kind=TaskKind::StageExport;input.source=export_;input.inventory=std::shared_ptr<const ExportPreview>(review_,&review_->inventory);
        input.path=std::filesystem::absolute(path).lexically_normal();input.attachments=attachments;input.editable=editable;begin(std::move(input),Status::Exporting);
    }catch(const std::exception &e){cancel();status_=Status::Failed;message_=e.what();}
}
void EditorMarkdownExchange::requestImport(std::uint64_t token)
{
    if(!accept(token))return;
    try {detail::require(review_->kind==Kind::Import,"Review a Markdown file first.");TaskInput input;input.kind=TaskKind::CheckImport;
        input.imported=std::shared_ptr<const ImportPreview>(review_,&review_->imported);begin(std::move(input),Status::CheckingApply);
    }catch(const std::exception &e){cancel();status_=Status::Failed;message_=e.what();}
}
void EditorMarkdownExchange::requestApplyUpdates(std::uint64_t token,const std::vector<EntityId> &selected)
{
    if(!accept(token))return;
    try {detail::require(review_->kind==Kind::Updates&&!selected.empty(),"Select reviewed note changes first.");selected_=selected;
        TaskInput input;input.kind=TaskKind::CheckUpdates;input.updates=std::shared_ptr<const UpdatePreview>(review_,&review_->updates);begin(std::move(input),Status::CheckingApply);
    }catch(const std::exception &e){cancel();status_=Status::Failed;message_=e.what();}
}
void EditorMarkdownExchange::poll()
{
    const bool active=uiActive_;uiActive_=false;
    if(!privateAvailable()||((review_||busy())&&!current(context_))){reset();if(owner_)owner_->view->_exchange.reset();return;}
    // Poll follows the actual view draw. Only this frame's open private popup
    // may adopt a result or perform a previously requested durable operation.
    if((ready()||busy())&&!active){cancel();return;}
    if(!task_)return;auto result=task_->takeCompletion();if(!result)return;
    if(result->serial!=serial_||!current(context_)){task_->retire(std::move(result));return;}
    const auto held=shared_from_this();complete(std::move(result));
}
void EditorMarkdownExchange::complete(std::unique_ptr<TaskResult> result)
{
    if(!result->success){message_=result->cancelled?uiText("Cancelled. No changes were applied.","Abgebrochen. Es wurden keine Änderungen übernommen."):result->error;
        status_=Status::Failed;task_->retire(std::move(result));return;}
    if(result->kind==TaskKind::CheckImport||result->kind==TaskKind::CheckUpdates){commit(*result);return;}
    if(result->kind==TaskKind::StageExport){
        try {detail::require(current(context_)&&export_&&sameExport(*export_,captureExport(owner_->_currentCampaign,options_.limits)),"Campaign notes or context changed while preparing export. Nothing was published.");
            detail::safePath(result->staging);detail::safePath(result->destination);
            ProjectDirectoryPublish::renameExclusive(result->staging,result->destination);result->ownedStage.reset();result->staging.clear();
            ++token_;status_=Status::Idle;message_=uiText("Exported to ","Exportiert nach ")+result->destination.u8string();
        }catch(const std::exception &e){message_=e.what();status_=current(context_)?Status::Ready:Status::Failed;task_->retire(std::move(result));}
        return;
    }
    try {auto next=std::make_shared<Snapshot>();next->token=token_;
        if(result->kind==TaskKind::ExportInventory){next->kind=Kind::Export;next->inventory=std::move(result->inventory);}
        else if(result->kind==TaskKind::ImportPreview){next->kind=Kind::Import;next->imported=std::move(result->imported);}
        else {next->kind=Kind::Updates;next->updates=std::move(result->updates);next->rows=classifyUpdates(owner_->_currentCampaign,next->updates);}
        review_=std::move(next);status_=Status::Ready;message_.clear();
    }catch(const std::exception &e){status_=Status::Failed;message_=e.what();}
}
