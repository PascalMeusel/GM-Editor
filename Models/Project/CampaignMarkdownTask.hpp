#pragma once
#include "CampaignMarkdownRoundTrip.hpp"
#include <cstdint>

namespace CampaignMarkdownExchange
{
namespace detail {struct OwnedStage;}
enum class TaskKind { ExportInventory,ImportPreview,UpdatePreview,StageExport,CheckImport,CheckUpdates };
enum class TaskPhase { Queued,Reading,Inventory,Copying,Ready };
struct Progress { TaskPhase phase=TaskPhase::Queued;std::uint64_t bytes=0,files=0;bool running=false; };
struct TaskOptions {
    Limits limits;
    // Owned test barrier only; production leaves empty. No UI/service captures.
    std::function<void(std::uint64_t,TaskPhase)> phaseHook;
};
struct TaskInput {
    TaskKind kind=TaskKind::ExportInventory;
    std::shared_ptr<const ExportInput> source;
    std::shared_ptr<const ExportPreview> inventory;
    std::shared_ptr<const ImportPreview> imported;
    std::shared_ptr<const UpdatePreview> updates;
    EntityId campaignId;
    std::filesystem::path path;
    std::vector<std::string> attachments;
    bool editable=true;
};
struct TaskResult {
    std::uint64_t serial=0;
    TaskKind kind=TaskKind::ExportInventory;
    bool success=false,cancelled=false;
    std::string error;
    ExportPreview inventory;
    ImportPreview imported;
    UpdatePreview updates;
    std::filesystem::path staging,destination;
    std::shared_ptr<detail::OwnedStage> ownedStage;
    std::unique_ptr<TaskResult> retiredNext;
};
// One persistent I/O worker. Its shared state owns all inputs and completion.
// Cancellation and destruction signal stop; neither waits for a blocked read.
class Task {
public:
    explicit Task(TaskOptions = {});
    ~Task();
    Task(const Task &)=delete;Task &operator=(const Task &)=delete;
    std::uint64_t request(TaskInput);
    void cancel();
    std::unique_ptr<TaskResult> takeCompletion();
    Progress progress() const;
    void retire(std::unique_ptr<TaskResult>); // stage cleanup on the worker
private:
    struct State;
    std::shared_ptr<State> state_;
};
}
