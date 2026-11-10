#pragma once
#include "Models/Media/FrozenImageDecodeWorker.hpp"
#include "Models/Session/FrozenImageStorage.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <thread>

namespace frozen_decode_test
{
inline void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
template<class Predicate> void until(Predicate predicate,const char *message)
{
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!predicate()){check(std::chrono::steady_clock::now()<end,message);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
struct Fixture
{
    std::shared_ptr<const FrozenImageStorage> storage=FrozenImageStorage::create();
    std::shared_ptr<const FrozenImageAssetGeneration> generation=std::make_shared<const FrozenImageAssetGeneration>();
    std::filesystem::path path;
    explicit Fixture(const std::string &data="IMMUTABLE_ENCODED_FIXTURE")
    {
        check(bool(storage),"Could not create owned decoder storage");path=storage->directory()/"image.png";
        std::ofstream out(path,std::ios::binary);out.write(data.data(),static_cast<std::streamsize>(data.size()));
        check(bool(out),"Could not write owned decoder fixture");
    }
    FrozenImageAsset asset()const{return {path,ProjectAssetImport::computeContentHash(path),generation,storage};}
};
struct Gate
{
    std::promise<void> enteredPromise,releasePromise;
    std::shared_future<void> entered=enteredPromise.get_future().share(),released=releasePromise.get_future().share();
    std::atomic<bool> enteredOnce{false},opened{false};
    void hold(){if(enteredOnce.exchange(true))return;enteredPromise.set_value();
        check(released.wait_for(std::chrono::seconds(5))==std::future_status::ready,"Decoder test gate timed out");}
    void open(){if(!opened.exchange(true))releasePromise.set_value();}
};
// Declare after the worker so exception unwinding opens the gate before its
// permitted teardown join. Test callbacks own only shared state.
struct Release{std::shared_ptr<Gate> gate;~Release(){if(gate)gate->open();}};
inline void entered(const std::shared_ptr<Gate> &gate)
{check(gate->entered.wait_for(std::chrono::seconds(2))==std::future_status::ready,"Decoder did not reach owned barrier");}
struct Decoder
{
    std::atomic<unsigned> calls{0};bool fail=false;
    std::vector<unsigned char> encodedSeen;
    bool decode(const std::vector<unsigned char> &encoded,const std::string &,LoadedRgbaImage &image,const RgbaImageLoadOptions &limits,std::string *error)
    {
        ++calls;encodedSeen=encoded;check(!encoded.empty()&&limits.maximumSourceEdge>0&&limits.maximumSourcePixels>0,"Worker omitted immutable bytes or allocation bounds");
        if(fail){if(error)*error="synthetic.decode.failure";return false;}
        image.width=image.sourceWidth=2;image.height=image.sourceHeight=2;image.exifOrientation=1;image.pixels.assign(16,123);return true;
    }
};
inline FrozenImageDecodeOptions options(const std::shared_ptr<Decoder> &decoder,const std::shared_ptr<Gate> &gate={},
    FrozenImageDecodePhase phase=FrozenImageDecodePhase::BeforeDecode)
{
    FrozenImageDecodeOptions result;result.measure=true;
    result.decoder=[decoder](const auto &encoded,const auto &extension,auto &image,const auto &limits,auto *error){return decoder->decode(encoded,extension,image,limits,error);};
    if(gate)result.phaseHook=[gate,phase](std::uint64_t,FrozenImageDecodePhase actual){if(actual==phase)gate->hold();};
    return result;
}
inline std::unique_ptr<FrozenImageDecodeResult> take(FrozenImageDecodeWorker &worker)
{
    std::unique_ptr<FrozenImageDecodeResult> result;
    until([&]{result=worker.takeCompletion();return bool(result);},"Decoder completion did not arrive");return result;
}
inline void acknowledge(FrozenImageDecodeWorker &worker,std::unique_ptr<FrozenImageDecodeResult> &result)
{const auto ticket=result->ticket;result.reset();worker.acknowledge(ticket);}
}
