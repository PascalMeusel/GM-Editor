#pragma once
#include "Models/Project/CampaignMarkdownTask.hpp"
#include "Models/Project/ProjectDirectoryPublishExclusive.hpp"
#include <chrono>
#include <future>
#include <thread>
#include <stdexcept>

namespace markdown_test
{
using namespace CampaignMarkdownExchange;
inline void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
template<class F> void until(F condition,const char *message)
{
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!condition()) {
        if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error(message);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
inline std::unique_ptr<TaskResult> completion(Task &task)
{
    std::unique_ptr<TaskResult> result;
    until([&]{result=task.takeCompletion();return bool(result);},"Markdown worker did not complete");
    return result;
}
inline TaskResult run(TaskInput input,TaskOptions options={})
{
    Task task(std::move(options));const auto serial=task.request(std::move(input));auto result=completion(task);
    require(result->serial==serial,"Markdown worker returned another request's completion");
    return std::move(*result);
}
inline void exported(const Campaign &campaign,const std::filesystem::path &destination,TaskOptions options={})
{
    TaskInput input;input.source=std::make_shared<const ExportInput>(captureExport(campaign,options.limits));
    auto inventory=run(input,options);if(!inventory.success)throw std::runtime_error("Markdown inventory failed: "+inventory.error);
    input.kind=TaskKind::StageExport;input.inventory=std::make_shared<const ExportPreview>(inventory.inventory);input.path=destination;
    auto stage=run(input,options);if(!stage.success||stage.staging.empty())throw std::runtime_error("Markdown staging failed: "+stage.error);
    require(stage.destination==destination&&!std::filesystem::exists(destination),"Worker published without explicit main action");
    ProjectDirectoryPublish::renameExclusive(stage.staging,destination);
}
inline UpdatePreview preview(const Campaign &campaign,const std::filesystem::path &source,TaskOptions options={})
{
    TaskInput input;input.kind=TaskKind::UpdatePreview;input.path=source;input.campaignId=campaign.metadata.id;
    auto result=run(input,std::move(options));if(!result.success||!result.updates.success)throw std::runtime_error("Round-trip preview failed: "+result.error);
    return std::move(result.updates);
}
struct Gate
{
    std::promise<void> enteredPromise,releasePromise;
    std::shared_future<void> entered=enteredPromise.get_future().share(),released=releasePromise.get_future().share();
    std::atomic_bool signalled{false},opened{false},exited{false},timedOut{false};
    void wait()
    {
        if(!signalled.exchange(true))enteredPromise.set_value();
        if(released.wait_for(std::chrono::seconds(5))!=std::future_status::ready)timedOut=true;
        exited=true;
    }
    void open(){if(!opened.exchange(true))releasePromise.set_value();}
    ~Gate(){open();}
};
struct Release {std::shared_ptr<Gate> gate;~Release(){if(gate)gate->open();}};
inline TaskOptions gated(const std::shared_ptr<Gate> &gate,TaskPhase phase=TaskPhase::Reading)
{
    TaskOptions options;options.phaseHook=[gate,phase](std::uint64_t,TaskPhase current){
        if(current==phase&&!gate->signalled.load())gate->wait();};return options;
}
inline void entered(const std::shared_ptr<Gate> &gate)
{
    require(gate->entered.wait_for(std::chrono::seconds(2))==std::future_status::ready,
        "Markdown worker did not reach its actual I/O barrier");
}
}
