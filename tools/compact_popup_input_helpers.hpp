// Included inside HeadlessPanel. New compact cases use queued input only;
// unlike legacy fixtures, these helpers never assign Scroll/Nav/Active state.
    UiInteractionTrace::Item compactItem(const char *label,ImGuiID exactId=0) const
    {
        if(!exactId)return visibleItem(label);
        const auto items=controls(label,exactId);
        if(items.size()!=1)controlFailure("Missing exact compact control",label,exactId);
        auto *owner=ImGui::FindWindowByName(items[0].window.c_str());
        auto *popup=context_->OpenPopupStack.empty()?nullptr:context_->OpenPopupStack.back().Window;
        auto *ancestor=owner;while(ancestor&&ancestor!=popup)ancestor=ancestor->ParentWindow;
        if(!owner||(popup&&!ancestor))controlFailure("Exact compact control is outside active popup",label,exactId);
        return items[0];
    }
    ImRect compactRect(const UiInteractionTrace::Item &item,bool selectable=false) const
    {
        ImRect rect(item.minimum,item.maximum);
        // ImGui Selectable expands only its hit rectangle by half ItemSpacing.
        // Restore that exact logical row for the explicit target-picker case.
        if(selectable){const auto spacing=ImGui::GetStyle().ItemSpacing;
            rect.Min.x+=ImTrunc(spacing.x*.5f);rect.Max.x-=spacing.x-ImTrunc(spacing.x*.5f);
            rect.Min.y+=ImTrunc(spacing.y*.5f);rect.Max.y-=spacing.y-ImTrunc(spacing.y*.5f);}
        return rect;
    }
    void compactReach(const char *label,ImGuiID id=0,bool selectable=false)
    {
        for(int n=0;n<48;++n){
            const auto item=compactItem(label,id);auto *w=ImGui::FindWindowByName(item.window.c_str());
            const auto rect=compactRect(item,selectable);const auto *vp=w->Viewport;
            const ImRect work(vp->WorkPos,ImVec2(vp->WorkPos.x+vp->WorkSize.x,vp->WorkPos.y+vp->WorkSize.y));
            if(w->InnerClipRect.Contains(rect)&&work.Contains(rect))return;
            if(rect.Min.x<w->InnerClipRect.Min.x-1||rect.Max.x>w->InnerClipRect.Max.x+1)
                controlFailure("Compact control is horizontally clipped",label,item.id);
            auto *scroll=w;while(scroll->ParentWindow&&scroll->ScrollMax.y<=0)scroll=scroll->ParentWindow;
            if(scroll->ScrollMax.y<=0)controlFailure("Compact control has no real scrolling owner",label,item.id);
            auto &io=ImGui::GetIO();const auto center=scroll->InnerClipRect.GetCenter();
            io.AddFocusEvent(true);io.AddMouseViewportEvent(scroll->ViewportId);io.AddMousePosEvent(center.x,center.y);frame();
            io.AddMouseWheelEvent(0,rect.Min.y<w->InnerClipRect.Min.y?1.f:-1.f);frame(3);
        }
        controlFailure("Queued wheel did not expose compact control",label,id);
    }
    void compactClick(const char *label,ImGuiID exactId=0,bool selectable=false)
    {
        compactReach(label,exactId,selectable);ImGuiID id=0;
        for(int n=0;n<3;++n){const auto item=compactItem(label,exactId);
            const auto rect=compactRect(item,selectable);auto *w=ImGui::FindWindowByName(item.window.c_str());
            if(!w->InnerClipRect.Contains(rect))controlFailure("Compact target moved outside clip",label,item.id);
            require(!id||id==item.id,"Compact input identity changed while hovering");id=item.id;
            auto &io=ImGui::GetIO();io.AddMouseViewportEvent(w->ViewportId);
            io.AddMousePosEvent(rect.GetCenter().x,rect.GetCenter().y);frame();}
        if(context_->HoveredId!=id||context_->HoveredIdIsDisabled)
            controlFailure("Compact target is not the exact enabled hovered widget",label,id);
        ImGui::GetIO().AddMouseButtonEvent(0,true);frame();
        ImGui::GetIO().AddMouseButtonEvent(0,false);frame(3);
    }
    void compactType(const char *label,const char *text,ImGuiID exactId=0)
    {
        compactClick(label,exactId);const auto item=compactItem(label,exactId);
        require(context_->ActiveId==item.id,"Compact input lacks exact keyboard activation");
        auto &io=ImGui::GetIO();io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();
        io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
        io.AddInputCharactersUTF8(text);frame(2);
    }
    ImGuiWindow *compactBounds(const char *footer,const char *popupName)
    {
        const auto item=compactItem(footer);reviewVisible(item);
        auto *popup=ImGui::FindWindowByName(item.window.c_str());
        require(popup&&(popup->Flags&ImGuiWindowFlags_Popup)&&context_->OpenPopupStack.Size==1,
            "Persistent footer is not in one actual popup");
        const auto &opened=context_->OpenPopupStack[0];
        require(opened.Window==popup&&popup->PopupId==opened.PopupId&&
            opened.PopupId==ImHashStr(popupName,0,opened.OpenParentId),"Persistent footer has the wrong popup identity/scope");
        const auto *vp=ImGui::GetMainViewport();
        const ImRect work(vp->WorkPos,ImVec2(vp->WorkPos.x+vp->WorkSize.x,vp->WorkPos.y+vp->WorkSize.y));
        if(popup->Viewport!=vp||!work.Contains(popup->Rect())||std::abs(popup->FontRefSize-27.f)>.01f||
            vp->WorkPos.x!=40.f||vp->WorkPos.y!=260.f||vp->WorkSize.x!=1280.f||vp->WorkSize.y!=720.f)
            controlFailure("Compact popup escaped exact owner work area or 27px font",footer,item.id);
        return popup;
    }
    void compactLastRow(const char *bodyAnchor,const char *message)
    {
        const auto item=compactItem(bodyAnchor);auto *body=ImGui::FindWindowByName(item.window.c_str());
        require(contains(rendered,message),"Expected localized failure text missing from rendered frame");
        for(int n=0;n<48;++n){
            const ImRect row(ImVec2(body->WorkRect.Min.x,body->DC.CursorPosPrevLine.y),
                ImVec2(body->DC.CursorPosPrevLine.x,body->DC.CursorPosPrevLine.y+body->DC.PrevLineSize.y));
            if(row.GetHeight()>0&&body->InnerClipRect.Contains(row.Min)&&
                body->InnerClipRect.Contains(ImVec2(row.Max.x-1,row.Max.y-1)))return;
            auto &io=ImGui::GetIO();const auto hover=body->InnerClipRect.GetCenter();
            io.AddMouseViewportEvent(body->ViewportId);io.AddMousePosEvent(hover.x,hover.y);frame();
            io.AddMouseWheelEvent(0,-1);frame(3);
        }
        controlFailure("Wrapped failure row is not reachable by real wheel",bodyAnchor,item.id);
    }
    void compactOutcomeStatus(const char *recordTab,const char *title,const char *message)
    {
        const auto tab=compactItem(recordTab);auto *body=ImGui::FindWindowByName(tab.window.c_str());
        require(body&&std::string(body->Name).find("SessionOutcomesBody")!=std::string::npos,
            "Outcome feedback has no actual popup-body owner");
        for(int n=0;n<48&&body->Scroll.y>0;++n){
            auto &io=ImGui::GetIO();const auto point=body->InnerClipRect.GetCenter();
            io.AddMouseViewportEvent(body->ViewportId);io.AddMousePosEvent(point.x,point.y);frame();
            io.AddMouseWheelEvent(0,1);frame(3);
        }
        require(body->Scroll.y==0&&contains(rendered,message),"Outcome failure feedback could not be reached normally");
        // The production panel's first rows are the selected title then status.
        // Measure their real font/wrap budget and correlate with the next
        // observed tab and actual submitted vertices; global log alone is not
        // evidence that this wrapped message is inside the visible body.
        const float width=body->WorkRect.GetWidth();auto *font=ImGui::GetIO().Fonts->Fonts[0];
        const float scale=body->FontRefSize/font->LegacySize;
        const auto measure=[&](const char *text){
            const auto size=font->CalcTextSizeA(font->LegacySize,FLT_MAX,width/scale,text);
            return ImVec2(size.x*scale,size.y*scale);};
        const auto titleSize=measure(title),messageSize=measure(message);
        require(ImGui::GetIO().Fonts->TexIsBuilt,"Outcome measurement dirtied the CPU font atlas");
        const ImVec2 start(body->DC.CursorStartPos.x,body->DC.CursorStartPos.y+titleSize.y+ImGui::GetStyle().ItemSpacing.y);
        const ImRect row(start,ImVec2(start.x+messageSize.x,start.y+messageSize.y));
        const auto observed=compactItem(recordTab);
        const bool glyphs=std::any_of(body->DrawList->VtxBuffer.begin(),body->DrawList->VtxBuffer.end(),
            [&](const ImDrawVert &v){return row.Contains(v.pos);});
        if(!glyphs||!body->InnerClipRect.Contains(row)||row.Max.y>observed.minimum.y+1)
            controlFailure("Wrapped outcome failure row is not visible above its observed tabs",recordTab,observed.id);
    }
