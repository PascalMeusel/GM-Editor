#pragma once
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include <algorithm>

namespace FrozenImageUi
{
inline const CachedImageTexture &lookup(const FrozenImageAsset &asset)
{return getAsyncImageTexture(asset,static_cast<std::uint64_t>(ImGui::GetFrameCount()));}

// Reserve layout independently of decode/upload. Only a genuinely visible
// slot may register work, including when ImGui logging keeps clipped items.
template<class Lookup>
bool slot(const FrozenImageAsset &asset,float height,const char *loading,const char *unavailable,
          const Lookup &lookup,bool upscale=true)
{
    const ImVec2 size(std::max(1.f,ImGui::GetContentRegionAvail().x),std::max(1.f,height));
    ImGui::Dummy(size);
    if(!ImGui::IsItemVisible())return false;
    const auto minimum=ImGui::GetItemRectMin(),maximum=ImGui::GetItemRectMax();
    const CachedImageTexture *texture=asset.valid()?&lookup(asset):nullptr;
    auto *draw=ImGui::GetWindowDrawList();draw->PushClipRect(minimum,maximum,true);
    const bool ready=texture&&texture->loaded&&texture->textureId&&texture->width>0&&texture->height>0;
    if(ready){
        float scale=std::min(size.x/texture->width,size.y/texture->height);
        if(!upscale)scale=std::min(1.f,scale);
        const ImVec2 image(texture->width*scale,texture->height*scale);
        const ImVec2 origin(minimum.x+(size.x-image.x)*.5f,minimum.y+(size.y-image.y)*.5f);
        draw->AddImage(texture->textureId,origin,ImVec2(origin.x+image.x,origin.y+image.y));
    }else draw->AddText(nullptr,0.f,minimum,ImGui::GetColorU32(ImGuiCol_Text),
        texture&&!texture->loadAttempted?loading:unavailable,nullptr,size.x);
    draw->PopClipRect();return ready;
}
}
