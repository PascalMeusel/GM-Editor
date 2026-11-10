#pragma once
// Only generated invocation-local paths and mocked clipboard. No Application,
// native window, operating-system clipboard or audio playback is used.
std::filesystem::path worldCheckpointPath(const Campaign &c)
{return ProjectPackageLayout::layoutForCampaign(c).recoveryDirectory/"Drafts"/(std::string(WorldDraftRecovery::Key)+".draft");}
std::string worldCheckpointBytes(const Campaign &c)
{std::ifstream input(worldCheckpointPath(c),std::ios::binary);return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};}
struct WorldCheckpointBlock
{
    std::filesystem::path path,prior;bool saved=false,active=true;
    explicit WorldCheckpointBlock(const Campaign &c):path(worldCheckpointPath(c)),prior(path.string()+".test-prior") {
        std::filesystem::create_directories(path.parent_path());saved=std::filesystem::exists(path);
        if(saved)std::filesystem::rename(path,prior);
        std::filesystem::create_directory(path);std::ofstream(path/"blocker")<<"isolated synthetic nonempty path";
    }
    void release(){if(!active)return;std::filesystem::remove(path/"blocker");std::filesystem::remove(path);
        if(saved)std::filesystem::rename(prior,path);active=false;}
    ~WorldCheckpointBlock(){if(active){try{release();}catch(...){}}}
};
struct WorldClipboardMock
{
    std::string text;
    decltype(ImGui::GetPlatformIO().Platform_SetClipboardTextFn) previous=ImGui::GetPlatformIO().Platform_SetClipboardTextFn;
    void *priorData=ImGui::GetPlatformIO().Platform_ClipboardUserData;
    WorldClipboardMock(){auto &p=ImGui::GetPlatformIO();p.Platform_ClipboardUserData=this;
        p.Platform_SetClipboardTextFn=[](ImGuiContext *,const char *text){static_cast<WorldClipboardMock *>(ImGui::GetPlatformIO().Platform_ClipboardUserData)->text=text;};}
    ~WorldClipboardMock(){auto &p=ImGui::GetPlatformIO();p.Platform_SetClipboardTextFn=previous;p.Platform_ClipboardUserData=priorData;}
};
void worldRecoveryOpen(Interaction &ui,bool de)
{ui.click(de?"Weltentwürfe prüfen":"Review World drafts","Workspace");}
void worldRecoveryRestore(Interaction &ui,bool de)
{worldRecoveryOpen(ui,de);ui.click(de?"Zur Prüfung wiederherstellen":"Restore for review");ui.frame(3);}
void worldRecoveryClose(Interaction &ui)
{ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,true);ui.frame();ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,false);ui.frame(3);}
struct WorldDataRootOverride
{
    std::string previous=std::getenv("TTRPG_DATA_ROOT")?std::getenv("TTRPG_DATA_ROOT"):"";
    static void set(const std::string &value){
#ifdef _WIN32
        require(_putenv_s("TTRPG_DATA_ROOT",value.c_str())==0,"Cannot switch synthetic recovery root");
#else
        require(setenv("TTRPG_DATA_ROOT",value.c_str(),1)==0,"Cannot switch synthetic recovery root");
#endif
    }
    explicit WorldDataRootOverride(const std::filesystem::path &path){set(path.u8string());}
    ~WorldDataRootOverride(){
#ifdef _WIN32
        _putenv_s("TTRPG_DATA_ROOT",previous.c_str());
#else
        setenv("TTRPG_DATA_ROOT",previous.c_str(),1);
#endif
    }
};
