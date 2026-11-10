#pragma once

#include "imgui.h"

#include <algorithm>
#include <cstring>
#include <string_view>

namespace gm::presentation_review_ui
{
inline void drawWrappedWarning(std::string_view message)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.43f, 0.34f, 1.0f));
    ImGui::TextWrapped("%.*s", static_cast<int>(message.size()), message.data());
    ImGui::PopStyleColor();
}

inline void drawWrappedBullet(std::string_view message)
{
    ImGui::Bullet();
    ImGui::SameLine();
    ImGui::TextWrapped("%.*s", static_cast<int>(message.size()), message.data());
}

// Match TaskUi::button's wrapped-label height. The footer lives outside the
// scrolling review so long localized labels cannot push Cancel out of reach.
struct FooterLayout
{
    ImVec2 confirm, cancel;
    bool stacked=false;
    float reservedHeight=0;
};
// Capture this in the popup before entering its scrolling child. Keyboard
// actions can remove or disable their own control while preparation runs;
// transfer focus once to the persistent Cancel button without activating it.
class FooterFocusTransfer
{
    ImGuiStorage *storage_=ImGui::GetStateStorage();
    ImGuiID key_=ImGui::GetID("##ReviewFooterFocusTransfer");
public:
    FooterFocusTransfer()
    { if(ImGui::IsWindowAppearing())storage_->SetBool(key_,false); }
    void rememberKeyboardActivation() const
    {
        // IsItemFocused alone also includes focus assigned by a mouse click.
        if(ImGui::IsItemFocused()&&ImGui::IsAnyItemFocused())storage_->SetBool(key_,true);
    }
    void focusCancel() const
    {
        if(!storage_->GetBool(key_))return;
        storage_->SetBool(key_,false);
        ImGui::SetKeyboardFocusHere();
    }
};
inline float buttonHeight(const char *label,float width)
{
    const auto padding=ImGui::GetStyle().FramePadding;
    const char *end=std::strstr(label,"##");
    if(!end)end=label+std::strlen(label);
    return ImGui::CalcTextSize(label,end,false,std::max(1.f,width-padding.x*2)).y+padding.y*2;
}
inline FooterLayout measureFooter(const char *confirm,const char *cancel,const char *explanation=nullptr)
{
    const auto &style=ImGui::GetStyle();
    const float width=std::max(1.f,ImGui::GetContentRegionAvail().x);
    const float confirmWidth=std::max(190.f,ImGui::CalcTextSize(confirm,nullptr,true).x+style.FramePadding.x*2);
    const float cancelWidth=std::max(100.f,ImGui::CalcTextSize(cancel,nullptr,true).x+style.FramePadding.x*2);
    FooterLayout result;
    result.stacked=confirmWidth+style.ItemSpacing.x+cancelWidth>width;
    result.confirm.x=result.stacked?width:confirmWidth;
    result.cancel.x=result.stacked?width:cancelWidth;
    result.confirm.y=buttonHeight(confirm,result.confirm.x);
    result.cancel.y=buttonHeight(cancel,result.cancel.x);
    result.reservedHeight=result.stacked?result.confirm.y+style.ItemSpacing.y+result.cancel.y:
        std::max(result.confirm.y,result.cancel.y);
    if(explanation&&*explanation)
        result.reservedHeight+=ImGui::CalcTextSize(explanation,nullptr,false,width).y+style.ItemSpacing.y;
    return result;
}
inline float reviewBodyHeight(const FooterLayout &footer)
{
    return std::max(1.f,ImGui::GetContentRegionAvail().y-footer.reservedHeight-ImGui::GetStyle().ItemSpacing.y);
}
inline void nextFooterButton(const FooterLayout &footer)
{
    if(!footer.stacked)ImGui::SameLine();
}
}
