void frozenWorkerCreditAndQueue()
{
    using namespace frozen_decode_test;
    Fixture first,second,third,fourth;auto gate=std::make_shared<Gate>();auto decoder=std::make_shared<Decoder>();
    auto config=options(decoder,gate);config.maxQueued=2;FrozenImageDecodeWorker worker(config);Release release{gate};
    const auto one=worker.request(first.asset());check(one!=0,"First decoder request refused");entered(gate);
    check(worker.request(first.asset())==one,"Duplicate immutable request consumed another ticket");
    const auto two=worker.request(second.asset()),three=worker.request(third.asset());
    check(two&&three&&two!=three&&worker.request(fourth.asset())==0,"Bounded FIFO queue admitted excess work");
    worker.cancel(two);check(!gate->opened&&worker.stats().running,"Cancel waited for held decoder");gate->open();
    auto result=take(worker);check(result->ticket==one&&result->success&&result->image.pixels.size()==16,"First immutable decode failed");
    check(worker.stats().started==1&&worker.stats().completionOutstanding&&!worker.takeCompletion(),
        "Taken completion released its credit before caller freed pixels");
    acknowledge(worker,result);result=take(worker);
    check(result->ticket==three&&result->success&&decoder->calls==2,"FIFO cancellation skipped/reordered the surviving request");
    acknowledge(worker,result);worker.shutdown();
    check(worker.request(fourth.asset())==0&&!worker.stats().running,"Stopped worker accepted work");
    auto zero=options(decoder);zero.maxQueued=0;FrozenImageDecodeWorker unavailable(zero);
    check(unavailable.request(fourth.asset())==0,"Zero queue budget did not refuse admission");
}
void frozenWorkerCancellationAndStorage()
{
    using namespace frozen_decode_test;
    Fixture f;const auto asset=f.asset();const auto path=f.path;
    auto gate=std::make_shared<Gate>();auto decoder=std::make_shared<Decoder>();
    FrozenImageDecodeWorker worker(options(decoder,gate));Release release{gate};
    const auto ticket=worker.request(asset);check(ticket!=0,"Storage request refused");entered(gate);
    f.generation.reset();f.storage.reset();check(!asset.valid()&&fs::exists(path),"Worker lease retained consent or lost encoded storage mid-read");
    worker.cancel(ticket);check(!gate->opened&&worker.stats().running,"Owner cancellation joined the held worker");
    gate->open();until([&]{return !worker.stats().running;},"Cancelled decode did not finish");
    if(auto result=worker.takeCompletion())acknowledge(worker,result);
    until([&]{return !fs::exists(path);},"Decode completion retained its storage lease");
    check(decoder->calls==0&&!worker.takeCompletion(),"Revoked generation decoded or exposed a late result");
    Fixture fresh;const auto retry=worker.request(fresh.asset());auto result=take(worker);
    check(result->ticket==retry&&result->success,"Cancellation poisoned later independent work");acknowledge(worker,result);
    // Queue storage leases protect files without prolonging player consent.
    auto held=std::make_shared<Gate>();auto cfg=options(decoder,held);FrozenImageDecodeWorker queued(cfg);Release releaseQueued{held};
    const auto active=queued.request(fresh.asset());entered(held);Fixture expired;auto late=expired.asset();const auto latePath=expired.path;
    check(queued.request(late)!=0,"Queued expiry fixture refused");expired.generation.reset();expired.storage.reset();
    check(!late.valid()&&fs::exists(latePath),"Queued storage lease lost files or prolonged consent");held->open();
    result=take(queued);check(result->ticket==active,"Expired queued item overtook active work");acknowledge(queued,result);
    until([&]{return !queued.stats().running&&queued.stats().queued==0;},"Expired queued item did not retire");
    if(auto discarded=queued.takeCompletion()){check(!discarded->success,"Expired queued item succeeded");acknowledge(queued,discarded);}
    until([&]{return !fs::exists(latePath);},"Retired queued work retained storage");
}
void frozenWorkerValidationAndParity()
{
    using namespace frozen_decode_test;
    Fixture f;auto decoder=std::make_shared<Decoder>();FrozenImageDecodeWorker worker(options(decoder));
    check(!worker.request({})&&!worker.request(FrozenImageAsset(f.path,"hash",f.generation)),"Missing storage/descriptor queued IO");
    auto outside=FrozenImageAsset(f.path.parent_path().parent_path()/"outside.png","hash",f.generation,f.storage);
    check(!worker.request(outside),"Storage lease authorized an external path");
    auto mismatch=FrozenImageAsset(f.path,"fnv1a64:0000000000000000",f.generation,f.storage);
    const auto bad=worker.request(mismatch);check(bad!=0,"Hash mismatch did not reach independent byte validation");auto result=take(worker);
    check(!result->success&&!result->error.empty()&&result->image.pixels.empty()&&decoder->calls==0,"Wrong hash reached decoder/pixels");acknowledge(worker,result);
    decoder->fail=true;check(worker.request(f.asset())!=0,"Failure request refused");result=take(worker);
    check(!result->success&&!result->error.empty()&&result->image.pixels.empty(),"Decoder failure retained pixels or succeeded");acknowledge(worker,result);
    Fixture png;LoadedRgbaImage source;source.width=source.sourceWidth=3;source.height=source.sourceHeight=2;
    source.pixels={255,0,0,255,0,255,0,170,0,0,255,80,13,47,91,0,29,61,127,255,3,7,11,33};
    std::string error;check(saveRgbaImageAsPng(png.path,source,&error),"Could not encode real parity PNG");
    LoadedRgbaImage synchronous;RgbaImageLoadOptions limits;limits.maximumSourceEdge=8192;limits.maximumSourcePixels=64ull*1024*1024;
    check(loadRgbaImage(png.path,synchronous,limits,&error),"Could not decode synchronous parity control");
    FrozenImageDecodeOptions measured;measured.measure=true;FrozenImageDecodeWorker real(measured);
    check(real.request(png.asset())!=0,"Real parity request refused");result=take(real);const auto &actual=result->image;
    check(result->success&&actual.width==synchronous.width&&actual.height==synchronous.height&&actual.sourceWidth==synchronous.sourceWidth&&
        actual.sourceHeight==synchronous.sourceHeight&&actual.exifOrientation==synchronous.exifOrientation&&actual.pixels==synchronous.pixels,
        "Worker immutable-byte decoder differs from real synchronous RGBA output");
    check(result->measurements.hashReads==1&&result->measurements.decodes==1&&result->measurements.hashMilliseconds>=0&&
        result->measurements.decodeMilliseconds>=0,"Worker did not retain its owned timing samples");acknowledge(real,result);
    // A malformed payload is real decoder input, not a fake success seam.
    check(real.request(f.asset())!=0,"Malformed real image request refused before decoding");result=take(real);
    check(!result->success&&result->image.pixels.empty(),"Malformed encoded file produced worker pixels");acknowledge(real,result);
}
