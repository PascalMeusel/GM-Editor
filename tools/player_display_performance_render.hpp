#pragma once
#include "player_display_performance_fixture.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "imgui_internal.h"
#include <GLFW/glfw3.h>
#include <limits>
#include <set>

namespace player_perf
{
struct Context
{
    GLFWwindow *previous=glfwGetCurrentContext();
    explicit Context(GLFWwindow *window) { glfwMakeContextCurrent(window); }
    ~Context() { glfwMakeContextCurrent(previous); }
};
inline void checkGl(const std::string &phase)
{ const auto error=glGetError(); require(error==GL_NO_ERROR,phase+": GL error="+std::to_string(error)); }
inline ImGuiWindow *playerWindow()
{
    for(auto *window:ImGui::GetCurrentContext()->Windows)
        if(!(window->Flags&ImGuiWindowFlags_ChildWindow) && std::string(window->Name).find("###LocalPlayerDisplay")!=std::string::npos)
            return window;
    return nullptr;
}
inline ImGuiViewport *detachedPlayer()
{
    auto *window=playerWindow();
    require(window && window->Viewport && window->Viewport!=ImGui::GetMainViewport() && window->Viewport->PlatformHandle,
        "Player Display has no independent native viewport");
    int w=0,h=0; glfwGetFramebufferSize(static_cast<GLFWwindow *>(window->Viewport->PlatformHandle),&w,&h);
    require(w>0 && h>0,"Detached Player Display has empty framebuffer");
    return window->Viewport;
}
struct ImageDraw { ImTextureID texture=0; ImRect bounds; ImGuiViewport *viewport=nullptr; };
inline ImageDraw imageDraw(ImTextureID known=0)
{
    auto *viewport=detachedPlayer(); const auto *data=viewport->DrawData;
    require(data && data->Valid,"Player viewport draw data unavailable");
    std::optional<Context> context;
    if(!known) context.emplace(static_cast<GLFWwindow *>(viewport->PlatformHandle));
    GLint priorBinding=0;
    if(!known) { checkGl("before texture discovery"); glGetIntegerv(GL_TEXTURE_BINDING_2D,&priorBinding); }
    for(int list=0;list<data->CmdListsCount;++list) for(const auto &command:data->CmdLists[list]->CmdBuffer)
    {
        const ImTextureID id=command.GetTexID();
        if(command.UserCallback || command.ElemCount==0 || !id || (known && id!=known)) continue;
        if(!known)
        {
            const auto handle=static_cast<GLuint>(id); if(!glIsTexture(handle)) continue;
            glBindTexture(GL_TEXTURE_2D,handle); GLint w=0,h=0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);
            glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(priorBinding)); checkGl("texture discovery");
            if(w!=4096 || h!=4096) continue;
        }
        const auto *draw=data->CmdLists[list];
        ImRect bounds(ImVec2(FLT_MAX,FLT_MAX),ImVec2(-FLT_MAX,-FLT_MAX));
        for(unsigned i=0;i<command.ElemCount;++i)
        {
            const auto index=draw->IdxBuffer[command.IdxOffset+i]+command.VtxOffset;
            bounds.Add(draw->VtxBuffer[index].pos);
        }
        const ImRect clip(ImVec2(command.ClipRect.x,command.ClipRect.y),ImVec2(command.ClipRect.z,command.ClipRect.w));
        require(bounds.GetWidth()>32 && bounds.GetHeight()>32 && clip.Contains(bounds),
            "Large handout draw is empty or clipped; fixture does not establish visible image workload");
        return {id,bounds,viewport};
    }
    return {};
}
inline Color readPixel(ImGuiViewport *viewport, ImVec2 position, GLenum buffer=GL_FRONT)
{
    Context context(static_cast<GLFWwindow *>(viewport->PlatformHandle));
    int w=0,h=0; glfwGetFramebufferSize(glfwGetCurrentContext(),&w,&h);
    require(viewport->Size.x>0 && viewport->Size.y>0 && w>0 && h>0,"Invalid pixel sample viewport dimensions");
    const int x=static_cast<int>((position.x-viewport->Pos.x)*w/viewport->Size.x);
    const int y=h-1-static_cast<int>((position.y-viewport->Pos.y)*h/viewport->Size.y);
    require(x>=0 && y>=0 && x<w && y<h,"Framebuffer sample outside actual player window");
    GLint oldRead=0,oldPack=0; glGetIntegerv(GL_READ_BUFFER,&oldRead); glGetIntegerv(GL_PACK_ALIGNMENT,&oldPack);
    glReadBuffer(buffer); glPixelStorei(GL_PACK_ALIGNMENT,1); Color pixel{};
    glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
    glReadBuffer(static_cast<GLenum>(oldRead)); glPixelStorei(GL_PACK_ALIGNMENT,oldPack); checkGl("player front-buffer sample");
    return pixel;
}
inline void positivePixels(const ImageDraw &draw, GLenum buffer=GL_FRONT,int permutation=0)
{
    require(draw.texture!=0,"No large handout texture submitted in player viewport");
    for(int q=0;q<4;++q)
    {
        const ImVec2 point(draw.bounds.Min.x+draw.bounds.GetWidth()*(q%2 ? .75f : .25f),
                           draw.bounds.Min.y+draw.bounds.GetHeight()*(q/2 ? .75f : .25f));
        const auto pixel=readPixel(draw.viewport,point,buffer);
        for(int channel=0;channel<3;++channel)
            require(std::abs(int(pixel[channel])-int(colors[(q+permutation)%4][channel]))<=25,
                "Player front-buffer color mismatch quadrant="+std::to_string(q)+" channel="+std::to_string(channel)+
                " actual="+std::to_string(pixel[channel])+" expected="+std::to_string(colors[(q+permutation)%4][channel]));
    }
}
inline void blackoutPixels()
{
    auto *viewport=detachedPlayer(); auto *window=playerWindow();
    const auto pixel=readPixel(viewport,window->InnerRect.GetCenter());
    require(pixel[0]<12 && pixel[1]<12 && pixel[2]<12,"Player blackout interior was not black");
}
inline void mapPixels(Metrics &metrics)
{
    auto *viewport=detachedPlayer(); ImGuiWindow *canvas=nullptr;
    for(auto *window:ImGui::GetCurrentContext()->Windows)
        if(window->Viewport==viewport && std::string(window->Name).find("PlayerDisplayMapCanvas")!=std::string::npos)
            canvas=window;
    require(canvas && canvas->Active && !canvas->Hidden && !canvas->SkipItems && canvas->DrawList &&
        canvas->DrawList->VtxBuffer.Size>100,"Player map canvas did not submit substantive visible geometry");
    auto visible=canvas->InnerRect; visible.ClipWith(canvas->InnerClipRect);
    require(visible.GetWidth()>100 && visible.GetHeight()>100,"Player map canvas has no useful visible region");
    std::set<unsigned> sampledColors;
    for(int y=1;y<=5;++y) for(int x=1;x<=5;++x)
    {
        const auto color=readPixel(viewport,ImVec2(visible.Min.x+visible.GetWidth()*x/6,
                                                 visible.Min.y+visible.GetHeight()*y/6));
        sampledColors.insert((unsigned(color[0]/16)<<8)|(unsigned(color[1]/16)<<4)|unsigned(color[2]/16));
    }
    require(sampledColors.size()>=3,"Player map framebuffer sample was blank/uniform despite geometry");
    metrics.facts["map_render_positive_control"]="Production PlayerDisplayMapCanvas: "+
        std::to_string(canvas->DrawList->VtxBuffer.Size)+" vertices; "+std::to_string(sampledColors.size())+" quantized colors in25 interior samples";
}
inline bool textureAlive(ImTextureID id) { return id && glIsTexture(static_cast<GLuint>(id))==GL_TRUE; }
inline void requireCounts(std::size_t expected, const std::string &phase)
{
    const auto stats=imageTextureCacheStats();
    require(stats.scopedEntries==expected && stats.scopedLoadedTextures==expected,
        phase+": scoped entries="+std::to_string(stats.scopedEntries)+" loaded="+std::to_string(stats.scopedLoadedTextures)+
        " expected="+std::to_string(expected));
}
inline void finishAllViewports()
{
    Context restore(glfwGetCurrentContext());
    for(auto *viewport:ImGui::GetPlatformIO().Viewports)
        if(viewport->PlatformHandle) { glfwMakeContextCurrent(static_cast<GLFWwindow *>(viewport->PlatformHandle)); glFinish(); checkGl("diagnostic all-viewport finish"); }
}
}
