// Opt-in memory-only platform model. No OS window/focus/input or renderer calls.
// Keep this state alive until after ImGui::DestroyContext invokes its callbacks.
struct CompactPopupPlatform
{
    struct Window { ImVec2 position, size; bool owned = false; };
    Window main{ImVec2(40,260),ImVec2(1280,720),false};
    static Window &state(ImGuiViewport *v) { return *static_cast<Window *>(v->PlatformUserData); }
    CompactPopupPlatform()
    {
        auto &io=ImGui::GetIO();io.DisplaySize=main.size;
        io.ConfigFlags|=ImGuiConfigFlags_ViewportsEnable;
        io.BackendFlags|=ImGuiBackendFlags_PlatformHasViewports|ImGuiBackendFlags_RendererHasViewports|
            ImGuiBackendFlags_HasMouseHoveredViewport;
        auto &p=ImGui::GetPlatformIO();
        p.Platform_CreateWindow=[](ImGuiViewport *v){v->PlatformUserData=new Window{v->Pos,v->Size,true};};
        p.Platform_DestroyWindow=[](ImGuiViewport *v){auto *s=static_cast<Window *>(v->PlatformUserData);
            if(s&&s->owned)delete s;v->PlatformUserData=nullptr;v->PlatformHandle=nullptr;};
        p.Platform_GetWindowPos=[](ImGuiViewport *v){return state(v).position;};
        p.Platform_GetWindowSize=[](ImGuiViewport *v){return state(v).size;};
        p.Platform_SetWindowPos=[](ImGuiViewport *v,ImVec2 x){state(v).position=x;};
        p.Platform_SetWindowSize=[](ImGuiViewport *v,ImVec2 x){state(v).size=x;};
        p.Platform_ShowWindow=[](ImGuiViewport *){};
        p.Platform_SetWindowTitle=[](ImGuiViewport *,const char *){};
        p.Platform_GetWindowMinimized=[](ImGuiViewport *){return false;};
        p.Platform_GetWindowFocus=nullptr;p.Platform_SetWindowFocus=nullptr;
        ImGuiPlatformMonitor monitor;monitor.MainSize=ImVec2(1920,1080);
        monitor.WorkSize=ImVec2(1920,1040);monitor.DpiScale=1.f;p.Monitors.push_back(monitor);
        ImGui::GetMainViewport()->PlatformUserData=&main;
    }
};
