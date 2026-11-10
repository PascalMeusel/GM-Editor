#pragma once
#include "Models/Project/CampaignMarkdownTask.hpp"

class EditorWindowController;
class EditorMarkdownExchange : public std::enable_shared_from_this<EditorMarkdownExchange>
{
public:
    enum class Status { Idle,Preparing,Ready,Exporting,CheckingApply,Failed,Cancelled };
    enum class Kind { Export,Import,Updates };
    struct Snapshot {
        std::uint64_t token=0;
        Kind kind=Kind::Export;
        CampaignMarkdownExchange::ExportPreview inventory;
        CampaignMarkdownExchange::ImportPreview imported;
        CampaignMarkdownExchange::UpdatePreview updates;
        std::vector<CampaignMarkdownExchange::UpdateRow> rows;
    };
    explicit EditorMarkdownExchange(EditorWindowController &,CampaignMarkdownExchange::TaskOptions = {});
    ~EditorMarkdownExchange();
    void requestExportInventory();
    void requestSingleImportPreview(const std::filesystem::path &);
    void requestUpdatePreview(const std::filesystem::path &);
    void requestExport(std::uint64_t,const std::vector<std::string> &,const std::filesystem::path &,bool editable=true);
    void requestImport(std::uint64_t);
    void requestApplyUpdates(std::uint64_t,const std::vector<EntityId> &);
    void cancel();
    void reset();
    void uiActive() {uiActive_=true;}
    void poll();
    void detach() noexcept;
    bool privateAvailable() const;
    Status status() const {return status_;}
    bool busy() const {return status_==Status::Preparing||status_==Status::Exporting||status_==Status::CheckingApply;}
    bool ready() const {return status_==Status::Ready&&review_&&review_->token==token_;}
    const std::string &message() const {return message_;}
    std::shared_ptr<const Snapshot> snapshot() const {return review_;}
    CampaignMarkdownExchange::Progress progress() const;
private:
    friend struct MarkdownExchangeTestProbe;
    struct Context {const Campaign *campaign=nullptr;EntityId id;std::filesystem::path root;std::string locale;std::uint64_t epoch=0;};
    Context context() const;
    bool current(const Context &) const;
    void begin(CampaignMarkdownExchange::TaskInput,Status);
    bool accept(std::uint64_t);
    void complete(std::unique_ptr<CampaignMarkdownExchange::TaskResult>);
    void commit(const CampaignMarkdownExchange::TaskResult &);
    EditorWindowController *owner_=nullptr;
    CampaignMarkdownExchange::TaskOptions options_;
    std::unique_ptr<CampaignMarkdownExchange::Task> task_;
    Context context_;
    Status status_=Status::Idle;
    std::uint64_t token_=0,serial_=0;
    bool uiActive_=false;
    std::string message_;
    std::shared_ptr<const Snapshot> review_;
    std::shared_ptr<const CampaignMarkdownExchange::ExportInput> export_;
    std::vector<EntityId> selected_;
};
