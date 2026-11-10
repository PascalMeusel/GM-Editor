void markdownWorkerCases(const std::filesystem::path &root)
{
    using namespace markdown_test;
    auto c=fixture();const auto first=root/"worker-first.md",second=root/"worker-second.md";
    write(first,std::string(131073,'a'));write(second,"LATEST_REQUEST_BYTES");
    const auto input=[&](const std::filesystem::path &path){TaskInput in;in.kind=TaskKind::ImportPreview;in.campaignId=c.metadata.id;in.path=path;return in;};
    {
        auto gate=std::make_shared<Gate>();Release release{gate};Task task(gated(gate,TaskPhase::Ready));
        const auto oldSerial=task.request(input(first));entered(gate);
        const auto progress=task.progress();check(progress.bytes>=131073&&progress.files>=1,"Actual chunked worker did not report bytes/files");
        check(!task.takeCompletion(),"Held worker exposed completion before ready barrier released");
        const auto newest=task.request(input(second));check(newest!=oldSerial&&!gate->opened,"Replacement waited for blocked prior read");
        gate->open();auto result=completion(task);
        check(result->serial==newest&&result->success&&result->imported.text=="LATEST_REQUEST_BYTES"&&!gate->timedOut,
            "Out-of-order worker completion replaced newest reviewed bytes");
    }
    {
        auto gate=std::make_shared<Gate>();Release release{gate};Task task(gated(gate));task.request(input(first));entered(gate);
        task.cancel();check(!gate->opened&&!gate->timedOut,"Cancel waited for blocked I/O");gate->open();
        until([&]{return !task.progress().running;},"Cancelled read did not retire after release");
        if(auto result=task.takeCompletion())check(result->cancelled&&!result->success,"Cancelled read delivered usable preview");
        const auto serial=task.request(input(second));auto result=completion(task);
        check(result->serial==serial&&result->success,"Explicit retry after cancellation did not work");
    }
    {
        auto gate=std::make_shared<Gate>();Release release{gate};std::weak_ptr<Gate> weak=gate;
        auto task=std::make_unique<Task>(gated(gate));task->request(input(first));entered(gate);
        task.reset();check(!gate->opened&&!gate->timedOut,"Task destruction joined a held file read");gate->open();
        until([&]{return gate->exited.load();},"Retired worker failed to leave external barrier");
        release.gate.reset();gate.reset();until([&]{return weak.expired();},"Detached worker retained its owned hook after retirement");
    }
    {
        TaskOptions options;options.phaseHook=[](std::uint64_t,TaskPhase phase){if(phase==TaskPhase::Reading)throw std::runtime_error("owned I/O failure");};
        const auto failed=run(input(first),options);check(!failed.success&&!failed.cancelled&&!failed.error.empty(),"Owned worker exception became success/cancellation");
        check(bytes(first)==std::string(131073,'a')&&run(input(second)).success,"Worker failure changed source or prevented independent retry");
    }
    // Destroy a completed-but-held export owner: no destination is published,
    // and the owned staging directory is cleaned once its worker can continue.
    {
        TaskInput in;in.source=std::make_shared<const ExportInput>(captureExport(c));auto inventory=run(in);
        check(inventory.success,"Stage cancellation inventory failed");in.inventory=std::make_shared<const ExportPreview>(inventory.inventory);
        in.kind=TaskKind::StageExport;const auto parent=root/"retired-export";std::filesystem::create_directory(parent);in.path=parent/"published";
        auto gate=std::make_shared<Gate>();Release release{gate};auto task=std::make_unique<Task>(gated(gate,TaskPhase::Ready));
        task->request(in);entered(gate);check(!std::filesystem::is_empty(parent)&&!std::filesystem::exists(in.path),"Export did not retain unpublished staging at barrier");
        task.reset();check(!gate->timedOut&&!gate->opened,"Export destruction joined blocked worker");gate->open();
        until([&]{return std::filesystem::is_empty(parent);},"Retired export left staging after worker release");
        check(!std::filesystem::exists(in.path),"Retired export published after owner destruction");
    }
}
