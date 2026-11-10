#include "CampaignMarkdownIo.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace CampaignMarkdownExchange
{
struct Task::State {
    struct Job {std::uint64_t serial;TaskInput input;std::unique_ptr<TaskResult> result;};
    explicit State(TaskOptions o):options(std::move(o)){}
    TaskOptions options;
    std::mutex mutex;std::condition_variable wake;
    std::atomic<std::uint64_t> serial{0};std::atomic<bool> stop{false};
    std::unique_ptr<Job> pending;
    std::unique_ptr<TaskResult> ready,retired;
    Progress progress;
    void cleanup(std::unique_ptr<TaskResult> results) {
        while(results){auto next=std::move(results->retiredNext);detail::cleanupStage(*results);results=std::move(next);}
    }
    void run() noexcept {
        for(;;) {
            std::unique_ptr<Job> job;std::unique_ptr<TaskResult> garbage;
            {
                std::unique_lock<std::mutex> lock(mutex);wake.wait(lock,[&]{return stop||pending||retired||(ready&&ready->serial!=serial);});
                garbage=std::move(retired);
                if(ready&&(stop||ready->serial!=serial)){ready->retiredNext=std::move(garbage);garbage=std::move(ready);}
                if(!stop){job=std::move(pending);if(job){progress={};progress.running=true;}}
            }
            cleanup(std::move(garbage));if(stop)return;if(!job)continue;
            auto result=std::move(job->result);result->serial=job->serial;
            try {
                detail::Io io;io.limits=options.limits;
                io.cancelled=[this,id=job->serial]{return stop||serial!=id;};
                TaskPhase last=TaskPhase::Queued;
                io.progress=[this,id=job->serial,&last](TaskPhase phase,std::uint64_t bytes,std::uint64_t files){
                    {std::lock_guard<std::mutex> lock(mutex);if(serial==id){progress.phase=phase;progress.bytes+=bytes;progress.files+=files;}}
                    if(phase!=last){last=phase;if(options.phaseHook)options.phaseHook(id,phase);}
                };
                if(options.phaseHook)options.phaseHook(job->serial,TaskPhase::Queued);
                detail::runTask(job->input,*result,io);
                if(options.phaseHook)options.phaseHook(job->serial,TaskPhase::Ready);
                io.check();
            }catch(const detail::Cancelled &){result->success=false;result->cancelled=true;}
            catch(const std::exception &e){result->success=false;try{result->error=e.what();}catch(...) {}}
            catch(...){result->success=false;try{result->error="Note exchange failed.";}catch(...) {}}
            const bool stale=stop||serial!=job->serial;
            if(stale||!result->success)detail::cleanupStage(*result);
            {std::lock_guard<std::mutex> lock(mutex);progress.running=false;if(!stop&&serial==job->serial)ready=std::move(result);}
            if(result)detail::cleanupStage(*result); // cancellation may race the final lock
        }
    }
};
Task::Task(TaskOptions options):state_(std::make_shared<State>(std::move(options)))
{std::thread([state=state_]{state->run();}).detach();}
Task::~Task(){if(state_){state_->stop=true;{std::lock_guard<std::mutex> lock(state_->mutex);++state_->serial;state_->pending.reset();}state_->wake.notify_one();}}
std::uint64_t Task::request(TaskInput input)
{
    auto job=std::make_unique<State::Job>();job->input=std::move(input);job->result=std::make_unique<TaskResult>();
    std::uint64_t id;{std::lock_guard<std::mutex> lock(state_->mutex);id=++state_->serial;job->serial=id;state_->pending=std::move(job);state_->progress={};}
    state_->wake.notify_one();return id;
}
void Task::cancel(){std::lock_guard<std::mutex> lock(state_->mutex);++state_->serial;state_->pending.reset();state_->wake.notify_one();}
std::unique_ptr<TaskResult> Task::takeCompletion(){std::lock_guard<std::mutex> lock(state_->mutex);if(state_->ready&&state_->ready->serial==state_->serial)return std::move(state_->ready);return {};}
Progress Task::progress() const {std::lock_guard<std::mutex> lock(state_->mutex);return state_->progress;}
void Task::retire(std::unique_ptr<TaskResult> result){if(!result)return;std::lock_guard<std::mutex> lock(state_->mutex);result->retiredNext=std::move(state_->retired);state_->retired=std::move(result);state_->wake.notify_one();}
}
