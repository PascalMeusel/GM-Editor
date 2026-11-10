#include "Application.hpp"
#include "player_display_performance_fixture.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

// Existing application friend seams; popup actions use observed ImGui controls
// and queued input events. No desktop automation or production behavior switch.
struct ApplicationQaHarness
{
    static void load(Application &app, Campaign campaign)
    { app.editor->setCurrentCampaign(std::move(campaign)); app.activeWindowController=app.editor; app.editor->setWindowOpacity(true); }
    static EditorWindowController &editor(Application &app) { return *app.editor; }
    static GLFWwindow *window(Application &app) { return app.mainWindow.get(); }
};
struct WorkspaceInteractionProbe
{
    static bool silent(EditorWindowController &editor)
    {
        const auto &campaign=editor.currentCampaign();
        return campaign.audioAssets.empty() && campaign.audioPlaylists.empty() && campaign.audioAmbienceLayers.empty() &&
            campaign.audioTriggers.empty() && editor._children.audioPlaybackService->activeAssetIds().empty();
    }
};
#include "player_display_performance_runner.hpp"

int main(int argc,char **argv)
{
    namespace fs=std::filesystem; using namespace player_perf;
    Metrics metrics; fs::path root; bool owned=false,completed=false; std::string failure; Environment environment;
    try
    {
        require(argc>=2&&argc<=5,"Usage: player_display_performance <new-run-directory> [--finish-all-viewports] [--no-snapshots] [--review-owned]");
        bool diagnostic=false,noSnapshots=false,reviewOwned=false;
        for(int i=2;i<argc;++i){const std::string arg=argv[i];
            if(arg=="--finish-all-viewports")diagnostic=true;else if(arg=="--no-snapshots")noSnapshots=true;
            else if(arg=="--review-owned")reviewOwned=true;
            else throw std::runtime_error("Unknown performance option: "+arg);}
        root=fs::absolute(fs::u8path(argv[1])).lexically_normal();
        require(!fs::exists(root),"Refusing an existing evidence directory: "+root.u8string());
        owned=fs::create_directory(root); require(owned,"Cannot reserve evidence directory");
        fs::create_directories(root/"data"); fs::create_directories(root/"temp");
        environment.set("TTRPG_DATA_ROOT",(root/"data").u8string()); environment.set("TTRPG_DB_PATH",(root/"data/fixture.db").u8string());
        environment.set("TTRPG_SETTINGS_PATH",(root/"data/settings.ini").u8string());
        environment.set("TEMP",(root/"temp").u8string()); environment.set("TMP",(root/"temp").u8string());
        metrics.facts["run_root"]=root.u8string(); metrics.facts["start_utc"]=currentUtcTimestamp();
        auto fixture=makeFixture(root,metrics); Runner runner{fixture,metrics,diagnostic,noSnapshots};
        runner.reviewOwned=reviewOwned;
        ApplicationRunOptions options; options.width=1280; options.height=720; options.showWindow=false;
        options.maximizeOnStart=false; options.maximumFrames=20000;
        options.initialized=[&](Application &app) { runner.initialized(app); };
        options.beforeFrame=[&](Application &app,std::uint64_t frame) {
            try { runner.before(app,frame); } catch(const std::exception &e) { throw std::runtime_error(runner.context()+": "+e.what()); }
        };
        options.afterRender=[&](Application &app,std::uint64_t frame) {
            try { runner.after(app,frame); } catch(const std::exception &e) { throw std::runtime_error(runner.context()+": "+e.what()); }
        };
        {
            gm::image_texture::ScopedMeasurements imageMeasurements(&metrics.imageStages);
            Application app(std::move(options));
            try { app.run(); } catch(...) { runner.popup.reset();throw; }
            runner.popup.reset();
            require(runner.complete,"Application ended before every planned phase completed");
        } // No GL query after Application/context destruction.
        require(ProjectAssetImport::computeContentHash(fixture.source)==fixture.replacementHash,"Final synthetic replacement bytes did not match validated pair");
        completed=true;
    }
    catch(const std::exception &error) { failure=error.what(); }
    metrics.facts["end_utc"]=currentUtcTimestamp();
    try { if(owned) metrics.write(root,completed,failure); }
    catch(const std::exception &error) { failure+=" Evidence write: "+std::string(error.what()); completed=false; }
    if(!completed) { std::cerr << failure << '\n'; return 1; }
    const bool passed=metrics.performancePassed();
    std::cout << "Correctness phases complete. Steady p95<=33ms and max<=100ms: " << (passed ? "PASS" : "FAIL")
        << ". Cold/control costs are separate; no cold responsiveness acceptance. Evidence: " << root.u8string() << '\n';
    return passed ? 0 : 2;
}
