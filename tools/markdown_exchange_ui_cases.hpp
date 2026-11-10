#include "markdown_exchange_ui_helpers.hpp"
void markdownUpdatesUi(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();ui.context(std::string("Markdown actual update/failure/consent 27px ")+(de?"DE":"EN"));
    MarkdownUiFixture f;auto exchange=std::make_shared<EditorMarkdownExchange>(f.editor);ExchangePanel panel;
    auto &c=f.campaign();SessionEventCounter events(c.metadata.id);std::string draft="PARENT_DRAFT";const auto route=PlayerReviewUiProbe::route(f.editor);
    ui.draw=[&]{ImGui::InputText("##markdown-parent-draft",&draft);panel.drawLauncher(*exchange);exchange->poll();};ui.frame(3);
    ui.compactType("##markdown-parent-draft","UNSAVED_PARENT_DRAFT");
    const char *open=de?"Kampagnennotizen austauschen":"Exchange campaign notes",*close=de?"Zurück zur Kampagne":"Return to campaign";
    const char *apply=de?"Gewählte Änderungen übernehmen":"Apply selected updates",*select=de?"Zum Aktualisieren wählen":"Select for update";
    const char *refresh=de?"Änderungen aktualisieren":"Refresh changes",*review=de?"Ich habe die gewählten Notizänderungen geprüft":"I reviewed the selected note changes";
    ui.compactClick(open);ui.compactBounds(close,"CampaignMarkdownExchange");
    ui.compactClick(de?"Exportierte Notizen aktualisieren":"Update exported notes");
    ui.compactType("##exchangeUpdateSource",(f.root/"export").u8string().c_str());
    const auto pane=ui.compactItem("##exchangeUpdateSource").window;
    ui.compactClick(de?"Änderungen prüfen":"Preview changes");markdownUiWait(ui,*exchange);
    require(exchange->ready()&&exchange->snapshot()->rows.size()==2,"Actual update preview did not become ready");
    ui.reviewState(apply,true);const auto eligible=markdownUiRow(ui,select,f.first,pane),conflict=markdownUiRow(ui,select,f.second,pane);
    markdownUiDisabled(ui,select,conflict);ui.compactClick(select,eligible);
    ui.compactClick(de?"Original beim Export":"Original at export");markdownUiRaw(ui,pane,"ComparedText","ORIGINAL_FIRST");
    ui.compactClick(de?"Bearbeitete Datei":"Edited file");markdownUiRaw(ui,pane,"ComparedText","EXTERNAL_FIRST");
    ui.compactClick(review);ui.reviewBounds(apply,close,27.f);ui.reviewState(apply,false);
    const auto before=exchange->snapshot();
    {CompactSqlFault fault("reject_markdown_ui","story_threads");ui.compactClick(apply);markdownUiWait(ui,*exchange);}
    require(exchange->ready()&&exchange->snapshot()==before&&!exchange->message().empty()&&
        f.saved(f.first)=="ORIGINAL_FIRST"&&f.saved(f.second)=="CONCURRENT_SECOND"&&events.saves==0,
        "Failed actual apply changed SQL or discarded reviewed edits");
    ui.reviewState(apply,true);ui.compactClick(refresh);markdownUiWait(ui,*exchange);
    ui.compactClick(select,markdownUiRow(ui,select,f.first,pane));ui.compactClick(review);
    ui.reviewTabTo(apply);ui.reviewKey(ImGuiKey_Space);markdownUiWait(ui,*exchange);
    require(f.saved(f.first)=="EXTERNAL_FIRST"&&f.saved(f.second)=="CONCURRENT_SECOND"&&events.saves==1&&draft=="UNSAVED_PARENT_DRAFT"&&
        PlayerReviewUiProbe::route(f.editor)==route,"Explicit successful retry changed conflict/draft/route or saved more than once");
    ui.compactBounds(close,"CampaignMarkdownExchange");ui.reviewTabTo(close);ui.reviewKey(ImGuiKey_Space);
    require(!ui.popupOpen()&&draft=="UNSAVED_PARENT_DRAFT","Keyboard footer failed to close while preserving parent draft");
}

void markdownPendingUi(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();ui.context(std::string("Markdown pending/retained/cancel/context 27px ")+(de?"DE":"EN"));
    MarkdownUiFixture f;auto gate=std::make_shared<markdown_test::Gate>();markdown_test::Release release{gate};
    auto options=markdown_test::gated(gate);auto exchange=std::make_shared<EditorMarkdownExchange>(f.editor,options);ExchangePanel panel;
    const auto path=f.root/"private.md";markdownUiWrite(path,"PRIVATE_REVIEW_BYTES");SessionEventCounter events(f.campaign().metadata.id);
    ui.draw=[&]{panel.drawLauncher(*exchange);exchange->poll();};ui.frame(3);
    const char *close=de?"Zurück zur Kampagne":"Return to campaign",*stop=de?"Vorgang abbrechen":"Cancel operation";
    const char *apply=de?"Als private Handlungsstrang-Notiz importieren":"Import as private story-thread note";
    const char *refresh=de?"Dateivorschau aktualisieren":"Refresh file preview";
    ui.compactClick(de?"Kampagnennotizen austauschen":"Exchange campaign notes");ui.compactClick(de?"Markdown importieren":"Import Markdown");
    ui.compactType("##exchangeSource",path.u8string().c_str());const auto pane=ui.compactItem("##exchangeSource").window;
    ui.compactClick(de?"Datei als Vorschau laden":"Preview file");markdown_test::entered(gate);
    require(exchange->busy()&&!exchange->ready(),"Held actual preview was already actionable");ui.reviewBounds(apply,close,27.f);ui.reviewState(apply,true);
    ui.reviewTabTo(stop);ui.reviewKey(ImGuiKey_Space);
    require(!exchange->busy()&&!exchange->ready()&&!gate->opened&&!gate->timedOut&&events.saves==0,"Actual Cancel waited or persisted");
    gate->open();ui.compactClick(de?"Datei als Vorschau laden":"Preview file");markdownUiWait(ui,*exchange);
    require(exchange->ready()&&exchange->snapshot()->imported.text=="PRIVATE_REVIEW_BYTES","Fresh actual preview failed after cancellation");
    const auto prior=exchange->snapshot();markdownUiWrite(path,std::string("a\0b",3));ui.compactClick(refresh);markdownUiWait(ui,*exchange);
    require(exchange->status()==EditorMarkdownExchange::Status::Failed&&exchange->snapshot()==prior&&
        prior->imported.text=="PRIVATE_REVIEW_BYTES"&&!exchange->ready(),"Failed refresh lost immutable previous preview or kept approval");
    markdownUiRaw(ui,pane,"ImportedText","PRIVATE_REVIEW_BYTES");
    ui.reviewState(apply,true);markdownUiWrite(path,"PRIVATE_NEW_BYTES");ui.compactClick(refresh);markdownUiWait(ui,*exchange);
    require(exchange->ready()&&exchange->snapshot()->imported.text=="PRIVATE_NEW_BYTES","Explicit refreshed preview did not adopt current bytes");
    f.campaign().presentationModeActive=true;ui.frame(2);
    require(!contains(ui.rendered,"PRIVATE_NEW_BYTES")&&!exchange->ready(),"Presentation mode exposed private review or kept consent");
    f.campaign().presentationModeActive=false;ui.frame(2);ui.reviewKey(ImGuiKey_Escape);
    require(events.saves==0&&f.saved(f.first)=="ORIGINAL_FIRST","Preview/cancel/privacy changed persisted notes");
}

void markdownOwnerRevocationUi()
{
    // Real source request, held after bytes are read. These cases exercise the
    // coordinator's context binding; no readiness or completion is fabricated.
    for(int boundary=0;boundary<4;++boundary) {
        HeadlessPanel ui(false,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
        ui.context("Markdown completed-before-context-change "+std::to_string(boundary));
        MarkdownUiFixture f;auto gate=std::make_shared<markdown_test::Gate>();markdown_test::Release release{gate};
        auto exchange=std::make_shared<EditorMarkdownExchange>(f.editor,markdown_test::gated(gate,CampaignMarkdownExchange::TaskPhase::Ready));
        ExchangePanel panel;bool surface=true;const auto path=f.root/"late.md";markdownUiWrite(path,"LATE_PRIVATE_REVIEW");
        SessionEventCounter events(f.campaign().metadata.id);
        ui.draw=[&]{if(surface)panel.drawLauncher(*exchange);exchange->poll();};ui.frame(3);
        ui.compactClick("Exchange campaign notes");ui.compactClick("Import Markdown");ui.compactType("##exchangeSource",path.u8string().c_str());
        ui.compactClick("Preview file");markdown_test::entered(gate);
        if(boundary==0) {
            player_review_async_test::DataRoot changed((f.root/"different-data-root").string());ui.frame(3);
        } else if(boundary==1) {
            gm::localization::LocalizationService::instance().setPreference(gm::localization::LocalePreference::German,false);ui.frame(3);
        } else if(boundary==2) {
            require(f.editor.setCurrentCampaign(f.initial),"Same-ID Markdown owner replacement failed");ui.frame(3);
        } else {surface=false;ui.frame(3);}
        gate->open();markdown_test::until([&]{ui.frame();return !exchange->progress().running;},"Revoked Markdown worker did not retire");ui.frame(3);
        require(!exchange->ready()&&!exchange->snapshot()&&events.saves==0&&f.saved(f.first)=="ORIGINAL_FIRST"&&
            !contains(ui.rendered,"LATE_PRIVATE_REVIEW"),"Late preview survived changed root/locale/same-ID/hidden owner");
    }
}
