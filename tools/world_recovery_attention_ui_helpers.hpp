#pragma once
#include "Views/Editor/WorkspaceUi.hpp"

enum class WorldAttentionCheck { None, Pending, Failed, Cleanup };
UiInteractionTrace::Item worldAttention(Interaction &ui,bool de,WorldAttentionCheck state,
    const UiInteractionTrace::Item *baseline=nullptr)
{
    ui.frame(2);const auto item=ui.locate(de?"Welt":"World");
    auto *owner=ImGui::FindWindowByName(item.window.c_str());
    require(owner&&owner->InnerClipRect.Contains(ImRect(item.minimum,item.maximum)),"World attention anchor is clipped");
    if(baseline)require(item.id==baseline->id&&item.window==baseline->window&&
        item.minimum.x==baseline->minimum.x&&item.minimum.y==baseline->minimum.y&&
        item.maximum.x==baseline->maximum.x&&item.maximum.y==baseline->maximum.y,
        "World recovery attention changed the actual navigation ID or rectangle");
    const ImU32 amber=ImGui::GetColorU32(WorkspaceUi::Amber);
    const ImRect trailing(ImVec2(item.maximum.x-45.f,item.minimum.y),item.maximum);
    int cueVertices=0;for(const auto &vertex:owner->DrawList->VtxBuffer)
        if(vertex.col==amber&&trailing.Contains(vertex.pos))++cueVertices;
    require(state==WorldAttentionCheck::None?cueVertices==0:cueVertices>=8,
        "World recovery cue draw geometry does not match cached attention state");
    if(state==WorldAttentionCheck::None)return item;
    const char *expected=state==WorldAttentionCheck::Pending?
        (de?"Gesicherte Weltdaten müssen geprüft werden. Welt > Weltentwürfe prüfen.":"Saved World checkpoint needs review. World > Review World drafts."):
        state==WorldAttentionCheck::Cleanup?
        (de?"Kampagnenänderungen sind gespeichert; die Weltsicherung muss noch bereinigt werden.":"Campaign changes were saved; World recovery cleanup still needs retry."):
        (de?"Die Weltsicherung benötigt Aufmerksamkeit. Lass Entwürfe geöffnet und prüfe den Ordnerzugriff.":"World recovery needs attention. Keep drafts open and check folder access before retrying.");
    const auto center=ImRect(item.minimum,item.maximum).GetCenter();
    ImGui::GetIO().AddMousePosEvent(center.x,center.y);ui.frame(40);
    require(ImGui::GetCurrentContext()->HoveredId==item.id,"Actual World attention button is not hovered");
    const auto rendered=ui.renderedText();
    require(rendered.find(expected)!=std::string::npos,"Actual World hover omitted its exact generic recovery guidance");
    bool tooltip=false;for(auto *window:ImGui::GetCurrentContext()->Windows)
        if(window->Active&&(window->Flags&ImGuiWindowFlags_Tooltip)) {
            const auto *viewport=ImGui::GetMainViewport();
            const ImRect work(viewport->WorkPos,ImVec2(viewport->WorkPos.x+viewport->WorkSize.x,viewport->WorkPos.y+viewport->WorkSize.y));
            require(work.Contains(window->Rect()),"World attention tooltip is outside the actual viewport");tooltip=true;
        }
    require(tooltip,"No actual World attention tooltip was submitted");
    return item;
}
