#pragma once
#include "Models/Session/PlayerReviewSession.hpp"
#include "Models/Session/PlayerReviewCompileWorker.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

// Configuration only before the first production worker exists. No completion,
// readiness, approval, result, or generation is fabricated by this probe.
struct PlayerReviewSessionTestProbe
{
    static void configure(PlayerReviewSession &owner,PlayerReviewCompileOptions options)
    {
        if(owner.preparation_)throw std::runtime_error("Cannot reconfigure an existing review worker");
        owner.compileOptions_=std::move(options);
    }
};
namespace player_review_async_test
{
using Clock=std::chrono::steady_clock;
struct DataRoot
{
    bool existed=std::getenv("TTRPG_DATA_ROOT")!=nullptr;
    std::string prior=existed?std::getenv("TTRPG_DATA_ROOT"):"";
    explicit DataRoot(const std::string &path){set(path.c_str());}
    static void set(const char *path)
    {
#ifdef _WIN32
        if(_putenv_s("TTRPG_DATA_ROOT",path)!=0)throw std::runtime_error("Cannot set synthetic review root");
#else
        if(setenv("TTRPG_DATA_ROOT",path,1)!=0)throw std::runtime_error("Cannot set synthetic review root");
#endif
    }
    ~DataRoot()
    {
#ifdef _WIN32
        _putenv_s("TTRPG_DATA_ROOT",existed?prior.c_str():"");
#else
        if(existed)setenv("TTRPG_DATA_ROOT",prior.c_str(),1);else unsetenv("TTRPG_DATA_ROOT");
#endif
    }
};
struct Gate
{
    std::promise<void> enteredPromise,releasePromise;
    std::shared_future<void> entered=enteredPromise.get_future().share(),released=releasePromise.get_future().share();
    std::atomic_bool signalled{false},opened{false},timedOut{false};
    void wait()
    {
        if(!signalled.exchange(true))enteredPromise.set_value();
        if(released.wait_for(std::chrono::seconds(3))!=std::future_status::ready)timedOut=true;
    }
    void open(){if(!opened.exchange(true))releasePromise.set_value();}
    ~Gate(){open();}
};
struct Release
{
    std::shared_ptr<Gate> gate;
    ~Release(){gate->open();}
};
struct Trace
{
    struct Event{std::uint64_t serial;PlayerReviewCompilePhase phase;std::thread::id thread;};
    std::mutex mutex;std::vector<Event> events;
    void record(std::uint64_t serial,PlayerReviewCompilePhase phase)
    {std::lock_guard<std::mutex> lock(mutex);events.push_back({serial,phase,std::this_thread::get_id()});}
    std::vector<Event> copy(){std::lock_guard<std::mutex> lock(mutex);return events;}
};
inline PlayerReviewCompileOptions gated(const std::shared_ptr<Gate> &gate,const std::shared_ptr<Trace> &trace,
    PlayerReviewCompilePhase phase=PlayerReviewCompilePhase::BeforeGm)
{
    PlayerReviewCompileOptions options;
    options.phaseHook=[gate,trace,phase](std::uint64_t serial,PlayerReviewCompilePhase current){
        trace->record(serial,current);
        if(current==phase&&!gate->signalled.load())gate->wait();
    };
    return options;
}
inline void entered(const std::shared_ptr<Gate> &gate)
{
    if(gate->entered.wait_for(std::chrono::seconds(2))!=std::future_status::ready)
        throw std::runtime_error("Actual review worker did not reach its test barrier");
}
template<class F> void until(F complete,const char *message)
{
    const auto deadline=Clock::now()+std::chrono::seconds(5);
    while(!complete()&&Clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(!complete())throw std::runtime_error(message);
}
inline void settle(PlayerReviewSession &owner,const Campaign &campaign,const std::string &locale,std::uint64_t epoch)
{
    until([&]{owner.pollPreparation(campaign,locale,epoch);return !owner.preparing();},"Review preparation did not settle");
}
inline void drain(PlayerReviewSession &owner,const Campaign &campaign,const std::string &locale,std::uint64_t epoch)
{
    until([&]{owner.pollPreparation(campaign,locale,epoch);const auto s=owner.compileStats();return !s.running&&!s.pending;},
        "Retired review worker did not drain");
}
}
